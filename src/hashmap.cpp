/*

Foment

*/

#include "foment.hpp"

// ---- Population Count ----
//
// popcount_3 from http://en.wikipedia.org/wiki/Hamming_weight#Efficient_implementation

const uint64_t m1  = 0x5555555555555555; //binary: 0101...
const uint64_t m2  = 0x3333333333333333; //binary: 00110011..
const uint64_t m4  = 0x0f0f0f0f0f0f0f0f; //binary:  4 zeros,  4 ones ...
const uint64_t h01 = 0x0101010101010101; //the sum of 256 to the power of 0,1,2,3...

int PopulationCount(uint64_t x)
{
    x -= (x >> 1) & m1;             //put count of each 2 bits into those 2 bits
    x = (x & m2) + ((x >> 2) & m2); //put count of each 4 bits into those 4 bits
    x = (x + (x >> 4)) & m4;        //put count of each 8 bits into those 8 bits
    return (x * h01)>>56;  //returns left 8 bits of x + (x<<8) + (x<<16) + (x<<24) + ...
}

// ---- Hash Tree ----
//
// The implementation uses hash array mapped tries;
// see http://lampwww.epfl.ch/papers/idealhashtrees.pdf

static FObject MakeHashTree(uint_t len, uint_t bm, FObject * bkts)
{
  FAssert(len == (uint_t) PopulationCount(bm));

    FHashTree * ht = (FHashTree *) MakeObject(sizeof(FHashTree) + (len - 1) * sizeof(FObject),
            HashTreeTag);
    ht->Length = MakeLength(len, HashTreeTag);
    ht->Bitmap = bm;

    for (uint_t idx = 0; idx < len; idx++)
        ht->Buckets[idx] = bkts[idx];

    return(ht);
}

FObject MakeHashTree()
{
    return(MakeHashTree(0, 0, 0));
}

#ifdef FOMENT_64BIT
#define INDEX_SHIFT 6
#define INDEX_MASK 0x3F
#endif // FOMENT_64BIT

#ifdef FOMENT_32BIT
#define INDEX_SHIFT 5
#define INDEX_MASK 0x1F
#endif // FOMENT_32BIT

static inline uint_t ActualIndex(FObject htree, uint_t bdx)
{
    FAssert(HashTreeP(htree));

    return(PopulationCount(AsHashTree(htree)->Bitmap & ((((uint_t) 1) << bdx) - 1)));
}

static inline uint_t BucketIndex(uint_t idx, uint_t dpth)
{
    FAssert(dpth <= sizeof(uint_t) * 8 / INDEX_SHIFT);

    return((idx >> (INDEX_SHIFT * dpth)) & INDEX_MASK);
}

static inline FObject GetBucket(FObject htree, uint_t bdx)
{
    FAssert(HashTreeP(htree));
    FAssert(ActualIndex(htree, bdx) < HashTreeLength(htree));

    return(AsHashTree(htree)->Buckets[ActualIndex(htree, bdx)]);
}

static inline uint_t BucketP(FObject htree, uint_t bdx)
{
    FAssert(HashTreeP(htree));

    return(AsHashTree(htree)->Bitmap & (((uint_t) 1) << bdx));
}

static FObject HashTreeRef(FObject htree, uint_t idx, FObject nfnd)
{
    FAssert(HashTreeP(htree));
    FAssert(idx < MAXIMUM_HASH_INDEX);

    uint_t dpth = 0;

    for (;;)
    {
        uint_t bdx = BucketIndex(idx, dpth);
        if (BucketP(htree, bdx) == 0)
            return(nfnd);

        FObject obj = GetBucket(htree, bdx);
        if (BoxP(obj))
        {
            FAssert(dpth == 0 || HashTreeLength(htree) > 1);

            if (BoxIndex(obj) == idx)
                return(Unbox(obj));
            break;
        }

        FAssert(HashTreeP(obj));

        htree = obj;
        dpth += 1;

        FAssert(dpth < sizeof(uint_t) * 8 / INDEX_SHIFT);
    }

    return(nfnd);
}

