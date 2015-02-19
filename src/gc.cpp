/*

Foment

*/

#ifdef FOMENT_WINDOWS
#include <windows.h>
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
#include <pthread.h>
#include <sys/mman.h>
#ifdef __APPLE__
#define MAP_ANONYMOUS MAP_ANON
#endif // __APPLE__
#endif // FOMENT_UNIX

#ifdef FOMENT_BSD
#include <stdlib.h>
#else // FOMENT_BSD
#include <malloc.h>
#endif // FOMENT_BSD
#include <stdio.h>
#include <string.h>
#include "foment.hpp"
#include "syncthrd.hpp"
#include "io.hpp"
#include "numbers.hpp"

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
    TrackerSectionTag,
    GuardianSectionTag
} FSectionTag;

#define SECTION_SIZE (1024 * 16)
#define SectionIndex(ptr) ((((uint_t) (ptr)) - ((uint_t) SectionTable)) >> 14)
#define SectionPointer(sdx) ((unsigned char *) (((sdx) << 14) + ((uint_t) SectionTable)))
#define SectionOffset(ptr) (((uint_t) (ptr)) & 0x3FFF)
#define SectionBase(ptr) ((unsigned char *) (((uint_t) (ptr)) & ~0x3FFF))

void * SectionTableBase = 0;
static unsigned char * SectionTable;
static uint_t UsedSections;

#ifdef FOMENT_64BIT
static uint_t MaximumSections = SECTION_SIZE * 32; // 4 gigabype heap
#endif // FOMENT_64BIT
#ifdef FOMENT_32BIT
static uint_t MaximumSections = SECTION_SIZE * 8; // 1 gigabyte heap
#endif // FOMENT_32BIT

static inline int_t MatureP(FObject obj)
{
    unsigned char st = SectionTable[SectionIndex(obj)];

    return(st == MatureSectionTag || st == TwoSlotSectionTag || st == FlonumSectionTag);
}

static inline FSectionTag SectionTag(FObject obj)
{
    return((FSectionTag) SectionTable[SectionIndex(obj)]);
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

volatile uint_t BytesAllocated = 0;
static volatile uint_t BytesSinceLast = 0;
static uint_t ObjectsSinceLast = 0;
uint_t CollectionCount = 0;
static uint_t PartialCount = 0;
static uint_t PartialPerFull = 4;
static uint_t TriggerBytes = SECTION_SIZE * 8;
static uint_t TriggerObjects = TriggerBytes / (sizeof(FPair) * 8);
static uint_t MaximumBackRefFraction = 128;

int_t volatile GCRequired;
static volatile int_t FullGCRequired;

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

typedef struct _Tracker
{
    struct _Tracker * Next;
    FObject Object;
    FObject Return;
    FObject TConc;
} FTracker;

static FTracker * YoungTrackers = 0;
static FTracker * FreeTrackers = 0;

typedef struct _Guardian
{
    struct _Guardian * Next;
    FObject Object;
    FObject TConc;
} FGuardian;

static FGuardian * YoungGuardians = 0;
static FGuardian * MatureGuardians = 0;
static FGuardian * FreeGuardians = 0;

#ifdef FOMENT_WINDOWS
unsigned int TlsIndex;
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
pthread_key_t ThreadKey;
#endif // FOMENT_UNIX

OSExclusive GCExclusive;
static OSCondition ReadyCondition;
static OSCondition DoneCondition;

static volatile int_t GCHappening;
volatile uint_t TotalThreads;
static volatile uint_t WaitThreads;
static volatile uint_t CollectThreads;
FThreadState * Threads;

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

    if (UsedSections + cnt > MaximumSections)
        RaiseExceptionC(R.Assertion, "foment", "out of memory", List(MakeFixnum(tag)));

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

#define OBJECT_ALIGNMENT 8
const static uint_t Align[8] = {0, 7, 6, 5, 4, 3, 2, 1};

static uint_t ObjectSize(FObject obj, uint_t tag, int ln)
{
    uint_t len;

    switch (tag)
    {
    case PairTag:
    case RatioTag:
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
        FAssert(sizeof(FBignum) % OBJECT_ALIGNMENT == 0);

        return(sizeof(FBignum));

    case HashTreeTag:
        FAssert(HashTreeP(obj));
        FAssert(HashTreeLength(obj) >= 0);

        len = sizeof(FHashTree) + sizeof(FObject) * (HashTreeLength(obj) - 1);
        break;

    case GCFreeTag:
        return(ByteLength(obj));

    default:
#ifdef FOMENT_DEBUG
        printf("Called From: %d\n", ln);
        FAssert(0);
#endif // FOMENT_DEBUG

        return(0xFFFF);
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
    fo->Next = 0;
    fo->Length = MakeLength(SECTION_SIZE * cnt - len, GCFreeTag);
    FreeMature = fo;
    return(((char *) FreeMature) + ByteLength(fo));
}

static FObject MakeMatureObject(uint_t sz, const char * who)
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
        unsigned char * tss = (unsigned char *) AllocateSection(1, TwoSlotSectionTag);

        for (uint_t idx = 0; idx < TWOSLOT_MB_SIZE; idx++)
            (tss + TWOSLOT_MB_OFFSET)[idx] = 0;

        FTwoSlot * ts = (FTwoSlot *) tss;

        for (uint_t idx = 0; idx < TWOSLOT_MB_OFFSET / sizeof(FTwoSlot); idx++)
        {
            ts->One = FreeTwoSlots;
            FreeTwoSlots = ts;
            ts += 1;
        }
    }

    FObject obj = (FObject) FreeTwoSlots;
    FreeTwoSlots = (FTwoSlot *) FreeTwoSlots->One;

    SetTwoSlotMark(obj);
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
    FAssert(val != 0);
    FAssert(*ref == val);
    FAssert(ObjectP(val));
    FAssert(MatureP(val) == 0);
    FAssert(MatureP(ref));

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

    if (AsVector(obj)->Vector[idx] != val)
    {
        AsVector(obj)->Vector[idx] = val;

        if (MatureP(obj) && ObjectP(val) && MatureP(val) == 0)
        {
            EnterExclusive(&GCExclusive);
            RecordBackRef(AsVector(obj)->Vector + idx, val);
            LeaveExclusive(&GCExclusive);
        }
    }
}

