/*

Foment

*/

#ifdef FOMENT_WINDOWS
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
#include <pthread.h>
#endif // FOMENT_UNIX

#include <string.h>
#include "foment.hpp"
#include "syncthrd.hpp"

EternalSymbol(ThreadSafeSymbol, "thread-safe");
EternalSymbol(WeakKeysSymbol, "weak-keys");
EternalSymbol(EphemeralKeysSymbol, "ephemeral-keys");
EternalSymbol(WeakValuesSymbol, "weak-values");
EternalSymbol(EphemeralValuesSymbol, "ephemeral-values");

// ----------------

static long_t SpecialStringEqualP(FObject str1, FObject str2)
{
    FObject obj;
    const char * cs;

    if (StringP(str1))
    {
        if (StringP(str2))
            return(StringCompare(str1, str2) == 0);

        FAssert(CStringP(str2));

        obj = str1;
        cs = AsCString(str2)->String;
    }
    else
    {
        FAssert(CStringP(str1));

        if (CStringP(str2))
            return(strcmp(AsCString(str1)->String, AsCString(str2)->String) == 0);

        FAssert(StringP(str2));

        obj = str2;
        cs = AsCString(str1)->String;
    }

    ulong_t sdx;
    for (sdx = 0; sdx < StringLength(obj); sdx++)
        if (cs[sdx] == 0 || AsString(obj)->String[sdx] != (FCh) cs[sdx])
            return(0);
    return(cs[sdx] == 0);
}

static uint32_t SpecialStringHash(FObject obj)
{
    if (StringP(obj))
        return(StringHash(obj));

    FAssert(CStringP(obj));

    return(CStringHash(AsCString(obj)->String));
}

// ---- Hash Tables ----

EternalBuiltinType(HashTableType, "hash-table", 0);

#define AsHashTable(obj) ((FHashTable *) (obj))

typedef uint32_t (*FHashFn)(FObject obj);
typedef long_t (*FEqualityP)(FObject obj1, FObject obj2);

typedef struct
{
    FObject BuiltinType;
    FObject Buckets; // VectorP
    FObject TypeTestP; // Comparator.TypeTestP
    FObject EqualityP; // Comparator.EqualityP
    FObject HashFn; // Comparator.HashFn
    FObject Tracker;
    FObject Exclusive;
    FHashFn UseHashFn;
    FEqualityP UseEqualityP;
    ulong_t Size;
    ulong_t InitialCapacity;
    ulong_t Flags;
    ulong_t BrokenCount;
} FHashTable;

inline void UseHashTableArgCheck(const char * who, FObject obj)
{
    if (HashTableP(obj) == 0 || AsHashTable(obj)->UseHashFn == 0)
        RaiseExceptionC(Assertion, who, "expected a hash table", List(obj));
}

