/*

Foment

Garbage Collection:
-- C code does not have to worry about it if it doesn't want to, other than Modify
-- StartAllowGC() and StopAllowGC() to denote a block of code where GC can occur; typically
    around a potentially lengthy IO operation
-- Root(obj) used to tell GC about a temporary root object; exceptions and return need to
    stop rooting objects; rooting extra objects is ok, if it simplifies the C code
-- each thread has an allocation block for the 1st generation: no syncronization necessary to
    allocate an object
-- GC does not occur during an allocation, only at AllowGC points for all threads

-- IO and GC
-- ExecuteState and stacks should be segments: the two stacks should grow towards each other
*/

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "foment.hpp"
#include "execute.hpp"
#include "io.hpp"

typedef void * FRaw;
#define AsRaw(obj) ((FRaw) (((unsigned int) (obj)) & ~0x3))

typedef enum
{
    HoleSectionTag,
    FreeSectionTag,
    TableSectionTag,
    ZeroSectionTag, // Generation Zero
    OneSectionTag, // Generation One
    MatureSectionTag,
    PairSectionTag,
    BackRefSectionTag,
    ScanSectionTag
} FSectionTag;

static unsigned char * SectionTable;
static unsigned int UsedSections;

#define SECTION_SIZE (1024 * 16)
#define SectionIndex(ptr) ((((unsigned int) (ptr)) - ((unsigned int) SectionTable)) >> 14)
#define SectionPointer(sdx) ((unsigned char *) (((sdx) << 14) + ((unsigned int) SectionTable)))
#define SectionOffset(ptr) (((unsigned int) (ptr)) & 0x3FFF)
#define SectionBase(ptr) ((unsigned char *) (((unsigned int) (ptr)) & ~0x3FFF))

#define MatureP(obj) (SectionTable[SectionIndex(obj)] == MatureSectionTag)
#define MaturePairP(obj) (SectionTable[SectionIndex(obj)] == PairSectionTag)

#define MAXIMUM_YOUNG_LENGTH (1024 * 4)

typedef struct _FYoungSection
{
    struct _FYoungSection * Next;
    unsigned int Used;
    unsigned int Scan;
} FYoungSection;

static FYoungSection * ActiveZero;
static FYoungSection * GenerationZero;
static FYoungSection * GenerationOne;

typedef struct _FFreeObject
{
    unsigned int Length;
    struct _FFreeObject * Next;
} FFreeObject;

static FFreeObject * FreeMature = 0;
static FPair * FreePairs = 0;

#define PAIR_MB_SIZE 256
#define PAIR_MB_OFFSET (SECTION_SIZE - PAIR_MB_SIZE)

static inline unsigned int PairMarkP(FRaw raw)
{
    unsigned int idx = SectionOffset(raw) / sizeof(FPair);

    return((SectionBase(raw) + PAIR_MB_OFFSET)[idx / 8] & (1 << (idx % 8)));
}

static inline void SetPairMark(FRaw raw)
{
    unsigned int idx = SectionOffset(raw) / sizeof(FPair);

    (SectionBase(raw) + PAIR_MB_OFFSET)[idx / 8] |= (1 << (idx % 8));
}

unsigned int BytesAllocated = 0;
unsigned int BytesSinceLast = 0;
unsigned int ObjectsSinceLast = 0;
unsigned int CollectionCount = 0;
unsigned int PartialPerFull = 4;
unsigned int TriggerBytes = SECTION_SIZE * 4;
unsigned int TriggerObjects = TriggerBytes / (sizeof(FPair) * 2);
unsigned int MaximumBackRefFraction = 128;

static int UsedRoots = 0;
static FObject * Roots[128];

static FExecuteState * ExecuteState = 0;
int GCRequired = 1;
int FullGCRequired = 0;

#define GCTagP(obj) ImmediateP(obj, GCTagTag)

#define Forward(obj) (*(((FObject *) (obj)) - 1))

#define MarkP(obj) (*((unsigned int *) (obj)) & RESERVED_MARK_BIT)
#define SetMark(obj) *((unsigned int *) (obj)) |= RESERVED_MARK_BIT
#define ClearMark(obj) *((unsigned int *) (obj)) &= ~RESERVED_MARK_BIT

static inline int AliveP(FObject obj)
{
    obj = AsRaw(obj);

    if (MatureP(obj))
        return(MarkP(obj));
    else if (MaturePairP(obj))
        return(PairMarkP(obj));
    return(GCTagP(Forward(obj)) == 0);
}

typedef struct
{
    FObject * Ref;
    FObject Value;
} FBackRef;

typedef struct _BackRefSection
{
    struct _BackRefSection * Next;
    int Used;
    FBackRef BackRef[1];
} FBackRefSection;

static FBackRefSection * BackRefSections;
static int BackRefSectionCount;

typedef struct _ScanSection
{
    struct _ScanSection * Next;
    int Used;
    FObject Scan[1];
} FScanSection;

static FScanSection * ScanSections;

static FObject YoungGuardians;
static FObject MatureGuardians;
static FObject YoungTrackers;
static FObject MatureTrackers;

static unsigned int Sizes[1024 * 8];

