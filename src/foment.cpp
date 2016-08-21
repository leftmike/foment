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
#endif // FOMENT_UNIX

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "foment.hpp"
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

uint_t SetupComplete = 0;
uint_t RandomSeed = 0;

uint_t CheckHeapFlag = 0;
uint_t VerboseFlag = 0;

FRoots R;

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

void ErrorExitFoment()
{
    if (SetupComplete)
    {
        if (CheckHeapFlag || VerboseFlag)
            printf("RandomSeed: " UINT_FMT "\n", RandomSeed);
        ExitFoment();
    }

    exit(1);
}

void FAssertFailed(const char * fn, int_t ln, const char * expr)
{
    printf("FAssert: %s (%d)%s\n", expr, (int) ln, fn);
    ErrorExitFoment();
}

void FMustBeFailed(const char * fn, int_t ln, const char * expr)
{
    printf("FMustBe: %s (%d)%s\n", expr, (int) ln, fn);
    ErrorExitFoment();
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

    int_t n = AsValue(obj);
    FAssert(n >= 0);
    FAssert(n < (int_t) (sizeof(SpecialSyntaxes) / sizeof(char *)));

    return(SpecialSyntaxes[n]);
}

void WriteSpecialSyntax(FObject port, FObject obj, int_t df)
{
    const char * n = SpecialSyntaxToName(obj);

    WriteStringC(port, "#<syntax: ");
    WriteStringC(port, n);
    WriteCh(port, '>');
}

// ---- Booleans ----

Define("not", NotPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("not", argc);

    return(argv[0] == FalseObject ? TrueObject : FalseObject);
}

Define("boolean?", BooleanPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("boolean?", argc);

    return(BooleanP(argv[0]) ? TrueObject : FalseObject);
}

Define("boolean=?", BooleanEqualPPrimitive)(int_t argc, FObject argv[])
{
    AtLeastTwoArgsCheck("boolean=?", argc);
    BooleanArgCheck("boolean=?", argv[0]);

    for (int_t adx = 1; adx < argc; adx++)
    {
        BooleanArgCheck("boolean=?", argv[adx]);

        if (argv[adx - 1] != argv[adx])
            return(FalseObject);
    }

    return(TrueObject);
}

Define("boolean-compare", BooleanComparePrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("boolean-compare", argc);
    BooleanArgCheck("boolean-compare", argv[0]);
    BooleanArgCheck("boolean-compare", argv[1]);

    if (argv[0] == argv[1])
        return(MakeFixnum(0));
    return(argv[0] < argv[1] ? MakeFixnum(-1) : MakeFixnum(1));
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
    uint_t sdx;
    for (sdx = 0; sdx < StringLength(str); sdx++)
        AsString(nstr)->String[sdx] = AsString(str)->String[sdx];

    for (uint_t idx = 0; idx < StringLength(sstr); idx++)
        AsString(nstr)->String[sdx + idx] = AsString(sstr)->String[idx];

    return(StringToSymbol(nstr));
}

Define("symbol?", SymbolPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("symbol?", argc);

    return(SymbolP(argv[0]) ? TrueObject : FalseObject);
}

Define("symbol=?", SymbolEqualPPrimitive)(int_t argc, FObject argv[])
{
    AtLeastTwoArgsCheck("symbol=?", argc);
    SymbolArgCheck("symbol=?", argv[0]);

    for (int_t adx = 1; adx < argc; adx++)
    {
        SymbolArgCheck("symbol=?", argv[adx]);

        if (argv[adx - 1] != argv[adx])
            return(FalseObject);
    }

    return(TrueObject);
}

Define("symbol->string", SymbolToStringPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("symbol->string", argc);
    SymbolArgCheck("symbol->string", argv[0]);

    return(SymbolToString(argv[0]));
}

Define("string->symbol", StringToSymbolPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("string->symbol", argc);
    StringArgCheck("string->symbol", argv[0]);

    return(StringToSymbol(argv[0]));
}