void ModifyObject(FObject obj, uint_t off, FObject val)
{
    FAssert(IndirectP(obj));
    FAssert(off % sizeof(FObject) == 0);

    if (((FObject *) obj)[off / sizeof(FObject)] != val)
    {
        ((FObject *) obj)[off / sizeof(FObject)] = val;

        if (MatureP(obj) && ObjectP(val) && MatureP(val) == 0)
        {
            EnterExclusive(&GCExclusive);
            RecordBackRef(((FObject *) obj) + (off / sizeof(FObject)), val);
            LeaveExclusive(&GCExclusive);
        }
    }
}

void SetFirst(FObject obj, FObject val)
{
    FAssert(PairP(obj));

    if (AsPair(obj)->First != val)
    {
        AsPair(obj)->First = val;

        if (MatureP(obj) && ObjectP(val) && MatureP(val) == 0)
        {
            EnterExclusive(&GCExclusive);
            RecordBackRef(&(AsPair(obj)->First), val);
            LeaveExclusive(&GCExclusive);
        }
    }
}

void SetRest(FObject obj, FObject val)
{
    FAssert(PairP(obj));

    if (AsPair(obj)->Rest != val)
    {
        AsPair(obj)->Rest = val;

        if (MatureP(obj) && ObjectP(val) && MatureP(val) == 0)
        {
            EnterExclusive(&GCExclusive);
            RecordBackRef(&(AsPair(obj)->Rest), val);
            LeaveExclusive(&GCExclusive);
        }
    }
}

void SetBox(FObject bx, FObject val)
{
    FAssert(BoxP(bx));

    if (AsBox(bx)->Value != val)
    {
        AsBox(bx)->Value = val;

        if (MatureP(bx) && ObjectP(val) && MatureP(val) == 0)
        {
            EnterExclusive(&GCExclusive);
            RecordBackRef(&(AsBox(bx)->Value), val);
            LeaveExclusive(&GCExclusive);
        }
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
    FAssert(mf == 0 || MatureP(pobj));
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
            uint_t len = ObjectSize(raw, tag, __LINE__);
            FObject nobj = CopyObject(len, tag);
            memcpy(nobj, raw, len);

            if (tag == PairTag)
                nobj = PairObject(nobj);
            else if (tag == RatioTag)
                nobj = RatioObject(nobj);
            else if (tag == ComplexTag)
                nobj = ComplexObject(nobj);
            else if (tag == FlonumTag)
                nobj = FlonumObject(nobj);

            AsForward(raw) = nobj;
            *pobj = nobj;

            FAssert(SectionTag(AsForward(raw)) == OneSectionTag || MatureP(AsForward(raw)));

            if (mf)
                RecordBackRef(pobj, nobj);
        }
        else
        {
            FAssert(SectionTag(AsForward(raw)) == OneSectionTag || MatureP(AsForward(raw)));

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
            uint_t len = ObjectSize(raw, tag, __LINE__);

            FObject nobj;
            if (tag == PairTag || tag == RatioTag || tag == ComplexTag)
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
            else if (tag == RatioTag)
                nobj = RatioObject(nobj);
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

        FAssert(MatureP(*pobj));
    }
}

static void CleanScan(int_t fcf);
static void ScanChildren(FRaw raw, uint_t tag, int_t fcf)
{
    int_t mf = MatureP(raw);

    switch (tag)
    {
    case PairTag:
    case RatioTag:
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
        if (ObjectP(AsProcedure(raw)->Filename))
            ScanObject(&(AsProcedure(raw)->Filename), fcf, mf);
        if (ObjectP(AsProcedure(raw)->LineNumber))
            ScanObject(&(AsProcedure(raw)->LineNumber), fcf, mf);
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

    case HashTreeTag:
        for (uint_t bdx = 0; bdx < HashTreeLength(raw); bdx++)
        {
            if (ObjectP(AsHashTree(raw)->Buckets[bdx]))
                ScanObject(AsHashTree(raw)->Buckets + bdx, fcf, mf);
        }
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
            else if (RatioP(obj))
                ScanChildren(AsRaw(obj), RatioTag, fcf);
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
                ys->Scan += ObjectSize(obj, tag, __LINE__) + sizeof(FYoungHeader);
            }

            ys = ys->Next;
        }

        if (GenerationOne->Scan == GenerationOne->Used && ScanSections->Used == 0)
        {
            FAssert(ScanSections->Next == 0);

            break;
        }
    }
}

