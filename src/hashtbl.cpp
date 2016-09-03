/*

Foment

*/

#include <string.h>
#include "foment.hpp"

static int_t SpecialStringEqualP(FObject str1, FObject str2)
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

    uint_t sdx;
    for (sdx = 0; sdx < StringLength(obj); sdx++)
        if (cs[sdx] == 0 || AsString(obj)->String[sdx] != cs[sdx])
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
typedef int_t (*FEqualityP)(FObject obj1, FObject obj2);

typedef struct
{
    FObject BuiltinType;
    FObject Entries; // VectorP
    FObject TypeTestP; // Comparator.TypeTestP
    FObject EqualityP; // Comparator.EqualityP
    FObject HashFn; // Comparator.HashFn
    FObject Tracker;
    FHashFn UseHashFn;
    FEqualityP UseEqualityP;
    uint_t Size;
    uint_t Immutable;
} FHashTable;

inline void UseHashTableArgCheck(const char * who, FObject obj)
{
    if (HashTableP(obj) == 0 || AsHashTable(obj)->UseHashFn == 0)
        RaiseExceptionC(Assertion, who, "expected a hash table", List(obj));
}

static FObject MakeHashTable(uint_t cap, FObject ttp, FObject eqp, FObject hashfn)
{
    FAssert(cap > 0);
    FAssert(ProcedureP(ttp) || PrimitiveP(ttp));
    FAssert(ProcedureP(eqp) || PrimitiveP(eqp));
    FAssert(ProcedureP(hashfn) || PrimitiveP(hashfn));

    FHashTable * htbl = (FHashTable *) MakeBuiltin(HashTableType, sizeof(FHashTable), 6,
            "make-hash-table");
    htbl->Entries = MakeVector(cap, 0, NoValueObject);
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
    htbl->Size = 0;
    htbl->Immutable = 0;

    return(htbl);
}

// ---- Hash Nodes ----

#define AsHashNode(obj) ((FHashNode *) (obj))
#define HashNodeP(obj) (IndirectTag(obj) == HashNodeTag)

typedef struct
{
    FObject Key;
    FObject Value;
    FObject Next;
    uint32_t Hash;
    uint32_t Deleted;
} FHashNode;

static FObject MakeHashNode(FObject key, FObject val, FObject next, uint32_t hsh, const char * who)
{
    FHashNode * node = (FHashNode *) MakeObject(HashNodeTag, sizeof(FHashNode), 3, who);
    node->Key = key;
    node->Value = val;
    node->Next = next;
    node->Hash = hsh;
    node->Deleted = 0;

    return(node);
}

// ----------------

static void
ResizeHashTable(FObject htbl)
{

    
}

// ---- Eq Hash Tables ----

Define("any?", AnyPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("any?", argc);

    return(TrueObject);
}

FObject MakeEqHashTable(uint_t cap)
{
    return(MakeHashTable(cap, AnyPPrimitive, EqPPrimitive, EqHashPrimitive));
}

static void RehashEqHashTable(FObject htbl)
{
    FAssert(HashTableP(htbl));
    FAssert(VectorP(AsHashTable(htbl)->Entries));

    FObject entries = AsHashTable(htbl)->Entries;
    FObject tconc = AsHashTable(htbl)->Tracker;
    while (TConcEmptyP(tconc) == 0)
    {
        FObject node = TConcRemove(tconc);

        FAssert(HashNodeP(node));

        if (AsHashNode(node)->Deleted == 0)
        {
            FAssert(AsHashNode(node)->Hash != EqHash(AsHashNode(node)->Key));

            uint32_t nhsh = EqHash(AsHashNode(node)->Key);
            uint32_t ndx = nhsh % VectorLength(entries);
            uint32_t odx = AsHashNode(node)->Hash % VectorLength(entries);

            if (odx != ndx)
            {
                if (AsVector(entries)->Vector[odx] == node)
                {
//                    AsVector(entries)->Vector[odx] = AsHashNode(node)->Next;
                    ModifyVector(entries, odx, AsHashNode(node)->Next);
                }
                else
                {
                    FObject prev = AsVector(entries)->Vector[odx];
                    while (AsHashNode(prev)->Next != node)
                    {
                        prev = AsHashNode(prev)->Next;

                        FAssert(HashNodeP(prev));
                    }

                    AsHashNode(prev)->Next = AsHashNode(node)->Next;
                }

//                AsHashNode(node)->Next = AsVector(entries)->Vector[ndx];
                Modify(FHashNode, node, Next, AsVector(entries)->Vector[ndx]);
//                AsVector(entries)->Vector[ndx] = node;
                ModifyVector(entries, ndx, node);
            }

            AsHashNode(node)->Hash = nhsh;
        }
    }
}

// ---- Hash Tables ----

FObject MakeStringHashTable(uint_t cap)
{
    return(MakeHashTable(cap, StringPPrimitive, StringEqualPPrimitive, StringHashPrimitive));
}