// ---- Exceptions ----

static const char * ExceptionFieldsC[] = {"type", "who", "kind", "message", "irritants"};

FObject MakeException(FObject typ, FObject who, FObject knd, FObject msg, FObject lst)
{
    FAssert(sizeof(FException) == sizeof(ExceptionFieldsC) + sizeof(FRecord));

    FException * exc = (FException *) MakeRecord(R.ExceptionRecordType);
    exc->Type = typ;
    exc->Who = who;
    exc->Kind = knd;
    exc->Message = msg;
    exc->Irritants = lst;

    return(exc);
}

void RaiseException(FObject typ, FObject who, FObject knd, FObject msg, FObject lst)
{
    Raise(MakeException(typ, who, knd, msg, lst));
}

void RaiseExceptionC(FObject typ, const char * who, FObject knd, const char * msg, FObject lst)
{
    char buf[128];

    FAssert(strlen(who) + strlen(msg) + 3 < sizeof(buf));

    if (strlen(who) + strlen(msg) + 3 >= sizeof(buf))
        Raise(MakeException(typ, StringCToSymbol(who), knd, MakeStringC(msg), lst));
    else
    {
        strcpy(buf, who);
        strcat(buf, ": ");
        strcat(buf, msg);

        Raise(MakeException(typ, StringCToSymbol(who), knd, MakeStringC(buf), lst));
    }
}

void Raise(FObject obj)
{
    throw obj;
}

Define("raise", RaisePrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("raise", argc);

    Raise(argv[0]);
    return(NoValueObject);
}

Define("error", ErrorPrimitive)(int_t argc, FObject argv[])
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

Define("error-object?", ErrorObjectPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("error-object?", argc);

    return(ExceptionP(argv[0]) ? TrueObject : FalseObject);
}

Define("error-object-type", ErrorObjectTypePrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("error-object-type", argc);
    ExceptionArgCheck("error-object-type", argv[0]);

    return(AsException(argv[0])->Type);
}

Define("error-object-who", ErrorObjectWhoPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("error-object-who", argc);
    ExceptionArgCheck("error-object-who", argv[0]);

    return(AsException(argv[0])->Who);
}

Define("error-object-kind", ErrorObjectKindPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("error-object-kind", argc);
    ExceptionArgCheck("error-object-kind", argv[0]);

    return(AsException(argv[0])->Kind);
}

Define("error-object-message", ErrorObjectMessagePrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("error-object-message", argc);
    ExceptionArgCheck("error-object-message", argv[0]);

    return(AsException(argv[0])->Message);
}

Define("error-object-irritants", ErrorObjectIrritantsPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("error-object-irritants", argc);
    ExceptionArgCheck("error-object-irritants", argv[0]);

    return(AsException(argv[0])->Irritants);
}

Define("full-error", FullErrorPrimitive)(int_t argc, FObject argv[])
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

Define("command-line", CommandLinePrimitive)(int_t argc, FObject argv[])
{
    ZeroArgsCheck("command-line", argc);

    return(R.CommandLine);
}

Define("full-command-line", FullCommandLinePrimitive)(int_t argc, FObject argv[])
{
    ZeroArgsCheck("full-command-line", argc);

    return(R.FullCommandLine);
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

    R.EnvironmentVariables = lst;
}

Define("get-environment-variable", GetEnvironmentVariablePrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("get-environment-variable", argc);
    StringArgCheck("get-environment-variable", argv[0]);

    FAssert(PairP(R.EnvironmentVariables));

    FObject ret = Assoc(argv[0], R.EnvironmentVariables);
    if (PairP(ret))
        return(Rest(ret));
    return(ret);
}

Define("get-environment-variables", GetEnvironmentVariablesPrimitive)(int_t argc, FObject argv[])
{
    ZeroArgsCheck("get-environment-variables", argc);

    FAssert(PairP(R.EnvironmentVariables));

    return(R.EnvironmentVariables);
}