static FGuardian * MakeGuardian(FGuardian * nxt, FObject obj, FObject tconc)
{
    if (FreeGuardians == 0)
    {
        FreeGuardians = (FGuardian *) AllocateSection(1, GuardianSectionTag);
        FreeGuardians->Next = 0;
        for (uint_t idx = 1; idx < SECTION_SIZE / sizeof(FGuardian); idx++)
        {
            FreeGuardians += 1;
            FreeGuardians->Next = (FreeGuardians - 1);
        }
    }

    FGuardian * grd = FreeGuardians;
    FreeGuardians = FreeGuardians->Next;
    grd->Next = nxt;
    grd->Object = obj;
    grd->TConc = tconc;
    return(grd);
}

static void FreeGuardian(FGuardian * grd)
{
    grd->Next = FreeGuardians;
    FreeGuardians = grd;
}

static FGuardian * CollectGuardians(int_t fcf)
{
    FGuardian * mbhold = 0;
    FGuardian * mbfinal = 0;
    FGuardian * final = 0;

    while (YoungGuardians != 0)
    {
        FGuardian * grd = YoungGuardians;
        YoungGuardians = YoungGuardians->Next;

        if (AliveP(grd->Object))
        {
            grd->Next = mbhold;
            mbhold = grd;
        }
        else
        {
            grd->Next = mbfinal;
            mbfinal = grd;
        }
    }

    if (fcf)
    {
        while (MatureGuardians != 0)
        {
            FGuardian * grd = MatureGuardians;
            MatureGuardians = MatureGuardians->Next;

            if (AliveP(grd->Object))
            {
                grd->Next = mbhold;
                mbhold = grd;
            }
            else
            {
                grd->Next = mbfinal;
                mbfinal = grd;
            }
        }
    }

    for (;;)
    {
        FGuardian * flst = 0;
        FGuardian * lst = mbfinal;
        mbfinal = 0;

        while (lst != 0)
        {
            FGuardian * grd = lst;
            lst = lst->Next;

            if (AliveP(grd->TConc))
            {
                grd->Next = flst;
                flst = grd;
            }
            else
            {
                grd->Next = mbfinal;
                mbfinal = grd;
            }
        }

        if (flst == 0)
            break;

        while (flst != 0)
        {
            FGuardian * grd = flst;
            flst = flst->Next;

            ScanObject(&grd->Object, fcf, 0);
            ScanObject(&grd->TConc, fcf, 0);

            grd->Next = final;
            final = grd;
        }

        CleanScan(fcf);
    }

    while (mbfinal != 0)
    {
        FGuardian * grd = mbfinal;
        mbfinal = mbfinal->Next;

        FreeGuardian(grd);
    }

    while (mbhold != 0)
    {
        FGuardian * grd = mbhold;
        mbhold = mbhold->Next;

        if (AliveP(grd->TConc))
        {
            ScanObject(&grd->Object, fcf, 0);
            ScanObject(&grd->TConc, fcf, 0);

            if (MatureP(grd->Object))
            {
                grd->Next = MatureGuardians;
                MatureGuardians = grd;
            }
            else
            {
                grd->Next = YoungGuardians;
                YoungGuardians = grd;
            }
        }
        else
            FreeGuardian(grd);
    }

    return(final);
}

static FTracker * MakeTracker(FTracker * nxt, FObject obj, FObject ret, FObject tconc)
{
    if (FreeTrackers == 0)
    {
        FreeTrackers = (FTracker *) AllocateSection(1, TrackerSectionTag);
        FreeTrackers->Next = 0;
        for (uint_t idx = 1; idx < SECTION_SIZE / sizeof(FTracker); idx++)
        {
            FreeTrackers += 1;
            FreeTrackers->Next = (FreeTrackers - 1);
        }
    }

    FTracker * trkr = FreeTrackers;
    FreeTrackers = FreeTrackers->Next;
    trkr->Next = nxt;
    trkr->Object = obj;
    trkr->Return = ret;
    trkr->TConc = tconc;
    return(trkr);
}

static void FreeTracker(FTracker * trkr)
{
    trkr->Next = FreeTrackers;
    FreeTrackers = trkr;
}

static FTracker * CollectTrackers(FTracker * trkrs, int_t fcf)
{
    FTracker * moved = 0;

    while (trkrs != 0)
    {
        FTracker * trkr = trkrs;
        trkrs = trkrs->Next;

        FAssert(ObjectP(trkr->Object));
        FAssert(ObjectP(trkr->TConc));

        if (AliveP(trkr->Object) && AliveP(trkr->Return) && AliveP(trkr->TConc))
        {
            FObject obj = trkr->Object;

            ScanObject(&obj, fcf, 0);
            if (ObjectP(trkr->Return))
                ScanObject(&trkr->Return, fcf, 0);
            ScanObject(&trkr->TConc, fcf, 0);

            if (obj == trkr->Object)
            {
                trkr->Next = YoungTrackers;
                YoungTrackers = trkr;
            }
            else
            {
                trkr->Object = obj;
                trkr->Next = moved;
                moved = trkr;
            }
        }
        else
            FreeTracker(trkr);
    }

    return(moved);
}

