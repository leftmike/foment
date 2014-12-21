/*

Foment

*/

#include "foment.hpp"

// ---- Comparator ----

static const char * ComparatorFieldsC[] = {"type-test-procedure", "equality-predicate",
    "comparison-procedure", "hash-function"};

FObject MakeComparator(FObject ttfn, FObject eqfn, FObject compfn, FObject hashfn)
{
    FAssert(sizeof(FComparator) == sizeof(ComparatorFieldsC) + sizeof(FRecord));

    FComparator * comp = (FComparator *) MakeRecord(R.ComparatorRecordType);
    comp->TypeTestFn = ttfn;
    comp->EqualityFn = eqfn;
    comp->ComparisonFn = compfn;
    comp->HashFn = hashfn;

    return(comp);
}

Define("make-comparator", MakeComparatorPrimitive)(int_t argc, FObject argv[])
{
    FourArgsCheck("make-comparator", argc);

    return(MakeComparator(argv[0], argv[1], argv[2], argv[3]));
}

Define("comparator?", ComparatorPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("comparator?", argc);

    return(ComparatorP(argv[0]) ? TrueObject : FalseObject);
}

Define("comparator-type-test-procedure", ComparatorTypeTestProcedurePrimitive)(int_t argc,
    FObject argv[])
{
    OneArgCheck("comparator-type-test-procedure", argc);
    ComparatorArgCheck("comparator-type-test-procedure", argv[0]);

    return(AsComparator(argv[0])->TypeTestFn);
}

Define("comparator-equality-predicate", ComparatorEqualityPredicatePrimitive)(int_t argc,
    FObject argv[])
{
    OneArgCheck("comparator-equality-predicate", argc);
    ComparatorArgCheck("comparator-equality-predicate", argv[0]);

    return(AsComparator(argv[0])->EqualityFn);
}

Define("comparator-comparison-procedure", ComparatorComparisonProcedurePrimitive)(int_t argc,
    FObject argv[])
{
    OneArgCheck("comparator-comparison-procedure", argc);
    ComparatorArgCheck("comparator-comparison-procedure", argv[0]);

    return(AsComparator(argv[0])->ComparisonFn);
}

Define("comparator-hash-function", ComparatorHashFunctionPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("comparator-hash-function", argc);
    ComparatorArgCheck("comparator-hash-function", argv[0]);

    return(AsComparator(argv[0])->HashFn);
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
    FObject bx = EqHashMapRef(ht, objx, FalseObject);
    FObject by = EqHashMapRef(ht, objy, FalseObject);

    if (bx == FalseObject)
    {
        if (by == FalseObject)
        {
            FObject nb = MakeBox(MakeFixnum(1));
            EqHashMapSet(ht, objx, nb);
            EqHashMapSet(ht, objy, nb);
        }
        else
        {
            FAssert(BoxP(by));

            EqHashMapSet(ht, objx, EqualPFind(by));
        }
    }
    else
    {
        FAssert(BoxP(bx));

        if (by == FalseObject)
            EqHashMapSet(ht, objy, EqualPFind(bx));
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
    return(EqualP(MakeEqHashMap(), obj1, obj2));
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

// ---- Primitives ----

static FPrimitive * Primitives[] =
{
    &MakeComparatorPrimitive,
    &ComparatorPPrimitive,
    &ComparatorTypeTestProcedurePrimitive,
    &ComparatorEqualityPredicatePrimitive,
    &ComparatorComparisonProcedurePrimitive,
    &ComparatorHashFunctionPrimitive,

    &EqvPPrimitive,
    &EqPPrimitive,
    &EqualPPrimitive
};

void SetupCompare()
{
    R.ComparatorRecordType = MakeRecordTypeC("comparator",
            sizeof(ComparatorFieldsC) / sizeof(char *), ComparatorFieldsC);

    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