static FObject MakeHashTreeBuckets(uint_t dpth, FObject bx1, FObject bx2)
{
    FAssert(BoxIndex(bx1) != BoxIndex(bx2));

    uint_t bdx1 = BucketIndex(BoxIndex(bx1), dpth);
    uint_t bdx2 = BucketIndex(BoxIndex(bx2), dpth);
    FObject bkts[2];

    if (bdx1 == bdx2)
    {
        bkts[0] = MakeHashTreeBuckets(dpth + 1, bx1, bx2);
        return(MakeHashTree(1, ((uint_t) 1) << bdx1, bkts));
    }
    else if (bdx1 > bdx2)
    {
        bkts[0] = bx2;
        bkts[1] = bx1;
    }
    else
    {
        bkts[0] = bx1;
        bkts[1] = bx2;
    }

    return(MakeHashTree(2, (((uint_t) 1) << bdx1) | (((uint_t) 1) << bdx2), bkts));
}

static FObject HashTreeSet(FObject htree, uint_t idx, uint_t dpth, FObject val)
{
#ifdef FOMENT_DEBUG
    FObject otree = htree;
#endif // FOMENT_DEBUG

    FAssert(HashTreeP(htree));

    uint_t bdx = BucketIndex(idx, dpth);
    if (BucketP(htree, bdx) == 0)
    {
        FObject bkts[sizeof(uint_t) * 8];
        uint_t udx = 0;

        for (uint_t ndx = 0; ndx < sizeof(uint_t) * 8; ndx++)
        {
            if (BucketP(htree, ndx))
            {
                FAssert(ndx != bdx);

                bkts[udx] = GetBucket(htree, ndx);
                udx += 1;
            }
            else if (ndx == bdx)
            {
                bkts[udx] = MakeBox(val, idx);
                udx += 1;
            }
        }

        FAssert(udx == HashTreeLength(htree) + 1);

        htree = MakeHashTree(udx, AsHashTree(htree)->Bitmap | (((uint_t) 1) << bdx), bkts);

#ifdef FOMENT_DEBUG
        for (uint_t ndx = 0; ndx < sizeof(uint_t) * 8; ndx++)
        {
            if (BucketP(htree, ndx))
            {
                if (ndx != bdx)
                {
                    FAssert(GetBucket(otree, ndx) == GetBucket(htree, ndx));
                }
                else
                {
                    FObject obj = GetBucket(htree, ndx);
                    FAssert(BoxP(obj));
                    FAssert(Unbox(obj) == val);
                    FAssert(BoxIndex(obj) == idx);
                    FAssert(BucketP(otree, ndx) == 0);
                }
            }
            else
            {
                FAssert(BucketP(otree, ndx) == 0);
            }
        }
#endif // FOMENT_DEBUG
    }
    else
    {
        FObject obj = GetBucket(htree, bdx);
        if (BoxP(obj))
        {
            if (BoxIndex(obj) == idx)
                SetBox(obj, val);
            else
                obj = MakeHashTreeBuckets(dpth + 1, obj, MakeBox(val, idx));
        }
        else
        {
            FAssert(HashTreeP(obj));

            obj = HashTreeSet(obj, idx, dpth + 1, val);
        }

//        AsHashTree(htree)->Buckets[ActualIndex(htree, bdx)] = obj;
        Modify(FHashTree, htree, Buckets[ActualIndex(htree, bdx)], obj);
    }

    return(htree);
}

static FObject HashTreeSet(FObject htree, uint_t idx, FObject val)
{
    FAssert(idx < MAXIMUM_HASH_INDEX);

    return(HashTreeSet(htree, idx, 0, val));
}