static const char * ObjectName(uint_t tag)
{
    switch (tag)
    {
    case PairTag:
        return("pair");

    case RatioTag:
        return("ratio");

    case ComplexTag:
        return("complex");

    case FlonumTag:
        return("flonum");

    case BoxTag:
        return("box");

    case StringTag:
        return("string");

    case VectorTag:
        return("vector");

    case BytevectorTag:
        return("bytevector");

    case BinaryPortTag:
        return("binary-port");

    case TextualPortTag:
        return("textual-port");

    case ProcedureTag:
        return("procedure");

    case SymbolTag:
        return("symbol");

    case RecordTypeTag:
        return("record-type");

    case RecordTag:
        return("record");

    case PrimitiveTag:
        return("primitive");

    case ThreadTag:
        return("thread");

    case ExclusiveTag:
        return("exclusive");

    case ConditionTag:
        return("condition");

    case BignumTag:
        return("bignum");

    case HashTreeTag:
        return("hash-tree");

    case GCFreeTag:
        return("gc-free");

    default:
        return("unknown");
    }
}

static const char * SectionName(uint_t sdx)
{
    switch (sdx)
    {
    case HoleSectionTag:
        return("hole");

    case FreeSectionTag:
        return("free");

    case TableSectionTag:
        return("table");

    case ZeroSectionTag:
        return("zero");

    case OneSectionTag:
        return("one");

    case MatureSectionTag:
        return("mature");

    case TwoSlotSectionTag:
        return("two-slot");

    case FlonumSectionTag:
        return("flonum");

    case BackRefSectionTag:
        return("back-ref");

    case ScanSectionTag:
        return("scan");

    case TrackerSectionTag:
        return("tracker");

    case GuardianSectionTag:
        return("guardian");

    default:
        return("unknown");
    }
}

static void ValidateSlot(FRaw raw, FObject * ref, uint_t tag, uint_t sec, uint_t off, int ln,
    int vdx, const char * slot)
{
    FObject val = *ref;

    if (ObjectP(val))
    {
        uint_t sdx = SectionIndex(val);

        if (sdx >= UsedSections ||
            (SectionTable[sdx] == ZeroSectionTag || SectionTable[sdx] == OneSectionTag
                || SectionTable[sdx] == MatureSectionTag || SectionTable[sdx] == TwoSlotSectionTag
                || SectionTable[sdx] == FlonumSectionTag) == 0)
        {
            printf("internal error: line: %d bad object %p in %s section offset: %lu\n",
                    ln, raw, SectionName(SectionTable[SectionIndex(raw)]),
                    (unsigned long) SectionOffset(raw));
            if (vdx == -1)
                printf("%s->%s = ", ObjectName(tag), slot);
            else
                printf("len: %lu %s[%d] = ",
                        (unsigned long) (tag == StringTag ? StringLength(raw) : ObjectLength(raw)),
                        ObjectName(tag), vdx);
            if (sdx >= UsedSections)
                printf("%p sdx: %lu >= used sections: %lu\n", val, (unsigned long) sdx,
                        (unsigned long) UsedSections);
            else
                printf("%p in %s section offset: %lu\n", val, SectionName(SectionTable[sdx]),
                        (unsigned long) SectionOffset(val));

            if (SectionTable[SectionIndex(raw)] == ZeroSectionTag)
            {
                FThreadState * ts = Threads;

                while (ts != 0)
                {
                    if (SectionIndex(ts->ActiveZero) == SectionIndex(raw))
                    {
                        if (ts == GetThreadState())
                            printf("object in zero section of current thread\n");
                        else
                            printf("object in zero section of thread: %p\n", ts->Thread);
                    }

                    ts = ts->Next;
                }
            }
        }

//        FAssert(sdx < UsedSections);
//        FAssert(SectionTable[sdx] == ZeroSectionTag || SectionTable[sdx] == OneSectionTag
//                || SectionTable[sdx] == MatureSectionTag || SectionTable[sdx] == TwoSlotSectionTag
//                || SectionTable[sdx] == FlonumSectionTag);

        if (FullGCRequired == 0 && MatureP(raw) && MatureP(val) == 0)
        {
            FBackRefSection * brs = BackRefSections;

            while (brs != 0)
            {
                for (uint_t idx = 0; idx < brs->Used; idx++)
                {
                    if (brs->BackRef[idx].Ref == ref && brs->BackRef[idx].Value == val)
                        return;
                }

                brs = brs->Next;
            }

            printf("internal error: missing back reference: tag: %d object: %p\n",
                    PairP(raw) ? (int) PairTag : (int) IndirectTag(raw), raw);
        }
    }
    else
    {
        if ((PairP(val) || RatioP(val) || ComplexP(val) || FlonumP(val) || FixnumP(val)
                || ImmediateP(val, CharacterTag) || ImmediateP(val, MiscellaneousTag)
                || ImmediateP(val, SpecialSyntaxTag) || ImmediateP(val, InstructionTag)
                || ImmediateP(val, ValuesCountTag)) == 0)
        {
            printf("internal error: line: %d bad object %p in %s section offset: %lu\n",
                    ln, raw, SectionName(SectionTable[SectionIndex(raw)]),
                    (unsigned long) SectionOffset(raw));
            if (vdx == -1)
                printf("%s->%s = ", ObjectName(tag), slot);
            else
                printf("%s[%d] = ", ObjectName(tag), vdx);
            printf("%p\n", val);
        }

//        FAssert(PairP(val) || RatioP(val) || ComplexP(val) || FlonumP(val) || FixnumP(val)
//                || ImmediateP(val, CharacterTag) || ImmediateP(val, MiscellaneousTag)
//                || ImmediateP(val, SpecialSyntaxTag) || ImmediateP(val, InstructionTag)
//                || ImmediateP(val, ValuesCountTag));
    }
}