static void * AllocateSection(unsigned int cnt, FSectionTag tag)
{
    unsigned int sdx;
    unsigned int fcnt = 0;

    FAssert(cnt > 0);

    for (sdx = 0; sdx < UsedSections; sdx++)
    {
        if (SectionTable[sdx] == FreeSectionTag)
            fcnt += 1;
        else
            fcnt = 0;

        if (fcnt == cnt)
        {
            while (fcnt > 0)
            {
                fcnt -= 1;
                SectionTable[sdx - fcnt] = tag;
            }

            return(SectionPointer(sdx - cnt + 1));
        }
    }

    if (UsedSections + cnt > SECTION_SIZE)
        return(0);

    sdx = UsedSections;
    UsedSections += cnt;

    void * sec = SectionPointer(sdx);
    VirtualAlloc(sec, SECTION_SIZE * cnt, MEM_COMMIT, PAGE_READWRITE);

    while (cnt > 0)
    {
        cnt -= 1;
        SectionTable[sdx + cnt] = tag;
    }

    return(sec);
}

static void FreeSection(void * sec)
{
    unsigned int sdx = SectionIndex(sec);

    FAssert(sdx < UsedSections);

    SectionTable[sdx] = FreeSectionTag;
}

static FYoungSection * AllocateYoung(FYoungSection * nxt, FSectionTag tag)
{
    FYoungSection * ns = (FYoungSection *) AllocateSection(1, tag);
    ns->Next = nxt;
    ns->Used = sizeof(FYoungSection);
    ns->Scan = sizeof(FYoungSection);
    return(ns);
}

const static unsigned int Align4[4] = {0, 3, 2, 1};

static unsigned int ObjectSize(FObject obj, unsigned int tag)
{
    switch (tag)
    {
    case PairTag:
        return(sizeof(FPair));

    case FlonumTag:
        return(sizeof(FFlonum));

    case BoxTag:
        FAssert(BoxP(obj));

        return(sizeof(FBox));

    case StringTag:
    {
        FAssert(StringP(obj));

        int len = sizeof(FString) + sizeof(FCh) * StringLength(obj);
        len += Align4[len % 4];
        return(len);
    }

    case VectorTag:
        FAssert(VectorP(obj));
        FAssert(VectorLength(obj) >= 0);

        return(sizeof(FVector) + sizeof(FObject) * (VectorLength(obj) - 1));

    case BytevectorTag:
    {
        FAssert(BytevectorP(obj));

        int len = sizeof(FBytevector) + sizeof(FByte) * (BytevectorLength(obj) - 1);
        len += Align4[len % 4];
        return(len);
    }

    case PortTag:
        FAssert(PortP(obj));

        return(sizeof(FPort));

    case ProcedureTag:
        FAssert(ProcedureP(obj));

        return(sizeof(FProcedure));

    case SymbolTag:
        FAssert(SymbolP(obj));

        return(sizeof(FSymbol));

    case RecordTypeTag:
        FAssert(RecordTypeP(obj));

        return(sizeof(FRecordType) + sizeof(FObject) * (RecordTypeNumFields(obj) - 1));

    case RecordTag:
        FAssert(GenericRecordP(obj));

        return(sizeof(FGenericRecord) + sizeof(FObject) * (RecordNumFields(obj) - 1));

    case PrimitiveTag:
        FAssert(PrimitiveP(obj));

        return(sizeof(FPrimitive));

    case GCFreeTag:
        return(ByteLength(obj));

    default:
        FAssert(0);
    }

    return(0);
}

// Allocate a new object in GenerationZero.
FObject MakeObject(unsigned int sz, unsigned int tag)
{
    unsigned int len = sz;
    len += Align4[len % 4];

    FAssert(len >= sz);
    FAssert(len % 4 == 0);
    FAssert(len >= sizeof(FObject));

    if (len < sizeof(Sizes) / sizeof(unsigned int))
        Sizes[len] += 1;

    if (len > MAXIMUM_YOUNG_LENGTH)
        return(0);

    BytesAllocated += len;
    BytesSinceLast += len;
    ObjectsSinceLast += 1;
    if (BytesSinceLast > TriggerBytes)
        GCRequired = 1;

    if (ObjectsSinceLast > TriggerObjects)
        GCRequired = 1;

    if (ActiveZero->Used + len + sizeof(FObject) > SECTION_SIZE)
    {
        FAssert(ActiveZero->Next == 0);

        ActiveZero->Next = GenerationZero;
        GenerationZero = ActiveZero;

        ActiveZero = AllocateYoung(0, ZeroSectionTag);
    }

    FObject * pobj = (FObject *) (((char *) ActiveZero) + ActiveZero->Used);
    ActiveZero->Used += len + sizeof(FObject);
    FObject obj = (FObject) (pobj + 1);

    Forward(obj) = MakeImmediate(tag, GCTagTag);

    FAssert(AsValue(*pobj) == tag);
    FAssert(GCTagP(*pobj));
    FAssert(SectionTable[SectionIndex(obj)] == ZeroSectionTag);

    return(obj);
}

// Copy an object from GenerationZero to GenerationOne.
static FObject CopyObject(unsigned int len, unsigned int tag)
{
    FAssert(len % 4 == 0);
    FAssert(len >= sizeof(FObject));
    FAssert(len <= MAXIMUM_OBJECT_LENGTH);

    if (GenerationOne->Used + len + sizeof(FObject) > SECTION_SIZE)
        GenerationOne = AllocateYoung(GenerationOne, OneSectionTag);

    FObject * pobj = (FObject *) (((char *) GenerationOne) + GenerationOne->Used);
    GenerationOne->Used += len + sizeof(FObject);
    FObject obj = (FObject) (pobj + 1);

    Forward(obj) = MakeImmediate(tag, GCTagTag);

    FAssert(AsValue(*pobj) == tag);
    FAssert(GCTagP(*pobj));
    FAssert(SectionTable[SectionIndex(obj)] == OneSectionTag);

    return(obj);
}