static FObject HashTreeDelete(FObject htree, uint_t idx, uint_t dpth)
{
#ifdef FOMENT_DEBUG
    FObject otree = htree;
#endif // FOMENT_DEBUG

    FAssert(HashTreeP(htree));

    uint_t bdx = BucketIndex(idx, dpth);
    if (BucketP(htree, bdx))
    {
        FObject obj = GetBucket(htree, bdx);
        if (BoxP(obj))
        {
            if (BoxIndex(obj) == idx)
            {
                if (dpth > 0)
                {
                    FAssert(HashTreeLength(htree) > 1);

                    if (HashTreeLength(htree) == 2)
                    {
                        for (uint_t ndx = 0; ndx < sizeof(uint_t) * 8; ndx++)
                            if (ndx != bdx && BucketP(htree, ndx))
                            {
                                obj = GetBucket(htree, ndx);
                                if (BoxP(obj))
                                    return(obj);
                                break;
                            }
                    }
                }

                FObject bkts[sizeof(uint_t) * 8];
                uint_t udx = 0;

                for (uint_t ndx = 0; ndx < sizeof(uint_t) * 8; ndx++)
                {
                    if (ndx != bdx && BucketP(htree, ndx))
                    {
                        bkts[udx] = GetBucket(htree, ndx);
                        udx += 1;
                    }
                }

                htree = MakeHashTree(udx, AsHashTree(htree)->Bitmap & ~(((uint_t) 1) << bdx),
                        bkts);
#ifdef FOMENT_DEBUG
                for (uint_t ndx = 0; ndx < sizeof(uint_t) * 8; ndx++)
                {
                    if (BucketP(htree, ndx))
                    {
                        FAssert(ndx != bdx);
                        FAssert(GetBucket(otree, ndx) == GetBucket(htree, ndx));
                    }
                    else if (ndx != bdx)
                    {
                        FAssert(BucketP(otree, ndx) == 0);
                    }
                    else
                    {
                        FAssert(BucketP(otree, ndx));
                    }
                }
#endif // FOMENT_DEBUG
            }
        }
        else
        {
            FAssert(HashTreeP(obj));

            obj = HashTreeDelete(obj, idx, dpth + 1);
            if (BoxP(obj) && HashTreeLength(htree) == 1 && dpth > 0)
                return(obj);

//            AsHashTree(htree)->Buckets[ActualIndex(htree, bdx)] = obj;
            Modify(FHashTree, htree, Buckets[ActualIndex(htree, bdx)], obj);
        }
    }

    return(htree);
}

static FObject HashTreeDelete(FObject htree, uint_t idx)
{
    FAssert(idx < MAXIMUM_HASH_INDEX);

    return(HashTreeDelete(htree, idx, 0));
}

typedef void (*FVisitBucketFn)(FObject obj, uint_t idx, FVisitFn vfn, FObject ctx);

static void HashTreeVisit(FObject htree, FVisitBucketFn vbfn, FVisitFn vfn, FObject ctx)
{
    FAssert(HashTreeP(htree));

    for (uint_t bdx = 0; bdx < HashTreeLength(htree); bdx++)
    {
        FObject obj = AsHashTree(htree)->Buckets[bdx];
        if (BoxP(obj))
            vbfn(Unbox(obj), BoxIndex(obj), vfn, ctx);
        else
        {
            FAssert(HashTreeP(obj));

            HashTreeVisit(obj, vbfn, vfn, ctx);
        }
    }
}

/*
void WriteHashTree(FObject port, FObject htree)
{
    FCh s[16];
    int_t sl = FixnumAsString((FFixnum) htree, s, 16);

    FAssert(HashTreeP(htree));

    WriteStringC(port, "#<hash-tree: ");
    WriteString(port, s, sl);
    WriteCh(port, ' ');

    for (uint_t idx = 0; idx < sizeof(uint_t) * 8; idx++)
    {
        if (BucketP(htree, idx))
        {
            WriteCh(port, '[');
            sl = FixnumAsString(idx, s, 10);
            WriteString(port, s, sl);
            WriteStringC(port, "] = ");
            Write(port, GetBucket(htree, idx), 1);
            WriteCh(port, ' ');
        }
    }

    WriteCh(port, '>');
}
*/

