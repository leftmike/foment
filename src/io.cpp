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
#include "unicode.hpp"

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

    FPort * port = (FPort *) MakeObject(sizeof(FPort), PortTag);
    port->Reserved = PortTag;
    port->Name = nam;
    port->Input = inp;
    port->Output = outp;
    port->Location = loc;
    port->CloseFn = cfn;
    port->Context = ctx;
    port->Object = obj;

    return(port);
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

#define ToFileContext(ctx) ((FILEContext *) (ctx))

static FCh FILEGetCh(void * ctx, FObject obj, int * eof)
{
    if (ToFileContext(ctx)->PeekedFlag)
    {
        ToFileContext(ctx)->PeekedFlag = 0;
        *eof = 0;
        return(ToFileContext(ctx)->PeekedCh);
    }

    int ch = fgetc(ToFileContext(ctx)->File);
    *eof = (ch == EOF ? 1 : 0);
    if (ch == '\n')
        ToFileContext(ctx)->LineNumber += 1;
    return(ch);
}

static FCh FILEPeekCh(void * ctx, FObject obj, int * eof)
{
    *eof = 0;

    if (ToFileContext(ctx)->PeekedFlag)
        return(ToFileContext(ctx)->PeekedCh);

    int ch = fgetc(ToFileContext(ctx)->File);
    if (ch == EOF)
    {
        *eof = 1;
        return(ch);
    }

    ToFileContext(ctx)->PeekedFlag = 1;
    ToFileContext(ctx)->PeekedCh = ch;
    return(ch);
}

static void FILEPutCh(void * ctx, FObject obj, FCh ch)
{
    fputc(ch, ToFileContext(ctx)->File);
}

static void FILEPutString(void * ctx, FObject obj, FCh * s, int sl)
{
    int sdx;

    for (sdx = 0; sdx < sl; sdx++)
        fputc(s[sdx], ToFileContext(ctx)->File);
}

static void FILEPutStringC(void * ctx, FObject obj, char * s)
{
    fputs(s, ToFileContext(ctx)->File);
}

static void FILEClose(void *ctx, FObject obj)
{
    FAssert(ctx != 0);
    FAssert(ToFileContext(ctx)->File != 0);

    if (ToFileContext(ctx)->DontCloseFile == 0)
        fclose(ToFileContext(ctx)->File);

    free(ctx);
}

static int FILEGetLocation(void * ctx, FObject obj)
{
    return(ToFileContext(ctx)->LineNumber);
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

    for (unsigned int idx = 0; idx < StringLength(fn) && idx < sizeof(cfn); idx++)
        cfn[idx] = (char) AsString(fn)->String[idx];

    cfn[StringLength(fn) >= sizeof(cfn) ? sizeof(cfn) - 1 : StringLength(fn)] = 0;

    FILE * f = fopen(cfn, "r");
    if (f == 0)
    {
        if (ref == 0)
            return(NoValueObject);

        RaiseExceptionC(R.Assertion, "open-input-file", "can not open file for reading", List(fn));
    }

    return(MakeFILEPort(fn, f, 1, 0, 0));
}