static FObject MakeMature(unsigned int len)
{
    FAssert(len % 4 == 0);
    FAssert(len <= MAXIMUM_OBJECT_LENGTH);

    FFreeObject ** pfo = &FreeMature;
    FFreeObject * fo = FreeMature;

    while (fo != 0)
    {
        FAssert(IndirectTag(fo) == GCFreeTag);

        if (ByteLength(fo) == len)
        {
            *pfo = fo->Next;
            return(fo);
        }
        else if (ByteLength(fo) >= len + sizeof(FFreeObject))
        {
            fo->Length = MakeLength(ByteLength(fo) - len, GCFreeTag);
            return(((char *) fo) + ByteLength(fo));
        }

        fo = fo->Next;
    }

    unsigned int cnt = 4;
    if (len > SECTION_SIZE * cnt / 2)
    {
        cnt = len / SECTION_SIZE;
        if (len % SECTION_SIZE != 0)
            cnt += 1;
        cnt += 1;

        FAssert(cnt >= 4);
    }

    fo = (FFreeObject *) AllocateSection(cnt, MatureSectionTag);
    if (fo == 0)
        return(0);

    fo->Next = 0;
    fo->Length = MakeLength(SECTION_SIZE * cnt - len, GCFreeTag);
    FreeMature = fo;
    return(((char *) FreeMature) + ByteLength(fo));
}

FObject MakeMatureObject(unsigned int len, char * who)
{
    if (len > MAXIMUM_OBJECT_LENGTH)
        RaiseExceptionC(R.Restriction, who, "length greater than maximum object length",
                EmptyListObject);

    FObject obj = MakeMature(len);
    if (obj == 0)
        RaiseExceptionC(R.Assertion, who, "out of memory", EmptyListObject);

    BytesAllocated += len;
    return(obj);
}

static FObject MakeMaturePair()
{
    if (FreePairs == 0)
    {
        FPair * pr = (FPair *) AllocateSection(1, PairSectionTag);
        for (unsigned int idx = 0; idx < PAIR_MB_OFFSET / sizeof(FPair); idx++)
        {
            pr->First = FreePairs;
            FreePairs = pr;
            pr += 1;
        }
    }

    FObject obj = (FObject) FreePairs;
    FreePairs = (FPair *) FreePairs->First;

    return(obj);
}

void PushRoot(FObject * rt)
{
    UsedRoots += 1;

    FAssert(UsedRoots < sizeof(Roots) / sizeof(FObject *));

    Roots[UsedRoots - 1] = rt;
}

void PopRoot()
{
    FAssert(UsedRoots > 0);

    UsedRoots -= 1;
}

void ClearRoots()
{
    UsedRoots = 0;
}

void EnterExecute(FExecuteState * es)
{
    FAssert(ExecuteState == 0);

    ExecuteState = es;
}

void LeaveExecute(FExecuteState * es)
{
    FAssert(ExecuteState == es);

    ExecuteState = 0;
}

static FBackRefSection * AllocateBackRefSection(FBackRefSection * nxt)
{
    FBackRefSection * brs = (FBackRefSection *) AllocateSection(1, BackRefSectionTag);
    brs->Next = nxt;
    brs->Used = 0;

    BackRefSectionCount += 1;

    return(brs);
}

static void FreeBackRefSections()
{
    FBackRefSection * brs = BackRefSections->Next;
    BackRefSections->Next = 0;
    BackRefSections->Used = 0;
    BackRefSectionCount = 1;

    while (brs != 0)
    {
        FBackRefSection * tbrs = brs;
        brs = brs->Next;
        FreeSection(tbrs);
    }
}

static FScanSection * AllocateScanSection(FScanSection * nxt)
{
    FScanSection * ss = (FScanSection *) AllocateSection(1, ScanSectionTag);
    ss->Next = nxt;
    ss->Used = 0;

    return(ss);
}

static void RecordBackRef(FObject * ref, FObject val)
{
    FAssert(*ref == val);
    FAssert(ObjectP(val));

    if (BackRefSections->Used == ((SECTION_SIZE - sizeof(FBackRefSection)) / sizeof(FBackRef)) + 1)
    {
        if (BackRefSectionCount * MaximumBackRefFraction > UsedSections)
        {
            FullGCRequired = 1;
            FreeBackRefSections();
        }
        else
            BackRefSections = AllocateBackRefSection(BackRefSections);
    }

    if (FullGCRequired == 0)
    {
        BackRefSections->BackRef[BackRefSections->Used].Ref = ref;
        BackRefSections->BackRef[BackRefSections->Used].Value = val;
        BackRefSections->Used += 1;
    }
}

void ModifyVector(FObject obj, unsigned int idx, FObject val)
{
    FAssert(VectorP(obj));
    FAssert(idx < VectorLength(obj));

    AsVector(obj)->Vector[idx] = val;

    if (MatureP(obj) && ObjectP(val) && MatureP(val) == 0 && MaturePairP(val) == 0)
        RecordBackRef(AsVector(obj)->Vector + idx, val);
}

void ModifyObject(FObject obj, int off, FObject val)
{
    FAssert(IndirectP(obj));
    FAssert(off % sizeof(FObject) == 0);

    ((FObject *) obj)[off / sizeof(FObject)] = val;

    if (MatureP(obj) && ObjectP(val) && MatureP(val) == 0 && MaturePairP(val) == 0)
        RecordBackRef(((FObject *) obj) + (off / sizeof(FObject)), val);

}

void SetFirst(FObject obj, FObject val)
{
    FAssert(PairP(obj));

    AsPair(obj)->First = val;

    if (MaturePairP(obj) && ObjectP(val) && MatureP(val) == 0 && MaturePairP(val) == 0)
        RecordBackRef(&(AsPair(obj)->First), val);
}