FObject MakeSymbolHashTable(uint_t cap)
{
    return(MakeHashTable(cap, SymbolPPrimitive, EqPPrimitive, SymbolHashPrimitive));
}

FObject HashTableRef(FObject htbl, FObject key, FObject def)
{
    FAssert(HashTableP(htbl));
    FAssert(VectorP(AsHashTable(htbl)->Entries));
    FAssert(AsHashTable(htbl)->UseEqualityP != 0);
    FAssert(AsHashTable(htbl)->UseHashFn != 0);

    FHashFn UseHashFn = AsHashTable(htbl)->UseHashFn;
    FEqualityP UseEqualityP = AsHashTable(htbl)->UseEqualityP;

    if (UseHashFn == EqHash)
        RehashEqHashTable(htbl);

    FObject entries = AsHashTable(htbl)->Entries;
    uint32_t hsh = UseHashFn(key);
    uint32_t idx = hsh % VectorLength(entries);
    FObject node = AsVector(entries)->Vector[idx];

    while (HashNodeP(node))
    {
        if (UseEqualityP(AsHashNode(node)->Key, key))
        {
            FAssert(AsHashNode(node)->Hash == hsh);

            return(AsHashNode(node)->Value);
        }

        node = AsHashNode(node)->Next;
    }

    return(def);
}

void HashTableSet(FObject htbl, FObject key, FObject val)
{
    FAssert(HashTableP(htbl));
    FAssert(VectorP(AsHashTable(htbl)->Entries));
    FAssert(AsHashTable(htbl)->UseEqualityP != 0);
    FAssert(AsHashTable(htbl)->UseHashFn != 0);
    FAssert(AsHashTable(htbl)->Immutable == 0);

    FHashFn UseHashFn = AsHashTable(htbl)->UseHashFn;
    FEqualityP UseEqualityP = AsHashTable(htbl)->UseEqualityP;

    if (UseHashFn == EqHash)
        RehashEqHashTable(htbl);

    FObject entries = AsHashTable(htbl)->Entries;
    uint32_t hsh = UseHashFn(key);
    uint32_t idx = hsh % VectorLength(entries);
    FObject nlst = AsVector(entries)->Vector[idx];
    FObject node = nlst;

    while (HashNodeP(node))
    {
        if (UseEqualityP(AsHashNode(node)->Key, key))
        {
            FAssert(AsHashNode(node)->Hash == hsh);

//            AsHashNode(node)->Value = val;
            Modify(FHashNode, node, Value, val);
            return;
        }

        node = AsHashNode(node)->Next;
    }

    node = MakeHashNode(key, val, nlst, hsh, "hash-table-set!");
//    AsVector(entries)->Vector[idx] = node;
    ModifyVector(entries, idx, node);
    if (UseHashFn == EqHash && ObjectP(key))
        InstallTracker(key, node, AsHashTable(htbl)->Tracker);

    AsHashTable(htbl)->Size += 1;
    ResizeHashTable(htbl);
}

void HashTableDelete(FObject htbl, FObject key)
{
    FAssert(HashTableP(htbl));
    FAssert(VectorP(AsHashTable(htbl)->Entries));
    FAssert(AsHashTable(htbl)->UseEqualityP != 0);
    FAssert(AsHashTable(htbl)->UseHashFn != 0);
    FAssert(AsHashTable(htbl)->Immutable == 0);

    FHashFn UseHashFn = AsHashTable(htbl)->UseHashFn;
    FEqualityP UseEqualityP = AsHashTable(htbl)->UseEqualityP;

    if (UseHashFn == EqHash)
        RehashEqHashTable(htbl);

    FObject entries = AsHashTable(htbl)->Entries;
    uint32_t hsh = UseHashFn(key);
    uint32_t idx = hsh % VectorLength(entries);
    FObject nlst = AsVector(entries)->Vector[idx];
    FObject node = nlst;

    while (HashNodeP(node))
    {
        FObject prev = NoValueObject;

        if (UseEqualityP(AsHashNode(node)->Key, key))
        {
            FAssert(AsHashNode(node)->Hash == hsh);
            FAssert(AsHashNode(node)->Deleted == 0);

            AsHashNode(node)->Deleted = 1;

            if (node == nlst)
            {
//                AsVector(entries)->Vector[idx] = AsHashNode(node)->Next;
                ModifyVector(entries, idx, AsHashNode(node)->Next);
            }
            else
            {
                FAssert(HashNodeP(prev));
                FAssert(AsHashNode(prev)->Next == node);

//                AsHashNode(prev)->Next = AsHashNode(node)->Next;
                Modify(FHashNode, prev, Next, AsHashNode(node)->Next);
            }

            AsHashTable(htbl)->Size -= 1;
            ResizeHashTable(htbl);
            return;
        }

        prev = node;
        node = AsHashNode(node)->Next;
    }

    ResizeHashTable(htbl);
}