Define("current-second", CurrentSecondPrimitive)(int_t argc, FObject argv[])
{
    ZeroArgsCheck("current-second", argc);

    time_t t = time(0);

    return(MakeFlonum((double64_t) t));
}

Define("current-jiffy", CurrentJiffyPrimitive)(int_t argc, FObject argv[])
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

Define("jiffies-per-second", JiffiesPerSecondPrimitive)(int_t argc, FObject argv[])
{
    ZeroArgsCheck("jiffies-per-second", argc);

    return(MakeFixnum(1000));
}

Define("features", FeaturesPrimitive)(int_t argc, FObject argv[])
{
    ZeroArgsCheck("features", argc);

    return(R.Features);
}

Define("%set-features!", SetFeaturesPrimitive)(int_t argc, FObject argv[])
{
    FMustBe(argc == 1);

    R.Features = argv[0];
    return(NoValueObject);
}

// ---- Boxes ----

FObject MakeBox(FObject val)
{
    return(MakeBox(val, 0));
}

FObject MakeBox(FObject val, uint_t idx)
{
    FBox * bx = (FBox *) MakeObject(BoxTag, sizeof(FBox), 1, "box");
    bx->Value = val;
    bx->Index = idx;

    return(bx);
}

// ---- Record Types ----

FObject MakeRecordType(FObject nam, uint_t nf, FObject flds[])
{
    FAssert(SymbolP(nam));

    FRecordType * rt = (FRecordType *) MakeObject(RecordTypeTag,
            sizeof(FRecordType) + sizeof(FObject) * nf, nf + 1, "%make-record-type");
    rt->Fields[0] = nam;

    for (uint_t fdx = 1; fdx <= nf; fdx++)
    {
        FAssert(SymbolP(flds[fdx - 1]));

        rt->Fields[fdx] = flds[fdx - 1];
    }

    return(rt);
}

FObject MakeRecordTypeC(const char * nam, uint_t nf, const char * flds[])
{
    FObject oflds[32];

    FAssert(nf <= sizeof(oflds) / sizeof(FObject));

    for (uint_t fdx = 0; fdx < nf; fdx++)
        oflds[fdx] = StringCToSymbol(flds[fdx]);

    return(MakeRecordType(StringCToSymbol(nam), nf, oflds));
}

Define("%make-record-type", MakeRecordTypePrimitive)(int_t argc, FObject argv[])
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

Define("%make-record", MakeRecordPrimitive)(int_t argc, FObject argv[])
{
    // (%make-record <record-type>)

    FMustBe(argc == 1);
    FMustBe(RecordTypeP(argv[0]));

    return(MakeRecord(argv[0]));
}

Define("%record-predicate", RecordPredicatePrimitive)(int_t argc, FObject argv[])
{
    // (%record-predicate <record-type> <obj>)

    FMustBe(argc == 2);
    FMustBe(RecordTypeP(argv[0]));

    return(RecordP(argv[1], argv[0]) ? TrueObject : FalseObject);
}

Define("%record-index", RecordIndexPrimitive)(int_t argc, FObject argv[])
{
    // (%record-index <record-type> <field-name>)

    FMustBe(argc == 2);
    FMustBe(RecordTypeP(argv[0]));

    for (uint_t rdx = 1; rdx < RecordTypeNumFields(argv[0]); rdx++)
        if (EqP(argv[1], AsRecordType(argv[0])->Fields[rdx]))
            return(MakeFixnum(rdx));

    RaiseExceptionC(Assertion, "define-record-type", "expected a field-name",
            List(argv[1], argv[0]));

    return(NoValueObject);
}