void SetRest(FObject obj, FObject val)
{
    FAssert(PairP(obj));

    AsPair(obj)->Rest = val;

    if (MaturePairP(obj) && ObjectP(val) && MatureP(val) == 0 && MaturePairP(val) == 0)
        RecordBackRef(&(AsPair(obj)->Rest), val);
}

static void AddToScan(FObject obj)
{
    if (ScanSections->Used == ((SECTION_SIZE - sizeof(FScanSection)) / sizeof(FObject)) + 1)
        ScanSections = AllocateScanSection(ScanSections);

    ScanSections->Scan[ScanSections->Used] = obj;
    ScanSections->Used += 1;
}

static void ScanObject(FObject * pobj, int fcf, int mf)
{
    FAssert(ObjectP(*pobj));

    FObject raw = AsRaw(*pobj);
    unsigned int sdx = SectionIndex(raw);

    FAssert(sdx < UsedSections);

    if (SectionTable[sdx] == MatureSectionTag)
    {
        if (fcf && MarkP(raw) == 0)
        {
            SetMark(raw);
            AddToScan(*pobj);
        }
    }
    else if (SectionTable[sdx] == PairSectionTag)
    {
        if (fcf && PairMarkP(raw) == 0)
        {
            SetPairMark(raw);
            AddToScan(*pobj);
        }
    }
    else if (SectionTable[sdx] == ZeroSectionTag)
    {
        if (GCTagP(Forward(raw)))
        {
            unsigned int tag = AsValue(Forward(raw));
            unsigned int len = ObjectSize(raw, tag);
            FObject nobj = CopyObject(len, tag);
            memcpy(nobj, raw, len);

            if (tag == PairTag)
                nobj = PairObject(nobj);
            else if (tag == FlonumTag)
                nobj = FlonumObject(nobj);

            Forward(raw) = nobj;
            *pobj = nobj;

            if (mf)
                RecordBackRef(pobj, nobj);
        }
        else
        {
            *pobj = Forward(raw);

            if (mf)
                RecordBackRef(pobj, Forward(raw));
        }
    }
    else // if (SectionTable[sdx] == OneSectionTag)
    {
        FAssert(SectionTable[sdx] == OneSectionTag);

        if (GCTagP(Forward(raw)))
        {
            unsigned int tag = AsValue(Forward(raw));
            unsigned int len = ObjectSize(raw, tag);

            FObject nobj;
            if (tag == PairTag)
            {
                nobj = MakeMaturePair();
                SetPairMark(nobj);
            }
            else
                nobj = MakeMature(len);

            memcpy(nobj, raw, len);

            if (tag == PairTag)
                nobj = PairObject(nobj);
//            else if (tag == FlonumTag)
//                nobj = FlonumObject(nobj);
            else
                SetMark(nobj);

            AddToScan(nobj);

            Forward(raw) = nobj;
            *pobj = nobj;
        }
        else
            *pobj = Forward(raw);
    }
}

static void CleanScan(int fcf);
static void ScanChildren(FRaw raw, unsigned int tag, int fcf)
{
    int mf = MatureP(raw) || MaturePairP(raw);

    switch (tag)
    {
    case PairTag:
    {
        FPair * pr = (FPair *) raw;

        if (ObjectP(pr->First))
            ScanObject(&pr->First, fcf, mf);
        if (ObjectP(pr->Rest))
            ScanObject(&pr->Rest, fcf, mf);
        break;
    }

    case BoxTag:
        if (ObjectP(AsBox(raw)->Value))
            ScanObject(&(AsBox(raw)->Value), fcf, mf);
        break;

    case StringTag:
        break;

    case VectorTag:
        for (unsigned int vdx = 0; vdx < VectorLength(raw); vdx++)
        {
            if (ObjectP(AsVector(raw)->Vector[vdx]))
                ScanObject(AsVector(raw)->Vector + vdx, fcf, mf);
            if (ScanSections->Next != 0 && ScanSections->Used
                    > ((SECTION_SIZE - sizeof(FScanSection)) / sizeof(FObject)) / 2)
                CleanScan(fcf);
        }
        break;

    case BytevectorTag:
        break;

    case PortTag:
        if (ObjectP(AsPort(raw)->Name))
            ScanObject(&(AsPort(raw)->Name), fcf, mf);
        if (ObjectP(AsPort(raw)->Object))
            ScanObject(&(AsPort(raw)->Object), fcf, mf);
        break;

    case ProcedureTag:
        if (ObjectP(AsProcedure(raw)->Name))
            ScanObject(&(AsProcedure(raw)->Name), fcf, mf);
        if (ObjectP(AsProcedure(raw)->Code))
            ScanObject(&(AsProcedure(raw)->Code), fcf, mf);
        if (ObjectP(AsProcedure(raw)->RestArg))
            ScanObject(&(AsProcedure(raw)->RestArg), fcf, mf);
        break;

    case SymbolTag:
        if (ObjectP(AsSymbol(raw)->String))
            ScanObject(&(AsSymbol(raw)->String), fcf, mf);
        break;

    case RecordTypeTag:
        for (unsigned int fdx = 0; fdx < RecordTypeNumFields(raw); fdx++)
            if (ObjectP(AsRecordType(raw)->Fields[fdx]))
                ScanObject(AsRecordType(raw)->Fields + fdx, fcf, mf);
        break;

    case RecordTag:
        for (unsigned int fdx = 0; fdx < RecordNumFields(raw); fdx++)
            if (ObjectP(AsGenericRecord(raw)->Fields[fdx]))
                ScanObject(AsGenericRecord(raw)->Fields + fdx, fcf, mf);
        break;

    case PrimitiveTag:
        break;

    default:
        FAssert(0);
    }
}

