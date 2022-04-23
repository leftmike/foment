/*

Foment

*/

#ifdef FOMENT_WINDOWS
#include <windows.h>
#endif // FOMENT_WINDOWS
#ifdef FOMENT_UNIX
#include <pthread.h>
#endif // FOMENT_UNIX
#include "foment.hpp"
#include "syncthrd.hpp"
#include "io.hpp"
#include "compile.hpp"

// Write

static inline long_t HasSlotsP(FObject obj)
{
    if (ObjectP(obj) == 0)
        return(0);

    FObjectTag tag = ObjectTag(obj);
    if (tag >= BadDogTag || ObjectTypes[tag].SlotCount == 0)
        return(0);
    return(1);
}

FWriteContext::FWriteContext(FObject port, long_t df)
{
    this->Port = port;
    DisplayFlag = df;
    WriteType = NoWrite;
    HashTable = NoValueObject;
    SharedCount = 0;
    PreviousLabel = 1;
}

void FWriteContext::Prepare(FObject obj, FWriteType wt)
{
    FAssert(WriteType == NoWrite);
    FAssert(wt != NoWrite);

    if (wt != SimpleWrite && HasSlotsP(obj))
    {
        HashTable = MakeEqHashTable(128, 0);
        FindSharedObjects(obj, wt);
        if (SharedCount == 0)
            WriteType = SimpleWrite;
        else
            WriteType = wt;
    }
    else
        WriteType = SimpleWrite;

    FAssert(WriteType != NoWrite);
}

void FWriteContext::Write(FObject obj)
{
    FAssert(WriteType != NoWrite);

    if (WriteType != SimpleWrite && HasSlotsP(obj))
    {
        FObject val = HashTableRef(HashTable, obj, MakeFixnum(1));

        FAssert(FixnumP(val));

        if (AsFixnum(val) > 0)
        {
            if (AsFixnum(val) > 1)
            {
                PreviousLabel -= 1;
                HashTableSet(HashTable, obj, MakeFixnum(PreviousLabel));

                WriteCh('#');
                FCh s[8];
                long_t sl = FixnumAsString(- PreviousLabel, s, 10);
                WriteString(s, sl);
                WriteCh('=');
            }

            if (PairP(obj))
            {
                WriteCh('(');
                for (;;)
                {
                    Write(First(obj));
                    if (PairP(Rest(obj))
                            && HashTableRef(HashTable, Rest(obj), MakeFixnum(1)) == MakeFixnum(1))
                    {
                        WriteCh(' ');
                        obj = Rest(obj);
                    }
                    else if (Rest(obj) == EmptyListObject)
                    {
                        WriteCh(')');
                        break;
                    }
                    else
                    {
                        WriteStringC(" . ");
                        Write(Rest(obj));
                        WriteCh(')');
                        break;
                    }
                }
            }
            else
                WriteSimple(obj);
        }
        else
        {
            FAssert(FixnumP(val) && AsFixnum(val) < 1);

            WriteCh('#');
            FCh s[8];
            long_t sl = FixnumAsString(- AsFixnum(val), s, 10);
            WriteString(s, sl);
            WriteCh('#');
        }
    }
    else
        WriteSimple(obj);
}

void FWriteContext::Display(FObject obj)
{
    long_t df = DisplayFlag;

    DisplayFlag = 1;
    Write(obj);
    DisplayFlag = df;
}

void FWriteContext::WriteCh(FCh ch)
{
    FAssert(WriteType != NoWrite);

    ::WriteCh(Port, ch);
}

void FWriteContext::WriteString(FCh * s, ulong_t sl)
{
    FAssert(WriteType != NoWrite);

    ::WriteString(Port, s, sl);
}

void FWriteContext::WriteStringC(const char * s)
{
    FAssert(WriteType != NoWrite);

    ::WriteStringC(Port, s);
}

