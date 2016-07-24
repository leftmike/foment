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

// ---- Roots ----

FObject DefaultComparator = NoValueObject;
FObject EqComparator = NoValueObject;

// ---- Comparator ----

Define("no-ordering-predicate", NoOrderingPredicatePrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("no-ordering-predicate", argc);

    RaiseExceptionC(Assertion, "no-ordering-predicate", "no ordering predicate available",
            EmptyListObject);
    return(NoValueObject);
}

Define("no-hash-function", NoHashFunctionPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("no-hash-function", argc);

    RaiseExceptionC(Assertion, "no-hash-function", "no hash function available", EmptyListObject);
    return(NoValueObject);
}

// ---- Comparator ----

EternalBuiltinType(ComparatorType, "comparator", 0);

static FObject MakeComparator(FObject ttp, FObject eqp, FObject orderp, FObject hashfn)
{
    FComparator * comp = (FComparator *) MakeBuiltin(ComparatorType, sizeof(FComparator), 5,
            "make-comparator");
    comp->TypeTestP = ttp;
    comp->EqualityP = eqp;
    comp->OrderingP = (orderp == FalseObject ? NoOrderingPredicatePrimitive : orderp);
    comp->HashFn = (hashfn == FalseObject ? NoHashFunctionPrimitive : hashfn);
    comp->Context = NoValueObject;

    return(comp);
}

void DefineComparator(const char * nam, FObject ttp, FObject eqp, FObject orderp, FObject hashfn)
{
    LibraryExport(BedrockLibrary, EnvironmentSetC(Bedrock, nam,
            MakeComparator(ttp, eqp, orderp, hashfn)));
}

Define("make-comparator", MakeComparatorPrimitive)(int_t argc, FObject argv[])
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

Define("comparator?", ComparatorPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("comparator?", argc);

    return(ComparatorP(argv[0]) ? TrueObject : FalseObject);
}

Define("comparator-type-test-predicate", ComparatorTypeTestPredicatePrimitive)(int_t argc,
    FObject argv[])
{
    OneArgCheck("comparator-type-test-predicate", argc);
    ComparatorArgCheck("comparator-type-test-predicate", argv[0]);

    return(AsComparator(argv[0])->TypeTestP);
}

Define("comparator-equality-predicate", ComparatorEqualityPredicatePrimitive)(int_t argc,
    FObject argv[])
{
    OneArgCheck("comparator-equality-predicate", argc);
    ComparatorArgCheck("comparator-equality-predicate", argv[0]);

    return(AsComparator(argv[0])->EqualityP);
}

Define("comparator-ordering-predicate", ComparatorOrderingPredicatePrimitive)(int_t argc,
    FObject argv[])
{
    OneArgCheck("comparator-ordering-predicate", argc);
    ComparatorArgCheck("comparator-ordering-predicate", argv[0]);

    return(AsComparator(argv[0])->OrderingP);
}

Define("comparator-hash-function", ComparatorHashFunctionPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("comparator-hash-function", argc);
    ComparatorArgCheck("comparator-hash-function", argv[0]);

    return(AsComparator(argv[0])->HashFn);
}

Define("comparator-ordered?", ComparatorOrderedPPrimitive)(int_t argc,
    FObject argv[])
{
    OneArgCheck("comparator-ordered?", argc);
    ComparatorArgCheck("comparator-ordered?", argv[0]);

    return(AsComparator(argv[0])->OrderingP == NoOrderingPredicatePrimitive ? FalseObject :
            TrueObject);
}

Define("comparator-hashable?", ComparatorHashablePPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("comparator-hashable?", argc);
    ComparatorArgCheck("comparator-hashable?", argv[0]);

    return(AsComparator(argv[0])->HashFn == NoHashFunctionPrimitive ? FalseObject : TrueObject);
}

Define("comparator-context", ComparatorContextPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("comparator-context", argc);
    ComparatorArgCheck("comparator-context", argv[0]);

    return(AsComparator(argv[0])->Context);
}

