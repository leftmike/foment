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

static inline int_t HasSlotsP(FObject obj)
{
    return(ObjectP(obj) && AsObjHdr(obj)->SlotCount() > 0);
}

FWriteContext::FWriteContext(FObject port, int_t df)
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
                int_t sl = FixnumAsString(- PreviousLabel, s, 10);
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
            int_t sl = FixnumAsString(- AsFixnum(val), s, 10);
            WriteString(s, sl);
            WriteCh('#');
        }
    }
    else
        WriteSimple(obj);
}

void FWriteContext::Display(FObject obj)
{
    int_t df = DisplayFlag;

    DisplayFlag = 1;
    Write(obj);
    DisplayFlag = df;
}

void FWriteContext::WriteCh(FCh ch)
{
    FAssert(WriteType != NoWrite);

    ::WriteCh(Port, ch);
}

void FWriteContext::WriteString(FCh * s, uint_t sl)
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
                for (uint_t idx = 0; idx < AsObjHdr(obj)->SlotCount(); idx++)
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

void FWriteContext::WritePair(FObject obj)
{
    FAssert(PairP(obj));

    WriteCh('(');
    for (;;)
    {
        Write(First(obj));
        if (PairP(Rest(obj)))
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

void FWriteContext::WriteRecord(FObject obj)
{
    if (BindingP(obj))
        Write(AsBinding(obj)->Identifier);
    else if (ReferenceP(obj))
        Write(AsReference(obj)->Identifier);
    else
    {
        FObject rt = AsGenericRecord(obj)->Fields[0];
        FCh s[16];
        int_t sl = FixnumAsString((FFixnum) obj, s, 16);

        WriteStringC("#<");
        Write(RecordTypeName(rt));
        WriteStringC(": #x");
        WriteString(s, sl);

        for (uint_t fdx = 1; fdx < RecordNumFields(obj); fdx++)
        {
            WriteCh(' ');
            Write(AsRecordType(rt)->Fields[fdx]);
            WriteStringC(": ");
            Write(AsGenericRecord(obj)->Fields[fdx]);
        }

        WriteStringC(">");
    }
}

void FWriteContext::WriteObject(FObject obj)
{
    switch (IndirectTag(obj))
    {
    case BoxTag:
    {
        FCh s[16];
        int_t sl = FixnumAsString((FFixnum) obj, s, 16);

        WriteStringC("#<box: #x");
        WriteString(s, sl);
        WriteCh(' ');
        Write(Unbox(obj));
        WriteStringC(">");
        break;
    }

    case StringTag:
        if (DisplayFlag)
            WriteString(AsString(obj)->String, StringLength(obj));
        else
        {
            WriteCh('"');

            for (uint_t idx = 0; idx < StringLength(obj); idx++)
            {
                FCh ch = AsString(obj)->String[idx];
                if (ch == '\\' || ch == '"')
                    WriteCh('\\');
                WriteCh(ch);
            }

            WriteCh('"');
        }
        break;

    case CStringTag:
    {
        const char * s = AsCString(obj)->String;

        if (DisplayFlag)
        {
            while (*s)
            {
                WriteCh(*s);
                s += 1;
            }
        }
        else
        {
            WriteCh('"');

            while (*s)
            {
                if (*s == '\\' || *s == '"')
                    WriteCh('\\');
                WriteCh(*s);
                s += 1;
            }

            WriteCh('"');
        }
        break;
    }

    case VectorTag:
    {
        WriteStringC("#(");
        for (uint_t idx = 0; idx < VectorLength(obj); idx++)
        {
            if (idx > 0)
                WriteCh(' ');
            Write(AsVector(obj)->Vector[idx]);
        }

        WriteCh(')');
        break;
    }

    case BytevectorTag:
    {
        FCh s[8];
        int_t sl;

        WriteStringC("#u8(");
        for (uint_t idx = 0; idx < BytevectorLength(obj); idx++)
        {
            if (idx > 0)
                WriteCh(' ');

            sl = FixnumAsString((FFixnum) AsBytevector(obj)->Vector[idx], s, 10);
            WriteString(s, sl);
        }

        WriteCh(')');
        break;
    }

    case BinaryPortTag:
    case TextualPortTag:
    {
        FCh s[16];
        int_t sl = FixnumAsString((FFixnum) obj, s, 16);

        WriteStringC("#<");
        if (TextualPortP(obj))
            WriteStringC("textual-");
        else
        {
            FAssert(BinaryPortP(obj));

            WriteStringC("binary-");
        }

        if (InputPortP(obj))
            WriteStringC("input-");
        if (OutputPortP(obj))
            WriteStringC("output-");
        WriteStringC("port: #x");
        WriteString(s, sl);

        if (InputPortOpenP(obj) == 0 && OutputPortOpenP(obj) == 0)
            WriteStringC(" closed");

        if (StringP(AsGenericPort(obj)->Name))
        {
            WriteCh(' ');
            WriteString(AsString(AsGenericPort(obj)->Name)->String,
                    StringLength(AsGenericPort(obj)->Name));
        }

        if (InputPortP(obj))
        {
            if (BinaryPortP(obj))
            {
                WriteStringC(" offset: ");
                sl = FixnumAsString(GetOffset(obj), s, 10);
                WriteString(s, sl);
            }
            else
            {
                FAssert(TextualPortP(obj));

                WriteStringC(" line: ");
                sl = FixnumAsString(GetLineColumn(obj, 0), s, 10);
                WriteString(s, sl);
            }
        }

        WriteCh('>');
        break;
    }

    case ProcedureTag:
    {
        FCh s[16];
        int_t sl = FixnumAsString((FFixnum) obj, s, 16);

        WriteStringC("#<procedure: ");
        WriteString(s, sl);

        if (AsProcedure(obj)->Name != NoValueObject)
        {
            WriteCh(' ');
            Write(AsProcedure(obj)->Name);
        }

        if (AsProcedure(obj)->Flags & PROCEDURE_FLAG_CLOSURE)
            WriteStringC(" closure");

        if (AsProcedure(obj)->Flags & PROCEDURE_FLAG_PARAMETER)
            WriteStringC(" parameter");

        if (AsProcedure(obj)->Flags & PROCEDURE_FLAG_CONTINUATION)
            WriteStringC(" continuation");

        if (StringP(AsProcedure(obj)->Filename) && FixnumP(AsProcedure(obj)->LineNumber))
        {
            WriteCh(' ');
            Write(AsProcedure(obj)->Filename);
            WriteCh('[');
            Write(AsProcedure(obj)->LineNumber);
            WriteCh(']');
        }

//        WriteCh(' ');
//        Write(AsProcedure(obj)->Code);
        WriteCh('>');
        break;
    }

    case SymbolTag:
        if (StringP(AsSymbol(obj)->String))
            WriteString(AsString(AsSymbol(obj)->String)->String,
                    StringLength(AsSymbol(obj)->String));
        else
        {
            FAssert(CStringP(AsSymbol(obj)->String));

            WriteStringC(AsCString(AsSymbol(obj)->String)->String);
        }
        break;

    case RecordTypeTag:
    {
        FCh s[16];
        int_t sl = FixnumAsString((FFixnum) obj, s, 16);

        WriteStringC("#<record-type: #x");
        WriteString(s, sl);
        WriteCh(' ');
        Write(RecordTypeName(obj));

        for (uint_t fdx = 1; fdx < RecordTypeNumFields(obj); fdx += 1)
        {
            WriteCh(' ');
            Write(AsRecordType(obj)->Fields[fdx]);
        }

        WriteStringC(">");
        break;
    }

    case RecordTag:
        WriteRecord(obj);
        break;

    case PrimitiveTag:
    {
        FAssert(SymbolP(AsPrimitive(obj)->Name));
        FAssert(CStringP(AsSymbol(AsPrimitive(obj)->Name)->String));

        WriteStringC("#<primitive: ");
        WriteStringC(AsCString(AsSymbol(AsPrimitive(obj)->Name)->String)->String);
        WriteCh(' ');

        const char * fn = AsPrimitive(obj)->Filename;
        const char * p = fn;
        while (*p != 0)
        {
            if (*p == '/' || *p == '\\')
                fn = p + 1;

            p += 1;
        }

        WriteStringC(fn);
        WriteCh('@');
        FCh s[16];
        int_t sl = FixnumAsString(AsPrimitive(obj)->LineNumber, s, 10);
        WriteString(s, sl);
        WriteCh('>');
        break;
    }

    case ThreadTag:
        WriteThread(this, obj);
        break;

    case ExclusiveTag:
        WriteExclusive(this, obj);
        break;

    case ConditionTag:
        WriteCondition(this, obj);
        break;

    case HashNodeTag:
    {
        FCh s[16];
        int_t sl = FixnumAsString((FFixnum) obj, s, 16);

        WriteStringC("#<hash-node: ");
        WriteString(s, sl);
        WriteCh('>');
        break;
    }

    case EphemeronTag:
    {
        FCh s[16];
        int_t sl = FixnumAsString((FFixnum) obj, s, 16);

        WriteStringC("#<ephemeron: ");
        WriteString(s, sl);
        if (EphemeronBrokenP(obj))
            WriteStringC(" (broken)");
        WriteCh('>');
        break;
    }

    case BuiltinTypeTag:
        WriteStringC("#<");
        WriteStringC(AsBuiltinType(obj)->Name);
        WriteStringC("-type>");
        break;

    case BuiltinTag:
        FAssert(BuiltinObjectP(obj));
        FAssert(BuiltinTypeP(AsBuiltin(obj)->BuiltinType));

        if (AsBuiltinType(AsBuiltin(obj)->BuiltinType)->Write != 0)
            AsBuiltinType(AsBuiltin(obj)->BuiltinType)->Write(this, obj);
        else
        {
            FCh s[16];
            int_t sl = FixnumAsString((FFixnum) obj, s, 16);

            WriteStringC("#<");
            WriteStringC(AsBuiltinType(AsBuiltin(obj)->BuiltinType)->Name);
            WriteStringC(": #x");
            WriteString(s, sl);
            WriteStringC(">");
        }
        break;

    default:
    {
        FCh s[16];
        int_t sl = FixnumAsString((FFixnum) obj, s, 16);

        WriteStringC("#<unknown: ");
        WriteString(s, sl);
        WriteCh('>');
        break;
    }

    }
}

void FWriteContext::WriteSimple(FObject obj)
{
    if (NumberP(obj))
    {
        obj = NumberToString(obj, 10);
        WriteString(AsString(obj)->String, StringLength(obj));
    }
    else if (CharacterP(obj))
    {
        if (AsCharacter(obj) < 128)
        {
            if (DisplayFlag == 0)
                WriteStringC("#\\");
            WriteCh(AsCharacter(obj));
        }
        else
        {
            if (DisplayFlag)
                WriteCh(AsCharacter(obj));
            else
            {
                FCh s[16];
                int_t sl = FixnumAsString(AsCharacter(obj), s, 16);
                WriteStringC("#\\x");
                WriteString(s, sl);
            }
        }
    }
    else if (SpecialSyntaxP(obj))
        WriteSpecialSyntax(this, obj);
    else if (InstructionP(obj))
        WriteInstruction(this, obj);
    else if (ValuesCountP(obj))
    {
        WriteStringC("#<values-count: ");

        FCh s[16];
        int_t sl = FixnumAsString(AsValuesCount(obj), s, 10);
        WriteString(s, sl);

        WriteCh('>');
    }
    else if (PairP(obj))
        WritePair(obj);
    else if (ObjectP(obj))
        WriteObject(obj);
    else if (obj == EmptyListObject)
        WriteStringC("()");
    else if (obj == FalseObject)
        WriteStringC("#f");
    else if (obj == TrueObject)
        WriteStringC("#t");
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
    {
        FCh s[16];
        int_t sl = FixnumAsString((FFixnum) obj, s, 16);

        WriteStringC("#<unknown: ");
        WriteString(s, sl);
        WriteCh('>');
    }
}

void Write(FObject port, FObject obj, int_t df)
{
    FWriteContext wctx(port, df);

    wctx.Prepare(obj, CircularWrite);
    wctx.Write(obj);
}

void WriteShared(FObject port, FObject obj, int_t df)
{
    FWriteContext wctx(port, df);

    wctx.Prepare(obj, SharedWrite);
    wctx.Write(obj);
}

void WriteSimple(FObject port, FObject obj, int_t df)
{
    FWriteContext wctx(port, df);

    wctx.Prepare(obj, SimpleWrite);
    wctx.Write(obj);
}

// ---- Primitives ----

Define("write", WritePrimitive)(int_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("write", argc);
    FObject port = (argc == 2 ? argv[1] : CurrentOutputPort());
    TextualOutputPortArgCheck("write", port);

    Write(port, argv[0], 0);
    return(NoValueObject);
}

Define("write-shared", WriteSharedPrimitive)(int_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("write-shared", argc);
    FObject port = (argc == 2 ? argv[1] : CurrentOutputPort());
    TextualOutputPortArgCheck("write-shared", port);

    WriteShared(port, argv[0], 0);
    return(NoValueObject);
}

Define("write-simple", WriteSimplePrimitive)(int_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("write-simple", argc);
    FObject port = (argc == 2 ? argv[1] : CurrentOutputPort());
    TextualOutputPortArgCheck("write-simple", port);

    WriteSimple(port, argv[0], 0);
    return(NoValueObject);
}

Define("display", DisplayPrimitive)(int_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("display", argc);
    FObject port = (argc == 2 ? argv[1] : CurrentOutputPort());
    TextualOutputPortArgCheck("display", port);

    Write(port, argv[0], 1);
    return(NoValueObject);
}

Define("newline", NewlinePrimitive)(int_t argc, FObject argv[])
{
    ZeroOrOneArgsCheck("newline", argc);
    FObject port = (argc == 1 ? argv[0] : CurrentOutputPort());
    TextualOutputPortArgCheck("newline", port);

    WriteCh(port, '\n');
    return(NoValueObject);
}

Define("write-char", WriteCharPrimitive)(int_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("write-char", argc);
    CharacterArgCheck("write-char", argv[0]);
    FObject port = (argc == 2 ? argv[1] : CurrentOutputPort());
    TextualOutputPortArgCheck("write-char", port);

    WriteCh(port, AsCharacter(argv[0]));
    return(NoValueObject);
}

Define("write-string", WriteStringPrimitive)(int_t argc, FObject argv[])
{
    OneToFourArgsCheck("write-string", argc);
    StringArgCheck("write-string", argv[0]);
    FObject port = (argc > 1 ? argv[1] : CurrentOutputPort());
    TextualOutputPortArgCheck("write-string", port);

    int_t strt;
    int_t end;
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
            end = (int_t) StringLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (int_t) StringLength(argv[0]);
    }

    WriteString(port, AsString(argv[0])->String + strt, end - strt);
    return(NoValueObject);
}

Define("write-u8", WriteU8Primitive)(int_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("write-u8", argc);
    ByteArgCheck("write-u8", argv[0]);
    FObject port = (argc == 2 ? argv[1] : CurrentOutputPort());
    BinaryOutputPortArgCheck("write-u8", port);

    FByte b = (FByte) AsFixnum(argv[0]);
    WriteBytes(port, &b, 1);
    return(NoValueObject);
}

Define("write-bytevector", WriteBytevectorPrimitive)(int_t argc, FObject argv[])
{
    OneToFourArgsCheck("write-bytevector", argc);
    BytevectorArgCheck("write-bytevector", argv[0]);
    FObject port = (argc > 1 ? argv[1] : CurrentOutputPort());
    BinaryOutputPortArgCheck("write-bytevector", port);

    int_t strt;
    int_t end;
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
            end = (int_t) BytevectorLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (int_t) BytevectorLength(argv[0]);
    }

    WriteBytes(port, AsBytevector(argv[0])->Vector + strt, end - strt);
    return(NoValueObject);
}

Define("flush-output-port", FlushOutputPortPrimitive)(int_t argc, FObject argv[])
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
    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(Bedrock, BedrockLibrary, Primitives[idx]);
}