static void ValidateObject(FRaw raw, uint_t tag, uint_t sec, uint_t off, int ln)
{
    switch (tag)
    {
    case PairTag:
        ValidateSlot(raw, &((FPair *) raw)->First, tag, sec, off, ln, -1, "first");
        ValidateSlot(raw, &((FPair *) raw)->Rest, tag, sec, off, ln, -1, "rest");
        break;

    case RatioTag:
        ValidateSlot(raw, &((FPair *) raw)->First, tag, sec, off, ln, -1, "numerator");
        ValidateSlot(raw, &((FPair *) raw)->Rest, tag, sec, off, ln, -1, "denominator");
        break;

    case ComplexTag:
        ValidateSlot(raw, &((FPair *) raw)->First, tag, sec, off, ln, -1, "real");
        ValidateSlot(raw, &((FPair *) raw)->Rest, tag, sec, off, ln, -1, "imaginary");
        break;

    case FlonumTag:
        break;

    case BoxTag:
        ValidateSlot(raw, &AsBox(raw)->Value, tag, sec, off, ln, -1, "value");
        break;

    case StringTag:
        break;

    case VectorTag:
        for (uint_t vdx = 0; vdx < VectorLength(raw); vdx++)
            ValidateSlot(raw, &AsVector(raw)->Vector[vdx], tag, sec, off, ln, (int) vdx, 0);
        break;

    case BytevectorTag:
        break;

    case BinaryPortTag:
    case TextualPortTag:
        ValidateSlot(raw, &AsGenericPort(raw)->Name, tag, sec, off, ln, -1, "name");
        ValidateSlot(raw, &AsGenericPort(raw)->Object, tag, sec, off, ln, -1, "object");
        break;

    case ProcedureTag:
        ValidateSlot(raw, &AsProcedure(raw)->Name, tag, sec, off, ln, -1, "name");
        ValidateSlot(raw, &AsProcedure(raw)->Filename, tag, sec, off, ln, -1, "filename");
        ValidateSlot(raw, &AsProcedure(raw)->LineNumber, tag, sec, off, ln, -1, "line-number");
        ValidateSlot(raw, &AsProcedure(raw)->Code, tag, sec, off, ln, -1, "code");
        break;

    case SymbolTag:
        ValidateSlot(raw, &AsSymbol(raw)->String, tag, sec, off, ln, -1, "string");
        break;

    case RecordTypeTag:
        for (uint_t fdx = 0; fdx < RecordTypeNumFields(raw); fdx++)
            ValidateSlot(raw, &AsRecordType(raw)->Fields[fdx], tag, sec, off, ln, (int) fdx, 0);
        break;

    case RecordTag:
        for (uint_t fdx = 0; fdx < RecordNumFields(raw); fdx++)
            ValidateSlot(raw, &AsGenericRecord(raw)->Fields[fdx], tag, sec, off, ln, (int) fdx, 0);
        break;

    case PrimitiveTag:
        break;

    case ThreadTag:
        ValidateSlot(raw, &AsThread(raw)->Result, tag, sec, off, ln, -1, "result");
        ValidateSlot(raw, &AsThread(raw)->Thunk, tag, sec, off, ln, -1, "thunk");
        ValidateSlot(raw, &AsThread(raw)->Parameters, tag, sec, off, ln, -1, "parameters");
        ValidateSlot(raw, &AsThread(raw)->IndexParameters, tag, sec, off, ln, -1,
                "index-parameters");
        break;

    case ExclusiveTag:
        break;

    case ConditionTag:
        break;

    case BignumTag:
        break;

    case HashTreeTag:
        for (uint_t bdx = 0; bdx < HashTreeLength(raw); bdx++)
            ValidateSlot(raw, &AsHashTree(raw)->Buckets[bdx], tag, sec, off, ln, (int) bdx, 0);
        break;

    case GCFreeTag:
        break;

    default:
        printf("internal error: line: %d bad tag: %x %p\n", ln, (int) tag, raw);
//        FAssert(0);
    }
}

