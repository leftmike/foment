/*

Foment

*/

#include <stdio.h>
#include <string.h>
#ifdef FOMENT_WINDOWS
#include <intrin.h>
#endif // FOMENT_WINDOWS
#include "foment.hpp"

// ---- Population Count ----
//
// popcount_3 from http://en.wikipedia.org/wiki/Hamming_weight#Efficient_implementation

#ifdef FOMENT_UNIX
#ifdef FOMENT_64BIT
#define PopulationCount(x) __builtin_popcountl(x)
#endif
#ifdef FOMENT_32BIT
#define PopulationCount(x) __builtin_popcount(x)
#endif
#endif // FOMENT_UNIX

#ifdef FOMENT_WINDOWS
#ifdef FOMENT_64BIT
#define PopulationCount(x) __popcnt64(x)
#endif
#ifdef FOMENT_32BIT
#define PopulationCount(x) __popcnt(x)
#endif
#endif // FOMENT_WINDOWS

#ifndef PopulationCount
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
#endif

// ---- Hash Tree ----
//
// The implementation uses hash array mapped tries;
// see http://lampwww.epfl.ch/papers/idealhashtrees.pdf

#define HASH_MODULO (MAXIMUM_FIXNUM + 1)

static FObject MakeHashTree(uint_t len, uint_t bm, FObject * bkts, const char * who)
{
    FAssert(len == (uint_t) PopulationCount(bm));

    FHashTree * ht = (FHashTree *) MakeObject(HashTreeTag,
            sizeof(uint_t) + len * sizeof(FObject), len, who);
    HashTreeBitmap(ht) = bm;

    for (uint_t idx = 0; idx < len; idx++)
        ht->Buckets[idx] = bkts[idx];

    return(ht);
}

FObject MakeHashTree(const char * who)
{
    return(MakeHashTree(0, 0, 0, who));
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

    return(PopulationCount(HashTreeBitmap(htree) & ((((uint_t) 1) << bdx) - 1)));
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

    return(HashTreeBitmap(htree) & (((uint_t) 1) << bdx));
}

