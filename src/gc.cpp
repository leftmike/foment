/*

Foment

*/

#include "foment.hpp"

#ifdef FOMENT_WINDOWS
#include <windows.h>
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
#include <pthread.h>
#include <sys/mman.h>
#include <sys/times.h>
#include <unistd.h>
#ifdef FOMENT_OSX
#define MAP_ANONYMOUS MAP_ANON
#endif // FOMENT_OSX
#endif // FOMENT_UNIX

#if defined(FOMENT_BSD) || defined(FOMENT_OSX)
#include <stdlib.h>
#else // FOMENT_BSD
#include <malloc.h>
#endif // FOMENT_BSD
#include <stdio.h>
#include <string.h>
#include "syncthrd.hpp"
#include "io.hpp"

#define GC_PAGE_SIZE 4096

const static ulong_t Align[8] = {0, 7, 6, 5, 4, 3, 2, 1};

FCollectorType CollectorType = MarkSweepCollector;
ulong_t MaximumStackSize = 1024 * 1024 * sizeof(FObject);

#ifdef FOMENT_64BIT
ulong_t MaximumHeapSize = 1024 * 1024 * 1024 * 8LL;
#endif // FOMENT_64BIT
#ifdef FOMENT_32BIT
ulong_t MaximumHeapSize = 1024 * 1024 * 1024;
#endif // FOMENT_32BIT

ulong_t TriggerObjects = 1024 * 16;
ulong_t TriggerBytes = TriggerObjects * (sizeof(FPair) + sizeof(FHeader));

volatile ulong_t BytesAllocated = 0;
volatile long_t GCRequired = 0;
static OSExclusive GCExclusive;
OSExclusive ThreadsExclusive;
static OSCondition ReadyCondition;
static OSCondition DoneCondition;
static volatile ulong_t TotalThreads;
static volatile ulong_t ReadyThreads;
static volatile long_t Collecting;
FThreadState * Threads;

#define FREE_OBJECTS 16
static FMemRegion Objects;
static ulong_t ObjectsUsed;
static FHeader * BigFreeObjects;
static FHeader * FreeObjects[FREE_OBJECTS];

typedef struct
{
    FMemRegion MemRegion;
    ulong_t Used;
} FMemSpace;

#ifdef FOMENT_WINDOWS
unsigned int TlsIndex;
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
pthread_key_t ThreadKey;
static ulong_t ClockTicksPerSecond;
#endif // FOMENT_UNIX

#ifdef FOMENT_32BIT
#define OBJECT_MAXIMUM_SLOTS ((MAXIMUM_ULONG >> 4) / sizeof(FObject))
#define MAXIMUM_TOTAL_SIZE (sizeof(FHeader) + OBJECT_MAXIMUM_SLOTS * sizeof(FObject))
#endif // FOMENT_32BIT
#ifdef FOMENT_64BIT
#define OBJECT_MAXIMUM_SLOTS (MAXIMUM_ULONG >> HEADER_SIZE_SHIFT)
#define MAXIMUM_TOTAL_SIZE (sizeof(FHeader) + OBJECT_MAXIMUM_SLOTS * OBJECT_ALIGNMENT)
#endif // FOMENT_64BIT

typedef struct _Guardian
{
    struct _Guardian * Next;
    FObject Object;
    FObject TConc;
} FGuardian;

static FGuardian * Guardians;

static ulong_t LiveEphemerons;
static FEphemeron ** KeyEphemeronMap;
static ulong_t KeyEphemeronMapSize;

static ulong_t NextParameterIndex = PARAMETERS;

// ---- Roots ----

FObject CleanupTConc = NoValueObject;

// ---- Counts ----

#define SIZE_COUNTS 256
static uint64_t SizeCounts[SIZE_COUNTS];
static uint64_t LargeCount;
static uint64_t TagCounts[FreeTag];

// ---- Roots ----

static FObject * Roots[128];
static ulong_t RootsUsed = 0;
static const char * RootNames[sizeof(Roots) / sizeof(FObject *)];

static inline ulong_t RoundToPageSize(ulong_t cnt)
{
    if (cnt % GC_PAGE_SIZE != 0)
    {
        cnt += GC_PAGE_SIZE - (cnt % GC_PAGE_SIZE);

        FAssert(cnt % GC_PAGE_SIZE == 0);
    }

    return(cnt);
}

typedef struct
{
    uint64_t UserTimeMS;
    uint64_t SystemTimeMS;
} FProcessorTimes;

static int TimesError = 0;
static FProcessorTimes GCTimes;
static FProcessorTimes TotalTimes;

void GetProcessorTimes(FProcessorTimes * ptms)
{
#ifdef FOMENT_UNIX
    struct tms tms;
    if (times(&tms) != (clock_t) -1)
    {
        ptms->UserTimeMS = (tms.tms_utime * 1000) / ClockTicksPerSecond;
        ptms->SystemTimeMS = (tms.tms_stime * 1000) / ClockTicksPerSecond;
    }
    else
        TimesError = 1;
#endif // FOMENT_UNIX

#ifdef FOMENT_WINDOWS
/*
    FILETIME ft, fu, fs;
    GetProcessTimes(GetCurrentProcess(), &ft, &ft, &fs, &fu);
    ptms->UserTimeMS = fu.QuadPart / 10000;
    ptms->SystemTimeMS = fs.QuadPart / 10000;
*/
#endif // FOMENT_WINDOWS
}

