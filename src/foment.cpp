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

#ifdef FOMENT_BSD
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

uint_t InlineProcedures = 1;
uint_t InlineImports = 1;

FRoots R;

void FAssertFailed(const char * fn, int_t ln, const char * expr)
{
    printf("FAssert: %s (%d)%s\n", expr, (int) ln, fn);

//    *((char *) 0) = 0;
//    fgetc(stdin);
    ExitFoment();
    exit(1);
}

void FMustBeFailed(const char * fn, int_t ln, const char * expr)
{
    printf("FMustBe: %s (%d)%s\n", expr, (int) ln, fn);

//    *((char *) 0) = 0;
    ExitFoment();
    exit(1);
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
    "let-syntax",
    "letrec-syntax",
    "case",
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

// ---- Equivalence predicates ----

int_t EqvP(FObject obj1, FObject obj2)
{
    if (obj1 == obj2)
        return(1);

    return(GenericEqvP(obj1, obj2));
}

int_t EqP(FObject obj1, FObject obj2)
{
    if (obj1 == obj2)
        return(1);

    return(0);
}

// ---- Equal ----
//
// Disjoint-set trees
// http://en.wikipedia.org/wiki/Disjoint-set_data_structure
// http://www.cs.indiana.edu/~dyb/pubs/equal.pdf

static FObject EqualPFind(FObject obj)
{
    FAssert(BoxP(obj));

    if (BoxP(Unbox(obj)))
    {
        FObject ret = EqualPFind(Unbox(obj));

        FAssert(BoxP(ret));
        FAssert(FixnumP(Unbox(ret)));

        SetBox(obj, ret);
        return(ret);
    }

    FAssert(FixnumP(Unbox(obj)));

    return(obj);
}

static int_t EqualPUnionFind(FObject ht, FObject objx, FObject objy)
{
    FObject bx = EqHashtableRef(ht, objx, FalseObject);
    FObject by = EqHashtableRef(ht, objy, FalseObject);

    if (bx == FalseObject)
    {
        if (by == FalseObject)
        {
            FObject nb = MakeBox(MakeFixnum(1));
            EqHashtableSet(ht, objx, nb);
            EqHashtableSet(ht, objy, nb);
        }
        else
        {
            FAssert(BoxP(by));

            EqHashtableSet(ht, objx, EqualPFind(by));
        }
    }
    else
    {
        FAssert(BoxP(bx));

        if (by == FalseObject)
            EqHashtableSet(ht, objy, EqualPFind(bx));
        else
        {
            FAssert(BoxP(by));

            FObject rx = EqualPFind(bx);
            FObject ry = EqualPFind(by);

            FAssert(BoxP(rx));
            FAssert(BoxP(ry));
            FAssert(FixnumP(Unbox(rx)));
            FAssert(FixnumP(Unbox(ry)));

            if (EqP(rx, ry))
                return(1);

            FFixnum nx = AsFixnum(Unbox(rx));
            FFixnum ny = AsFixnum(Unbox(ry));

            if (nx > ny)
            {
                SetBox(ry, rx);
                SetBox(rx, MakeFixnum(nx + ny));
            }
            else
            {
                SetBox(rx, ry);
                SetBox(ry, MakeFixnum(nx + ny));
            }
        }
    }

    return(0);
}

static int_t EqualP(FObject ht, FObject obj1, FObject obj2)
{
    if (EqvP(obj1, obj2))
        return(1);

    if (PairP(obj1))
    {
        if (PairP(obj2) == 0)
            return(0);

        if (EqualPUnionFind(ht, obj1, obj2))
            return(1);

        if (EqualP(ht, First(obj1), First(obj2)) && EqualP(ht, Rest(obj1), Rest(obj2)))
            return(1);

        return(0);
    }

    if (BoxP(obj1))
    {
        if (BoxP(obj2) == 0)
            return(0);

        if (EqualPUnionFind(ht, obj1, obj2))
            return(1);

        return(EqualP(ht, Unbox(obj1), Unbox(obj2)));
    }

    if (VectorP(obj1))
    {
        if (VectorP(obj2) == 0)
            return(0);

        if (VectorLength(obj1) != VectorLength(obj2))
            return(0);

        if (EqualPUnionFind(ht, obj1, obj2))
            return(1);

        for (uint_t idx = 0; idx < VectorLength(obj1); idx++)
            if (EqualP(ht, AsVector(obj1)->Vector[idx], AsVector(obj2)->Vector[idx]) == 0)
                return(0);

        return(1);
    }

    if (StringP(obj1))
    {
        if (StringP(obj2) == 0)
            return(0);

        return(StringEqualP(obj1, obj2));
    }

    if (BytevectorP(obj1))
    {
        if (BytevectorP(obj2) == 0)
            return(0);

        if (BytevectorLength(obj1) != BytevectorLength(obj2))
            return(0);

        for (uint_t idx = 0; idx < BytevectorLength(obj1); idx++)
            if (AsBytevector(obj1)->Vector[idx] != AsBytevector(obj2)->Vector[idx])
                return(0);
        return(1);
    }

    return(0);
}

int_t EqualP(FObject obj1, FObject obj2)
{
    return(EqualP(MakeEqHashtable(31), obj1, obj2));
}

Define("eqv?", EqvPPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("eqv?", argc);

    return(EqvP(argv[0], argv[1]) ? TrueObject : FalseObject);
}

Define("eq?", EqPPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("eq?", argc);

    return(EqP(argv[0], argv[1]) ? TrueObject : FalseObject);
}

Define("equal?", EqualPPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("equal?", argc);

    return(EqualP(argv[0], argv[1]) ? TrueObject : FalseObject);
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

// ---- Symbols ----

static uint_t NextSymbolHash = 0;

FObject StringToSymbol(FObject str)
{
    FAssert(StringP(str));

    FObject obj = HashtableRef(R.SymbolHashtable, str, FalseObject, StringEqualP, StringHash);
    if (obj == FalseObject)
    {
        FSymbol * sym = (FSymbol *) MakeObject(sizeof(FSymbol), SymbolTag);
        sym->Reserved = MakeLength(NextSymbolHash, SymbolTag);
        sym->String = str;
        NextSymbolHash += 1;
        if (NextSymbolHash > MAXIMUM_OBJECT_LENGTH)
            NextSymbolHash = 0;

        obj = sym;
        HashtableSet(R.SymbolHashtable, str, obj, StringEqualP, StringHash);
    }

    FAssert(SymbolP(obj));
    return(obj);
}

FObject StringCToSymbol(const char * s)
{
    return(StringToSymbol(MakeStringC(s)));
}

FObject StringLengthToSymbol(FCh * s, int_t sl)
{
    FObject obj = HashtableStringRef(R.SymbolHashtable, s, sl, FalseObject);
    if (obj == FalseObject)
    {
        FSymbol * sym = (FSymbol *) MakeObject(sizeof(FSymbol), SymbolTag);
        sym->Reserved = MakeLength(NextSymbolHash, SymbolTag);
        sym->String = MakeString(s, sl);
        NextSymbolHash += 1;
        if (NextSymbolHash > MAXIMUM_OBJECT_LENGTH)
            NextSymbolHash = 0;

        obj = sym;
        HashtableSet(R.SymbolHashtable, sym->String, obj, StringEqualP, StringHash);
    }

    FAssert(SymbolP(obj));
    return(obj);
}

FObject PrefixSymbol(FObject str, FObject sym)
{
    FAssert(StringP(str));
    FAssert(SymbolP(sym));

    FObject nstr = MakeStringCh(StringLength(str) + StringLength(AsSymbol(sym)->String), 0);
    uint_t sdx;
    for (sdx = 0; sdx < StringLength(str); sdx++)
        AsString(nstr)->String[sdx] = AsString(str)->String[sdx];

    for (uint_t idx = 0; idx < StringLength(AsSymbol(sym)->String); idx++)
        AsString(nstr)->String[sdx + idx] = AsString(AsSymbol(sym)->String)->String[idx];

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

    return(AsSymbol(argv[0])->String);
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

    Raise(MakeException(R.Assertion, StringCToSymbol("error"), NoValueObject, argv[0], lst));
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
    FBox * bx = (FBox *) MakeObject(sizeof(FBox), BoxTag);
    bx->Reserved = BoxTag;
    bx->Value = val;

    return(bx);
}

// ---- Hashtables ----

uint_t EqHash(FObject obj)
{
    return((uint_t) obj);
}

uint_t EqvHash(FObject obj)
{
    return(EqHash(obj));
}

#define MaxHashDepth 128

static uint_t DoEqualHash(FObject obj, int_t d)
{
    uint_t h;

    if (d >= MaxHashDepth)
        return(1);

    if (PairP(obj))
    {
        h = 0;
        for (int_t n = 0; n < MaxHashDepth; n++)
        {
            h += (h << 3);
            h += DoEqualHash(First(obj), d + 1);
            obj = Rest(obj);
            if (PairP(obj) == 0)
            {
                h += (h << 3);
                h += DoEqualHash(obj, d + 1);
                return(h);
            }
        }
        return(h);
    }
    else if (BoxP(obj))
        return(DoEqualHash(Unbox(obj), d + 1));
    else if (StringP(obj))
        return(StringHash(obj));
    else if (VectorP(obj))
    {
        if (VectorLength(obj) == 0)
            return(1);

        h = 0;
        for (uint_t idx = 0; idx < VectorLength(obj) && idx < MaxHashDepth; idx++)
            h += (h << 5) + DoEqualHash(AsVector(obj)->Vector[idx], d + 1);
        return(h);
    }
    else if (BytevectorP(obj))
        return(BytevectorHash(obj));

    return(EqHash(obj));
}

uint_t EqualHash(FObject obj)
{
    return(DoEqualHash(obj, 0));
}

static int_t Primes[] =
{
    23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107,
    109, 113, 127, 131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197,
    199, 211, 223, 227, 229, 233, 239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293,
    307, 311, 313, 317, 331, 337, 347, 349, 353, 359, 367, 373, 379, 383, 389, 397, 401,
    409, 419, 421, 431, 433, 439, 443, 449, 457, 461, 463, 467, 479, 487, 491, 499, 503,
    509, 521, 523, 541, 547, 557, 563, 569, 571, 577, 587, 593, 599, 601, 607, 613, 617,
    619, 631, 641, 643, 647, 653, 659, 661, 673, 677, 683, 691, 701, 709, 719, 727, 733,
    739, 743, 751, 757, 761, 769, 773, 787, 797, 809, 811, 821, 823, 827, 829, 839, 853,
    857, 859, 863, 877, 881, 883, 887, 907, 911, 919, 929, 937, 941, 947, 953, 967, 971,
    977, 983, 991, 997
};

static const char * HashtableFieldsC[] = {"buckets", "size", "tracker"};

static FObject MakeHashtable(int_t nb, FObject trkr)
{
    FAssert(sizeof(FHashtable) == sizeof(HashtableFieldsC) + sizeof(FRecord));

    if (nb <= Primes[0])
        nb = Primes[0];
    else if (nb >= Primes[sizeof(Primes) / sizeof(int_t) - 1])
        nb = Primes[sizeof(Primes) / sizeof(int_t) - 1];
    else
    {
        for (int_t idx = sizeof(Primes) / sizeof(int_t) - 2; idx >= 0; idx--)
            if (nb > Primes[idx])
            {
                nb = Primes[idx + 1];
                break;
            }
    }

    FHashtable * ht = (FHashtable *) MakeRecord(R.HashtableRecordType);
    ht->Buckets = MakeVector(nb, 0, NoValueObject);
    for (int_t idx = 0; idx < nb; idx++)
        ModifyVector(ht->Buckets, idx, MakeFixnum(idx));
    ht->Size = MakeFixnum(0);
    ht->Tracker = trkr;

    return(ht);
}

FObject MakeHashtable(int_t nb)
{
    return(MakeHashtable(nb, NoValueObject));
}

static FObject DoHashtableRef(FObject ht, FObject key, FEquivFn efn, FHashFn hfn)
{
    FAssert(HashtableP(ht));

    uint_t idx = hfn(key) % (uint_t) VectorLength(AsHashtable(ht)->Buckets);

    FObject node = AsVector(AsHashtable(ht)->Buckets)->Vector[idx];

    while (PairP(node))
    {
        FAssert(PairP(First(node)));

        if (efn(First(First(node)), key))
            break;

        node = Rest(node);
    }

    return(node);
}

FObject HashtableRef(FObject ht, FObject key, FObject def, FEquivFn efn, FHashFn hfn)
{
    FAssert(HashtableP(ht));

    FObject node = DoHashtableRef(ht, key, efn, hfn);
    if (PairP(node))
        return(Rest(First(node)));
    return(def);
}

FObject HashtableStringRef(FObject ht, FCh * s, int_t sl, FObject def)
{
    FAssert(HashtableP(ht));

    uint_t idx = StringLengthHash(s, sl)
            % (uint_t) VectorLength(AsHashtable(ht)->Buckets);
    FObject node = AsVector(AsHashtable(ht)->Buckets)->Vector[idx];

    while (PairP(node))
    {
        FAssert(PairP(First(node)));

        if (StringLengthEqualP(s, sl, First(First(node))))
            return(Rest(First(node)));

        node = Rest(node);
    }

    return(def);
}

void HashtableSet(FObject ht, FObject key, FObject val, FEquivFn efn, FHashFn hfn)
{
    FAssert(HashtableP(ht));

    FObject node = DoHashtableRef(ht, key, efn, hfn);
    if (PairP(node))
    {
//        AsPair(First(node))->Rest = val;
        SetRest(First(node), val);
    }
    else
    {
        uint_t idx = hfn(key) % (uint_t) VectorLength(AsHashtable(ht)->Buckets);

//        AsVector(AsHashtable(ht)->Buckets)->Vector[idx] =
//                MakePair(MakePair(key, val),
//                AsVector(AsHashtable(ht)->Buckets)->Vector[idx]);

        FObject kvn = MakePair(MakePair(key, val),
                AsVector(AsHashtable(ht)->Buckets)->Vector[idx]);
        if (PairP(AsHashtable(ht)->Tracker))
            InstallTracker(key, kvn, AsHashtable(ht)->Tracker);

        ModifyVector(AsHashtable(ht)->Buckets, idx, kvn);

//        AsHashtable(ht)->Size = MakeFixnum(AsFixnum(AsHashtable(ht)->Size) + 1);
        Modify(FHashtable, ht, Size, MakeFixnum(AsFixnum(AsHashtable(ht)->Size) + 1));
    }
}

void HashtableDelete(FObject ht, FObject key, FEquivFn efn, FHashFn hfn)
{
    FAssert(HashtableP(ht));

    uint_t idx = hfn(key) % (uint_t) VectorLength(AsHashtable(ht)->Buckets);

    FObject node = AsVector(AsHashtable(ht)->Buckets)->Vector[idx];
    FObject prev = NoValueObject;

    while (PairP(node))
    {
        FAssert(PairP(First(node)));

        if (efn(First(First(node)), key))
        {
            if (PairP(prev))
            {
//                AsPair(prev)->Rest = Rest(node);
                SetRest(prev, Rest(node));
            }
            else
            {
//                AsVector(AsHashtable(ht)->Buckets)->Vector[idx] = Rest(node);
                ModifyVector(AsHashtable(ht)->Buckets, idx, Rest(node));
            }

            FAssert(AsFixnum(AsHashtable(ht)->Size) > 0);
//            AsHashtable(ht)->Size = MakeFixnum(AsFixnum(AsHashtable(ht)->Size) - 1);
            Modify(FHashtable, ht, Size, MakeFixnum(AsFixnum(AsHashtable(ht)->Size) - 1));

            break;
        }

        prev = node;
        node = Rest(node);
    }
}

int_t HashtableContainsP(FObject ht, FObject key, FEquivFn efn, FHashFn hfn)
{
    FAssert(HashtableP(ht));

    FObject node = DoHashtableRef(ht, key, efn, hfn);
    if (PairP(node))
        return(1);
    return(0);
}

FObject MakeEqHashtable(int_t nb)
{
    return(MakeHashtable(nb, MakeTConc()));
}

static uint_t RehashFindBucket(FObject kvn)
{
    while (PairP(kvn))
        kvn = Rest(kvn);

    FAssert(FixnumP(kvn));

    return(AsFixnum(kvn));
}

static void RehashRemoveBucket(FObject ht, FObject kvn, uint_t idx)
{
    FObject node = AsVector(AsHashtable(ht)->Buckets)->Vector[idx];
    FObject prev = NoValueObject;

    while (PairP(node))
    {
        if (node == kvn)
        {
            if (PairP(prev))
                SetRest(prev, Rest(node));
            else
                ModifyVector(AsHashtable(ht)->Buckets, idx, Rest(node));

            return;
        }

        prev = node;
        node = Rest(node);
    }

    FAssert(0);
}

#ifdef FOMENT_DEBUG
static void CheckEqHashtable(FObject ht)
{
    FAssert(HashtableP(ht));
    FAssert(VectorP(AsHashtable(ht)->Buckets));
    uint_t len = VectorLength(AsHashtable(ht)->Buckets);

    for (uint_t idx = 0; idx < len; idx++)
    {
        FObject node = AsVector(AsHashtable(ht)->Buckets)->Vector[idx];

        while (PairP(node))
        {
            FAssert(PairP(First(node)));
            FAssert(EqHash(First(First(node))) % len == idx);

            node = Rest(node);
        }

        FAssert(FixnumP(node));
        FAssert(AsFixnum(node) == (int_t) idx);
    }
}
#endif // FOMENT_DEBUG

static void EqHashtableRehash(FObject ht, FObject tconc)
{
    FObject kvn;

    while (TConcEmptyP(tconc) == 0)
    {
        kvn = TConcRemove(tconc);

        FAssert(PairP(kvn));
        FAssert(PairP(First(kvn)));

        FObject key = First(First(kvn));
        uint_t odx = RehashFindBucket(kvn);
        uint_t idx = EqHash(key) % (uint_t) VectorLength(AsHashtable(ht)->Buckets);

        if (idx != odx)
        {
            RehashRemoveBucket(ht, kvn, odx);
            SetRest(kvn, AsVector(AsHashtable(ht)->Buckets)->Vector[idx]);
            ModifyVector(AsHashtable(ht)->Buckets, idx, kvn);
        }

        InstallTracker(key, kvn, AsHashtable(ht)->Tracker);
    }

#ifdef FOMENT_DEBUG
    CheckEqHashtable(ht);
#endif // FOMENT_DEBUG
}

FObject EqHashtableRef(FObject ht, FObject key, FObject def)
{
    FAssert(HashtableP(ht));
    FAssert(PairP(AsHashtable(ht)->Tracker));

    if (TConcEmptyP(AsHashtable(ht)->Tracker) == 0)
        EqHashtableRehash(ht, AsHashtable(ht)->Tracker);

    return(HashtableRef(ht, key, def, EqP, EqHash));
}

void EqHashtableSet(FObject ht, FObject key, FObject val)
{
    FAssert(HashtableP(ht));
    FAssert(PairP(AsHashtable(ht)->Tracker));

    if (TConcEmptyP(AsHashtable(ht)->Tracker) == 0)
        EqHashtableRehash(ht, AsHashtable(ht)->Tracker);

    HashtableSet(ht, key, val, EqP, EqHash);
}

void EqHashtableDelete(FObject ht, FObject key)
{
    FAssert(HashtableP(ht));
    FAssert(PairP(AsHashtable(ht)->Tracker));

    if (TConcEmptyP(AsHashtable(ht)->Tracker) == 0)
        EqHashtableRehash(ht, AsHashtable(ht)->Tracker);

    HashtableDelete(ht, key, EqP, EqHash);
}

int_t EqHashtableContainsP(FObject ht, FObject key)
{
    FAssert(HashtableP(ht));
    FAssert(PairP(AsHashtable(ht)->Tracker));

    if (TConcEmptyP(AsHashtable(ht)->Tracker) == 0)
        EqHashtableRehash(ht, AsHashtable(ht)->Tracker);

    return(HashtableContainsP(ht, key, EqP, EqHash));
}

uint_t HashtableSize(FObject ht)
{
    FAssert(HashtableP(ht));
    FAssert(FixnumP(AsHashtable(ht)->Size));

    if (TConcEmptyP(AsHashtable(ht)->Tracker) == 0)
        EqHashtableRehash(ht, AsHashtable(ht)->Tracker);

    return(AsFixnum(AsHashtable(ht)->Size));
}

void HashtableWalkUpdate(FObject ht, FWalkUpdateFn wfn, FObject ctx)
{
    FAssert(HashtableP(ht));

    FObject bkts = AsHashtable(ht)->Buckets;
    int_t len = VectorLength(bkts);

    for (int_t idx = 0; idx < len; idx++)
    {
        FObject lst = AsVector(bkts)->Vector[idx];

        while (PairP(lst))
        {
            FAssert(PairP(First(lst)));

            FObject val = wfn(First(First(lst)), Rest(First(lst)), ctx);
            if (val != Rest(First(lst)))
            {
//                AsPair(First(lst))->Rest = val;
                SetRest(First(lst), val);
            }

            lst = Rest(lst);
        }
    }
}

void HashtableWalkDelete(FObject ht, FWalkDeleteFn wfn, FObject ctx)
{
    FAssert(HashtableP(ht));

    FObject bkts = AsHashtable(ht)->Buckets;
    int_t len = VectorLength(bkts);

    for (int_t idx = 0; idx < len; idx++)
    {
        FObject lst = AsVector(bkts)->Vector[idx];
        FObject prev = NoValueObject;

        while (PairP(lst))
        {
            FAssert(PairP(First(lst)));

            if (wfn(First(First(lst)), Rest(First(lst)), ctx))
            {
                if (PairP(prev))
                {
//                    AsPair(prev)->Rest = Rest(lst);
                    SetRest(prev, Rest(lst));
                }
                else
                {
//                    AsVector(bkts)->Vector[idx] = Rest(lst);
                    ModifyVector(bkts, idx, Rest(lst));
                }
            }

            prev = lst;
            lst = Rest(lst);
        }
    }
}

void HashtableWalkVisit(FObject ht, FWalkVisitFn wfn, FObject ctx)
{
    FAssert(HashtableP(ht));

    FObject bkts = AsHashtable(ht)->Buckets;
    int_t len = VectorLength(bkts);

    for (int_t idx = 0; idx < len; idx++)
    {
        FObject lst = AsVector(bkts)->Vector[idx];

        while (PairP(lst))
        {
            FAssert(PairP(First(lst)));

            wfn(First(First(lst)), Rest(First(lst)), ctx);
            lst = Rest(lst);
        }
    }
}

Define("make-eq-hashtable", MakeEqHashtablePrimitive)(int_t argc, FObject argv[])
{
    ZeroOrOneArgsCheck("make-eq-hashtable", argc);

    if (argc == 1)
        NonNegativeArgCheck("make-eq-hashtable", argv[0], 0);

    return(MakeEqHashtable(argc == 0 ? 0 : AsFixnum(argv[0])));
}

Define("eq-hashtable-ref", EqHashtableRefPrimitive)(int_t argc, FObject argv[])
{
    ThreeArgsCheck("eq-hashtable-ref", argc);
    EqHashtableArgCheck("eq-hashtable-ref", argv[0]);

    return(EqHashtableRef(argv[0], argv[1], argv[2]));
}

Define("eq-hashtable-set!", EqHashtableSetPrimitive)(int_t argc, FObject argv[])
{
    ThreeArgsCheck("eq-hashtable-set!", argc);
    EqHashtableArgCheck("eq-hashtable-set!", argv[0]);

    EqHashtableSet(argv[0], argv[1], argv[2]);
    return(NoValueObject);
}

Define("eq-hashtable-delete", EqHashtableDeletePrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("eq-hashtable-delete", argc);
    EqHashtableArgCheck("eq-hashtable-delete", argv[0]);

    EqHashtableDelete(argv[0], argv[1]);
    return(NoValueObject);
}

// ---- Record Types ----

FObject MakeRecordType(FObject nam, uint_t nf, FObject flds[])
{
    FAssert(SymbolP(nam));

    FRecordType * rt = (FRecordType *) MakeObject(sizeof(FRecordType) + sizeof(FObject) * nf,
            RecordTypeTag);
    rt->NumFields = MakeLength(nf + 1, RecordTypeTag);
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
            RaiseExceptionC(R.Assertion, "define-record-type", "expected a list of fields",
                    List(argv[1], First(flst)));

        if (Memq(First(First(flst)), flds) != FalseObject)
            RaiseExceptionC(R.Assertion, "define-record-type", "duplicate field name",
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

    RaiseExceptionC(R.Assertion, "define-record-type", "expected a field-name",
            List(argv[1], argv[0]));

    return(NoValueObject);
}

Define("%record-ref", RecordRefPrimitive)(int_t argc, FObject argv[])
{
    // (%record-ref <record-type> <obj> <index>)

    FMustBe(argc == 3);
    FMustBe(RecordTypeP(argv[0]));

    if (RecordP(argv[1], argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "%record-ref", "not a record of the expected type",
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
        RaiseExceptionC(R.Assertion, "%record-set!", "not a record of the expected type",
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

    FGenericRecord * r = (FGenericRecord *) MakeObject(
            sizeof(FGenericRecord) + sizeof(FObject) * (nf - 1), RecordTag);
    r->NumFields = MakeLength(nf, RecordTag);
    r->Fields[0] = rt;

    for (uint_t fdx = 1; fdx < nf; fdx++)
        r->Fields[fdx] = NoValueObject;

    return(r);
}

// ---- Primitives ----

FObject MakePrimitive(FPrimitive * prim)
{
    FPrimitive * p = (FPrimitive *) MakeObject(sizeof(FPrimitive), PrimitiveTag);
    memcpy(p, prim, sizeof(FPrimitive));

    return(p);
}

void DefinePrimitive(FObject env, FObject lib, FPrimitive * prim)
{
    LibraryExport(lib, EnvironmentSetC(env, prim->Name, MakePrimitive(prim)));
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

// ---- Primitives ----

static FPrimitive * Primitives[] =
{
    &EqvPPrimitive,
    &EqPPrimitive,
    &EqualPPrimitive,
    &NotPrimitive,
    &BooleanPPrimitive,
    &BooleanEqualPPrimitive,
    &SymbolPPrimitive,
    &SymbolEqualPPrimitive,
    &SymbolToStringPrimitive,
    &StringToSymbolPrimitive,
    &RaisePrimitive,
    &ErrorPrimitive,
    &ErrorObjectPPrimitive,
    &ErrorObjectTypePrimitive,
    &ErrorObjectWhoPrimitive,
    &ErrorObjectKindPrimitive,
    &ErrorObjectMessagePrimitive,
    &ErrorObjectIrritantsPrimitive,
    &FullErrorPrimitive,
    &CommandLinePrimitive,
    &GetEnvironmentVariablePrimitive,
    &GetEnvironmentVariablesPrimitive,
    &CurrentSecondPrimitive,
    &CurrentJiffyPrimitive,
    &JiffiesPerSecondPrimitive,
    &FeaturesPrimitive,
    &SetFeaturesPrimitive,
    &MakeEqHashtablePrimitive,
    &EqHashtableRefPrimitive,
    &EqHashtableSetPrimitive,
    &EqHashtableDeletePrimitive,
    &MakeRecordTypePrimitive,
    &MakeRecordPrimitive,
    &RecordPredicatePrimitive,
    &RecordIndexPrimitive,
    &RecordRefPrimitive,
    &RecordSetPrimitive,
    &LoadedLibrariesPrimitive,
    &LibraryPathPrimitive,
    &RandomPrimitive,
    &NoValuePrimitive,
    &ImplementationNamePrimitive,
    &ImplementationVersionPrimitive,
    &CPUArchitecturePrimitive,
    &MachineNamePrimitive,
    &OSNamePrimitive,
    &OSVersionPrimitive,
    &BoxPrimitive,
    &BoxPPrimitive,
    &UnboxPrimitive,
    &SetBoxPrimitive
};

// ----------------

extern char BaseCode[];

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

    port = MakeStringCInputPort(BaseCode);
    WantIdentifiersPort(port, 1);
    PushRoot(&port);

    for (;;)
    {
        FObject obj = Read(port);

        if (obj == EndOfFileObject)
            break;
        Eval(obj, R.Bedrock);
    }

    PopRoot();
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

void SetupFoment(FThreadState * ts)
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

    srand((unsigned int) time(0));

    FObject * rv = (FObject *) &R;
    for (uint_t rdx = 0; rdx < sizeof(FRoots) / sizeof(FObject); rdx++)
        rv[rdx] = NoValueObject;

    SetupCore(ts);

    FAssert(R.HashtableRecordType == NoValueObject);
    R.SymbolHashtable = MakeObject(sizeof(FHashtable), RecordTag);

    AsHashtable(R.SymbolHashtable)->Record.NumFields = RecordTag;
    AsHashtable(R.SymbolHashtable)->Record.RecordType = R.HashtableRecordType;
    AsHashtable(R.SymbolHashtable)->Buckets = MakeVector(941, 0, EmptyListObject);
    AsHashtable(R.SymbolHashtable)->Size = MakeFixnum(0);
    AsHashtable(R.SymbolHashtable)->Tracker = NoValueObject;

    FAssert(HashtableP(R.SymbolHashtable));

    R.HashtableRecordType = MakeRecordTypeC("hashtable",
            sizeof(HashtableFieldsC) / sizeof(char *), HashtableFieldsC);
    AsHashtable(R.SymbolHashtable)->Record.RecordType = R.HashtableRecordType;
    AsHashtable(R.SymbolHashtable)->Record.NumFields =
            MakeLength(RecordTypeNumFields(R.HashtableRecordType), RecordTag);

    FAssert(HashtableP(R.SymbolHashtable));

    ts->Parameters = MakeEqHashtable(0);

    SetupLibrary();
    R.ExceptionRecordType = MakeRecordTypeC("exception",
            sizeof(ExceptionFieldsC) / sizeof(char *), ExceptionFieldsC);
    R.Assertion = StringCToSymbol("assertion-violation");
    R.Restriction = StringCToSymbol("implementation-restriction");
    R.Lexical = StringCToSymbol("lexical-violation");
    R.Syntax = StringCToSymbol("syntax-violation");
    R.Error = StringCToSymbol("error-violation");

    FObject nam = List(StringCToSymbol("foment"), StringCToSymbol("bedrock"));
    R.Bedrock = MakeEnvironment(nam, FalseObject);
    R.LoadedLibraries = EmptyListObject;
    R.BedrockLibrary = MakeLibrary(nam);

    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);

    R.NoValuePrimitiveObject = MakePrimitive(&NoValuePrimitive);

    for (uint_t n = 0; n < sizeof(SpecialSyntaxes) / sizeof(char *); n++)
        LibraryExport(R.BedrockLibrary, EnvironmentSetC(R.Bedrock, SpecialSyntaxes[n],
                MakeImmediate(n, SpecialSyntaxTag)));

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

    SetupScheme();

    SetupComplete = 1;
}
