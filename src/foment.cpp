/*

Foment

*/

#ifdef FOMENT_WINDOWS
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#define exit(n) _exit(n)
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
#include <unistd.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <ctype.h>
#include <pthread.h>
#endif // FOMENT_UNIX

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "foment.hpp"
#include "syncthrd.hpp"
#include "unicode.hpp"

#if defined(FOMENT_BSD) || defined(FOMENT_OSX)
extern char ** environ;
#endif // FOMENT_BSD

#ifdef FOMENT_WINDOWS
static ULONGLONG StartingTicks = 0;
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
static struct utsname utsname;
static time_t StartingSecond = 0;

static uint64_t GetMillisecondCount64()
{
    struct timeval tv;
    struct timezone tz;

    gettimeofday(&tv, &tz);

    FAssert(tv.tv_sec >= StartingSecond);

    uint64_t mc = (tv.tv_sec - StartingSecond) * 1000;
    mc += (tv.tv_usec / 1000);

    return(mc);
}
#endif // FOMENT_UNIX

ulong_t SetupComplete = 0;
ulong_t RandomSeed = 0;

ulong_t CheckHeapFlag = 0;
ulong_t VerboseFlag = 0;

EternalSymbol(BeginSymbol, "begin");
EternalSymbol(QuoteSymbol, "quote");
EternalSymbol(QuasiquoteSymbol, "quasiquote");
EternalSymbol(UnquoteSymbol, "unquote");
EternalSymbol(UnquoteSplicingSymbol, "unquote-splicing");
EternalSymbol(Assertion, "assertion-violation");
EternalSymbol(Restriction, "implementation-restriction");
EternalSymbol(Lexical, "lexical-violation");
EternalSymbol(Syntax, "syntax-violation");
EternalSymbol(Error, "error-violation");
EternalSymbol(MakeObjectSymbol, "%make-object");
EternalSymbol(CollectSymbol, "%collect");
EternalSymbol(StartThreadSymbol, "%start-thread");
EternalSymbol(ExecuteSymbol, "%execute");

// ---- Roots ----

FObject SymbolHashTable = NoValueObject;
FObject Bedrock = NoValueObject;
FObject BedrockLibrary = NoValueObject;
FObject LoadedLibraries = EmptyListObject;
FObject Features = EmptyListObject;
FObject LibraryPath = EmptyListObject;
FObject LibraryExtensions = NoValueObject;
FObject MakeObjectOutOfMemory = NoValueObject;
FObject CollectOutOfMemory = NoValueObject;
FObject StartThreadOutOfMemory = NoValueObject;
FObject ExecuteStackOverflow = NoValueObject;

static FObject FomentLibrariesVector = NoValueObject;

void ErrorExitFoment(const char * what, const char * msg)
{
    printf("\n%s: %s\n", what, msg);
    if (SetupComplete)
    {
        if (CheckHeapFlag || VerboseFlag)
            printf("RandomSeed: " ULONG_FMT "\n", RandomSeed);
        ExitFoment();
    }

    exit(1);
}

static char FailedMessage[512];

void FAssertFailed(const char * fn, long_t ln, const char * expr)
{
    sprintf_s(FailedMessage, sizeof(FailedMessage), "%s (%d)%s", expr, (int) ln, fn);
    ErrorExitFoment("assert", FailedMessage);
}

void FMustBeFailed(const char * fn, long_t ln, const char * expr)
{
    sprintf_s(FailedMessage, sizeof(FailedMessage), "%s (%d)%s", expr, (int) ln, fn);
    ErrorExitFoment("must-be", FailedMessage);
}

// ---- Immediates ----

static const char * SpecialSyntaxes[] =
{
    "quote",
    "lambda",
    "if",
    "set!",
    "let",
    "let*",
    "letrec",
    "letrec*",
    "let-values",
    "let*-values",
    "letrec-values",
    "letrec*-values",
    "let-syntax",
    "letrec-syntax",
    "or",
    "begin",
    "do",
    "syntax-rules",
    "syntax-error",
    "include",
    "include-ci",
    "cond-expand",
    "case-lambda",
    "quasiquote",

    "define",
    "define-values",
    "define-syntax",

    "else",
    "=>",
    "unquote",
    "unquote-splicing",
    "...",
    "_",

    "set!-values"
};

const char * SpecialSyntaxToName(FObject obj)
{
    FAssert(SpecialSyntaxP(obj));

    long_t n = AsValue(obj);
    FAssert(n >= 0);
    FAssert(n < (long_t) (sizeof(SpecialSyntaxes) / sizeof(char *)));

    return(SpecialSyntaxes[n]);
}

void WriteSpecialSyntax(FWriteContext * wctx, FObject obj)
{
    const char * n = SpecialSyntaxToName(obj);

    wctx->WriteStringC("#<syntax: ");
    wctx->WriteStringC(n);
    wctx->WriteCh('>');
}

// ---- Booleans ----

Define("not", NotPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("not", argc);

    return(argv[0] == FalseObject ? TrueObject : FalseObject);
}

Define("boolean?", BooleanPPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("boolean?", argc);

    return(BooleanP(argv[0]) ? TrueObject : FalseObject);
}

Define("boolean=?", BooleanEqualPPrimitive)(long_t argc, FObject argv[])
{
    AtLeastTwoArgsCheck("boolean=?", argc);
    BooleanArgCheck("boolean=?", argv[0]);

    for (long_t adx = 1; adx < argc; adx++)
    {
        BooleanArgCheck("boolean=?", argv[adx]);

        if (argv[adx - 1] != argv[adx])
            return(FalseObject);
    }

    return(TrueObject);
}

// ---- Symbols ----

FObject StringCToSymbol(const char * s)
{
    return(StringToSymbol(MakeStringC(s)));
}

FObject AddPrefixToSymbol(FObject str, FObject sym)
{
    FAssert(StringP(str));
    FAssert(SymbolP(sym));

    FObject sstr = SymbolToString(sym);

    FAssert(StringP(sstr));

    FObject nstr = MakeStringCh(StringLength(str) + StringLength(sstr), 0);
    ulong_t sdx;
    for (sdx = 0; sdx < StringLength(str); sdx++)
        AsString(nstr)->String[sdx] = AsString(str)->String[sdx];

    for (ulong_t idx = 0; idx < StringLength(sstr); idx++)
        AsString(nstr)->String[sdx + idx] = AsString(sstr)->String[idx];

    return(StringToSymbol(nstr));
}