FObject OpenOutputFile(FObject fn, int ref)
{
    FAssert(StringP(fn));

    char cfn[256];

    for (unsigned int idx = 0; idx < StringLength(fn) && idx < sizeof(cfn); idx++)
        cfn[idx] = (char) AsString(fn)->String[idx];

    cfn[StringLength(fn) >= sizeof(cfn) ? sizeof(cfn) - 1 : StringLength(fn)] = 0;

    FILE * f = fopen(cfn, "w");
    if (f == 0)
    {
        if (ref == 0)
            return(NoValueObject);

        RaiseExceptionC(R.Assertion, "open-output-file", "can not open file for writing",
                List(fn));
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

    if (StringLength(obj) == sic->Index)
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

    if (StringLength(obj) == sic->Index)
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

#define ToStringOutputContext(ctx) ((StringOutputContext *) (ctx))

static void StringOutputPutCh(void * ctx, FObject obj, FCh ch)
{
    FAssert(BoxP(obj));

    ToStringOutputContext(ctx)->Count += 1;
//    AsBox(obj)->Value = MakePair(MakeCharacter(ch), Unbox(obj));
    Modify(FBox, obj, Value, MakePair(MakeCharacter(ch), Unbox(obj)));
}

static void StringOutputPutString(void * ctx, FObject obj, FCh * s, int sl)
{
    FAssert(BoxP(obj));

    ToStringOutputContext(ctx)->Count += sl;
//    AsBox(obj)->Value = MakePair(MakeString(s, sl), Unbox(obj));
    Modify(FBox, obj, Value, MakePair(MakeString(s, sl), Unbox(obj)));
}

static void StringOutputPutStringC(void * ctx, FObject obj, char * s)
{
    FAssert(BoxP(obj));

    ToStringOutputContext(ctx)->Count += strlen(s);
//    AsBox(obj)->Value = MakePair(MakeStringC(s), Unbox(obj));
    Modify(FBox, obj, Value, MakePair(MakeStringC(s), Unbox(obj)));
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

    int idx = ToStringOutputContext(AsPort(port)->Context)->Count;
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

            idx -= (int) StringLength(First(lst));

            for (unsigned int sdx = 0; sdx < StringLength(First(lst)); sdx++)
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
        RaiseExceptionC(R.Assertion, "open-output-string", "expected no arguments",
                EmptyListObject);

    return(MakeStringOutputPort());
}

Define("get-output-string", GetOutputStringPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "get-output-string", "expected one argument",
                EmptyListObject);

    if (OutputPortP(argv[0]) == 0 || AsPort(argv[0])->Output != &StringOutputPort)
        RaiseExceptionC(R.Assertion, "get-output-string", "expected a string output port",
                List(argv[0]));

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

static int NumericP(FCh ch)
{
    int dv = DigitValue(ch);
    if (dv < 0 || dv > 9)
        return(0);
    return(1);
}

static int SymbolCharP(FCh ch)
{
    return(AlphabeticP(ch) || NumericP(ch) || ch == '!' || ch == '$' || ch == '%' || ch == '&'
             || ch =='*' || ch == '+' || ch == '-' || ch == '.' || ch == '/' || ch == ':'
             || ch == '<' || ch == '=' || ch == '>' || ch == '?' || ch == '@' || ch == '^'
             || ch == '_' || ch == '~');
}

#define DotObject ((FObject) -1)
#define EolObject ((FObject *) -2)

static FCh DoReadStringHexChar(FObject port)
{
    FCh s[16];
    int sl = 2;
    FCh ch;
    int eof;

    for (;;)
    {
        ch = GetCh(port, &eof);
        if (eof)
            RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading string",
                    List(port));

        if (ch == ';')
            break;

        s[sl] = ch;
        sl += 1;
        if (sl == sizeof(s) / sizeof(FCh))
            RaiseExceptionC(R.Lexical, "read",
                    "missing ; to terminate \\x<hex-value> in string", List(port));
    }

    FFixnum n;

    if (StringToNumber(s + 2, sl - 2, &n, 16) == 0)
    {
        s[0] = '\\';
        s[1] = 'x';

        RaiseExceptionC(R.Lexical, "read", "expected a valid hexidecimal value for a character",
                List(port, MakeString(s, sl)));
    }

    return((FCh) n);
}

static FObject DoReadString(FObject port)
{
    FCh s[512];
    int sl = 0;
    FCh ch;
    int eof = 0;

    for (;;)
    {
        ch = GetCh(port, &eof);
        if (eof)
            break;

Again:

        if (ch == '"')
            break;

        if (ch == '\\')
        {
            ch = GetCh(port, &eof);
            if (eof)
                break;

            if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
            {
                while (ch == ' ' || ch == '\t')
                {
                    ch = GetCh(port, &eof);
                    if (eof)
                        goto UnexpectedEof;
                }

                while (ch == '\r' || ch == '\n')
                {
                    ch = GetCh(port, &eof);
                    if (eof)
                        goto UnexpectedEof;
                }

                while (ch == ' ' || ch == '\t')
                {
                    ch = GetCh(port, &eof);
                    if (eof)
                        goto UnexpectedEof;
                }

                goto Again;
            }

            switch (ch)
            {
            case 'a': ch = 0x0007; break;
            case 'b': ch = 0x0008; break;
            case 't': ch = 0x0009; break;
            case 'n': ch = 0x000A; break;
            case 'r': ch = 0x000D; break;
            case '"': ch = 0x0022; break;
            case '\\': ch = 0x005C; break;
            case '|': ch = 0x007C; break;
            case 'x':
                ch = DoReadStringHexChar(port);
                break;
            default:
                RaiseExceptionC(R.Lexical, "read", "unexpected character following \\",
                        List(port, MakeCharacter(ch)));
            }
        }

        s[sl] = ch;
        sl += 1;
        if (sl == sizeof(s) / sizeof(FCh))
            RaiseExceptionC(R.Restriction, "read", "string too long", List(port));
    }

    if (eof)
    {
UnexpectedEof:

        RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading string", List(port));
    }

    return(MakeString(s, sl));
}

static int DoReadToken(FObject port, int ch, FCh * s, int msl)
{
    int sl;
    int eof;

    sl = 0;
    for (;;)
    {
        s[sl] = ch;
        sl += 1;
        if (sl == msl)
            RaiseExceptionC(R.Restriction, "read", "symbol or number too long", List(port));

        ch = PeekCh(port, &eof);
        if (eof || SymbolCharP(ch) == 0)
            break;

        GetCh(port, &eof);
        FAssert(eof == 0);
    }

    return(sl);
}

static FObject DoReadSymbolOrNumber(FObject port, int ch, int rif, int fcf)
{
    FCh s[256];
    FFixnum n;
    int ln;

    if (rif)
        ln = GetLocation(port);

    int sl = DoReadToken(port, ch, s, sizeof(s) / sizeof(FCh));

    if (StringToNumber(s, sl, &n, 10))
        return(MakeFixnum(n));

    FObject sym = fcf ? StringToSymbol(FoldcaseString(MakeString(s, sl)))
            : StringLengthToSymbol(s, sl);
    if (rif)
        return(MakeIdentifier(sym, ln));
    return(sym);
}

static FObject DoReadList(FObject port, int rif, int fcf);
static FObject DoReadSharp(FObject port, int rif, int fcf)
{
    int eof;

    int ch = GetCh(port, &eof);
    if (eof)
        RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading #", List(port));

    if (ch == 't' || ch == 'f')
    {
        FCh s[16];

        int sl = DoReadToken(port, ch, s, sizeof(s) / sizeof(FCh));

        if (StringCEqualP("t", s, sl) || StringCEqualP("true", s, sl))
            return(TrueObject);

        if (StringCEqualP("f", s, sl) || StringCEqualP("false", s, sl))
            return(FalseObject);

        RaiseExceptionC(R.Lexical, "read", "unexpected character(s) following #",
                List(port, MakeString(s, sl)));
    }
    else if (ch == '\\')
    {
        FCh s[32];

        ch = GetCh(port, &eof);
        if (eof)
            RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading #\\", List(port));

        if (SymbolCharP(ch) == 0)
            return(MakeCharacter(ch));

        int sl = DoReadToken(port, ch, s, sizeof(s) / sizeof(FCh));

        if (sl == 1)
            return(MakeCharacter(ch));

        if (s[0] == 'x')
        {
            FAssert(sl > 1);

            FFixnum n;

            if (StringToNumber(s + 1, sl - 1, &n, 16) == 0 || n < 0)
                RaiseExceptionC(R.Lexical, "read", "expected #\\x<hex value>",
                        List(port, MakeString(s, sl)));

            return(MakeCharacter(n));
        }

        if (StringCEqualP("alarm", s, sl))
            return(MakeCharacter(0x0007));

        if (StringCEqualP("backspace", s, sl))
            return(MakeCharacter(0x0008));

        if (StringCEqualP("delete", s, sl))
            return(MakeCharacter(0x007F));

        if (StringCEqualP("escape", s, sl))
            return(MakeCharacter(0x001B));

        if (StringCEqualP("newline", s, sl))
            return(MakeCharacter(0x000A));

        if (StringCEqualP("null", s, sl))
            return(MakeCharacter(0x0000));

        if (StringCEqualP("return", s, sl))
            return(MakeCharacter(0x000D));

        if (StringCEqualP("space", s, sl))
            return(MakeCharacter(' '));

        if (StringCEqualP("tab", s, sl))
            return(MakeCharacter(0x0009));

        RaiseExceptionC(R.Lexical, "read", "unexpected character name",
                List(port, MakeString(s, sl)));
    }
    else if (ch == 'b' || ch == 'o' || ch == 'd' || ch == 'x')
    {
        FCh s[32];
        FFixnum n;
        int b = (ch == 'b' ? 2 : (ch == 'o' ? 8 : (ch == 'd' ? 10 : 16)));

        ch = GetCh(port, &eof);
        if (eof)
            RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading constant",
                    List(port));

        int sl = DoReadToken(port, ch, s, sizeof(s) / sizeof(FCh));

        if (StringToNumber(s, sl, &n, b) == 0)
            RaiseExceptionC(R.Lexical, "read", "expected a numerical constant",
                    List(port, MakeString(s, sl)));

        return(MakeFixnum(n));
    }
    else if (ch ==  '(')
        return(ListToVector(DoReadList(port, rif, fcf)));
    else if (ch == 'u')
    {
        ch = GetCh(port, &eof);
        if (eof)
            RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading bytevector",
                    List(port));
        if (ch != '8')
            RaiseExceptionC(R.Lexical, "read", "expected #\u8(", List(port));

        ch = GetCh(port, &eof);
        if (eof)
            RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading bytevector",
                    List(port));
        if (ch != '(')
            RaiseExceptionC(R.Lexical, "read", "expected #\u8(", List(port));
        return(U8ListToBytevector(DoReadList(port, rif, fcf)));
    }

    RaiseExceptionC(R.Lexical, "read", "unexpected character following #",
            List(port, MakeCharacter(ch)));

    return(NoValueObject);
}

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
                return(DoReadSharp(port, rif, fcf));

            case '"':
                return(DoReadString(port));

            case '(':
                return(DoReadList(port, rif, fcf));

            case ')':
                if (rlf)
                    return(EolObject);
                RaiseExceptionC(R.Lexical, "read", "unexpected )", List(port));
                break;

            case '.':
                ch = PeekCh(port, &eof);
                if (eof)
                    RaiseExceptionC(R.Lexical, "read",
                            "unexpected end-of-file reading dotted pair", List(port));
                if (ch == '.')
                {
                    GetCh(port, &eof);

                    ch = GetCh(port, &eof);
                    if (eof)
                        RaiseExceptionC(R.Lexical, "read",
                                "unexpected end-of-file reading ...", List(port));
                        if (ch != '.')
                            RaiseExceptionC(R.Lexical, "read", "expected ...", List(port));
                    return(rif ? MakeIdentifier(R.EllipsisSymbol, GetLocation(port))
                            : R.EllipsisSymbol);
                }

                if (rlf)
                    return(DotObject);
                RaiseExceptionC(R.Lexical, "read", "unexpected dotted pair", List(port));
                break;

            case '\'':
            {
                FObject obj;
                obj = DoRead(port, 0, 0, rif, fcf);
                return(MakePair( rif ? MakeIdentifier(R.QuoteSymbol,
                        GetLocation(port)) : R.QuoteSymbol, MakePair(obj, EmptyListObject)));
            }

            case '`':
            {
                FObject obj;
                obj = DoRead(port, 0, 0, rif, fcf);
                return(MakePair(
                        rif ? MakeIdentifier(R.QuasiquoteSymbol, GetLocation(port))
                        : R.QuasiquoteSymbol, MakePair(obj, EmptyListObject)));
            }

            case ',':
            {
                ch = PeekCh(port, &eof);
                if (eof)
                    RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading unquote",
                            List(port));

                FObject sym;
                if (ch == '@')
                {
                    GetCh(port, &eof);
                    sym = R.UnquoteSplicingSymbol;
                }
                else
                    sym = R.UnquoteSymbol;

                FObject obj;
                obj = DoRead(port, 0, 0, rif, fcf);
                return(MakePair(
                        rif ? MakeIdentifier(sym, GetLocation(port)) : sym,
                        MakePair(obj, EmptyListObject)));
            }

            default:
                if (SymbolCharP(ch))
                    return(DoReadSymbolOrNumber(port, ch, rif, fcf));
                break;
            }
        }
    }

