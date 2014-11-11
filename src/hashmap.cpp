/*

Foment

*/

#include "foment.hpp"

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
        if (PairP(AsHashtable(ht)->Tracker))
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

uint_t HashtableSize(FObject ht)
{
    FAssert(HashtableP(ht));
    FAssert(FixnumP(AsHashtable(ht)->Size));

    if (TConcEmptyP(AsHashtable(ht)->Tracker) == 0)
        EqHashtableRehash(ht, AsHashtable(ht)->Tracker);

    return(AsFixnum(AsHashtable(ht)->Size));
}

void HashtableWalkUpdate(FObject ht, FWalkUpdateFn wfn, FObject ctx)
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
    int_t len = VectorLength(bkts);

    for (int_t idx = 0; idx < len; idx++)
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
    int_t len = VectorLength(bkts);

    for (int_t idx = 0; idx < len; idx++)
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