Define("symbol?", SymbolPPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("symbol?", argc);

    return(SymbolP(argv[0]) ? TrueObject : FalseObject);
}

Define("symbol=?", SymbolEqualPPrimitive)(long_t argc, FObject argv[])
{
    AtLeastTwoArgsCheck("symbol=?", argc);
    SymbolArgCheck("symbol=?", argv[0]);

    for (long_t adx = 1; adx < argc; adx++)
    {
        SymbolArgCheck("symbol=?", argv[adx]);

        if (argv[adx - 1] != argv[adx])
            return(FalseObject);
    }

    return(TrueObject);
}

Define("symbol->string", SymbolToStringPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("symbol->string", argc);
    SymbolArgCheck("symbol->string", argv[0]);

    return(SymbolToString(argv[0]));
}

Define("string->symbol", StringToSymbolPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("string->symbol", argc);
    StringArgCheck("string->symbol", argv[0]);

    return(StringToSymbol(argv[0]));
}

// ---- Exceptions ----

static void WriteLocation(FWriteContext * wctx, FObject obj)
{
    if (PairP(obj))
    {
        if (IdentifierP(First(obj)))
            obj = First(obj);
        else if (PairP(First(obj)) && IdentifierP(First(First(obj))))
            obj = First(First(obj));
    }

    if (IdentifierP(obj) && FixnumP(AsIdentifier(obj)->LineNumber)
            && AsFixnum(AsIdentifier(obj)->LineNumber) > 0)
    {
        FCh s[16];
        long_t sl = FixnumAsString(AsFixnum(AsIdentifier(obj)->LineNumber), s, 10);

        wctx->WriteStringC(" line: ");
        wctx->WriteString(s, sl);
    }
}

static void
WriteException(FWriteContext * wctx, FObject obj)
{
    FCh s[16];
    long_t sl = FixnumAsString((long_t) obj, s, 16);

    wctx->WriteStringC("#<exception: #x");
    wctx->WriteString(s, sl);

    wctx->WriteCh(' ');
    wctx->Write(AsException(obj)->Type);

    if (SymbolP(AsException(obj)->Who))
    {
        wctx->WriteCh(' ');
        wctx->Write(AsException(obj)->Who);
    }

    wctx->WriteCh(' ');
    wctx->Write(AsException(obj)->Message);

    wctx->WriteStringC(" irritants: ");
    wctx->Write(AsException(obj)->Irritants);

    WriteLocation(wctx, AsException(obj)->Irritants);
    wctx->WriteStringC(">");
}

EternalBuiltinType(ExceptionType, "exception", WriteException);

FObject MakeException(FObject typ, FObject who, FObject knd, FObject msg, FObject lst)
{
    FException * exc = (FException *) MakeBuiltin(ExceptionType, 6, "make-exception");
    exc->Type = typ;
    exc->Who = who;
    exc->Kind = knd;
    exc->Message = msg;
    exc->Irritants = lst;

    return(exc);
}

void RaiseException(FObject typ, FObject who, FObject knd, FObject msg, FObject lst)
{
    FThreadState * ts = GetThreadState();

    if (ts->ExceptionCount > 0)
        ErrorExitFoment("error", "recursive exception");
    ts->ExceptionCount += 1;
    FObject exc = MakeException(typ, who, knd, msg, lst);

    FAssert(ts->ExceptionCount > 0);

    ts->ExceptionCount -= 1;
    Raise(exc);
}

void RaiseExceptionC(FObject typ, const char * who, FObject knd, const char * msg, FObject lst)
{
    char buf[128];
    FThreadState * ts = GetThreadState();
    FObject exc;

    if (ts->ExceptionCount > 0)
    {
        sprintf_s(FailedMessage, sizeof(FailedMessage), "recursive exception: %s: %s\n", who, msg);
        ErrorExitFoment("error", FailedMessage);
    }
    ts->ExceptionCount += 1;

    FAssert(strlen(who) + strlen(msg) + 3 < sizeof(buf));

    if (strlen(who) + strlen(msg) + 3 >= sizeof(buf))
        exc = MakeException(typ, StringCToSymbol(who), knd, MakeStringC(msg), lst);
    else
    {
        strcpy(buf, who);
        strcat(buf, ": ");
        strcat(buf, msg);

        exc = MakeException(typ, StringCToSymbol(who), knd, MakeStringC(buf), lst);
    }

    FAssert(ts->ExceptionCount > 0);

    ts->ExceptionCount -= 1;
    Raise(exc);
}

void Raise(FObject obj)
{
    throw obj;
}

Define("raise", RaisePrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("raise", argc);

    Raise(argv[0]);
    return(NoValueObject);
}

Define("error", ErrorPrimitive)(long_t argc, FObject argv[])
{
    AtLeastOneArgCheck("error", argc);
    StringArgCheck("error", argv[0]);

    FObject lst = EmptyListObject;
    while (argc > 1)
    {
        argc -= 1;
        lst = MakePair(argv[argc], lst);
    }

    Raise(MakeException(Assertion, StringCToSymbol("error"), NoValueObject, argv[0], lst));
    return(NoValueObject);
}

Define("error-object?", ErrorObjectPPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("error-object?", argc);

    return(ExceptionP(argv[0]) ? TrueObject : FalseObject);
}

Define("error-object-type", ErrorObjectTypePrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("error-object-type", argc);
    ExceptionArgCheck("error-object-type", argv[0]);

    return(AsException(argv[0])->Type);
}

Define("error-object-who", ErrorObjectWhoPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("error-object-who", argc);
    ExceptionArgCheck("error-object-who", argv[0]);

    return(AsException(argv[0])->Who);
}

Define("error-object-kind", ErrorObjectKindPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("error-object-kind", argc);
    ExceptionArgCheck("error-object-kind", argv[0]);

    return(AsException(argv[0])->Kind);
}

Define("error-object-message", ErrorObjectMessagePrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("error-object-message", argc);
    ExceptionArgCheck("error-object-message", argv[0]);

    return(AsException(argv[0])->Message);
}

Define("error-object-irritants", ErrorObjectIrritantsPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("error-object-irritants", argc);
    ExceptionArgCheck("error-object-irritants", argv[0]);

    return(AsException(argv[0])->Irritants);
}

