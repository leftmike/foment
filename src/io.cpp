/*

Foment

*/

#ifdef FOMENT_WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif // FOMENT_WIN32

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include "foment.hpp"
#include "io.hpp"

FObject StandardInput;
FObject StandardOutput;
static FObject QuoteSymbol;
static FObject QuasiquoteSymbol;
static FObject UnquoteSymbol;
static FObject UnquoteSplicingSymbol;

// ---- Ports ----

int InputPortP(FObject obj)
{
    return(PortP(obj) && AsPort(obj)->Input != 0);
}

int OutputPortP(FObject obj)
{
    return(PortP(obj) && AsPort(obj)->Output != 0);
}

void ClosePort(FObject port)
{
    FAssert(PortP(port));

    if (AsPort(port)->CloseFn != 0)
    {
        if (AsPort(port)->Context != 0)
        {
            AsPort(port)->CloseFn(AsPort(port)->Context, AsPort(port)->Object);
            AsPort(port)->Context = 0;
        }
    }
}

FObject MakePort(FObject nam, FInputPort * inp, FOutputPort * outp, FPortLocation* loc,
    FCloseFn cfn, void * ctx, FObject obj)
{
    FAssert(inp != 0 || outp != 0);
    FAssert(ctx != 0);

    FPort * port = (FPort *) MakeObject(PortTag, sizeof(FPort));
    port->Name = nam;
    port->Input = inp;
    port->Output = outp;
    port->Location = loc;
    port->CloseFn = cfn;
    port->Context = ctx;
    port->Object = obj;
    return(AsObject(port));
}

// ---- FILE Input and Output Ports ----

typedef struct
{
    FILE * File;
    FCh PeekedCh;
    int PeekedFlag;
    int DontCloseFile;
    int LineNumber;
} FILEContext;

#define AsFileContext(ctx) ((FILEContext *) (ctx))

static FCh FILEGetCh(void * ctx, FObject obj, int * eof)
{
    if (AsFileContext(ctx)->PeekedFlag)
    {
        AsFileContext(ctx)->PeekedFlag = 0;
        *eof = 0;
        return(AsFileContext(ctx)->PeekedCh);
    }

    int ch = fgetc(AsFileContext(ctx)->File);
    *eof = (ch == EOF ? 1 : 0);
    if (ch == '\n')
        AsFileContext(ctx)->LineNumber += 1;
    return(ch);
}

static FCh FILEPeekCh(void * ctx, FObject obj, int * eof)
{
    *eof = 0;

    if (AsFileContext(ctx)->PeekedFlag)
        return(AsFileContext(ctx)->PeekedCh);

    int ch = fgetc(AsFileContext(ctx)->File);
    if (ch == EOF)
    {
        *eof = 1;
        return(ch);
    }

    AsFileContext(ctx)->PeekedFlag = 1;
    AsFileContext(ctx)->PeekedCh = ch;
    return(ch);
}

static void FILEPutCh(void * ctx, FObject obj, FCh ch)
{
    fputc(ch, AsFileContext(ctx)->File);
}

static void FILEPutString(void * ctx, FObject obj, FCh * s, int sl)
{
    int sdx;

    for (sdx = 0; sdx < sl; sdx++)
        fputc(s[sdx], AsFileContext(ctx)->File);
}

static void FILEPutStringC(void * ctx, FObject obj, char * s)
{
    fputs(s, AsFileContext(ctx)->File);
}

static void FILEClose(void *ctx, FObject obj)
{
    FAssert(ctx != 0);
    FAssert(AsFileContext(ctx)->File != 0);

    if (AsFileContext(ctx)->DontCloseFile == 0)
        fclose(AsFileContext(ctx)->File);

    free(ctx);
}

static int FILEGetLocation(void * ctx, FObject obj)
{
    return(AsFileContext(ctx)->LineNumber);
}

static FInputPort FILEInputPort = {FILEGetCh, FILEPeekCh};
static FOutputPort FILEOutputPort = {FILEPutCh, FILEPutString, FILEPutStringC};
static FPortLocation FILEPortLocation = {FILEGetLocation, 0};

static FObject MakeFILEPort(FObject nam, FILE * f, int ipf, int opf, int dcf)
{
    FAssert(f != 0);
    FAssert(ipf != 0 || opf != 0);

    FILEContext * fc = (FILEContext *) malloc(sizeof(FILEContext));
    fc->File = f;
    fc->PeekedFlag = 0;
    fc->DontCloseFile = dcf;
    fc->LineNumber = 1;

    return(MakePort(nam, ipf ? &FILEInputPort : 0, opf ? &FILEOutputPort : 0,
            &FILEPortLocation, FILEClose, fc, FalseObject));
}