static void CleanScan(int fcf)
{
    for (;;)
    {

        for (;;)
        {
            if (ScanSections->Used == 0)
            {
                if (ScanSections->Next == 0)
                    break;

                FScanSection * ss = ScanSections;
                ScanSections = ScanSections->Next;
                FreeSection(ss);
                FAssert(ScanSections->Used > 0);
            }

            ScanSections->Used -= 1;
            FObject obj = ScanSections->Scan[ScanSections->Used];

            FAssert(ObjectP(obj));

            if (PairP(obj))
                ScanChildren(AsRaw(obj), PairTag, fcf);
            else if (FlonumP(obj))
                ScanChildren(AsRaw(obj), FlonumTag, fcf);
            else
            {
                FAssert(IndirectP(obj));

                ScanChildren(obj, IndirectTag(obj), fcf);
            }
        }

        FYoungSection * ys = GenerationOne;
        while (ys != 0 && ys->Scan < ys->Used)
        {
            while (ys->Scan < ys->Used)
            {
                FObject *pobj = (FObject *) (((char *) ys) + ys->Scan);

                FAssert(SectionTable[SectionIndex(pobj)] == OneSectionTag);

                unsigned int tag = AsValue(*pobj);
                FObject obj = (FObject) (pobj + 1);

                ScanChildren(obj, tag, fcf);
                ys->Scan += ObjectSize(obj, tag) + sizeof(FObject);
            }

            ys = ys->Next;
        }
        if (GenerationOne->Scan == GenerationOne->Used && ScanSections->Used == 0)
            break;
    }
}

static void CollectGuardians(int fcf)
{
    FObject mbhold = EmptyListObject;
    FObject mbfinal = EmptyListObject;

    while (YoungGuardians != EmptyListObject)
    {
        FAssert(PairP(YoungGuardians));
        FAssert(PairP(First(YoungGuardians)));

        FObject ot = First(YoungGuardians);
        if (AliveP(First(ot)))
            mbhold = MakePair(ot, mbhold);
        else
            mbfinal = MakePair(ot, mbfinal);

        YoungGuardians = Rest(YoungGuardians);
    }

    if (fcf)
    {
        while (MatureGuardians != EmptyListObject)
        {
            FAssert(PairP(MatureGuardians));
            FAssert(PairP(First(MatureGuardians)));

            FObject ot = First(MatureGuardians);
            if (AliveP(First(ot)))
                mbhold = MakePair(ot, mbhold);
            else
                mbfinal = MakePair(ot, mbfinal);

            MatureGuardians = Rest(MatureGuardians);
        }
    }

    for (;;)
    {
        FObject flst = EmptyListObject;

        FObject lst = mbfinal;
        while (lst != EmptyListObject)
        {
            FAssert(PairP(lst));

            if (PairP(First(lst)))
            {
                FObject ot = First(lst);
                if (AliveP(Rest(ot)))
                {
                    flst = MakePair(ot, flst);
                    AsPair(lst)->First = NoValueObject;
                }
            }

            lst = Rest(lst);
        }

        if (flst == EmptyListObject)
            break;

        while (flst != EmptyListObject)
        {
            FAssert(PairP(flst));
            FAssert(PairP(First(flst)));

            FObject obj = First(First(flst));
            FObject tconc = Rest(First(flst));

            ScanObject(&obj, fcf, 0);
            ScanObject(&tconc, fcf, 0);
            TConcAdd(tconc, obj);

            flst = Rest(flst);
        }

        CleanScan(fcf);
    }

    while (mbhold != EmptyListObject)
    {
        FAssert(PairP(mbhold));
        FAssert(PairP(First(mbhold)));

        FObject obj = First(First(mbhold));
        FObject tconc = Rest(First(mbhold));

        if (AliveP(tconc))
        {
            ScanObject(&obj, fcf, 0);
            ScanObject(&tconc, fcf, 0);

            if (MatureP(obj) || MaturePairP(obj))
                MatureGuardians = MakePair(MakePair(obj, tconc), MatureGuardians);
            else
                YoungGuardians = MakePair(MakePair(obj, tconc), YoungGuardians);
        }

        mbhold = Rest(mbhold);
    }
}

static void CollectTrackers(FObject trkrs, int fcf)
{
    while (trkrs != EmptyListObject)
    {
        FAssert(PairP(trkrs));
        FAssert(PairP(First(trkrs)));
        FAssert(PairP(Rest(First(trkrs))));

        FObject obj = First(First(trkrs));
        FObject ret = First(Rest(First(trkrs)));
        FObject tconc = Rest(Rest(First(trkrs)));

        FAssert(ObjectP(obj));
        FAssert(ObjectP(tconc));

        if (AliveP(obj) && AliveP(tconc))
        {
            FObject oo = obj;

            ScanObject(&obj, fcf, 0);
            if (ObjectP(ret))
                ScanObject(&ret, fcf, 0);
            ScanObject(&tconc, fcf, 0);

            if (oo == obj)
                InstallTracker(obj, ret, tconc);
            else
                TConcAdd(tconc, ret);
        }

        trkrs = Rest(trkrs);
    }
}

