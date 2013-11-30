/*

Foment

*/

#ifdef FOMENT_WINDOWS
#include <windows.h>
#endif // FOMENT_WINDOWS
#ifdef FOMENT_UNIX
#include <pthread.h>
#include <sys/mman.h>
#endif // FOMENT_UNIX
#include <stdio.h>
#include <string.h>
#include "foment.hpp"
#include "syncthrd.hpp"
#include "io.hpp"

typedef void * FRaw;
#define AsRaw(obj) ((FRaw) (((uint_t) (obj)) & ~0x7))

typedef enum
{
    HoleSectionTag,
    FreeSectionTag,
    TableSectionTag,
    ZeroSectionTag, // Generation Zero
    OneSectionTag, // Generation One
    MatureSectionTag,
    TwoSlotSectionTag,
    FlonumSectionTag,
    BackRefSectionTag,
    ScanSectionTag,
    StackSectionTag
} FSectionTag;

static unsigned char * SectionTable;
static uint_t UsedSections;

#define SECTION_SIZE (1024 * 16)
#define SectionIndex(ptr) ((((uint_t) (ptr)) - ((uint_t) SectionTable)) >> 14)
#define SectionPointer(sdx) ((unsigned char *) (((sdx) << 14) + ((uint_t) SectionTable)))
#define SectionOffset(ptr) (((uint_t) (ptr)) & 0x3FFF)
#define SectionBase(ptr) ((unsigned char *) (((uint_t) (ptr)) & ~0x3FFF))

static inline int_t MatureP(FObject obj)
{
    unsigned char st = SectionTable[SectionIndex(obj)];

    return(st == MatureSectionTag || st == TwoSlotSectionTag || st == FlonumSectionTag);
}

#define MAXIMUM_YOUNG_LENGTH (1024 * 4)

static FYoungSection * GenerationZero;
static FYoungSection * GenerationOne;

typedef struct _FFreeObject
{
    uint_t Length;
    struct _FFreeObject * Next;
} FFreeObject;

typedef struct
{
    FObject One;
    FObject Two;
} FTwoSlot;

static FFreeObject * FreeMature = 0;
static FTwoSlot * FreeTwoSlots = 0;
static FTwoSlot * FreeFlonums = 0;

#define TWOSLOTS_PER_SECTION (SECTION_SIZE * 8 / (sizeof(FTwoSlot) * 8 + 1))
#define TWOSLOT_MB_SIZE (TWOSLOTS_PER_SECTION / 8)
#define TWOSLOT_MB_OFFSET (SECTION_SIZE - TWOSLOT_MB_SIZE)

static inline uint_t TwoSlotMarkP(FRaw raw)
{
    uint_t idx = SectionOffset(raw) / sizeof(FTwoSlot);

    return((SectionBase(raw) + TWOSLOT_MB_OFFSET)[idx / 8] & (1 << (idx % 8)));
}

static inline void SetTwoSlotMark(FRaw raw)
{
    uint_t idx = SectionOffset(raw) / sizeof(FTwoSlot);

    (SectionBase(raw) + TWOSLOT_MB_OFFSET)[idx / 8] |= (1 << (idx % 8));
}

uint_t BytesAllocated = 0;
uint_t BytesSinceLast = 0;
uint_t ObjectsSinceLast = 0;
uint_t CollectionCount = 0;
uint_t PartialCount = 0;
uint_t PartialPerFull = 4;
uint_t TriggerBytes = SECTION_SIZE * 8;
uint_t TriggerObjects = TriggerBytes / (sizeof(FPair) * 8);
uint_t MaximumBackRefFraction = 128;

int_t GCRequired;
static int_t FullGCRequired;

#define GCTagP(obj) ImmediateP(obj, GCTagTag)

#define AsForward(obj) ((((FYoungHeader *) (obj)) - 1)->Forward)

typedef struct
{
    FObject Forward;
#ifdef FOMENT_32BIT
    FObject Pad;
#endif // FOMENT_32BIT
} FYoungHeader;

#define MarkP(obj) (*((uint_t *) (obj)) & RESERVED_MARK_BIT)
#define SetMark(obj) *((uint_t *) (obj)) |= RESERVED_MARK_BIT
#define ClearMark(obj) *((uint_t *) (obj)) &= ~RESERVED_MARK_BIT

static inline int_t AliveP(FObject obj)
{
    obj = AsRaw(obj);
    unsigned char st = SectionTable[SectionIndex(obj)];

    if (st == MatureSectionTag)
        return(MarkP(obj));
    else if (st == TwoSlotSectionTag || st == FlonumSectionTag)
        return(TwoSlotMarkP(obj));
    return(GCTagP(AsForward(obj)) == 0);
}

typedef struct
{
    FObject * Ref;
    FObject Value;
} FBackRef;

typedef struct _BackRefSection
{
    struct _BackRefSection * Next;
    uint_t Used;
    FBackRef BackRef[1];
} FBackRefSection;

static FBackRefSection * BackRefSections;
static int_t BackRefSectionCount;

typedef struct _ScanSection
{
    struct _ScanSection * Next;
    uint_t Used;
    FObject Scan[1];
} FScanSection;

static FScanSection * ScanSections;

static FObject YoungGuardians;
static FObject MatureGuardians;
static FObject YoungTrackers;

#ifdef FOMENT_WINDOWS
unsigned int TlsIndex;
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
pthread_key_t ThreadKey;
#endif // FOMENT_UNIX

OSExclusive GCExclusive;
static OSCondition ReadyCondition;
static OSCondition DoneCondition;

static int_t GCHappening;
uint_t TotalThreads;
static uint_t WaitThreads;
static uint_t CollectThreads;
static FThreadState * Threads;

static uint_t Sizes[1024 * 8];