Define("make-hash-tree", MakeHashTreePrimitive)(int_t argc, FObject argv[])
{
    ZeroArgsCheck("make-hash-tree", argc);

    return(MakeHashTree(0, 0, 0));
}

Define("hash-tree?", HashTreePPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("hash-tree?", argc);

    return(HashTreeP(argv[0]) ? TrueObject : FalseObject);
}

Define("hash-tree-ref", HashTreeRefPrimitive)(int_t argc, FObject argv[])
{
    ThreeArgsCheck("hash-tree-ref", argc);
    HashTreeArgCheck("hash-tree-ref", argv[0]);
    FixnumArgCheck("hash-tree-ref", argv[1]);

    return(HashTreeRef(argv[0], AsFixnum(argv[1]), argv[2]));
}

Define("hash-tree-set!", HashTreeSetPrimitive)(int_t argc, FObject argv[])
{
    ThreeArgsCheck("hash-tree-set!", argc);
    HashTreeArgCheck("hash-tree-set!", argv[0]);
    FixnumArgCheck("hash-tree-set!", argv[1]);

    return(HashTreeSet(argv[0], AsFixnum(argv[1]), argv[2]));
}

Define("hash-tree-delete", HashTreeDeletePrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("hash-tree-delete", argc);
    HashTreeArgCheck("hash-tree-delete", argv[0]);
    FixnumArgCheck("hash-tree-delete", argv[1]);

    return(HashTreeDelete(argv[0], AsFixnum(argv[1])));
}

Define("hash-tree-buckets", HashTreeBucketsPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("hash-tree-buckets", argc);
    HashTreeArgCheck("hash-tree-buckets", argv[0]);

    return(MakeVector(HashTreeLength(argv[0]), AsHashTree(argv[0])->Buckets, NoValueObject));
}

Define("hash-tree-bitmap", HashTreeBitmapPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("hash-tree-bitmap", argc);
    HashTreeArgCheck("hash-tree-bitmap", argv[0]);

    return(MakeIntegerU(AsHashTree(argv[0])->Bitmap));
}

// ---- Symbols ----

static uint_t NextSymbolHash = 0;

FObject StringToSymbol(FObject str)
{
    FAssert(StringP(str));

    uint_t idx = StringHash(str) % MAXIMUM_HASH_INDEX;
    FObject lst = HashTreeRef(R.SymbolHashTree, idx, MakeFixnum(idx));
    FObject obj = lst;

    while (PairP(obj))
    {
        FAssert(SymbolP(First(obj)));

        if (StringCompare(AsSymbol(First(obj))->String, str) == 0)
            return(First(obj));
        obj = Rest(obj);
    }

    FSymbol * sym = (FSymbol *) MakeObject(sizeof(FSymbol), SymbolTag);
    sym->Reserved = MakeLength(NextSymbolHash, SymbolTag);
    sym->String = str;
    NextSymbolHash = (NextSymbolHash + 1) % MAXIMUM_HASH_INDEX;

    R.SymbolHashTree = HashTreeSet(R.SymbolHashTree, idx, MakePair(sym, lst));

    return(sym);
}

FObject StringLengthToSymbol(FCh * s, int_t sl)
{
    uint_t idx = StringLengthHash(s, sl) % MAXIMUM_HASH_INDEX;
    FObject lst = HashTreeRef(R.SymbolHashTree, idx, MakeFixnum(idx));
    FObject obj = lst;

    while (PairP(obj))
    {
        FAssert(SymbolP(First(obj)));

        if (StringLengthEqualP(s, sl, AsSymbol(First(obj))->String))
            return(First(obj));
        obj = Rest(obj);
    }

    FSymbol * sym = (FSymbol *) MakeObject(sizeof(FSymbol), SymbolTag);
    sym->Reserved = MakeLength(NextSymbolHash, SymbolTag);
    sym->String = MakeString(s, sl);
    NextSymbolHash = (NextSymbolHash + 1) % MAXIMUM_HASH_INDEX;

    R.SymbolHashTree = HashTreeSet(R.SymbolHashTree, idx, MakePair(sym, lst));

    return(sym);
}