static void Collect(int fcf)
{
/*
    if (fcf)
printf("Full Collection...");
    else
printf("Partial Collection...");
*/

    CollectionCount += 1;
    GCRequired = 0;
    BytesSinceLast = 0;
    ObjectsSinceLast = 0;

    ScanSections->Used = 0;

    FAssert(ScanSections->Next == 0);

    ActiveZero->Next = GenerationZero;
    FYoungSection * gz  = ActiveZero;
    GenerationZero = 0;
    ActiveZero = AllocateYoung(0, ZeroSectionTag);

    FYoungSection * go = GenerationOne;
    GenerationOne = AllocateYoung(0, OneSectionTag);

    if (fcf)
    {
        FullGCRequired = 0;

        FreeBackRefSections();

        unsigned int sdx = 0;
        while (sdx <= UsedSections)
        {
            if (SectionTable[sdx] == MatureSectionTag)
            {
                unsigned int cnt = 1;
                while (SectionTable[sdx + cnt] == MatureSectionTag && sdx + cnt <= UsedSections)
                    cnt += 1;

                unsigned char * ms = (unsigned char *) SectionPointer(sdx);
                FObject obj = (FObject) ms;

                while (obj < ((char *) ms) + SECTION_SIZE * cnt)
                {
                    ClearMark(obj);
                    obj = ((char *) obj) + ObjectSize(obj, IndirectTag(obj));
                }

                sdx += cnt;
            }
            else if (SectionTable[sdx] == PairSectionTag)
            {
                unsigned char * ps = (unsigned char *) SectionPointer(sdx);

                for (unsigned int idx = 0; idx < PAIR_MB_SIZE; idx++)
                    (ps + PAIR_MB_OFFSET)[idx] = 0;

                sdx += 1;
            }
            else
                sdx += 1;
        }
    }

    FObject * rv = (FObject *) &R;
    for (int rdx = 0; rdx < sizeof(FRoots) / sizeof(FObject); rdx++)
        if (ObjectP(rv[rdx]))
            ScanObject(rv + rdx, fcf, 0);

    for (int rdx = 0; rdx < UsedRoots; rdx++)
        if (ObjectP(*Roots[rdx]))
            ScanObject(Roots[rdx], fcf, 0);

    if (ExecuteState != 0)
    {
        for (int adx = 0; adx < ExecuteState->AStackPtr; adx++)
            if (ObjectP(ExecuteState->AStack[adx]))
                ScanObject(ExecuteState->AStack + adx, fcf, 0);

        for (int cdx = 0; cdx < ExecuteState->CStackPtr; cdx++)
            if (ObjectP(ExecuteState->CStack[cdx]))
                ScanObject(ExecuteState->CStack + cdx, fcf, 0);

        if (ObjectP(ExecuteState->Proc))
            ScanObject(&ExecuteState->Proc, fcf, 0);
        if (ObjectP(ExecuteState->Frame))
            ScanObject(&ExecuteState->Frame, fcf, 0);
    }

    if (fcf == 0)
    {
        FBackRefSection * brs = BackRefSections;
        while (brs != 0)
        {
            for (int idx = 0; idx < brs->Used; idx++)
            {
                if (*brs->BackRef[idx].Ref == brs->BackRef[idx].Value)
                {
                    FAssert(ObjectP(*brs->BackRef[idx].Ref));

                    ScanObject(brs->BackRef[idx].Ref, fcf, 0);
                    brs->BackRef[idx].Value = *brs->BackRef[idx].Ref;
                }
            }

            brs = brs->Next;
        }

        if (ObjectP(MatureGuardians))
            ScanObject(&MatureGuardians, fcf, 0);

        if (ObjectP(MatureTrackers))
            ScanObject(&MatureTrackers, fcf, 0);
    }

    CleanScan(fcf);
    CollectGuardians(fcf);

    if (fcf)
    {
        FObject yt = YoungTrackers;
        YoungTrackers = EmptyListObject;
        FObject mt = MatureTrackers;
        MatureTrackers = EmptyListObject;

        CollectTrackers(yt, fcf);
        CollectTrackers(mt, fcf);
    }
    else
    {
        FObject yt = YoungTrackers;
        YoungTrackers = EmptyListObject;

        CollectTrackers(yt, fcf);
    }

    CleanScan(fcf);

    while (gz != 0)
    {
        FYoungSection * ys = gz;
        gz = gz->Next;
#ifdef FOMENT_DEBUG
        memset(ys, 0, SECTION_SIZE);
#endif // FOMENT_DEBUG
        FreeSection(ys);
    }

    while (go != 0)
    {
        FYoungSection * ys = go;
        go = go->Next;
#ifdef FOMENT_DEBUG
        memset(ys, 0, SECTION_SIZE);
#endif // FOMENT_DEBUG
        FreeSection(ys);
    }

    if (fcf)
    {
        FreePairs = 0;
        FreeMature = 0;

        unsigned int sdx = 0;
        while (sdx <= UsedSections)
        {
            if (SectionTable[sdx] == MatureSectionTag)
            {
                unsigned int cnt = 1;
                while (SectionTable[sdx + cnt] == MatureSectionTag && sdx + cnt <= UsedSections)
                    cnt += 1;

                unsigned char * ms = (unsigned char *) SectionPointer(sdx);
                FObject obj = (FObject) ms;
                FFreeObject * pfo = 0;

                while (obj < ((char *) ms) + SECTION_SIZE * cnt)
                {
                    if (MarkP(obj) == 0)
                    {
                        if (pfo == 0)
                        {
                            FFreeObject * fo = (FFreeObject *) obj;
                            fo->Length = MakeLength(ObjectSize(obj, IndirectTag(obj)), GCFreeTag);
                            fo->Next = FreeMature;
                            FreeMature = fo;
                            pfo = fo;
                        }
                        else
                            pfo->Length = MakeLength(ByteLength(pfo)
                                    + ObjectSize(obj, IndirectTag(obj)), GCFreeTag);
                    }
                    else
                        pfo = 0;

                    obj = ((char *) obj) + ObjectSize(obj, IndirectTag(obj));
                }

                sdx += cnt;
            }
            else if (SectionTable[sdx] == PairSectionTag)
            {
                unsigned char * ps = (unsigned char *) SectionPointer(sdx);

                for (unsigned int idx = 0; idx < PAIR_MB_OFFSET / (sizeof(FPair) * 8); idx++)
                    if ((ps + PAIR_MB_OFFSET)[idx] != 0xFF)
                    {
                        FPair * pr = (FPair *) (ps + sizeof(FPair) * idx * 8);
                        for (unsigned int bdx = 0; bdx < 8; bdx++)
                        {
                            if (PairMarkP(pr) == 0)
                            {
                                pr->First = FreePairs;
                                FreePairs = pr;
                            }

                            pr += 1;
                        }
                    }

                sdx += 1;
            }
            else
                sdx += 1;
        }
    }

//printf("Done.\n");
}