void FWriteContext::FindSharedObjects(FObject obj, FWriteType wt)
{
    FAssert(wt == CircularWrite || wt == SharedWrite);

Again:

    if (HasSlotsP(obj))
    {
        FObject val = HashTableRef(HashTable, obj, MakeFixnum(0));

        FAssert(FixnumP(val));

        HashTableSet(HashTable, obj, MakeFixnum(AsFixnum(val) + 1));

        if (AsFixnum(val) == 0)
        {
            if (PairP(obj))
            {
                FindSharedObjects(First(obj), wt);
                if (wt == CircularWrite)
                    FindSharedObjects(Rest(obj), wt);
                else
                {
                    obj = Rest(obj);
                    goto Again;
                }
            }
            else
            {
                for (ulong_t idx = 0; idx < XXXSlotCount(obj); idx++)
                    FindSharedObjects(((FObject *) obj)[idx], wt);
            }

            if (wt == CircularWrite)
            {
                val = HashTableRef(HashTable, obj, MakeFixnum(0));

                FAssert(FixnumP(val));
                FAssert(AsFixnum(val) > 0);

                if (AsFixnum(val) == 1)
                    HashTableDelete(HashTable, obj);
            }
        }
        else
            SharedCount += 1;
    }
}

static void WriteUnknown(FWriteContext * wctx, FObject obj)
{
    FCh s[16];
    long_t sl = FixnumAsString((long_t) obj, s, 16);

    wctx->WriteStringC("#<unknown: ");
    wctx->WriteString(s, sl);
    wctx->WriteCh('>');
}

void FWriteContext::WriteSimple(FObject obj)
{
    if (ObjectP(obj))
    {
        FObjectTag tag = ObjectTag(obj);

        if (tag < BadDogTag && ObjectTypes[tag].Write != 0)
            ObjectTypes[tag].Write(this, obj);
        else
            WriteUnknown(this, obj);
    }
    else if (FixnumP(obj))
    {
        FCh s[64];
        long_t sl = FixnumAsString(AsFixnum(obj), s, 10);

        WriteString(s, sl);
    }
    else if (DirectP(obj, ImmediateDirectTag))
    {
        switch (ImmediateTag(obj))
        {
        case CharacterTag:
            if (DisplayFlag)
                WriteCh(AsCharacter(obj));
            else if (AsCharacter(obj) == 0x07)
                WriteStringC("#\\alarm");
            else if (AsCharacter(obj) == 0x08)
                WriteStringC("#\\backspace");
            else if (AsCharacter(obj) == 0x7F)
                WriteStringC("#\\delete");
            else if (AsCharacter(obj) == 0x1B)
                WriteStringC("#\\escape");
            else if (AsCharacter(obj) == 0x0A)
                WriteStringC("#\\newline");
            else if (AsCharacter(obj) == 0x00)
                WriteStringC("#\\null");
            else if (AsCharacter(obj) == 0x0D)
                WriteStringC("#\\return");
            else if (AsCharacter(obj) == 0x20)
                WriteStringC("#\\space");
            else if (AsCharacter(obj) == 0x09)
                WriteStringC("#\\tab");
            else if (AsCharacter(obj) < 128)
            {
                WriteStringC("#\\");
                WriteCh(AsCharacter(obj));
            }
            else
            {
                FCh s[16];
                long_t sl = FixnumAsString(AsCharacter(obj), s, 16);
                WriteStringC("#\\x");
                WriteString(s, sl);
            }
            break;

        case MiscellaneousTag:
            if (obj == EmptyListObject)
                WriteStringC("()");
            else if (obj == EndOfFileObject)
                WriteStringC("#<end-of-file>");
            else if (obj == NoValueObject)
                WriteStringC("#<no-value>");
            else if (obj == WantValuesObject)
                WriteStringC("#<want-values>");
            else if (obj == NotFoundObject)
                WriteStringC("#<not-found>");
            else if (obj == MatchAnyObject)
                WriteStringC("#<match-any>");
            else
                WriteUnknown(this, obj);
            break;

        case SpecialSyntaxTag:
            WriteSpecialSyntax(this, obj);
            break;

        case InstructionTag:
            WriteInstruction(this, obj);
            break;

        case ValuesCountTag:
        {
            WriteStringC("#<values-count: ");

            FCh s[16];
            long_t sl = FixnumAsString(AsValuesCount(obj), s, 10);
            WriteString(s, sl);

            WriteCh('>');
            break;
        }

        case BooleanTag:
            if (obj == FalseObject)
                WriteStringC("#f");
            else if (obj == TrueObject)
                WriteStringC("#t");
            else
                WriteUnknown(this, obj);
            break;

        default:
            FAssert(0);
        }
    }
    else
    {
        FAssert(0);
    }
}