// ---- Hash Map ----

static const char * HashMapFieldsC[4] = {"hash-tree", "comparator", "tracker", "size"};

static FObject MakeHashMap(FObject comp, FObject tracker)
{
    FAssert(sizeof(FHashMap) == sizeof(HashMapFieldsC) + sizeof(FRecord));
    FAssert(ComparatorP(comp));
    FAssert(tracker == NoValueObject || PairP(tracker));

    FHashMap * hmap = (FHashMap *) MakeRecord(R.HashMapRecordType);
    hmap->HashTree = MakeHashTree();
    hmap->Comparator = comp;
    hmap->Tracker = tracker;
    hmap->Size = MakeFixnum(0);

    return(hmap);
}

static FObject HashMapRef(FObject hmap, FObject key, FObject def, FEquivFn eqfn, FHashFn hashfn)
{
    FAssert(HashMapP(hmap));

    uint_t idx = hashfn(key) % MAXIMUM_HASH_INDEX;
    FObject lst = HashTreeRef(AsHashMap(hmap)->HashTree, idx, MakeFixnum(idx));

    while (PairP(lst))
    {
        FAssert(PairP(First(lst)));

        if (eqfn(First(First(lst)), key))
            return(Rest(First(lst)));
        lst = Rest(lst);
    }

    return(def);
}

static void HashMapSet(FObject hmap, FObject key, FObject val, FEquivFn eqfn, FHashFn hashfn)
{
    FAssert(HashMapP(hmap));

    uint_t idx = hashfn(key) % MAXIMUM_HASH_INDEX;
    FObject slot = HashTreeRef(AsHashMap(hmap)->HashTree, idx, MakeFixnum(idx));
    FObject lst = slot;

    while (PairP(lst))
    {
        FAssert(PairP(First(lst)));

        if (eqfn(First(First(lst)), key))
        {
            SetRest(First(lst), val);
            return;
        }
        lst = Rest(lst);
    }

    FObject kvn = MakePair(MakePair(key, val), slot);
    if (PairP(AsHashMap(hmap)->Tracker) && ObjectP(key))
        InstallTracker(key, kvn, AsHashMap(hmap)->Tracker);

//    AsHashMap(hmap)->HashTree = HashTreeSet(AsHashMap(hmap)->HashTree, idx, kvn);
    Modify(FHashMap, hmap, HashTree, HashTreeSet(AsHashMap(hmap)->HashTree, idx, kvn));

    FAssert(FixnumP(AsHashMap(hmap)->Size));

//    AsHashMap(hmap)->Size = MakeFixnum(AsFixnum(AsHashMap(hmap)->Size) + 1);
    Modify(FHashMap, hmap, Size, MakeFixnum(AsFixnum(AsHashMap(hmap)->Size) + 1));
}