Define("%record-ref", RecordRefPrimitive)(int_t argc, FObject argv[])
{
    // (%record-ref <record-type> <obj> <index>)

    FMustBe(argc == 3);
    FMustBe(RecordTypeP(argv[0]));

    if (RecordP(argv[1], argv[0]) == 0)
        RaiseExceptionC(Assertion, "%record-ref", "not a record of the expected type",
                List(argv[1], argv[0]));

    FMustBe(FixnumP(argv[2]));
    FMustBe(AsFixnum(argv[2]) > 0 && AsFixnum(argv[2]) < (int_t) RecordNumFields(argv[1]));

    return(AsGenericRecord(argv[1])->Fields[AsFixnum(argv[2])]);
}

Define("%record-set!", RecordSetPrimitive)(int_t argc, FObject argv[])
{
    // (%record-set! <record-type> <obj> <index> <value>)

    FMustBe(argc == 4);
    FMustBe(RecordTypeP(argv[0]));

    if (RecordP(argv[1], argv[0]) == 0)
        RaiseExceptionC(Assertion, "%record-set!", "not a record of the expected type",
                List(argv[1], argv[0]));

    FMustBe(FixnumP(argv[2]));
    FMustBe(AsFixnum(argv[2]) > 0 && AsFixnum(argv[2]) < (int_t) RecordNumFields(argv[1]));

//    AsGenericRecord(argv[1])->Fields[AsFixnum(argv[2])] = argv[3];
    Modify(FGenericRecord, argv[1], Fields[AsFixnum(argv[2])], argv[3]);
    return(NoValueObject);
}

// ---- Records ----

FObject MakeRecord(FObject rt)
{
    FAssert(RecordTypeP(rt));

    uint_t nf = RecordTypeNumFields(rt);

    FGenericRecord * r = (FGenericRecord *) MakeObject(RecordTag,
            sizeof(FGenericRecord) + sizeof(FObject) * (nf - 1), nf, "%make-record");
    r->Fields[0] = rt;

    for (uint_t fdx = 1; fdx < nf; fdx++)
        r->Fields[fdx] = NoValueObject;

    return(r);
}

// ---- Primitives ----

void DefinePrimitive(FObject env, FObject lib, FObject prim)
{
    LibraryExport(lib, EnvironmentSet(env, InternSymbol(AsPrimitive(prim)->Name), prim));
}

// Foment specific

Define("loaded-libraries", LoadedLibrariesPrimitive)(int_t argc, FObject argv[])
{
    ZeroArgsCheck("loaded-libraries", argc);

    return(R.LoadedLibraries);
}

Define("library-path", LibraryPathPrimitive)(int_t argc, FObject argv[])
{
    ZeroArgsCheck("library-path", argc);

    return(R.LibraryPath);
}

Define("random", RandomPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("random", argc);
    NonNegativeArgCheck("random", argv[0], 0);

    return(MakeFixnum(rand() % AsFixnum(argv[0])));
}

Define("no-value", NoValuePrimitive)(int_t argc, FObject argv[])
{
    ZeroArgsCheck("no-value", argc);

    return(NoValueObject);
}

// ---- SRFI 112: Environment Inquiry ----

Define("implementation-name", ImplementationNamePrimitive)(int_t argc, FObject argv[])
{
    ZeroArgsCheck("implementation-name", argc);

    return(MakeStringC("foment"));
}

Define("implementation-version", ImplementationVersionPrimitive)(int_t argc, FObject argv[])
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

Define("cpu-architecture", CPUArchitecturePrimitive)(int_t argc, FObject argv[])
{
    ZeroArgsCheck("cpu-architecture", argc);

    return(MakeStringC(CPUArchitecture()));
}

Define("machine-name", MachineNamePrimitive)(int_t argc, FObject argv[])
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

Define("os-name", OSNamePrimitive)(int_t argc, FObject argv[])
{
    ZeroArgsCheck("os-name", argc);

    return(MakeStringC(OSName()));
}

Define("os-version", OSVersionPrimitive)(int_t argc, FObject argv[])
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

Define("box", BoxPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("box", argc);

    return(MakeBox(argv[0]));
}