void * InitializeMemRegion(FMemRegion * mrgn, ulong_t max)
{
    mrgn->TopUsed = 0;
    mrgn->BottomUsed = 0;
    mrgn->MaximumSize = RoundToPageSize(max);
#ifdef FOMENT_WINDOWS
    mrgn->Base = VirtualAlloc(0, mrgn->MaximumSize, MEM_RESERVE, PAGE_READWRITE);
#endif // FOMENT_WINDOWS
#ifdef FOMENT_UNIX
    mrgn->Base = mmap(0, mrgn->MaximumSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif // FOMENT_UNIX
    return(mrgn->Base);
}

void DeleteMemRegion(FMemRegion * mrgn)
{
    FAssert(mrgn->Base != 0);
    FAssert(mrgn->MaximumSize % GC_PAGE_SIZE == 0);
    FAssert(mrgn->MaximumSize > 0);

#ifdef FOMENT_WINDOWS
    VirtualFree(mrgn->Base, 0, MEM_RELEASE);
#endif // FOMENT_WINDOWS
#ifdef FOMENT_UNIX
    munmap(mrgn->Base, mrgn->MaximumSize);
#endif // FOMENT_UNIX

    mrgn->Base = 0;
}

long_t GrowMemRegionUp(FMemRegion * mrgn, ulong_t sz)
{
    FAssert(mrgn->Base != 0);
    FAssert(mrgn->TopUsed % GC_PAGE_SIZE == 0);
    FAssert(mrgn->BottomUsed % GC_PAGE_SIZE == 0);
    FAssert(mrgn->MaximumSize % GC_PAGE_SIZE == 0);
    FAssert(mrgn->TopUsed <= mrgn->MaximumSize);
    FAssert(mrgn->BottomUsed <= mrgn->MaximumSize);
    FAssert(mrgn->BottomUsed + mrgn->TopUsed <= mrgn->MaximumSize);

    if (sz > mrgn->BottomUsed)
    {
        sz = RoundToPageSize(sz);
        if (sz > mrgn->MaximumSize - mrgn->TopUsed)
            return(0);

#ifdef FOMENT_WINDOWS
        if (VirtualAlloc(((uint8_t *) mrgn->Base) + mrgn->BottomUsed, sz - mrgn->BottomUsed,
                MEM_COMMIT, PAGE_READWRITE) == 0)
            return(0);
#endif // FOMENT_WINDOWS
#ifdef FOMENT_UNIX
        if (mprotect(((uint8_t *) mrgn->Base) + mrgn->BottomUsed, sz - mrgn->BottomUsed,
                PROT_READ | PROT_WRITE) != 0)
            return(0);
#endif // FOMENT_UNIX

        mrgn->BottomUsed = sz;
    }

    return(1);
}

long_t GrowMemRegionDown(FMemRegion * mrgn, ulong_t sz)
{
    FAssert(mrgn->Base != 0);
    FAssert(mrgn->TopUsed % GC_PAGE_SIZE == 0);
    FAssert(mrgn->BottomUsed % GC_PAGE_SIZE == 0);
    FAssert(mrgn->MaximumSize % GC_PAGE_SIZE == 0);
    FAssert(mrgn->TopUsed <= mrgn->MaximumSize);
    FAssert(mrgn->BottomUsed <= mrgn->MaximumSize);
    FAssert(mrgn->BottomUsed + mrgn->TopUsed <= mrgn->MaximumSize);

    if (sz > mrgn->TopUsed)
    {
        sz = RoundToPageSize(sz);
        if (sz > mrgn->MaximumSize - mrgn->BottomUsed)
            return(0);

#ifdef FOMENT_WINDOWS
        if (VirtualAlloc(((uint8_t *) mrgn->Base) + (mrgn->MaximumSize - sz), sz - mrgn->TopUsed,
                MEM_COMMIT, PAGE_READWRITE) == 0)
            return(0);
#endif // FOMENT_WINDOWS
#ifdef FOMENT_UNIX
        if (mprotect(((uint8_t *) mrgn->Base) + (mrgn->MaximumSize - sz), sz - mrgn->TopUsed,
                PROT_READ | PROT_WRITE) != 0)
            return(0);
#endif // FOMENT_UNIX

        mrgn->TopUsed = sz;
    }

    return(1);
}

inline uint32_t Tag(FHeader h)
{
    return(h & HEADER_TAG_MASK);
}

inline FHeader * ObjHeader(FObject obj)
{
    FAssert(ObjectP(obj));

    return(((FHeader *) obj) - 1);
}

inline FObject * Slots(FHeader * oh)
{
    return((FObject *) (oh + 1));
}

// Number of slots (FObjects) which must be at the beginning of the object.
ulong_t SlotCount(FHeader h)
{
    uint32_t tag = h & HEADER_TAG_MASK;

    FAssert(tag > ZeroTag);
    FAssert(tag <= FreeTag);

    ulong_t sc = ObjectTypes[tag].SlotCount;
    if (sc == MAXIMUM_ULONG)
#ifdef FOMENT_64BIT
        return((ulong_t) (h >> HEADER_SIZE_SHIFT));
#endif // FOMENT_64BIT
#ifdef FOMENT_32BIT
        return((ulong_t) (((h >> HEADER_SIZE_SHIFT) * 2) -
                ((h & HEADER_FLAG_PAD) >> HEADER_PAD_SHIFT)));
#endif // FOMENT_32BIT
    return(sc);
}

static inline void SetMark(FHeader * oh)
{
    *oh |= HEADER_FLAG_MARK;
}

static inline void ClearMark(FHeader * oh)
{
    *oh &= ~HEADER_FLAG_MARK;
}

static inline int MarkP(FHeader * oh)
{
    return(*oh & HEADER_FLAG_MARK);
}

static inline int AliveP(FObject obj)
{
    return(MarkP(ObjHeader(obj)));
}

static inline void SetCheckMark(FObject obj)
{
    *ObjHeader(obj) |= HEADER_FLAG_CHECK;
}

static inline void ClearCheckMark(FHeader * oh)
{
    *oh &= ~HEADER_FLAG_CHECK;
}

static inline int CheckMarkP(FObject obj)
{
    return(AsHeader(obj) & HEADER_FLAG_CHECK);
}

static inline void SetEphemeronKeyMark(FObject obj)
{
    *ObjHeader(obj) |= HEADER_FLAG_EPHEMERON_KEY_MARK;
}

static inline void ClearEphemeronKeyMark(FHeader * oh)
{
    *oh &= ~HEADER_FLAG_EPHEMERON_KEY_MARK;
}

static inline int EphemeronKeyMarkP(FHeader * oh)
{
    return(*oh & HEADER_FLAG_EPHEMERON_KEY_MARK);
}

static inline ulong_t TotalSize(FHeader * oh)
{
    return(ObjectSize(*oh) + sizeof(FHeader));
}

inline FHeader InitHeader(uint64_t tsz, FObjectTag tag)
{
    return((((tsz - sizeof(FHeader)) / sizeof(FHeader)) << HEADER_SIZE_SHIFT) | tag);
}

static FHeader * AllocateObject(ulong_t tsz, FObject exc)
{
    ulong_t bkt = tsz / OBJECT_ALIGNMENT;
    FHeader * oh = 0;

    if (bkt < FREE_OBJECTS)
    {
        if (FreeObjects[bkt] != 0)
        {
            oh = FreeObjects[bkt];
            FreeObjects[bkt] = (FHeader *) *Slots(oh);

            FAssert(TotalSize(oh) == tsz);
            FAssert(Tag(*oh) == FreeTag);
            FAssert((*oh & HEADER_FLAG_ETERNAL) == 0);
        }
    }
    else
//    if (oh == 0)
    {
        FHeader * foh = BigFreeObjects;
        FHeader ** pfoh = &BigFreeObjects;
        while (foh != 0)
        {
            ulong_t ftsz = TotalSize(foh);

            FAssert(ftsz % OBJECT_ALIGNMENT == 0);
            FAssert(Tag(*foh) == FreeTag);

            if (ftsz == tsz)
            {
                *pfoh = (FHeader *) *Slots(foh);
                oh = foh;
                break;
            }
            else if (ftsz >= tsz + FREE_OBJECTS * OBJECT_ALIGNMENT)
            {
                FAssert(TotalSize(foh) > tsz);
                FAssert(TotalSize(foh) - tsz >= OBJECT_ALIGNMENT);

                oh = (FHeader *) (((char *) foh) + ftsz - tsz);
                *foh = InitHeader(ftsz - tsz, FreeTag);
                break;
            }

            pfoh = (FHeader **) Slots(foh);
            foh = (FHeader *) *Slots(foh);
        }
    }

    if (oh == 0)
    {
        if (ObjectsUsed + tsz > Objects.BottomUsed)
        {
            if (ObjectsUsed + tsz > Objects.MaximumSize)
            {
                if (ExceptionP(exc) == 0)
                    ErrorExitFoment("error", "heap too small; increase maximum-heap-size");

                LeaveExclusive(&GCExclusive);
                Raise(exc);
            }

            ulong_t gsz = 1024 * 1024;
            if (tsz > gsz)
                gsz = tsz;
            if (gsz > Objects.MaximumSize - Objects.BottomUsed)
                gsz = Objects.MaximumSize - Objects.BottomUsed;
            if (GrowMemRegionUp(&Objects, Objects.BottomUsed + gsz) == 0)
            {
                if (ExceptionP(exc) == 0)
                    ErrorExitFoment("error", "out of memory during setup");

                LeaveExclusive(&GCExclusive);
                Raise(exc);
            }
        }

        oh = (FHeader *) (((char *) Objects.Base) + ObjectsUsed);
        ObjectsUsed += tsz;
    }

    FAssert(oh != 0);

    return(oh);
}

FObject MakeObject(FObjectTag tag, ulong_t sz, ulong_t sc, const char * who)
{
    FAssert(tag > ZeroTag);
    FAssert(tag < FreeTag);
    FAssert(ObjectTypes[tag].SlotCount == MAXIMUM_ULONG || ObjectTypes[tag].SlotCount == sc);

    ulong_t tsz = sz;

    tsz += Align[tsz % OBJECT_ALIGNMENT];
    tsz += sizeof(FHeader);

    FAssert(tsz % OBJECT_ALIGNMENT == 0);
    FAssert(sz >= sizeof(FObject) * sc);

    if (tsz > MAXIMUM_TOTAL_SIZE)
        RaiseException(Restriction, MakeObjectSymbol, NoValueObject, MakeStringC("object too big"),
                EmptyListObject);
    if (sc > OBJECT_MAXIMUM_SLOTS)
        RaiseException(Restriction, MakeObjectSymbol, NoValueObject, MakeStringC("too many slots"),
                EmptyListObject);

    TagCounts[tag] += 1;
    if (tsz / OBJECT_ALIGNMENT < SIZE_COUNTS)
        SizeCounts[tsz / OBJECT_ALIGNMENT] += 1;
    else
        LargeCount += 1;

    EnterExclusive(&GCExclusive);
    FHeader * oh = AllocateObject(tsz, MakeObjectOutOfMemory);
    LeaveExclusive(&GCExclusive);

    FAssert(oh != 0);

    *oh = InitHeader(tsz, tag);
#ifdef FOMENT_32BIT
    if (sc % 2 == 1)
        *oh |= HEADER_FLAG_PAD;
#endif // FOMENT_32BIT

    FThreadState * ts = GetThreadState();
    BytesAllocated += tsz;
    ts->ObjectsSinceLast += 1;
    ts->BytesSinceLast += tsz;

    if (CollectorType != NoCollector &&
            (ts->ObjectsSinceLast > TriggerObjects || ts->BytesSinceLast > TriggerBytes))
        GCRequired = 1;

    FAssert(ObjectSize(*oh) >= sz);
    FAssert(SlotCount(*oh) == sc);
    FAssert(Tag(*oh) == tag);

    return(oh + 1);
}

void RegisterRoot(FObject * root, const char * name)
{
    FAssert(RootsUsed < sizeof(Roots) / sizeof(FObject *));

    Roots[RootsUsed] = root;
    RootNames[RootsUsed] = name;
    RootsUsed += 1;
}

typedef struct
{
    long_t Index;
    long_t Repeat;
    FObject Object;
} FCheckStack;

#define WALK_STACK_SIZE (1024 * 8)
static ulong_t CheckStackPtr;
static FCheckStack CheckStack[WALK_STACK_SIZE];
static const char * CheckFrom;

static const char * WhereFrom(FObject obj, long_t * idx)
{
    const char * from;

    if (ObjectP(obj))
    {
        switch (ObjectTag(obj))
        {
        case BoxTag:
            FMustBe(*idx == 0);
            from = "box";
            *idx = -1;
            break;

        case PairTag:
            if (*idx == 0)
                from = "pair.first";
            else
            {
                FMustBe(*idx == 1);
                from = "pair.rest";
            }
            *idx = -1;
            break;

        case BinaryPortTag:
        case TextualPortTag:
            if (*idx == 0)
                from = "port.name";
            else
            {
                FMustBe(*idx == 1);
                from = "port.object";
            }
            *idx = -1;
            break;

        case ProcedureTag:
            if (*idx == 0)
                from = "procedure.name";
            else if (*idx == 1)
                from = "procedure.filename";
            else if (*idx == 2)
                from = "procedure.line-number";
            else
            {
                FMustBe(*idx == 3);
                from = "procedure.code";
            }
            *idx = -1;
            break;

        case SymbolTag:
            FMustBe(*idx == 0);
            from = "symbol.string";
            *idx = -1;
            break;

        case RecordTypeTag:
            from = "record-type.fields";
            break;

        case RecordTag:
            from = "record.fields";
            break;

        case ThreadTag:
            if (*idx == 0)
                from = "thread.result";
            else if (*idx == 1)
                from = "thread.thunk";
            else if (*idx == 2)
                from = "thread.parameters";
            else
            {
                FMustBe(*idx == 3);
                from = "thread.parameters";
            }
            *idx = -1;
            break;

        case RatioTag:
            if (*idx == 0)
                from = "ratio.numerator";
            else
            {
                FMustBe(*idx == 1);
                from = "ratio.denominator";
            }
            *idx = -1;
            break;

        case ComplexTag:
            if (*idx == 0)
                from = "complex.real";
            else
            {
                FMustBe(*idx == 1);
                from = "complex.imaginary";
            }
            *idx = -1;
            break;

        case HashNodeTag:
            if (*idx == 0)
                from = "hash-node.key";
            else if (*idx == 1)
                from = "hash-node.value";
            else
            {
                FMustBe(*idx = 2);
                from = "hash-node.next";
            }
            *idx = -1;
            break;

        default:
            if (ObjectTag(obj) > ZeroTag && ObjectTag(obj) < FreeTag)
                from = ObjectTypes[ObjectTag(obj)].Name;
            else
                from = "unknown";
            break;
        }
    }
    else
        from = "unknown";

    return(from);
}

static void PrintObjectString(FObject obj)
{
    if (TextualPortP(obj) || BinaryPortP(obj))
        obj = AsGenericPort(obj)->Name;
    else if (ProcedureP(obj))
        obj = AsProcedure(obj)->Name;
    else if (RecordTypeP(obj))
        obj = AsRecordType(obj)->Fields[0];
    else if (GenericRecordP(obj))
    {
        FMustBe(RecordTypeP(AsGenericRecord(obj)->Fields[0]));

        obj = AsRecordType(AsGenericRecord(obj)->Fields[0])->Fields[0];
    }

    if (SymbolP(obj))
        obj = SymbolToString(obj);

    if (StringP(obj))
    {
        printf(" ");

        for (ulong_t idx = 0; idx < StringLength(obj); idx++)
            putc(AsString(obj)->String[idx], stdout);
    }
}

static void PrintCheckStack()
{
    FMustBe(CheckStackPtr > 0);

    const char * from = CheckFrom;
    long_t idx = CheckStack[0].Index;

    for (ulong_t cdx = 0; cdx < CheckStackPtr - 1; cdx++)
    {
        if (idx >= 0)
            printf("%s[" LONG_FMT "]", from, idx);
        else
            printf("%s", from);

        idx = CheckStack[cdx + 1].Index;
        from = WhereFrom(CheckStack[cdx].Object, &idx);
        PrintObjectString(CheckStack[cdx].Object);

        if (CheckStack[cdx].Repeat > 1)
            printf(" (repeats " LONG_FMT " times)", CheckStack[cdx].Repeat);
        printf("\n");
    }

    FObject obj = CheckStack[CheckStackPtr - 1].Object;
    if (ObjectP(obj))
    {
        if (ObjectTag(obj) > ZeroTag && ObjectTag(obj) <= FreeTag)
        {
            printf("%s: %p", ObjectTypes[ObjectTag(obj)].Name, obj);
            PrintObjectString(obj);
        }
        else
            printf("unknown: %p tag: %x", obj, ObjectTag(obj));
    }
    else
        printf("unknown: %p", obj);
    printf("\n");
}

static long_t CheckFailedCount;
static long_t CheckCount;
static long_t CheckTooDeep;

static void FCheckFailed(const char * fn, long_t ln, const char * expr, FHeader * oh)
{
    ulong_t idx;

    CheckFailedCount += 1;
    if (CheckFailedCount > 10)
        return;

    printf("\nFCheck: %s (%d)%s\n", expr, (int) ln, fn);

    ulong_t tsz = TotalSize(oh);
    const char * tag = "unknown";
    if (Tag(*oh) > ZeroTag && Tag(*oh) <= FreeTag)
        tag = ObjectTypes[Tag(*oh)].Name;
    printf("tsz: " ULONG_FMT " osz: " ULONG_FMT " slots: " ULONG_FMT " tag: %s",
            tsz, ObjectSize(*oh), SlotCount(*oh), tag);
    if (MarkP(oh))
        printf(" mark");
    printf(" |");
    for (idx = 0; idx < tsz && idx < 64; idx++)
        printf(" %x", ((uint8_t *) oh)[idx]);
    if (idx < tsz)
        printf(" ... (" ULONG_FMT " more)", tsz - idx);
    printf("\n");

    if (CheckStackPtr > 0)
        PrintCheckStack();
}

#define FCheck(expr, oh)\
    if (! (expr)) FCheckFailed(__FILE__, __LINE__, #expr, oh)

static long_t ValidAddress(FHeader * oh)
{
    if (*oh & HEADER_FLAG_ETERNAL)
        return(1);

    ulong_t tsz = TotalSize(oh);
    void * strt = oh;
    void * end = ((char *) oh) + tsz;

    if (Objects.Base != 0 && strt >= Objects.Base && strt < ((char *) Objects.Base) + ObjectsUsed
            && end <= ((char *) Objects.Base) + ObjectsUsed)
        return(1);

    return(0);
}

static void CheckObject(FObject obj, long_t idx, long_t ef)
{
    if (obj == 0)
    {
        printf("CheckObject: obj == 0\n");
        if (CheckStackPtr > 0)
            PrintCheckStack();
        return;
    }

    if (CheckStackPtr == WALK_STACK_SIZE)
    {
        CheckTooDeep += 1;
        return;
    }

    CheckStack[CheckStackPtr].Index = idx;
    CheckStack[CheckStackPtr].Repeat = 1;
    CheckStack[CheckStackPtr].Object = obj;
    CheckStackPtr += 1;

Again:
    if (ObjectP(obj))
    {
        FCheck(ef == 0 || EternalP(obj), ObjHeader(obj));

        if (CheckMarkP(obj))
            goto Done;
        SetCheckMark(obj);
        CheckCount += 1;

        FHeader * oh = ObjHeader(obj);
        FCheck(CheckMarkP(obj), oh);
        FCheck(ValidAddress(oh), oh);
        FCheck(ObjectTag(obj) > ZeroTag && ObjectTag(obj) < FreeTag, oh);
        FCheck(ObjectSize(*oh) >= SlotCount(*oh) * sizeof(FObject), oh);

        if (PairP(obj))
        {
            CheckObject(AsPair(obj)->First, 0, ef);

            FMustBe(CheckStackPtr > 0);

            if (CheckStackPtr > 1 && PairP(CheckStack[CheckStackPtr - 2].Object)
                    && CheckStack[CheckStackPtr - 2].Index == 1)
            {
                CheckStack[CheckStackPtr - 1].Repeat += 1;
                obj = AsPair(obj)->Rest;
                goto Again;
            }
            else
                CheckObject(AsPair(obj)->Rest, 1, ef);
        }
        else if (EphemeronP(obj))
        {
            CheckObject(AsEphemeron(obj)->Key, 0, ef);
            CheckObject(AsEphemeron(obj)->Datum, 1, ef);
            CheckObject(AsEphemeron(obj)->HashTable, 2, ef);
        }
        else if (SlotCount(AsHeader(obj)) > 0)
        {
            for (ulong_t idx = 0; idx < SlotCount(AsHeader(obj)); idx++)
                CheckObject(((FObject *) obj)[idx], idx, ef);
        }
    }

Done:
    FMustBe(CheckStackPtr > 0);
    CheckStackPtr -= 1;
}

static void CheckRoot(FObject obj, const char * from, long_t idx)
{
    CheckFrom = from;
    CheckObject(obj, idx, 0);
}

static void CheckThreadState(FThreadState * ts)
{
    CheckRoot(ts->Thread, "thread-state.thread", -1);

    ulong_t idx = 0;
    for (FAlive * ap = ts->AliveList; ap != 0; ap = ap->Next, idx++)
        CheckRoot(*ap->Pointer, "thread-state.alive-list", idx);

    for (long_t adx = 0; adx < ts->AStackPtr; adx++)
        CheckRoot(ts->AStack[adx], "thread-state.astack", adx);

    for (long_t cdx = 0; cdx < ts->CStackPtr; cdx++)
        CheckRoot(ts->CStack[- cdx], "thread-state.cstack", cdx);

    CheckRoot(ts->Proc, "thread-state.proc", -1);
    CheckRoot(ts->Frame, "thread-state.frame", -1);
    CheckRoot(ts->DynamicStack, "thread-state.dynamic-stack", -1);

    for (ulong_t idx = 0; idx < ts->ParametersLength; idx++)
        CheckRoot(ts->Parameters[idx], "thread-state.parameters", idx);

    CheckRoot(ts->NotifyObject, "thread-state.notify-object", -1);
}

static void CheckMemRegion(FMemRegion * mrgn, ulong_t used)
{
    FHeader * oh = (FHeader *) mrgn->Base;
    ulong_t cnt = 0;

    while (cnt < used)
    {
        FCheck(cnt + sizeof(FHeader) <= mrgn->BottomUsed, oh);
        ulong_t osz = ObjectSize(*oh);
        ulong_t tsz = TotalSize(oh);

        FCheck(tsz >= osz + sizeof(FHeader), oh);
        FCheck(tsz % OBJECT_ALIGNMENT == 0, oh);
        FCheck(cnt + tsz <= mrgn->BottomUsed, oh);
        FCheck(SlotCount(*oh) * sizeof(FObject) <= ObjectSize(*oh), oh);
        FCheck(Tag(*oh) > ZeroTag && Tag(*oh) <= FreeTag, oh);

        ClearCheckMark(oh);
        oh = (FHeader *) (((char *) oh) + tsz);
        cnt += tsz;
    }
}

void CheckHeap(const char * fn, int ln)
{
    EnterExclusive(&GCExclusive);

    if (VerboseFlag)
        printf("CheckHeap: %s(%d)\n", fn, ln);

    CheckFailedCount = 0;
    CheckCount = 0;
    CheckTooDeep = 0;
    CheckStackPtr = 0;

    if (Objects.Base != 0)
        CheckMemRegion(&Objects, ObjectsUsed);

    for (ulong_t rdx = 0; rdx < RootsUsed; rdx++)
        CheckRoot(*Roots[rdx], RootNames[rdx], -1);

    FThreadState * ts = Threads;
    while (ts != 0)
    {
        CheckThreadState(ts);
        ts = ts->Next;
    }

    FGuardian * grd = Guardians;
    while (grd)
    {
        CheckRoot(grd->Object, "guardian.object", -1);
        CheckRoot(grd->TConc, "guardian.tconc", -1);

        grd = grd->Next;
    }

    if (CheckTooDeep > 0)
        printf("CheckHeap: %d object too deep to walk\n", (int) CheckTooDeep);
    if (VerboseFlag)
        printf("CheckHeap: %d active objects\n", (int) CheckCount);

    if (CheckFailedCount > 0)
        printf("CheckHeap: %s(%d)\n", fn, ln);
    FMustBe(CheckFailedCount == 0);
    LeaveExclusive(&GCExclusive);
}

static void ScanObject(FObject obj);
static inline void LiveObject(FObject obj)
{
    if (ObjectP(obj))
        ScanObject(obj);
}

static void ScanObject(FObject obj)
{
Again:
    FAssert(ObjectP(obj));

    FHeader * oh = ObjHeader(obj);

    if (EphemeronKeyMarkP(oh))
    {
        FAssert(MarkP(oh) == 0);

        ClearEphemeronKeyMark(oh);

        FObject key = (FObject) (oh + 1);
        ulong_t idx = EqHash(key) % KeyEphemeronMapSize;
        FEphemeron * eph = KeyEphemeronMap[idx];
        FEphemeron ** peph = &KeyEphemeronMap[idx];
        while (eph != 0)
        {
            if (eph->Key == key)
            {
                LiveEphemerons += 1;
                LiveObject(eph->Key);
                LiveObject(eph->Datum);
                LiveObject(eph->HashTable);

                *peph = eph->Next;
                eph->Next = 0;
                eph = *peph;
            }
            else
            {
                peph = &eph->Next;
                eph = eph->Next;
            }
        }
    }

    if ((*oh & HEADER_FLAG_ETERNAL) == 0)
    {
        if (MarkP(oh))
            return;

        SetMark(oh);
    }

    if (Tag(*oh) != EphemeronTag)
    {
        ulong_t sc = SlotCount(*oh);
        if (sc > 0)
        {
            ulong_t sdx = 0;
            while (sdx < sc - 1)
            {
                LiveObject(Slots(oh)[sdx]);
                sdx += 1;
            }

            if (ObjectP(Slots(oh)[sdx]))
            {
                obj = Slots(oh)[sdx];
                goto Again;
            }
        }
    }
    else
    {
        FEphemeron * eph = (FEphemeron *) (oh + 1);

        FAssert(SlotCount(*oh) == 0);

        if (eph->Next != EPHEMERON_BROKEN)
        {
            if (KeyEphemeronMap == 0 || ObjectP(eph->Key) == 0 || AliveP(eph->Key))
            {
                LiveEphemerons += 1;
                LiveObject(eph->Key);
                LiveObject(eph->Datum);
                LiveObject(eph->HashTable);
            }
            else
            {
                FAssert(ObjectP(eph->Key));

                ulong_t idx = EqHash(eph->Key) % KeyEphemeronMapSize;
                eph->Next = KeyEphemeronMap[idx];
                KeyEphemeronMap[idx] = eph;

                if (EphemeronKeyMarkP(ObjHeader(eph->Key)) == 0)
                    SetEphemeronKeyMark(eph->Key);
            }
        }
        else
        {
            FAssert(eph->Key == FalseObject);
            FAssert(eph->Datum == FalseObject);
            FAssert(eph->HashTable == NoValueObject);
        }
    }
}

static FGuardian * CollectGuardians()
{
    FGuardian * final = 0;
    FGuardian * maybe = Guardians;

    Guardians = 0;
    for (;;)
    {
        FGuardian * mlst = maybe;
        maybe = 0;
        FGuardian * pending = 0;

        while (mlst)
        {
            FGuardian * grd = mlst;
            mlst = mlst->Next;

            if (AliveP(grd->TConc))
            {
                if (AliveP(grd->Object))
                {
                    grd->Next = Guardians;
                    Guardians = grd;

                    LiveObject(grd->Object);
                    LiveObject(grd->TConc);
                }
                else
                {
                    grd->Next = pending;
                    pending = grd;
                }
            }
            else
            {
                grd->Next = maybe;
                maybe = grd;
            }
        }

        if (pending == 0)
            break;

        while (pending)
        {
            FGuardian * grd = pending;
            pending = pending->Next;

            FAssert(AliveP(grd->TConc));

            grd->Next = final;
            final = grd;
            LiveObject(grd->Object);
            LiveObject(grd->TConc);
        }
    }

    while (maybe)
    {
        FAssert(AliveP(maybe->Object) == 0);
        FAssert(AliveP(maybe->TConc) == 0);

        FGuardian * grd = maybe;
        maybe = maybe->Next;
        free(grd);
    }

    return(final);
}

static void Collect()
{
    FAssert(CollectorType != NoCollector);

    if (VerboseFlag)
        printf("Garbage Collection...\n");

    FProcessorTimes tstart;
    if (TimesError == 0)
        GetProcessorTimes(&tstart);

    if (CheckHeapFlag)
        CheckHeap(__FILE__, __LINE__);

    FHeader * oh = (FHeader *) Objects.Base;
    while ((char *) oh < ((char *) Objects.Base) + ObjectsUsed)
    {
        ClearMark(oh);
        oh = (FHeader *) (((char *) oh) + TotalSize(oh));
    }

    FAssert(KeyEphemeronMap == 0);

    KeyEphemeronMapSize = LiveEphemerons;
    KeyEphemeronMap = (FEphemeron **) malloc(sizeof(FEphemeron *) * KeyEphemeronMapSize);
    if (KeyEphemeronMap != 0)
        memset(KeyEphemeronMap, 0, sizeof(FEphemeron *) * KeyEphemeronMapSize);
    LiveEphemerons = 0;

    for (ulong_t rdx = 0; rdx < RootsUsed; rdx++)
        LiveObject(*Roots[rdx]);

    FThreadState * ts = Threads;
    while (ts != 0)
    {
        LiveObject(ts->Thread);

        for (FAlive * ap = ts->AliveList; ap != 0; ap = ap->Next)
            LiveObject(*(ap->Pointer));

        for (long_t adx = 0; adx < ts->AStackPtr; adx++)
            LiveObject(ts->AStack[adx]);

        for (long_t cdx = 0; cdx < ts->CStackPtr; cdx++)
            LiveObject(*(ts->CStack - cdx));

        LiveObject(ts->Proc);
        LiveObject(ts->Frame);
        LiveObject(ts->DynamicStack);

        for (ulong_t idx = 0; idx < ts->ParametersLength; idx++)
            LiveObject(ts->Parameters[idx]);

        LiveObject(ts->NotifyObject);

        ts->ObjectsSinceLast = 0;
        ts->BytesSinceLast = 0;
        ts = ts->Next;
    }

    FGuardian * final = CollectGuardians();

    BigFreeObjects = 0;
    for (long_t idx = 0; idx < FREE_OBJECTS; idx++)
        FreeObjects[idx] = 0;

    oh = (FHeader *) Objects.Base;
    while ((char *) oh < ((char *) Objects.Base) + ObjectsUsed)
    {
        ulong_t tsz = TotalSize(oh);
        if (MarkP(oh) == 0)
        {
            FHeader * noh = (FHeader *) (((char *) oh) + tsz);
            while ((char *) noh < ((char *) Objects.Base) + ObjectsUsed)
            {
                if (MarkP(noh)
                    || ((char *) noh - (char *) oh) + TotalSize(noh) > MAXIMUM_TOTAL_SIZE)
                    break;
                noh = (FHeader *) (((char *) noh) + TotalSize(noh));
            }

            FAssert((ulong_t) ((char *) noh - (char *) oh) >= tsz);

            tsz = (char *) noh - (char *) oh;

            if (ZeroHeapFlag)
                memset(oh + 1, 0, tsz - sizeof(FHeader));

            if (tsz > OBJECT_ALIGNMENT)
            {
                ulong_t bkt = tsz / OBJECT_ALIGNMENT;
                if (bkt < FREE_OBJECTS)
                {
                    *Slots(oh) = FreeObjects[bkt];
                    FreeObjects[bkt] = oh;
                }
                else
                {
                    *Slots(oh) = BigFreeObjects;
                    BigFreeObjects = oh;
                }

                *oh = InitHeader(tsz, FreeTag);
            }
        }

        oh = (FHeader *) (((char *) oh) + tsz);
    }

    while (final != 0)
    {
        FGuardian * grd = final;
        final = final->Next;

        TConcAdd(grd->TConc, grd->Object);
        free(grd);
    }

    if (KeyEphemeronMap != 0)
    {
        for (ulong_t idx = 0; idx < KeyEphemeronMapSize; idx++)
        {
            FEphemeron * eph = KeyEphemeronMap[idx];
            while (eph != 0)
            {
                eph->Key = FalseObject;
                eph->Datum = FalseObject;

                if (HashTableP(eph->HashTable))
                    HashTableEphemeronBroken(eph->HashTable);
                eph->HashTable = NoValueObject;

                FEphemeron * neph = eph->Next;
                eph->Next = EPHEMERON_BROKEN;
                eph = neph;
            }
        }

        free(KeyEphemeronMap);
        KeyEphemeronMap = 0;
    }

    while (TConcEmptyP(CleanupTConc) == 0)
    {
        FObject obj = TConcRemove(CleanupTConc);

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
        else if (ThreadP(obj))
            DeleteThread(obj);
        else
        {
            FAssert(0);
        }
    }

    FProcessorTimes tend;
    GetProcessorTimes(&tend);
    if (TimesError == 0)
    {
        GCTimes.UserTimeMS += (tend.UserTimeMS - tstart.UserTimeMS);
        GCTimes.SystemTimeMS += (tend.SystemTimeMS - tstart.SystemTimeMS);
    }

    if (VerboseFlag)
        printf("Collection Done\n");

    if (CheckHeapFlag)
        CheckHeap(__FILE__, __LINE__);
}

void EnterWait()
{
    EnterExclusive(&ThreadsExclusive);
    ReadyThreads += 1;
    if (Collecting && ReadyThreads == TotalThreads)
        WakeCondition(&ReadyCondition);
    LeaveExclusive(&ThreadsExclusive);
}

void LeaveWait()
{
    EnterExclusive(&ThreadsExclusive);
    while (Collecting)
        ConditionWait(&DoneCondition, &ThreadsExclusive);

    FAssert(ReadyThreads > 0);
    ReadyThreads -= 1;
    LeaveExclusive(&ThreadsExclusive);
}

void ReadyForGC()
{
    EnterExclusive(&ThreadsExclusive);
    if (Collecting)
    {
        ReadyThreads += 1;
        if (ReadyThreads == TotalThreads)
            WakeCondition(&ReadyCondition);

        while (Collecting)
            ConditionWait(&DoneCondition, &ThreadsExclusive);

        FAssert(ReadyThreads > 0);
        ReadyThreads -= 1;
        LeaveExclusive(&ThreadsExclusive);
    }
    else
    {
        Collecting = 1;
        ReadyThreads += 1;

        while (ReadyThreads < TotalThreads)
            ConditionWait(&ReadyCondition, &ThreadsExclusive);

        FAssert(ReadyThreads == TotalThreads);

        GCRequired = 0;
        Collect();

        FAssert(ReadyThreads > 0);
        ReadyThreads -= 1;
        Collecting = 0;
        LeaveExclusive(&ThreadsExclusive);

        WakeAllCondition(&DoneCondition);
    }
}

void InstallGuardian(FObject obj, FObject tconc)
{
    if (CollectorType != NoCollector)
    {
        FAssert(ObjectP(obj));
        FAssert(PairP(tconc));
        FAssert(PairP(First(tconc)));
        FAssert(PairP(Rest(tconc)));

        FGuardian * grd = (FGuardian *) malloc(sizeof(FGuardian));
        if (grd == 0)
            RaiseExceptionC(Assertion, "install-guardian", "out of memory", EmptyListObject);

        grd->Object = obj;
        grd->TConc = tconc;

        EnterExclusive(&GCExclusive);
        grd->Next = Guardians;
        Guardians = grd;
        LeaveExclusive(&GCExclusive);
    }
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

#ifdef FOMENT_DEBUG
static ulong_t CountThreads()
{
    FThreadState * ts = Threads;
    ulong_t cnt = 0;

    while (ts != 0)
    {
        cnt += 1;
        ts = ts->Next;
    }

    return(cnt);
}
#endif // FOMENT_DEBUG

long_t EnterThread(FThreadState * ts, FObject thrd, FObject prms)
{
    memset(ts, 0, sizeof(FThreadState));

#ifdef FOMENT_WINDOWS
    FAssert(TlsGetValue(TlsIndex) == 0);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    FAssert(pthread_getspecific(ThreadKey) == 0);
#endif // FOMENT_UNIX

    SetThreadState(ts);

    ts->Thread = thrd;
    ts->AliveList = 0;
    ts->ObjectsSinceLast = 0;
    ts->BytesSinceLast = 0;

    if (InitializeMemRegion(&ts->Stack, MaximumStackSize) == 0)
        goto Failed;

    if (GrowMemRegionUp(&ts->Stack, GC_PAGE_SIZE) == 0
            || GrowMemRegionDown(&ts->Stack, GC_PAGE_SIZE) == 0)
        goto Failed;

    ts->AStackPtr = 0;
    ts->AStack = (FObject *) ts->Stack.Base;
    ts->CStackPtr = 0;
    ts->CStack = ts->AStack + (ts->Stack.MaximumSize / sizeof(FObject)) - 1;
    ts->Proc = NoValueObject;
    ts->Frame = NoValueObject;
    ts->IP = -1;
    ts->ArgCount = -1;
    ts->DynamicStack = EmptyListObject;
    ts->NotifyObject = NoValueObject;

    ts->ParametersLength = 64;

    FAssert(ts->ParametersLength >= PARAMETERS);

    ts->Parameters = (FObject *) malloc(sizeof(FObject) * ts->ParametersLength);
    if (ts->Parameters == 0)
        goto Failed;

    for (ulong_t idx = 0; idx < ts->ParametersLength; idx++)
        ts->Parameters[idx] = NoValueObject;

    if (VectorP(prms))
    {
        FAssert(VectorLength(prms) <= ts->ParametersLength);

        for (ulong_t idx = 0; idx < VectorLength(prms); idx++)
            ts->Parameters[idx] = AsVector(prms)->Vector[idx];
    }

    ts->NotifyFlag = 0;
    ts->ExceptionCount = 0;
    ts->NestedExecute = 0;

    EnterExclusive(&ThreadsExclusive);

    if (Threads == 0)
        ts->Next = 0;
    else
    {
        ts->Next = Threads;
        Threads->Previous = ts;
    }

    ts->Previous = 0;
    Threads = ts;
    TotalThreads += 1;

    FAssert(TotalThreads == CountThreads());

    LeaveExclusive(&ThreadsExclusive);
    return(1);

Failed:
    if (ts->Stack.Base != 0)
        DeleteMemRegion(&ts->Stack);
    if (ts->Parameters != 0)
        free(ts->Parameters);
    return(0);
}

ulong_t LeaveThread(FThreadState * ts)
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

    EnterExclusive(&ThreadsExclusive);
    FAssert(TotalThreads > 0);
    TotalThreads -= 1;

    ulong_t tt = TotalThreads;

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

    FAssert(TotalThreads == CountThreads());

    FAssert(ts->Stack.Base != 0);
    DeleteMemRegion(&ts->Stack);

    ts->AStack = 0;
    ts->CStack = 0;
    ts->Thread = NoValueObject;

    if (Collecting && ReadyThreads == TotalThreads)
        WakeCondition(&ReadyCondition); // Just in case a collection is pending.
    LeaveExclusive(&ThreadsExclusive);

    if (ts->Parameters != 0)
        free(ts->Parameters);

    return(tt);
}

long_t SetupCore(FThreadState * ts)
{
#if FOMENT_LITTLE_ENDIAN
    FAssert(LittleEndianP());
#else // FOMENT_LITTLE_ENDIAN
    FAssert(LittleEndianP() == 0);
#endif // FOMENT_LITTLE_ENDIAN
    FAssert(sizeof(FObject) == sizeof(long_t));
    FAssert(sizeof(FObject) == sizeof(ulong_t));
    FAssert(sizeof(FObject) == sizeof(char *));
    FAssert(sizeof(FCh) <= sizeof(ulong_t));
    FAssert(sizeof(FHeader) == OBJECT_ALIGNMENT);
    FAssert(FreeTag <= HEADER_TAG_MASK + 1);
    FAssert(sizeof(FObject) <= OBJECT_ALIGNMENT);
    FAssert(sizeof(FCString) % OBJECT_ALIGNMENT == 0);
    FAssert(sizeof(FSymbol) % OBJECT_ALIGNMENT == 0);
    FAssert(sizeof(FPrimitive) % OBJECT_ALIGNMENT == 0);
    FAssert(Objects.Base == 0);
    FAssert(ObjectsUsed == 0);

#ifdef FOMENT_DEBUG
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

    FAssert(BigFreeObjects == 0);

    if (InitializeMemRegion(&Objects, MaximumHeapSize) == 0)
        return(0);
    if (GrowMemRegionUp(&Objects, GC_PAGE_SIZE * 8) == 0)
        return(0);

#ifdef FOMENT_DEBUG
    for (ulong_t idx = 0; idx < FREE_OBJECTS; idx++)
        FAssert(FreeObjects[idx] == 0);
#endif // FOMENT_DEBUG

#ifdef FOMENT_WINDOWS
    TlsIndex = TlsAlloc();
    FAssert(TlsIndex != TLS_OUT_OF_INDEXES);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    pthread_key_create(&ThreadKey, 0);
    ClockTicksPerSecond = sysconf(_SC_CLK_TCK);
#endif // FOMENT_UNIX

    InitializeExclusive(&GCExclusive);
    InitializeExclusive(&ThreadsExclusive);
    InitializeCondition(&ReadyCondition);
    InitializeCondition(&DoneCondition);

#ifdef FOMENT_WINDOWS
    HANDLE h = OpenThread(STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | 0x3FF, 0,
            GetCurrentThreadId());
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    pthread_t h = pthread_self();
#endif // FOMENT_UNIX

    TotalThreads = 0;
    if (EnterThread(ts, NoValueObject, NoValueObject) == 0)
        return(0);
    ts->Thread = MakeThread(h, NoValueObject);

    RegisterRoot(&CleanupTConc, "cleanup-tconc");
    CleanupTConc = MakeTConc();

    if (CheckHeapFlag)
        CheckHeap(__FILE__, __LINE__);

    return(1);
}

Define("install-guardian", InstallGuardianPrimitive)(long_t argc, FObject argv[])
{
    // (install-guardian <obj> <tconc>)

    TwoArgsCheck("install-guardian", argc);
    TConcArgCheck("install-guardian", argv[1]);

    if (ObjectP(argv[0]))
        InstallGuardian(argv[0], argv[1]);

    return(NoValueObject);
}

Define("collect", CollectPrimitive)(long_t argc, FObject argv[])
{
    // (collect)

    ZeroArgsCheck("collect", argc);

    if (CollectorType != NoCollector)
        GCRequired = 1;
    return(NoValueObject);
}

FObject MakeEphemeron(FObject key, FObject dat, FObject htbl)
{
    FAssert(htbl == NoValueObject || HashTableP(htbl));

    // Note that ephemerons are treated specially by the garbage collector and they are
    // allocated as having no slots.
    FEphemeron * eph =
            (FEphemeron *) MakeObject(EphemeronTag, sizeof(FEphemeron), 0, "make-ephemeron");
    eph->Key = key;
    eph->Datum = dat;
    eph->HashTable = htbl;
    eph->Next = 0;

    FAssert(EphemeronBrokenP(eph) == 0);

    EnterExclusive(&GCExclusive);
    LiveEphemerons += 1;
    LeaveExclusive(&GCExclusive);

    return(eph);
}

void EphemeronKeySet(FObject eph, FObject key)
{
    FAssert(EphemeronP(eph));

    if (AsEphemeron(eph)->Next != EPHEMERON_BROKEN)
        AsEphemeron(eph)->Key = key;
}

void EphemeronDatumSet(FObject eph, FObject dat)
{
    FAssert(EphemeronP(eph));

    if (AsEphemeron(eph)->Next != EPHEMERON_BROKEN)
        AsEphemeron(eph)->Datum = dat;
}

void WriteEphemeron(FWriteContext * wctx, FObject obj)
{
    FCh s[16];
    long_t sl = FixnumAsString((long_t) obj, s, 16);

    wctx->WriteStringC("#<ephemeron: ");
    wctx->WriteString(s, sl);
    if (EphemeronBrokenP(obj))
        wctx->WriteStringC(" (broken)");
    wctx->WriteCh('>');
}

Define("ephemeron?", EphemeronPPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("ephemeron?", argc);

    return(EphemeronP(argv[0]) ? TrueObject : FalseObject);
}

Define("make-ephemeron", MakeEphemeronPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("make-ephemeron", argc);

    return(MakeEphemeron(argv[0], argv[1], NoValueObject));
}

Define("ephemeron-broken?", EphemeronBrokenPPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("ephemeron-broken?", argc);
    EphemeronArgCheck("ephemeron-broken?", argv[0]);

    return(EphemeronBrokenP(argv[0]) ? TrueObject : FalseObject);
}

Define("ephemeron-key", EphemeronKeyPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("ephemeron-key", argc);
    EphemeronArgCheck("ephemeron-key", argv[0]);

    return(AsEphemeron(argv[0])->Key);
}

Define("ephemeron-datum", EphemeronDatumPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("ephemeron-datum", argc);
    EphemeronArgCheck("ephemeron-datum", argv[0]);

    return(AsEphemeron(argv[0])->Datum);
}

Define("set-ephemeron-key!", SetEphemeronKeyPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("set-ephemeron-key!", argc);
    EphemeronArgCheck("set-ephemeron-key!", argv[0]);

    EphemeronKeySet(argv[0], argv[1]);
    return(NoValueObject);
}

Define("set-ephemeron-datum!", SetEphemeronDatumPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("set-ephemeron-datum!", argc);
    EphemeronArgCheck("set-ephemeron-datum!", argv[0]);

    EphemeronDatumSet(argv[0], argv[1]);
    return(NoValueObject);
}

Define("process-times", ProcessTimesPrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("process-times", argc);

    FProcessorTimes tnow;
    GetProcessorTimes(&tnow);
    if (TimesError != 0)
        return(EmptyListObject);

    return(List(MakeIntegerFromUInt64(tnow.UserTimeMS - TotalTimes.UserTimeMS),
            MakeIntegerFromUInt64(tnow.SystemTimeMS - TotalTimes.SystemTimeMS),
            MakeIntegerFromUInt64(GCTimes.UserTimeMS),
            MakeIntegerFromUInt64(GCTimes.SystemTimeMS)));
}

Define("process-times-reset!", ProcessTimesResetPrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("process-times-reset!", argc);

    TimesError = 0;
    GetProcessorTimes(&TotalTimes);
    return(NoValueObject);
}

Define("object-counts", ObjectCountsPrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("object-counts", argc);

    FObject lst = EmptyListObject;

    for (int tdx = 1; tdx < FreeTag; tdx++)
        if (TagCounts[tdx] > 0)
            lst = MakePair(MakePair(StringCToSymbol(ObjectTypes[tdx].Name),
                    MakeIntegerFromUInt64(TagCounts[tdx])), lst);

    for (int sdx = 0; sdx < SIZE_COUNTS; sdx++)
        if (SizeCounts[sdx] > 0)
            lst = MakePair(MakePair(MakeFixnum(sdx * OBJECT_ALIGNMENT),
                    MakeIntegerFromUInt64(SizeCounts[sdx])), lst);

    if (LargeCount > 0)
        lst = MakePair(MakePair(StringCToSymbol("large-size"),
                MakeIntegerFromUInt64(LargeCount)), lst);

    return(ReverseListModify(lst));
}

Define("object-counts-reset!", ObjectCountsResetPrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("object-counts-reset!", argc);

    memset(SizeCounts, 0, sizeof(SizeCounts));
    LargeCount = 0;
    memset(TagCounts, 0, sizeof(TagCounts));
    return(NoValueObject);
}

Define("stack-used", StackUsedPrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("stack-used", argc);

    FThreadState * ts = GetThreadState();
    long_t used = 0;
    if (ts->AStackUsed > ts->AStackPtr)
        used += (ts->AStackUsed - ts->AStackPtr);
    if (ts->CStackUsed > ts->CStackPtr)
        used += (ts->CStackUsed - ts->CStackPtr);
    return(MakeFixnum(used));
}

Define("stack-used-reset!", StackUsedResetPrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("stack-used-reset!", argc);

    FThreadState * ts = GetThreadState();
    ts->AStackUsed = ts->AStackPtr;
    ts->CStackUsed = ts->CStackPtr;
    return(NoValueObject);
}

Define("%next-parameter-index", NextParameterIndexPrimitive)(long_t argc, FObject argv[])
{
    FMustBe(argc == 0);

    EnterExclusive(&ThreadsExclusive);
    FObject obj = MakeFixnum(NextParameterIndex);
    NextParameterIndex += 1;
    LeaveExclusive(&ThreadsExclusive);
    return(obj);
}

static FObject Primitives[] =
{
    InstallGuardianPrimitive,
    CollectPrimitive,
    EphemeronPPrimitive,
    MakeEphemeronPrimitive,
    EphemeronBrokenPPrimitive,
    EphemeronKeyPrimitive,
    EphemeronDatumPrimitive,
    SetEphemeronKeyPrimitive,
    SetEphemeronDatumPrimitive,
    ProcessTimesPrimitive,
    ProcessTimesResetPrimitive,
    ObjectCountsPrimitive,
    ObjectCountsResetPrimitive,
    StackUsedPrimitive,
    StackUsedResetPrimitive,
    NextParameterIndexPrimitive
};

void SetupGC()
{
    for (ulong_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(Bedrock, BedrockLibrary, Primitives[idx]);
}