static void HashMapDelete(FObject hmap, FObject key, FEquivFn eqfn, FHashFn hashfn)
{
    FAssert(HashMapP(hmap));

    uint_t idx = hashfn(key) % MAXIMUM_HASH_INDEX;
    FObject lst = HashTreeRef(AsHashMap(hmap)->HashTree, idx, MakeFixnum(idx));
    FObject prev = NoValueObject;

    while (PairP(lst))
    {
        FAssert(PairP(First(lst)));

        if (eqfn(First(First(lst)), key))
        {
            FAssert(FixnumP(AsHashMap(hmap)->Size));
            FAssert(AsFixnum(AsHashMap(hmap)->Size) > 0);

//            AsHashMap(hmap)->Size = MakeFixnum(AsFixnum(AsHashMap(hmap)->Size) - 1);
            Modify(FHashMap, hmap, Size, MakeFixnum(AsFixnum(AsHashMap(hmap)->Size) - 1));

            if (PairP(prev))
                SetRest(prev, Rest(lst));
            else if (PairP(Rest(lst)))
            {
//                AsHashMap(hmap)->HashTree = HashTreeSet(AsHashMap(hmap)->HashTree, idx,
//                        Rest(lst));
                Modify(FHashMap, hmap, HashTree, HashTreeSet(AsHashMap(hmap)->HashTree, idx,
                        Rest(lst)));
            }
            else
            {
//                AsHashMap(hmap)->HashTree = HashTreeDelete(AsHashMap(hmap)->HashTree, idx);
                Modify(FHashMap, hmap, HashTree, HashTreeDelete(AsHashMap(hmap)->HashTree, idx));
            }
            break;
        }
        prev = lst;
        lst = Rest(lst);
    }
}

static void VisitHashMapBucket(FObject lst, uint_t idx, FVisitFn vfn, FObject ctx)
{
    while (PairP(lst))
    {
        FAssert(PairP(First(lst)));

        vfn(First(First(lst)), Rest(First(lst)), ctx);
        lst = Rest(lst);
    }
}

static void HashMapVisit(FObject hmap, FVisitFn vfn, FObject ctx)
{
    FAssert(HashMapP(hmap));

    HashTreeVisit(AsHashMap(hmap)->HashTree, VisitHashMapBucket, vfn, ctx);
}

FObject MakeEqHashMap()
{
    return(MakeHashMap(R.EqComparator, MakeTConc()));
}

static uint_t OldIndex(FObject kvn)
{
    while (PairP(kvn))
        kvn = Rest(kvn);

    FAssert(FixnumP(kvn));

    return(AsFixnum(kvn));
}

static void RemoveOld(FObject hmap, FObject kvn, uint_t idx)
{
    FObject node = HashTreeRef(AsHashMap(hmap)->HashTree, idx, MakeFixnum(idx));
    FObject prev = NoValueObject;

    while (PairP(node))
    {
        if (node == kvn)
        {
            if (PairP(prev))
                SetRest(prev, Rest(node));
            else if (PairP(Rest(node)))
            {
//                AsHashMap(hmap)->HashTree = HashTreeSet(AsHashMap(hmap)->HashTree, idx,
//                        Rest(node));
                Modify(FHashMap, hmap, HashTree, HashTreeSet(AsHashMap(hmap)->HashTree, idx,
                        Rest(node)));
            }
            else
            {
//                AsHashMap(hmap)->HashTree = HashTreeDelete(AsHashMap(hmap)->HashTree, idx);
                Modify(FHashMap, hmap, HashTree, HashTreeDelete(AsHashMap(hmap)->HashTree, idx));
            }

            return;
        }

        prev = node;
        node = Rest(node);
    }

    FAssert(0);
}

#if FOMENT_DEBUG
void CheckVisitBucket(FObject lst, uint_t idx, FVisitFn vfn, FObject ctx)
{
    while (PairP(lst))
    {
        FAssert(PairP(First(lst)));
        FAssert(EqHash(First(First(lst))) % MAXIMUM_HASH_INDEX == idx);

        lst = Rest(lst);
    }

    FAssert(FixnumP(lst));
    FAssert(AsFixnum(lst) == (int_t) idx);
}

static void CheckEqHashMap(FObject hmap)
{
    FAssert(HashMapP(hmap));

    HashTreeVisit(AsHashMap(hmap)->HashTree, CheckVisitBucket, 0, 0);
}
#endif // FOMENT_DEBUG