void Collect()
{
    FAssert(GCRequired != 0);
    FAssert(PartialPerFull >= 0);

    if (FullGCRequired)
        Collect(1);
    else
        Collect((CollectionCount + 1) % (PartialPerFull + 1) == 0);
}

void InstallGuardian(FObject obj, FObject tconc)
{
    FAssert(ObjectP(obj));
    FAssert(PairP(tconc));
    FAssert(PairP(First(tconc)));
    FAssert(PairP(Rest(tconc)));

    if (MatureP(obj) || MaturePairP(obj))
        MatureGuardians = MakePair(MakePair(obj, tconc), MatureGuardians);
    else
        YoungGuardians = MakePair(MakePair(obj, tconc), YoungGuardians);
}

void InstallTracker(FObject obj, FObject ret, FObject tconc)
{
    FAssert(ObjectP(obj));
    FAssert(PairP(tconc));
    FAssert(PairP(First(tconc)));
    FAssert(PairP(Rest(tconc)));

    if (MatureP(obj) || MaturePairP(obj))
        MatureTrackers = MakePair(MakePair(obj, MakePair(ret, tconc)), MatureTrackers);
    else
        YoungTrackers = MakePair(MakePair(obj, MakePair(ret, tconc)), YoungTrackers);
}

void SetupGC()
{
    FAssert(sizeof(FObject) == sizeof(FImmediate));
    FAssert(sizeof(FObject) == sizeof(char *));
    FAssert(sizeof(FFixnum) <= sizeof(FImmediate));
    FAssert(sizeof(FCh) <= sizeof(FImmediate));

#ifdef FOMENT_DEBUG
    unsigned int len = MakeLength(MAXIMUM_OBJECT_LENGTH, GCFreeTag);
    FAssert(ByteLength(&len) == MAXIMUM_OBJECT_LENGTH);
#endif // FOMENT_DEBUG

    FAssert(SECTION_SIZE == 1024 * 16);
    FAssert(PAIR_MB_OFFSET / (sizeof(FPair) * 8) <= PAIR_MB_SIZE);
    FAssert(MAXIMUM_YOUNG_LENGTH <= SECTION_SIZE / 2);

    SectionTable = (unsigned char *) VirtualAlloc(0, SECTION_SIZE * SECTION_SIZE, MEM_RESERVE,
            PAGE_READWRITE);
    FAssert(SectionTable != 0);

    VirtualAlloc(SectionTable, SECTION_SIZE, MEM_COMMIT, PAGE_READWRITE);

    for (unsigned int sdx = 0; sdx < SECTION_SIZE; sdx++)
        SectionTable[sdx] = HoleSectionTag;

    FAssert(SectionIndex(SectionTable) == 0);

    UsedSections = 1;

    SectionTable[0] = TableSectionTag;

    ActiveZero = AllocateYoung(0, ZeroSectionTag);

    YoungGuardians = EmptyListObject;
    MatureGuardians = EmptyListObject;
    YoungTrackers = EmptyListObject;
    MatureTrackers = EmptyListObject;

    BackRefSections = AllocateBackRefSection(0);
    BackRefSectionCount = 1;

    ScanSections = AllocateScanSection(0);

    for (unsigned int idx = 0; idx < sizeof(Sizes) / sizeof(unsigned int); idx++)
        Sizes[idx] = 0;
}

Define("install-guardian", InstallGuardianPrimitive)(int argc, FObject argv[])
{
    // (install-guardian <obj> <tconc>)

    if (argc != 2)
        RaiseExceptionC(R.Assertion, "install-guardian",
                "install-guardian: expected two arguments", EmptyListObject);

    if (ObjectP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "install-guardian",
                "install-guardian: immediate object unexpected", List(argv[0]));

    if (PairP(argv[1]) == 0 || PairP(First(argv[1])) == 0 || PairP(Rest(argv[1])) == 0)
        RaiseExceptionC(R.Assertion, "install-guardian",
                "install-guardian: expected a tconc", List(argv[1]));

    InstallGuardian(argv[0], argv[1]);
    return(NoValueObject);
}

Define("install-tracker", InstallTrackerPrimitive)(int argc, FObject argv[])
{
    // (install-tracker <obj> <ret> <tconc>)

    if (argc != 3)
        RaiseExceptionC(R.Assertion, "install-tracker",
                "install-tracker: expected three arguments", EmptyListObject);

    if (ObjectP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "install-tracker",
                "install-tracker: immediate object unexpected", List(argv[0]));

    if (PairP(argv[2]) == 0 || PairP(First(argv[2])) == 0 || PairP(Rest(argv[2])) == 0)
        RaiseExceptionC(R.Assertion, "install-tracker",
                "install-tracker: expected a tconc", List(argv[2]));

    InstallTracker(argv[0], argv[1], argv[2]);
    return(NoValueObject);
}

