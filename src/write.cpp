/*

Foment

*/

#include <windows.h>
#include "foment.hpp"
#include "syncthrd.hpp"
#include "io.hpp"
#include "compile.hpp"

// Write

typedef void (*FWriteFn)(FObject port, FObject obj, int df, void * wfn, void * ctx);
void WriteGeneric(FObject port, FObject obj, int df, FWriteFn wfn, void * ctx);

typedef struct
{
    FObject Hashtable;
    int Label;
} FWriteSharedCtx;

#define ToWriteSharedCtx(ctx) ((FWriteSharedCtx *) (ctx))

inline int SharedObjectP(FObject obj)
{
    return(PairP(obj) || BoxP(obj) || VectorP(obj) || ProcedureP(obj) || GenericRecordP(obj));
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

static void WritePrettyObject(FObject port, FObject obj, int df, FWriteFn wfn, void * ctx)
{
    if (EnvironmentP(obj))
    {
        WriteStringC(port, "#<");
        WriteGeneric(port, AsEnvironment(obj)->Name, 1, (FWriteFn) WriteGeneric, 0);
        WriteCh(port, '>');
    }
    else if (LibraryP(obj))
    {
        WriteStringC(port, "#<library: ");
        WriteGeneric(port, AsLibrary(obj)->Name, 1, (FWriteFn) WriteGeneric, 0);
/*        WriteStringC(port, " (");

        FObject lst = AsLibrary(obj)->Exports;
        while (PairP(lst))
        {
            FAssert(PairP(First(lst)));
            FAssert(SymbolP(First(First(lst))));

            WriteGeneric(port, First(First(lst)), 1, (FWriteFn) WriteGeneric, 0);
            lst = Rest(lst);

            if (lst != EmptyListObject)
                WriteCh(port, ' ');
        }

        WriteStringC(port, ")>");
*/
        WriteCh(port, '>');
    }
    else if (IdentifierP(obj))
    {
        WriteGeneric(port, AsIdentifier(obj)->Symbol, 1, (FWriteFn) WriteGeneric, 0);
        WriteCh(port, '.');
        WriteGeneric(port, AsIdentifier(obj)->Magic, 1, (FWriteFn) WriteGeneric, 0);
        WriteCh(port, '.');
        WriteGeneric(port, AsIdentifier(obj)->SyntacticEnv, 1, wfn, 0);
    }
    else if (ReferenceP(obj))
    {
        wfn(port, AsReference(obj)->Identifier, 1, wfn, ctx);
        WriteCh(port, ':');
        wfn(port, AsReference(obj)->Binding, 1, wfn, ctx);
    }
    else if (BindingP(obj))
    {
        FCh s[16];
        int sl = NumberAsString((FFixnum) obj, s, 16);

        WriteStringC(port, "#<binding: #x");
        WriteString(port, s, sl);
        WriteCh(port, ' ');

        obj = AsBinding(obj)->Identifier;
        WriteGeneric(port, AsIdentifier(obj)->Symbol, 1, (FWriteFn) WriteGeneric, 0);
        WriteCh(port, '.');
        WriteGeneric(port, AsIdentifier(obj)->Magic, 1, (FWriteFn) WriteGeneric, 0);
        WriteCh(port, '>');
    }
    else if (LambdaP(obj))
    {
        WriteStringC(port, "#<(lambda: ");
        if (AsLambda(obj)->Name != NoValueObject)
        {
            wfn(port, AsLambda(obj)->Name, 1, wfn, ctx);
            WriteCh(port, ' ');
        }

        wfn(port, AsLambda(obj)->Bindings, 1, wfn, ctx);

        WriteCh(port, ' ');
        wfn(port, AsLambda(obj)->Body, 1, wfn, ctx);
        WriteStringC(port, ")>");
    }
    else if (PatternVariableP(obj))
    {
        WriteCh(port, '{');
        wfn(port, AsPatternVariable(obj)->Variable, 1, wfn, ctx);
        WriteCh(port, '}');
    }
    else if (ctx == 0)
        WriteGeneric(port, obj, df, wfn, 0);
    else
        WriteSharedObject(port, obj, df, wfn, ctx);
}

void WritePretty(FObject port, FObject obj, int df)
{
    FObject ht = MakeEqHashtable(23);

    if (SharedObjectP(obj))
    {
        if (FindSharedObjects(ht, obj, 0, 1) == 0)
        {
            FAssert(HashtableSize(ht) == 0);

            WritePrettyObject(port, obj, df, (FWriteFn) WritePrettyObject, 0);
        }
        else
        {
            HashtableWalkUpdate(ht, WalkUpdate, NoValueObject);

            FWriteSharedCtx ctx;
            ctx.Hashtable = ht;
            ctx.Label = -1;
            WritePrettyObject(port, obj, df, (FWriteFn) WritePrettyObject, &ctx);
        }
    }
    else
        WritePrettyObject(port, obj, df, (FWriteFn) WritePrettyObject, 0);
}

// ---- Primitives ----

Define("write", WritePrimitive)(int argc, FObject argv[])
{
    OneOrTwoArgsCheck("write", argc);
    FObject port = (argc == 2 ? argv[1] : CurrentOutputPort());
    TextualOutputPortArgCheck("write", port);

    Write(port, argv[0], 0);
    return(NoValueObject);
}

Define("write-shared", WriteSharedPrimitive)(int argc, FObject argv[])
{
    OneOrTwoArgsCheck("write-shared", argc);
    FObject port = (argc == 2 ? argv[1] : CurrentOutputPort());
    TextualOutputPortArgCheck("write-shared", port);

    WriteShared(port, argv[0], 0);
    return(NoValueObject);
}

Define("write-simple", WriteSimplePrimitive)(int argc, FObject argv[])
{
    OneOrTwoArgsCheck("write-simple", argc);
    FObject port = (argc == 2 ? argv[1] : CurrentOutputPort());
    TextualOutputPortArgCheck("write-simple", port);

    WriteSimple(port, argv[0], 0);
    return(NoValueObject);
}

Define("display", DisplayPrimitive)(int argc, FObject argv[])
{
    OneOrTwoArgsCheck("display", argc);
    FObject port = (argc == 2 ? argv[1] : CurrentOutputPort());
    TextualOutputPortArgCheck("display", port);

    Write(port, argv[0], 1);
    return(NoValueObject);
}

Define("newline", NewlinePrimitive)(int argc, FObject argv[])
{
    ZeroOrOneArgsCheck("newline", argc);
    FObject port = (argc == 1 ? argv[0] : CurrentOutputPort());
    TextualOutputPortArgCheck("newline", port);

    WriteCh(port, '\n');
    return(NoValueObject);
}

Define("write-char", WriteCharPrimitive)(int argc, FObject argv[])
{
    OneOrTwoArgsCheck("write-char", argc);
    CharacterArgCheck("write-char", argv[0]);
    FObject port = (argc == 2 ? argv[1] : CurrentOutputPort());
    TextualOutputPortArgCheck("write-char", port);

    WriteCh(port, AsCharacter(argv[0]));
    return(NoValueObject);
}

Define("write-string", WriteStringPrimitive)(int argc, FObject argv[])
{
    OneToFourArgsCheck("write-string", argc);
    StringArgCheck("write-string", argv[0]);
    FObject port = (argc > 1 ? argv[1] : CurrentOutputPort());
    TextualOutputPortArgCheck("write-string", port);

    int strt;
    int end;
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
            end = (int) StringLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (int) StringLength(argv[0]);
    }

    WriteString(port, AsString(argv[0])->String + strt, end - strt);
    return(NoValueObject);
}