inline void HashTableArgCheck(const char * who, FObject obj)
{
    if (HashTableP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a hash table", List(obj));
}

static FObject MakeHashTable(ulong_t cap, FObject ttp, FObject eqp, FObject hashfn, ulong_t flags)
{
    FAssert(cap > 0);
    FAssert(ProcedureP(ttp) || PrimitiveP(ttp));
    FAssert(ProcedureP(eqp) || PrimitiveP(eqp));
    FAssert(ProcedureP(hashfn) || PrimitiveP(hashfn));
    FAssert((flags &  HASH_TABLE_KEYS_MASK) == HASH_TABLE_NORMAL_KEYS ||
            (flags &  HASH_TABLE_KEYS_MASK) == HASH_TABLE_WEAK_KEYS ||
            (flags &  HASH_TABLE_KEYS_MASK) == HASH_TABLE_EPHEMERAL_KEYS);
    FAssert((flags &  HASH_TABLE_VALUES_MASK) == HASH_TABLE_NORMAL_VALUES ||
            (flags &  HASH_TABLE_VALUES_MASK) == HASH_TABLE_WEAK_VALUES ||
            (flags &  HASH_TABLE_VALUES_MASK) == HASH_TABLE_EPHEMERAL_VALUES);

    FHashTable * htbl = (FHashTable *) MakeBuiltin(HashTableType, sizeof(FHashTable), 7,
            "%make-hash-table");
    htbl->Buckets = MakeVector(cap, 0, NoValueObject);
    htbl->TypeTestP = ttp;
    htbl->EqualityP = eqp;
    htbl->HashFn = hashfn;
    if (hashfn == EqHashPrimitive)
    {
        htbl->Tracker = MakeTConc();
        htbl->UseHashFn = EqHash;
        htbl->UseEqualityP = EqP;
    }
    else if (hashfn == StringHashPrimitive)
    {
        htbl->Tracker = NoValueObject;
        htbl->UseHashFn = SpecialStringHash;
        htbl->UseEqualityP = SpecialStringEqualP;
    }
    else if (hashfn == SymbolHashPrimitive)
    {
        htbl->Tracker = NoValueObject;
        htbl->UseHashFn = SymbolHash;
        htbl->UseEqualityP = EqP;
    }
    else
    {
        htbl->Tracker = NoValueObject;
        htbl->UseHashFn = 0;
        htbl->UseEqualityP = 0;
    }

    if (flags & HASH_TABLE_THREAD_SAFE)
        htbl->Exclusive = MakeExclusive();
    else
        htbl->Exclusive = NoValueObject;

    htbl->Size = 0;
    htbl->InitialCapacity = cap;
    htbl->Flags = flags;
    htbl->BrokenCount = 0;

    return(htbl);
}

// ---- Hash Nodes ----

#define HASH_NODE_NORMAL_KEYS      HASH_TABLE_NORMAL_KEYS
#define HASH_NODE_WEAK_KEYS        HASH_TABLE_WEAK_KEYS
#define HASH_NODE_EPHEMERAL_KEYS   HASH_TABLE_EPHEMERAL_KEYS
#define HASH_NODE_KEYS_MASK        HASH_TABLE_KEYS_MASK

#define HASH_NODE_NORMAL_VALUES    HASH_TABLE_NORMAL_VALUES
#define HASH_NODE_WEAK_VALUES      HASH_TABLE_WEAK_VALUES
#define HASH_NODE_EPHEMERAL_VALUES HASH_TABLE_EPHEMERAL_VALUES
#define HASH_NODE_VALUES_MASK      HASH_TABLE_VALUES_MASK

#define HASH_NODE_DELETED          0x10

#define AsHashNode(obj) ((FHashNode *) (obj))
#define HashNodeP(obj) (IndirectTag(obj) == HashNodeTag)

typedef struct
{
    FObject Key;
    FObject Value;
    FObject Next;
    uint32_t Hash;
    uint32_t Flags;
} FHashNode;

static FObject MakeHashNode(FObject htbl, FObject key, FObject val, FObject next, uint32_t hsh,
    const char * who)
{
    FAssert(HashTableP(htbl));

    uint32_t ktype = AsHashTable(htbl)->Flags & HASH_TABLE_KEYS_MASK;
    uint32_t vtype = AsHashTable(htbl)->Flags & HASH_TABLE_VALUES_MASK;
    FHashNode * node = (FHashNode *) MakeObject(HashNodeTag, sizeof(FHashNode), 3, who);

    if (ktype == HASH_NODE_NORMAL_KEYS)
        node->Key = key;
    else if (ktype == HASH_NODE_EPHEMERAL_KEYS)
        node->Key = MakeEphemeron(key, val, htbl);
    else
    {
        FAssert(ktype == HASH_NODE_WEAK_KEYS);

        node->Key = MakeEphemeron(key, key, htbl);
    }

    if (vtype == HASH_NODE_NORMAL_VALUES)
        node->Value = val;
    else if (vtype == HASH_NODE_EPHEMERAL_VALUES)
        node->Value = MakeEphemeron(val, key, htbl);
    else
    {
        FAssert(vtype == HASH_NODE_WEAK_VALUES);

        node->Value = MakeEphemeron(val, val, htbl);
    }

    node->Next = next;
    node->Hash = hsh;
    node->Flags = ktype | vtype;

    return(node);
}

static FObject CopyHashNode(FObject node, FObject next, const char * who)
{
    FAssert(HashNodeP(node));
    FAssert((AsHashNode(node)->Flags & HASH_NODE_DELETED) == 0);

    FHashNode * nnd = (FHashNode *) MakeObject(HashNodeTag, sizeof(FHashNode), 3, who);
    nnd->Key = AsHashNode(node)->Key;
    nnd->Value = AsHashNode(node)->Value;
    nnd->Next = next;
    nnd->Hash = AsHashNode(node)->Hash;
    nnd->Flags = AsHashNode(node)->Flags;

    return(nnd);
}

inline void HashNodeArgCheck(const char * who, FObject obj)
{
    if (HashNodeP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a hash node", List(obj));
}

inline FObject HashNodeKey(FObject node)
{
    FAssert(HashNodeP(node));

    if ((AsHashNode(node)->Flags & HASH_NODE_KEYS_MASK) != HASH_NODE_NORMAL_KEYS)
    {
        FAssert(EphemeronP(AsHashNode(node)->Key));

        return(AsEphemeron(AsHashNode(node)->Key)->Key);
    }

    return(AsHashNode(node)->Key);
}

inline FObject HashNodeValue(FObject node)
{
    FAssert(HashNodeP(node));

    if ((AsHashNode(node)->Flags & HASH_NODE_VALUES_MASK) != HASH_NODE_NORMAL_VALUES)
    {
        FAssert(EphemeronP(AsHashNode(node)->Value));

        return(AsEphemeron(AsHashNode(node)->Value)->Key);
    }

    return(AsHashNode(node)->Value);
}

inline void HashNodeValueSet(FObject node, FObject val)
{
    FAssert(HashNodeP(node));

    if ((AsHashNode(node)->Flags & HASH_NODE_KEYS_MASK) == HASH_NODE_EPHEMERAL_KEYS)
    {
        FAssert(EphemeronP(AsHashNode(node)->Key));

        EphemeronDatumSet(AsHashNode(node)->Key, val);
    }

    if ((AsHashNode(node)->Flags & HASH_NODE_VALUES_MASK) == HASH_NODE_NORMAL_VALUES)
    {
//        AsHashNode(node)->Value = val;
        Modify(FHashNode, node, Value, val);
    }
    else if ((AsHashNode(node)->Flags & HASH_NODE_VALUES_MASK) == HASH_NODE_EPHEMERAL_VALUES)
    {
        FAssert(EphemeronP(AsHashNode(node)->Value));

        EphemeronKeySet(AsHashNode(node)->Value, val);
    }
    else
    {
        FAssert((AsHashNode(node)->Flags & HASH_NODE_VALUES_MASK) == HASH_NODE_WEAK_VALUES);
        FAssert(EphemeronP(AsHashNode(node)->Value));

        EphemeronKeySet(AsHashNode(node)->Value, val);
        EphemeronDatumSet(AsHashNode(node)->Value, val);
    }
}

inline long_t HashNodeBrokenP(FObject node)
{
    FAssert(HashNodeP(node));

    if ((AsHashNode(node)->Flags & HASH_NODE_KEYS_MASK) != HASH_NODE_NORMAL_KEYS)
    {
        FAssert(EphemeronP(AsHashNode(node)->Key));

        if (EphemeronBrokenP(AsHashNode(node)->Key))
            return(1);
    }

    if ((AsHashNode(node)->Flags & HASH_NODE_VALUES_MASK) != HASH_NODE_NORMAL_VALUES)
    {
        FAssert(EphemeronP(AsHashNode(node)->Value));

        if (EphemeronBrokenP(AsHashNode(node)->Value))
            return(1);
    }

    return(0);
}

static FObject CopyHashNodeList(FObject htbl, FObject nlst, FObject skp, const char * who)
{
    if (HashNodeP(nlst))
    {
        if (nlst == skp || (AsHashNode(nlst)->Flags & HASH_NODE_DELETED))
            return(CopyHashNodeList(htbl, AsHashNode(nlst)->Next, skp, who));
        if (HashNodeBrokenP(nlst))
        {
            FAssert(HashTableP(htbl));
            FAssert(AsHashTable(htbl)->Size > 0);

            AsHashTable(htbl)->Size -= 1;

            return(CopyHashNodeList(htbl, AsHashNode(nlst)->Next, skp, who));
        }
        return(CopyHashNode(nlst, CopyHashNodeList(htbl, AsHashNode(nlst)->Next, skp, who), who));
    }

    FAssert(nlst == NoValueObject);

    return(nlst);
}

static FObject MakeHashNodeList(FObject htbl, FObject nlst)
{
    if (HashNodeP(nlst))
    {
        if ((AsHashNode(nlst)->Flags & HASH_NODE_DELETED) || HashNodeBrokenP(nlst))
            return(MakeHashNodeList(htbl, AsHashNode(nlst)->Next));

        AsHashTable(htbl)->Size += 1;
        return(MakeHashNode(htbl, HashNodeKey(nlst), HashNodeValue(nlst),
                MakeHashNodeList(htbl, AsHashNode(nlst)->Next), AsHashNode(nlst)->Hash,
                "hash-table-copy"));
    }

    FAssert(nlst == NoValueObject);

    return(nlst);
}

// ----------------

void HashTableEphemeronBroken(FObject htbl)
{
    FAssert(HashTableP(htbl));

    AsHashTable(htbl)->BrokenCount += 1;
}

static void
HashTableClean(FObject htbl)
{
    FAssert(HashTableP(htbl));
    FAssert(VectorP(AsHashTable(htbl)->Buckets));

    FObject buckets = AsHashTable(htbl)->Buckets;
    for (ulong_t idx = 0; idx < VectorLength(buckets); idx++)
    {
//        AsVector(buckets)->Vector[idx] =
//                CopyHashNodeList(htbl, AsVector(buckets)->Vector[idx], NoValueObject,
//                        "%hash-table-clean");
        ModifyVector(buckets, idx,
                CopyHashNodeList(htbl, AsVector(buckets)->Vector[idx], NoValueObject,
                        "%hash-table-clean"));
    }

    AsHashTable(htbl)->BrokenCount = 0;
}

static void
HashTableAdjust(FObject htbl, ulong_t sz)
{
    FAssert(HashTableP(htbl));
    FAssert(VectorP(AsHashTable(htbl)->Buckets));

    AsHashTable(htbl)->Size = sz;

    FObject buckets = AsHashTable(htbl)->Buckets;
    ulong_t cap = VectorLength(buckets);
    if (AsHashTable(htbl)->Size > cap * 2)
        cap *= 2;
    else if (AsHashTable(htbl)->Size * 16 < cap && cap > AsHashTable(htbl)->InitialCapacity)
        cap /= 2;
    else
    {
        if (AsHashTable(htbl)->BrokenCount * 5 > AsHashTable(htbl)->Size)
        {
            FAssert((AsHashTable(htbl)->Flags & HASH_TABLE_KEYS_MASK) != HASH_TABLE_NORMAL_KEYS ||
                    (AsHashTable(htbl)->Flags & HASH_TABLE_VALUES_MASK) != HASH_TABLE_NORMAL_VALUES);

            HashTableClean(htbl);
        }

        return;
    }

    FHashFn UseHashFn = AsHashTable(htbl)->UseHashFn;
    FObject nbuckets = MakeVector(cap, 0, NoValueObject);
    for (ulong_t idx = 0; idx < VectorLength(buckets); idx++)
    {
        FObject nlst = AsVector(buckets)->Vector[idx];

        while (HashNodeP(nlst))
        {
            FObject node = nlst;
            nlst = AsHashNode(nlst)->Next;

            if (HashNodeBrokenP(node))
            {
                FAssert(AsHashTable(htbl)->Size > 0);

                AsHashTable(htbl)->Size -= 1;
            }
            else
            {
                ulong_t ndx = AsHashNode(node)->Hash % cap;

                if (UseHashFn == EqHash)
                {
//                    AsHashNode(node)->Next = AsVector(nbuckets)->Vector[ndx];
                    Modify(FHashNode, node, Next, AsVector(nbuckets)->Vector[ndx]);
                }
                else
                    node = CopyHashNode(node, AsVector(nbuckets)->Vector[ndx], "%hash-table-adjust");

//                AsVector(nbuckets)->Vector[ndx] = node;
                ModifyVector(nbuckets, ndx, node);
            }
        }
    }

//    AsHashTable(htbl)->Buckets = nbuckets;
    Modify(FHashTable, htbl, Buckets, nbuckets);
    AsHashTable(htbl)->BrokenCount = 0;
}

// ---- Eq Hash Tables ----

Define("any?", AnyPPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("any?", argc);

    return(TrueObject);
}

FObject MakeEqHashTable(ulong_t cap, ulong_t flags)
{
    return(MakeHashTable(cap, AnyPPrimitive, EqPPrimitive, EqHashPrimitive, flags));
}

static void RehashEqHashTable(FObject htbl)
{
    FAssert(HashTableP(htbl));
    FAssert(VectorP(AsHashTable(htbl)->Buckets));

    FObject buckets = AsHashTable(htbl)->Buckets;
    FObject tconc = AsHashTable(htbl)->Tracker;
    while (TConcEmptyP(tconc) == 0)
    {
        FObject node = TConcRemove(tconc);

        FAssert(HashNodeP(node));

        if ((AsHashNode(node)->Flags & HASH_NODE_DELETED) == 0 && HashNodeBrokenP(node) == 0)
        {
            FObject key = HashNodeKey(node);

            FAssert(AsHashNode(node)->Hash != EqHash(key));

            uint32_t nhsh = EqHash(key);
            uint32_t ndx = nhsh % VectorLength(buckets);
            uint32_t odx = AsHashNode(node)->Hash % VectorLength(buckets);

            if (odx != ndx)
            {
                if (AsVector(buckets)->Vector[odx] == node)
                {
//                    AsVector(buckets)->Vector[odx] = AsHashNode(node)->Next;
                    ModifyVector(buckets, odx, AsHashNode(node)->Next);
                }
                else
                {
                    FObject prev = AsVector(buckets)->Vector[odx];
                    while (AsHashNode(prev)->Next != node)
                    {
                        prev = AsHashNode(prev)->Next;

                        FAssert(HashNodeP(prev));
                    }

                    AsHashNode(prev)->Next = AsHashNode(node)->Next;
                }

//                AsHashNode(node)->Next = AsVector(buckets)->Vector[ndx];
                Modify(FHashNode, node, Next, AsVector(buckets)->Vector[ndx]);
//                AsVector(buckets)->Vector[ndx] = node;
                ModifyVector(buckets, ndx, node);
            }

            AsHashNode(node)->Hash = nhsh;
            InstallTracker(key, node, tconc);
        }
    }
}

// ---- Hash Tables ----

FObject MakeStringHashTable(ulong_t cap, ulong_t flags)
{
    return(MakeHashTable(cap, StringPPrimitive, StringEqualPPrimitive, StringHashPrimitive, flags));
}

FObject MakeSymbolHashTable(ulong_t cap, ulong_t flags)
{
    return(MakeHashTable(cap, SymbolPPrimitive, EqPPrimitive, SymbolHashPrimitive, flags));
}

static FObject RefHashTable(FObject htbl, FObject key, FObject def)
{
    FAssert(HashTableP(htbl));
    FAssert(VectorP(AsHashTable(htbl)->Buckets));
    FAssert(AsHashTable(htbl)->UseEqualityP != 0);
    FAssert(AsHashTable(htbl)->UseHashFn != 0);

    FHashFn UseHashFn = AsHashTable(htbl)->UseHashFn;
    FEqualityP UseEqualityP = AsHashTable(htbl)->UseEqualityP;

    FObject buckets = AsHashTable(htbl)->Buckets;
    uint32_t hsh = UseHashFn(key);
    uint32_t idx = hsh % VectorLength(buckets);
    FObject node = AsVector(buckets)->Vector[idx];

    while (HashNodeP(node))
    {
        if (UseEqualityP(HashNodeKey(node), key) && HashNodeBrokenP(node) == 0)
        {
            FAssert(AsHashNode(node)->Hash == hsh);

            return(HashNodeValue(node));
        }

        node = AsHashNode(node)->Next;
    }

    return(def);
}

FObject HashTableRef(FObject htbl, FObject key, FObject def)
{
    FAssert(HashTableP(htbl));

    if (AsHashTable(htbl)->UseHashFn == EqHash)
    {
        if (ExclusiveP(AsHashTable(htbl)->Exclusive))
        {
            FWithExclusive we(AsHashTable(htbl)->Exclusive);

            RehashEqHashTable(htbl);
            return(RefHashTable(htbl, key, def));
        }

        RehashEqHashTable(htbl);
    }

    return(RefHashTable(htbl, key, def));
}

static void SetHashTable(FObject htbl, FObject key, FObject val)
{
    FAssert(HashTableP(htbl));
    FAssert(VectorP(AsHashTable(htbl)->Buckets));
    FAssert(AsHashTable(htbl)->UseEqualityP != 0);
    FAssert(AsHashTable(htbl)->UseHashFn != 0);
    FAssert((AsHashTable(htbl)->Flags & HASH_TABLE_IMMUTABLE) == 0);

    FHashFn UseHashFn = AsHashTable(htbl)->UseHashFn;
    FEqualityP UseEqualityP = AsHashTable(htbl)->UseEqualityP;

    if (UseHashFn == EqHash)
        RehashEqHashTable(htbl);

    FObject buckets = AsHashTable(htbl)->Buckets;
    uint32_t hsh = UseHashFn(key);
    uint32_t idx = hsh % VectorLength(buckets);
    FObject nlst = AsVector(buckets)->Vector[idx];
    FObject node = nlst;

    while (HashNodeP(node))
    {
        if (UseEqualityP(HashNodeKey(node), key) && HashNodeBrokenP(node) == 0)
        {
            FAssert(AsHashNode(node)->Hash == hsh);

            HashNodeValueSet(node, val);
            return;
        }

        node = AsHashNode(node)->Next;
    }

    node = MakeHashNode(htbl, key, val, nlst, hsh, "hash-table-set!");
//    AsVector(buckets)->Vector[idx] = node;
    ModifyVector(buckets, idx, node);
    if (UseHashFn == EqHash && ObjectP(key))
        InstallTracker(key, node, AsHashTable(htbl)->Tracker);

    HashTableAdjust(htbl, AsHashTable(htbl)->Size + 1);
}

void HashTableSet(FObject htbl, FObject key, FObject val)
{
    FAssert(HashTableP(htbl));

    if (ExclusiveP(AsHashTable(htbl)->Exclusive))
    {
        FWithExclusive we(AsHashTable(htbl)->Exclusive);

        SetHashTable(htbl, key, val);
    }
    else
        SetHashTable(htbl, key, val);
}

static void DeleteHashTable(FObject htbl, FObject key)
{
    FAssert(HashTableP(htbl));
    FAssert(VectorP(AsHashTable(htbl)->Buckets));
    FAssert(AsHashTable(htbl)->UseEqualityP != 0);
    FAssert(AsHashTable(htbl)->UseHashFn != 0);
    FAssert((AsHashTable(htbl)->Flags & HASH_TABLE_IMMUTABLE) == 0);

    FHashFn UseHashFn = AsHashTable(htbl)->UseHashFn;
    FEqualityP UseEqualityP = AsHashTable(htbl)->UseEqualityP;

    if (UseHashFn == EqHash)
        RehashEqHashTable(htbl);

    FObject buckets = AsHashTable(htbl)->Buckets;
    uint32_t hsh = UseHashFn(key);
    uint32_t idx = hsh % VectorLength(buckets);
    FObject nlst = AsVector(buckets)->Vector[idx];
    FObject node = nlst;
    FObject prev = NoValueObject;

    while (HashNodeP(node))
    {
        if (UseEqualityP(HashNodeKey(node), key) && HashNodeBrokenP(node) == 0)
        {
            FAssert(AsHashNode(node)->Hash == hsh);
            FAssert((AsHashNode(node)->Flags & HASH_NODE_DELETED) == 0);

            AsHashNode(node)->Flags |= HASH_NODE_DELETED;

            if (node == nlst)
            {
//                AsVector(buckets)->Vector[idx] = AsHashNode(node)->Next;
                ModifyVector(buckets, idx, AsHashNode(node)->Next);
            }
            else if (UseHashFn == EqHash)
            {
                FAssert(HashNodeP(prev));
                FAssert(AsHashNode(prev)->Next == node);

//                AsHashNode(prev)->Next = AsHashNode(node)->Next;
                Modify(FHashNode, prev, Next, AsHashNode(node)->Next);
            }
            else
            {
//                AsVector(buckets)->Vector[idx] = CopyHashNodeList(nlst, node);
                ModifyVector(buckets, idx, CopyHashNodeList(htbl, nlst, node, "%hash-table-delete"));
            }

            HashTableAdjust(htbl, AsHashTable(htbl)->Size - 1);
            return;
        }

        prev = node;
        node = AsHashNode(node)->Next;
    }
}

void HashTableDelete(FObject htbl, FObject key)
{
    FAssert(HashTableP(htbl));
    if (ExclusiveP(AsHashTable(htbl)->Exclusive))
    {
        FWithExclusive we(AsHashTable(htbl)->Exclusive);

        DeleteHashTable(htbl, key);
    }
    else
        DeleteHashTable(htbl, key);
}

static FObject FoldHashTable(FObject htbl, FFoldFn foldfn, void * ctx, FObject seed)
{
    FAssert(HashTableP(htbl));
    FAssert(VectorP(AsHashTable(htbl)->Buckets));

    FObject buckets = AsHashTable(htbl)->Buckets;
    FObject accum = seed;

    for (ulong_t idx = 0; idx < VectorLength(buckets); idx++)
    {
        FObject nlst = AsVector(buckets)->Vector[idx];

        while (HashNodeP(nlst))
        {
            if (HashNodeBrokenP(nlst) == 0)
                accum = foldfn(HashNodeKey(nlst), HashNodeValue(nlst), ctx, accum);
            nlst = AsHashNode(nlst)->Next;
        }
    }

    return(accum);
}

FObject HashTableFold(FObject htbl, FFoldFn foldfn, void * ctx, FObject seed)
{
    FAssert(HashTableP(htbl));

    if (AsHashTable(htbl)->UseHashFn == EqHash)
    {
        if (ExclusiveP(AsHashTable(htbl)->Exclusive))
        {
            FWithExclusive we(AsHashTable(htbl)->Exclusive);

            RehashEqHashTable(htbl);
            return(FoldHashTable(htbl, foldfn, ctx, seed));
        }

        RehashEqHashTable(htbl);
    }

    return(FoldHashTable(htbl, foldfn, ctx, seed));
}

static FObject HashTablePop(FObject htbl)
{
    FAssert(HashTableP(htbl));
    FAssert(VectorP(AsHashTable(htbl)->Buckets));

    if (AsHashTable(htbl)->Size == 0)
        RaiseExceptionC(Assertion, "%hash-table-pop!", "hash table is empty", List(htbl));

    if (AsHashTable(htbl)->UseHashFn == EqHash)
        RehashEqHashTable(htbl);

    FObject buckets = AsHashTable(htbl)->Buckets;

    for (ulong_t idx = 0; idx < VectorLength(buckets); idx++)
    {
        FObject nlst = AsVector(buckets)->Vector[idx];

        if (HashNodeP(nlst))
        {
//            AsVector(buckets)->Vector[idx] = AsHashNode(nlst)->Next;
            ModifyVector(buckets, idx, AsHashNode(nlst)->Next);

            HashTableAdjust(htbl, AsHashTable(htbl)->Size - 1);

            AsHashNode(nlst)->Flags |= HASH_NODE_DELETED;
            return(nlst);
        }
    }

    FAssert(0);

    return(NoValueObject);
}

static void HashTableClear(FObject htbl)
{
    FAssert(HashTableP(htbl));

//    AsHashTable(htbl)->Buckets = MakeVector(AsHashTable(htbl)->InitialCapacity, 0, NoValueObject);
    Modify(FHashTable, htbl, Buckets,
            MakeVector(AsHashTable(htbl)->InitialCapacity, 0, NoValueObject));
    AsHashTable(htbl)->Size = 0;
}

static FObject HashTableCopy(FObject htbl)
{
    FAssert(HashTableP(htbl));
    FAssert(VectorP(AsHashTable(htbl)->Buckets));

    FObject nhtbl = MakeHashTable(VectorLength(AsHashTable(htbl)->Buckets),
            AsHashTable(htbl)->TypeTestP, AsHashTable(htbl)->EqualityP, AsHashTable(htbl)->HashFn,
            AsHashTable(htbl)->Flags & ~HASH_TABLE_IMMUTABLE);
    AsHashTable(nhtbl)->InitialCapacity = AsHashTable(htbl)->InitialCapacity;

    if (AsHashTable(htbl)->UseHashFn == EqHash)
    {
        FAssert(AsHashTable(nhtbl)->UseHashFn == EqHash);

        RehashEqHashTable(htbl);
    }

    FObject buckets = AsHashTable(htbl)->Buckets;
    FObject nbuckets = AsHashTable(nhtbl)->Buckets;

    FAssert(VectorP(nbuckets));
    FAssert(VectorLength(buckets) == VectorLength(nbuckets));

    for (ulong_t idx = 0; idx < VectorLength(buckets); idx++)
    {
//        AsVector(nbuckets)->Vector[idx] = MakeHashNodeList(nhtbl, AsVector(buckets)->Vector[idx]);
        ModifyVector(nbuckets, idx, MakeHashNodeList(nhtbl, AsVector(buckets)->Vector[idx]));
    }

    return(nhtbl);
}

Define("hash-table?", HashTablePPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("hash-table?", argc);

    return(HashTableP(argv[0]) ? TrueObject : FalseObject);
}

Define("%eq-hash-table?", EqHashTablePPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("%eq-hash-table?", argc);

    return(HashTableP(argv[0]) && AsHashTable(argv[0])->UseHashFn == EqHash ? TrueObject :
            FalseObject);
}

Define("make-eq-hash-table", MakeEqHashTablePrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("make-eq-hash-table", argc);

    return(MakeEqHashTable(128, 0));
}

Define("%make-hash-table", MakeHashTablePrimitive)(long_t argc, FObject argv[])
{
    FourArgsCheck("%make-hash-table", argc);
    ProcedureArgCheck("%make-hash-table", argv[0]);
    ProcedureArgCheck("%make-hash-table", argv[1]);
    ProcedureArgCheck("%make-hash-table", argv[2]);

    ulong_t cap = 128;
    ulong_t flags = 0;

    FObject args = argv[3];
    while (PairP(args))
    {
        FObject arg = First(args);

        if (FixnumP(arg) && AsFixnum(arg) >= 0)
            cap = AsFixnum(arg);
        else if (arg == ThreadSafeSymbol)
            flags |= HASH_TABLE_THREAD_SAFE;
        else if (arg == WeakKeysSymbol)
            flags |= HASH_TABLE_WEAK_KEYS;
        else if (arg == EphemeralKeysSymbol)
            flags |= HASH_TABLE_EPHEMERAL_KEYS;
        else if (arg == WeakValuesSymbol)
            flags |= HASH_TABLE_WEAK_VALUES;
        else if (arg == EphemeralValuesSymbol)
            flags |= HASH_TABLE_EPHEMERAL_VALUES;
        else
            RaiseExceptionC(Assertion, "make-hash-table", "unsupported optional argument(s)",
                    argv[3]);

        args = Rest(args);
    }

    if ((flags & HASH_TABLE_KEYS_MASK) == (HASH_TABLE_WEAK_KEYS | HASH_TABLE_EPHEMERAL_KEYS))
        RaiseExceptionC(Assertion, "make-hash-table",
                "must specify as most one of weak-keys and ephemeron-keys", argv[3]);

    if ((flags & HASH_TABLE_VALUES_MASK) == (HASH_TABLE_WEAK_VALUES | HASH_TABLE_EPHEMERAL_VALUES))
        RaiseExceptionC(Assertion, "make-hash-table",
                "must specify as most one of weak-values and ephemeron-values", argv[3]);

    return(MakeHashTable(cap, argv[0], argv[1], argv[2], flags));
}

Define("%hash-table-buckets", HashTableBucketsPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("%hash-table-buckets", argc);
    HashTableArgCheck("%hash-table-buckets", argv[0]);

    return(AsHashTable(argv[0])->Buckets);
}

Define("%hash-table-buckets-set!", HashTableBucketsSetPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("%hash-table-buckets-set!", argc);
    HashTableArgCheck("%hash-table-buckets-set!", argv[0]);
    VectorArgCheck("%hash-table-buckets-set!", argv[1]);

//    AsHashTable(argv[0])->Buckets = argv[1];
    Modify(FHashTable, argv[0], Buckets, argv[1]);
    return(NoValueObject);
}

Define("%hash-table-type-test-predicate", HashTableTypeTestPredicatePrimitive)(long_t argc,
    FObject argv[])
{
    OneArgCheck("%hash-table-type-test-predicate", argc);
    HashTableArgCheck("%hash-table-type-test-predicate", argv[0]);

    return(AsHashTable(argv[0])->TypeTestP);
}

Define("%hash-table-equality-predicate", HashTableEqualityPredicatePrimitive)(long_t argc,
    FObject argv[])
{
    OneArgCheck("%hash-table-equality-predicate", argc);
    HashTableArgCheck("%hash-table-equality-predicate", argv[0]);

    return(AsHashTable(argv[0])->EqualityP);
}

Define("hash-table-hash-function", HashTableHashFunctionPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("hash-table-hash-function", argc);
    HashTableArgCheck("hash-table-hash-function", argv[0]);

    return(AsHashTable(argv[0])->HashFn);
}

Define("%hash-table-pop!", HashTablePopPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("%hash-table-pop!", argc);
    HashTableArgCheck("%hash-table-pop!", argv[0]);

    return(HashTablePop(argv[0]));
}

Define("hash-table-clear!", HashTableClearPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("hash-table-clear!", argc);
    HashTableArgCheck("hash-table-clear!", argv[0]);

    HashTableClear(argv[0]);
    return(NoValueObject);
}

Define("hash-table-size", HashTableSizePrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("hash-table-size", argc);
    HashTableArgCheck("hash-table-size", argv[0]);

    return(MakeFixnum(AsHashTable(argv[0])->Size));
}

Define("%hash-table-adjust!", HashTableAdjustPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("%hash-table-adjust!", argc);
    HashTableArgCheck("%hash-table-adjust!", argv[0]);
    NonNegativeArgCheck("%hash-table-adjust!", argv[1], 0);

    HashTableAdjust(argv[0], AsFixnum(argv[1]));
    return(NoValueObject);
}

Define("hash-table-mutable?", HashTableMutablePPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("hash-table-mutable?", argc);
    HashTableArgCheck("hash-table-mutable?", argv[0]);

    return((AsHashTable(argv[0])->Flags & HASH_TABLE_IMMUTABLE) ? FalseObject : TrueObject);
}

Define("%hash-table-immutable!", HashTableImmutablePrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("%hash-table-immutable!", argc);
    HashTableArgCheck("%hash-table-immutable!", argv[0]);

    AsHashTable(argv[0])->Flags |= HASH_TABLE_IMMUTABLE;
    return(NoValueObject);
}

Define("%hash-table-exclusive", HashTableExclusivePrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("%hash-table-exclusive", argc);
    HashTableArgCheck("%hash-table-exclusive", argv[0]);

    return(AsHashTable(argv[0])->Exclusive);
}

Define("%hash-table-ref", HashTableRefPrimitive)(long_t argc, FObject argv[])
{
    ThreeArgsCheck("%hash-table-ref", argc);
    UseHashTableArgCheck("%hash-table-ref", argv[0]);

    return(HashTableRef(argv[0], argv[1], argv[2]));
}

Define("%hash-table-set!", HashTableSetPrimitive)(long_t argc, FObject argv[])
{
    ThreeArgsCheck("%hash-table-set!", argc);
    UseHashTableArgCheck("%hash-table-set!", argv[0]);

    HashTableSet(argv[0], argv[1], argv[2]);
    return(NoValueObject);
}

Define("%hash-table-delete!", HashTableDeletePrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("%hash-table-delete", argc);
    UseHashTableArgCheck("%hash-table-delete", argv[0]);

    HashTableDelete(argv[0], argv[1]);
    return(NoValueObject);
}

Define("hash-table-empty-copy", HashTableEmptyCopyPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("hash-table-empty-copy", argc);
    HashTableArgCheck("hash-table-empty-copy", argv[0]);

    return(MakeHashTable(AsHashTable(argv[0])->InitialCapacity, AsHashTable(argv[0])->TypeTestP,
            AsHashTable(argv[0])->EqualityP, AsHashTable(argv[0])->HashFn,
            AsHashTable(argv[0])->Flags & ~HASH_TABLE_IMMUTABLE));
}

Define("%hash-table-copy", HashTableCopyPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("%hash-table-copy", argc);
    HashTableArgCheck("%hash-table-copy", argv[0]);

    return(HashTableCopy(argv[0]));
}

static FObject FoldAList(FObject key, FObject val, void * ctx, FObject lst)
{
    return(MakePair(MakePair(key, val), lst));
}

Define("hash-table->alist", HashTableToAlistPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("hash-table->alist", argc);
    HashTableArgCheck("%hash-table->alist", argv[0]);

    return(HashTableFold(argv[0], FoldAList, 0, EmptyListObject));
}

static FObject FoldKeys(FObject key, FObject val, void * ctx, FObject lst)
{
    return(MakePair(key, lst));
}

static FObject FoldValues(FObject key, FObject val, void * ctx, FObject lst)
{
    return(MakePair(val, lst));
}

static FObject FoldEntries(FObject key, FObject val, void * ctx, FObject lst)
{
    *((FObject *) ctx) = MakePair(val, *((FObject *) ctx));
    return(MakePair(key, lst));
}

Define("hash-table-keys", HashTableKeysPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("hash-table-keys", argc);
    HashTableArgCheck("hash-table-keys", argv[0]);

    return(HashTableFold(argv[0], FoldKeys, 0, EmptyListObject));
}

Define("hash-table-values", HashTableValuesPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("hash-table-values", argc);
    HashTableArgCheck("hash-table-values", argv[0]);

    return(HashTableFold(argv[0], FoldValues, 0, EmptyListObject));
}

Define("%hash-table-entries", HashTableEntriesPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("%hash-table-entries", argc);
    HashTableArgCheck("%hash-table-entries", argv[0]);

    FObject vlst = EmptyListObject;
    return(MakePair(HashTableFold(argv[0], FoldEntries, &vlst, EmptyListObject), vlst));
}

// ---- Symbols ----

static long_t SpecialStringLengthEqualP(FCh * s, long_t sl, FObject str)
{
    if (StringP(str))
        return(StringLengthEqualP(s, sl, str));

    FAssert(CStringP(str));

    const char * cs = AsCString(str)->String;
    long_t sdx;
    for (sdx = 0; sdx < sl; sdx++)
        if (cs[sdx] == 0 || s[sdx] != (FCh) cs[sdx])
            return(0);
    return(cs[sdx] == 0);
}

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

    FObject sym = HashTableRef(SymbolHashTable, str, NoValueObject);
    if (sym == NoValueObject)
    {
        sym = (FSymbol *) MakeObject(SymbolTag, sizeof(FSymbol), 1, "string->symbol");
        AsSymbol(sym)->String = MakeString(AsString(str)->String, StringLength(str));
        AsSymbol(sym)->Hash = StringHash(str);

        HashTableSet(SymbolHashTable, AsSymbol(sym)->String, sym);
    }

    return(sym);
}

FObject StringLengthToSymbol(FCh * s, long_t sl)
{
    FAssert((AsHashTable(SymbolHashTable)->Flags & HASH_TABLE_KEYS_MASK) == HASH_TABLE_NORMAL_KEYS);
    FAssert((AsHashTable(SymbolHashTable)->Flags & HASH_TABLE_VALUES_MASK) ==
            HASH_TABLE_NORMAL_VALUES);

    FObject buckets = AsHashTable(SymbolHashTable)->Buckets;
    uint32_t hsh = StringLengthHash(s, sl);
    uint32_t idx = hsh % VectorLength(buckets);
    FObject nlst = AsVector(buckets)->Vector[idx];
    FObject node = nlst;

    while (HashNodeP(node))
    {
        FObject key = HashNodeKey(node);

        FAssert(StringP(key) || CStringP(key));

        if (SpecialStringLengthEqualP(s, sl, key))
        {
            FAssert(AsHashNode(node)->Hash == hsh);
            FAssert(SymbolP(HashNodeValue(node)));

            return(HashNodeValue(node));
        }

        node = AsHashNode(node)->Next;
    }

    FSymbol * sym = (FSymbol *) MakeObject(SymbolTag, sizeof(FSymbol), 1, "string->symbol");
    sym->String = MakeString(s, sl);
    sym->Hash = hsh;

//    AsVector(buckets)->Vector[idx] =
//            MakeHashNode(SymbolHashTable, sym->String, sym, nlst, hsh, "string->symbol");
    ModifyVector(buckets, idx,
            MakeHashNode(SymbolHashTable, sym->String, sym, nlst, hsh, "string->symbol"));

    HashTableAdjust(SymbolHashTable, AsHashTable(SymbolHashTable)->Size + 1);
    return(sym);
}

FObject InternSymbol(FObject sym)
{
    FAssert(SymbolP(sym));
    FAssert(((ulong_t) sym) % OBJECT_ALIGNMENT == 0);
    FAssert(AsObjHdr(sym)->Generation() == OBJHDR_GEN_ETERNAL);
    FAssert(AsObjHdr(sym)->SlotCount() == 1);
    FAssert(AsObjHdr(sym)->ObjectSize() >= sizeof(FSymbol));

    FAssert(CStringP(AsSymbol(sym)->String));
    FAssert(((ulong_t) AsSymbol(sym)->String) % OBJECT_ALIGNMENT == 0);
    FAssert(AsObjHdr(AsSymbol(sym)->String)->Generation() == OBJHDR_GEN_ETERNAL);
    FAssert(AsObjHdr(AsSymbol(sym)->String)->SlotCount() == 0);
    FAssert(AsObjHdr(AsSymbol(sym)->String)->ObjectSize() >= sizeof(FCString));

    FObject obj = HashTableRef(SymbolHashTable, AsSymbol(sym)->String, NoValueObject);
    if (obj == NoValueObject)
    {
        AsSymbol(sym)->Hash = CStringHash(AsCString(AsSymbol(sym)->String)->String);
        HashTableSet(SymbolHashTable, AsSymbol(sym)->String, sym);
        return(sym);
    }

    return(obj);
}

// ---- Hash Nodes ----

Define("%make-hash-node", MakeHashNodePrimitive)(long_t argc, FObject argv[])
{
    FiveArgsCheck("%make-hash-node", argc);
    HashTableArgCheck("%make-hash-node", argv[0]);
    NonNegativeArgCheck("%make-hash-node", argv[4], 0);

    return(MakeHashNode(argv[0], argv[1], argv[2], argv[3], (uint32_t) AsFixnum(argv[4]),
            "%make-hash-node"));
}

Define("%copy-hash-node-list", CopyHashNodeListPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("%copy-hash-node-list", argc);
    HashTableArgCheck("%copy-hash-node-list", argv[0]);
    HashNodeArgCheck("%copy-hash-node-list", argv[1]);
    HashNodeArgCheck("%copy-hash-node-list", argv[2]);

    return(CopyHashNodeList(argv[0], argv[1], argv[2], "%copy-hash-node-list"));
}

Define("%hash-node?", HashNodePPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("%hash-node?", argc);

    return(HashNodeP(argv[0]) ? TrueObject : FalseObject);
}

Define("%hash-node-broken?", HashNodeBrokenPPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("%hash-node-broken?", argc);
    HashNodeArgCheck("%hash-node-broken?", argv[0]);

    return(HashNodeBrokenP(argv[0]) ? TrueObject : FalseObject);
}

Define("%hash-node-key", HashNodeKeyPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("%hash-node-key", argc);
    HashNodeArgCheck("%hash-node-key", argv[0]);

    return(HashNodeKey(argv[0]));
}

Define("%hash-node-value", HashNodeValuePrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("%hash-node-value", argc);
    HashNodeArgCheck("%hash-node-value", argv[0]);

    return(HashNodeValue(argv[0]));
}

Define("%hash-node-value-set!", HashNodeValueSetPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("%hash-node-value-set!", argc);
    HashNodeArgCheck("%hash-node-value-set!", argv[0]);

    HashNodeValueSet(argv[0], argv[1]);
    return(NoValueObject);
}

Define("%hash-node-next", HashNodeNextPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("%hash-node-next", argc);
    HashNodeArgCheck("%hash-node-next", argv[0]);

    return(AsHashNode(argv[0])->Next);
}

Define("%hash-node-next-set!", HashNodeNextSetPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("%hash-node-next-set!", argc);
    HashNodeArgCheck("%hash-node-next-set!", argv[0]);

//    AsHashNode(argv[0])->Next = argv[1];
    Modify(FHashNode, argv[0], Next, argv[1]);
    return(NoValueObject);
}

Define("%hash-node-hash", HashNodeHashPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("%hash-node-hash", argc);
    HashNodeArgCheck("%hash-node-hash", argv[0]);

    return(MakeFixnum(AsHashNode(argv[0])->Hash));
}

// ---- Primitives ----

static FObject Primitives[] =
{
    HashTablePPrimitive,
    EqHashTablePPrimitive,
    MakeEqHashTablePrimitive,
    MakeHashTablePrimitive,
    HashTableBucketsPrimitive,
    HashTableBucketsSetPrimitive,
    HashTableTypeTestPredicatePrimitive,
    HashTableEqualityPredicatePrimitive,
    HashTableHashFunctionPrimitive,
    HashTablePopPrimitive,
    HashTableClearPrimitive,
    HashTableSizePrimitive,
    HashTableAdjustPrimitive,
    HashTableMutablePPrimitive,
    HashTableImmutablePrimitive,
    HashTableExclusivePrimitive,
    HashTableRefPrimitive,
    HashTableSetPrimitive,
    HashTableDeletePrimitive,
    HashTableEmptyCopyPrimitive,
    HashTableCopyPrimitive,
    HashTableToAlistPrimitive,
    HashTableKeysPrimitive,
    HashTableValuesPrimitive,
    HashTableEntriesPrimitive,
    MakeHashNodePrimitive,
    CopyHashNodeListPrimitive,
    HashNodePPrimitive,
    HashNodeBrokenPPrimitive,
    HashNodeKeyPrimitive,
    HashNodeValuePrimitive,
    HashNodeValueSetPrimitive,
    HashNodeNextPrimitive,
    HashNodeNextSetPrimitive,
    HashNodeHashPrimitive
};

void SetupHashTables()
{
    ThreadSafeSymbol = InternSymbol(ThreadSafeSymbol);
    WeakKeysSymbol = InternSymbol(WeakKeysSymbol);
    EphemeralKeysSymbol = InternSymbol(EphemeralKeysSymbol);
    WeakValuesSymbol = InternSymbol(WeakValuesSymbol);
    EphemeralValuesSymbol = InternSymbol(EphemeralValuesSymbol);

    for (ulong_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(Bedrock, BedrockLibrary, Primitives[idx]);
}