FObject OpenInputFile(FObject fn, int ref)
{
    FAssert(StringP(fn));

    char cfn[256];

    for (int idx = 0; idx < AsString(fn)->Length && idx < sizeof(cfn); idx++)
        cfn[idx] = (char) AsString(fn)->String[idx];

    cfn[AsString(fn)->Length >= sizeof(cfn) ? sizeof(cfn) - 1 : AsString(fn)->Length] = 0;

    FILE * f = fopen(cfn, "r");
    if (f == 0)
    {
        if (ref == 0)
            return(NoValueObject);

        RaiseExceptionC(Assertion, "open-input-file",
                "open-input-file: can not open file for reading", List(fn));
    }

    return(MakeFILEPort(fn, f, 1, 0, 0));
}

FObject OpenOutputFile(FObject fn, int ref)
{
    FAssert(StringP(fn));

    char cfn[256];

    for (int idx = 0; idx < AsString(fn)->Length && idx < sizeof(cfn); idx++)
        cfn[idx] = (char) AsString(fn)->String[idx];

    cfn[AsString(fn)->Length >= sizeof(cfn) ? sizeof(cfn) - 1 : AsString(fn)->Length] = 0;

    FILE * f = fopen(cfn, "w");
    if (f == 0)
    {
        if (ref == 0)
            return(NoValueObject);

        RaiseExceptionC(Assertion, "open-output-file",
                "open-output-file: can not open file for writing", List(fn));
    }

    return(MakeFILEPort(fn, f, 0, 1, 0));
}

// ---- String Input Ports ----

typedef struct
{
    int Index;
} StringInputContext;

static FCh StringInputGetCh(void * ctx, FObject obj, int * eof)
{
    FAssert(StringP(obj));

    StringInputContext * sic = (StringInputContext *) ctx;

    if (AsString(obj)->Length == sic->Index)
    {
        *eof = 1;
        return(0);
    }

    FCh ch = AsString(obj)->String[sic->Index];
    sic->Index += 1;

    return(ch);
}

static FCh StringInputPeekCh(void * ctx, FObject obj, int * eof)
{
    FAssert(StringP(obj));

    StringInputContext * sic = (StringInputContext *) ctx;

    if (AsString(obj)->Length == sic->Index)
    {
        *eof = 1;
        return(0);
    }

    return(AsString(obj)->String[sic->Index]);
}

static void StringInputClose(void *ctx, FObject obj)
{
    FAssert(ctx != 0);
    free(ctx);
}

static FInputPort StringInputPort = {StringInputGetCh, StringInputPeekCh};

static FObject MakeStringInputPort(FObject str)
{
    FAssert(StringP(str));

    StringInputContext * sic = (StringInputContext *) malloc(sizeof(StringInputContext));
    sic->Index = 0;

    return(MakePort(FalseObject, &StringInputPort, 0, 0, StringInputClose, sic, str));
}

// ---- String Output Ports ----

typedef struct
{
    int Count;
} StringOutputContext;

#define AsStringOutputContext(ctx) ((StringOutputContext *) (ctx))

static void StringOutputPutCh(void * ctx, FObject obj, FCh ch)
{
    FAssert(BoxP(obj));

    AsStringOutputContext(ctx)->Count += 1;
    AsBox(obj)->Value = MakePair(MakeCharacter(ch), Unbox(obj));
}

static void StringOutputPutString(void * ctx, FObject obj, FCh * s, int sl)
{
    FAssert(BoxP(obj));

    AsStringOutputContext(ctx)->Count += sl;
    AsBox(obj)->Value = MakePair(MakeString(s, sl), Unbox(obj));
}

static void StringOutputPutStringC(void * ctx, FObject obj, char * s)
{
    FAssert(BoxP(obj));

    AsStringOutputContext(ctx)->Count += strlen(s);
    AsBox(obj)->Value = MakePair(MakeStringC(s), Unbox(obj));
}

static void StringOutputClose(void * ctx, FObject obj)
{
    FAssert(ctx != 0);

    free(ctx);
}

static FOutputPort StringOutputPort = {StringOutputPutCh, StringOutputPutString,
    StringOutputPutStringC};

FObject MakeStringOutputPort()
{
    StringOutputContext * soc = (StringOutputContext *) malloc(sizeof(StringOutputContext));
    soc->Count = 0;

    return(MakePort(FalseObject, 0, &StringOutputPort, 0, StringInputClose, soc,
            MakeBox(EmptyListObject)));
}

FObject GetOutputString(FObject port)
{
    FAssert(OutputPortP(port));
    FAssert(AsPort(port)->Output == &StringOutputPort);
    FAssert(BoxP(AsPort(port)->Object));

    int idx = AsStringOutputContext(AsPort(port)->Context)->Count;
    FObject s = MakeStringCh(idx, 0);
    FObject lst = Unbox(AsPort(port)->Object);

    while (PairP(lst))
    {
        if (CharacterP(First(lst)))
        {
            idx -= 1;
            AsString(s)->String[idx] = AsCharacter(First(lst));
        }
        else
        {
            FAssert(StringP(First(lst)));

            idx -= AsString(First(lst))->Length;

            for (int sdx = 0; sdx < AsString(First(lst))->Length; sdx++)
                AsString(s)->String[idx + sdx] = AsString(First(lst))->String[sdx];
        }

        lst = Rest(lst);
    }

    FAssert(lst == EmptyListObject);

    return(s);
}