Define("full-error", FullErrorPrimitive)(long_t argc, FObject argv[])
{
    AtLeastFourArgsCheck("full-error", argc);
    SymbolArgCheck("full-error", argv[0]);
    SymbolArgCheck("full-error", argv[1]);
    StringArgCheck("full-error", argv[3]);

    FObject lst = EmptyListObject;
    while (argc > 4)
    {
        argc -= 1;
        lst = MakePair(argv[argc], lst);
    }

    Raise(MakeException(argv[0], argv[1], argv[2], argv[3], lst));
    return(NoValueObject);
}

// ---- System interface ----

Define("command-line", CommandLinePrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("command-line", argc);

    return(CommandLine);
}

Define("full-command-line", FullCommandLinePrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("full-command-line", argc);

    return(FullCommandLine);
}

static void GetEnvironmentVariables()
{
#ifdef FOMENT_WINDOWS
    FChS ** envp = _wenviron;
#endif // FOMENT_WINDOWS
#ifdef FOMENT_UNIX
    FChS ** envp = environ;
#endif // FOMENT_UNIX
    FObject lst = EmptyListObject;

    while (*envp)
    {
        FChS * s = *envp;
        while (*s)
        {
            if (*s == (FChS) '=')
                break;
            s += 1;
        }

        FAssert(*s != 0);

        if (*s != 0)
            lst = MakePair(MakePair(MakeStringS(*envp, s - *envp), MakeStringS(s + 1)), lst);

        envp += 1;
    }

    EnvironmentVariables = lst;
}

Define("get-environment-variable", GetEnvironmentVariablePrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("get-environment-variable", argc);
    StringArgCheck("get-environment-variable", argv[0]);

    FAssert(PairP(EnvironmentVariables));

    FObject ret = Assoc(argv[0], EnvironmentVariables);
    if (PairP(ret))
        return(Rest(ret));
    return(ret);
}

Define("get-environment-variables", GetEnvironmentVariablesPrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("get-environment-variables", argc);

    FAssert(PairP(EnvironmentVariables));

    return(EnvironmentVariables);
}

Define("current-second", CurrentSecondPrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("current-second", argc);

    time_t t = time(0);

    return(MakeFlonum((double64_t) t));
}

Define("current-jiffy", CurrentJiffyPrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("current-jiffy", argc);

#ifdef FOMENT_WINDOWS
    ULONGLONG tc = (GetTickCount64() - StartingTicks);
    return(MakeFixnum(tc));
#endif // FOMENT_WINDOWS
#ifdef FOMENT_UNIX
    return(MakeFixnum(GetMillisecondCount64()));
#endif // FOMENT_UNIX
}

Define("jiffies-per-second", JiffiesPerSecondPrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("jiffies-per-second", argc);

    return(MakeFixnum(1000));
}

Define("features", FeaturesPrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("features", argc);

    return(Features);
}

Define("%set-features!", SetFeaturesPrimitive)(long_t argc, FObject argv[])
{
    FMustBe(argc == 1);

    Features = argv[0];
    return(NoValueObject);
}

// ---- Boxes ----

FObject MakeBox(FObject val)
{
    return(MakeBox(val, 0));
}

FObject MakeBox(FObject val, ulong_t idx)
{
    FBox * bx = (FBox *) MakeObject(BoxTag, sizeof(FBox), 1, "box");
    bx->Value = val;
    bx->Index = idx;

    return(bx);
}

static void WriteBox(FWriteContext * wctx, FObject obj)
{
    FCh s[16];
    long_t sl = FixnumAsString((long_t) obj, s, 16);

    wctx->WriteStringC("#<box: #x");
    wctx->WriteString(s, sl);
    wctx->WriteCh(' ');
    wctx->Write(Unbox(obj));
    wctx->WriteStringC(">");
}

// ---- Builtin Types ----

static void WriteBuiltinType(FWriteContext * wctx, FObject obj)
{
    wctx->WriteStringC("#<");
    wctx->WriteStringC(AsBuiltinType(obj)->Name);
    wctx->WriteStringC("-type>");
}

// ---- Builtins ----

FObject MakeBuiltin(FObject bt, ulong_t sc, const char * who)
{
    FAssert(BuiltinTypeP(bt));

    FBuiltin * bltn = (FBuiltin *) MakeObject(BuiltinTag, sc * sizeof(FObject), sc, who);
    bltn->BuiltinType = bt;

    return(bltn);
}

static void WriteBuiltin(FWriteContext * wctx, FObject obj)
{
    FAssert(BuiltinObjectP(obj));
    FAssert(BuiltinTypeP(AsBuiltin(obj)->BuiltinType));

    if (AsBuiltinType(AsBuiltin(obj)->BuiltinType)->Write != 0)
        AsBuiltinType(AsBuiltin(obj)->BuiltinType)->Write(wctx, obj);
    else
    {
        FCh s[16];
        long_t sl = FixnumAsString((long_t) obj, s, 16);

        wctx->WriteStringC("#<");
        wctx->WriteStringC(AsBuiltinType(AsBuiltin(obj)->BuiltinType)->Name);
        wctx->WriteStringC(": #x");
        wctx->WriteString(s, sl);
        wctx->WriteStringC(">");
    }
}

// ---- Record Types ----

FObject MakeRecordType(FObject nam, ulong_t nf, FObject flds[])
{
    FAssert(SymbolP(nam));

    FRecordType * rt = (FRecordType *) MakeObject(RecordTypeTag,
            sizeof(FRecordType) + sizeof(FObject) * nf, nf + 1, "%make-record-type");
    rt->Fields[0] = nam;

    for (ulong_t fdx = 1; fdx <= nf; fdx++)
    {
        FAssert(SymbolP(flds[fdx - 1]));

        rt->Fields[fdx] = flds[fdx - 1];
    }

    return(rt);
}

static void WriteRecordType(FWriteContext * wctx, FObject obj)
{
    FCh s[16];
    long_t sl = FixnumAsString((long_t) obj, s, 16);

    wctx->WriteStringC("#<record-type: #x");
    wctx->WriteString(s, sl);
    wctx->WriteCh(' ');
    wctx->Write(RecordTypeName(obj));

    for (ulong_t fdx = 1; fdx < RecordTypeNumFields(obj); fdx += 1)
    {
        wctx->WriteCh(' ');
        wctx->Write(AsRecordType(obj)->Fields[fdx]);
    }

    wctx->WriteStringC(">");
}

