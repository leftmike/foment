/*

Foment

*/

#ifdef FOMENT_WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif // FOMENT_WIN32

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <io.h>
#include "foment.hpp"
#include "io.hpp"
#include "unicode.hpp"

#define NOT_PEEKED ((unsigned int) -1)

// ---- Binary Ports ----

FObject MakeBinaryPort(FObject nam, FObject obj, void * ictx, void * octx, FCloseInputFn cifn,
    FCloseOutputFn cofn, FFlushOutputFn fofn, FReadBytesFn rbfn, FByteReadyPFn brpfn,
    FWriteBytesFn wbfn)
{
    FAssert((cifn == 0 && rbfn == 0 && brpfn == 0) || (cifn != 0 && rbfn != 0 && brpfn != 0));
    FAssert((cofn == 0 && wbfn == 0 && fofn == 0) || (cofn != 0 && wbfn != 0 && fofn != 0));
    FAssert(cifn != 0 || cofn != 0);

    FBinaryPort * port = (FBinaryPort *) MakeObject(sizeof(FBinaryPort), BinaryPortTag);
    port->Generic.Flags = BinaryPortTag
            | (cifn != 0 ? (PORT_FLAG_INPUT | PORT_FLAG_INPUT_OPEN) : 0)
            | (cofn != 0 ? (PORT_FLAG_OUTPUT | PORT_FLAG_OUTPUT_OPEN) : 0);
    port->Generic.Name = nam;
    port->Generic.Object = obj;
    port->Generic.InputContext = ictx;
    port->Generic.OutputContext = octx;
    port->Generic.CloseInputFn = cifn;
    port->Generic.CloseOutputFn = cofn;
    port->Generic.FlushOutputFn = fofn;
    port->ReadBytesFn = rbfn;
    port->ByteReadyPFn = brpfn;
    port->WriteBytesFn = wbfn;
    port->PeekedByte = NOT_PEEKED;

    return(port);
}

unsigned int ReadBytes(FObject port, FByte * b, unsigned int bl)
{
    FAssert(BinaryPortP(port) && InputPortOpenP(port));
    FAssert(bl > 0);

    if (AsBinaryPort(port)->PeekedByte != NOT_PEEKED)
    {
        FAssert(AsBinaryPort(port)->PeekedByte >= 0 && AsBinaryPort(port)->PeekedByte <= 0xFF);

        *b = AsBinaryPort(port)->PeekedByte;
        AsBinaryPort(port)->PeekedByte = NOT_PEEKED;

        if (bl == 1)
            return(1);

        return(AsBinaryPort(port)->ReadBytesFn(port, b + 1, bl - 1) + 1);
    }

    return(AsBinaryPort(port)->ReadBytesFn(port, b, bl));
}

int PeekByte(FObject port, FByte * b)
{
    FAssert(BinaryPortP(port) && InputPortOpenP(port));

    if (AsBinaryPort(port)->PeekedByte != NOT_PEEKED)
    {
        FAssert(AsBinaryPort(port)->PeekedByte >= 0 && AsBinaryPort(port)->PeekedByte <= 0xFF);

        *b = (FByte) AsBinaryPort(port)->PeekedByte;
        return(0);
    }

    if (AsBinaryPort(port)->ReadBytesFn(port, b, 1) == 0)
        return(1);

    AsBinaryPort(port)->PeekedByte = *b;
    return(0);
}

int ByteReadyP(FObject port)
{
    FAssert(BinaryPortP(port) && InputPortOpenP(port));

    if (AsBinaryPort(port)->PeekedByte != NOT_PEEKED)
        return(1);

    return(AsBinaryPort(port)->ByteReadyPFn(port));
}

void WriteBytes(FObject port, void * b, unsigned int bl)
{
    FAssert(BinaryPortP(port) && OutputPortOpenP(port));
    FAssert(bl > 0);

    AsBinaryPort(port)->WriteBytesFn(port, b, bl);
}

static void StdioCloseInput(FObject port)
{
    FAssert(BinaryPortP(port));

    if (OutputPortOpenP(port) == 0
            || AsGenericPort(port)->InputContext != AsGenericPort(port)->OutputContext)
        fclose((FILE *) AsGenericPort(port)->InputContext);
}