Define("box?", BoxPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("box?", argc);

    return(BoxP(argv[0]) ? TrueObject : FalseObject);
}

Define("unbox", UnboxPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("unbox", argc);
    BoxArgCheck("unbox", argv[0]);

    return(Unbox(argv[0]));
}

Define("set-box!", SetBoxPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("set-box!", argc);
    BoxArgCheck("set-box!", argv[0]);

    SetBox(argv[0], argv[1]);
    return(NoValueObject);
}

// ---- Type maps ----

/*
(make-type-map)
(type-map-ref <type-map> <obj>)
(type-map-set! <type-map> <type-test> <value>)
*/

FObject MakeTypeMap()
{
    uint_t idx;
    FTypeMap * tmap = (FTypeMap *) MakeObject(TypeMapTag, sizeof(FTypeMap),
            sizeof(FTypeMap) / sizeof(FObject), "type-map");
    for (idx = 0; idx < TAG_MAP_SIZE; idx++)
        tmap->TagMap[idx] = NoValueObject;

    return(tmap);
}

FObject TypeMapRef(FObject tmap, FObject obj)
{
    FAssert(TypeMapP(tmap));

    if (ObjectP(obj))
    {
        uint32_t tag = AsObjHdr(obj)->Tag();

        FAssert(tag >= 1 && tag < FreeTag);

        return(AsTypeMap(tmap)->TagMap[tag + INDIRECT_TAG_OFFSET]);
    }
    else if (ImmediateTag(obj) == MiscellaneousTag)
    {
        if (AsValue(obj) < INDIRECT_TAG_OFFSET - MISCELLANEOUS_TAG_OFFSET)
            return(AsTypeMap(tmap)->TagMap[AsValue(obj) + MISCELLANEOUS_TAG_OFFSET]);
    }
    else
        return(AsTypeMap(tmap)->TagMap[ImmediateTag(obj)]);

    return(NoValueObject);
}

FObject CharPPrimitiveFn(int_t argc, FObject argv[]);
FObject NullPPrimitiveFn(int_t argc, FObject argv[]);
FObject BooleanPPrimitiveFn(int_t argc, FObject argv[]);
FObject EofObjectPPrimitiveFn(int_t argc, FObject argv[]);
FObject NumberPPrimitiveFn(int_t argc, FObject argv[]);
FObject BoxPPrimitiveFn(int_t argc, FObject argv[]);
FObject PairPPrimitiveFn(int_t argc, FObject argv[]);
FObject StringPPrimitiveFn(int_t argc, FObject argv[]);
FObject VectorPPrimitiveFn(int_t argc, FObject argv[]);
FObject BytevectorPPrimitiveFn(int_t argc, FObject argv[]);
FObject BinaryPortPPrimitiveFn(int_t argc, FObject argv[]);
FObject TextualPortPPrimitiveFn(int_t argc, FObject argv[]);
FObject ProcedurePPrimitiveFn(int_t argc, FObject argv[]);
FObject SymbolPPrimitiveFn(int_t argc, FObject argv[]);
FObject ThreadPPrimitiveFn(int_t argc, FObject argv[]);
FObject ExclusivePPrimitiveFn(int_t argc, FObject argv[]);
FObject ConditionPPrimitiveFn(int_t argc, FObject argv[]);
FObject EphemeronPPrimitiveFn(int_t argc, FObject argv[]);