Define("%make-record-type", MakeRecordTypePrimitive)(long_t argc, FObject argv[])
{
    // (%make-record-type <record-type-name> (<field> ...))

    FMustBe(argc == 2);

    SymbolArgCheck("define-record-type", argv[0]);

    FObject flds = EmptyListObject;
    FObject flst = argv[1];
    while (PairP(flst))
    {
        if (PairP(First(flst)) == 0 || SymbolP(First(First(flst))) == 0)
            RaiseExceptionC(Assertion, "define-record-type", "expected a list of fields",
                    List(argv[1], First(flst)));

        if (Memq(First(First(flst)), flds) != FalseObject)
            RaiseExceptionC(Assertion, "define-record-type", "duplicate field name",
                    List(argv[1], First(flst)));

        flds = MakePair(First(First(flst)), flds);
        flst = Rest(flst);
    }

    FAssert(flst == EmptyListObject);

    flds = ListToVector(ReverseListModify(flds));
    return(MakeRecordType(argv[0], VectorLength(flds), AsVector(flds)->Vector));
}

Define("%make-record", MakeRecordPrimitive)(long_t argc, FObject argv[])
{
    // (%make-record <record-type>)

    FMustBe(argc == 1);
    FMustBe(RecordTypeP(argv[0]));

    return(MakeRecord(argv[0]));
}

Define("%record-predicate", RecordPredicatePrimitive)(long_t argc, FObject argv[])
{
    // (%record-predicate <record-type> <obj>)

    FMustBe(argc == 2);
    FMustBe(RecordTypeP(argv[0]));

    return(RecordP(argv[1], argv[0]) ? TrueObject : FalseObject);
}

Define("%record-index", RecordIndexPrimitive)(long_t argc, FObject argv[])
{
    // (%record-index <record-type> <field-name>)

    FMustBe(argc == 2);
    FMustBe(RecordTypeP(argv[0]));

    for (ulong_t rdx = 1; rdx < RecordTypeNumFields(argv[0]); rdx++)
        if (EqP(argv[1], AsRecordType(argv[0])->Fields[rdx]))
            return(MakeFixnum(rdx));

    RaiseExceptionC(Assertion, "define-record-type", "expected a field-name",
            List(argv[1], argv[0]));

    return(NoValueObject);
}

Define("%record-ref", RecordRefPrimitive)(long_t argc, FObject argv[])
{
    // (%record-ref <record-type> <obj> <index>)

    FMustBe(argc == 3);
    FMustBe(RecordTypeP(argv[0]));

    if (RecordP(argv[1], argv[0]) == 0)
        RaiseExceptionC(Assertion, "%record-ref", "not a record of the expected type",
                List(argv[1], argv[0]));

    FMustBe(FixnumP(argv[2]));
    FMustBe(AsFixnum(argv[2]) > 0 && AsFixnum(argv[2]) < (long_t) RecordNumFields(argv[1]));

    return(AsGenericRecord(argv[1])->Fields[AsFixnum(argv[2])]);
}

Define("%record-set!", RecordSetPrimitive)(long_t argc, FObject argv[])
{
    // (%record-set! <record-type> <obj> <index> <value>)

    FMustBe(argc == 4);
    FMustBe(RecordTypeP(argv[0]));

    if (RecordP(argv[1], argv[0]) == 0)
        RaiseExceptionC(Assertion, "%record-set!", "not a record of the expected type",
                List(argv[1], argv[0]));

    FMustBe(FixnumP(argv[2]));
    FMustBe(AsFixnum(argv[2]) > 0 && AsFixnum(argv[2]) < (long_t) RecordNumFields(argv[1]));

//    AsGenericRecord(argv[1])->Fields[AsFixnum(argv[2])] = argv[3];
    Modify(FGenericRecord, argv[1], Fields[AsFixnum(argv[2])], argv[3]);
    return(NoValueObject);
}

// ---- Records ----

FObject MakeRecord(FObject rt)
{
    FAssert(RecordTypeP(rt));

    ulong_t nf = RecordTypeNumFields(rt);

    FGenericRecord * r = (FGenericRecord *) MakeObject(RecordTag,
            sizeof(FGenericRecord) + sizeof(FObject) * (nf - 1), nf, "%make-record");
    r->Fields[0] = rt;

    for (ulong_t fdx = 1; fdx < nf; fdx++)
        r->Fields[fdx] = NoValueObject;

    return(r);
}

static void WriteRecord(FWriteContext * wctx, FObject obj)
{
    FObject rt = AsGenericRecord(obj)->Fields[0];
    FCh s[16];
    long_t sl = FixnumAsString((long_t) obj, s, 16);

    wctx->WriteStringC("#<");
    wctx->Write(RecordTypeName(rt));
    wctx->WriteStringC(": #x");
    wctx->WriteString(s, sl);

    for (ulong_t fdx = 1; fdx < RecordNumFields(obj); fdx++)
    {
        wctx->WriteCh(' ');
        wctx->Write(AsRecordType(rt)->Fields[fdx]);
        wctx->WriteStringC(": ");
        wctx->Write(AsGenericRecord(obj)->Fields[fdx]);
    }

    wctx->WriteStringC(">");
}

// ---- Primitives ----

void DefinePrimitive(FObject env, FObject lib, FObject prim)
{
    FAssert(PrimitiveP(prim));
    FAssert(((ulong_t) prim) % OBJECT_ALIGNMENT == 0);
    FAssert(AsObjHdr(prim)->Generation() == OBJHDR_GEN_ETERNAL);
    FAssert(AsObjHdr(prim)->SlotCount() == 1);
    FAssert(AsObjHdr(prim)->ObjectSize() >= sizeof(FPrimitive));

    LibraryExport(lib, EnvironmentSet(env, InternSymbol(AsPrimitive(prim)->Name), prim));
}