Define("open-output-string", OpenOutputStringPrimitive)(int argc, FObject argv[])
{
    if (argc != 0)
        RaiseExceptionC(Assertion, "open-output-string",
                "open-output-string: expected no arguments", EmptyListObject);

    return(MakeStringOutputPort());
}

Define("get-output-string", GetOutputStringPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(Assertion, "get-output-string",
                "get-output-string: expected one argument", EmptyListObject);

    if (OutputPortP(argv[0]) == 0 || AsPort(argv[0])->Output != &StringOutputPort)
        RaiseExceptionC(Assertion, "get-output-string",
                "get-output-string: expected a string output port", List(argv[0]));

    return(GetOutputString(argv[0]));
}

// ---- ReadStringC ----

typedef struct
{
    char * String;
} StringCInputContext;

static FCh StringCInputPortGetCh(void * ctx, FObject obj, int * eof)
{
    StringCInputContext * sctx = (StringCInputContext *) ctx;
    FCh ch;

    ch = *sctx->String;
    if (ch == 0)
        *eof = 1;
    else
    {
        *eof = 0;
        sctx->String += 1;
    }

    return(ch);
}

static FCh StringCInputPortPeekCh(void * ctx, FObject obj, int * eof)
{
    StringCInputContext * sctx = (StringCInputContext *) ctx;
    FCh ch;

    ch = *sctx->String;
    if (ch == 0)
        *eof = 1;
    else
        *eof = 0;

    return(ch);
}

static void StringCInputPortClose(void * ctx, FObject obj)
{
    FAssert(ctx != 0);
    free(ctx);
}

static FInputPort StringCInputPort = {StringCInputPortGetCh, StringCInputPortPeekCh};

FObject MakeStringCInputPort(char * s)
{
    StringCInputContext * ctx = (StringCInputContext *) malloc(
            sizeof(StringCInputContext));
    ctx->String = s;

    return(MakePort(FalseObject, &StringCInputPort, 0, 0, StringCInputPortClose, ctx,
            FalseObject));
}

FObject ReadStringC(char * s, int rif)
{
    return(Read(MakeStringCInputPort(s), rif, 0));
}

// ---- Port Input ----

FCh GetCh(FObject port, int * eof)
{
    FAssert(InputPortP(port) && AsPort(port)->Context != 0);

    return(AsPort(port)->Input->GetChFn(AsPort(port)->Context, AsPort(port)->Object, eof));
}

FCh PeekCh(FObject port, int * eof)
{
    FAssert(InputPortP(port) && AsPort(port)->Context != 0);

    return(AsPort(port)->Input->PeekChFn(AsPort(port)->Context, AsPort(port)->Object, eof));
}

#define SymbolChP(ch) \
    (ChAlphabeticP((ch)) || ChNumericP((ch)) \
        || (ch) == '+' || (ch) == '-' || (ch) == '.' \
        || (ch) == '*' || (ch) == '/' || (ch) == '<' || (ch) == '=' \
        || (ch) == '>' || (ch) == '!' || (ch) == '?' || (ch) == ':' \
        || (ch) == '$' || (ch) == '%' || (ch) == '_' || (ch) == '&' \
        || (ch) == '~' || (ch) == '^')

#define DotObject ((FObject) -1)
#define EolObject ((FObject *) -2)

static FObject DoReadString(FObject port)
{
    FCh s[512];
    int sl = 0;
    FCh ch;
    int eof;

    for (;;)
    {
        ch = GetCh(port, &eof);
        if (ch == '"')
            break;
        if (ch == '\\')
            ch = GetCh(port, &eof);
        if (eof)
            RaiseExceptionC(Lexical, "read", "read: unexpected end-of-file reading string",
                    List(port));

        s[sl] = ch;
        sl += 1;
        if (sl == sizeof(s) / sizeof(FCh))
            RaiseExceptionC(Restriction, "read", "read: string too long", List(port));
    }

    return(MakeString(s, sl));
}

static FObject DoReadSymbolOrNumber(FObject port, int ch, int rif, int fcf)
{
    FCh s[256];
    int sl;
    int eof;
    FFixnum n;
    int ln;

    if (rif)
        ln = GetLocation(port);

    sl = 0;
    for (;;)
    {
        s[sl] = ch;
        sl += 1;
        if (sl == sizeof(s) / sizeof(FCh))
            RaiseExceptionC(Restriction, "read", "read: symbol or number too long",
                    List(port));

        ch = PeekCh(port, &eof);
        if (eof || SymbolChP(ch) == 0)
            break;

        GetCh(port, &eof);
        FAssert(eof == 0);
    }

    if (StringAsNumber(s, sl, &n))
        return(MakeFixnum(n));

    FObject sym = fcf ? StringToSymbol(FoldCaseString(MakeString(s, sl)))
            : StringLengthToSymbol(s, sl);
    if (rif)
        return(MakeIdentifier(sym, ln));
    return(sym);
}

