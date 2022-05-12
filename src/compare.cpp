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
#include "unicode.hpp"

// ---- Comparator ----

Define("no-ordering-predicate", NoOrderingPredicatePrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("no-ordering-predicate", argc);

    RaiseExceptionC(Assertion, "no-ordering-predicate", "no ordering predicate available",
            EmptyListObject);
    return(NoValueObject);
}

Define("no-hash-function", NoHashFunctionPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("no-hash-function", argc);

    RaiseExceptionC(Assertion, "no-hash-function", "no hash function available", EmptyListObject);
    return(NoValueObject);
}

// ---- Comparator ----

static FObject MakeComparator(FObject ttp, FObject eqp, FObject orderp, FObject hashfn)
{
    FComparator * comp = (FComparator *) MakeObject(ComparatorTag, sizeof(FComparator), 5,
            "make-comparator");
    comp->TypeTestP = ttp;
    comp->EqualityP = eqp;
    comp->OrderingP = (orderp == FalseObject ? NoOrderingPredicatePrimitive : orderp);
    comp->HashFn = (hashfn == FalseObject ? NoHashFunctionPrimitive : hashfn);
    comp->Context = NoValueObject;

    return(comp);
}

Define("make-comparator", MakeComparatorPrimitive)(long_t argc, FObject argv[])
{
    FourArgsCheck("make-comparator", argc);

    ProcedureArgCheck("make-comparator", argv[0]);
    ProcedureArgCheck("make-comparator", argv[1]);
    if (argv[2] != FalseObject)
        ProcedureArgCheck("make-comparator", argv[2]);
    if (argv[3] != FalseObject)
        ProcedureArgCheck("make-comparator", argv[3]);

    return(MakeComparator(argv[0], argv[1], argv[2], argv[3]));
}

Define("comparator?", ComparatorPPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("comparator?", argc);

    return(ComparatorP(argv[0]) ? TrueObject : FalseObject);
}

Define("comparator-type-test-predicate", ComparatorTypeTestPredicatePrimitive)(long_t argc,
    FObject argv[])
{
    OneArgCheck("comparator-type-test-predicate", argc);
    ComparatorArgCheck("comparator-type-test-predicate", argv[0]);

    return(AsComparator(argv[0])->TypeTestP);
}

Define("comparator-equality-predicate", ComparatorEqualityPredicatePrimitive)(long_t argc,
    FObject argv[])
{
    OneArgCheck("comparator-equality-predicate", argc);
    ComparatorArgCheck("comparator-equality-predicate", argv[0]);

    return(AsComparator(argv[0])->EqualityP);
}

Define("comparator-ordering-predicate", ComparatorOrderingPredicatePrimitive)(long_t argc,
    FObject argv[])
{
    OneArgCheck("comparator-ordering-predicate", argc);
    ComparatorArgCheck("comparator-ordering-predicate", argv[0]);

    return(AsComparator(argv[0])->OrderingP);
}

Define("comparator-hash-function", ComparatorHashFunctionPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("comparator-hash-function", argc);
    ComparatorArgCheck("comparator-hash-function", argv[0]);

    return(AsComparator(argv[0])->HashFn);
}

Define("comparator-ordered?", ComparatorOrderedPPrimitive)(long_t argc,
    FObject argv[])
{
    OneArgCheck("comparator-ordered?", argc);
    ComparatorArgCheck("comparator-ordered?", argv[0]);

    return(AsComparator(argv[0])->OrderingP == NoOrderingPredicatePrimitive ? FalseObject :
            TrueObject);
}

Define("comparator-hashable?", ComparatorHashablePPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("comparator-hashable?", argc);
    ComparatorArgCheck("comparator-hashable?", argv[0]);

    return(AsComparator(argv[0])->HashFn == NoHashFunctionPrimitive ? FalseObject : TrueObject);
}

Define("comparator-context", ComparatorContextPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("comparator-context", argc);
    ComparatorArgCheck("comparator-context", argv[0]);

    return(AsComparator(argv[0])->Context);
}