static void StdioCloseOutput(FObject port)
{
    FAssert(BinaryPortP(port));

    if (InputPortOpenP(port) == 0
            || AsGenericPort(port)->InputContext != AsGenericPort(port)->OutputContext)
        fclose((FILE *) AsGenericPort(port)->OutputContext);
}

static void StdioFlushOutput(FObject port)
{
    FAssert(BinaryPortP(port) && OutputPortOpenP(port));

    fflush((FILE *) AsGenericPort(port)->OutputContext);
}

static unsigned int StdioReadBytes(FObject port, void * b, unsigned int bl)
{
    FAssert(BinaryPortP(port) && InputPortOpenP(port));
    FAssert(AsGenericPort(port)->InputContext != 0);

    return(fread(b, 1, bl, (FILE *) AsGenericPort(port)->InputContext));
}

static int StdioByteReadyP(FObject port)
{
    
    
    return(0);
}

static void StdioWriteBytes(FObject port, void * b, unsigned int bl)
{
    FAssert(BinaryPortP(port) && OutputPortOpenP(port));
    FAssert(AsGenericPort(port)->OutputContext != 0);

    fwrite(b, 1, bl, (FILE *) AsGenericPort(port)->OutputContext);
}

static FObject MakeStdioPort(FObject nam, FILE * ifp, FILE * ofp)
{
    return(MakeBinaryPort(nam, NoValueObject, ifp, ofp, ifp ? StdioCloseInput : 0,
            ofp ? StdioCloseOutput : 0, ofp ? StdioFlushOutput : 0, ifp ? StdioReadBytes : 0,
            ifp ? StdioByteReadyP : 0, ofp ? StdioWriteBytes : 0));
}

// ---- Textual Ports ----

FObject MakeTextualPort(FObject nam, FObject obj, void * ictx, void * octx, FCloseInputFn cifn,
    FCloseOutputFn cofn, FFlushOutputFn fofn, FReadChFn rcfn, FCharReadyPFn crpfn,
    FWriteStringFn wsfn)
{
    FAssert((cifn == 0 && rcfn == 0 && crpfn == 0) || (cifn != 0 && rcfn != 0 && crpfn != 0));
    FAssert((cofn == 0 && wsfn == 0 && fofn == 0) || (cofn != 0 && wsfn != 0 && fofn != 0));
    FAssert(cifn != 0 || cofn != 0);

    FTextualPort * port = (FTextualPort *) MakeObject(sizeof(FTextualPort), TextualPortTag);
    port->Generic.Flags = TextualPortTag
            | (cifn != 0 ? (PORT_FLAG_INPUT | PORT_FLAG_INPUT_OPEN) : 0)
            | (cofn != 0 ? (PORT_FLAG_OUTPUT | PORT_FLAG_OUTPUT_OPEN) : 0);
    port->Generic.Name = nam;
    port->Generic.Object = obj;
    port->Generic.InputContext = ictx;
    port->Generic.OutputContext = octx;
    port->Generic.CloseInputFn = cifn;
    port->Generic.CloseOutputFn = cofn;
    port->Generic.FlushOutputFn = fofn;
    port->ReadChFn = rcfn;
    port->CharReadyPFn = crpfn;
    port->WriteStringFn = wsfn;
    port->PeekedChar = NOT_PEEKED;

    return(port);
}

unsigned int ReadCh(FObject port, FCh * ch)
{
    FAssert(TextualPortP(port) && InputPortOpenP(port));

    if (AsTextualPort(port)->PeekedChar != NOT_PEEKED)
    {
        *ch = AsTextualPort(port)->PeekedChar;
        AsTextualPort(port)->PeekedChar = NOT_PEEKED;

        return(1);
    }

    return(AsTextualPort(port)->ReadChFn(port, ch));
}

unsigned int PeekCh(FObject port, FCh * ch)
{
    FAssert(TextualPortP(port) && InputPortOpenP(port));

    if (AsTextualPort(port)->PeekedChar != NOT_PEEKED)
    {
        *ch = AsTextualPort(port)->PeekedChar;
        return(1);
    }

    if (ReadCh(port, ch) == 0)
        return(0);

    AsTextualPort(port)->PeekedChar = *ch;
    return(1);
}