static FObject HashTreeRef(FObject htree, uint_t idx, FObject nfnd)
{
    FAssert(HashTreeP(htree));
    FAssert(idx < HASH_MODULO);

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
        return(MakeHashTree(1, ((uint_t) 1) << bdx1, bkts, "hash-tree-set!"));
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

    return(MakeHashTree(2, (((uint_t) 1) << bdx1) | (((uint_t) 1) << bdx2), bkts,
            "hash-tree-set!"));
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

        htree = MakeHashTree(udx, HashTreeBitmap(htree) | (((uint_t) 1) << bdx), bkts,
                "hash-tree-set!");

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
    FAssert(idx < HASH_MODULO);

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

                htree = MakeHashTree(udx, HashTreeBitmap(htree) & ~(((uint_t) 1) << bdx),
                        bkts, "hash-tree-delete");
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
    FAssert(idx < HASH_MODULO);

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

static FObject HashTreeCopy(FObject htree)
{
    FAssert(HashTreeP(htree));

    FObject bkts[sizeof(uint_t) * 8];
    uint_t bdx;

    for (bdx = 0; bdx < HashTreeLength(htree); bdx++)
    {
        FObject obj = AsHashTree(htree)->Buckets[bdx];
        if (BoxP(obj))
            bkts[bdx] = MakeBox(Unbox(obj), BoxIndex(obj));
        else
        {
            FAssert(HashTreeP(obj));

            bkts[bdx] = HashTreeCopy(obj);
        }
    }

    return(MakeHashTree(HashTreeLength(htree), HashTreeBitmap(htree), bkts, "hash-tree-copy"));
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

    return(MakeHashTree(0, 0, 0, "make-hash-tree"));
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
    NonNegativeArgCheck("hash-tree-ref", argv[1], 1);

    return(HashTreeRef(argv[0], NumberHash(argv[1]) % HASH_MODULO, argv[2]));
}

Define("hash-tree-set!", HashTreeSetPrimitive)(int_t argc, FObject argv[])
{
    ThreeArgsCheck("hash-tree-set!", argc);
    HashTreeArgCheck("hash-tree-set!", argv[0]);
    NonNegativeArgCheck("hash-tree-set!", argv[1], 1);

    return(HashTreeSet(argv[0], NumberHash(argv[1]) % HASH_MODULO, argv[2]));
}

Define("hash-tree-delete", HashTreeDeletePrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("hash-tree-delete", argc);
    HashTreeArgCheck("hash-tree-delete", argv[0]);
    NonNegativeArgCheck("hash-tree-delete", argv[1], 1);

    return(HashTreeDelete(argv[0], NumberHash(argv[1]) % HASH_MODULO));
}

Define("hash-buckets-ref", HashBucketsRefPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("hash-buckets-ref", argc);
    HashTreeArgCheck("hash-buckets-ref", argv[0]);
    IndexArgCheck("hash-buckets-ref", argv[1], HashTreeLength(argv[0]));

    return(AsHashTree(argv[0])->Buckets[AsFixnum(argv[1])]);
}

Define("hash-buckets-length", HashBucketsLengthPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("hash-buckets-length!", argc);
    HashTreeArgCheck("hash-buckets-length", argv[0]);

    return(MakeFixnum(HashTreeLength(argv[0])));
}

Define("hash-tree-bitmap", HashTreeBitmapPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("hash-tree-bitmap", argc);
    HashTreeArgCheck("hash-tree-bitmap", argv[0]);

    return(MakeIntegerU(HashTreeBitmap(argv[0])));
}

Define("hash-tree-copy", HashTreeCopyPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("hash-tree-copy", argc);
    HashTreeArgCheck("hash-tree-copy", argv[0]);

    return(HashTreeCopy(argv[0]));
}

// ---- Symbols ----

FObject SymbolToString(FObject sym)
{
    if (StringP(AsSymbol(sym)->String))
        return(AsSymbol(sym)->String);

    FAssert(CStringP(AsSymbol(sym)->String));

    return(MakeStringC(AsCString(AsSymbol(sym)->String)->String));
}

FObject StringToSymbol(FObject str)
{
    FAssert(StringP(str));

    uint_t idx = StringHash(str) % HASH_MODULO;
    FObject lst = HashTreeRef(R.SymbolHashTree, idx, MakeFixnum(idx));
    FObject obj = lst;

    while (PairP(obj))
    {
        FAssert(SymbolP(First(obj)));

        if (StringCompare(SymbolToString(First(obj)), str) == 0)
            return(First(obj));
        obj = Rest(obj);
    }

    FSymbol * sym = (FSymbol *) MakeObject(SymbolTag, sizeof(FSymbol), 1, "string->symbol");
    sym->String = str;

    R.SymbolHashTree = HashTreeSet(R.SymbolHashTree, idx, MakePair(sym, lst));

    return(sym);
}

FObject StringLengthToSymbol(FCh * s, int_t sl)
{
    uint_t idx = StringLengthHash(s, sl) % HASH_MODULO;
    FObject lst = HashTreeRef(R.SymbolHashTree, idx, MakeFixnum(idx));
    FObject obj = lst;

    while (PairP(obj))
    {
        FAssert(SymbolP(First(obj)));

        if (StringLengthEqualP(s, sl, SymbolToString(First(obj))))
            return(First(obj));
        obj = Rest(obj);
    }

    FSymbol * sym = (FSymbol *) MakeObject(SymbolTag, sizeof(FSymbol), 1, "string->symbol");
    sym->String = MakeString(s, sl);

    R.SymbolHashTree = HashTreeSet(R.SymbolHashTree, idx, MakePair(sym, lst));

    return(sym);
}

static uint_t CStringHash(const char * s)
{
    uint_t h = 0;

    while (*s)
    {
        h = ((h << 5) + h) + *s;
        s += 1;
    }

    return(h);
}

uint_t SymbolHash(FObject obj)
{
    FAssert(SymbolP(obj));

    if (StringP(AsSymbol(obj)->String))
        return(StringHash(AsSymbol(obj)->String));

    FAssert(CStringP(AsSymbol(obj)->String));

    return(CStringHash(AsCString(AsSymbol(obj)->String)->String));
}

static int_t CStringCompare(const char * s, FObject obj)
{
    FAssert(StringP(obj));

    uint_t sdx;
    for (sdx = 0; s[sdx] != 0 && sdx < StringLength(obj); sdx++)
        if (s[sdx] != AsString(obj)->String[sdx])
            return((uint8_t) s[sdx] < AsString(obj)->String[sdx] ? -1 : 1);

    if (s[sdx] == 0 && sdx == StringLength(obj))
        return(0);
    return(s[sdx] == 0 ? -1 : 1);
}

int_t SymbolCompare(FObject obj1, FObject obj2)
{
    FAssert(SymbolP(obj1));
    FAssert(SymbolP(obj2));

    if (StringP(AsSymbol(obj1)->String))
    {
        if (StringP(AsSymbol(obj2)->String))
            return(StringCompare(AsSymbol(obj1)->String, AsSymbol(obj2)->String));

        FAssert(CStringP(AsSymbol(obj2)->String));

        return(CStringCompare(AsCString(AsSymbol(obj2)->String)->String,
                AsSymbol(obj1)->String) * -1);
    }

    FAssert(CStringP(AsSymbol(obj1)->String));

    if (StringP(AsSymbol(obj2)->String))
        return(CStringCompare(AsCString(AsSymbol(obj1)->String)->String, AsSymbol(obj2)->String));

    FAssert(CStringP(AsSymbol(obj2)->String));

    return(strcmp(AsCString(AsSymbol(obj1)->String)->String,
            AsCString(AsSymbol(obj1)->String)->String));
}

FObject InternSymbol(FObject sym, int_t msf)
{
    FAssert(SymbolP(sym));
    FAssert(AsObjHdr(sym)->Generation() == OBJHDR_GEN_ETERNAL);
    FAssert(CStringP(AsSymbol(sym)->String));
    FAssert(AsObjHdr(AsSymbol(sym)->String)->Generation() == OBJHDR_GEN_ETERNAL);

    uint_t idx = SymbolHash(sym) % HASH_MODULO;
    FObject lst = HashTreeRef(R.SymbolHashTree, idx, MakeFixnum(idx));
    FObject obj = lst;

    while (PairP(obj))
    {
        FAssert(SymbolP(First(obj)));
        if (SymbolCompare(First(obj), sym) == 0)
        {
            if (msf == 0)
                return(First(obj));
            printf("%s\n", AsCString(AsSymbol(sym)->String)->String);
        }

        FMustBe(SymbolCompare(First(obj), sym) != 0);

        obj = Rest(obj);
    }

    R.SymbolHashTree = HashTreeSet(R.SymbolHashTree, idx, MakePair(sym, lst));
    return(sym);
}

// ---- Hash Container ----

static FObject HashContainerRef(FObject hcontain, FObject key, FObject def, FEquivFn eqfn,
    FHashFn hashfn)
{
    FAssert(HashContainerP(hcontain));

    uint_t idx = hashfn(key) % HASH_MODULO;
    FObject lst = HashTreeRef(AsHashContainer(hcontain)->HashTree, idx, MakeFixnum(idx));

    while (PairP(lst))
    {
        FAssert(PairP(First(lst)));

        if (eqfn(First(First(lst)), key))
            return(Rest(First(lst)));
        lst = Rest(lst);
    }

    return(def);
}

static void HashContainerSet(FObject hcontain, FObject key, FObject val, FEquivFn eqfn,
    FHashFn hashfn)
{
    FAssert(HashContainerP(hcontain));

    uint_t idx = hashfn(key) % HASH_MODULO;
    FObject slot = HashTreeRef(AsHashContainer(hcontain)->HashTree, idx, MakeFixnum(idx));
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
    if (PairP(AsHashContainer(hcontain)->Tracker) && ObjectP(key))
        InstallTracker(key, kvn, AsHashContainer(hcontain)->Tracker);

//    AsHashContainer(hcontain)->HashTree =
//            HashTreeSet(AsHashContainer(hcontain)->HashTree, idx, kvn);
    Modify(FHashContainer, hcontain, HashTree,
            HashTreeSet(AsHashContainer(hcontain)->HashTree, idx, kvn));

    FAssert(FixnumP(AsHashContainer(hcontain)->Size));

//    AsHashContainer(hcontain)->Size = MakeFixnum(AsFixnum(AsHashContainer(hcontain)->Size) + 1);
    Modify(FHashContainer, hcontain, Size,
            MakeFixnum(AsFixnum(AsHashContainer(hcontain)->Size) + 1));
}

static void HashContainerDelete(FObject hcontain, FObject key, FEquivFn eqfn, FHashFn hashfn)
{
    FAssert(HashContainerP(hcontain));

    uint_t idx = hashfn(key) % HASH_MODULO;
    FObject lst = HashTreeRef(AsHashContainer(hcontain)->HashTree, idx, MakeFixnum(idx));
    FObject prev = NoValueObject;

    while (PairP(lst))
    {
        FAssert(PairP(First(lst)));

        if (eqfn(First(First(lst)), key))
        {
            FAssert(FixnumP(AsHashContainer(hcontain)->Size));
            FAssert(AsFixnum(AsHashContainer(hcontain)->Size) > 0);

//            AsHashContainer(hcontain)->Size =
//                    MakeFixnum(AsFixnum(AsHashContainer(hcontain)->Size) - 1);
            Modify(FHashContainer, hcontain, Size,
                    MakeFixnum(AsFixnum(AsHashContainer(hcontain)->Size) - 1));

            if (PairP(prev))
                SetRest(prev, Rest(lst));
            else if (PairP(Rest(lst)))
            {
//                AsHashContainer(hcontain)->HashTree =
//                        HashTreeSet(AsHashContainer(hcontain)->HashTree, idx, Rest(lst));
                Modify(FHashContainer, hcontain, HashTree,
                        HashTreeSet(AsHashContainer(hcontain)->HashTree, idx, Rest(lst)));
            }
            else
            {
//                AsHashContainer(hcontain)->HashTree =
//                        HashTreeDelete(AsHashContainer(hcontain)->HashTree, idx);
                Modify(FHashContainer, hcontain, HashTree,
                        HashTreeDelete(AsHashContainer(hcontain)->HashTree, idx));
            }
            break;
        }
        prev = lst;
        lst = Rest(lst);
    }
}

static void VisitHashContainerBucket(FObject lst, uint_t idx, FVisitFn vfn, FObject ctx)
{
    while (PairP(lst))
    {
        FAssert(PairP(First(lst)));

        vfn(First(First(lst)), Rest(First(lst)), ctx);
        lst = Rest(lst);
    }
}

static void HashContainerVisit(FObject hcontain, FVisitFn vfn, FObject ctx)
{
    FAssert(HashContainerP(hcontain));

    HashTreeVisit(AsHashContainer(hcontain)->HashTree, VisitHashContainerBucket, vfn, ctx);
}

static uint_t OldIndex(FObject kvn)
{
    while (PairP(kvn))
        kvn = Rest(kvn);

    FAssert(FixnumP(kvn));

    return(AsFixnum(kvn));
}

static void RemoveOld(FObject hcontain, FObject kvn, uint_t idx)
{
    FObject node = HashTreeRef(AsHashContainer(hcontain)->HashTree, idx, MakeFixnum(idx));
    FObject prev = NoValueObject;

    while (PairP(node))
    {
        if (node == kvn)
        {
            if (PairP(prev))
                SetRest(prev, Rest(node));
            else if (PairP(Rest(node)))
            {
//                AsHashContainer(hcontain)->HashTree =
//                        HashTreeSet(AsHashContainer(hcontain)->HashTree, idx, Rest(node));
                Modify(FHashContainer, hcontain, HashTree,
                        HashTreeSet(AsHashContainer(hcontain)->HashTree, idx, Rest(node)));
            }
            else
            {
//                AsHashContainer(hcontain)->HashTree =
//                        HashTreeDelete(AsHashContainer(hcontain)->HashTree, idx);
                Modify(FHashContainer, hcontain, HashTree,
                        HashTreeDelete(AsHashContainer(hcontain)->HashTree, idx));
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
        FAssert(EqHash(First(First(lst))) % HASH_MODULO == idx);

        lst = Rest(lst);
    }

    FAssert(FixnumP(lst));
    FAssert(AsFixnum(lst) == (int_t) idx);
}

static void CheckEqHashContainer(FObject hcontain)
{
    FAssert(HashContainerP(hcontain));

    HashTreeVisit(AsHashContainer(hcontain)->HashTree, CheckVisitBucket, 0, 0);
}
#endif // FOMENT_DEBUG

static void EqHashContainerRehash(FObject hcontain, FObject tconc)
{
    FObject kvn;

    while (TConcEmptyP(tconc) == 0)
    {
        kvn = TConcRemove(tconc);

        FAssert(PairP(kvn));
        FAssert(PairP(First(kvn)));

        FObject key = First(First(kvn));
        uint_t odx = OldIndex(kvn);
        uint_t idx = EqHash(key) % HASH_MODULO;

        if (idx != odx)
        {
            RemoveOld(hcontain, kvn, odx);
            SetRest(kvn, HashTreeRef(AsHashContainer(hcontain)->HashTree, idx, MakeFixnum(idx)));

//            AsHashContainer(hcontain)->HashTree =
//                    HashTreeSet(AsHashContainer(hcontain)->HashTree, idx, Rest(lst));
            Modify(FHashContainer, hcontain, HashTree,
                    HashTreeSet(AsHashContainer(hcontain)->HashTree, idx, kvn));
        }

        FAssert(ObjectP(key));

        InstallTracker(key, kvn, AsHashContainer(hcontain)->Tracker);
    }

#ifdef FOMENT_DEBUG
    CheckEqHashContainer(hcontain);
#endif // FOMENT_DEBUG
}

static FObject EqHashContainerRef(FObject hcontain, FObject key, FObject def, FEquivFn eqfn,
    FHashFn hashfn)
{
    FAssert(HashContainerP(hcontain));
    FAssert(PairP(AsHashContainer(hcontain)->Tracker));

    if (TConcEmptyP(AsHashContainer(hcontain)->Tracker) == 0)
        EqHashContainerRehash(hcontain, AsHashContainer(hcontain)->Tracker);

    FObject ret = HashContainerRef(hcontain, key, def, EqP, EqHash);

#ifdef FOMENT_DEBUG
    CheckEqHashContainer(hcontain);
#endif // FOMENT_DEBUG

    return(ret);
}

static void EqHashContainerSet(FObject hcontain, FObject key, FObject val, FEquivFn eqfn,
    FHashFn hashfn)
{
    FAssert(HashContainerP(hcontain));
    FAssert(PairP(AsHashContainer(hcontain)->Tracker));

    if (TConcEmptyP(AsHashContainer(hcontain)->Tracker) == 0)
        EqHashContainerRehash(hcontain, AsHashContainer(hcontain)->Tracker);

    HashContainerSet(hcontain, key, val, EqP, EqHash);

#ifdef FOMENT_DEBUG
    CheckEqHashContainer(hcontain);
#endif // FOMENT_DEBUG
}

static void EqHashContainerDelete(FObject hcontain, FObject key, FEquivFn eqfn, FHashFn hashfn)
{
    FAssert(HashContainerP(hcontain));
    FAssert(PairP(AsHashContainer(hcontain)->Tracker));

    if (TConcEmptyP(AsHashContainer(hcontain)->Tracker) == 0)
        EqHashContainerRehash(hcontain, AsHashContainer(hcontain)->Tracker);

    HashContainerDelete(hcontain, key, EqP, EqHash);

#ifdef FOMENT_DEBUG
    CheckEqHashContainer(hcontain);
#endif // FOMENT_DEBUG
}

static void EqHashContainerVisit(FObject hcontain, FVisitFn vfn, FObject ctx)
{
    FAssert(HashContainerP(hcontain));
    FAssert(PairP(AsHashContainer(hcontain)->Tracker));

    if (TConcEmptyP(AsHashContainer(hcontain)->Tracker) == 0)
        EqHashContainerRehash(hcontain, AsHashContainer(hcontain)->Tracker);

    HashContainerVisit(hcontain, vfn, ctx);
}

Define("hash-container-tree-ref", HashContainerTreeRefPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("hash-container-tree-ref", argc);
    HashContainerArgCheck("hash-container-tree-ref", argv[0]);

    return(AsHashContainer(argv[0])->HashTree);
}

Define("hash-container-tree-set!", HashContainerTreeSetPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("hash-container-tree-set!", argc);
    HashContainerArgCheck("hash-container-tree-set!", argv[0]);
    HashTreeArgCheck("hash-container-tree-set!", argv[1]);

//    AsHashContainer(argv[0])->HashTree = argv[1];
    Modify(FHashContainer, argv[0], HashTree, argv[1]);
    return(NoValueObject);
}

Define("hash-container-comparator", HashContainerComparatorPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("hash-container-comparator", argc);
    HashContainerArgCheck("hash-container-comparator", argv[0]);

    return(AsHashContainer(argv[0])->Comparator);
}

Define("hash-container-size", HashContainerSizePrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("hash-container-size", argc);
    HashContainerArgCheck("hash-container-size", argv[0]);

    return(AsHashContainer(argv[0])->Size);
}

Define("hash-container-size-set!", HashContainerSizeSetPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("hash-container-size-set!", argc);
    HashContainerArgCheck("hash-container-size-set!", argv[0]);
    NonNegativeArgCheck("hash-container-size-set!", argv[1], 0);

//    AsHashContainer(argv[0])->Size = argv[1];
    Modify(FHashContainer, argv[0], Size, argv[1]);
    return(NoValueObject);
}

// ---- Hash Map ----

static const char * HashContainerFieldsC[4] = {"hash-tree", "comparator", "tracker", "size"};

static FObject MakeHashMap(FObject comp, FObject tracker)
{
    FAssert(sizeof(FHashMap) == sizeof(HashContainerFieldsC) + sizeof(FRecord));
    FAssert(ComparatorP(comp));
    FAssert(tracker == NoValueObject || PairP(tracker));

    FHashMap * hmap = (FHashMap *) MakeRecord(R.HashMapRecordType);
    hmap->HashTree = MakeHashTree("make-hash-map");
    hmap->Comparator = comp;
    hmap->Tracker = tracker;
    hmap->Size = MakeFixnum(0);

    return(hmap);
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

FObject MakeEqHashMap()
{
    return(MakeHashMap(R.EqComparator, MakeTConc()));
}

FObject EqHashMapRef(FObject hmap, FObject key, FObject def)
{
    FAssert(HashMapP(hmap));

    return(EqHashContainerRef(hmap, key, def, EqP, EqHash));
}

void EqHashMapSet(FObject hmap, FObject key, FObject val)
{
    FAssert(HashMapP(hmap));

    EqHashContainerSet(hmap, key, val, EqP, EqHash);
}

void EqHashMapDelete(FObject hmap, FObject key)
{
    FAssert(HashMapP(hmap));

    EqHashContainerDelete(hmap, key, EqP, EqHash);
}

void EqHashMapVisit(FObject hmap, FVisitFn vfn, FObject ctx)
{
    FAssert(HashMapP(hmap));

    EqHashContainerVisit(hmap, vfn, ctx);
}

Define("make-eq-hash-map", MakeEqHashMapPrimitive)(int_t argc, FObject argv[])
{
    ZeroArgsCheck("make-eq-hash-map", argc);

    return(MakeEqHashMap());
}

Define("eq-hash-map?", EqHashMapPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("eq-hash-map?", argc);

    return((HashMapP(argv[0]) &&
            PairP(AsHashContainer(argv[0])->Tracker)) ? TrueObject : FalseObject);
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

// ---- Hash Set ----

static FObject MakeHashSet(FObject comp, FObject tracker)
{
    FAssert(sizeof(FHashSet) == sizeof(HashContainerFieldsC) + sizeof(FRecord));
    FAssert(ComparatorP(comp));
    FAssert(tracker == NoValueObject || PairP(tracker));

    FHashSet * hset = (FHashSet *) MakeRecord(R.HashSetRecordType);
    hset->HashTree = MakeHashTree("make-hash-set");
    hset->Comparator = comp;
    hset->Tracker = tracker;
    hset->Size = MakeFixnum(0);

    return(hset);
}

Define("make-hash-set", MakeHashSetPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("make-hash-set", argc);
    ComparatorArgCheck("make-hash-set", argv[0]);

    return(MakeHashSet(argv[0], NoValueObject));
}

Define("hash-set?", HashSetPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("hash-set?", argc);

    return(HashSetP(argv[0]) ? TrueObject : FalseObject);
}

FObject MakeEqHashSet()
{
    return(MakeHashSet(R.EqComparator, MakeTConc()));
}

int_t EqHashSetContainsP(FObject hset, FObject elem)
{
    FAssert(HashSetP(hset));

    return(EqHashContainerRef(hset, elem, FalseObject, EqP, EqHash) == TrueObject);
}

void EqHashSetAdjoin(FObject hset, FObject elem)
{
    FAssert(HashSetP(hset));

    EqHashContainerSet(hset, elem, TrueObject, EqP, EqHash);
}

void EqHashSetDelete(FObject hset, FObject elem)
{
    FAssert(HashSetP(hset));

    EqHashContainerDelete(hset, elem, EqP, EqHash);
}

Define("make-eq-hash-set", MakeEqHashSetPrimitive)(int_t argc, FObject argv[])
{
    ZeroArgsCheck("make-eq-hash-set", argc);

    return(MakeEqHashSet());
}

Define("eq-hash-set?", EqHashSetPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("eq-hash-set?", argc);

    return((HashSetP(argv[0]) &&
            PairP(AsHashContainer(argv[0])->Tracker)) ? TrueObject : FalseObject);
}

Define("eq-hash-set-contains?", EqHashSetContainsPPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("eq-hash-set-contains?", argc);
    EqHashSetArgCheck("eq-hash-set-contains?", argv[0]);

    return(EqHashSetContainsP(argv[0], argv[1]) ? TrueObject : FalseObject);
}

Define("eq-hash-set-adjoin!", EqHashSetAdjoinPrimitive)(int_t argc, FObject argv[])
{
    AtLeastOneArgCheck("eq-hash-set-adjoin!", argc);
    EqHashSetArgCheck("eq-hash-set-adjoin!", argv[0]);

    for (int_t adx = 1; adx < argc; adx++)
        EqHashSetAdjoin(argv[0], argv[adx]);
    return(NoValueObject);
}

Define("eq-hash-set-delete!", EqHashSetDeletePrimitive)(int_t argc, FObject argv[])
{
    AtLeastOneArgCheck("eq-hash-set-delete!", argc);
    EqHashSetArgCheck("eq-hash-set-delete!", argv[0]);

    for (int_t adx = 1; adx < argc; adx++)
        EqHashSetDelete(argv[0], argv[adx]);
    return(NoValueObject);
}

// ---- Hash Bag ----

static FObject MakeHashBag(FObject comp, FObject tracker)
{
    FAssert(sizeof(FHashBag) == sizeof(HashContainerFieldsC) + sizeof(FRecord));
    FAssert(ComparatorP(comp));
    FAssert(tracker == NoValueObject || PairP(tracker));

    FHashBag * hbag = (FHashBag *) MakeRecord(R.HashBagRecordType);
    hbag->HashTree = MakeHashTree("make-hash-bag");
    hbag->Comparator = comp;
    hbag->Tracker = tracker;
    hbag->Size = MakeFixnum(0);

    return(hbag);
}

Define("make-hash-bag", MakeHashBagPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("make-hash-bag", argc);
    ComparatorArgCheck("make-hash-bag", argv[0]);

    return(MakeHashBag(argv[0], NoValueObject));
}

Define("hash-bag?", HashBagPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("hash-bag?", argc);

    return(HashBagP(argv[0]) ? TrueObject : FalseObject);
}

// ---- Primitives ----

static FObject Primitives[] =
{
    MakeHashTreePrimitive,
    HashTreePPrimitive,
    HashTreeRefPrimitive,
    HashTreeSetPrimitive,
    HashTreeDeletePrimitive,
    HashBucketsRefPrimitive,
    HashBucketsLengthPrimitive,
    HashTreeBitmapPrimitive,
    HashTreeCopyPrimitive,
    HashContainerTreeRefPrimitive,
    HashContainerTreeSetPrimitive,
    HashContainerComparatorPrimitive,
    HashContainerSizePrimitive,
    HashContainerSizeSetPrimitive,
    MakeHashMapPrimitive,
    HashMapPPrimitive,
    MakeEqHashMapPrimitive,
    EqHashMapPPrimitive,
    EqHashMapRefPrimitive,
    EqHashMapSetPrimitive,
    EqHashMapDeletePrimitive,
    MakeHashSetPrimitive,
    HashSetPPrimitive,
    MakeEqHashSetPrimitive,
    EqHashSetPPrimitive,
    EqHashSetContainsPPrimitive,
    EqHashSetAdjoinPrimitive,
    EqHashSetDeletePrimitive,
    MakeHashBagPrimitive,
    HashBagPPrimitive
};

void SetupHashContainers()
{
    R.HashMapRecordType = MakeRecordTypeC("hash-map",
            sizeof(HashContainerFieldsC) / sizeof(char *), HashContainerFieldsC);
    R.HashSetRecordType = MakeRecordTypeC("hash-set",
            sizeof(HashContainerFieldsC) / sizeof(char *), HashContainerFieldsC);
    R.HashBagRecordType = MakeRecordTypeC("hash-bag",
            sizeof(HashContainerFieldsC) / sizeof(char *), HashContainerFieldsC);
}

void SetupHashContainerPrims()
{
    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);

    FAssert(CStringHash("abcdefghijklmn") == StringHash(MakeStringC("abcdefghijklmn")));
    FAssert(CStringCompare("abc", MakeStringC("abc"))
            == StringCompare(MakeStringC("abc"), MakeStringC("abc")));
    FAssert(CStringCompare("abc", MakeStringC("abcd"))
            == StringCompare(MakeStringC("abc"), MakeStringC("abcd")));
    FAssert(CStringCompare("abcd", MakeStringC("abc"))
            == StringCompare(MakeStringC("abcd"), MakeStringC("abc")));
    FAssert(CStringCompare("abc", MakeStringC("def"))
            == StringCompare(MakeStringC("abc"), MakeStringC("def")));
    FAssert(CStringCompare("def", MakeStringC("abc"))
            == StringCompare(MakeStringC("def"), MakeStringC("abc")));
}