void WritePortObject(FWriteContext * wctx, FObject obj)
{
    FCh s[16];
    long_t sl = FixnumAsString((long_t) obj, s, 16);

    wctx->WriteStringC("#<");
    if (TextualPortP(obj))
        wctx->WriteStringC("textual-");
    else
    {
        FAssert(BinaryPortP(obj));

        wctx->WriteStringC("binary-");
    }

    if (InputPortP(obj))
        wctx->WriteStringC("input-");
    if (OutputPortP(obj))
        wctx->WriteStringC("output-");
    wctx->WriteStringC("port: #x");
    wctx->WriteString(s, sl);

    if (InputPortOpenP(obj) == 0 && OutputPortOpenP(obj) == 0)
        wctx->WriteStringC(" closed");

    if (StringP(AsGenericPort(obj)->Name))
    {
        wctx->WriteCh(' ');
        wctx->WriteString(AsString(AsGenericPort(obj)->Name)->String,
                StringLength(AsGenericPort(obj)->Name));
    }

    if (InputPortP(obj))
    {
        if (BinaryPortP(obj))
        {
            wctx->WriteStringC(" offset: ");
            sl = FixnumAsString(GetOffset(obj), s, 10);
            wctx->WriteString(s, sl);
        }
        else
        {
            FAssert(TextualPortP(obj));

            wctx->WriteStringC(" line: ");
            sl = FixnumAsString(GetLineColumn(obj, 0), s, 10);
            wctx->WriteString(s, sl);
        }
    }

    wctx->WriteCh('>');
}

void Write(FObject port, FObject obj, long_t df)
{
    FWriteContext wctx(port, df);

    wctx.Prepare(obj, CircularWrite);
    wctx.Write(obj);
}

void WriteShared(FObject port, FObject obj, long_t df)
{
    FWriteContext wctx(port, df);

    wctx.Prepare(obj, SharedWrite);
    wctx.Write(obj);
}

void WriteSimple(FObject port, FObject obj, long_t df)
{
    FWriteContext wctx(port, df);

    wctx.Prepare(obj, SimpleWrite);
    wctx.Write(obj);
}

// ---- Primitives ----

Define("write", WritePrimitive)(long_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("write", argc);
    FObject port = (argc == 2 ? argv[1] : CurrentOutputPort());
    TextualOutputPortArgCheck("write", port);

    Write(port, argv[0], 0);
    return(NoValueObject);
}

Define("write-shared", WriteSharedPrimitive)(long_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("write-shared", argc);
    FObject port = (argc == 2 ? argv[1] : CurrentOutputPort());
    TextualOutputPortArgCheck("write-shared", port);

    WriteShared(port, argv[0], 0);
    return(NoValueObject);
}

Define("write-simple", WriteSimplePrimitive)(long_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("write-simple", argc);
    FObject port = (argc == 2 ? argv[1] : CurrentOutputPort());
    TextualOutputPortArgCheck("write-simple", port);

    WriteSimple(port, argv[0], 0);
    return(NoValueObject);
}

Define("display", DisplayPrimitive)(long_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("display", argc);
    FObject port = (argc == 2 ? argv[1] : CurrentOutputPort());
    TextualOutputPortArgCheck("display", port);

    Write(port, argv[0], 1);
    return(NoValueObject);
}