int CharReadyP(FObject port)
{
    FAssert(TextualPortP(port) && InputPortOpenP(port));

    if (AsTextualPort(port)->PeekedChar != NOT_PEEKED)
        return(1);

    return(AsTextualPort(port)->CharReadyPFn(port));
}

void WriteCh(FObject port, FCh ch)
{
    FAssert(TextualPortP(port) && OutputPortOpenP(port));

    AsTextualPort(port)->WriteStringFn(port, &ch, 1);
}

void WriteString(FObject port, FCh * s, int sl)
{
    FAssert(TextualPortP(port) && OutputPortOpenP(port));

    AsTextualPort(port)->WriteStringFn(port, s, sl);
}

void WriteStringC(FObject port, char * s)
{
    FAssert(TextualPortP(port) && OutputPortOpenP(port));

    while (*s)
    {
        FCh ch = *s;
        AsTextualPort(port)->WriteStringFn(port, &ch, 1);
        s += 1;
    }
}

void CloseInput(FObject port)
{
    FAssert(BinaryPortP(port) || TextualPortP(port));

    if (InputPortOpenP(port))
    {
        AsGenericPort(port)->Flags &= ~PORT_FLAG_INPUT_OPEN;

        AsGenericPort(port)->CloseInputFn(port);
    }
}

void CloseOutput(FObject port)
{
    FAssert(BinaryPortP(port) || TextualPortP(port));

    if (OutputPortOpenP(port))
    {
        AsGenericPort(port)->Flags &= ~PORT_FLAG_OUTPUT_OPEN;

        AsGenericPort(port)->CloseOutputFn(port);
    }
}

void FlushOutput(FObject port)
{
    FAssert(BinaryPortP(port) || TextualPortP(port));
    FAssert(OutputPortOpenP(port));

    AsGenericPort(port)->FlushOutputFn(port);
}

static void TranslatorCloseInput(FObject port)
{
    FAssert(BinaryPortP(AsGenericPort(port)->Object));

    CloseInput(AsGenericPort(port)->Object);
}

static void TranslatorCloseOutput(FObject port)
{
    FAssert(BinaryPortP(AsGenericPort(port)->Object));

    CloseOutput(AsGenericPort(port)->Object);
}

static void TranslatorFlushOutput(FObject port)
{
    FAssert(BinaryPortP(AsGenericPort(port)->Object));

    FlushOutput(AsGenericPort(port)->Object);
}

static FObject MakeTranslatorPort(FObject port, FReadChFn rcfn, FCharReadyPFn crpfn,
    FWriteStringFn wsfn)
{
    FAssert(BinaryPortP(port));

    return(MakeTextualPort(AsGenericPort(port)->Name, port, 0, 0,
            (AsGenericPort(port)->Flags & PORT_FLAG_INPUT) ? TranslatorCloseInput : 0,
            (AsGenericPort(port)->Flags & PORT_FLAG_OUTPUT) ? TranslatorCloseOutput : 0,
            (AsGenericPort(port)->Flags & PORT_FLAG_OUTPUT) ? TranslatorFlushOutput : 0,
            (AsGenericPort(port)->Flags & PORT_FLAG_INPUT) ? rcfn : 0,
            (AsGenericPort(port)->Flags & PORT_FLAG_INPUT) ? crpfn : 0,
            (AsGenericPort(port)->Flags & PORT_FLAG_OUTPUT) ? wsfn : 0));
}

static unsigned int Latin1ReadCh(FObject port, FCh * ch)
{
    FAssert(BinaryPortP(AsGenericPort(port)->Object));

    unsigned char b;

    if (ReadBytes(AsGenericPort(port)->Object, &b, 1) != 1)
        return(0);

    *ch = b;
    return(1);
}

static int Latin1CharReadyP(FObject port)
{
    FAssert(BinaryPortP(AsGenericPort(port)->Object));

    return(ByteReadyP(AsGenericPort(port)->Object));
}

void Latin1WriteString(FObject port, FCh * s, unsigned int sl)
{
    FAssert(BinaryPortP(AsGenericPort(port)->Object));

    for (unsigned int sdx = 0; sdx < sl; sdx++)
    {
        unsigned char b;

        if (s[sdx] > 0xFF)
            b = '?';
        else
            b = (unsigned char) s[sdx];

        WriteBytes(AsGenericPort(port)->Object, &b, 1);
    }
}