static void ValidateSections(int ln)
{
    uint_t sdx = 0;
    while (sdx < UsedSections)
    {
        FAssert(SectionTable[sdx] == HoleSectionTag || SectionTable[sdx] == FreeSectionTag ||
                SectionTable[sdx] == TableSectionTag || SectionTable[sdx] == ZeroSectionTag ||
                SectionTable[sdx] == OneSectionTag || SectionTable[sdx] == MatureSectionTag ||
                SectionTable[sdx] == TwoSlotSectionTag || SectionTable[sdx] == FlonumSectionTag ||
                SectionTable[sdx] == BackRefSectionTag || SectionTable[sdx] == ScanSectionTag ||
                SectionTable[sdx] == TrackerSectionTag || SectionTable[sdx] == GuardianSectionTag);

        if (SectionTable[sdx] == ZeroSectionTag || SectionTable[sdx] == OneSectionTag)
        {
            FYoungSection * ys = (FYoungSection *) SectionPointer(sdx);
            uint_t idx = sizeof(FYoungSection);

            while (idx < ys->Used)
            {
                FYoungHeader * yh = (FYoungHeader *) (((char *) ys) + idx);

                uint_t tag = AsValue(yh->Forward);
                FObject obj = (FObject) (yh + 1);

                FAssert(SectionIndex(obj) == sdx);
                FAssert(SectionOffset(obj) == idx + sizeof(FYoungHeader));

                ValidateObject(obj, tag, sdx, idx, ln);

                idx += ObjectSize(obj, tag, __LINE__) + sizeof(FYoungHeader);
            }

            sdx += 1;
        }
        else if (SectionTable[sdx] == MatureSectionTag)
        {
            uint_t cnt = 1;
            while (sdx + cnt < UsedSections && SectionTable[sdx + cnt] == MatureSectionTag)
                cnt += 1;

            unsigned char * ms = (unsigned char *) SectionPointer(sdx);
            FObject obj = (FObject) ms;

            while (obj < ms + SECTION_SIZE * cnt)
            {
                FAssert(SectionIndex(obj) >= sdx && SectionIndex(obj) < sdx + cnt);

                ValidateObject(obj, IndirectTag(obj), sdx, 0, ln);
                obj = ((char *) obj) + ObjectSize(obj, IndirectTag(obj), __LINE__);
            }

            sdx += cnt;
        }
        else if (SectionTable[sdx] == TwoSlotSectionTag)
        {
            FTwoSlot * ts = (FTwoSlot *) SectionPointer(sdx);

            for (uint_t idx = 0; idx < TWOSLOT_MB_OFFSET / sizeof(FTwoSlot); idx++)
            {
                if (TwoSlotMarkP(ts))
                {
                    FAssert(ts->One != 0);
                    FAssert(ts->Two != 0);

                    ValidateObject(ts, PairTag, sdx, idx, ln);
                }

                ts += 1;
            }

            sdx += 1;
        }
        else
            sdx += 1;
    }
}

static void ValidateRoot(FObject obj, int ln)
{
    uint_t tag;

    if (PairP(obj))
        tag = PairTag;
    else if (RatioP(obj))
        tag = RatioTag;
    else if (ComplexP(obj))
        tag = ComplexTag;
    else if (FlonumP(obj))
        tag = FlonumTag;
    else
        tag = IndirectTag(obj);

    ValidateObject(AsRaw(obj), tag, SectionIndex(obj), SectionOffset(obj), ln);
}

void ValidateGC(int ln)
{
    ValidateSections(ln);

    FObject * rv = (FObject *) &R;
    for (uint_t rdx = 0; rdx < sizeof(FRoots) / sizeof(FObject); rdx++)
        if (ObjectP(rv[rdx]))
            ValidateRoot(rv[rdx], ln);

    FThreadState * ts = Threads;
    while (ts != 0)
    {
        FAssert(ObjectP(ts->Thread));
        ValidateRoot(ts->Thread, ln);

        for (FAlive * ap = ts->AliveList; ap != 0; ap = ap->Next)
            if (ObjectP(*ap->Pointer))
                ValidateRoot(*ap->Pointer, ln);

        for (uint_t rdx = 0; rdx < ts->UsedRoots; rdx++)
            if (ObjectP(*ts->Roots[rdx]))
                ValidateRoot(*ts->Roots[rdx], ln);

        for (int_t adx = 0; adx < ts->AStackPtr; adx++)
            if (ObjectP(ts->AStack[adx]))
                ValidateRoot(ts->AStack[adx], ln);

        for (int_t cdx = 0; cdx < ts->CStackPtr; cdx++)
            if (ObjectP(ts->CStack[- cdx]))
                ValidateRoot(ts->CStack[ - cdx], ln);

        if (ObjectP(ts->Proc))
            ValidateRoot(ts->Proc, ln);
        if (ObjectP(ts->Frame))
            ValidateRoot(ts->Frame, ln);
        if (ObjectP(ts->DynamicStack))
            ValidateRoot(ts->DynamicStack, ln);
        if (ObjectP(ts->Parameters))
            ValidateRoot(ts->Parameters, ln);

        for (int_t idx = 0; idx < INDEX_PARAMETERS; idx++)
            if (ObjectP(ts->IndexParameters[idx]))
                ValidateRoot(ts->IndexParameters[idx], ln);

        if (ObjectP(ts->NotifyObject))
            ValidateRoot(ts->NotifyObject, ln);

        ts = ts->Next;
    }

    FGuardian * grds = MatureGuardians;
    while (grds != 0)
    {
        if (ObjectP(grds->Object))
            ValidateRoot(grds->Object, ln);
        if (ObjectP(grds->TConc))
            ValidateRoot(grds->TConc, ln);

        grds = grds->Next;
    }

    grds = YoungGuardians;
    while (grds != 0)
    {
        if (ObjectP(grds->Object))
            ValidateRoot(grds->Object, ln);
        if (ObjectP(grds->TConc))
            ValidateRoot(grds->TConc, ln);

        grds = grds->Next;
    }

    FTracker * trkrs = YoungTrackers;
    while (trkrs != 0)
    {
        if (ObjectP(trkrs->Object))
            ValidateRoot(trkrs->Object, ln);
        if (ObjectP(trkrs->Return))
            ValidateRoot(trkrs->Return, ln);
        if (ObjectP(trkrs->TConc))
            ValidateRoot(trkrs->TConc, ln);

        trkrs = trkrs->Next;
    }
}