Eof:

    if (eaf == 0)
        RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading list or vector",
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
            RaiseExceptionC(R.Lexical, "read", "bad dotted pair", List(port));
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
        FObject val = EqHashtableRef(ht, obj, MakeFixnum(0));

        FAssert(FixnumP(val));

        EqHashtableSet(ht, obj, MakeFixnum(AsFixnum(val) + 1));

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
                for (unsigned int idx = 0; idx < VectorLength(obj); idx++)
                    cnt = FindSharedObjects(ht, AsVector(obj)->Vector[idx], cnt, cof);
            }
            else if (ProcedureP(obj))
                cnt = FindSharedObjects(ht, AsProcedure(obj)->Code, cnt, cof);
            else
            {
                FAssert(GenericRecordP(obj));

                for (unsigned int fdx = 0; fdx < RecordNumFields(obj); fdx++)
                    cnt = FindSharedObjects(ht, AsGenericRecord(obj)->Fields[fdx], cnt, cof);
            }

            if (cof)
            {
                val = EqHashtableRef(ht, obj, MakeFixnum(0));
                FAssert(FixnumP(val));
                FAssert(AsFixnum(val) > 0);

                if (AsFixnum(val) == 1)
                    EqHashtableDelete(ht, obj);
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
        FObject val = EqHashtableRef(ToWriteSharedCtx(ctx)->Hashtable, obj, FalseObject);

        if (BooleanP(val))
        {
            if (val == TrueObject)
            {
                ToWriteSharedCtx(ctx)->Label += 1;
                EqHashtableSet(ToWriteSharedCtx(ctx)->Hashtable, obj,
                        MakeFixnum(ToWriteSharedCtx(ctx)->Label));

                PutCh(port, '#');
                FCh s[8];
                int sl = NumberAsString(ToWriteSharedCtx(ctx)->Label, s, 10);
                PutString(port, s, sl);
                PutCh(port, '=');
            }

            if (PairP(obj))
            {
                PutCh(port, '(');
                for (;;)
                {
                    wfn(port, First(obj), df, wfn, ctx);
                    if (PairP(Rest(obj)) && EqHashtableRef(ToWriteSharedCtx(ctx)->Hashtable,
                            Rest(obj), FalseObject) == FalseObject)
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
    FObject ht = MakeEqHashtable(23);

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
    FObject ht = MakeEqHashtable(23);

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

static void WritePair(FObject port, FObject obj, int df, FWriteFn wfn, void * ctx)
{
    FAssert(PairP(obj));

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
}

static void WriteIndirectObject(FObject port, FObject obj, int df, FWriteFn wfn, void * ctx)
{
    switch (IndirectTag(obj))
    {
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
        if (df)
            PutString(port, AsString(obj)->String, StringLength(obj));
        else
        {
            PutCh(port, '"');

            for (unsigned int idx = 0; idx < StringLength(obj); idx++)
            {
                FCh ch = AsString(obj)->String[idx];
                if (ch == '\\' || ch == '"')
                    PutCh(port, '\\');
                PutCh(port, ch);
            }

            PutCh(port, '"');
        }
        break;

    case VectorTag:
    {
        PutStringC(port, "#(");
        for (unsigned int idx = 0; idx < VectorLength(obj); idx++)
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
        FCh s[8];
        int sl;

        PutStringC(port, "#u8(");
        for (unsigned int idx = 0; idx < BytevectorLength(obj); idx++)
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
            PutString(port, AsString(AsPort(obj)->Name)->String, StringLength(AsPort(obj)->Name));
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

        if (AsProcedure(obj)->Reserved & PROCEDURE_FLAG_CLOSURE)
            PutStringC(port, " closure");

        if (AsProcedure(obj)->Reserved & PROCEDURE_FLAG_PARAMETER)
            PutStringC(port, " parameter");

        if (AsProcedure(obj)->Reserved & PROCEDURE_FLAG_CONTINUATION)
            PutStringC(port, " continuation");

//        PutCh(port, ' ');
//        wfn(port, AsProcedure(obj)->Code, df, wfn, ctx);
        PutCh(port, '>');
        break;
    }

    case SymbolTag:
        PutString(port, AsString(AsSymbol(obj)->String)->String,
                StringLength(AsSymbol(obj)->String));
        break;

    case RecordTypeTag:
    {
        FCh s[16];
        int sl = NumberAsString((FFixnum) obj, s, 16);

        PutStringC(port, "#<record-type: #x");
        PutString(port, s, sl);
        PutCh(port, ' ');
        wfn(port, RecordTypeName(obj), df, wfn, ctx);

        for (unsigned int fdx = 1; fdx < RecordTypeNumFields(obj); fdx += 1)
        {
            PutCh(port, ' ');
            wfn(port, AsRecordType(obj)->Fields[fdx], df, wfn, ctx);
        }

        PutStringC(port, ">");
        break;
    }

    case RecordTag:
    {
        FObject rt = AsGenericRecord(obj)->Fields[0];
        FCh s[16];
        int sl = NumberAsString((FFixnum) obj, s, 16);

        PutStringC(port, "#<(");
        wfn(port, RecordTypeName(rt), df, wfn, ctx);
        PutStringC(port, ": #x");
        PutString(port, s, sl);

        for (unsigned int fdx = 1; fdx < RecordNumFields(obj); fdx++)
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

    case ThreadTag:
        WriteThread(port, obj, df);
        break;

    case ExclusiveTag:
        WriteExclusive(port, obj, df);
        break;

    case ConditionTag:
        WriteCondition(port, obj, df);
        break;

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
        if (AsCharacter(obj) < 128)
        {
            if (df == 0)
                PutStringC(port, "#\\");
            PutCh(port, AsCharacter(obj));
        }
        else
        {
            FCh s[16];
            int sl = NumberAsString(AsCharacter(obj), s, 16);
            PutStringC(port, "#\\x");
            PutString(port, s, sl);
        }
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
    else if (PairP(obj))
        WritePair(port, obj, df, wfn, ctx);
    else if (IndirectP(obj))
        WriteIndirectObject(port, obj, df, wfn, ctx);
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
        RaiseExceptionC(R.Assertion, "write", "expected one or two arguments", EmptyListObject);

    if (argc == 2)
    {
        if (OutputPortP(argv[1]) == 0)
            RaiseExceptionC(R.Assertion, "write", "expected an output port", List(argv[1]));

        port = argv[1];
    }
    else
        port = R.StandardOutput;

    Write(port, argv[0], 0);

    return(NoValueObject);
}

Define("display", DisplayPrimitive)(int argc, FObject argv[])
{
    FObject port;

    if (argc < 1 || argc > 2)
        RaiseExceptionC(R.Assertion, "display", "expected one or two arguments", EmptyListObject);

    if (argc == 2)
    {
        if (OutputPortP(argv[1]) == 0)
            RaiseExceptionC(R.Assertion, "display", "expected an output port", List(argv[1]));

        port = argv[1];
    }
    else
        port = R.StandardOutput;

    Write(port, argv[0], 1);

    return(NoValueObject);
}

Define("write-shared", WriteSharedPrimitive)(int argc, FObject argv[])
{
    FObject port;

    if (argc < 1 || argc > 2)
        RaiseExceptionC(R.Assertion, "write-shared", "expected one or two arguments",
                EmptyListObject);

    if (argc == 2)
    {
        if (OutputPortP(argv[1]) == 0)
            RaiseExceptionC(R.Assertion, "write-shared", "expected an output port",
                    List(argv[1]));

        port = argv[1];
    }
    else
        port = R.StandardOutput;

    WriteShared(port, argv[0], 0);

    return(NoValueObject);
}

Define("display-shared", DisplaySharedPrimitive)(int argc, FObject argv[])
{
    FObject port;

    if (argc < 1 || argc > 2)
        RaiseExceptionC(R.Assertion, "display-shared", "expected one or two arguments",
                EmptyListObject);

    if (argc == 2)
    {
        if (OutputPortP(argv[1]) == 0)
            RaiseExceptionC(R.Assertion, "display-shared", "expected an output port",
                    List(argv[1]));

        port = argv[1];
    }
    else
        port = R.StandardOutput;

    WriteShared(port, argv[0], 1);

    return(NoValueObject);
}

Define("write-simple", WriteSimplePrimitive)(int argc, FObject argv[])
{
    FObject port;

    if (argc < 1 || argc > 2)
        RaiseExceptionC(R.Assertion, "write-simple", "expected one or two arguments",
                EmptyListObject);

    if (argc == 2)
    {
        if (OutputPortP(argv[1]) == 0)
            RaiseExceptionC(R.Assertion, "write-simple", "expected an output port", List(argv[1]));

        port = argv[1];
    }
    else
        port = R.StandardOutput;

    WriteSimple(port, argv[0], 0);

    return(NoValueObject);
}

Define("display-simple", DisplaySimplePrimitive)(int argc, FObject argv[])
{
    FObject port;

    if (argc < 1 || argc > 2)
        RaiseExceptionC(R.Assertion, "display-simple", "expected one or two arguments",
                EmptyListObject);

    if (argc == 2)
    {
        if (OutputPortP(argv[1]) == 0)
            RaiseExceptionC(R.Assertion, "display-simple", "expected an output port",
                    List(argv[1]));

        port = argv[1];
    }
    else
        port = R.StandardOutput;

    WriteSimple(port, argv[0], 1);

    return(NoValueObject);
}

Define("newline", NewlinePrimitive)(int argc, FObject argv[])
{
    FObject port;

    if (argc > 1)
        RaiseExceptionC(R.Assertion, "newline", "expected zero or one arguments", EmptyListObject);

    if (argc == 1)
    {
        if (OutputPortP(argv[0]) == 0)
            RaiseExceptionC(R.Assertion, "newline", "expected an output port", List(argv[0]));

        port = argv[1];
    }
    else
        port = R.StandardOutput;

    PutCh(port, '\n');

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
    &DisplaySimplePrimitive,
    &NewlinePrimitive
};

void SetupIO()
{
    R.StandardInput = MakeFILEPort(MakeStringC("--standard-input--"), stdin, 1, 0, 1);
    R.StandardOutput = MakeFILEPort(MakeStringC("--standard-output--"), stdout, 0, 1, 1);
    R.QuoteSymbol = StringCToSymbol("quote");
    R.QuasiquoteSymbol = StringCToSymbol("quasiquote");
    R.UnquoteSymbol = StringCToSymbol("unquote");
    R.UnquoteSplicingSymbol = StringCToSymbol("unquote-splicing");

    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);

    SetupPrettyPrint();
}