int_t TypeMapSet(FObject tmap, FObject typep, FObject val)
{
    FAssert(TypeMapP(tmap));

    if (PrimitiveP(typep))
    {
        if (AsPrimitive(typep)->PrimitiveFn == CharPPrimitiveFn)
        {
            AsTypeMap(tmap)->TagMap[CharacterTag + INDIRECT_TAG_OFFSET] = val;
            return(1);
        }
        else if (AsPrimitive(typep)->PrimitiveFn == NullPPrimitiveFn)
        {
            AsTypeMap(tmap)->TagMap[AsValue(EmptyListObject) + MISCELLANEOUS_TAG_OFFSET] = val;
            return(1);
        }
        else if (AsPrimitive(typep)->PrimitiveFn == EofObjectPPrimitiveFn)
        {
            AsTypeMap(tmap)->TagMap[AsValue(EndOfFileObject) + MISCELLANEOUS_TAG_OFFSET] = val;
            return(1);
        }
        else if (AsPrimitive(typep)->PrimitiveFn == BooleanPPrimitiveFn)
        {
            AsTypeMap(tmap)->TagMap[BooleanTag] = val;
            return(1);
        }
        else if (AsPrimitive(typep)->PrimitiveFn == NumberPPrimitiveFn)
        {
            AsTypeMap(tmap)->TagMap[FixnumTag] = val;
            AsTypeMap(tmap)->TagMap[BignumTag + INDIRECT_TAG_OFFSET] = val;
            AsTypeMap(tmap)->TagMap[RatioTag + INDIRECT_TAG_OFFSET] = val;
            AsTypeMap(tmap)->TagMap[ComplexTag + INDIRECT_TAG_OFFSET] = val;
            AsTypeMap(tmap)->TagMap[FlonumTag + INDIRECT_TAG_OFFSET] = val;
            return(1);
        }
        else if (AsPrimitive(typep)->PrimitiveFn == BoxPPrimitiveFn)
        {
            AsTypeMap(tmap)->TagMap[BoxTag + INDIRECT_TAG_OFFSET] = val;
            return(1);
        }
        else if (AsPrimitive(typep)->PrimitiveFn == PairPPrimitiveFn)
        {
            AsTypeMap(tmap)->TagMap[PairTag + INDIRECT_TAG_OFFSET] = val;
            return(1);
        }
        else if (AsPrimitive(typep)->PrimitiveFn == StringPPrimitiveFn)
        {
            AsTypeMap(tmap)->TagMap[StringTag + INDIRECT_TAG_OFFSET] = val;
            return(1);
        }
        else if (AsPrimitive(typep)->PrimitiveFn == VectorPPrimitiveFn)
        {
            AsTypeMap(tmap)->TagMap[VectorTag + INDIRECT_TAG_OFFSET] = val;
            return(1);
        }
        else if (AsPrimitive(typep)->PrimitiveFn == BytevectorPPrimitiveFn)
        {
            AsTypeMap(tmap)->TagMap[BytevectorTag + INDIRECT_TAG_OFFSET] = val;
            return(1);
        }
        else if (AsPrimitive(typep)->PrimitiveFn == BinaryPortPPrimitiveFn)
        {
            AsTypeMap(tmap)->TagMap[BinaryPortTag + INDIRECT_TAG_OFFSET] = val;
            return(1);
        }
        else if (AsPrimitive(typep)->PrimitiveFn == TextualPortPPrimitiveFn)
        {
            AsTypeMap(tmap)->TagMap[TextualPortTag + INDIRECT_TAG_OFFSET] = val;
            return(1);
        }
        else if (AsPrimitive(typep)->PrimitiveFn == ProcedurePPrimitiveFn)
        {
            AsTypeMap(tmap)->TagMap[ProcedureTag + INDIRECT_TAG_OFFSET] = val;
            AsTypeMap(tmap)->TagMap[PrimitiveTag + INDIRECT_TAG_OFFSET] = val;
            return(1);
        }
        else if (AsPrimitive(typep)->PrimitiveFn == SymbolPPrimitiveFn)
        {
            AsTypeMap(tmap)->TagMap[SymbolTag + INDIRECT_TAG_OFFSET] = val;
            return(1);
        }
        else if (AsPrimitive(typep)->PrimitiveFn == ThreadPPrimitiveFn)
        {
            AsTypeMap(tmap)->TagMap[ThreadTag + INDIRECT_TAG_OFFSET] = val;
            return(1);
        }
        else if (AsPrimitive(typep)->PrimitiveFn == ExclusivePPrimitiveFn)
        {
            AsTypeMap(tmap)->TagMap[ExclusiveTag + INDIRECT_TAG_OFFSET] = val;
            return(1);
        }
        else if (AsPrimitive(typep)->PrimitiveFn == ConditionPPrimitiveFn)
        {
            AsTypeMap(tmap)->TagMap[ConditionTag + INDIRECT_TAG_OFFSET] = val;
            return(1);
        }
        else if (AsPrimitive(typep)->PrimitiveFn == EphemeronPPrimitiveFn)
        {
            AsTypeMap(tmap)->TagMap[EphemeronTag + INDIRECT_TAG_OFFSET] = val;
            return(1);
        }
    }

    return(0);
}