static FObject DoReadList(FObject port, int rif, int fcf);
static FObject DoRead(FObject port, int eaf, int rlf, int rif, int fcf)
{
    FCh ch;
    int eof;

    for (;;)
    {
        ch = GetCh(port, &eof);
        if (eof)
            break;

        if (ch == ';')
        {
            do
            {
                ch = GetCh(port, &eof);
                if (eof)
                    goto Eof;
            }
            while (ch != '\n' && ch != '\r');
        }
        else
        {
            switch (ch)
            {
            case '#':
                ch = GetCh(port, &eof);
                if (eof)
                    RaiseExceptionC(Lexical, "read", "read: unexpected end-of-file reading #",
                            List(port));

                switch (ch)
                {
                case 't':
                case 'T':
                    return(TrueObject);

                case 'f':
                case 'F':
                    return(FalseObject);

                case '\\':
                    ch = GetCh(port, &eof);
                    if (eof)
                        RaiseExceptionC(Lexical, "read", "read:unexpected end-of-file reading #\\",
                                List(port));
                    return(MakeCharacter(ch));

                case '(':
                    return(ListToVector(DoReadList(port, rif, fcf)));

                case 'u':
                    ch = GetCh(port, &eof);
                    if (eof)
                        RaiseExceptionC(Lexical, "read",
                                "read: unexpected end-of-file reading bytevector", List(port));
                    if (ch != '8')
                        RaiseExceptionC(Lexical, "read", "read: expected #\u8(", List(port));

                    ch = GetCh(port, &eof);
                    if (eof)
                        RaiseExceptionC(Lexical, "read",
                                "read: unexpected end-of-file reading bytevector", List(port));
                    if (ch != '(')
                        RaiseExceptionC(Lexical, "read", "read: expected #\u8(", List(port));
                    return(U8ListToBytevector(DoReadList(port, rif, fcf)));
                }

                RaiseExceptionC(Lexical, "read", "read: unexpected character following #",
                        List(port));
                break;

            case '"':
                return(DoReadString(port));

            case '(':
                return(DoReadList(port, rif, fcf));

            case ')':
                if (rlf)
                    return(EolObject);
                RaiseExceptionC(Lexical, "read", "read: unexpected )", List(port));
                break;

            case '.':
                ch = PeekCh(port, &eof);
                if (eof)
                    RaiseExceptionC(Lexical, "read",
                            "read: unexpected end-of-file reading dotted pair", List(port));
                if (ch == '.')
                {
                    GetCh(port, &eof);

                    ch = GetCh(port, &eof);
                    if (eof)
                        RaiseExceptionC(Lexical, "read",
                                "read: unexpected end-of-file reading ...", List(port));
                        if (ch != '.')
                            RaiseExceptionC(Lexical, "read", "read: expected ...", List(port));
                    return(rif ? MakeIdentifier(EllipsisSymbol, GetLocation(port))
                            : EllipsisSymbol);
                }

                if (rlf)
                    return(DotObject);
                RaiseExceptionC(Lexical, "read", "read: unexpected dotted pair", List(port));
                break;

            case '\'':
            {
                FObject obj;
                obj = DoRead(port, 0, 0, rif, fcf);
                return(MakePair(
                        rif ? MakeIdentifier(QuoteSymbol, GetLocation(port)) : QuoteSymbol,
                        MakePair(obj, EmptyListObject)));
            }

            case '`':
            {
                FObject obj;
                obj = DoRead(port, 0, 0, rif, fcf);
                return(MakePair(
                        rif ? MakeIdentifier(QuasiquoteSymbol, GetLocation(port))
                        : QuasiquoteSymbol, MakePair(obj, EmptyListObject)));
            }

            case ',':
            {
                ch = PeekCh(port, &eof);
                if (eof)
                    RaiseExceptionC(Lexical, "read",
                            "read: unexpected end-of-file reading unquote", List(port));

                FObject sym;
                if (ch == '@')
                {
                    GetCh(port, &eof);
                    sym = UnquoteSplicingSymbol;
                }
                else
                    sym = UnquoteSymbol;

                FObject obj;
                obj = DoRead(port, 0, 0, rif, fcf);
                return(MakePair(
                        rif ? MakeIdentifier(sym, GetLocation(port)) : sym,
                        MakePair(obj, EmptyListObject)));
            }

            default:
                if (SymbolChP(ch))
                    return(DoReadSymbolOrNumber(port, ch, rif, fcf));
                break;
            }
        }
    }

Eof:

    if (eaf == 0)
        RaiseExceptionC(Lexical, "read", "read: unexpected end-of-file reading list or vector",
                List(port));
    return(EndOfFileObject);
}

static FObject DoReadList(FObject port, int rif, int fcf)
{
    FObject obj;

    obj = DoRead(port, 0, 1, rif, fcf);

    if (obj == EolObject)
        return(EmptyListObject);

    if (obj == DotObject)
    {
        obj = DoRead(port, 0, 0, rif, fcf);

        if (DoRead(port, 0, 1, rif, fcf) != EolObject)
            RaiseExceptionC(Lexical, "read", "read: bad dotted pair", List(port));
        return(obj);
    }

    return(MakePair(obj, DoReadList(port, rif, fcf)));
}

