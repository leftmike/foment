/*

Foment

*/

#ifdef FOMENT_WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif // FOMENT_WIN32

#include <windows.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <io.h>
#include "foment.hpp"
#include "syncthrd.hpp"
#include "io.hpp"
#include "unicode.hpp"

#define NOT_PEEKED ((unsigned int) -1)
#define CR 13
#define LF 10

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
    port->Offset = 0;

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
        return(1);
    }

    if (AsBinaryPort(port)->ReadBytesFn(port, b, 1) == 0)
        return(0);

    AsBinaryPort(port)->PeekedByte = *b;
    return(1);
}

int ByteReadyP(FObject port)
{
    FAssert(BinaryPortP(port) && InputPortOpenP(port));

    if (AsBinaryPort(port)->PeekedByte != NOT_PEEKED)
        return(1);

    return(AsBinaryPort(port)->ByteReadyPFn(port));
}

unsigned int GetOffset(FObject port)
{
    FAssert(BinaryPortP(port) && InputPortP(port));

    return(AsBinaryPort(port)->Offset);
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
    FAssert(BinaryPortP(port) && InputPortOpenP(port));

    return(1);
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

static void BvinCloseInput(FObject port)
{
    FAssert(BinaryPortP(port));

    AsGenericPort(port)->Object = NoValueObject;
}

static unsigned int BvinReadBytes(FObject port, void * b, unsigned int bl)
{
    FAssert(BinaryPortP(port) && InputPortOpenP(port));

    FObject bv = AsGenericPort(port)->Object;
    unsigned int bdx = (unsigned int) AsGenericPort(port)->InputContext;

    FAssert(BytevectorP(bv));
    FAssert(bdx <= BytevectorLength(bv));

    if (bdx == BytevectorLength(bv))
        return(0);

    if (bdx + bl > BytevectorLength(bv))
        bl = BytevectorLength(bv) - bdx;

    memcpy(b, AsBytevector(bv)->Vector + bdx, bl);
    AsGenericPort(port)->InputContext = (void *) (bdx + bl);

    return(bl);
}

static int BvinByteReadyP(FObject port)
{
    FAssert(BinaryPortP(port) && InputPortOpenP(port));

    return(1);
}

static FObject MakeBytevectorInputPort(FObject bv)
{
    FAssert(BytevectorP(bv));

    return(MakeBinaryPort(NoValueObject, bv, 0, 0, BvinCloseInput, 0, 0, BvinReadBytes,
            BvinByteReadyP, 0));
}

static void BvoutCloseOutput(FObject port)
{
    // Nothing.
}

static void BvoutFlushOutput(FObject port)
{
    // Nothing.
}

static void BvoutWriteBytes(FObject port, void * b, unsigned int bl)
{
    FAssert(BinaryPortP(port) && OutputPortOpenP(port));
    FAssert(AsGenericPort(port)->Object == EmptyListObject || PairP(AsGenericPort(port)->Object));

    FObject lst = AsGenericPort(port)->Object;

    for (unsigned int bdx = 0; bdx < bl; bdx++)
        lst = MakePair(MakeFixnum(((unsigned char *) b)[bdx]), lst);

    AsGenericPort(port)->Object = lst;
}

static FObject GetOutputBytevector(FObject port)
{
    FAssert(BytevectorOutputPortP(port));
    FAssert(AsGenericPort(port)->Object == EmptyListObject || PairP(AsGenericPort(port)->Object));

    FObject lst = AsGenericPort(port)->Object;
    int bl = ListLength(lst);
    FObject bv = MakeBytevector(bl);
    int bdx = bl;

    while (PairP(lst))
    {
        bdx -= 1;

        FAssert(bdx >= 0);
        FAssert(FixnumP(First(lst)) && AsFixnum(First(lst)) >= 0 && AsFixnum(First(lst)) <= 0xFF);

        AsBytevector(bv)->Vector[bdx] = AsFixnum(First(lst));
        lst = Rest(lst);
    }

    FAssert(lst == EmptyListObject);

    return(bv);
}

static FObject MakeBytevectorOutputPort()
{
    FObject port = MakeBinaryPort(NoValueObject, EmptyListObject, 0, 0, 0, BvoutCloseOutput,
            BvoutFlushOutput, 0, 0, BvoutWriteBytes);
    AsGenericPort(port)->Flags |= PORT_FLAG_BYTEVECTOR_OUTPUT;
    return(port);
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
    port->Line = 1;
    port->Column = 0;

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

static FObject ListToString(FObject lst)
{
    unsigned int sl = ListLength(lst);
    FObject s = MakeString(0, sl);
    unsigned int sdx = sl;

    while (sdx > 0)
    {
        sdx -= 1;
        AsString(s)->String[sdx] = AsCharacter(First(lst));
        lst = Rest(lst);
    }

    return(s);
}

FObject ReadLine(FObject port)
{
    FAssert(TextualPortP(port) && InputPortOpenP(port));

    FObject lst = EmptyListObject;
    FCh ch = 0;

    while (ReadCh(port, &ch))
    {
        if (ch == 0x0A || ch == 0x0D)
            break;

        lst = MakePair(MakeCharacter(ch), lst);
    }

    if (ch == 0x0D) // carriage return
    {
        while(PeekCh(port, &ch))
        {
            if (ch == 0x0A) // linefeed
            {
                ReadCh(port, &ch);
                break;
            }

            if (ch != 0x0D) // carriage return
                break;

            ReadCh(port, &ch);
        }
    }

    if (ch == 0 && lst == EmptyListObject)
        return(EndOfFileObject);

    return(ListToString(lst));
}

FObject ReadString(FObject port, int cnt)
{
    FAssert(TextualPortP(port) && InputPortOpenP(port));

    FObject lst = EmptyListObject;
    FCh ch;

    while (cnt > 0 && ReadCh(port, &ch))
    {
        cnt -= 1;
        lst = MakePair(MakeCharacter(ch), lst);
    }

    if (lst == EmptyListObject)
        return(EndOfFileObject);

    return(ListToString(lst));
}

unsigned int GetLineColumn(FObject port, unsigned int * col)
{
    FAssert(TextualPortP(port) && InputPortP(port));

    if (col)
        *col = AsTextualPort(port)->Column;
    return(AsTextualPort(port)->Line);
}

void FoldcasePort(FObject port, int fcf)
{
    FAssert(TextualPortP(port) && InputPortP(port));

    if (fcf)
        AsGenericPort(port)->Flags |= PORT_FLAG_FOLDCASE;
    else
        AsGenericPort(port)->Flags &= ~PORT_FLAG_FOLDCASE;
}

void WantIdentifiersPort(FObject port, int wif)
{
    FAssert(TextualPortP(port) && InputPortP(port));

    if (wif)
        AsGenericPort(port)->Flags |= PORT_FLAG_WANT_IDENTIFIERS;
    else
        AsGenericPort(port)->Flags &= ~PORT_FLAG_WANT_IDENTIFIERS;
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
    if (b == CR)
    {
        if (PeekByte(AsGenericPort(port)->Object, &b) != 0 && b != LF)
        {
            AsTextualPort(port)->Line += 1;
            AsTextualPort(port)->Column = 0;
        }
    }
    else if (b == LF)
    {
        AsTextualPort(port)->Line += 1;
        AsTextualPort(port)->Column = 0;
    }

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

    FILE * fp = _wfopen(ss, L"rb");
    if (fp == 0)
        return(NoValueObject);

    return(MakeLatin1Port(MakeStdioPort(fn, fp, 0)));
}

FObject OpenOutputFile(FObject fn)
{
    SCh * ss;
    ConvertToSystem(fn, ss);

    FILE * fp = _wfopen(ss, L"wb");
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
    FAssert(TextualPortP(port) && InputPortOpenP(port));

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
    // Nothing.
}

static void SoutFlushOutput(FObject port)
{
    // Nothing.
}

static void SoutWriteString(FObject port, FCh * s, unsigned int sl)
{
    FAssert(TextualPortP(port) && OutputPortOpenP(port));
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

Define("input-port?", InputPortPPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("input-port?", argc);

    return(InputPortP(argv[0]) ? TrueObject : FalseObject);
}

Define("output-port?", OutputPortPPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("output-port?", argc);

    return(OutputPortP(argv[0]) ? TrueObject : FalseObject);
}

Define("textual-port?", TextualPortPPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("textual-port?", argc);

    return(TextualPortP(argv[0]) ? TrueObject : FalseObject);
}

Define("binary-port?", BinaryPortPPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("binary-port?", argc);

    return(BinaryPortP(argv[0]) ? TrueObject : FalseObject);
}

Define("port?", PortPPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("port?", argc);

    return((TextualPortP(argv[0]) || BinaryPortP(argv[0])) ? TrueObject : FalseObject);
}

Define("input-port-open?", InputPortOpenPPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("input-port-open?", argc);
    PortArgCheck("input-port-open?", argv[0]);

    return(InputPortOpenP(argv[0]) ? TrueObject : FalseObject);
}

Define("output-port-open?", OutputPortOpenPPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("output-port-open?", argc);
    PortArgCheck("output-port-open?", argv[0]);

    return(OutputPortOpenP(argv[0]) ? TrueObject : FalseObject);
}

Define("open-binary-input-file", OpenBinaryInputFilePrimitive)(int argc, FObject argv[])
{
    OneArgCheck("open-binary-input-file", argc);
    StringArgCheck("open-binary-input-file", argv[0]);

    SCh * ss;
    ConvertToSystem(argv[0], ss);

    FILE * fp = _wfopen(ss, L"r");
    if (fp == 0)
        RaiseExceptionC(R.Assertion, "open-binary-input-file",
                "unable to open file for input", List(argv[0]));

    return(MakeStdioPort(argv[0], fp, 0));
}

Define("open-binary-output-file", OpenBinaryOutputFilePrimitive)(int argc, FObject argv[])
{
    OneArgCheck("open-binary-output-file", argc);
    StringArgCheck("open-binary-output-file", argv[0]);

    SCh * ss;
    ConvertToSystem(argv[0], ss);

    FILE * fp = _wfopen(ss, L"w");
    if (fp == 0)
        RaiseExceptionC(R.Assertion, "open-binary-output-file",
                "unable to open file for input", List(argv[0]));

    return(MakeStdioPort(argv[0], 0, fp));
}

Define("close-port", ClosePortPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("close-port", argc);
    PortArgCheck("close-port", argv[0]);

    CloseInput(argv[0]);
    CloseOutput(argv[0]);

    return(NoValueObject);
}

Define("close-input-port", CloseInputPortPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("close-input-port", argc);
    InputPortArgCheck("close-input-port", argv[0]);

    CloseInput(argv[0]);

    return(NoValueObject);
}

Define("close-output-port", CloseOutputPortPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("close-output-port", argc);
    OutputPortArgCheck("close-output-port", argv[0]);

    CloseOutput(argv[0]);

    return(NoValueObject);
}

Define("open-input-string", OpenInputStringPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("open-input-string", argc);
    StringArgCheck("open-input-string", argv[0]);

    return(MakeStringInputPort(argv[0]));
}

Define("open-output-string", OpenOutputStringPrimitive)(int argc, FObject argv[])
{
    ZeroArgsCheck("open-output-string", argc);

    return(MakeStringOutputPort());
}

Define("get-output-string", GetOutputStringPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("get-output-string", argc);
    StringOutputPortArgCheck("get-output-string", argv[0]);

    return(GetOutputString(argv[0]));
}

Define("open-input-bytevector", OpenInputBytevectorPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("open-input-bytevector", argc);
    BytevectorArgCheck("open-input-bytevector", argv[0]);

    return(MakeBytevectorInputPort(argv[0]));
}

Define("open-output-bytevector", OpenOutputBytevectorPrimitive)(int argc, FObject argv[])
{
    ZeroArgsCheck("open-output-bytevector", argc);

    return(MakeBytevectorOutputPort());
}

Define("get-output-bytevector", GetOutputBytevectorPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("get-output-bytevector", argc);
    BytevectorOutputPortArgCheck("get-output-bytevector", argv[0]);

    return(GetOutputBytevector(argv[0]));
}

Define("make-latin-1-port", MakeLatin1PortPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("make-latin-1-port", argc);
    BinaryPortArgCheck("make-latin-1-port", argv[0]);

    return(MakeLatin1Port(argv[0]));
}

static FPrimitive * Primitives[] =
{
    &FileExistsPPrimitive,
    &DeleteFilePrimitive,
    &InputPortPPrimitive,
    &OutputPortPPrimitive,
    &TextualPortPPrimitive,
    &BinaryPortPPrimitive,
    &PortPPrimitive,
    &InputPortOpenPPrimitive,
    &OutputPortOpenPPrimitive,
    &OpenBinaryInputFilePrimitive,
    &OpenBinaryOutputFilePrimitive,
    &ClosePortPrimitive,
    &CloseInputPortPrimitive,
    &CloseOutputPortPrimitive,
    &OpenInputStringPrimitive,
    &OpenOutputStringPrimitive,
    &GetOutputStringPrimitive,
    &OpenInputBytevectorPrimitive,
    &OpenOutputBytevectorPrimitive,
    &GetOutputBytevectorPrimitive,
    &MakeLatin1PortPrimitive
};

void SetupIO()
{
    R.StandardInput = MakeLatin1Port(MakeStdioPort(MakeStringC("standard-input"), stdin, 0));
    R.StandardOutput = MakeLatin1Port(MakeStdioPort(MakeStringC("standard-output"), 0, stdout));
    R.StandardError = MakeLatin1Port(MakeStdioPort(MakeStringC("standard-error"), 0, stderr));

    R.QuoteSymbol = StringCToSymbol("quote");
    R.QuasiquoteSymbol = StringCToSymbol("quasiquote");
    R.UnquoteSymbol = StringCToSymbol("unquote");
    R.UnquoteSplicingSymbol = StringCToSymbol("unquote-splicing");

    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);

    SetupWrite();
    SetupRead();
}