static void WritePrimitive(FWriteContext * wctx, FObject obj)
{
    FAssert(SymbolP(AsPrimitive(obj)->Name));
    FAssert(CStringP(AsSymbol(AsPrimitive(obj)->Name)->String));

    wctx->WriteStringC("#<primitive: ");
    wctx->WriteStringC(AsCString(AsSymbol(AsPrimitive(obj)->Name)->String)->String);
    wctx->WriteCh(' ');

    const char * fn = AsPrimitive(obj)->Filename;
    const char * p = fn;
    while (*p != 0)
    {
        if (*p == '/' || *p == '\\')
            fn = p + 1;

        p += 1;
    }

    wctx->WriteStringC(fn);
    wctx->WriteCh('@');
    FCh s[16];
    long_t sl = FixnumAsString(AsPrimitive(obj)->LineNumber, s, 10);
    wctx->WriteString(s, sl);
    wctx->WriteCh('>');
}

// Foment specific

Define("loaded-libraries", LoadedLibrariesPrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("loaded-libraries", argc);

    return(LoadedLibraries);
}

Define("library-path", LibraryPathPrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("library-path", argc);

    return(LibraryPath);
}

Define("random", RandomPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("random", argc);
    NonNegativeArgCheck("random", argv[0], 0);

    return(MakeFixnum(rand() % AsFixnum(argv[0])));
}

Define("no-value", NoValuePrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("no-value", argc);

    return(NoValueObject);
}

// ---- SRFI 112: Environment Inquiry ----

Define("implementation-name", ImplementationNamePrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("implementation-name", argc);

    return(MakeStringC("foment"));
}

Define("implementation-version", ImplementationVersionPrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("implementation-version", argc);

    return(MakeStringC(FOMENT_VERSION));
}

static const char * CPUArchitecture()
{
#ifdef FOMENT_WINDOWS
    SYSTEM_INFO si;

    GetSystemInfo(&si);

    if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
        return("x86-64");
    else if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM)
        return("arm");
    else if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
        return("i686");

    return("unknown-cpu");
/*
#ifdef FOMENT_32BIT
    return("i386");
#endif // FOMENT_32BIT
#ifdef FOMENT_64BIT
    return("x86-64");
#endif // FOMENT_64BIT
*/
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    return(utsname.machine);
#endif // FOMENT_UNIX
}

Define("cpu-architecture", CPUArchitecturePrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("cpu-architecture", argc);

    return(MakeStringC(CPUArchitecture()));
}

Define("machine-name", MachineNamePrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("machine-name", argc);

#ifdef FOMENT_WINDOWS
    DWORD sz = 0;
    GetComputerNameExW(ComputerNameDnsHostname, NULL, &sz);

    FAssert(sz > 0);

    FObject b = MakeBytevector(sz * sizeof(FCh16));
    GetComputerNameExW(ComputerNameDnsHostname, (FCh16 *) AsBytevector(b)->Vector, &sz);

    return(ConvertUtf16ToString((FCh16 *) AsBytevector(b)->Vector, sz));
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    return(MakeStringC(utsname.nodename));
#endif // FOMENT_UNIX
}

static const char * OSName()
{
#ifdef FOMENT_WINDOWS
    return("windows");
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    return(utsname.sysname);
#endif // FOMENT_UNIX
}

Define("os-name", OSNamePrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("os-name", argc);

    return(MakeStringC(OSName()));
}

Define("os-version", OSVersionPrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("os-version", argc);

#ifdef FOMENT_WINDOWS
    OSVERSIONINFOEXA ovi;
    ovi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXA);
    GetVersionExA((LPOSVERSIONINFOA) &ovi);

    if (ovi.dwMajorVersion == 5)
    {
        if (ovi.dwMinorVersion == 0)
            return(MakeStringC("2000"));
        else if (ovi.dwMinorVersion == 1)
        {
            if (ovi.wServicePackMajor > 0)
            {
                char buf[128];
                sprintf(buf, "xp service pack %d", ovi.wServicePackMajor);
                return(MakeStringC(buf));
            }

            return(MakeStringC("xp"));
        }
        else if (ovi.dwMinorVersion == 2)
            return(MakeStringC("server 2003"));
    }
    else if (ovi.dwMajorVersion == 6)
    {
        if (ovi.dwMinorVersion == 0)
        {
            if (ovi.wServicePackMajor > 0)
            {
                char buf[128];
                sprintf(buf, "vista service pack %d", ovi.wServicePackMajor);
                return(MakeStringC(buf));
            }

            return(MakeStringC("vista"));
        }
        else if (ovi.dwMinorVersion == 1)
        {
            if (ovi.wServicePackMajor > 0)
            {
                char buf[128];
                sprintf(buf, "7 service pack %d", ovi.wServicePackMajor);
                return(MakeStringC(buf));
            }

            return(MakeStringC("7"));
        }
        else if (ovi.dwMinorVersion == 2)
            return(MakeStringC("8"));
        else if (ovi.dwMinorVersion == 3)
            return(MakeStringC("8.1"));
    }

    return(FalseObject);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    return(MakeStringC(utsname.release));
#endif // FOMENT_UNIX
}

// ---- SRFI 111: Boxes ----

Define("box", BoxPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("box", argc);

    return(MakeBox(argv[0]));
}

Define("box?", BoxPPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("box?", argc);

    return(BoxP(argv[0]) ? TrueObject : FalseObject);
}

Define("unbox", UnboxPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("unbox", argc);
    BoxArgCheck("unbox", argv[0]);

    return(Unbox(argv[0]));
}

Define("set-box!", SetBoxPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("set-box!", argc);
    BoxArgCheck("set-box!", argv[0]);

    SetBox(argv[0], argv[1]);
    return(NoValueObject);
}

// ---- Type tags ----

#define MISCELLANEOUS_TAG_OFFSET 8
#define INDIRECT_TAG_OFFSET 10

static long_t ObjectTypeTag(FObject obj)
{
    if (ObjectP(obj))
    {
        uint32_t tag = AsObjHdr(obj)->Tag();

        FAssert(tag >= 1 && tag < FreeTag);

        return(tag + INDIRECT_TAG_OFFSET);
    }
    else if (ImmediateTag(obj) == MiscellaneousTag)
    {
        if (AsValue(obj) < INDIRECT_TAG_OFFSET - MISCELLANEOUS_TAG_OFFSET)
            return(AsValue(obj) + MISCELLANEOUS_TAG_OFFSET);
    }
    else
        return(ImmediateTag(obj));

    return(-1);
}