static void EqHashMapRehash(FObject hmap, FObject tconc)
{
    FObject kvn;

    while (TConcEmptyP(tconc) == 0)
    {
        kvn = TConcRemove(tconc);

        FAssert(PairP(kvn));
        FAssert(PairP(First(kvn)));

        FObject key = First(First(kvn));
        uint_t odx = OldIndex(kvn);
        uint_t idx = EqHash(key) % MAXIMUM_HASH_INDEX;

        if (idx != odx)
        {
            RemoveOld(hmap, kvn, odx);
            SetRest(kvn, HashTreeRef(AsHashMap(hmap)->HashTree, idx, MakeFixnum(idx)));

//            AsHashMap(hmap)->HashTree = HashTreeSet(AsHashMap(hmap)->HashTree, idx,
//                    Rest(lst));
            Modify(FHashMap, hmap, HashTree, HashTreeSet(AsHashMap(hmap)->HashTree, idx, kvn));
        }

        FAssert(ObjectP(key));

        InstallTracker(key, kvn, AsHashMap(hmap)->Tracker);
    }

#ifdef FOMENT_DEBUG
    CheckEqHashMap(hmap);
#endif // FOMENT_DEBUG
}

FObject EqHashMapRef(FObject hmap, FObject key, FObject def)
{
    FAssert(HashMapP(hmap));
    FAssert(PairP(AsHashMap(hmap)->Tracker));

    if (TConcEmptyP(AsHashMap(hmap)->Tracker) == 0)
        EqHashMapRehash(hmap, AsHashMap(hmap)->Tracker);

    FObject ret = HashMapRef(hmap, key, def, EqP, EqHash);

#ifdef FOMENT_DEBUG
    CheckEqHashMap(hmap);
#endif // FOMENT_DEBUG

    return(ret);
}

void EqHashMapSet(FObject hmap, FObject key, FObject val)
{
    FAssert(HashMapP(hmap));
    FAssert(PairP(AsHashMap(hmap)->Tracker));

    if (TConcEmptyP(AsHashMap(hmap)->Tracker) == 0)
        EqHashMapRehash(hmap, AsHashMap(hmap)->Tracker);

    HashMapSet(hmap, key, val, EqP, EqHash);

#ifdef FOMENT_DEBUG
    CheckEqHashMap(hmap);
#endif // FOMENT_DEBUG
}

void EqHashMapDelete(FObject hmap, FObject key)
{
    FAssert(HashMapP(hmap));
    FAssert(PairP(AsHashMap(hmap)->Tracker));

    if (TConcEmptyP(AsHashMap(hmap)->Tracker) == 0)
        EqHashMapRehash(hmap, AsHashMap(hmap)->Tracker);

    HashMapDelete(hmap, key, EqP, EqHash);

#ifdef FOMENT_DEBUG
    CheckEqHashMap(hmap);
#endif // FOMENT_DEBUG
}

void EqHashMapVisit(FObject hmap, FVisitFn vfn, FObject ctx)
{
    FAssert(HashMapP(hmap));
    FAssert(PairP(AsHashMap(hmap)->Tracker));

    if (TConcEmptyP(AsHashMap(hmap)->Tracker) == 0)
        EqHashMapRehash(hmap, AsHashMap(hmap)->Tracker);

    HashMapVisit(hmap, vfn, ctx);
}


Define("make-eq-hash-map", MakeEqHashMapPrimitive)(int_t argc, FObject argv[])
{
    ZeroArgsCheck("make-eq-hash-map", argc);

    return(MakeEqHashMap());
}

Define("eq-hash-map?", EqHashMapPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("eq-hash-map?", argc);

    return((HashMapP(argv[0]) && PairP(AsHashMap(argv[0])->Tracker)) ? TrueObject : FalseObject);
}

Define("eq-hash-map-ref", EqHashMapRefPrimitive)(int_t argc, FObject argv[])
{
    ThreeArgsCheck("eq-hash-map-ref", argc);
    EqHashMapArgCheck("eq-hash-map-ref", argv[0]);

    return(EqHashMapRef(argv[0], argv[1], argv[2]));
}

