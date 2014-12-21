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

#define MAXIMUM_HASH_INDEX MAXIMUM_FIXNUM

static FObject MakeHashTree(uint_t len, uint_t bm, FObject * bkts)
{
    FAssert(len == PopulationCount(bm));

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

        if (StringEqualP(AsSymbol(First(obj))->String, str))
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

const char * HashMapFieldsC[3] = {"hash-tree", "comparator", "tracker"};

static FObject MakeHashMap(FObject comp, FObject tracker)
{
    FAssert(sizeof(FHashMap) == sizeof(HashMapFieldsC) + sizeof(FRecord));

    FHashMap * hmap = (FHashMap *) MakeRecord(R.HashMapRecordType);
    hmap->HashTree = MakeHashTree();
    hmap->Comparator = comp;
    hmap->Tracker = tracker;

    return(hmap);
}

static FObject MakeHashMap(FObject comp)
{
    return(MakeHashMap(comp, NoValueObject));
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
    return(MakeHashMap(NoValueObject, MakeTConc()));
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

const char * HashtableFieldsC[3] = {"buckets", "size", "tracker"};

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
        if (PairP(AsHashtable(ht)->Tracker) && ObjectP(key))
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

        FAssert(ObjectP(key));

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

void HashtableVisit(FObject ht, FVisitFn vfn, FObject ctx)
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

            vfn(First(First(lst)), Rest(First(lst)), ctx);
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

// ---- Primitives ----

static FPrimitive * Primitives[] =
{
    &MakeHashTreePrimitive,
    &HashTreeRefPrimitive,
    &HashTreeSetPrimitive,
    &HashTreeDeletePrimitive,
    &MakeEqHashMapPrimitive,
    &EqHashMapRefPrimitive,
    &EqHashMapSetPrimitive,
    &EqHashMapDeletePrimitive,
    
    &MakeEqHashtablePrimitive,
    &EqHashtableRefPrimitive,
    &EqHashtableSetPrimitive,
    &EqHashtableDeletePrimitive
};

void SetupHashMaps()
{
    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