static void Collect(int_t fcf)
{
/*    if (fcf)
        printf("Full Collection...");
    else
        printf("Partial Collection...");*/

    if (ValidateHeap)
        ValidateGC(__LINE__);

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
                    obj = ((char *) obj) + ObjectSize(obj, IndirectTag(obj), __LINE__);
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

        for (FAlive * ap = ts->AliveList; ap != 0; ap = ap->Next)
            if (ObjectP(*ap->Pointer))
                ScanObject(ap->Pointer, fcf, 0);

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

        if (ObjectP(ts->NotifyObject))
            ScanObject(&ts->NotifyObject, fcf, 0);

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
                    FAssert(MatureP(*brs->BackRef[idx].Ref) == 0);
                    FAssert(MatureP(brs->BackRef[idx].Ref));

                    ScanObject(brs->BackRef[idx].Ref, fcf, 0);
                    if (MatureP(*brs->BackRef[idx].Ref))
                        brs->BackRef[idx].Value = 0;
                    else
                        brs->BackRef[idx].Value = *brs->BackRef[idx].Ref;
                }
                else
                    brs->BackRef[idx].Value = 0;
            }

            brs = brs->Next;
        }

        FGuardian * grds = MatureGuardians;
        while (grds != 0)
        {
            ScanObject(&grds->Object, fcf, 0);
            ScanObject(&grds->TConc, fcf, 0);

            grds = grds->Next;
        }
    }

    CleanScan(fcf);
    FGuardian * final = CollectGuardians(fcf);

    FTracker * yt = YoungTrackers;
    YoungTrackers = 0;
    FTracker * moved = CollectTrackers(yt, fcf);

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
                            fo->Length = MakeLength(ObjectSize(obj, IndirectTag(obj), __LINE__),
                                    GCFreeTag);
                            fo->Next = FreeMature;
                            FreeMature = fo;
                            pfo = fo;
                        }
                        else
                            pfo->Length = MakeLength(ByteLength(pfo)
                                    + ObjectSize(obj, IndirectTag(obj), __LINE__), GCFreeTag);
                    }
                    else
                        pfo = 0;

                    obj = ((char *) obj) + ObjectSize(obj, IndirectTag(obj), __LINE__);
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

    while (final != 0)
    {
        FGuardian * grd = final;
        final = final->Next;

        TConcAdd(grd->TConc, grd->Object);
        FreeGuardian(grd);
    }

    while (moved != 0)
    {
        FTracker * trkr = moved;
        moved = moved->Next;

        TConcAdd(trkr->TConc, trkr->Return);
        FreeTracker(trkr);
    }

//    if (ValidateHeap)
//        ValidateGC(__LINE__);
//    printf("Done.\n");
}

void FailedGC()
{
    if (GCHappening == 0)
        ValidateGC(__LINE__);
    printf("SectionTable: %p\n", SectionTable);
}

void EnterWait()
{
    if (GetThreadState()->DontWait == 0)
    {
#ifdef FOMENT_STRESSWAIT
        GCRequired = 1;
        Collect();
#endif // FOMENT_STRESSWAIT

        EnterExclusive(&GCExclusive);
        WaitThreads += 1;
        if (GCHappening && TotalThreads == WaitThreads + CollectThreads)
            WakeCondition(&ReadyCondition);
        LeaveExclusive(&GCExclusive);
    }
}

void LeaveWait()
{
    if (GetThreadState()->DontWait == 0)
    {
        EnterExclusive(&GCExclusive);
        WaitThreads -= 1;

        if (GCHappening)
        {
            CollectThreads += 1;

            while (GCHappening)
                ConditionWait(&DoneCondition, &GCExclusive);

            FAssert(CollectThreads > 0);

            CollectThreads -= 1;
        }

        LeaveExclusive(&GCExclusive);
    }
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

        FAssert(CollectThreads > 0);

        CollectThreads -= 1;
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
            else if (BignumP(obj))
                DeleteBignum(obj);
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

        while (GCHappening)
            ConditionWait(&DoneCondition, &GCExclusive);

        FAssert(CollectThreads > 0);

        CollectThreads -= 1;
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
        MatureGuardians = MakeGuardian(MatureGuardians, obj, tconc);
    else
        YoungGuardians = MakeGuardian(YoungGuardians, obj, tconc);

    LeaveExclusive(&GCExclusive);
}