FObject Read(FObject port, int rif, int fcf)
{
    FAssert(InputPortP(port) && AsPort(port)->Context != 0);

    return(DoRead(port, 1, 0, rif, fcf));
}

// ---- Port Output ----

void PutCh(FObject port, FCh ch)
{
    FAssert(OutputPortP(port) && AsPort(port)->Context != 0);

    AsPort(port)->Output->PutChFn(AsPort(port)->Context, AsPort(port)->Object, ch);
}

void PutString(FObject port, FCh * s, int sl)
{
    FAssert(OutputPortP(port) && AsPort(port)->Context != 0);

    AsPort(port)->Output->PutStringFn(AsPort(port)->Context, AsPort(port)->Object, s, sl);
}

void PutStringC(FObject port, char * s)
{
    FAssert(OutputPortP(port) && AsPort(port)->Context != 0);

    AsPort(port)->Output->PutStringCFn(AsPort(port)->Context, AsPort(port)->Object, s);
}

unsigned int FindSharedObjects(FObject ht, FObject obj, unsigned int cnt, int cof)
{
    FAssert(HashtableP(ht));

Again:

    if (SharedObjectP(obj))
    {
        FObject val = HashtableRef(ht, obj, MakeFixnum(0), EqP, EqHash);

        FAssert(FixnumP(val));

        HashtableSet(ht, obj, MakeFixnum(AsFixnum(val) + 1), EqP, EqHash);

        if (AsFixnum(val) == 0)
        {
            if (PairP(obj))
            {
                cnt = FindSharedObjects(ht, First(obj), cnt, cof);
                if (cof != 0)
                    cnt = FindSharedObjects(ht, Rest(obj), cnt, cof);
                else
                {
                    obj = Rest(obj);
                    goto Again;
                }
            }
            else if (BoxP(obj))
                cnt = FindSharedObjects(ht, Unbox(obj), cnt, cof);
            else if (VectorP(obj))
            {
                for (int idx = 0; idx < VectorLen(obj); idx++)
                    cnt = FindSharedObjects(ht, AsVector(obj)->Vector[idx], cnt, cof);
            }
            else if (ProcedureP(obj))
                cnt = FindSharedObjects(ht, AsProcedure(obj)->Code, cnt, cof);
            else
            {
                FAssert(GenericRecordP(obj));

                FObject rt = AsGenericRecord(obj)->Record.RecordType;
                FAssert(RecordTypeP(rt));

                for (int fdx = 0; fdx < AsRecordType(rt)->NumFields; fdx++)
                    cnt = FindSharedObjects(ht, AsGenericRecord(obj)->Fields[fdx], cnt, cof);
            }

            if (cof)
            {
                val = HashtableRef(ht, obj, MakeFixnum(0), EqP, EqHash);
                FAssert(FixnumP(val));
                FAssert(AsFixnum(val) > 0);

                if (AsFixnum(val) == 1)
                    HashtableDelete(ht, obj, EqP, EqHash);
            }
        }
        else
            return(cnt + 1);
    }

    return(cnt);
}

void WriteSharedObject(FObject port, FObject obj, int df, FWriteFn wfn, void * ctx)
{
    if (SharedObjectP(obj))
    {
        FObject val = HashtableRef(AsWriteSharedCtx(ctx)->Hashtable, obj, FalseObject, EqP,
                EqHash);

        if (BooleanP(val))
        {
            if (val == TrueObject)
            {
                AsWriteSharedCtx(ctx)->Label += 1;
                HashtableSet(AsWriteSharedCtx(ctx)->Hashtable, obj,
                        MakeFixnum(AsWriteSharedCtx(ctx)->Label), EqP, EqHash);

                PutCh(port, '#');
                FCh s[8];
                int sl = NumberAsString(AsWriteSharedCtx(ctx)->Label, s, 10);
                PutString(port, s, sl);
                PutCh(port, '=');
            }

            if (PairP(obj))
            {
                PutCh(port, '(');
                for (;;)
                {
                    wfn(port, First(obj), df, wfn, ctx);
                    if (PairP(Rest(obj)) && HashtableRef(AsWriteSharedCtx(ctx)->Hashtable,
                            Rest(obj), FalseObject, EqP, EqHash) == FalseObject)
                    {
                        PutCh(port, ' ');
                        obj = Rest(obj);
                    }
                    else if (Rest(obj) == EmptyListObject)
                    {
                        PutCh(port, ')');
                        break;
                    }
                    else
                    {
                        PutStringC(port, " . ");
                        wfn(port, Rest(obj), df, wfn, ctx);
                        PutCh(port, ')');
                        break;
                    }
                }
            }
            else
                WriteGeneric(port, obj, df, wfn, ctx);
        }
        else
        {
            FAssert(FixnumP(val));

            PutCh(port, '#');
            FCh s[8];
            int sl = NumberAsString(AsFixnum(val), s, 10);
            PutString(port, s, sl);
            PutCh(port, '#');
        }
    }
    else
        WriteGeneric(port, obj, df, wfn, ctx);
}

