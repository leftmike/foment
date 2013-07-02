/*

Foment

*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "foment.hpp"

FConfig Config = {1, 1, 1, 0};
FRoots R;

void FAssertFailed(char * fn, int ln, char * expr)
{
    printf("FAssert: %s (%d)%s\n", expr, ln, fn);

    *((char *) 0) = 0;
    exit(1);
}

// ---- Immediates ----

static char * SpecialSyntaxes[] =
{
    "quote",
    "lambda",
    "if",
    "set!",
    "let",
    "letrec",
    "letrec*",
    "let*",
    "let-values",
    "let*-values",
    "letrec-values",
    "letrec*-values",
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

    "import",
    "define-library",

    "else",
    "=>",
    "unquote",
    "unquote-splicing",

    "only",
    "except",
    "prefix",
    "rename",
    "export",
    "include-library-declarations"
};

static char * SpecialSyntaxToName(FObject obj)
{
    FAssert(SpecialSyntaxP(obj));

    int n = AsValue(obj);
    FAssert(n >= 0);
    FAssert(n < sizeof(SpecialSyntaxes) / sizeof(char *));

    return(SpecialSyntaxes[n]);
}

FObject SpecialSyntaxToSymbol(FObject obj)
{
    return(StringCToSymbol(SpecialSyntaxToName(obj)));
}

FObject SpecialSyntaxMsgC(FObject obj, char * msg)
{
    char buf[128];
    char * s = buf;
    char * n = SpecialSyntaxToName(obj);

    while (*n)
        *s++ = *n++;

    *s++ = ':';
    *s++ = ' ';

    while (*msg)
        *s++ = *msg++;

    *s = 0;

    return(MakeStringC(buf));
}

void WriteSpecialSyntax(FObject port, FObject obj, int df)
{
    char * n = SpecialSyntaxToName(obj);

    PutStringC(port, "#<syntax: ");
    PutStringC(port, n);
    PutCh(port, '>');
}

Define("boolean?", BooleanPPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "boolean?", "boolean?: expected one argument",
                EmptyListObject);

    return(BooleanP(argv[0]) ? TrueObject : FalseObject);
}

Define("not", NotPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "not", "not: expected one argument", EmptyListObject);

    return(argv[0] == FalseObject ? TrueObject : FalseObject);
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

unsigned int EqHash(FObject obj)
{
    if (SymbolP(obj))
        return(SymbolHash(obj));
    return((unsigned int) obj);
}

unsigned int EqvHash(FObject obj)
{
    return(EqHash(obj));
}

#define MaxHashDepth 128

static unsigned int DoEqualHash(FObject obj, int d)
{
    unsigned int h;

    if (d >= MaxHashDepth)
        return(1);

    if (PairP(obj))
    {
        h = 0;
        for (int n = 0; n < MaxHashDepth; n++)
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
        for (unsigned int idx = 0; idx < VectorLength(obj) && idx < MaxHashDepth; idx++)
            h += (h << 5) + DoEqualHash(AsVector(obj)->Vector[idx], d + 1);
        return(h);
    }
    else if (BytevectorP(obj))
        return(BytevectorHash(obj));

    return(EqHash(obj));
}

unsigned int EqualHash(FObject obj)
{
    return(DoEqualHash(obj, 0));
}

Define("eq-hash", EqHashPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "eq-hash", "eq-hash: expected one argument",
                EmptyListObject);

    return(MakeFixnum(EqHash(argv[0])));
}

Define("eqv-hash", EqvHashPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "eqv-hash", "eqv-hash: expected one argument",
                EmptyListObject);

    return(MakeFixnum(EqvHash(argv[0])));
}

Define("equal-hash", EqualHashPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "equal-hash", "equal-hash: expected one argument",
                EmptyListObject);

    return(MakeFixnum(EqualHash(argv[0])));
}

static int Primes[] =
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

static char * HashtableFieldsC[] = {"buckets", "size"};

FObject MakeHashtable(int nb)
{
    FAssert(sizeof(FHashtable) == sizeof(HashtableFieldsC) + sizeof(FRecord));

    if (nb <= Primes[0])
        nb = Primes[0];
    else if (nb >= Primes[sizeof(Primes) / sizeof(int) - 1])
        nb = Primes[sizeof(Primes) / sizeof(int) - 1];
    else
    {
        for (int idx = sizeof(Primes) / sizeof(int) - 2; idx >= 0; idx--)
            if (nb > Primes[idx])
            {
                nb = Primes[idx + 1];
                break;
            }
    }

    FHashtable * ht = (FHashtable *) MakeRecord(R.HashtableRecordType);
    ht->Buckets = MakeVector(nb, 0, EmptyListObject);
    ht->Size = MakeFixnum(0);

    return(ht);
}

static FObject DoHashtableRef(FObject ht, FObject key, FEquivFn efn, FHashFn hfn)
{
    FAssert(HashtableP(ht));

    unsigned int idx = hfn(key) % (unsigned int) VectorLength(AsHashtable(ht)->Buckets);

    FObject node = AsVector(AsHashtable(ht)->Buckets)->Vector[idx];

    while (node != EmptyListObject)
    {
        FAssert(PairP(node));
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

FObject HashtableStringRef(FObject ht, FCh * s, int sl, FObject def)
{
    FAssert(HashtableP(ht));

    unsigned int idx = StringLengthHash(s, sl)
            % (unsigned int) VectorLength(AsHashtable(ht)->Buckets);
    FObject node = AsVector(AsHashtable(ht)->Buckets)->Vector[idx];

    while (node != EmptyListObject)
    {
        FAssert(PairP(node));
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
        unsigned int idx = hfn(key) % (unsigned int) VectorLength(AsHashtable(ht)->Buckets);

//        AsVector(AsHashtable(ht)->Buckets)->Vector[idx] =
//                MakePair(MakePair(key, val),
//                AsVector(AsHashtable(ht)->Buckets)->Vector[idx]);
        ModifyVector(AsHashtable(ht)->Buckets, idx,
                MakePair(MakePair(key, val), AsVector(AsHashtable(ht)->Buckets)->Vector[idx]));

//        AsHashtable(ht)->Size = MakeFixnum(AsFixnum(AsHashtable(ht)->Size) + 1);
        Modify(FHashtable, ht, Size, MakeFixnum(AsFixnum(AsHashtable(ht)->Size) + 1));
    }
}

void HashtableDelete(FObject ht, FObject key, FEquivFn efn, FHashFn hfn)
{
    FAssert(HashtableP(ht));

    unsigned int idx = hfn(key) % (unsigned int) VectorLength(AsHashtable(ht)->Buckets);

    FObject node = AsVector(AsHashtable(ht)->Buckets)->Vector[idx];
    FObject prev = NoValueObject;

    while (node != EmptyListObject)
    {
        FAssert(PairP(node));
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

int HashtableContainsP(FObject ht, FObject key, FEquivFn efn, FHashFn hfn)
{
    FAssert(HashtableP(ht));

    FObject node = DoHashtableRef(ht, key, efn, hfn);
    if (PairP(node))
        return(1);
    return(0);
}

unsigned int HashtableSize(FObject ht)
{
    FAssert(HashtableP(ht));
    FAssert(FixnumP(AsHashtable(ht)->Size));

    return(AsFixnum(AsHashtable(ht)->Size));
}

void HashtableWalkUpdate(FObject ht, FWalkUpdateFn wfn, FObject ctx)
{
    FAssert(HashtableP(ht));

    FObject bkts = AsHashtable(ht)->Buckets;
    int len = VectorLength(bkts);

    for (int idx = 0; idx < len; idx++)
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
    int len = VectorLength(bkts);

    for (int idx = 0; idx < len; idx++)
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
    int len = VectorLength(bkts);

    for (int idx = 0; idx < len; idx++)
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

// ---- Symbols ----

static unsigned int NextSymbolHash = 0;

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

FObject StringCToSymbol(char * s)
{
    return(StringToSymbol(MakeStringC(s)));
}

FObject StringLengthToSymbol(FCh * s, int sl)
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
    unsigned int sdx;
    for (sdx = 0; sdx < StringLength(str); sdx++)
        AsString(nstr)->String[sdx] = AsString(str)->String[sdx];

    for (unsigned int idx = 0; idx < StringLength(AsSymbol(sym)->String); idx++)
        AsString(nstr)->String[sdx + idx] = AsString(AsSymbol(sym)->String)->String[idx];

    return(StringToSymbol(nstr));
}

// ---- Record Types ----

FObject MakeRecordType(FObject nam, unsigned int nf, FObject flds[])
{
    FAssert(SymbolP(nam));

    FRecordType * rt = (FRecordType *) MakeObject(sizeof(FRecordType) + sizeof(FObject) * nf,
            RecordTypeTag);
    rt->NumFields = MakeLength(nf + 1, RecordTypeTag);
    rt->Fields[0] = nam;

    for (unsigned int fdx = 1; fdx < nf + 1; fdx++)
    {
        FAssert(SymbolP(flds[fdx - 1]));

        rt->Fields[fdx] = flds[fdx - 1];
    }

    return(rt);
}

FObject MakeRecordTypeC(char * nam, unsigned int nf, char * flds[])
{
    FObject oflds[32];

    FAssert(nf <= sizeof(oflds) / sizeof(FObject));

    for (unsigned int fdx = 0; fdx < nf; fdx++)
        oflds[fdx] = StringCToSymbol(flds[fdx]);

    return(MakeRecordType(StringCToSymbol(nam), nf, oflds));
}

// ---- Records ----

FObject MakeRecord(FObject rt)
{
    FAssert(RecordTypeP(rt));

    unsigned int nf = RecordTypeNumFields(rt);

    FGenericRecord * r = (FGenericRecord *) MakeObject(
            sizeof(FGenericRecord) + sizeof(FObject) * nf, RecordTag);
    r->NumFields = MakeLength(nf + 1, RecordTag);
    r->Fields[0] = rt;

    for (unsigned int fdx = 1; fdx <= nf; fdx++)
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

// ---- Exception ----

static char * ExceptionFieldsC[] = {"type", "who", "message", "irritants"};

FObject MakeException(FObject typ, FObject who, FObject msg, FObject lst)
{
    FAssert(sizeof(FException) == sizeof(ExceptionFieldsC) + sizeof(FRecord));

    FException * exc = (FException *) MakeRecord(R.ExceptionRecordType);
    exc->Type = typ;
    exc->Who = who;
    exc->Message = msg;
    exc->Irritants = lst;

    return(exc);
}

void RaiseException(FObject typ, FObject who, FObject msg, FObject lst)
{
    Raise(MakeException(typ, who, msg, lst));
}

void RaiseExceptionC(FObject typ, char * who, char * msg, FObject lst)
{
    Raise(MakeException(typ, StringCToSymbol(who), MakeStringC(msg), lst));
}

void Raise(FObject obj)
{
    throw obj;
}

Define("error", ErrorPrimitive)(int argc, FObject argv[])
{
    if (argc < 1)
        RaiseExceptionC(R.Assertion, "error", "error: expected at least one argument",
                EmptyListObject);

    if (StringP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "error", "error: expected a string", List(argv[0]));

    FObject lst = EmptyListObject;
    while (argc > 1)
    {
        argc -= 1;
        lst = MakePair(argv[argc], lst);
    }

    throw MakeException(R.Assertion, StringCToSymbol("error"), argv[0], lst);

    return(NoValueObject);
}

Define("full-error", FullErrorPrimitive)(int argc, FObject argv[])
{
    if (argc < 3)
        RaiseExceptionC(R.Assertion, "full-error",
                "full-error: expected at least three arguments", EmptyListObject);

    if (SymbolP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "full-error", "full-error: expected a symbol",
                List(argv[0]));

    if (SymbolP(argv[1]) == 0)
        RaiseExceptionC(R.Assertion, "full-error", "full-error: expected a symbol",
                List(argv[1]));

    if (StringP(argv[2]) == 0)
        RaiseExceptionC(R.Assertion, "full-error", "full-error: expected a string",
                List(argv[2]));

    FObject lst = EmptyListObject;
    while (argc > 3)
    {
        argc -= 1;
        lst = MakePair(argv[argc], lst);
    }

    throw MakeException(argv[0], argv[1], argv[2], lst);

    return(NoValueObject);
}

// ----------------

int EqvP(FObject obj1, FObject obj2)
{
    if (obj1 == obj2)
        return(1);

    return(0);
}

int EqP(FObject obj1, FObject obj2)
{
    if (obj1 == obj2)
        return(1);

    return(0);
}

int EqualP(FObject obj1, FObject obj2)
{
    if (EqvP(obj1, obj2))
        return(1);

    if (PairP(obj1))
    {
        if (PairP(obj2) == 0)
            return(0);

        if (EqualP(First(obj1), First(obj2)) == 0
                || EqualP(Rest(obj1), Rest(obj2)) == 0)
            return(0);
        return(1);
    }

    if (BoxP(obj1))
    {
        if (BoxP(obj2) == 0)
            return(0);

        return(EqualP(Unbox(obj1), Unbox(obj2)));
    }

    if (StringP(obj1))
    {
        if (StringP(obj2) == 0)
            return(0);

        return(StringEqualP(obj1, obj2));
    }

    if (VectorP(obj1))
    {
        if (VectorP(obj2) == 0)
            return(0);

        if (VectorLength(obj1) != VectorLength(obj2))
            return(0);

        for (unsigned int idx = 0; idx < VectorLength(obj1); idx++)
            if (EqualP(AsVector(obj1)->Vector[idx], AsVector(obj2)->Vector[idx]) == 0)
                return(0);
        return(1);
    }

    if (BytevectorP(obj1))
    {
        if (BytevectorP(obj2) == 0)
            return(0);

        if (BytevectorLength(obj1) != BytevectorLength(obj2))
            return(0);

        for (unsigned int idx = 0; idx < BytevectorLength(obj1); idx++)
            if (AsBytevector(obj1)->Vector[idx] != AsBytevector(obj2)->Vector[idx])
                return(0);
        return(1);
    }

    return(0);
}

Define("eq?", EqPPrimitive)(int argc, FObject argv[])
{
    if (argc != 2)
        RaiseExceptionC(R.Assertion, "eq?", "eq?: expected two arguments", EmptyListObject);

    return(EqP(argv[0], argv[1]) ? TrueObject : FalseObject);
}

Define("eqv?", EqvPPrimitive)(int argc, FObject argv[])
{
    if (argc != 2)
        RaiseExceptionC(R.Assertion, "eqv?", "eqv?: expected two arguments", EmptyListObject);

    return(EqvP(argv[0], argv[1]) ? TrueObject : FalseObject);
}

Define("equal?", EqualPPrimitive)(int argc, FObject argv[])
{
    if (argc != 2)
        RaiseExceptionC(R.Assertion, "equal?", "equal?: expected two arguments",
                EmptyListObject);

    return(EqualP(argv[0], argv[1]) ? TrueObject : FalseObject);
}

// System interface

Define("command-line", CommandLinePrimitive)(int argc, FObject argv[])
{
    if (argc != 0)
        RaiseExceptionC(R.Assertion, "command-line", "command-line: expected no arguments",
                EmptyListObject);

    return(R.CommandLine);
}

// Foment specific

Define("loaded-libraries", LoadedLibrariesPrimitive)(int argc, FObject argv[])
{
    if (argc != 0)
        RaiseExceptionC(R.Assertion, "loaded-libraries",
                "loaded-libraries: expected no arguments", EmptyListObject);

    return(R.LoadedLibraries);
}

Define("library-path", LibraryPathPrimitive)(int argc, FObject argv[])
{
    if (argc != 0)
        RaiseExceptionC(R.Assertion, "library-path", "library-path: expected no arguments",
                EmptyListObject);

    return(R.LibraryPath);
}

Define("full-command-line", FullCommandLinePrimitive)(int argc, FObject argv[])
{
    if (argc != 0)
        RaiseExceptionC(R.Assertion, "full-command-line",
                "full-command-line: expected no arguments", EmptyListObject);

    return(R.FullCommandLine);
}

// ---- Primitives ----

static FPrimitive * Primitives[] =
{
    &BooleanPPrimitive,
    &NotPrimitive,
    &EqHashPrimitive,
    &EqvHashPrimitive,
    &EqualHashPrimitive,
    &ErrorPrimitive,
    &FullErrorPrimitive,
    &EqPPrimitive,
    &EqvPPrimitive,
    &EqualPPrimitive,
    &CommandLinePrimitive,
    &LoadedLibrariesPrimitive,
    &LibraryPathPrimitive,
    &FullCommandLinePrimitive
};

// ----------------

extern char StartupCode[];

static void SetupScheme()
{
    Eval(ReadStringC(
        "(define-syntax and"
            "(syntax-rules ()"
                "((and) #t)"
                "((and test) test)"
                "((and test1 test2 ...) (if test1 (and test2 ...) #f))))", 1), R.Bedrock);

    LibraryExport(R.BedrockLibrary, EnvironmentLookup(R.Bedrock, StringCToSymbol("and")));

    FObject port = MakeStringCInputPort(StartupCode);
    PushRoot(&port);

    for (;;)
    {
        FObject obj = Read(port, 1, 0);

        if (obj == EndOfFileObject)
            break;
        Eval(obj, R.Bedrock);
    }

    PopRoot();
}

static char * FeaturesC[] =
{
    "r7rs",
#ifdef FOMENT_WIN32
    "windows",
#endif // FOMENT_WIN32
    "i386",
    "ilp32",
    "little-endian",
    "foment",
    "foment-0.1"
};

FObject MakeCommandLine(int argc, char * argv[])
{
    FObject cl = EmptyListObject;

    while (argc > 0)
    {
        argc -= 1;
        cl = MakePair(MakeStringC(argv[argc]), cl);
    }

    return(cl);
}

void SetupFoment(int argc, char * argv[])
{
    SetupGC();

    FObject * rv = (FObject *) &R;
    for (int rdx = 0; rdx < sizeof(FRoots) / sizeof(FObject); rdx++)
        rv[rdx] = NoValueObject;

    FAssert(R.HashtableRecordType == NoValueObject);
    R.SymbolHashtable = MakeObject(sizeof(FHashtable), RecordTag);

    AsHashtable(R.SymbolHashtable)->Record.NumFields = RecordTag;
    AsHashtable(R.SymbolHashtable)->Record.RecordType = R.HashtableRecordType;
    AsHashtable(R.SymbolHashtable)->Buckets = MakeVector(941, 0, EmptyListObject);
    AsHashtable(R.SymbolHashtable)->Size = MakeFixnum(0);

    FAssert(HashtableP(R.SymbolHashtable));

    R.HashtableRecordType = MakeRecordTypeC("hashtable",
            sizeof(HashtableFieldsC) / sizeof(char *), HashtableFieldsC);
    AsHashtable(R.SymbolHashtable)->Record.RecordType = R.HashtableRecordType;
    AsHashtable(R.SymbolHashtable)->Record.NumFields =
            MakeLength(RecordTypeNumFields(R.HashtableRecordType), RecordTag);

    FAssert(HashtableP(R.SymbolHashtable));

    SetupLibrary();
    R.ExceptionRecordType = MakeRecordTypeC("exception",
            sizeof(ExceptionFieldsC) / sizeof(char *), ExceptionFieldsC);
    R.EllipsisSymbol = StringCToSymbol("...");
    R.Assertion = StringCToSymbol("assertion-violation");
    R.Restriction = StringCToSymbol("implementation-restriction");
    R.Lexical = StringCToSymbol("lexical-violation");
    R.Syntax = StringCToSymbol("syntax-violation");
    R.Error = StringCToSymbol("error-violation");

    FObject nam = List(StringCToSymbol("foment"), StringCToSymbol("bedrock"));
    R.Bedrock = MakeEnvironment(nam, FalseObject);
    R.LoadedLibraries = EmptyListObject;
    R.BedrockLibrary = MakeLibrary(nam);

    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);

    for (int n = 0; n < sizeof(SpecialSyntaxes) / sizeof(char *); n++)
        LibraryExport(R.BedrockLibrary, EnvironmentSetC(R.Bedrock, SpecialSyntaxes[n],
                MakeImmediate(n, SpecialSyntaxTag)));

    R.Features = EmptyListObject;

    for (int idx = 0; idx < sizeof(FeaturesC) / sizeof(char *); idx++)
        R.Features = MakePair(StringCToSymbol(FeaturesC[idx]), R.Features);

    R.FullCommandLine = MakeCommandLine(argc, argv);
    R.CommandLine = R.FullCommandLine;
    R.LibraryPath = MakePair(MakeStringC("."), EmptyListObject);

    if (argc > 0)
    {
        char * s = strrchr(argv[0], PathCh);
        if (s != 0)
        {
            *s = 0;
            R.LibraryPath = MakePair(MakeStringC(argv[0]), R.LibraryPath);
            *s = PathCh;
        }
    }

    SetupPairs();
    SetupStrings();
    SetupVectors();
    SetupIO();
    SetupCompile();
    SetupExecute();
    SetupNumbers();
    SetupMM();
    SetupScheme();

    LibraryExport(R.BedrockLibrary,
            EnvironmentSetC(R.Bedrock, "standard-input", R.StandardInput));
    LibraryExport(R.BedrockLibrary,
            EnvironmentSetC(R.Bedrock, "standard-output", R.StandardOutput));
    LibraryExport(R.BedrockLibrary,
            EnvironmentSetC(R.Bedrock, "symbol-hashtable", R.SymbolHashtable));
}
