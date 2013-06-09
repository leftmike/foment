/*

Foment

*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "foment.hpp"

FConfig Config = {1, 1, 1, 0};

FObject Bedrock;
FObject BedrockLibrary;
FObject EllipsisSymbol;
FObject Features;
FObject CommandLine;
FObject FullCommandLine;
FObject LibraryPath;

FObject HashtableRecordType;
FObject ExceptionRecordType;

#ifdef FOMENT_DEBUG
void FAssertFailed(char * fn, int ln, char * expr)
{
    printf("FAssert: %s (%d)%s\n", expr, ln, fn);

    exit(1);
}
#endif // FOMENT_DEBUG

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
        RaiseExceptionC(Assertion, "boolean?", "boolean?: expected one argument", EmptyListObject);

    return(BooleanP(argv[0]) ? TrueObject : FalseObject);
}

Define("not", NotPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(Assertion, "not", "not: expected one argument", EmptyListObject);

    return(argv[0] == FalseObject ? TrueObject : FalseObject);
}

// ---- Boxes ----

FObject MakeBox(FObject val)
{
    FBox * bx = (FBox *) MakeObject(BoxTag, sizeof(FBox));
    bx->Value = val;

    FObject obj = AsObject(bx);
    FAssert(ObjectLength(obj) == sizeof(FBox));
    return(obj);
}

// ---- Hashtables ----

unsigned int EqHash(FObject obj)
{
    if (SymbolP(obj))
        return(AsFixnum(AsSymbol(obj)->Hash));
    return((unsigned int) obj);
}

unsigned int EqvHash(FObject obj)
{
    if (SymbolP(obj))
        return(AsFixnum(AsSymbol(obj)->Hash));
    return((unsigned int) obj);
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
        if (VectorLen(obj) == 0)
            return(1);

        h = 0;
        for (int idx = 0; idx < VectorLen(obj) && idx < MaxHashDepth; idx++)
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
        RaiseExceptionC(Assertion, "eq-hash", "eq-hash: expected one argument", EmptyListObject);

    return(MakeFixnum(EqHash(argv[0])));
}

Define("eqv-hash", EqvHashPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(Assertion, "eqv-hash", "eqv-hash: expected one argument", EmptyListObject);

    return(MakeFixnum(EqvHash(argv[0])));
}

Define("equal-hash", EqualHashPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(Assertion, "equal-hash", "equal-hash: expected one argument",
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

    FHashtable * ht = (FHashtable *) MakeRecord(HashtableRecordType);
    ht->Buckets = MakeVector(nb, 0, EmptyListObject);
    ht->Size = MakeFixnum(0);

    return(AsObject(ht));
}

static FObject DoHashtableRef(FObject ht, FObject key, FEquivFn efn, FHashFn hfn)
{
    FAssert(HashtableP(ht));

    unsigned int idx = hfn(key) % (unsigned int) VectorLen(AsHashtable(ht)->Buckets);

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
            % (unsigned int) VectorLen(AsHashtable(ht)->Buckets);
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
        Modify(FPair, First(node), Rest, val);
    }
    else
    {
        unsigned int idx = hfn(key) % (unsigned int) VectorLen(AsHashtable(ht)->Buckets);

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

    unsigned int idx = hfn(key) % (unsigned int) VectorLen(AsHashtable(ht)->Buckets);

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
                Modify(FPair, prev, Rest, Rest(node));
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
    int len = VectorLen(bkts);

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
                Modify(FPair, First(lst), Rest, val);
            }

            lst = Rest(lst);
        }
    }
}

void HashtableWalkDelete(FObject ht, FWalkDeleteFn wfn, FObject ctx)
{
    FAssert(HashtableP(ht));

    FObject bkts = AsHashtable(ht)->Buckets;
    int len = VectorLen(bkts);

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
                    Modify(FPair, prev, Rest, Rest(lst));
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
    int len = VectorLen(bkts);

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

static FObject SymbolHashtable;
static unsigned int NextSymbolHash = 1;

FObject StringToSymbol(FObject str)
{
    FAssert(StringP(str));

    FObject obj = HashtableRef(SymbolHashtable, str, FalseObject, StringEqualP, StringHash);
    if (obj == FalseObject)
    {
        FSymbol * sym = (FSymbol *) MakeObject(SymbolTag, sizeof(FSymbol));
        sym->String = str;
        sym->Hash = MakeFixnum(NextSymbolHash);
        NextSymbolHash += 1;

        obj = AsObject(sym);

        FAssert(ObjectLength(obj) == sizeof(FSymbol));

        HashtableSet(SymbolHashtable, str, obj, StringEqualP, StringHash);
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
    FObject obj = HashtableStringRef(SymbolHashtable, s, sl, FalseObject);
    if (obj == FalseObject)
    {
        FSymbol * sym = (FSymbol *) MakeObject(SymbolTag, sizeof(FSymbol));
        sym->String = MakeString(s, sl);
        sym->Hash = MakeFixnum(NextSymbolHash);
        NextSymbolHash += 1;

        obj = AsObject(sym);

        FAssert(ObjectLength(obj) == sizeof(FSymbol));

        HashtableSet(SymbolHashtable, sym->String, obj, StringEqualP, StringHash);
    }

    FAssert(SymbolP(obj));
    return(obj);
}

FObject PrefixSymbol(FObject str, FObject sym)
{
    FAssert(StringP(str));
    FAssert(SymbolP(sym));

    FObject nstr = MakeStringCh(AsString(str)->Length + AsString(AsSymbol(sym)->String)->Length,
            0);
    int sdx;
    for (sdx = 0; sdx < AsString(str)->Length; sdx++)
        AsString(nstr)->String[sdx] = AsString(str)->String[sdx];

    for (int idx = 0; idx < AsString(AsSymbol(sym)->String)->Length; idx++)
        AsString(nstr)->String[sdx + idx] = AsString(AsSymbol(sym)->String)->String[idx];

    return(StringToSymbol(nstr));
}

// ---- Record Types ----

FObject MakeRecordType(FObject nam, int nf, FObject flds[])
{
    FAssert(SymbolP(nam));

    FRecordType * rt = (FRecordType *) MakeObject(RecordTypeTag,
            sizeof(FRecordType) + sizeof(FObject) * (nf - 1));
    rt->Name = nam;
    rt->NumFields = nf;

    for (int fdx = 0; fdx < nf; fdx++)
    {
        FAssert(SymbolP(flds[fdx]));

        rt->Fields[fdx] = flds[fdx];
    }

    return(AsObject(rt));
}

FObject MakeRecordTypeC(char * nam, int nf, char * flds[])
{
    FObject oflds[32];

    FAssert(nf <= sizeof(oflds) / sizeof(FObject));

    for (int fdx = 0; fdx < nf; fdx++)
        oflds[fdx] = StringCToSymbol(flds[fdx]);

    return(MakeRecordType(StringCToSymbol(nam), nf, oflds));
}

// ---- Records ----

FObject MakeRecord(FObject rt)
{
    FAssert(RecordTypeP(rt));

    int nf = AsRecordType(rt)->NumFields;
    FGenericRecord * r = (FGenericRecord *) MakeObject(RecordTag,
            sizeof(FGenericRecord) + sizeof(FObject) * (nf - 1));
    r->Record.RecordType = rt;
    r->Record.NumFields = nf;

    for (int fdx = 0; fdx < nf; fdx++)
        r->Fields[fdx] = NoValueObject;

    return(AsObject(r));
}

// ---- Primitives ----

FObject MakePrimitive(FPrimitive * prim)
{
    FPrimitive * p = (FPrimitive *) MakeObject(PrimitiveTag, sizeof(FPrimitive));
    memcpy(p, prim, sizeof(FPrimitive));

    FObject obj = AsObject(p);
    FAssert(ObjectLength(obj) == sizeof(FPrimitive));
    return(obj);
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

    FException * exc = (FException *) MakeRecord(ExceptionRecordType);
    exc->Type = typ;
    exc->Who = who;
    exc->Message = msg;
    exc->Irritants = lst;

    return(AsObject(exc));
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

FObject Assertion;
FObject Restriction;
FObject Lexical;
FObject Syntax;
FObject Error;

Define("error", ErrorPrimitive)(int argc, FObject argv[])
{
    if (argc < 1)
        RaiseExceptionC(Assertion, "error", "error: expected at least one argument",
                EmptyListObject);

    if (StringP(argv[0]) == 0)
        RaiseExceptionC(Assertion, "error", "error: expected a string", List(argv[0]));

    FObject lst = EmptyListObject;
    while (argc > 1)
    {
        argc -= 1;
        lst = MakePair(argv[argc], lst);
    }

    throw MakeException(Assertion, StringCToSymbol("error"), argv[0], lst);

    return(NoValueObject);
}

Define("full-error", FullErrorPrimitive)(int argc, FObject argv[])
{
    if (argc < 3)
        RaiseExceptionC(Assertion, "full-error", "full-error: expected at least three arguments",
                EmptyListObject);

    if (SymbolP(argv[0]) == 0)
        RaiseExceptionC(Assertion, "full-error", "full-error: expected a symbol", List(argv[0]));

    if (SymbolP(argv[1]) == 0)
        RaiseExceptionC(Assertion, "full-error", "full-error: expected a symbol", List(argv[1]));

    if (StringP(argv[2]) == 0)
        RaiseExceptionC(Assertion, "full-error", "full-error: expected a string", List(argv[2]));

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

        if (VectorLen(obj1) != VectorLen(obj2))
            return(0);

        for (int idx = 0; idx < VectorLen(obj1); idx++)
            if (EqualP(AsVector(obj1)->Vector[idx], AsVector(obj2)->Vector[idx]) == 0)
                return(0);
        return(1);
    }

    if (BytevectorP(obj1))
    {
        if (BytevectorP(obj2) == 0)
            return(0);

        if (AsBytevector(obj1)->Length != AsBytevector(obj2)->Length)
            return(0);

        for (int idx = 0; idx < AsBytevector(obj1)->Length; idx++)
            if (AsBytevector(obj1)->Vector[idx] != AsBytevector(obj2)->Vector[idx])
                return(0);
        return(1);
    }

    return(0);
}

Define("eq?", EqPPrimitive)(int argc, FObject argv[])
{
    if (argc != 2)
        RaiseExceptionC(Assertion, "eq?", "eq?: expected two arguments", EmptyListObject);

    return(EqP(argv[0], argv[1]) ? TrueObject : FalseObject);
}

Define("eqv?", EqvPPrimitive)(int argc, FObject argv[])
{
    if (argc != 2)
        RaiseExceptionC(Assertion, "eqv?", "eqv?: expected two arguments", EmptyListObject);

    return(EqvP(argv[0], argv[1]) ? TrueObject : FalseObject);
}

Define("equal?", EqualPPrimitive)(int argc, FObject argv[])
{
    if (argc != 2)
        RaiseExceptionC(Assertion, "equal?", "equal?: expected two arguments", EmptyListObject);

    return(EqualP(argv[0], argv[1]) ? TrueObject : FalseObject);
}

// System interface

Define("command-line", CommandLinePrimitive)(int argc, FObject argv[])
{
    if (argc != 0)
        RaiseExceptionC(Assertion, "command-line", "command-line: expected no arguments",
                EmptyListObject);

    return(CommandLine);
}

// Foment specific

Define("loaded-libraries", LoadedLibrariesPrimitive)(int argc, FObject argv[])
{
    if (argc != 0)
        RaiseExceptionC(Assertion, "loaded-libraries", "loaded-libraries: expected no arguments",
                EmptyListObject);

    return(LoadedLibraries);
}

Define("library-path", LibraryPathPrimitive)(int argc, FObject argv[])
{
    if (argc != 0)
        RaiseExceptionC(Assertion, "library-path", "library-path: expected no arguments",
                EmptyListObject);

    return(LibraryPath);
}

Define("full-command-line", FullCommandLinePrimitive)(int argc, FObject argv[])
{
    if (argc != 0)
        RaiseExceptionC(Assertion, "full-command-line", "full-command-line: expected no arguments",
                EmptyListObject);

    return(FullCommandLine);
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
                "((and test1 test2 ...) (if test1 (and test2 ...) #f))))", 1), Bedrock);

    LibraryExport(BedrockLibrary, EnvironmentLookup(Bedrock, StringCToSymbol("and")));

    FObject port = MakeStringCInputPort(StartupCode);
    for (;;)
    {
        FObject obj = Read(port, 1, 0);

        if (obj == EndOfFileObject)
            break;
        Eval(obj, Bedrock);
    }
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

    HashtableRecordType = NoValueObject;
    Root(&HashtableRecordType);

    SymbolHashtable = MakeObject(RecordTag, sizeof(FHashtable));
    Root(&SymbolHashtable);

    AsHashtable(SymbolHashtable)->Record.RecordType = HashtableRecordType;
    AsHashtable(SymbolHashtable)->Buckets = MakeVector(23, 0, EmptyListObject);
    AsHashtable(SymbolHashtable)->Size = MakeFixnum(0);

    HashtableRecordType = MakeRecordTypeC("hashtable",
            sizeof(HashtableFieldsC) / sizeof(char *), HashtableFieldsC);
    AsHashtable(SymbolHashtable)->Record.RecordType = HashtableRecordType;
    AsHashtable(SymbolHashtable)->Record.NumFields = AsRecordType(HashtableRecordType)->NumFields;
    AsObject(SymbolHashtable);
    FAssert(ObjectLength(SymbolHashtable) == sizeof(FHashtable));

    SetupLibrary();
    ExceptionRecordType = MakeRecordTypeC("exception",
            sizeof(ExceptionFieldsC) / sizeof(char *), ExceptionFieldsC);
    Root(&ExceptionRecordType);

    EllipsisSymbol = StringCToSymbol("...");
    Root(&EllipsisSymbol);

    Assertion = StringCToSymbol("assertion-violation");
    Root(&Assertion);

    Restriction = StringCToSymbol("implementation-restriction");
    Root(&Restriction);

    Lexical = StringCToSymbol("lexical-violation");
    Root(&Lexical);

    Syntax = StringCToSymbol("syntax-violation");
    Root(&Syntax);

    Error = StringCToSymbol("error-violation");
    Root(&Error);

    FObject nam = List(StringCToSymbol("foment"), StringCToSymbol("bedrock"));
    Bedrock = MakeEnvironment(nam, FalseObject);
    Root(&Bedrock);

    LoadedLibraries = EmptyListObject;
    Root(&LoadedLibraries);

    BedrockLibrary = MakeLibrary(nam);
    Root(&BedrockLibrary);

    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(Bedrock, BedrockLibrary, Primitives[idx]);

    for (int n = 0; n < sizeof(SpecialSyntaxes) / sizeof(char *); n++)
        LibraryExport(BedrockLibrary, EnvironmentSetC(Bedrock, SpecialSyntaxes[n],
                MakeImmediate(n, SpecialSyntaxTag)));

    Features = EmptyListObject;
    Root(&Features);

    for (int idx = 0; idx < sizeof(FeaturesC) / sizeof(char *); idx++)
        Features = MakePair(StringCToSymbol(FeaturesC[idx]), Features);

    FullCommandLine = MakeCommandLine(argc, argv);
    Root(&FullCommandLine);

    CommandLine = FullCommandLine;
    Root(&CommandLine);

    LibraryPath = MakePair(MakeStringC("."), EmptyListObject);
    Root(&LibraryPath);

    if (argc > 0)
    {
        char * s = strrchr(argv[0], PathCh);
        if (s != 0)
        {
            *s = 0;
            LibraryPath = MakePair(MakeStringC(argv[0]), LibraryPath);
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
    SetupScheme();

    EnvironmentSetC(Bedrock, "standard-input", StandardInput);
    EnvironmentSetC(Bedrock, "standard-output", StandardOutput);
}