static void * AllocateSection(uint_t cnt, FSectionTag tag)
{
    uint_t sdx;
    uint_t fcnt = 0;

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
#ifdef FOMENT_WINDOWS
    VirtualAlloc(sec, SECTION_SIZE * cnt, MEM_COMMIT, PAGE_READWRITE);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    mprotect(sec, SECTION_SIZE * cnt, PROT_READ | PROT_WRITE);
#endif // FOMENT_UNIX

    while (cnt > 0)
    {
        cnt -= 1;
        SectionTable[sdx + cnt] = tag;
    }

    return(sec);
}

static void FreeSection(void * sec)
{
    uint_t sdx = SectionIndex(sec);

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

#if 0
#ifdef FOMENT_32BIT
const static uint_t Align[4] = {0, 3, 2, 1};
#endif // FOMENT_32BIT

#ifdef FOMENT_64BIT
#endif // FOMENT_64BIT
#endif // 0

#define OBJECT_ALIGNMENT 8
const static uint_t Align[8] = {0, 7, 6, 5, 4, 3, 2, 1};

static uint_t ObjectSize(FObject obj, uint_t tag)
{
    uint_t len;

    switch (tag)
    {
    case PairTag:
    case RatnumTag:
    case ComplexTag:
        FAssert(sizeof(FPair) % OBJECT_ALIGNMENT == 0);

        return(sizeof(FPair));

    case FlonumTag:
        FAssert(sizeof(FFlonum) % OBJECT_ALIGNMENT == 0);

        return(sizeof(FFlonum));

    case BoxTag:
        FAssert(BoxP(obj));
        FAssert(sizeof(FBox) % OBJECT_ALIGNMENT == 0);

        return(sizeof(FBox));

    case StringTag:
        FAssert(StringP(obj));

        len = sizeof(FString) + sizeof(FCh) * StringLength(obj);
        break;

    case VectorTag:
        FAssert(VectorP(obj));
        FAssert(VectorLength(obj) >= 0);

        len = sizeof(FVector) + sizeof(FObject) * (VectorLength(obj) - 1);
        break;

    case BytevectorTag:
        FAssert(BytevectorP(obj));

        len = sizeof(FBytevector) + sizeof(FByte) * (BytevectorLength(obj) - 1);
        break;

    case BinaryPortTag:
        FAssert(BinaryPortP(obj));

        len = sizeof(FBinaryPort);
        break;

    case TextualPortTag:
        FAssert(TextualPortP(obj));

        len = sizeof(FTextualPort);
        break;

    case ProcedureTag:
        FAssert(ProcedureP(obj));

        len = sizeof(FProcedure);
        break;

    case SymbolTag:
        FAssert(SymbolP(obj));

        len = sizeof(FSymbol);
        break;

    case RecordTypeTag:
        FAssert(RecordTypeP(obj));

        len = sizeof(FRecordType) + sizeof(FObject) * (RecordTypeNumFields(obj) - 1);
        break;

    case RecordTag:
        FAssert(GenericRecordP(obj));

        len = sizeof(FGenericRecord) + sizeof(FObject) * (RecordNumFields(obj) - 1);
        break;

    case PrimitiveTag:
        FAssert(PrimitiveP(obj));

        len = sizeof(FPrimitive);
        break;

    case ThreadTag:
        FAssert(ThreadP(obj));

        len = sizeof(FThread);
        break;

    case ExclusiveTag:
        FAssert(ExclusiveP(obj));

        len = sizeof(FExclusive);
        break;

    case ConditionTag:
        FAssert(ConditionP(obj));

        len = sizeof(FCondition);
        break;

    case BignumTag:
        FAssert(BignumP(obj));

        len = sizeof(FBignum) + sizeof(FFixnum) * (BignumLength(obj) - 1);
        break;

    case GCFreeTag:
        return(ByteLength(obj));

    default:
        FAssert(0);

        return(0);
    }

    len += Align[len % OBJECT_ALIGNMENT];
    return(len);
}

// Allocate a new object in GenerationZero.
FObject MakeObject(uint_t sz, uint_t tag)
{
    uint_t len = sz;
    len += Align[len % OBJECT_ALIGNMENT];

    FAssert(len >= sz);
    FAssert(len % OBJECT_ALIGNMENT == 0);
    FAssert(len >= OBJECT_ALIGNMENT);

    if (len < sizeof(Sizes) / sizeof(uint_t))
        Sizes[len] += 1;

    if (len > MAXIMUM_YOUNG_LENGTH)
        return(0);

    FThreadState * ts = GetThreadState();
    BytesAllocated += len;
    ts->ObjectsSinceLast += 1;

    if (ts->ActiveZero == 0 || ts->ActiveZero->Used + len + sizeof(FYoungHeader) > SECTION_SIZE)
    {
        EnterExclusive(&GCExclusive);

        if (ts->ActiveZero != 0)
        {
            ObjectsSinceLast += ts->ObjectsSinceLast;
            ts->ObjectsSinceLast = 0;

            if (ObjectsSinceLast > TriggerObjects)
                GCRequired = 1;

            BytesSinceLast += ts->ActiveZero->Used;
            if (BytesSinceLast > TriggerBytes)
                GCRequired = 1;

            FAssert(ts->ActiveZero->Next == 0);

            ts->ActiveZero->Next = GenerationZero;
            GenerationZero = ts->ActiveZero;
        }

        ts->ActiveZero = AllocateYoung(0, ZeroSectionTag);

        LeaveExclusive(&GCExclusive);
    }

    FYoungHeader * yh = (FYoungHeader *) (((char *) ts->ActiveZero) + ts->ActiveZero->Used);
    ts->ActiveZero->Used += len + sizeof(FYoungHeader);
    FObject obj = (FObject) (yh + 1);

    AsForward(obj) = MakeImmediate(tag, GCTagTag);

    FAssert(AsValue(yh->Forward) == tag);
    FAssert(GCTagP(yh->Forward));
    FAssert(SectionTable[SectionIndex(obj)] == ZeroSectionTag);

    FAssert((((uint_t) (obj)) & 0x7) == 0);

    return(obj);
}

// Copy an object from GenerationZero to GenerationOne.
static FObject CopyObject(uint_t len, uint_t tag)
{
    FAssert(len % OBJECT_ALIGNMENT == 0);
    FAssert(len >= OBJECT_ALIGNMENT);
    FAssert(len <= MAXIMUM_OBJECT_LENGTH);

    if (GenerationOne->Used + len + sizeof(FYoungHeader) > SECTION_SIZE)
        GenerationOne = AllocateYoung(GenerationOne, OneSectionTag);

    FYoungHeader * yh = (FYoungHeader *) (((char *) GenerationOne) + GenerationOne->Used);
    GenerationOne->Used += len + sizeof(FYoungHeader);
    FObject obj = (FObject) (yh + 1);

    AsForward(obj) = MakeImmediate(tag, GCTagTag);

    FAssert(AsValue(yh->Forward) == tag);
    FAssert(GCTagP(yh->Forward));
    FAssert(SectionTable[SectionIndex(obj)] == OneSectionTag);

    return(obj);
}

static FObject MakeMature(uint_t len)
{
    FAssert(len % OBJECT_ALIGNMENT == 0);
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

    uint_t cnt = 4;
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

FObject MakeMatureObject(uint_t sz, const char * who)
{
    if (sz > MAXIMUM_OBJECT_LENGTH)
        RaiseExceptionC(R.Restriction, who, "length greater than maximum object length",
                EmptyListObject);

    uint_t len = sz;
    len += Align[len % OBJECT_ALIGNMENT];

    FAssert(len >= sz);
    FAssert(len % OBJECT_ALIGNMENT == 0);
    FAssert(len >= OBJECT_ALIGNMENT);

    EnterExclusive(&GCExclusive);
    FObject obj = MakeMature(len);
    LeaveExclusive(&GCExclusive);
    if (obj == 0)
        RaiseExceptionC(R.Assertion, who, "out of memory", EmptyListObject);

    BytesAllocated += len;

    FAssert((((uint_t) (obj)) & 0x7) == 0);

    return(obj);
}

FObject MakePinnedObject(uint_t len, const char * who)
{
    return(MakeMatureObject(len, who));
}

static FObject MakeMatureTwoSlot()
{
    if (FreeTwoSlots == 0)
    {
        FTwoSlot * ts = (FTwoSlot *) AllocateSection(1, TwoSlotSectionTag);

        for (uint_t idx = 0; idx < TWOSLOT_MB_OFFSET / sizeof(FTwoSlot); idx++)
        {
            ts->One = FreeTwoSlots;
            FreeTwoSlots = ts;
            ts += 1;
        }
    }

    FObject obj = (FObject) FreeTwoSlots;
    FreeTwoSlots = (FTwoSlot *) FreeTwoSlots->One;

    return(obj);
}

static FObject MakeMatureFlonum()
{
    if (FreeFlonums == 0)
    {
        FTwoSlot * ts = (FTwoSlot *) AllocateSection(1, FlonumSectionTag);

        for (uint_t idx = 0; idx < TWOSLOT_MB_OFFSET / sizeof(FTwoSlot); idx++)
        {
            ts->One = FreeFlonums;
            FreeFlonums = ts;
            ts += 1;
        }
    }

    FObject obj = (FObject) FreeFlonums;
    FreeFlonums = (FTwoSlot *) FreeFlonums->One;

    return(obj);
}

void PushRoot(FObject * rt)
{
    FThreadState * ts = GetThreadState();

    ts->UsedRoots += 1;

    FAssert(ts->UsedRoots < sizeof(ts->Roots) / sizeof(FObject *));

    ts->Roots[ts->UsedRoots - 1] = rt;
}

void PopRoot()
{
    FThreadState * ts = GetThreadState();

    FAssert(ts->UsedRoots > 0);

    ts->UsedRoots -= 1;
}

void ClearRoots()
{
    FThreadState * ts = GetThreadState();

    ts->UsedRoots = 0;
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

void ModifyVector(FObject obj, uint_t idx, FObject val)
{
    FAssert(VectorP(obj));
    FAssert(idx < VectorLength(obj));

    AsVector(obj)->Vector[idx] = val;

    if (MatureP(obj) && ObjectP(val) && MatureP(val) == 0)
    {
        EnterExclusive(&GCExclusive);
        RecordBackRef(AsVector(obj)->Vector + idx, val);
        LeaveExclusive(&GCExclusive);
    }
}

void ModifyObject(FObject obj, uint_t off, FObject val)
{
    FAssert(IndirectP(obj));
    FAssert(off % sizeof(FObject) == 0);

    ((FObject *) obj)[off / sizeof(FObject)] = val;

    if (MatureP(obj) && ObjectP(val) && MatureP(val) == 0)
    {
        EnterExclusive(&GCExclusive);
        RecordBackRef(((FObject *) obj) + (off / sizeof(FObject)), val);
        LeaveExclusive(&GCExclusive);
    }
}

void SetFirst(FObject obj, FObject val)
{
    FAssert(PairP(obj));

    AsPair(obj)->First = val;

    if (MatureP(obj) && ObjectP(val) && MatureP(val) == 0)
    {
        EnterExclusive(&GCExclusive);
        RecordBackRef(&(AsPair(obj)->First), val);
        LeaveExclusive(&GCExclusive);
    }
}

void SetRest(FObject obj, FObject val)
{
    FAssert(PairP(obj));

    AsPair(obj)->Rest = val;

    if (MatureP(obj) && ObjectP(val) && MatureP(val) == 0)
    {
        EnterExclusive(&GCExclusive);
        RecordBackRef(&(AsPair(obj)->Rest), val);
        LeaveExclusive(&GCExclusive);
    }
}

void SetBox(FObject bx, FObject val)
{
    FAssert(BoxP(bx));

    AsBox(bx)->Value = val;

    if (MatureP(bx) && ObjectP(val) && MatureP(val) == 0)
    {
        EnterExclusive(&GCExclusive);
        RecordBackRef(&(AsBox(bx)->Value), val);
        LeaveExclusive(&GCExclusive);
    }
}

static void AddToScan(FObject obj)
{
    if (ScanSections->Used == ((SECTION_SIZE - sizeof(FScanSection)) / sizeof(FObject)) + 1)
        ScanSections = AllocateScanSection(ScanSections);

    ScanSections->Scan[ScanSections->Used] = obj;
    ScanSections->Used += 1;
}

static void ScanObject(FObject * pobj, int_t fcf, int_t mf)
{
    FAssert(ObjectP(*pobj));

    FObject raw = AsRaw(*pobj);
    uint_t sdx = SectionIndex(raw);

    FAssert(sdx < UsedSections);

    if (SectionTable[sdx] == MatureSectionTag)
    {
        if (fcf && MarkP(raw) == 0)
        {
            SetMark(raw);
            AddToScan(*pobj);
        }
    }
    else if (SectionTable[sdx] == TwoSlotSectionTag)
    {
        if (fcf && TwoSlotMarkP(raw) == 0)
        {
            SetTwoSlotMark(raw);
            AddToScan(*pobj);
        }
    }
    else if (SectionTable[sdx] == FlonumSectionTag)
    {
        if (fcf && TwoSlotMarkP(raw) == 0)
            SetTwoSlotMark(raw);
    }
    else if (SectionTable[sdx] == ZeroSectionTag)
    {
        if (GCTagP(AsForward(raw)))
        {
            uint_t tag = AsValue(AsForward(raw));
            uint_t len = ObjectSize(raw, tag);
            FObject nobj = CopyObject(len, tag);
            memcpy(nobj, raw, len);

            if (tag == PairTag)
                nobj = PairObject(nobj);
            else if (tag == RatnumTag)
                nobj = RatnumObject(nobj);
            else if (tag == ComplexTag)
                nobj = ComplexObject(nobj);
            else if (tag == FlonumTag)
                nobj = FlonumObject(nobj);

            AsForward(raw) = nobj;
            *pobj = nobj;

            if (mf)
                RecordBackRef(pobj, nobj);
        }
        else
        {
            *pobj = AsForward(raw);

            if (mf)
                RecordBackRef(pobj, AsForward(raw));
        }
    }
    else // if (SectionTable[sdx] == OneSectionTag)
    {
        FAssert(SectionTable[sdx] == OneSectionTag);

        if (GCTagP(AsForward(raw)))
        {
            uint_t tag = AsValue(AsForward(raw));
            uint_t len = ObjectSize(raw, tag);

            FObject nobj;
            if (tag == PairTag || tag == RatnumTag || tag == ComplexTag)
            {
                nobj = MakeMatureTwoSlot();
                SetTwoSlotMark(nobj);
            }
            else if (tag == FlonumTag)
            {
                nobj = MakeMatureFlonum();
                SetTwoSlotMark(nobj);
            }
            else
                nobj = MakeMature(len);

            memcpy(nobj, raw, len);

            if (tag == PairTag)
                nobj = PairObject(nobj);
            else if (tag == RatnumTag)
                nobj = RatnumObject(nobj);
            else if (tag == ComplexTag)
                nobj = ComplexObject(nobj);
            else if (tag == FlonumTag)
                nobj = FlonumObject(nobj);
            else
                SetMark(nobj);

            AddToScan(nobj);

            AsForward(raw) = nobj;
            *pobj = nobj;
        }
        else
            *pobj = AsForward(raw);
    }
}

static void CleanScan(int_t fcf);
static void ScanChildren(FRaw raw, uint_t tag, int_t fcf)
{
    int_t mf = MatureP(raw);

    switch (tag)
    {
    case PairTag:
    case RatnumTag:
    case ComplexTag:
    {
        FPair * pr = (FPair *) raw;

        if (ObjectP(pr->First))
            ScanObject(&pr->First, fcf, mf);
        if (ObjectP(pr->Rest))
            ScanObject(&pr->Rest, fcf, mf);
        break;
    }

    case FlonumTag:
        break;

    case BoxTag:
        if (ObjectP(AsBox(raw)->Value))
            ScanObject(&(AsBox(raw)->Value), fcf, mf);
        break;

    case StringTag:
        break;

    case VectorTag:
        for (uint_t vdx = 0; vdx < VectorLength(raw); vdx++)
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

    case BinaryPortTag:
    case TextualPortTag:
        if (ObjectP(AsGenericPort(raw)->Name))
            ScanObject(&(AsGenericPort(raw)->Name), fcf, mf);
        if (ObjectP(AsGenericPort(raw)->Object))
            ScanObject(&(AsGenericPort(raw)->Object), fcf, mf);
        break;

    case ProcedureTag:
        if (ObjectP(AsProcedure(raw)->Name))
            ScanObject(&(AsProcedure(raw)->Name), fcf, mf);
        if (ObjectP(AsProcedure(raw)->Code))
            ScanObject(&(AsProcedure(raw)->Code), fcf, mf);
        break;

    case SymbolTag:
        if (ObjectP(AsSymbol(raw)->String))
            ScanObject(&(AsSymbol(raw)->String), fcf, mf);
        break;

    case RecordTypeTag:
        for (uint_t fdx = 0; fdx < RecordTypeNumFields(raw); fdx++)
            if (ObjectP(AsRecordType(raw)->Fields[fdx]))
                ScanObject(AsRecordType(raw)->Fields + fdx, fcf, mf);
        break;

    case RecordTag:
        for (uint_t fdx = 0; fdx < RecordNumFields(raw); fdx++)
            if (ObjectP(AsGenericRecord(raw)->Fields[fdx]))
                ScanObject(AsGenericRecord(raw)->Fields + fdx, fcf, mf);
        break;

    case PrimitiveTag:
        break;

    case ThreadTag:
        if (ObjectP(AsThread(raw)->Result))
            ScanObject(&(AsThread(raw)->Result), fcf, mf);
        if (ObjectP(AsThread(raw)->Thunk))
            ScanObject(&(AsThread(raw)->Thunk), fcf, mf);
        if (ObjectP(AsThread(raw)->Parameters))
            ScanObject(&(AsThread(raw)->Parameters), fcf, mf);
        if (ObjectP(AsThread(raw)->IndexParameters))
            ScanObject(&(AsThread(raw)->IndexParameters), fcf, mf);
        break;

    case ExclusiveTag:
        break;

    case ConditionTag:
        break;

    case BignumTag:
        break;

    default:
        FAssert(0);
    }
}

static void CleanScan(int_t fcf)
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
            else if (RatnumP(obj))
                ScanChildren(AsRaw(obj), RatnumTag, fcf);
            else if (ComplexP(obj))
                ScanChildren(AsRaw(obj), ComplexTag, fcf);
            else if (FlonumP(obj) == 0)
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
                FYoungHeader * yh = (FYoungHeader *) (((char *) ys) + ys->Scan);

                FAssert(SectionTable[SectionIndex(yh)] == OneSectionTag);
                FAssert(GCTagP(yh->Forward));

                uint_t tag = AsValue(yh->Forward);
                FObject obj = (FObject) (yh + 1);

                ScanChildren(obj, tag, fcf);
                ys->Scan += ObjectSize(obj, tag) + sizeof(FYoungHeader);
            }

            ys = ys->Next;
        }

        if (GenerationOne->Scan == GenerationOne->Used && ScanSections->Used == 0)
            break;
    }
}