Define("make-type-map", MakeTypeMapPrimitive)(int_t argc, FObject argv[])
{
    ZeroArgsCheck("make-type-map", argc);

    return(MakeTypeMap());
}

Define("type-map-ref", TypeMapRefPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("type-map-ref", argc);
    TypeMapArgCheck("type-map-ref", argv[0]);

    return(TypeMapRef(argv[0], argv[1]));
}

Define("type-map-set!", TypeMapSetPrimitive)(int_t argc, FObject argv[])
{
    ThreeArgsCheck("type-map-set!", argc);
    TypeMapArgCheck("type-map-set!", argv[0]);

    if (TypeMapSet(argv[0], argv[1], argv[2]) == 0)
        RaiseExceptionC(Assertion, "type-map-set!", "expected a predicate", List(argv[1]));

    return(NoValueObject);
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
    MakeTypeMapPrimitive,
    TypeMapRefPrimitive,
    TypeMapSetPrimitive
};

// ----------------

// From base.cpp which is generated from base.scm
extern char FomentBase[];
extern char BuiltinLibraryNames[];
extern char * BuiltinLibraries[];

static void SetupScheme()
{
    FObject port = MakeStringCInputPort(
        "(define-syntax and"
            "(syntax-rules ()"
                "((and) #t)"
                "((and test) test)"
                "((and test1 test2 ...) (if test1 (and test2 ...) #f))))");
    WantIdentifiersPort(port, 1);
    Eval(Read(port), R.Bedrock);

    LibraryExport(R.BedrockLibrary, EnvironmentLookup(R.Bedrock, StringCToSymbol("and")));

    port = MakeStringCInputPort(BuiltinLibraryNames);
    R.BuiltinLibraryNames = Read(port);

    FAssert(VectorP(R.BuiltinLibraryNames));

    port = MakeStringCInputPort(FomentBase);
    WantIdentifiersPort(port, 1);
    FAlive ap(&port);

    for (;;)
    {
        FObject obj = Read(port);

        if (obj == EndOfFileObject)
            break;
        Eval(obj, R.Bedrock);
    }
}