void InstallTracker(FObject obj, FObject ret, FObject tconc)
{
    FAssert(ObjectP(obj));
    FAssert(PairP(tconc));
    FAssert(PairP(First(tconc)));
    FAssert(PairP(Rest(tconc)));

    if (MatureP(obj) == 0)
        YoungTrackers = MakeTracker(YoungTrackers, obj, ret, tconc);
}

FAlive::FAlive(FObject * ptr)
{
    FThreadState * ts = GetThreadState();

    Next = ts->AliveList;
    ts->AliveList = this;
    Pointer = ptr;
}

FAlive::~FAlive()
{
    FThreadState * ts = GetThreadState();

    FAssert(ts->AliveList == this);

    ts->AliveList = Next;
}

FDontWait::FDontWait()
{
    FThreadState * ts = GetThreadState();
    ts->DontWait += 1;
}

FDontWait::~FDontWait()
{
    FThreadState * ts = GetThreadState();

    FAssert(ts->DontWait > 0);

    ts->DontWait -= 1;
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
    ts->AliveList = 0;
    ts->DontWait = 0;
    ts->ActiveZero = 0;
    ts->ObjectsSinceLast = 0;
    ts->UsedRoots = 0;
    ts->StackSize = 1024 * 16;
    ts->AStackPtr = 0;
    ts->AStack = (FObject *) malloc(ts->StackSize * sizeof(FObject));
    ts->CStackPtr = 0;
    ts->CStack = ts->AStack + ts->StackSize - 1;
    ts->Proc = NoValueObject;
    ts->Frame = NoValueObject;
    ts->IP = -1;
    ts->ArgCount = -1;
    ts->DynamicStack = EmptyListObject;
    ts->Parameters = prms;
    ts->NotifyObject = NoValueObject;

    if (VectorP(idxprms))
    {
        FAssert(VectorLength(idxprms) == INDEX_PARAMETERS);

        for (int_t idx = 0; idx < INDEX_PARAMETERS; idx++)
            ts->IndexParameters[idx] = AsVector(idxprms)->Vector[idx];
    }
    else
        for (int_t idx = 0; idx < INDEX_PARAMETERS; idx++)
            ts->IndexParameters[idx] = NoValueObject;

    ts->NotifyFlag = 0;
}

uint_t LeaveThread(FThreadState * ts)
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

    uint_t tt = TotalThreads;

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

    FAssert(ts->AStack != 0);
    free(ts->AStack);

    ts->AStack = 0;
    ts->CStack = 0;
    ts->Thread = NoValueObject;

    if (ts->ActiveZero != 0)
    {
        ts->ActiveZero->Next = GenerationZero;
        GenerationZero = ts->ActiveZero;
        ts->ActiveZero = 0;
    }

    LeaveExclusive(&GCExclusive);
    WakeCondition(&ReadyCondition); // Just in case a collection is pending.

    return(tt);
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
    FAssert(sizeof(FRatio) == sizeof(FTwoSlot));
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

    FAssert(MaximumSections % SECTION_SIZE == 0);
    FAssert(MaximumSections >= SECTION_SIZE);

#ifdef FOMENT_WINDOWS
    SectionTable = (unsigned char *) VirtualAlloc(SectionTableBase, SECTION_SIZE * MaximumSections,
            MEM_RESERVE, PAGE_READWRITE);

    FAssert(SectionTable != 0);

    VirtualAlloc(SectionTable, MaximumSections, MEM_COMMIT, PAGE_READWRITE);

    TlsIndex = TlsAlloc();
    FAssert(TlsIndex != TLS_OUT_OF_INDEXES);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    SectionTable = (unsigned char *) mmap(0, (SECTION_SIZE + 1) * MaximumSections, PROT_NONE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1 ,0);
    FAssert(SectionTable != 0);

    if (SectionTable != SectionBase(SectionTable))
      SectionTable += (SECTION_SIZE - SectionOffset(SectionTable));

    mprotect(SectionTable, MaximumSections, PROT_READ | PROT_WRITE);

    pthread_key_create(&ThreadKey, 0);
#endif // FOMENT_UNIX

    FAssert(SectionTable == SectionBase(SectionTable));

    for (uint_t sdx = 0; sdx < MaximumSections; sdx++)
        SectionTable[sdx] = HoleSectionTag;

    FAssert(SectionIndex(SectionTable) == 0);

    UsedSections = 0;
    while (UsedSections < MaximumSections / SECTION_SIZE)
    {
        SectionTable[UsedSections] = TableSectionTag;
        UsedSections += 1;
    }

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

    for (uint_t idx = 0; idx < sizeof(Sizes) / sizeof(uint_t); idx++)
        Sizes[idx] = 0;

    EnterThread(ts, NoValueObject, NoValueObject, NoValueObject);
    ts->Thread = MakeThread(h, NoValueObject, NoValueObject, NoValueObject);

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
        NonNegativeArgCheck("partial-per-full", argv[0], 0);

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
        NonNegativeArgCheck("trigger-bytes", argv[0], 0);

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
        NonNegativeArgCheck("trigger-objects", argv[0], 0);

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
        case TrackerSectionTag: printf("Tracker\n"); break;
        case GuardianSectionTag: printf("Guardian\n"); break;
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