static void CollectGuardians(int_t fcf)
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

            if (MatureP(obj))
                MatureGuardians = MakePair(MakePair(obj, tconc), MatureGuardians);
            else
                YoungGuardians = MakePair(MakePair(obj, tconc), YoungGuardians);
        }

        mbhold = Rest(mbhold);
    }
}

static void CollectTrackers(FObject trkrs, int_t fcf, int_t mtf)
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

        if (AliveP(obj) && AliveP(ret) && AliveP(tconc))
        {
            FObject oo = obj;

            ScanObject(&obj, fcf, 0);
            if (ObjectP(ret))
                ScanObject(&ret, fcf, 0);
            ScanObject(&tconc, fcf, 0);

            FAssert(mtf == 0 || oo == obj);

            if (oo == obj)
                InstallTracker(obj, ret, tconc);
            else
                TConcAdd(tconc, ret);
        }

        trkrs = Rest(trkrs);
    }
}

static void Collect(int_t fcf)
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

    FThreadState * ts = Threads;
    while (ts != 0)
    {
        if (ts->ActiveZero != 0)
        {
            ts->ActiveZero->Next = GenerationZero;
            GenerationZero = ts->ActiveZero;
            ts->ActiveZero = 0;
        }

        ts->ObjectsSinceLast = 0;
        ts = ts->Next;
    }

    FYoungSection * gz  = GenerationZero;
    GenerationZero = 0;

    FYoungSection * go = GenerationOne;
    GenerationOne = AllocateYoung(0, OneSectionTag);

    if (fcf)
    {
        FullGCRequired = 0;

        FreeBackRefSections();

        uint_t sdx = 0;
        while (sdx < UsedSections)
        {
            if (SectionTable[sdx] == MatureSectionTag)
            {
                uint_t cnt = 1;
                while (sdx + cnt < UsedSections && SectionTable[sdx + cnt] == MatureSectionTag)
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
            else if (SectionTable[sdx] == TwoSlotSectionTag
                    || SectionTable[sdx] == FlonumSectionTag)
            {
                unsigned char * tss = (unsigned char *) SectionPointer(sdx);

                for (uint_t idx = 0; idx < TWOSLOT_MB_SIZE; idx++)
                    (tss + TWOSLOT_MB_OFFSET)[idx] = 0;

                sdx += 1;
            }
            else
                sdx += 1;
        }
    }

    FObject * rv = (FObject *) &R;
    for (uint_t rdx = 0; rdx < sizeof(FRoots) / sizeof(FObject); rdx++)
        if (ObjectP(rv[rdx]))
            ScanObject(rv + rdx, fcf, 0);

    ts = Threads;
    while (ts != 0)
    {
        FAssert(ObjectP(ts->Thread));
        ScanObject(&(ts->Thread), fcf, 0);

        for (uint_t rdx = 0; rdx < ts->UsedRoots; rdx++)
            if (ObjectP(*ts->Roots[rdx]))
                ScanObject(ts->Roots[rdx], fcf, 0);

        for (int_t adx = 0; adx < ts->AStackPtr; adx++)
            if (ObjectP(ts->AStack[adx]))
                ScanObject(ts->AStack + adx, fcf, 0);

        for (int_t cdx = 0; cdx < ts->CStackPtr; cdx++)
            if (ObjectP(ts->CStack[- cdx]))
                ScanObject(ts->CStack - cdx, fcf, 0);

        if (ObjectP(ts->Proc))
            ScanObject(&ts->Proc, fcf, 0);
        if (ObjectP(ts->Frame))
            ScanObject(&ts->Frame, fcf, 0);
        if (ObjectP(ts->DynamicStack))
            ScanObject(&ts->DynamicStack, fcf, 0);
        if (ObjectP(ts->Parameters))
            ScanObject(&ts->Parameters, fcf, 0);

        for (int_t idx = 0; idx < INDEX_PARAMETERS; idx++)
            if (ObjectP(ts->IndexParameters[idx]))
                ScanObject(ts->IndexParameters + idx, fcf, 0);

        ts = ts->Next;
    }

    if (fcf == 0)
    {
        FBackRefSection * brs = BackRefSections;
        while (brs != 0)
        {
            for (uint_t idx = 0; idx < brs->Used; idx++)
            {
                if (*brs->BackRef[idx].Ref == brs->BackRef[idx].Value)
                {
                    FAssert(ObjectP(*brs->BackRef[idx].Ref));

                    ScanObject(brs->BackRef[idx].Ref, fcf, 0);
                    brs->BackRef[idx].Value = *brs->BackRef[idx].Ref;
                }
                else
                    brs->BackRef[idx].Value = 0;
            }

            brs = brs->Next;
        }

        if (ObjectP(MatureGuardians))
            ScanObject(&MatureGuardians, fcf, 0);
    }

    CleanScan(fcf);
    CollectGuardians(fcf);

    FObject yt = YoungTrackers;
    YoungTrackers = EmptyListObject;
    CollectTrackers(yt, fcf, 0);

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
        FreeTwoSlots = 0;
        FreeFlonums = 0;
        FreeMature = 0;

        uint_t sdx = 0;
        while (sdx < UsedSections)
        {
            if (SectionTable[sdx] == MatureSectionTag)
            {
                uint_t cnt = 1;
                while (sdx + cnt < UsedSections && SectionTable[sdx + cnt] == MatureSectionTag)
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
            else if (SectionTable[sdx] == TwoSlotSectionTag
                    || SectionTable[sdx] == FlonumSectionTag)
            {
                unsigned char * tss = (unsigned char *) SectionPointer(sdx);

                for (uint_t idx = 0; idx < TWOSLOT_MB_OFFSET / (sizeof(FTwoSlot) * 8); idx++)
                    if ((tss + TWOSLOT_MB_OFFSET)[idx] != 0xFF)
                    {
                        FTwoSlot * ts = (FTwoSlot *) (tss + sizeof(FTwoSlot) * idx * 8);
                        for (uint_t bdx = 0; bdx < 8; bdx++)
                        {
                            if (TwoSlotMarkP(ts) == 0)
                            {
                                if (SectionTable[sdx] == TwoSlotSectionTag)
                                {
                                    ts->One = FreeTwoSlots;
                                    FreeTwoSlots = ts;
                                }
                                else
                                {
                                    FAssert(SectionTable[sdx] == FlonumSectionTag);

                                    ts->One = FreeFlonums;
                                    FreeFlonums = ts;
                                }
                            }

                            ts += 1;
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

void EnterWait()
{
    EnterExclusive(&GCExclusive);
    WaitThreads += 1;
    if (GCHappening && TotalThreads == WaitThreads + CollectThreads)
        WakeCondition(&ReadyCondition);
    LeaveExclusive(&GCExclusive);
}

void LeaveWait()
{
    EnterExclusive(&GCExclusive);
    WaitThreads -= 1;

    if (GCHappening)
    {
        CollectThreads += 1;
        ConditionWait(&DoneCondition, &GCExclusive);
    }
    LeaveExclusive(&GCExclusive);
}

void Collect()
{
    EnterExclusive(&GCExclusive);
    FAssert(GCRequired);

    if (GCHappening == 0)
    {
        FAssert(PartialPerFull >= 0);

        CollectThreads += 1;
        GCHappening = 1;

        while (WaitThreads + CollectThreads != TotalThreads)
        {
            FAssert(WaitThreads + CollectThreads < TotalThreads);

            ConditionWait(&ReadyCondition, &GCExclusive);
        }
        LeaveExclusive(&GCExclusive);

        if (FullGCRequired || PartialCount >= PartialPerFull)
        {
            PartialCount = 0;
            Collect(1);
        }
        else
        {
            PartialCount += 1;
            Collect(0);
        }

        EnterExclusive(&GCExclusive);
        CollectThreads = 0;
        GCHappening = 0;
        LeaveExclusive(&GCExclusive);

        WakeAllCondition(&DoneCondition);

        while (TConcEmptyP(R.CleanupTConc) == 0)
        {
            FObject obj = TConcRemove(R.CleanupTConc);

            if (ExclusiveP(obj))
                DeleteExclusive(&AsExclusive(obj)->Exclusive);
            else if (ConditionP(obj))
                DeleteCondition(&AsCondition(obj)->Condition);
            else if (BinaryPortP(obj) || TextualPortP(obj))
            {
                CloseInput(obj);
                CloseOutput(obj);
            }
            else
            {
                FAssert(0);
            }
        }
    }
    else
    {
        CollectThreads += 1;
        if (TotalThreads == WaitThreads + CollectThreads)
            WakeCondition(&ReadyCondition);

        ConditionWait(&DoneCondition, &GCExclusive);
        LeaveExclusive(&GCExclusive);
    }
}

void InstallGuardian(FObject obj, FObject tconc)
{
    EnterExclusive(&GCExclusive);

    FAssert(ObjectP(obj));
    FAssert(PairP(tconc));
    FAssert(PairP(First(tconc)));
    FAssert(PairP(Rest(tconc)));

    if (MatureP(obj))
        MatureGuardians = MakePair(MakePair(obj, tconc), MatureGuardians);
    else
        YoungGuardians = MakePair(MakePair(obj, tconc), YoungGuardians);

    LeaveExclusive(&GCExclusive);
}

void InstallTracker(FObject obj, FObject ret, FObject tconc)
{
    FAssert(ObjectP(obj));
    FAssert(PairP(tconc));
    FAssert(PairP(First(tconc)));
    FAssert(PairP(Rest(tconc)));

    if (MatureP(obj) == 0)
        YoungTrackers = MakePair(MakePair(obj, MakePair(ret, tconc)), YoungTrackers);
}

void EnterThread(FThreadState * ts, FObject thrd, FObject prms, FObject idxprms)
{
#ifdef FOMENT_WINDOWS
    FAssert(TlsGetValue(TlsIndex) == 0);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    FAssert(pthread_getspecific(ThreadKey) == 0);
#endif // FOMENT_UNIX

    SetThreadState(ts);

    EnterExclusive(&GCExclusive);
    FAssert(TotalThreads > 0);

    if (Threads == 0)
        ts->Next = 0;
    else
    {
        ts->Next = Threads;
        Threads->Previous = ts;
    }

    ts->Previous = 0;
    Threads = ts;
    LeaveExclusive(&GCExclusive);

    ts->Thread = thrd;
    ts->ActiveZero = 0;
    ts->ObjectsSinceLast = 0;
    ts->UsedRoots = 0;
    ts->StackSize = SECTION_SIZE / sizeof(FObject);
    ts->AStackPtr = 0;
    ts->AStack = (FObject *) AllocateSection(1, StackSectionTag);
    ts->CStackPtr = 0;
    ts->CStack = ts->AStack + ts->StackSize - 1;
    ts->Proc = NoValueObject;
    ts->Frame = NoValueObject;
    ts->IP = -1;
    ts->ArgCount = -1;
    ts->DynamicStack = EmptyListObject;
    ts->Parameters = prms;

    if (VectorP(idxprms))
    {
        FAssert(VectorLength(idxprms) == INDEX_PARAMETERS);

        for (int_t idx = 0; idx < INDEX_PARAMETERS; idx++)
            ts->IndexParameters[idx] = AsVector(idxprms)->Vector[idx];
    }
    else
        for (int_t idx = 0; idx < INDEX_PARAMETERS; idx++)
            ts->IndexParameters[idx] = NoValueObject;
}

void LeaveThread(FThreadState * ts)
{
    FAssert(ts == GetThreadState());
    SetThreadState(0);

    FAssert(ThreadP(ts->Thread));

    if (AsThread(ts->Thread)->Handle != 0)
    {
#ifdef FOMENT_WINDOWS
        CloseHandle(AsThread(ts->Thread)->Handle);
#endif // FOMENT_WINDOWS
        AsThread(ts->Thread)->Handle = 0;
    }

    EnterExclusive(&GCExclusive);
    FAssert(TotalThreads > 0);
    TotalThreads -= 1;

    FAssert(Threads != 0);

    if (Threads == ts)
    {
        Threads = ts->Next;
        if (Threads != 0)
        {
            FAssert(TotalThreads > 0);

            Threads->Previous = 0;
        }
    }
    else
    {
        if (ts->Next != 0)
            ts->Next->Previous = ts->Previous;

        FAssert(ts->Previous != 0);
        ts->Previous->Next = ts->Next;
    }

    LeaveExclusive(&GCExclusive);
    WakeCondition(&ReadyCondition); // Just in case a collection is pending.

    FAssert(ts->AStack != 0);
    FAssert((ts->StackSize * sizeof(FObject)) % SECTION_SIZE == 0);

    int_t cnt = (ts->StackSize * sizeof(FObject)) / SECTION_SIZE;
    for (int_t sdx = 0; sdx < cnt; sdx++)
        FreeSection(ts->AStack + sdx * (SECTION_SIZE / sizeof(FObject)));

    ts->AStack = 0;
    ts->CStack = 0;
    ts->Thread = NoValueObject;
}

void SetupCore(FThreadState * ts)
{
    FAssert(sizeof(FObject) == sizeof(int_t));
    FAssert(sizeof(FObject) == sizeof(uint_t));
    FAssert(sizeof(FObject) == sizeof(FImmediate));
    FAssert(sizeof(FObject) == sizeof(char *));
    FAssert(sizeof(FFixnum) <= sizeof(FImmediate));
    FAssert(sizeof(FCh) <= sizeof(FImmediate));
    FAssert(sizeof(FTwoSlot) == sizeof(FObject) * 2);
    FAssert(sizeof(FPair) == sizeof(FTwoSlot));
    FAssert(sizeof(FRatnum) == sizeof(FTwoSlot));
    FAssert(sizeof(FComplex) == sizeof(FTwoSlot));
    FAssert(sizeof(FYoungHeader) == OBJECT_ALIGNMENT);

#ifdef FOMENT_DEBUG
    uint_t len = MakeLength(MAXIMUM_OBJECT_LENGTH, GCFreeTag);
    FAssert(ByteLength(&len) == MAXIMUM_OBJECT_LENGTH);

    if (strcmp(FOMENT_MEMORYMODEL, "ilp32") == 0)
    {
        FAssert(sizeof(int) == 4);
        FAssert(sizeof(long) == 4);
        FAssert(sizeof(void *) == 4);
    }
    else if (strcmp(FOMENT_MEMORYMODEL, "lp64") == 0)
    {
        FAssert(sizeof(int) == 4);
        FAssert(sizeof(long) == 8);
        FAssert(sizeof(void *) == 8);
    }
    else if (strcmp(FOMENT_MEMORYMODEL, "llp64") == 0)
    {
        FAssert(sizeof(int) == 4);
        FAssert(sizeof(long) == 4);
//#ifdef FOMENT_WINDOWS
        FAssert(sizeof(long long) == 8);
//#endif // FOMENT_WINDOWS
        FAssert(sizeof(void *) == 8);
    }
#endif // FOMENT_DEBUG

    FAssert(SECTION_SIZE == 1024 * 16);
    FAssert(TWOSLOT_MB_OFFSET / (sizeof(FTwoSlot) * 8) <= TWOSLOT_MB_SIZE);
    FAssert(TWOSLOTS_PER_SECTION % 8 == 0);
    FAssert(MAXIMUM_YOUNG_LENGTH <= SECTION_SIZE / 2);

#ifdef FOMENT_WINDOWS
    SectionTable = (unsigned char *) VirtualAlloc(0, SECTION_SIZE * SECTION_SIZE, MEM_RESERVE,
            PAGE_READWRITE);
    FAssert(SectionTable != 0);

    VirtualAlloc(SectionTable, SECTION_SIZE, MEM_COMMIT, PAGE_READWRITE);

    TlsIndex = TlsAlloc();
    FAssert(TlsIndex != TLS_OUT_OF_INDEXES);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    SectionTable = (unsigned char *) mmap(0, (SECTION_SIZE + 1) * SECTION_SIZE, PROT_NONE,
					  MAP_PRIVATE | MAP_ANONYMOUS, -1 ,0);
    FAssert(SectionTable != 0);

    if (SectionTable != SectionBase(SectionTable))
      SectionTable += (SECTION_SIZE - SectionOffset(SectionTable));

    mprotect(SectionTable, SECTION_SIZE, PROT_READ | PROT_WRITE);    

    pthread_key_create(&ThreadKey, 0);
#endif // FOMENT_UNIX

    FAssert(SectionTable == SectionBase(SectionTable));

    for (uint_t sdx = 0; sdx < SECTION_SIZE; sdx++)
        SectionTable[sdx] = HoleSectionTag;

    FAssert(SectionIndex(SectionTable) == 0);

    UsedSections = 1;

    SectionTable[0] = TableSectionTag;

    YoungGuardians = EmptyListObject;
    MatureGuardians = EmptyListObject;
    YoungTrackers = EmptyListObject;

    BackRefSections = AllocateBackRefSection(0);
    BackRefSectionCount = 1;

    ScanSections = AllocateScanSection(0);

    InitializeExclusive(&GCExclusive);
    InitializeCondition(&ReadyCondition);
    InitializeCondition(&DoneCondition);

    TotalThreads = 1;
    WaitThreads = 0;
    Threads = 0;
    GCRequired = 1;
    GCHappening = 0;
    FullGCRequired = 0;

#ifdef FOMENT_WINDOWS
    HANDLE h = OpenThread(STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | 0x3FF, 0,
            GetCurrentThreadId());
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    pthread_t h = pthread_self();
#endif // FOMENT_UNIX

    EnterThread(ts, NoValueObject, NoValueObject, NoValueObject);
    ts->Thread = MakeThread(h, NoValueObject, NoValueObject, NoValueObject);

    for (uint_t idx = 0; idx < sizeof(Sizes) / sizeof(uint_t); idx++)
        Sizes[idx] = 0;

    R.CleanupTConc = MakeTConc();
}

Define("install-guardian", InstallGuardianPrimitive)(int_t argc, FObject argv[])
{
    // (install-guardian <obj> <tconc>)

    TwoArgsCheck("install-guardian", argc);
    TConcArgCheck("install-guardian", argv[1]);

    if (ObjectP(argv[0]))
        InstallGuardian(argv[0], argv[1]);

    return(NoValueObject);
}

Define("install-tracker", InstallTrackerPrimitive)(int_t argc, FObject argv[])
{
    // (install-tracker <obj> <ret> <tconc>)

    ThreeArgsCheck("install-tracker", argc);
    TConcArgCheck("install-tracker", argv[2]);

    if (ObjectP(argv[0]))
        InstallTracker(argv[0], argv[1], argv[2]);

    return(NoValueObject);
}

Define("collect", CollectPrimitive)(int_t argc, FObject argv[])
{
    // (collect [<full>])

    ZeroOrOneArgsCheck("collect", argc);

    EnterExclusive(&GCExclusive);
    GCRequired = 1;
    if (argc > 0 && argv[0] != FalseObject)
        FullGCRequired = 1;
    LeaveExclusive(&GCExclusive);

    CheckForGC();
    return(NoValueObject);
}

Define("partial-per-full", PartialPerFullPrimitive)(int_t argc, FObject argv[])
{
    // (partial-per-full [<val>])

    ZeroOrOneArgsCheck("partial-per-full", argc);

    if (argc > 0)
    {
        NonNegativeArgCheck("partial-per-full", argv[0]);

        PartialPerFull = AsFixnum(argv[0]);
    }

    return(MakeFixnum(PartialPerFull));
}

Define("trigger-bytes", TriggerBytesPrimitive)(int_t argc, FObject argv[])
{
    // (trigger-bytes [<val>])

    ZeroOrOneArgsCheck("trigger-bytes", argc);

    if (argc > 0)
    {
        NonNegativeArgCheck("trigger-bytes", argv[0]);

        TriggerBytes = AsFixnum(argv[0]);
    }

    return(MakeFixnum(TriggerBytes));
}

Define("trigger-objects", TriggerObjectsPrimitive)(int_t argc, FObject argv[])
{
    // (trigger-objects [<val>])

    ZeroOrOneArgsCheck("trigger-objects", argc);

    if (argc > 0)
    {
        NonNegativeArgCheck("trigger-objects", argv[0]);

        TriggerObjects = AsFixnum(argv[0]);
    }

    return(MakeFixnum(TriggerObjects));
}

Define("dump-gc", DumpGCPrimitive)(int_t argc, FObject argv[])
{
    ZeroArgsCheck("dump-gc", argc);

    for (uint_t idx = 0; idx < sizeof(Sizes) / sizeof(uint_t); idx++)
        if (Sizes[idx] > 0)
	  printf("%d: %d (%d)\n", (int) idx, (int) Sizes[idx], (int) (idx * Sizes[idx]));

    for (uint_t sdx = 0; sdx < UsedSections; sdx++)
    {
        switch (SectionTable[sdx])
        {
        case HoleSectionTag: printf("Hole\n"); break;
        case FreeSectionTag: printf("Free\n"); break;
        case TableSectionTag: printf("Table\n"); break;
        case ZeroSectionTag: printf("Zero\n"); break;
        case OneSectionTag: printf("One\n"); break;
        case MatureSectionTag: printf("Mature\n"); break;
        case TwoSlotSectionTag: printf("TwoSlot\n"); break;
        case FlonumSectionTag: printf("Flonum\n"); break;
        case BackRefSectionTag: printf("BackRef\n"); break;
        case ScanSectionTag: printf("Scan\n"); break;
        case StackSectionTag: printf("Stack\n"); break;
        default: printf("Unknown\n"); break;
        }
    }

    FFreeObject * fo = (FFreeObject *) FreeMature;
    while (fo != 0)
    {
        printf("%d ", (int) fo->Length);
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

void SetupGC()
{
    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