FObject WalkUpdate(FObject key, FObject val, FObject ctx)
{
    return(TrueObject);
}

void Write(FObject port, FObject obj, int df)
{
    FObject ht = MakeHashtable(23);

    if (SharedObjectP(obj))
    {
        if (FindSharedObjects(ht, obj, 0, 1) == 0)
        {
            FAssert(HashtableSize(ht) == 0);

            WriteGeneric(port, obj, df, (FWriteFn) WriteGeneric, 0);
        }
        else
        {
            HashtableWalkUpdate(ht, WalkUpdate, NoValueObject);

            FWriteSharedCtx ctx;
            ctx.Hashtable = ht;
            ctx.Label = -1;
            WriteSharedObject(port, obj, df, (FWriteFn) WriteSharedObject, &ctx);
        }
    }
    else
        WriteGeneric(port, obj, df, (FWriteFn) WriteGeneric, 0);
}

int WalkDelete(FObject key, FObject val, FObject ctx)
{
    FAssert(FixnumP(val));
    FAssert(AsFixnum(val) > 0);

    return(AsFixnum(val) == 1);
}

void WriteShared(FObject port, FObject obj, int df)
{
    FObject ht = MakeHashtable(23);

    if (SharedObjectP(obj))
    {
        if (FindSharedObjects(ht, obj, 0, 0) == 0)
            WriteGeneric(port, obj, df, (FWriteFn) WriteGeneric, 0);
        else
        {
            HashtableWalkDelete(ht, WalkDelete, NoValueObject);
            HashtableWalkUpdate(ht, WalkUpdate, NoValueObject);

            FWriteSharedCtx ctx;
            ctx.Hashtable = ht;
            ctx.Label = -1;
            WriteSharedObject(port, obj, df, (FWriteFn) WriteSharedObject, &ctx);
        }
    }
    else
        WriteGeneric(port, obj, df, (FWriteFn) WriteGeneric, 0);
}