FObject OpenBuiltinLibrary(FObject nam)
{
    FAssert(VectorP(R.BuiltinLibraryNames));

    for (uint_t idx = 0; idx < VectorLength(R.BuiltinLibraryNames); idx++)
        if (EqualP(nam, AsVector(R.BuiltinLibraryNames)->Vector[idx]))
            return(MakeStringCInputPort(BuiltinLibraries[idx]));

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

static int LittleEndianP()
{
    uint_t nd = 1;

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

int_t SetupFoment(FThreadState * ts)
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
    srand(RandomSeed);

    FObject * rv = (FObject *) &R;
    for (uint_t rdx = 0; rdx < sizeof(FRoots) / sizeof(FObject); rdx++)
        rv[rdx] = NoValueObject;

    if (SetupCore(ts) == 0)
        return(0);

    R.SymbolHashTree = MakeHashTree("%setup-foment");

    SetupHashContainers();
    SetupCompare();

    ts->Parameters = MakeEqHashMap();

    SetupLibrary();
    R.ExceptionRecordType = MakeRecordTypeC("exception",
            sizeof(ExceptionFieldsC) / sizeof(char *), ExceptionFieldsC);

    FObject nam = List(StringCToSymbol("foment"), StringCToSymbol("bedrock"));
    R.Bedrock = MakeEnvironment(nam, FalseObject);
    R.LoadedLibraries = EmptyListObject;
    R.BedrockLibrary = MakeLibrary(nam);

    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);

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

    for (uint_t n = 0; n < sizeof(SpecialSyntaxes) / sizeof(char *); n++)
        LibraryExport(R.BedrockLibrary, EnvironmentSetC(R.Bedrock, SpecialSyntaxes[n],
                MakeImmediate(n, SpecialSyntaxTag)));

    SetupHashContainerPrims();
    SetupComparePrims();
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

    DefineComparator("boolean-comparator", BooleanPPrimitive, BooleanEqualPPrimitive,
            BooleanComparePrimitive, EqHashPrimitive);

    LibraryExport(R.BedrockLibrary,
            EnvironmentSetC(R.Bedrock, "%standard-input", R.StandardInput));
    LibraryExport(R.BedrockLibrary,
            EnvironmentSetC(R.Bedrock, "%standard-output", R.StandardOutput));
    LibraryExport(R.BedrockLibrary,
            EnvironmentSetC(R.Bedrock, "%standard-error", R.StandardError));

#ifdef FOMENT_DEBUG
    LibraryExport(R.BedrockLibrary,
            EnvironmentSetC(R.Bedrock, "%debug-build", TrueObject));
#else // FOMENT_DEBUG
    LibraryExport(R.BedrockLibrary,
            EnvironmentSetC(R.Bedrock, "%debug-build", FalseObject));
#endif // FOMENT_DEBUG

    R.Features = EmptyListObject;

    for (uint_t idx = 0; idx < sizeof(FeaturesC) / sizeof(char *); idx++)
        R.Features = MakePair(StringCToSymbol(FeaturesC[idx]), R.Features);

    R.Features = MakePair(StringCToSymbol(CPUArchitecture()), R.Features);
    R.Features = MakePair(StringCToSymbol(OSName()), R.Features);
    R.Features = MakePair(StringCToSymbol(LittleEndianP() ? "little-endian" : "big-endian"),
            R.Features);
    if (CollectorType == MarkSweepCollector || CollectorType == GenerationalCollector)
        R.Features = MakePair(StringCToSymbol("guardians"), R.Features);
    if (CollectorType == GenerationalCollector)
        R.Features = MakePair(StringCToSymbol("trackers"), R.Features);

    R.LibraryPath = EmptyListObject;

    GetEnvironmentVariables();

    FObject lp = Assoc(MakeStringC("FOMENT_LIBPATH"), R.EnvironmentVariables);
    if (PairP(lp))
    {
        FAssert(StringP(First(lp)));

        lp = Rest(lp);

        uint_t strt = 0;
        uint_t idx = 0;
        while (idx < StringLength(lp))
        {
            if (AsString(lp)->String[idx] == PathSep)
            {
                if (idx > strt)
                    R.LibraryPath = MakePair(
                            MakeString(AsString(lp)->String + strt, idx - strt), R.LibraryPath);

                idx += 1;
                strt = idx;
            }

            idx += 1;
        }

        if (idx > strt)
            R.LibraryPath = MakePair(
                    MakeString(AsString(lp)->String + strt, idx - strt), R.LibraryPath);
    }

    R.LibraryExtensions = List(MakeStringC("sld"), MakeStringC("scm"));

    if (CheckHeapFlag)
        CheckHeap(__FILE__, __LINE__);
    SetupScheme();
    if (CheckHeapFlag)
        CheckHeap(__FILE__, __LINE__);

    SetupComplete = 1;
    return(1);
}