FObject MakeLatin1Port(FObject port)
{
    return(MakeTranslatorPort(port, Latin1ReadCh, Latin1CharReadyP, Latin1WriteString));
}

FObject OpenInputFile(FObject fn)
{
    SCh * ss;
    ConvertToSystem(fn, ss);

    FILE * fp = _wfopen(ss, L"r");
    if (fp == 0)
        return(NoValueObject);

    return(MakeLatin1Port(MakeStdioPort(fn, fp, 0)));
}

FObject OpenOutputFile(FObject fn)
{
    SCh * ss;
    ConvertToSystem(fn, ss);

    FILE * fp = _wfopen(ss, L"w");
    if (fp == 0)
        return(NoValueObject);

    return(MakeLatin1Port(MakeStdioPort(fn, 0, fp)));
}

static void SinCloseInput(FObject port)
{
    FAssert(TextualPortP(port));

    AsGenericPort(port)->Object = NoValueObject;
}

static unsigned int SinReadCh(FObject port, FCh * ch)
{
    FAssert(TextualPortP(port));

    FObject s = AsGenericPort(port)->Object;
    unsigned int sdx = (unsigned int) AsGenericPort(port)->InputContext;

    FAssert(StringP(s));
    FAssert(sdx <= StringLength(s));

    if (sdx == StringLength(s))
        return(0);

    *ch = AsString(s)->String[sdx];
    AsGenericPort(port)->InputContext = (void *) (sdx + 1);

    return(1);
}

static int SinCharReadyP(FObject port)
{
    return(1);
}

FObject MakeStringInputPort(FObject s)
{
    FAssert(StringP(s));

    return(MakeTextualPort(NoValueObject, s, 0, 0, SinCloseInput, 0, 0, SinReadCh, SinCharReadyP,
            0));
}

static void SoutCloseOutput(FObject port)
{
    FAssert(TextualPortP(port));

    AsGenericPort(port)->Object = NoValueObject;
}

static void SoutFlushOutput(FObject port)
{
    // Nothing.
}

static void SoutWriteString(FObject port, FCh * s, unsigned int sl)
{
    FAssert(TextualPortP(port));
    FAssert(AsGenericPort(port)->Object == EmptyListObject || PairP(AsGenericPort(port)->Object));

    FObject lst = AsGenericPort(port)->Object;

    for (unsigned int sdx = 0; sdx < sl; sdx++)
        lst = MakePair(MakeCharacter(s[sdx]), lst);

    AsGenericPort(port)->Object = lst;
}

FObject GetOutputString(FObject port)
{
    FAssert(StringOutputPortP(port));
    FAssert(AsGenericPort(port)->Object == EmptyListObject || PairP(AsGenericPort(port)->Object));

    FObject lst = AsGenericPort(port)->Object;
    int sl = ListLength(lst);
    FObject s = MakeString(0, sl);
    int sdx = sl;

    while (PairP(lst))
    {
        sdx -= 1;

        FAssert(sdx >= 0);
        FAssert(CharacterP(First(lst)));

        AsString(s)->String[sdx] = AsCharacter(First(lst));
        lst = Rest(lst);
    }

    FAssert(lst == EmptyListObject);

    return(s);
}

FObject MakeStringOutputPort()
{
    FObject port = MakeTextualPort(NoValueObject, EmptyListObject, 0, 0, 0, SoutCloseOutput,
            SoutFlushOutput, 0, 0, SoutWriteString);
    AsGenericPort(port)->Flags |= PORT_FLAG_STRING_OUTPUT;
    return(port);
}

static void CinCloseInput(FObject port)
{
    // Nothing.
}

static unsigned int CinReadCh(FObject port, FCh * ch)
{
    FAssert(TextualPortP(port));

    char * s = (char *) AsGenericPort(port)->InputContext;

    if (*s == 0)
        return(0);

    *ch = *s;
    AsGenericPort(port)->InputContext = (void *) (s + 1);

    return(1);
}

static int CinCharReadyP(FObject port)
{
    return(1);
}

FObject MakeStringCInputPort(char * s)
{
    return(MakeTextualPort(NoValueObject, NoValueObject, s, 0, CinCloseInput, 0, 0, CinReadCh,
            CinCharReadyP, 0));
}