static void WriteGenericObject(FObject port, FObject obj, int df, FWriteFn wfn, void * ctx)
{
    switch (ObjectTag(obj))
    {
    case PairTag:
        PutCh(port, '(');
        for (;;)
        {
            wfn(port, First(obj), df, wfn, ctx);
            if (PairP(Rest(obj)))
            {
                PutCh(port, ' ');
                obj = Rest(obj);
            }
            else if (Rest(obj) == EmptyListObject)
            {
                PutCh(port, ')');
                break;
            }
            else
            {
                PutStringC(port, " . ");
                wfn(port, Rest(obj), df, wfn, ctx);
                PutCh(port, ')');
                break;
            }
        }
        break;

    case BoxTag:
    {
        FCh s[16];
        int sl = NumberAsString((FFixnum) obj, s, 16);

        PutStringC(port, "#<(box: #x");
        PutString(port, s, sl);
        PutCh(port, ' ');
        wfn(port, Unbox(obj), df, wfn, ctx);
        PutStringC(port, ")>");
        break;
    }

    case StringTag:
        if (df == 0)
            PutCh(port, '"');
        PutString(port, AsString(obj)->String, AsString(obj)->Length);
        if (df == 0)
            PutCh(port, '"');
        break;

    case VectorTag:
    {
        int idx;

        PutStringC(port, "#(");
        for (idx = 0; idx < VectorLen(obj); idx++)
        {
            if (idx > 0)
                PutCh(port, ' ');
            wfn(port, AsVector(obj)->Vector[idx], df, wfn, ctx);
        }

        PutCh(port, ')');
        break;
    }

    case BytevectorTag:
    {
        int idx;
        FCh s[8];
        int sl;

        PutStringC(port, "#u8(");
        for (idx = 0; idx < AsBytevector(obj)->Length; idx++)
        {
            if (idx > 0)
                PutCh(port, ' ');

            sl = NumberAsString((FFixnum) AsBytevector(obj)->Vector[idx], s, 10);
            PutString(port, s, sl);
        }

        PutCh(port, ')');
        break;
    }

    case PortTag:
    {
        FCh s[16];
        int sl = NumberAsString((FFixnum) obj, s, 16);

        PutStringC(port, "#<");
        if (InputPortP(obj))
            PutStringC(port, "input-");
        if (OutputPortP(obj))
            PutStringC(port, "output-");
        PutStringC(port, "port: #x");
        PutString(port, s, sl);

        if (AsPort(obj)->Context == 0)
            PutStringC(port, " closed");

        if (StringP(AsPort(obj)->Name))
        {
            PutCh(port, ' ');
            PutString(port, AsString(AsPort(obj)->Name)->String,
                    AsString(AsPort(obj)->Name)->Length);
        }

        PutCh(port, '>');
        break;
    }

    case ProcedureTag:
    {
        FCh s[16];
        int sl = NumberAsString((FFixnum) obj, s, 16);

        PutStringC(port, "#<procedure: ");
        PutString(port, s, sl);

        if (AsProcedure(obj)->Name != NoValueObject)
        {
            PutCh(port, ' ');
            wfn(port, AsProcedure(obj)->Name, df, wfn, ctx);
        }

        if (AsProcedure(obj)->Flags & PROCEDURE_FLAG_CLOSURE)
            PutStringC(port, " closure");

        if (AsProcedure(obj)->Flags & PROCEDURE_FLAG_PARAMETER)
            PutStringC(port, " parameter");

        PutCh(port, ' ');
        wfn(port, AsProcedure(obj)->Code, df, wfn, ctx);
        PutCh(port, '>');
        break;
    }

    case SymbolTag:
        PutString(port, AsString(AsSymbol(obj)->String)->String,
                AsString(AsSymbol(obj)->String)->Length);
        break;

    case RecordTypeTag:
    {
        FCh s[16];
        int sl = NumberAsString((FFixnum) obj, s, 16);

        PutStringC(port, "#<record-type: #x");
        PutString(port, s, sl);
        PutCh(port, ' ');
        wfn(port, AsRecordType(obj)->Name, df, wfn, ctx);

        for (int fdx = 0; fdx < AsRecordType(obj)->NumFields; fdx += 1)
        {
            PutCh(port, ' ');
            wfn(port, AsRecordType(obj)->Fields[fdx], df, wfn, ctx);
        }

        PutStringC(port, ">");
        break;
    }

    case RecordTag:
    {
        FObject rt = AsGenericRecord(obj)->Record.RecordType;
        FCh s[16];
        int sl = NumberAsString((FFixnum) obj, s, 16);

        PutStringC(port, "#<(");
        wfn(port, AsRecordType(rt)->Name, df, wfn, ctx);
        PutStringC(port, ": #x");
        PutString(port, s, sl);

        for (int fdx = 0; fdx < AsRecordType(rt)->NumFields; fdx++)
        {
            PutCh(port, ' ');
            wfn(port, AsRecordType(rt)->Fields[fdx], df, wfn, ctx);
            PutStringC(port, ": ");
            wfn(port, AsGenericRecord(obj)->Fields[fdx], df, wfn, ctx);
        }

        PutStringC(port, ")>");
        break;
    }

    case PrimitiveTag:
    {
        PutStringC(port, "#<primitive: ");
        PutStringC(port, AsPrimitive(obj)->Name);
        PutCh(port, ' ');

        char * fn = AsPrimitive(obj)->Filename;
        char * p = fn;
        while (*p != 0)
        {
            if (*p == '/' || *p == '\\')
                fn = p + 1;

            p += 1;
        }

        PutStringC(port, fn);
        PutCh(port, '@');
        FCh s[16];
        int sl = NumberAsString(AsPrimitive(obj)->LineNumber, s, 10);
        PutString(port, s, sl);
        PutCh(port, '>');
        break;
    }

    default:
    {
        FCh s[16];
        int sl = NumberAsString((FFixnum) obj, s, 16);

        PutStringC(port, "#<unknown: ");
        PutString(port, s, sl);
        PutCh(port, '>');
        break;
    }

    }
}

void WriteGeneric(FObject port, FObject obj, int df, FWriteFn wfn, void * ctx)
{
    if (FixnumP(obj))
    {
        FCh s[16];
        int sl = NumberAsString(AsFixnum(obj), s, 10);
        PutString(port, s, sl);
    }
    else if (CharacterP(obj))
    {
        if (df == 0)
            PutStringC(port, "#\\");
        PutCh(port, AsCharacter(obj));
    }
    else if (SpecialSyntaxP(obj))
        WriteSpecialSyntax(port, obj, df);
    else if (InstructionP(obj))
        WriteInstruction(port, obj, df);
    else if (ValuesCountP(obj))
    {
        PutStringC(port, "#<values-count: ");

        FCh s[16];
        int sl = NumberAsString(AsValuesCount(obj), s, 10);
        PutString(port, s, sl);

        PutCh(port, '>');
    }
    else if (ObjectP(obj))
        WriteGenericObject(port, obj, df, wfn, ctx);
    else if (obj == EmptyListObject)
        PutStringC(port, "()");
    else if (obj == FalseObject)
        PutStringC(port, "#f");
    else if (obj == TrueObject)
        PutStringC(port, "#t");
    else if (obj == EndOfFileObject)
        PutStringC(port, "#<end-of-file>");
    else if (obj == NoValueObject)
        PutStringC(port, "#<no-value>");
    else if (obj == WantValuesObject)
        PutStringC(port, "#<want-values>");
    else
    {
        FCh s[16];
        int sl = NumberAsString((FFixnum) obj, s, 16);

        PutStringC(port, "#<unknown: ");
        PutString(port, s, sl);
        PutCh(port, '>');
    }
}

void WriteSimple(FObject port, FObject obj, int df)
{
    FAssert(OutputPortP(port) && AsPort(port)->Context != 0);

    WriteGeneric(port, obj, df, (FWriteFn) WriteGeneric, 0);
}