Define("comparator-context-set!", ComparatorContextSetPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("comparator-context-set!", argc);
    ComparatorArgCheck("comparator-context-set!", argv[0]);

    AsComparator(argv[0])->Context = argv[1];
    return(NoValueObject);
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

uint_t EqHash(FObject obj)
{
    return(((uint_t) obj) >> 3);
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

static int_t EqualPUnionFind(FObject hm, FObject objx, FObject objy)
{
    FObject bx = EqHashMapRef(hm, objx, FalseObject);
    FObject by = EqHashMapRef(hm, objy, FalseObject);

    if (bx == FalseObject)
    {
        if (by == FalseObject)
        {
            FObject nb = MakeBox(MakeFixnum(1));
            EqHashMapSet(hm, objx, nb);
            EqHashMapSet(hm, objy, nb);
        }
        else
        {
            FAssert(BoxP(by));

            EqHashMapSet(hm, objx, EqualPFind(by));
        }
    }
    else
    {
        FAssert(BoxP(bx));

        if (by == FalseObject)
            EqHashMapSet(hm, objy, EqualPFind(bx));
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

static int_t EqualP(FObject hm, FObject obj1, FObject obj2)
{
    if (EqvP(obj1, obj2))
        return(1);

    if (PairP(obj1))
    {
        if (PairP(obj2) == 0)
            return(0);

        if (EqualPUnionFind(hm, obj1, obj2))
            return(1);

        if (EqualP(hm, First(obj1), First(obj2)) && EqualP(hm, Rest(obj1), Rest(obj2)))
            return(1);

        return(0);
    }

    if (BoxP(obj1))
    {
        if (BoxP(obj2) == 0)
            return(0);

        if (EqualPUnionFind(hm, obj1, obj2))
            return(1);

        return(EqualP(hm, Unbox(obj1), Unbox(obj2)));
    }

    if (VectorP(obj1))
    {
        if (VectorP(obj2) == 0)
            return(0);

        if (VectorLength(obj1) != VectorLength(obj2))
            return(0);

        if (EqualPUnionFind(hm, obj1, obj2))
            return(1);

        for (uint_t idx = 0; idx < VectorLength(obj1); idx++)
            if (EqualP(hm, AsVector(obj1)->Vector[idx], AsVector(obj2)->Vector[idx]) == 0)
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

Define("eq-hash", EqHashPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("eq-hash", argc);

    return(MakeFixnum(EqHash(argv[0]) & MAXIMUM_FIXNUM));
}

// ---- Hashing ----

inline uint_t HashBound()
{
    FAssert(PairP(IndexParameter(INDEX_PARAMETER_HASH_BOUND)));
    FAssert(FixnumP(First(IndexParameter(INDEX_PARAMETER_HASH_BOUND))));

    return(AsFixnum(First(IndexParameter(INDEX_PARAMETER_HASH_BOUND))));
}

inline uint_t HashSalt()
{
    FAssert(PairP(IndexParameter(INDEX_PARAMETER_HASH_SALT)));
    FAssert(FixnumP(First(IndexParameter(INDEX_PARAMETER_HASH_SALT))));

    return(AsFixnum(First(IndexParameter(INDEX_PARAMETER_HASH_SALT))));
}

Define("%check-hash-bound", CheckHashBoundPrimitive)(int_t argc, FObject argv[])
{
    FMustBe(argc == 1);
    NonNegativeArgCheck("%check-hash-bound", argv[0], 1);

    if (BignumP(argv[0]))
        return(MakeFixnum(MAXIMUM_FIXNUM));

    FAssert(FixnumP(argv[0]));

    return(argv[0]);
}

Define("%check-hash-salt", CheckHashSaltPrimitive)(int_t argc, FObject argv[])
{
    FMustBe(argc == 1);
    NonNegativeArgCheck("%check-hash-salt", argv[0], 1);

    if (BignumP(argv[0]))
        return(MakeFixnum(MAXIMUM_FIXNUM));

    FAssert(FixnumP(argv[0]));

    return(argv[0]);
}

Define("boolean-hash", BooleanHashPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("boolean-hash", argc);
    BooleanArgCheck("boolean-hash", argv[0]);

    return(MakeFixnum(argv[0] == FalseObject ? 0 : HashSalt() % HashBound()));
}

Define("char-hash", CharHashPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("char-hash", argc);
    CharacterArgCheck("char-hash", argv[0]);

    return(MakeFixnum((AsCharacter(argv[0]) * HashSalt()) % HashBound()));
}

Define("char-ci-hash", CharCiHashPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("char-ci-hash", argc);
    CharacterArgCheck("char-ci-hash", argv[0]);

    return(MakeFixnum((CharFoldcase(AsCharacter(argv[0])) * HashSalt()) % HashBound()));
}

Define("string-hash", StringHashPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("string-hash", argc);
    StringArgCheck("string-hash", argv[0]);

    return(MakeFixnum((StringHash(argv[0]) * HashSalt()) % HashBound()));
}

Define("string-ci-hash", StringCiHashPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("string-ci-hash", argc);
    StringArgCheck("string-ci-hash", argv[0]);

    return(MakeFixnum((StringCiHash(argv[0]) * HashSalt()) % HashBound()));
}

Define("symbol-hash", SymbolHashPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("symbol-hash", argc);
    SymbolArgCheck("symbol-hash", argv[0]);

    return(MakeFixnum((SymbolHash(argv[0]) * HashSalt()) % HashBound()));
}

Define("number-hash", NumberHashPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("number-hash", argc);
    NumberArgCheck("number-hash", argv[0]);

    return(MakeFixnum((NumberHash(argv[0]) * HashSalt()) % HashBound()));
}

// ---- Default Comparator ----

typedef enum
{
    EmptyListOrder,
    PairOrder,
    BooleanOrder,
    CharacterOrder,
    StringOrder,
    SymbolOrder,
    NumberOrder,
    VectorOrder,
    BytevectorOrder,
    RecordTypeOrder,
    RecordOrder,
    HashTreeOrder,
    BoxOrder,
    UnknownOrder
} FCompareOrder;

static FCompareOrder CompareOrder(FObject obj)
{
    if (obj == EmptyListObject)
        return(EmptyListOrder);
    if (PairP(obj))
        return(PairOrder);
    if (BooleanP(obj))
        return(BooleanOrder);
    if (CharacterP(obj))
        return(CharacterOrder);
    if (StringP(obj))
        return(StringOrder);
    if (SymbolP(obj))
        return(SymbolOrder);
    if (NumberP(obj))
        return(NumberOrder);
    if (VectorP(obj))
        return(VectorOrder);
    if (BytevectorP(obj))
        return(BytevectorOrder);
    if (RecordTypeP(obj))
        return(RecordTypeOrder);
    if (GenericRecordP(obj))
        return(RecordOrder);
    if (HashTreeP(obj))
        return(HashTreeOrder);
    if (BoxP(obj))
        return(BoxOrder);
    return(UnknownOrder);
}

static int_t DefaultCompare(FObject obj1, FObject obj2)
{
    if (obj1 == obj2)
        return(0);

    FCompareOrder ord1 = CompareOrder(obj1);
    FCompareOrder ord2 = CompareOrder(obj2);

    if (ord1 < ord2)
        return(-1);
    if (ord1 > ord2)
        return(1);

    switch (ord1)
    {
    case EmptyListOrder:
        FAssert(0);
        break;

    case PairOrder:
    {
        FAssert(PairP(obj1));
        FAssert(PairP(obj2));

        int_t ret = DefaultCompare(First(obj1), First(obj2));
        if (ret == 0)
            ret = DefaultCompare(Rest(obj1), Rest(obj2));
        return(ret);
    }

    case BooleanOrder:
    case CharacterOrder:
        return(obj1 < obj2 ? -1 : 1);

    case StringOrder:
        return(StringCompare(obj1, obj2));

    case SymbolOrder:
        if (obj1 == obj2)
            return(0);
        return(SymbolCompare(obj1, obj2));

    case NumberOrder:
        return(NumberCompare(obj1, obj2));

    case VectorOrder:
        FAssert(VectorP(obj1));
        FAssert(VectorP(obj2));

        if (VectorLength(obj1) != VectorLength(obj2))
            return(VectorLength(obj1) < VectorLength(obj2) ? -1 : 1);

        for (uint_t vdx = 0; vdx < VectorLength(obj1); vdx++)
        {
            int_t ret = DefaultCompare(AsVector(obj1)->Vector[vdx], AsVector(obj2)->Vector[vdx]);
            if (ret != 0)
                return(ret);
        }

        return(0);

    case BytevectorOrder:
        return(BytevectorCompare(obj1, obj2));

    case RecordTypeOrder:
        FAssert(RecordTypeP(obj1));
        FAssert(RecordTypeP(obj2));

        if (RecordTypeNumFields(obj1) != RecordTypeNumFields(obj2))
            return(RecordTypeNumFields(obj1) < RecordTypeNumFields(obj2) ? -1 : 1);

        for (uint_t fdx = 0; fdx < RecordTypeNumFields(obj1); fdx++)
        {
            int_t ret = DefaultCompare(AsRecordType(obj1)->Fields[fdx],
                    AsRecordType(obj2)->Fields[fdx]);
            if (ret != 0)
                return(ret);
        }

        return(0);

    case RecordOrder:
        FAssert(GenericRecordP(obj1));
        FAssert(GenericRecordP(obj2));

        if (RecordNumFields(obj1) != RecordNumFields(obj2))
            return(RecordNumFields(obj1) < RecordNumFields(obj2) ? -1 : 1);

        for (uint_t fdx = 0; fdx < RecordNumFields(obj1); fdx++)
        {
            int_t ret = DefaultCompare(AsGenericRecord(obj1)->Fields[fdx],
                    AsGenericRecord(obj2)->Fields[fdx]);
            if (ret != 0)
                return(ret);
        }

        return(0);

    case HashTreeOrder:
        FAssert(HashTreeP(obj1));
        FAssert(HashTreeP(obj2));

        if (HashTreeLength(obj1) != HashTreeLength(obj2))
            return(HashTreeLength(obj1) < HashTreeLength(obj2) ? -1 : 1);

        if (HashTreeBitmap(obj1) != HashTreeBitmap(obj2))
            return(HashTreeBitmap(obj1) < HashTreeBitmap(obj2) ? -1 : 1);

        for (uint_t bdx = 0; bdx < HashTreeLength(obj1); bdx++)
        {
            int_t ret = DefaultCompare(AsHashTree(obj1)->Buckets[bdx],
                    AsHashTree(obj2)->Buckets[bdx]);
            if (ret != 0)
                return(ret);
        }

        return(0);

    case BoxOrder:
        FAssert(BoxP(obj1));
        FAssert(BoxP(obj2));

        return(DefaultCompare(Unbox(obj1), Unbox(obj2)));

    default:
        return(0);
    }

    return(0);
}

static int_t DefaultEquality(FObject obj1, FObject obj2)
{
    return(DefaultCompare(obj1, obj2) == 0);
}

#define MAX_HASH_DEPTH 16

static uint_t DefaultHash(FObject obj, int_t dpth)
{
    if (ObjectP(obj) == 0)
        return((uint_t) obj);

    if (dpth > MAX_HASH_DEPTH)
        return(1);

    if (PairP(obj))
    {
        uint_t hash = 0;
        for (int_t n = 0; n < MAX_HASH_DEPTH; n++)
        {
            hash += (hash << 3);
            hash += DefaultHash(First(obj), dpth + 1);
            obj = Rest(obj);
            if (PairP(obj) == 0)
            {
                hash += (hash << 3);
                hash += DefaultHash(obj, dpth + 1);
                return(hash);
            }
        }
        return(hash);
    }
    else if (StringP(obj))
        return(StringHash(obj));
    else if (SymbolP(obj))
        return(SymbolHash(obj));
    else if (NumberP(obj))
        return(NumberHash(obj));
    else if (VectorP(obj))
    {
        uint_t hash = VectorLength(obj) + 1;
        for (uint_t idx = 0; idx < VectorLength(obj) && idx < MAX_HASH_DEPTH; idx++)
            hash += (hash << 5) + DefaultHash(AsVector(obj)->Vector[idx], dpth + 1);
        return(hash);
    }
    else if (BytevectorP(obj))
        return(BytevectorHash(obj));
    else if (RecordTypeP(obj))
    {
        uint_t hash = RecordTypeNumFields(obj) + 1;
        for (uint_t idx = 0; idx < RecordTypeNumFields(obj) && idx < MAX_HASH_DEPTH; idx++)
            hash += (hash << 5) + DefaultHash(AsRecordType(obj)->Fields[idx], dpth + 1);
        return(hash);
    }
    else if (GenericRecordP(obj))
    {
        uint_t hash = RecordNumFields(obj) + 1;
        for (uint_t idx = 0; idx < RecordNumFields(obj) && idx < MAX_HASH_DEPTH; idx++)
            hash += (hash << 5) + DefaultHash(AsGenericRecord(obj)->Fields[idx], dpth + 1);
        return(hash);
    }
    else if (HashTreeP(obj))
    {
        uint_t hash = HashTreeBitmap(obj);
        for (uint_t idx = 0; idx < HashTreeLength(obj) && idx < MAX_HASH_DEPTH; idx++)
            hash += (hash << 5) + DefaultHash(AsHashTree(obj)->Buckets[idx], dpth + 1);
        return(hash);
    }
    else if (BoxP(obj))
        return(DefaultHash(Unbox(obj), dpth + 1));

    return(1);
}

static uint_t DefaultHash(FObject obj)
{
    return(DefaultHash(obj, 0));
}

Define("default-type-test", DefaultTypeTestPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("default-type-test", argc);

    return(CompareOrder(argv[0]) == UnknownOrder ? FalseObject : TrueObject);
}

Define("default-equality", DefaultEqualityPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("default-equality", argc);

    return(DefaultEquality(argv[0], argv[1]) ? TrueObject : FalseObject);
}

Define("default-hash", DefaultHashPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("default-hash", argc);

    return(MakeFixnum(DefaultHash(argv[0]) & MAXIMUM_FIXNUM));
}

Define("default-compare", DefaultComparePrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("default-compare", argc);

    return(MakeFixnum(DefaultCompare(argv[0], argv[1])));
}

Define("any?", AnyPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("any?", argc);

    return(TrueObject);
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
    EqHashPrimitive,
    CheckHashBoundPrimitive,
    CheckHashSaltPrimitive,
    BooleanHashPrimitive,
    CharHashPrimitive,
    CharCiHashPrimitive,
    StringHashPrimitive,
    StringCiHashPrimitive,
    SymbolHashPrimitive,
    NumberHashPrimitive
};

void SetupCompare()
{
    RegisterRoot(&DefaultComparator, "default-comparator");
    RegisterRoot(&EqComparator, "eq-comparator");

    EqComparator = MakeComparator(TrueObject, EqPPrimitive, FalseObject, EqHashPrimitive);
    DefaultComparator = MakeComparator(DefaultTypeTestPrimitive, DefaultEqualityPrimitive,
            DefaultComparePrimitive, DefaultHashPrimitive);
}

void SetupComparePrims()
{
    LibraryExport(BedrockLibrary,
            EnvironmentSetC(Bedrock, "default-comparator", DefaultComparator));

    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(Bedrock, BedrockLibrary, Primitives[idx]);
}