int GetLocation(FObject port)
{
    
    
    
    
    return(0);
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

    for (;;)
    {
        if (ReadCh(port, &ch) == 0)
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

static FObject DoReadString(FObject port, FCh tch)
{
    FCh s[512];
    int sl = 0;
    FCh ch;

    FAssert(tch == '"' || tch == '|');

    for (;;)
    {
        if (ReadCh(port, &ch) == 0)
            goto UnexpectedEof;

Again:

        if (ch == tch)
            break;

        if (ch == '\\')
        {
            if (ReadCh(port, &ch) == 0)
                goto UnexpectedEof;

            if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
            {
                while (ch == ' ' || ch == '\t')
                {
                    if (ReadCh(port, &ch) == 0)
                        goto UnexpectedEof;
                }

                while (ch == '\r' || ch == '\n')
                {
                    if (ReadCh(port, &ch) == 0)
                        goto UnexpectedEof;
                }

                while (ch == ' ' || ch == '\t')
                {
                    if (ReadCh(port, &ch) == 0)
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

    return(MakeString(s, sl));

UnexpectedEof:
    RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading string", List(port));
    return(NoValueObject);
}

static int DoReadToken(FObject port, FCh ch, FCh * s, int msl)
{
    int sl;

    sl = 0;
    for (;;)
    {
        s[sl] = ch;
        sl += 1;
        if (sl == msl)
            RaiseExceptionC(R.Restriction, "read", "symbol or number too long", List(port));

        if (PeekCh(port, &ch) == 0)
            break;

        if (SymbolCharP(ch) == 0)
            break;

        ReadCh(port, &ch);
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
static FObject DoRead(FObject port, int eaf, int rlf, int rif, int fcf);
static FObject DoReadSharp(FObject port, int eaf, int rlf, int rif, int fcf)
{
    FCh ch;

    if (ReadCh(port, &ch) == 0)
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

        if (ReadCh(port, &ch) == 0)
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

        if (ReadCh(port, &ch) == 0)
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
        if (ReadCh(port, &ch) == 0)
            RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading bytevector",
                    List(port));
        if (ch != '8')
            RaiseExceptionC(R.Lexical, "read", "expected #\u8(", List(port));

        if (ReadCh(port, &ch) == 0)
            RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading bytevector",
                    List(port));
        if (ch != '(')
            RaiseExceptionC(R.Lexical, "read", "expected #\u8(", List(port));
        return(U8ListToBytevector(DoReadList(port, rif, fcf)));
    }
    else if (ch == ';')
    {
        DoRead(port, 0, 0, 0, 0);

        return(DoRead(port, eaf, rlf, rif, fcf));
    }
    else if (ch == '|')
    {
        int lvl = 1;

        FCh pch = 0;
        while (lvl > 0)
        {
            if (ReadCh(port, &ch) == 0)
                RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file in block comment",
                        List(port));

            if (pch == '#' && ch == '|')
                lvl += 1;
            else if (pch == '|' && ch == '#')
                lvl -= 1;

            pch = ch;
        }

        return(DoRead(port, eaf, rlf, rif, fcf));
    }

    RaiseExceptionC(R.Lexical, "read", "unexpected character following #",
            List(port, MakeCharacter(ch)));

    return(NoValueObject);
}

static FObject DoRead(FObject port, int eaf, int rlf, int rif, int fcf)
{
    FCh ch;

    for (;;)
    {
        if (ReadCh(port, &ch) == 0)
            break;

        if (ch == ';')
        {
            do
            {
                if (ReadCh(port, &ch) == 0)
                    goto Eof;
            }
            while (ch != '\n' && ch != '\r');
        }
        else
        {
            switch (ch)
            {
            case '#':
                return(DoReadSharp(port, eaf, rlf, rif, fcf));

            case '"':
                return(DoReadString(port, '"'));

            case '|':
            {
                int ln;

                if (rif)
                    ln = GetLocation(port);

                FObject sym = fcf ? StringToSymbol(FoldcaseString(DoReadString(port, '|')))
                        : StringToSymbol(DoReadString(port, '|'));
                return(rif ? MakeIdentifier(sym, ln) : sym);
            }

            case '(':
                return(DoReadList(port, rif, fcf));

            case ')':
                if (rlf)
                    return(EolObject);
                RaiseExceptionC(R.Lexical, "read", "unexpected )", List(port));
                break;

            case '.':
                if (PeekCh(port, &ch) == 0)
                    RaiseExceptionC(R.Lexical, "read",
                            "unexpected end-of-file reading dotted pair", List(port));
                if (ch == '.')
                {
                    ReadCh(port, &ch);

                    if (ReadCh(port, &ch) == 0)
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
                if (PeekCh(port, &ch) == 0)
                    RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading unquote",
                            List(port));

                FObject sym;
                if (ch == '@')
                {
                    ReadCh(port, &ch);
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
//    FAssert(OldInputPortP(port) && AsPort(port)->Context != 0);

    return(DoRead(port, 1, 0, rif, fcf));
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

                WriteCh(port, '#');
                FCh s[8];
                int sl = NumberAsString(ToWriteSharedCtx(ctx)->Label, s, 10);
                WriteString(port, s, sl);
                WriteCh(port, '=');
            }

            if (PairP(obj))
            {
                WriteCh(port, '(');
                for (;;)
                {
                    wfn(port, First(obj), df, wfn, ctx);
                    if (PairP(Rest(obj)) && EqHashtableRef(ToWriteSharedCtx(ctx)->Hashtable,
                            Rest(obj), FalseObject) == FalseObject)
                    {
                        WriteCh(port, ' ');
                        obj = Rest(obj);
                    }
                    else if (Rest(obj) == EmptyListObject)
                    {
                        WriteCh(port, ')');
                        break;
                    }
                    else
                    {
                        WriteStringC(port, " . ");
                        wfn(port, Rest(obj), df, wfn, ctx);
                        WriteCh(port, ')');
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

            WriteCh(port, '#');
            FCh s[8];
            int sl = NumberAsString(AsFixnum(val), s, 10);
            WriteString(port, s, sl);
            WriteCh(port, '#');
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

    WriteCh(port, '(');
    for (;;)
    {
        wfn(port, First(obj), df, wfn, ctx);
        if (PairP(Rest(obj)))
        {
            WriteCh(port, ' ');
            obj = Rest(obj);
        }
        else if (Rest(obj) == EmptyListObject)
        {
            WriteCh(port, ')');
            break;
        }
        else
        {
            WriteStringC(port, " . ");
            wfn(port, Rest(obj), df, wfn, ctx);
            WriteCh(port, ')');
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

        WriteStringC(port, "#<(box: #x");
        WriteString(port, s, sl);
        WriteCh(port, ' ');
        wfn(port, Unbox(obj), df, wfn, ctx);
        WriteStringC(port, ")>");
        break;
    }

    case StringTag:
        if (df)
            WriteString(port, AsString(obj)->String, StringLength(obj));
        else
        {
            WriteCh(port, '"');

            for (unsigned int idx = 0; idx < StringLength(obj); idx++)
            {
                FCh ch = AsString(obj)->String[idx];
                if (ch == '\\' || ch == '"')
                    WriteCh(port, '\\');
                WriteCh(port, ch);
            }

            WriteCh(port, '"');
        }
        break;

    case VectorTag:
    {
        WriteStringC(port, "#(");
        for (unsigned int idx = 0; idx < VectorLength(obj); idx++)
        {
            if (idx > 0)
                WriteCh(port, ' ');
            wfn(port, AsVector(obj)->Vector[idx], df, wfn, ctx);
        }

        WriteCh(port, ')');
        break;
    }

    case BytevectorTag:
    {
        FCh s[8];
        int sl;

        WriteStringC(port, "#u8(");
        for (unsigned int idx = 0; idx < BytevectorLength(obj); idx++)
        {
            if (idx > 0)
                WriteCh(port, ' ');

            sl = NumberAsString((FFixnum) AsBytevector(obj)->Vector[idx], s, 10);
            WriteString(port, s, sl);
        }

        WriteCh(port, ')');
        break;
    }

    case BinaryPortTag:
    case TextualPortTag:
    {
        FCh s[16];
        int sl = NumberAsString((FFixnum) obj, s, 16);

        WriteStringC(port, "#<");
        if (TextualPortP(obj))
            WriteStringC(port, "textual-");
        else
        {
            FAssert(BinaryPortP(obj));

            WriteStringC(port, "binary-");
        }

        if (InputPortP(obj))
            WriteStringC(port, "input-");
        if (OutputPortP(obj))
            WriteStringC(port, "output-");
        WriteStringC(port, "port: #x");
        WriteString(port, s, sl);

        if (InputPortOpenP(obj) == 0 && OutputPortOpenP(obj) == 0)
            WriteStringC(port, " closed");

        if (StringP(AsGenericPort(obj)->Name))
        {
            WriteCh(port, ' ');
            WriteString(port, AsString(AsGenericPort(obj)->Name)->String,
                    StringLength(AsGenericPort(obj)->Name));
        }

        WriteCh(port, '>');
        break;
    }

    case ProcedureTag:
    {
        FCh s[16];
        int sl = NumberAsString((FFixnum) obj, s, 16);

        WriteStringC(port, "#<procedure: ");
        WriteString(port, s, sl);

        if (AsProcedure(obj)->Name != NoValueObject)
        {
            WriteCh(port, ' ');
            wfn(port, AsProcedure(obj)->Name, df, wfn, ctx);
        }

        if (AsProcedure(obj)->Reserved & PROCEDURE_FLAG_CLOSURE)
            WriteStringC(port, " closure");

        if (AsProcedure(obj)->Reserved & PROCEDURE_FLAG_PARAMETER)
            WriteStringC(port, " parameter");

        if (AsProcedure(obj)->Reserved & PROCEDURE_FLAG_CONTINUATION)
            WriteStringC(port, " continuation");

//        WriteCh(port, ' ');
//        wfn(port, AsProcedure(obj)->Code, df, wfn, ctx);
        WriteCh(port, '>');
        break;
    }

    case SymbolTag:
        WriteString(port, AsString(AsSymbol(obj)->String)->String,
                StringLength(AsSymbol(obj)->String));
        break;

    case RecordTypeTag:
    {
        FCh s[16];
        int sl = NumberAsString((FFixnum) obj, s, 16);

        WriteStringC(port, "#<record-type: #x");
        WriteString(port, s, sl);
        WriteCh(port, ' ');
        wfn(port, RecordTypeName(obj), df, wfn, ctx);

        for (unsigned int fdx = 1; fdx < RecordTypeNumFields(obj); fdx += 1)
        {
            WriteCh(port, ' ');
            wfn(port, AsRecordType(obj)->Fields[fdx], df, wfn, ctx);
        }

        WriteStringC(port, ">");
        break;
    }

    case RecordTag:
    {
        FObject rt = AsGenericRecord(obj)->Fields[0];
        FCh s[16];
        int sl = NumberAsString((FFixnum) obj, s, 16);

        WriteStringC(port, "#<(");
        wfn(port, RecordTypeName(rt), df, wfn, ctx);
        WriteStringC(port, ": #x");
        WriteString(port, s, sl);

        for (unsigned int fdx = 1; fdx < RecordNumFields(obj); fdx++)
        {
            WriteCh(port, ' ');
            wfn(port, AsRecordType(rt)->Fields[fdx], df, wfn, ctx);
            WriteStringC(port, ": ");
            wfn(port, AsGenericRecord(obj)->Fields[fdx], df, wfn, ctx);
        }

        WriteStringC(port, ")>");
        break;
    }

    case PrimitiveTag:
    {
        WriteStringC(port, "#<primitive: ");
        WriteStringC(port, AsPrimitive(obj)->Name);
        WriteCh(port, ' ');

        char * fn = AsPrimitive(obj)->Filename;
        char * p = fn;
        while (*p != 0)
        {
            if (*p == '/' || *p == '\\')
                fn = p + 1;

            p += 1;
        }

        WriteStringC(port, fn);
        WriteCh(port, '@');
        FCh s[16];
        int sl = NumberAsString(AsPrimitive(obj)->LineNumber, s, 10);
        WriteString(port, s, sl);
        WriteCh(port, '>');
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

        WriteStringC(port, "#<unknown: ");
        WriteString(port, s, sl);
        WriteCh(port, '>');
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
        WriteString(port, s, sl);
    }
    else if (CharacterP(obj))
    {
        if (AsCharacter(obj) < 128)
        {
            if (df == 0)
                WriteStringC(port, "#\\");
            WriteCh(port, AsCharacter(obj));
        }
        else
        {
            FCh s[16];
            int sl = NumberAsString(AsCharacter(obj), s, 16);
            WriteStringC(port, "#\\x");
            WriteString(port, s, sl);
        }
    }
    else if (SpecialSyntaxP(obj))
        WriteSpecialSyntax(port, obj, df);
    else if (InstructionP(obj))
        WriteInstruction(port, obj, df);
    else if (ValuesCountP(obj))
    {
        WriteStringC(port, "#<values-count: ");

        FCh s[16];
        int sl = NumberAsString(AsValuesCount(obj), s, 10);
        WriteString(port, s, sl);

        WriteCh(port, '>');
    }
    else if (PairP(obj))
        WritePair(port, obj, df, wfn, ctx);
    else if (IndirectP(obj))
        WriteIndirectObject(port, obj, df, wfn, ctx);
    else if (obj == EmptyListObject)
        WriteStringC(port, "()");
    else if (obj == FalseObject)
        WriteStringC(port, "#f");
    else if (obj == TrueObject)
        WriteStringC(port, "#t");
    else if (obj == EndOfFileObject)
        WriteStringC(port, "#<end-of-file>");
    else if (obj == NoValueObject)
        WriteStringC(port, "#<no-value>");
    else if (obj == WantValuesObject)
        WriteStringC(port, "#<want-values>");
    else
    {
        FCh s[16];
        int sl = NumberAsString((FFixnum) obj, s, 16);

        WriteStringC(port, "#<unknown: ");
        WriteString(port, s, sl);
        WriteCh(port, '>');
    }
}

void WriteSimple(FObject port, FObject obj, int df)
{
//    FAssert(OutputPortP(port) && AsPort(port)->Context != 0);

    WriteGeneric(port, obj, df, (FWriteFn) WriteGeneric, 0);
}

// ---- System interface ----

Define("file-exists?", FileExistsPPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("file-exists?", argc);
    StringArgCheck("file-exists?", argv[0]);

    SCh * ss;
    ConvertToSystem(argv[0], ss);

    return(_waccess(ss, 0) == 0 ? TrueObject : FalseObject);
}

Define("delete-file", DeleteFilePrimitive)(int argc, FObject argv[])
{
    OneArgCheck("delete-file", argc);
    StringArgCheck("delete-file", argv[0]);

    SCh * ss;
    ConvertToSystem(argv[0], ss);

    if (_wremove(ss) != 0)
        RaiseExceptionC(R.Assertion, "delete-file", "unable to delete file", List(argv[0]));

    return(NoValueObject);
}

// ---- Input and output ----

Define("open-output-string", OpenOutputStringPrimitive)(int argc, FObject argv[])
{
    if (argc != 0)
        RaiseExceptionC(R.Assertion, "open-output-string", "expected no arguments",
                EmptyListObject);

    return(MakeStringOutputPort());
}

Define("get-output-string", GetOutputStringPrimitive)(int argc, FObject argv[])
{
#if 0
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "get-output-string", "expected one argument",
                EmptyListObject);

    if (OldOutputPortP(argv[0]) == 0 || AsPort(argv[0])->Output != &StringOutputPort)
        RaiseExceptionC(R.Assertion, "get-output-string", "expected a string output port",
                List(argv[0]));
#endif // 0
    return(GetOutputString(argv[0]));
}

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

    WriteCh(port, '\n');

    return(NoValueObject);
}

static FPrimitive * Primitives[] =
{
    &FileExistsPPrimitive,
    &DeleteFilePrimitive,
    
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
    R.StandardInput = MakeLatin1Port(MakeStdioPort(MakeStringC("standard-input"), stdin, 0));
    R.StandardOutput = MakeLatin1Port(MakeStdioPort(MakeStringC("standard-output"), 0, stdout));

    R.QuoteSymbol = StringCToSymbol("quote");
    R.QuasiquoteSymbol = StringCToSymbol("quasiquote");
    R.UnquoteSymbol = StringCToSymbol("unquote");
    R.UnquoteSplicingSymbol = StringCToSymbol("unquote-splicing");

    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);

    SetupPrettyPrint();
}