FObject CharPPrimitiveFn(long_t argc, FObject argv[]);
FObject NullPPrimitiveFn(long_t argc, FObject argv[]);
FObject BooleanPPrimitiveFn(long_t argc, FObject argv[]);
FObject EofObjectPPrimitiveFn(long_t argc, FObject argv[]);
FObject NumberPPrimitiveFn(long_t argc, FObject argv[]);
FObject BoxPPrimitiveFn(long_t argc, FObject argv[]);
FObject PairPPrimitiveFn(long_t argc, FObject argv[]);
FObject StringPPrimitiveFn(long_t argc, FObject argv[]);
FObject VectorPPrimitiveFn(long_t argc, FObject argv[]);
FObject BytevectorPPrimitiveFn(long_t argc, FObject argv[]);
FObject BinaryPortPPrimitiveFn(long_t argc, FObject argv[]);
FObject TextualPortPPrimitiveFn(long_t argc, FObject argv[]);
FObject ProcedurePPrimitiveFn(long_t argc, FObject argv[]);
FObject SymbolPPrimitiveFn(long_t argc, FObject argv[]);
FObject ThreadPPrimitiveFn(long_t argc, FObject argv[]);
FObject ExclusivePPrimitiveFn(long_t argc, FObject argv[]);
FObject ConditionPPrimitiveFn(long_t argc, FObject argv[]);
FObject EphemeronPPrimitiveFn(long_t argc, FObject argv[]);

static FObject LookupTypeTags(FObject ttp)
{
    if (PrimitiveP(ttp))
    {
        if (AsPrimitive(ttp)->PrimitiveFn == CharPPrimitiveFn)
            return(List(MakeFixnum(CharacterTag)));
        else if (AsPrimitive(ttp)->PrimitiveFn == NullPPrimitiveFn)
            return(List(MakeFixnum(AsValue(EmptyListObject) + MISCELLANEOUS_TAG_OFFSET)));
        else if (AsPrimitive(ttp)->PrimitiveFn == EofObjectPPrimitiveFn)
            return(List(MakeFixnum(AsValue(EndOfFileObject) + MISCELLANEOUS_TAG_OFFSET)));
        else if (AsPrimitive(ttp)->PrimitiveFn == BooleanPPrimitiveFn)
            return(List(MakeFixnum(BooleanTag)));
        else if (AsPrimitive(ttp)->PrimitiveFn == NumberPPrimitiveFn)
            return(List(MakeFixnum(FixnumTag), MakeFixnum(BignumTag + INDIRECT_TAG_OFFSET),
                    MakeFixnum(RatioTag + INDIRECT_TAG_OFFSET),
                    MakeFixnum(ComplexTag + INDIRECT_TAG_OFFSET),
                    MakeFixnum(FlonumTag + INDIRECT_TAG_OFFSET)));
        else if (AsPrimitive(ttp)->PrimitiveFn == BoxPPrimitiveFn)
            return(List(MakeFixnum(BoxTag + INDIRECT_TAG_OFFSET)));
        else if (AsPrimitive(ttp)->PrimitiveFn == PairPPrimitiveFn)
            return(List(MakeFixnum(PairTag + INDIRECT_TAG_OFFSET)));
        else if (AsPrimitive(ttp)->PrimitiveFn == StringPPrimitiveFn)
            return(List(MakeFixnum(StringTag + INDIRECT_TAG_OFFSET)));
        else if (AsPrimitive(ttp)->PrimitiveFn == VectorPPrimitiveFn)
            return(List(MakeFixnum(VectorTag + INDIRECT_TAG_OFFSET)));
        else if (AsPrimitive(ttp)->PrimitiveFn == BytevectorPPrimitiveFn)
            return(List(MakeFixnum(BytevectorTag + INDIRECT_TAG_OFFSET)));
        else if (AsPrimitive(ttp)->PrimitiveFn == BinaryPortPPrimitiveFn)
            return(List(MakeFixnum(BinaryPortTag + INDIRECT_TAG_OFFSET)));
        else if (AsPrimitive(ttp)->PrimitiveFn == TextualPortPPrimitiveFn)
            return(List(MakeFixnum(TextualPortTag + INDIRECT_TAG_OFFSET)));
        else if (AsPrimitive(ttp)->PrimitiveFn == ProcedurePPrimitiveFn)
            return(List(MakeFixnum(ProcedureTag + INDIRECT_TAG_OFFSET),
                    MakeFixnum(PrimitiveTag + INDIRECT_TAG_OFFSET)));
        else if (AsPrimitive(ttp)->PrimitiveFn == SymbolPPrimitiveFn)
            return(List(MakeFixnum(SymbolTag + INDIRECT_TAG_OFFSET)));
        else if (AsPrimitive(ttp)->PrimitiveFn == ThreadPPrimitiveFn)
            return(List(MakeFixnum(ThreadTag + INDIRECT_TAG_OFFSET)));
        else if (AsPrimitive(ttp)->PrimitiveFn == ExclusivePPrimitiveFn)
            return(List(MakeFixnum(ExclusiveTag + INDIRECT_TAG_OFFSET)));
        else if (AsPrimitive(ttp)->PrimitiveFn == ConditionPPrimitiveFn)
            return(List(MakeFixnum(ConditionTag + INDIRECT_TAG_OFFSET)));
        else if (AsPrimitive(ttp)->PrimitiveFn == EphemeronPPrimitiveFn)
            return(List(MakeFixnum(EphemeronTag + INDIRECT_TAG_OFFSET)));
    }

    return(EmptyListObject);
}

Define("object-type-tag", ObjectTypeTagPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("object-type-tag", argc);

    return(MakeFixnum(ObjectTypeTag(argv[0])));
}

Define("lookup-type-tags", LookupTypeTagsPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("lookup-type-tags", argc);

    FObject tlst = LookupTypeTags(argv[0]);
    if (tlst == EmptyListObject)
        RaiseExceptionC(Assertion, "lookup-type-tags", "expected a predicate", List(argv[0]));

    return(tlst);
}

// ---- Primitives ----