int GetLocation(FObject port)
{
    FAssert(PortP(port) && AsPort(port)->Context != 0);

    if (AsPort(port)->Location == 0 || AsPort(port)->Location->GetLocationFn == 0)
        return(-1);
    return(AsPort(port)->Location->GetLocationFn(AsPort(port)->Context, AsPort(port)->Object));
}

// ---- Primitives ----

Define("write", WritePrimitive)(int argc, FObject argv[])
{
    FObject port;

    if (argc < 1 || argc > 2)
        RaiseExceptionC(Assertion, "write", "write: expected one or two arguments",
                EmptyListObject);

    if (argc == 2)
    {
        if (OutputPortP(argv[1]) == 0)
            RaiseExceptionC(Assertion, "write", "write: expected an output port", List(argv[1]));

        port = argv[1];
    }
    else
        port = StandardOutput;

    Write(port, argv[0], 0);

    return(NoValueObject);
}

Define("display", DisplayPrimitive)(int argc, FObject argv[])
{
    FObject port;

    if (argc < 1 || argc > 2)
        RaiseExceptionC(Assertion, "display", "display: expected one or two arguments",
                EmptyListObject);

    if (argc == 2)
    {
        if (OutputPortP(argv[1]) == 0)
            RaiseExceptionC(Assertion, "display", "display: expected an output port",
                    List(argv[1]));

        port = argv[1];
    }
    else
        port = StandardOutput;

    Write(port, argv[0], 1);

    return(NoValueObject);
}

Define("write-shared", WriteSharedPrimitive)(int argc, FObject argv[])
{
    FObject port;

    if (argc < 1 || argc > 2)
        RaiseExceptionC(Assertion, "write-shared", "write-shared: expected one or two arguments",
                EmptyListObject);

    if (argc == 2)
    {
        if (OutputPortP(argv[1]) == 0)
            RaiseExceptionC(Assertion, "write-shared", "write-shared: expected an output port",
                    List(argv[1]));

        port = argv[1];
    }
    else
        port = StandardOutput;

    WriteShared(port, argv[0], 0);

    return(NoValueObject);
}

Define("display-shared", DisplaySharedPrimitive)(int argc, FObject argv[])
{
    FObject port;

    if (argc < 1 || argc > 2)
        RaiseExceptionC(Assertion, "display-shared",
                "display-shared: expected one or two arguments", EmptyListObject);

    if (argc == 2)
    {
        if (OutputPortP(argv[1]) == 0)
            RaiseExceptionC(Assertion, "display-shared", "display-shared: expected an output port",
                    List(argv[1]));

        port = argv[1];
    }
    else
        port = StandardOutput;

    WriteShared(port, argv[0], 1);

    return(NoValueObject);
}

Define("write-simple", WriteSimplePrimitive)(int argc, FObject argv[])
{
    FObject port;

    if (argc < 1 || argc > 2)
        RaiseExceptionC(Assertion, "write-simple", "write-simple: expected one or two arguments",
                EmptyListObject);

    if (argc == 2)
    {
        if (OutputPortP(argv[1]) == 0)
            RaiseExceptionC(Assertion, "write-simple", "write-simple: expected an output port",
                    List(argv[1]));

        port = argv[1];
    }
    else
        port = StandardOutput;

    WriteSimple(port, argv[0], 0);

    return(NoValueObject);
}

Define("display-simple", DisplaySimplePrimitive)(int argc, FObject argv[])
{
    FObject port;

    if (argc < 1 || argc > 2)
        RaiseExceptionC(Assertion, "display-simple",
                "display-simple: expected one or two arguments", EmptyListObject);

    if (argc == 2)
    {
        if (OutputPortP(argv[1]) == 0)
            RaiseExceptionC(Assertion, "display-simple", "display-simple: expected an output port",
                    List(argv[1]));

        port = argv[1];
    }
    else
        port = StandardOutput;

    WriteSimple(port, argv[0], 1);

    return(NoValueObject);
}

static FPrimitive * Primitives[] =
{
    &OpenOutputStringPrimitive,
    &GetOutputStringPrimitive,
    &WritePrimitive,
    &DisplayPrimitive,
    &WriteSharedPrimitive,
    &DisplaySharedPrimitive,
    &WriteSimplePrimitive,
    &DisplaySimplePrimitive
};

void SetupIO()
{
    StandardInput = MakeFILEPort(MakeStringC("--standard-input--"), stdin, 1, 0, 1);
    StandardOutput = MakeFILEPort(MakeStringC("--standard-output--"), stdout, 0, 1, 1);

    QuoteSymbol = StringCToSymbol("quote");
    QuasiquoteSymbol = StringCToSymbol("quasiquote");
    UnquoteSymbol = StringCToSymbol("unquote");
    UnquoteSplicingSymbol = StringCToSymbol("unquote-splicing");

    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(Bedrock, BedrockLibrary, Primitives[idx]);

    SetupPrettyPrint();
}