Define("write-u8", WriteU8Primitive)(int argc, FObject argv[])
{
    OneOrTwoArgsCheck("write-u8", argc);
    ByteArgCheck("write-u8", argv[0]);
    FObject port = (argc == 2 ? argv[1] : CurrentOutputPort());
    BinaryOutputPortArgCheck("write-u8", port);

    FByte b = (FByte) AsFixnum(argv[0]);
    WriteBytes(port, &b, 1);
    return(NoValueObject);
}

Define("write-bytevector", WriteBytevectorPrimitive)(int argc, FObject argv[])
{
    OneToFourArgsCheck("write-bytevector", argc);
    BytevectorArgCheck("write-bytevector", argv[0]);
    FObject port = (argc > 1 ? argv[1] : CurrentOutputPort());
    BinaryOutputPortArgCheck("write-bytevector", port);

    int strt;
    int end;
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
            end = (int) BytevectorLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (int) BytevectorLength(argv[0]);
    }

    WriteBytes(port, AsBytevector(argv[0])->Vector + strt, end - strt);
    return(NoValueObject);
}

Define("flush-output-port", FlushOutputPortPrimitive)(int argc, FObject argv[])
{
    ZeroOrOneArgsCheck("flush-output-port", argc);
    FObject port = (argc == 1 ? argv[0] : CurrentOutputPort());
    OutputPortArgCheck("flush-output-port", port);

    FlushOutput(port);
    return(NoValueObject);
}

static FPrimitive * Primitives[] =
{
    &WritePrimitive,
    &WriteSharedPrimitive,
    &WriteSimplePrimitive,
    &DisplayPrimitive,
    &NewlinePrimitive,
    &WriteCharPrimitive,
    &WriteStringPrimitive,
    &WriteU8Primitive,
    &WriteBytevectorPrimitive,
    &FlushOutputPortPrimitive
};

void SetupWrite()
{
    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