static FObject Primitives[] =
{
    NotPrimitive,
    BooleanPPrimitive,
    BooleanEqualPPrimitive,
    SymbolPPrimitive,
    SymbolEqualPPrimitive,
    SymbolToStringPrimitive,
    StringToSymbolPrimitive,
    RaisePrimitive,
    ErrorPrimitive,
    ErrorObjectPPrimitive,
    ErrorObjectTypePrimitive,
    ErrorObjectWhoPrimitive,
    ErrorObjectKindPrimitive,
    ErrorObjectMessagePrimitive,
    ErrorObjectIrritantsPrimitive,
    FullErrorPrimitive,
    CommandLinePrimitive,
    FullCommandLinePrimitive,
    GetEnvironmentVariablePrimitive,
    GetEnvironmentVariablesPrimitive,
    CurrentSecondPrimitive,
    CurrentJiffyPrimitive,
    JiffiesPerSecondPrimitive,
    FeaturesPrimitive,
    SetFeaturesPrimitive,
    MakeRecordTypePrimitive,
    MakeRecordPrimitive,
    RecordPredicatePrimitive,
    RecordIndexPrimitive,
    RecordRefPrimitive,
    RecordSetPrimitive,
    LoadedLibrariesPrimitive,
    LibraryPathPrimitive,
    RandomPrimitive,
    NoValuePrimitive,
    ImplementationNamePrimitive,
    ImplementationVersionPrimitive,
    CPUArchitecturePrimitive,
    MachineNamePrimitive,
    OSNamePrimitive,
    OSVersionPrimitive,
    BoxPrimitive,
    BoxPPrimitive,
    UnboxPrimitive,
    SetBoxPrimitive,
    ObjectTypeTagPrimitive,
    LookupTypeTagsPrimitive
};

// ----------------

// From base.cpp which is generated from base.scm
extern char FomentBase[];
extern char FomentLibraryNames[];
extern char * FomentLibraries[];

static void SetupScheme()
{
    FObject port = MakeStringCInputPort(
        "(define-syntax and"
            "(syntax-rules ()"
                "((and) #t)"
                "((and test) test)"
                "((and test1 test2 ...) (if test1 (and test2 ...) #f))))");
    WantIdentifiersPort(port, 1);
    Eval(Read(port), Bedrock);

    LibraryExport(BedrockLibrary, EnvironmentLookup(Bedrock, StringCToSymbol("and")));

    port = MakeStringCInputPort(FomentLibraryNames);
    FomentLibrariesVector = Read(port);

    FAssert(VectorP(FomentLibrariesVector));

    port = MakeStringCInputPort(FomentBase);
    WantIdentifiersPort(port, 1);
    FAlive ap(&port);

    for (;;)
    {
        FObject obj = Read(port);

        if (obj == EndOfFileObject)
            break;
        Eval(obj, Bedrock);
    }
}

FObject OpenFomentLibrary(FObject nam)
{
    FAssert(VectorP(FomentLibrariesVector));

    for (ulong_t idx = 0; idx < VectorLength(FomentLibrariesVector); idx++)
        if (EqualP(nam, AsVector(FomentLibrariesVector)->Vector[idx]))
            return(MakeStringCInputPort(FomentLibraries[idx]));

    return(NoValueObject);
}

static const char * FeaturesC[] =
{
#ifdef FOMENT_UNIX
    "unix",
#endif // FOMENT_UNIX

    FOMENT_MEMORYMODEL,
    "r7rs",
    "exact-closed",
    "exact-complex",
    "ieee-float",
    "full-unicode",
    "ratios",
    "threads",
    "foment",
    "foment-" FOMENT_VERSION
};

int LittleEndianP()
{
    ulong_t nd = 1;

    return(*((char *) &nd) == 1);
}

#ifdef FOMENT_UNIX
static void FixupUName(char * s)
{
    while (*s != 0)
    {
        if (*s == '/' || *s == '_')
            *s = '-';
        else
            *s = tolower(*s);

        s += 1;
    }
}
#endif // FOMENT_UNIX

// ---- Indirect Object Types ----

FIndirectType IndirectTypes[] =
{
    {0, 0},
    {"bignum", WriteNumber},
    {"ratio", WriteNumber},
    {"complex", WriteNumber},
    {"flonum", WriteNumber},
    {"box", WriteBox},
    {"pair", WritePair},
    {"string", WriteStringObject},
    {"string-c", WriteCStringObject},
    {"vector", WriteVector},
    {"bytevector", WriteBytevector},
    {"binary-port", WritePortObject},
    {"textual-port", WritePortObject},
    {"procedure", WriteProcedure},
    {"symbol", WriteSymbol},
    {"identifier", WriteIdentifier},
    {"record-type", WriteRecordType},
    {"record", WriteRecord},
    {"primitive", WritePrimitive},
    {"thread", WriteThread},
    {"exclusive", WriteExclusive},
    {"condition", WriteCondition},
    {"hash-node", WriteHashNode},
    {"hash-table", WriteHashTable},
    {"ephemeron", WriteEphemeron},
    {"builtin-type", WriteBuiltinType},
    {"builtin", WriteBuiltin},
    {"free", 0}
};