Define("collect", CollectPrimitive)(int argc, FObject argv[])
{
    // (collect [<full>])

    if (argc > 1)
        RaiseExceptionC(R.Assertion, "collect", "collect: expected zero or one arguments",
                EmptyListObject);

    if (argc > 0 && argv[0] != FalseObject)
        Collect(1);
    else
        Collect(0);

    return(NoValueObject);
}

Define("partial-per-full", PartialPerFullPrimitive)(int argc, FObject argv[])
{
    // (partial-per-full [<val>])

    if (argc > 1)
        RaiseExceptionC(R.Assertion, "partial-per-full",
                "partial-per-full: expected zero or one arguments", EmptyListObject);

    if (argc > 0)
    {
        if (FixnumP(argv[0]) == 0 || AsFixnum(argv[0]) < 0)
            RaiseExceptionC(R.Assertion, "partial-per-full",
                "partial-per-full: expected a non-negative fixnum", List(argv[0]));

        PartialPerFull = AsFixnum(argv[0]);
    }

    return(MakeFixnum(PartialPerFull));
}

Define("trigger-bytes", TriggerBytesPrimitive)(int argc, FObject argv[])
{
    // (trigger-bytes [<val>])

    if (argc > 1)
        RaiseExceptionC(R.Assertion, "trigger-bytes",
                "trigger-bytes: expected zero or one arguments", EmptyListObject);

    if (argc > 0)
    {
        if (FixnumP(argv[0]) == 0 || AsFixnum(argv[0]) < 0)
            RaiseExceptionC(R.Assertion, "trigger-bytes",
                "trigger-bytes: expected a non-negative fixnum", List(argv[0]));

        TriggerBytes = AsFixnum(argv[0]);
    }

    return(MakeFixnum(TriggerBytes));
}

Define("trigger-objects", TriggerObjectsPrimitive)(int argc, FObject argv[])
{
    // (trigger-objects [<val>])

    if (argc > 1)
        RaiseExceptionC(R.Assertion, "trigger-objects",
                "trigger-objects: expected zero or one arguments", EmptyListObject);

    if (argc > 0)
    {
        if (FixnumP(argv[0]) == 0 || AsFixnum(argv[0]) < 0)
            RaiseExceptionC(R.Assertion, "trigger-objects",
                "trigger-objects: expected a non-negative fixnum", List(argv[0]));

        TriggerObjects = AsFixnum(argv[0]);
    }

    return(MakeFixnum(TriggerObjects));
}

Define("dump-gc", DumpGCPrimitive)(int argc, FObject argv[])
{
    if (argc != 0)
        RaiseExceptionC(R.Assertion, "dump-gc", "dump-gc: expected zero arguments",
                EmptyListObject);

    for (unsigned int idx = 0; idx < sizeof(Sizes) / sizeof(unsigned int); idx++)
        if (Sizes[idx] > 0)
            printf("%d: %d (%d)\n", idx, Sizes[idx], idx * Sizes[idx]);

    for (unsigned int sdx = 0; sdx < UsedSections; sdx++)
    {
        switch (SectionTable[sdx])
        {
        case HoleSectionTag: printf("Hole\n"); break;
        case FreeSectionTag: printf("Free\n"); break;
        case TableSectionTag: printf("Table\n"); break;
        case ZeroSectionTag: printf("Zero\n"); break;
        case OneSectionTag: printf("One\n"); break;
        case MatureSectionTag: printf("Mature\n"); break;
        case PairSectionTag: printf("Pair\n"); break;
        case BackRefSectionTag: printf("BackRef\n"); break;
        case ScanSectionTag: printf("Scan\n"); break;
        default: printf("Unknown\n"); break;
        }
    }

    FFreeObject * fo = (FFreeObject *) FreeMature;
    while (fo != 0)
    {
        printf("%d ", fo->Length);
        fo = fo->Next;
    }

    printf("\n");

    return(NoValueObject);
}

static FPrimitive * Primitives[] =
{
    &InstallGuardianPrimitive,
    &InstallTrackerPrimitive,
    &CollectPrimitive,
    &PartialPerFullPrimitive,
    &TriggerBytesPrimitive,
    &TriggerObjectsPrimitive,
    &DumpGCPrimitive
};

void SetupMM()
{
    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);

    Eval(ReadStringC(
        "(define (make-guardian)"
            "(let ((tconc (let ((last (cons #f '()))) (cons last last))))"
                "(case-lambda"
                    "(()"
                        "(if (eq? (car tconc) (cdr tconc))"
                            "#f"
                            "(let ((first (car tconc)))"
                                "(set-car! tconc (cdr first))"
                                "(car first))))"
                    "((obj) (install-guardian obj tconc)))))", 1), R.Bedrock);

    LibraryExport(R.BedrockLibrary, EnvironmentLookup(R.Bedrock,
            StringCToSymbol("make-guardian")));

    Eval(ReadStringC(
        "(define (make-tracker)"
            "(let ((tconc (let ((last (cons #f '()))) (cons last last))))"
                "(case-lambda"
                    "(()"
                        "(if (eq? (car tconc) (cdr tconc))"
                            "#f"
                            "(let ((first (car tconc)))"
                                "(set-car! tconc (cdr first))"
                                "(car first))))"
                    "((obj) (install-tracker obj obj tconc))"
                    "((obj ret) (install-tracker obj ret tconc)))))", 1), R.Bedrock);

    LibraryExport(R.BedrockLibrary, EnvironmentLookup(R.Bedrock,
            StringCToSymbol("make-tracker")));
}