Define("eq-hash-map-set!", EqHashMapSetPrimitive)(int_t argc, FObject argv[])
{
    ThreeArgsCheck("eq-hash-map-set!", argc);
    EqHashMapArgCheck("eq-hash-map-set!", argv[0]);

    EqHashMapSet(argv[0], argv[1], argv[2]);
    return(NoValueObject);
}

Define("eq-hash-map-delete", EqHashMapDeletePrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("eq-hash-map-delete", argc);
    EqHashMapArgCheck("eq-hash-map-delete", argv[0]);

    EqHashMapDelete(argv[0], argv[1]);
    return(NoValueObject);
}

Define("make-hash-map", MakeHashMapPrimitive)(int_t argc, FObject argv[])
{
    ZeroOrOneArgsCheck("make-hash-map", argc);
    if (argc == 1)
        ComparatorArgCheck("make-hash-map", argv[0]);

    return(MakeHashMap(argc == 1 ? argv[0] : R.DefaultComparator, NoValueObject));
}

Define("hash-map?", HashMapPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("hash-map?", argc);

    return(HashMapP(argv[0]) ? TrueObject : FalseObject);
}

Define("hash-map-tree-ref", HashMapTreeRefPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("hash-map-tree-ref", argc);
    HashMapArgCheck("hash-map-tree-ref", argv[0]);

    return(AsHashMap(argv[0])->HashTree);
}

Define("hash-map-tree-set!", HashMapTreeSetPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("hash-map-tree-set!", argc);
    HashMapArgCheck("hash-map-tree-set!", argv[0]);
    HashTreeArgCheck("hash-map-tree-set!", argv[1]);

//    AsHashMap(argv[0])->HashTree = argv[1];
    Modify(FHashMap, argv[0], HashTree, argv[1]);
    return(NoValueObject);
}

Define("hash-map-comparator", HashMapComparatorPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("hash-map-comparator", argc);
    HashMapArgCheck("hash-map-comparator", argv[0]);

    return(AsHashMap(argv[0])->Comparator);
}

Define("hash-map-size", HashMapSizePrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("hash-map-size", argc);
    HashMapArgCheck("hash-map-size", argv[0]);

    return(AsHashMap(argv[0])->Size);
}

Define("hash-map-size-set!", HashMapSizeSetPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("hash-map-size-set!", argc);
    HashMapArgCheck("hash-map-size-set!", argv[0]);
    NonNegativeArgCheck("hash-map-size-set!", argv[1], 0);

//    AsHashMap(argv[0])->Size = argv[1];
    Modify(FHashMap, argv[0], Size, argv[1]);
    return(NoValueObject);
}

// ---- Primitives ----

static FPrimitive * Primitives[] =
{
    &MakeHashTreePrimitive,
    &HashTreePPrimitive,
    &HashTreeRefPrimitive,
    &HashTreeSetPrimitive,
    &HashTreeDeletePrimitive,
    &HashTreeBucketsPrimitive,
    &HashTreeBitmapPrimitive,
    &MakeEqHashMapPrimitive,
    &EqHashMapPPrimitive,
    &EqHashMapRefPrimitive,
    &EqHashMapSetPrimitive,
    &EqHashMapDeletePrimitive,
    &MakeHashMapPrimitive,
    &HashMapPPrimitive,
    &HashMapTreeRefPrimitive,
    &HashMapTreeSetPrimitive,
    &HashMapComparatorPrimitive,
    &HashMapSizePrimitive,
    &HashMapSizeSetPrimitive
};

void SetupHashMaps()
{
    R.HashMapRecordType = MakeRecordTypeC("hash-map",
            sizeof(HashMapFieldsC) / sizeof(char *), HashMapFieldsC);
}

void SetupHashMapPrims()
{
    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