long_t SetupFoment(FThreadState * ts)
{
#ifdef FOMENT_WINDOWS
    StartingTicks = GetTickCount64();
#endif // FOMENT_WINDOWS
#ifdef FOMENT_UNIX
    struct timeval tv;
    struct timezone tz;

    gettimeofday(&tv, &tz);
    StartingSecond = tv.tv_sec;

    uname(&utsname);
    FixupUName(utsname.machine);
    FixupUName(utsname.sysname);
#endif // FOMENT_UNIX

    if (RandomSeed == 0)
        RandomSeed = (unsigned int) time(0);
    srand((unsigned int) RandomSeed);

    if (SetupCore(ts) == 0)
        return(0);

    // Likely a new indirect tag was added, but a corresponding entry is
    // missing from IndirectTypes just about this procedure in this file.
    FAssert(sizeof(IndirectTypes) / sizeof(FIndirectType) == BadDogTag);
    FAssert(FreeTag + 1 == BadDogTag);
    FAssert(strcmp(IndirectTypes[FreeTag].Name, "free") == 0);
    FAssert(IndirectTypes[FreeTag].Write == 0);

    RegisterRoot(&SymbolHashTable, "symbol-hash-table");
    RegisterRoot(&Bedrock, "bedrock");
    RegisterRoot(&BedrockLibrary, "bedrock-library");
    RegisterRoot(&LoadedLibraries, "loaded-libraries");
    RegisterRoot(&Features, "features");
    RegisterRoot(&LibraryPath, "library-path");
    RegisterRoot(&LibraryExtensions, "library-extensions");
    RegisterRoot(&FomentLibrariesVector, "foment-libraries-vector");

    SymbolHashTable = MakeStringHashTable(4096, HASH_TABLE_THREAD_SAFE);

    ts->Parameters = MakeEqHashTable(32, 0);

    SetupLibrary();

    FObject nam = List(StringCToSymbol("foment"), StringCToSymbol("bedrock"));
    Bedrock = MakeEnvironment(nam, FalseObject);
    BedrockLibrary = MakeLibrary(nam);

    for (ulong_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(Bedrock, BedrockLibrary, Primitives[idx]);

    BeginSymbol = InternSymbol(BeginSymbol);
    QuoteSymbol = InternSymbol(QuoteSymbol);
    QuasiquoteSymbol = InternSymbol(QuasiquoteSymbol);
    UnquoteSymbol = InternSymbol(UnquoteSymbol);
    UnquoteSplicingSymbol = InternSymbol(UnquoteSplicingSymbol);
    Assertion = InternSymbol(Assertion);
    Restriction = InternSymbol(Restriction);
    Lexical = InternSymbol(Lexical);
    Syntax = InternSymbol(Syntax);
    Error = InternSymbol(Error);
    MakeObjectSymbol = InternSymbol(MakeObjectSymbol);
    StartThreadSymbol = InternSymbol(StartThreadSymbol);
    ExecuteSymbol = InternSymbol(ExecuteSymbol);

    FAssert(BeginSymbol == StringCToSymbol("begin"));
    FAssert(QuoteSymbol == StringCToSymbol("quote"));
    FAssert(QuasiquoteSymbol == StringCToSymbol("quasiquote"));
    FAssert(UnquoteSymbol == StringCToSymbol("unquote"));
    FAssert(UnquoteSplicingSymbol == StringCToSymbol("unquote-splicing"));
    FAssert(Assertion == StringCToSymbol("assertion-violation"));
    FAssert(Restriction == StringCToSymbol("implementation-restriction"));
    FAssert(Lexical == StringCToSymbol("lexical-violation"));
    FAssert(Syntax == StringCToSymbol("syntax-violation"));
    FAssert(Error == StringCToSymbol("error-violation"));
    FAssert(MakeObjectSymbol == StringCToSymbol("%make-object"));
    FAssert(CollectSymbol = StringCToSymbol("%collect"));
    FAssert(StartThreadSymbol == StringCToSymbol("%start-thread"));
    FAssert(ExecuteSymbol == StringCToSymbol("%execute"));

    MakeObjectOutOfMemory = MakeException(Assertion, MakeObjectSymbol, NoValueObject,
            MakeStringC("%make-object: out of memory"), EmptyListObject);
    CollectOutOfMemory = MakeException(Assertion, CollectSymbol, NoValueObject,
            MakeStringC("%collect: out of memory"), EmptyListObject);
    StartThreadOutOfMemory = MakeException(Assertion, StartThreadSymbol, NoValueObject,
            MakeStringC("%start-thread: out of memory"), EmptyListObject);
    ExecuteStackOverflow =  MakeException(Assertion, ExecuteSymbol, NoValueObject,
            MakeStringC("%execute: stack overflow"), EmptyListObject);

    for (ulong_t n = 0; n < sizeof(SpecialSyntaxes) / sizeof(char *); n++)
        LibraryExport(BedrockLibrary, EnvironmentSetC(Bedrock, SpecialSyntaxes[n],
                MakeImmediate(n, SpecialSyntaxTag)));

    SetupHashTables();
    SetupCompare();
    SetupPairs();
    SetupCharacters();
    SetupStrings();
    SetupVectors();
    SetupIO();
    SetupFileSys();
    SetupCompile();
    SetupExecute();
    SetupNumbers();
    SetupThreads();
    SetupGC();
    SetupMain();

    LibraryExport(BedrockLibrary,
            EnvironmentSetC(Bedrock, "%standard-input", StandardInput));
    LibraryExport(BedrockLibrary,
            EnvironmentSetC(Bedrock, "%standard-output", StandardOutput));
    LibraryExport(BedrockLibrary,
            EnvironmentSetC(Bedrock, "%standard-error", StandardError));

#ifdef FOMENT_DEBUG
    LibraryExport(BedrockLibrary,
            EnvironmentSetC(Bedrock, "%debug-build", TrueObject));
#else // FOMENT_DEBUG
    LibraryExport(BedrockLibrary,
            EnvironmentSetC(Bedrock, "%debug-build", FalseObject));
#endif // FOMENT_DEBUG

    for (ulong_t idx = 0; idx < sizeof(FeaturesC) / sizeof(char *); idx++)
        Features = MakePair(StringCToSymbol(FeaturesC[idx]), Features);

    Features = MakePair(StringCToSymbol(CPUArchitecture()), Features);
    Features = MakePair(StringCToSymbol(OSName()), Features);
    Features = MakePair(StringCToSymbol(LittleEndianP() ? "little-endian" : "big-endian"),
            Features);
    if (CollectorType == MarkSweepCollector || CollectorType == GenerationalCollector)
        Features = MakePair(StringCToSymbol("guardians"), Features);
    if (CollectorType == GenerationalCollector)
        Features = MakePair(StringCToSymbol("trackers"), Features);

    GetEnvironmentVariables();

    FObject lp = Assoc(MakeStringC("FOMENT_LIBPATH"), EnvironmentVariables);
    if (PairP(lp))
    {
        FAssert(StringP(First(lp)));

        lp = Rest(lp);

        ulong_t strt = 0;
        ulong_t idx = 0;
        while (idx < StringLength(lp))
        {
            if (AsString(lp)->String[idx] == PathSep)
            {
                if (idx > strt)
                    LibraryPath = MakePair(
                            MakeString(AsString(lp)->String + strt, idx - strt), LibraryPath);

                idx += 1;
                strt = idx;
            }

            idx += 1;
        }

        if (idx > strt)
            LibraryPath = MakePair(
                    MakeString(AsString(lp)->String + strt, idx - strt), LibraryPath);
    }

    LibraryExtensions = List(MakeStringC("sld"), MakeStringC("scm"));

    if (CheckHeapFlag)
        CheckHeap(__FILE__, __LINE__);
    SetupScheme();
    if (CheckHeapFlag)
        CheckHeap(__FILE__, __LINE__);

    SetupComplete = 1;
    return(1);
}