Define("comparator-context-set!", ComparatorContextSetPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("comparator-context-set!", argc);
    ComparatorArgCheck("comparator-context-set!", argv[0]);

    AsComparator(argv[0])->Context = argv[1];
    return(NoValueObject);
}

// ---- Equivalence predicates ----

long_t EqvP(FObject obj1, FObject obj2)
{
    if (obj1 == obj2)
        return(1);

    return(GenericEqvP(obj1, obj2));
}

long_t EqP(FObject obj1, FObject obj2)
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

static long_t EqualPUnionFind(FObject htbl, FObject objx, FObject objy)
{
    FObject bx = HashTableRef(htbl, objx, FalseObject);
    FObject by = HashTableRef(htbl, objy, FalseObject);

    if (bx == FalseObject)
    {
        if (by == FalseObject)
        {
            FObject nb = MakeBox(MakeFixnum(1));
            HashTableSet(htbl, objx, nb);
            HashTableSet(htbl, objy, nb);
        }
        else
        {
            FAssert(BoxP(by));

            HashTableSet(htbl, objx, EqualPFind(by));
        }
    }
    else
    {
        FAssert(BoxP(bx));

        if (by == FalseObject)
            HashTableSet(htbl, objy, EqualPFind(bx));
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

            long_t nx = AsFixnum(Unbox(rx));
            long_t ny = AsFixnum(Unbox(ry));

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

static long_t EqualP(FObject htbl, FObject obj1, FObject obj2)
{
    if (EqvP(obj1, obj2))
        return(1);

    if (PairP(obj1))
    {
        if (PairP(obj2) == 0)
            return(0);

        if (EqualPUnionFind(htbl, obj1, obj2))
            return(1);

        if (EqualP(htbl, First(obj1), First(obj2)) && EqualP(htbl, Rest(obj1), Rest(obj2)))
            return(1);

        return(0);
    }

    if (BoxP(obj1))
    {
        if (BoxP(obj2) == 0)
            return(0);

        if (EqualPUnionFind(htbl, obj1, obj2))
            return(1);

        return(EqualP(htbl, Unbox(obj1), Unbox(obj2)));
    }

    if (VectorP(obj1))
    {
        if (VectorP(obj2) == 0)
            return(0);

        if (VectorLength(obj1) != VectorLength(obj2))
            return(0);

        if (EqualPUnionFind(htbl, obj1, obj2))
            return(1);

        for (ulong_t idx = 0; idx < VectorLength(obj1); idx++)
            if (EqualP(htbl, AsVector(obj1)->Vector[idx], AsVector(obj2)->Vector[idx]) == 0)
                return(0);

        return(1);
    }

    if (StringP(obj1))
    {
        if (StringP(obj2) == 0)
            return(0);

        return(StringCompare(obj1, obj2) == 0);
    }

    if (BytevectorP(obj1))
    {
        if (BytevectorP(obj2) == 0)
            return(0);

        if (BytevectorLength(obj1) != BytevectorLength(obj2))
            return(0);

        for (ulong_t idx = 0; idx < BytevectorLength(obj1); idx++)
            if (AsBytevector(obj1)->Vector[idx] != AsBytevector(obj2)->Vector[idx])
                return(0);
        return(1);
    }

    return(0);
}

long_t EqualP(FObject obj1, FObject obj2)
{
    return(EqualP(MakeEqHashTable(128, 0), obj1, obj2));
}

Define("eqv?", EqvPPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("eqv?", argc);

    return(EqvP(argv[0], argv[1]) ? TrueObject : FalseObject);
}

Define("eq?", EqPPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("eq?", argc);

    return(EqP(argv[0], argv[1]) ? TrueObject : FalseObject);
}

Define("equal?", EqualPPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("equal?", argc);

    return(EqualP(argv[0], argv[1]) ? TrueObject : FalseObject);
}

// ---- Hashing ----

inline ulong_t HashBound()
{
    FAssert(FixnumP(Parameter(PARAMETER_HASH_BOUND)));

    return(AsFixnum(Parameter(PARAMETER_HASH_BOUND)));
}

inline ulong_t HashSalt()
{
    FAssert(FixnumP(Parameter(PARAMETER_HASH_SALT)));

    return(AsFixnum(Parameter(PARAMETER_HASH_SALT)));
}

Define("%check-hash-bound", CheckHashBoundPrimitive)(long_t argc, FObject argv[])
{
    FMustBe(argc == 1);
    NonNegativeArgCheck("%check-hash-bound", argv[0], 1);

    if (BignumP(argv[0]))
        return(MakeFixnum(MAXIMUM_FIXNUM));

    FAssert(FixnumP(argv[0]));

    return(argv[0]);
}

Define("%check-hash-salt", CheckHashSaltPrimitive)(long_t argc, FObject argv[])
{
    FMustBe(argc == 1);
    NonNegativeArgCheck("%check-hash-salt", argv[0], 1);

    if (BignumP(argv[0]))
        return(MakeFixnum(MAXIMUM_FIXNUM));

    FAssert(FixnumP(argv[0]));

    return(argv[0]);
}

Define("boolean-hash", BooleanHashPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("boolean-hash", argc);
    BooleanArgCheck("boolean-hash", argv[0]);

    return(MakeFixnum(argv[0] == FalseObject ? 0 : HashSalt() % HashBound()));
}

Define("char-hash", CharHashPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("char-hash", argc);
    CharacterArgCheck("char-hash", argv[0]);

    return(MakeFixnum((AsCharacter(argv[0]) * HashSalt()) % HashBound()));
}

Define("char-ci-hash", CharCiHashPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("char-ci-hash", argc);
    CharacterArgCheck("char-ci-hash", argv[0]);

    return(MakeFixnum((CharFoldcase(AsCharacter(argv[0])) * HashSalt()) % HashBound()));
}

Define("string-hash", StringHashPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("string-hash", argc);
    StringArgCheck("string-hash", argv[0]);

    return(MakeFixnum((StringHash(argv[0]) * HashSalt()) % HashBound()));
}

Define("string-ci-hash", StringCiHashPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("string-ci-hash", argc);
    StringArgCheck("string-ci-hash", argv[0]);

    return(MakeFixnum((StringCiHash(argv[0]) * HashSalt()) % HashBound()));
}

Define("symbol-hash", SymbolHashPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("symbol-hash", argc);
    SymbolArgCheck("symbol-hash", argv[0]);

    return(MakeFixnum((SymbolHash(argv[0]) * HashSalt()) % HashBound()));
}

Define("number-hash", NumberHashPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("number-hash", argc);
    NumberArgCheck("number-hash", argv[0]);

    return(MakeFixnum((NumberHash(argv[0]) * HashSalt()) % HashBound()));
}

uint32_t EqHash(FObject obj)
{
    return(NormalizeHash(((ulong_t) obj) >> 3));
}

Define("eq-hash", EqHashPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("eq-hash", argc);

    // Do not add HashSalt and HashBound because EqHash is used internally by EqHashTable*.

    return(MakeFixnum(EqHash(argv[0])));
}

// ---- Primitives ----

static FObject Primitives[] =
{
    MakeComparatorPrimitive,
    ComparatorPPrimitive,
    ComparatorTypeTestPredicatePrimitive,
    ComparatorEqualityPredicatePrimitive,
    ComparatorOrderingPredicatePrimitive,
    ComparatorHashFunctionPrimitive,
    ComparatorOrderedPPrimitive,
    ComparatorHashablePPrimitive,
    ComparatorContextPrimitive,
    ComparatorContextSetPrimitive,
    EqvPPrimitive,
    EqPPrimitive,
    EqualPPrimitive,
    CheckHashBoundPrimitive,
    CheckHashSaltPrimitive,
    BooleanHashPrimitive,
    CharHashPrimitive,
    CharCiHashPrimitive,
    StringHashPrimitive,
    StringCiHashPrimitive,
    SymbolHashPrimitive,
    NumberHashPrimitive,
    EqHashPrimitive
};

void SetupCompare()
{
    for (ulong_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(Bedrock, BedrockLibrary, Primitives[idx]);
}