Define("newline", NewlinePrimitive)(long_t argc, FObject argv[])
{
    ZeroOrOneArgsCheck("newline", argc);
    FObject port = (argc == 1 ? argv[0] : CurrentOutputPort());
    TextualOutputPortArgCheck("newline", port);

    WriteCh(port, '\n');
    return(NoValueObject);
}

Define("write-char", WriteCharPrimitive)(long_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("write-char", argc);
    CharacterArgCheck("write-char", argv[0]);
    FObject port = (argc == 2 ? argv[1] : CurrentOutputPort());
    TextualOutputPortArgCheck("write-char", port);

    WriteCh(port, AsCharacter(argv[0]));
    return(NoValueObject);
}

Define("write-string", WriteStringPrimitive)(long_t argc, FObject argv[])
{
    OneToFourArgsCheck("write-string", argc);
    StringArgCheck("write-string", argv[0]);
    FObject port = (argc > 1 ? argv[1] : CurrentOutputPort());
    TextualOutputPortArgCheck("write-string", port);

    long_t strt;
    long_t end;
    if (argc > 2)
    {
        IndexArgCheck("write-string", argv[2], StringLength(argv[0]));

        strt = AsFixnum(argv[2]);

        if (argc > 3)
        {
            EndIndexArgCheck("write-string", argv[3], strt, StringLength(argv[0]));

            end = AsFixnum(argv[3]);
        }
        else
            end = (long_t) StringLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (long_t) StringLength(argv[0]);
    }

    WriteString(port, AsString(argv[0])->String + strt, end - strt);
    return(NoValueObject);
}

Define("write-u8", WriteU8Primitive)(long_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("write-u8", argc);
    ByteArgCheck("write-u8", argv[0]);
    FObject port = (argc == 2 ? argv[1] : CurrentOutputPort());
    BinaryOutputPortArgCheck("write-u8", port);

    FByte b = (FByte) AsFixnum(argv[0]);
    WriteBytes(port, &b, 1);
    return(NoValueObject);
}

Define("write-bytevector", WriteBytevectorPrimitive)(long_t argc, FObject argv[])
{
    OneToFourArgsCheck("write-bytevector", argc);
    BytevectorArgCheck("write-bytevector", argv[0]);
    FObject port = (argc > 1 ? argv[1] : CurrentOutputPort());
    BinaryOutputPortArgCheck("write-bytevector", port);

    long_t strt;
    long_t end;
    if (argc > 2)
    {
        IndexArgCheck("write-bytevector", argv[2], BytevectorLength(argv[0]));

        strt = AsFixnum(argv[2]);

        if (argc > 3)
        {
            EndIndexArgCheck("write-bytevector", argv[3], strt, BytevectorLength(argv[0]));

            end = AsFixnum(argv[3]);
        }
        else
            end = (long_t) BytevectorLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (long_t) BytevectorLength(argv[0]);
    }

    WriteBytes(port, AsBytevector(argv[0])->Vector + strt, end - strt);
    return(NoValueObject);
}

Define("flush-output-port", FlushOutputPortPrimitive)(long_t argc, FObject argv[])
{
    ZeroOrOneArgsCheck("flush-output-port", argc);
    FObject port = (argc == 1 ? argv[0] : CurrentOutputPort());
    OutputPortArgCheck("flush-output-port", port);

    FlushOutput(port);
    return(NoValueObject);
}

static FObject Primitives[] =
{
    WritePrimitive,
    WriteSharedPrimitive,
    WriteSimplePrimitive,
    DisplayPrimitive,
    NewlinePrimitive,
    WriteCharPrimitive,
    WriteStringPrimitive,
    WriteU8Primitive,
    WriteBytevectorPrimitive,
    FlushOutputPortPrimitive
};

void SetupWrite()
{
    for (ulong_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(Bedrock, BedrockLibrary, Primitives[idx]);
}