void HashTableVisit(FObject htbl, FVisitFn vfn, FObject ctx)
{
    FAssert(HashTableP(htbl));
    FAssert(VectorP(AsHashTable(htbl)->Entries));
    FAssert(AsHashTable(htbl)->UseEqualityP != 0);
    FAssert(AsHashTable(htbl)->UseHashFn != 0);

    if (AsHashTable(htbl)->UseHashFn == EqHash)
        RehashEqHashTable(htbl);

    FObject entries = AsHashTable(htbl)->Entries;

    for (uint_t idx = 0; idx < VectorLength(entries); idx++)
    {
        FObject nlst = AsVector(entries)->Vector[idx];

        while (HashNodeP(nlst))
        {
            vfn(AsHashNode(nlst)->Key, AsHashNode(nlst)->Value, ctx);
            nlst = AsHashNode(nlst)->Next;
        }
    }
}

Define("make-eq-hash-table", MakeEqHashTablePrimitive)(int_t argc, FObject argv[])
{
    ZeroArgsCheck("make-eq-hash-table", argc);

    return(MakeEqHashTable(128));
}

Define("%hash-table-ref", HashTableRefPrimitive)(int_t argc, FObject argv[])
{
    ThreeArgsCheck("%hash-table-ref", argc);
    UseHashTableArgCheck("%hash-table-ref", argv[0]);

    return(HashTableRef(argv[0], argv[1], argv[2]));
}

Define("%hash-table-set!", HashTableSetPrimitive)(int_t argc, FObject argv[])
{
    ThreeArgsCheck("%hash-table-set!", argc);
    UseHashTableArgCheck("%hash-table-set!", argv[0]);

    HashTableSet(argv[0], argv[1], argv[2]);
    return(NoValueObject);
}

Define("%hash-table-delete!", HashTableDeletePrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("%hash-table-delete", argc);
    UseHashTableArgCheck("%hash-table-delete", argv[0]);

    HashTableDelete(argv[0], argv[1]);
    return(NoValueObject);
}

// ---- Symbols ----

static int_t SpecialStringLengthEqualP(FCh * s, int_t sl, FObject str)
{
    if (StringP(str))
        return(StringLengthEqualP(s, sl, str));

    FAssert(CStringP(str));

    const char * cs = AsCString(str)->String;
    uint_t sdx;
    for (sdx = 0; sdx < sl; sdx++)
        if (cs[sdx] == 0 || s[sdx] != cs[sdx])
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

FObject StringLengthToSymbol(FCh * s, int_t sl)
{
    FObject entries = AsHashTable(SymbolHashTable)->Entries;
    uint32_t hsh = StringLengthHash(s, sl);
    uint32_t idx = hsh % VectorLength(entries);
    FObject nlst = AsVector(entries)->Vector[idx];
    FObject node = nlst;

    while (HashNodeP(node))
    {
        FAssert(StringP(AsHashNode(node)->Key) || CStringP(AsHashNode(node)->Key));

        if (SpecialStringLengthEqualP(s, sl, AsHashNode(node)->Key))
        {
            FAssert(AsHashNode(node)->Hash == hsh);
            FAssert(SymbolP(AsHashNode(node)->Value));

            return(AsHashNode(node)->Value);
        }

        node = AsHashNode(node)->Next;
    }

    FSymbol * sym = (FSymbol *) MakeObject(SymbolTag, sizeof(FSymbol), 1, "string->symbol");
    sym->String = MakeString(s, sl);
    sym->Hash = hsh;

//    AsVector(entries)->Vector[idx] = MakeHashNode(key, val, nlst, hsh, "string->symbol");
    ModifyVector(entries, idx, MakeHashNode(sym->String, sym, nlst, hsh, "string->symbol"));

    AsHashTable(SymbolHashTable)->Size += 1;
    ResizeHashTable(SymbolHashTable);
    return(sym);
}

FObject InternSymbol(FObject sym)
{
    FAssert(SymbolP(sym));
    FAssert(AsObjHdr(sym)->Generation() == OBJHDR_GEN_ETERNAL);
    FAssert(CStringP(AsSymbol(sym)->String));
    FAssert(AsObjHdr(AsSymbol(sym)->String)->Generation() == OBJHDR_GEN_ETERNAL);

    FObject obj = HashTableRef(SymbolHashTable, AsSymbol(sym)->String, NoValueObject);
    if (obj == NoValueObject)
    {
        AsSymbol(sym)->Hash = CStringHash(AsCString(AsSymbol(sym)->String)->String);
        HashTableSet(SymbolHashTable, AsSymbol(sym)->String, sym);
        return(sym);
    }

    return(obj);
}

// ---- Primitives ----

static FObject Primitives[] =
{
    MakeEqHashTablePrimitive,
    HashTableRefPrimitive,
    HashTableSetPrimitive,
    HashTableDeletePrimitive
};

void SetupHashTables()
{
    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(Bedrock, BedrockLibrary, Primitives[idx]);
}
