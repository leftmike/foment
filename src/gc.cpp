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
#ifdef FOMENT_OSX
#define MAP_ANONYMOUS MAP_ANON
#endif // __APPLE__
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
#include "numbers.hpp"

#define GC_PAGE_SIZE 4096

#define OBJECT_ALIGNMENT 8
const static uint_t Align[8] = {0, 7, 6, 5, 4, 3, 2, 1};

FCollectorType CollectorType = MarkSweepCollector;
uint_t MaximumStackSize = 1024 * 1024 * 4 * sizeof(FObject);
uint_t MaximumBabiesSize = 0;
uint_t MaximumKidsSize = 0;
uint_t MaximumGenerationalBaby = 1024 * 64;

#ifdef FOMENT_64BIT
uint_t MaximumAdultsSize = 1024 * 1024 * 1024 * 4LL;
#endif // FOMENT_64BIT
#ifdef FOMENT_32BIT
uint_t MaximumAdultsSize = 1024 * 1024 * 1024;
#endif // FOMENT_32BIT

uint_t TriggerObjects = 1024 * 16;
uint_t TriggerBytes = TriggerObjects * (sizeof(FPair) + sizeof(FObjHdr));
uint_t PartialPerFull = 4;

volatile uint_t BytesAllocated = 0;
volatile int_t GCRequired = 0;
static OSExclusive GCExclusive;
OSExclusive ThreadsExclusive;
static OSCondition ReadyCondition;
static OSCondition DoneCondition;
volatile uint_t TotalThreads;
static volatile uint_t ReadyThreads;
static volatile int_t Collecting;
FThreadState * Threads;

#define FREE_ADULTS 16
static FMemRegion Adults;
static uint_t AdultsUsed;
static FObjHdr * BigFreeAdults;
static FObjHdr * FreeAdults[FREE_ADULTS];

typedef struct
{
    FMemRegion MemRegion;
    uint_t Used;
} FMemSpace;

static FMemSpace Kids[2];
static FMemSpace * ActiveKids;

#ifdef FOMENT_WINDOWS
unsigned int TlsIndex;
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
pthread_key_t ThreadKey;
#endif // FOMENT_UNIX

#define FOMENT_OBJFTR 1

#ifdef FOMENT_OBJFTR
#define OBJECT_HDRFTR_LENGTH (sizeof(FObjHdr) + sizeof(FObjFtr))
#else // FOMENT_OBJFTR
#define OBJECT_HDRFTR_LENGTH sizeof(FObjHdr)
#endif // FOMENT_OBJFTR

#define MAXIMUM_TOTAL_LENGTH (OBJECT_HDRFTR_LENGTH + OBJHDR_COUNT_MASK * OBJECT_ALIGNMENT)
#define MINIMUM_TOTAL_LENGTH (OBJECT_HDRFTR_LENGTH + OBJECT_ALIGNMENT)

typedef struct _Guardian
{
    struct _Guardian * Next;
    FObject Object;
    FObject TConc;
} FGuardian;

static FGuardian * Guardians;

typedef struct _Tracker
{
    struct _Tracker * Next;
    FObject Object;
    FObject Return;
    FObject TConc;
} FTracker;

static FTracker * Trackers;

static uint_t LiveEphemerons;
static FEphemeron ** KeyEphemeronMap;
static uint_t KeyEphemeronMapSize;

// ---- Roots ----

FObject CleanupTConc = NoValueObject;

static FObject * Roots[128];
static uint_t RootsUsed = 0;
static const char * RootNames[sizeof(Roots) / sizeof(FObject *)];

static inline uint_t RoundToPageSize(uint_t cnt)
{
    if (cnt % GC_PAGE_SIZE != 0)
    {
        cnt += GC_PAGE_SIZE - (cnt % GC_PAGE_SIZE);

        FAssert(cnt % GC_PAGE_SIZE == 0);
    }

    return(cnt);
}

void * InitializeMemRegion(FMemRegion * mrgn, uint_t max)
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

int_t GrowMemRegionUp(FMemRegion * mrgn, uint_t sz)
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

int_t GrowMemRegionDown(FMemRegion * mrgn, uint_t sz)
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

static inline void SetGeneration(FObjHdr * oh, uint_t gen)
{
    oh->FlagsAndTag = ((oh->FlagsAndTag & ~OBJHDR_GEN_MASK) | ((uint16_t) gen));
}

static inline void SetMark(FObjHdr * oh)
{
    oh->FlagsAndTag |= OBJHDR_MARK_FORWARD;
}

static inline void ClearMark(FObjHdr * oh)
{
    oh->FlagsAndTag &= ~OBJHDR_MARK_FORWARD;
}

static inline int MarkP(FObjHdr * oh)
{
    return(oh->FlagsAndTag & OBJHDR_MARK_FORWARD);
}

static inline void SetForward(FObjHdr * oh)
{
    oh->FlagsAndTag |= OBJHDR_MARK_FORWARD;
}

static inline int ForwardP(FObjHdr * oh)
{
    return(oh->FlagsAndTag & OBJHDR_MARK_FORWARD);
}

static inline int AliveP(FObject obj)
{
    FAssert(MarkP(AsObjHdr(obj)) == ForwardP(AsObjHdr(obj)));

    return(MarkP(AsObjHdr(obj)));
}

static inline void SetCheckMark(FObject obj)
{
    AsObjHdr(obj)->FlagsAndTag |= OBJHDR_CHECK_MARK;
}

static inline void ClearCheckMark(FObjHdr * oh)
{
    oh->FlagsAndTag &= ~OBJHDR_CHECK_MARK;
}

static inline int CheckMarkP(FObject obj)
{
    return(AsObjHdr(obj)->FlagsAndTag & OBJHDR_CHECK_MARK);
}

static inline void SetEphemeronKeyMark(FObject obj)
{
    AsObjHdr(obj)->FlagsAndTag |= OBJHDR_EPHEMERON_KEY_MARK;
}

static inline void ClearEphemeronKeyMark(FObjHdr * oh)
{
    oh->FlagsAndTag &= ~OBJHDR_EPHEMERON_KEY_MARK;
}

static inline int EphemeronKeyMarkP(FObjHdr * oh)
{
    return(oh->FlagsAndTag & OBJHDR_EPHEMERON_KEY_MARK);
}

inline uint_t FObjHdr::TotalSize()
{
#ifdef FOMENT_OBJFTR
    return(ObjectSize() + sizeof(FObjHdr) + sizeof(FObjFtr));
#else // FOMENT_OBJFTR
    return(ObjectSize() + sizeof(FObjHdr));
#endif // FOMENT_OBJFTR
}

#ifdef FOMENT_OBJFTR
FObjFtr * AsObjFtr(FObjHdr * oh)
{
    return((FObjFtr *) (((char *) (oh + 1)) + oh->ObjectSize()));
}
#endif // FOMENT_OBJFTR

static void InitializeObjHdr(FObjHdr * oh, uint_t tsz, uint_t tag, uint_t gen, uint_t sz,
    uint_t sc)
{
    FAssert(tsz - sizeof(FObjHdr) >= OBJECT_ALIGNMENT);

    uint_t osz = tsz - sizeof(FObjHdr);

#ifdef FOMENT_OBJFTR
    FAssert(osz - sizeof(FObjFtr) >= OBJECT_ALIGNMENT);

    osz -= sizeof(FObjFtr);
#endif // FOMENT_OBJFTR

    FAssert(osz < (OBJHDR_COUNT_MASK * OBJECT_ALIGNMENT));
    FAssert(osz % OBJECT_ALIGNMENT == 0);

    oh->BlockSizeAndCount = (uint32_t) (osz / OBJECT_ALIGNMENT);
    oh->FlagsAndTag = (uint16_t) (gen | tag);

    FAssert(oh->ObjectSize() == osz);

    if (sc > 0)
    {
        oh->ExtraCount = (uint16_t) (osz / sizeof(FObject) - sc);
        oh->FlagsAndTag |= OBJHDR_HAS_SLOTS;

        FAssert(oh->SlotCount() == sc);
    }
    else if (tag != FreeTag)
    {
        FAssert(osz >= sz);
        FAssert(osz - sz <= 0xFFFF);

        oh->ExtraCount = (uint16_t) (osz - sz);

        FAssert(oh->ByteLength() == sz);
    }
    else
        oh->ExtraCount = 0;

    FAssert(oh->Tag() == tag);
    FAssert(oh->Generation() == gen);
}

static FObjHdr * MakeBaby(uint_t tsz, uint_t tag, uint_t sz, uint_t sc, const char * who)
{
    FThreadState * ts = GetThreadState();

    if (ts->BabiesUsed + tsz > ts->Babies.BottomUsed)
    {
        if (ts->BabiesUsed + tsz > ts->Babies.MaximumSize)
            RaiseExceptionC(Assertion, who, "babies too small; increase maximum-babies-size",
                    EmptyListObject);

        uint_t gsz = GC_PAGE_SIZE * 8;
        if (tsz > gsz)
            gsz = tsz;
        if (gsz > ts->Babies.MaximumSize - ts->Babies.BottomUsed)
            gsz = ts->Babies.MaximumSize - ts->Babies.BottomUsed;

        if (GrowMemRegionUp(&ts->Babies, ts->Babies.BottomUsed + gsz) == 0)
            RaiseExceptionC(Assertion, who, "babies: out of memory", EmptyListObject);
    }

    FObjHdr * oh = (FObjHdr *) (((char *) ts->Babies.Base) + ts->BabiesUsed);
    ts->BabiesUsed += tsz;

    InitializeObjHdr(oh, tsz, tag, OBJHDR_GEN_BABIES, sz, sc);
    return(oh);
}

static FObjHdr * AllocateKid(uint_t tsz)
{
    if (ActiveKids->Used + tsz > ActiveKids->MemRegion.BottomUsed)
    {
        if (ActiveKids->Used + tsz > ActiveKids->MemRegion.MaximumSize)
            return(0);

        uint_t gsz = GC_PAGE_SIZE * 8;
        if (tsz > gsz)
            gsz = tsz;
        if (gsz > ActiveKids->MemRegion.MaximumSize - ActiveKids->MemRegion.BottomUsed)
            gsz = ActiveKids->MemRegion.MaximumSize - ActiveKids->MemRegion.BottomUsed;

        if (GrowMemRegionUp(&ActiveKids->MemRegion, ActiveKids->MemRegion.BottomUsed + gsz) == 0)
            return(0);
    }

    FObjHdr * oh = (FObjHdr *) (((char *) ActiveKids->MemRegion.Base) + ActiveKids->Used);
    ActiveKids->Used += tsz;

    return(oh);
}

static FObjHdr * AllocateAdult(uint_t tsz, const char * who)
{
    uint_t bkt = tsz / OBJECT_ALIGNMENT;
    FObjHdr * oh = 0;

    if (bkt < FREE_ADULTS)
    {
        if (FreeAdults[bkt] != 0)
        {
            oh = FreeAdults[bkt];
            FreeAdults[bkt] = (FObjHdr *) *oh->Slots();

            FAssert(oh->TotalSize() == tsz);
            FAssert(oh->Tag() == FreeTag);
            FAssert(oh->Generation() == OBJHDR_GEN_ADULTS);
        }
    }
    else
//    if (oh == 0)
    {
        FObjHdr * foh = BigFreeAdults;
        FObjHdr ** pfoh = &BigFreeAdults;
        while (foh != 0)
        {
            uint_t ftsz = foh->TotalSize();

            FAssert(ftsz % OBJECT_ALIGNMENT == 0);
            FAssert(foh->Tag() == FreeTag);
            FAssert(foh->Generation() == OBJHDR_GEN_ADULTS);

            if (ftsz == tsz)
            {
                *pfoh = (FObjHdr *) *foh->Slots();
                break;
            }
            else if (ftsz >= tsz + FREE_ADULTS * OBJECT_ALIGNMENT)
            {
                FAssert(foh->TotalSize() > tsz);
                FAssert(foh->TotalSize() - tsz >= OBJECT_ALIGNMENT);

                oh = (FObjHdr *) (((char *) foh) + ftsz - tsz);
                foh->BlockSizeAndCount = (uint32_t) ((foh->ObjectSize() - tsz) / OBJECT_ALIGNMENT);

#ifdef FOMENT_OBJFTR
                FObjFtr * of = AsObjFtr(foh);
                of->Feet[0] = OBJFTR_FEET;
                of->Feet[1] = OBJFTR_FEET;
#endif // FOMENT_OBJFTR
                break;
            }

            pfoh = (FObjHdr **) foh->Slots();
            foh = (FObjHdr *) *foh->Slots();
        }
    }

    if (oh == 0)
    {
        if (AdultsUsed + tsz > Adults.BottomUsed)
        {
            if (AdultsUsed + tsz > Adults.MaximumSize)
            {
                if (who == 0)
                {
                    printf("error: adults too small; increase maximum-adults-size\n");
                    ErrorExitFoment();
                }

                LeaveExclusive(&GCExclusive);
                RaiseExceptionC(Assertion, who, "adults too small; increase maximum-adults-size",
                        EmptyListObject);
            }

            uint_t gsz = 1024 * 1024;
            if (tsz > gsz)
                gsz = tsz;
            if (gsz > Adults.MaximumSize - Adults.BottomUsed)
                gsz = Adults.MaximumSize - Adults.BottomUsed;
            if (GrowMemRegionUp(&Adults, Adults.BottomUsed + gsz) == 0)
            {
                if (who == 0)
                {
                    printf("error: adults: out of memory\n");
                    ErrorExitFoment();
                }

                LeaveExclusive(&GCExclusive);
                RaiseExceptionC(Assertion, who, "adults: out of memory", EmptyListObject);
            }
        }

        oh = (FObjHdr *) (((char *) Adults.Base) + AdultsUsed);
        AdultsUsed += tsz;
    }

    return(oh);
}

static FObjHdr * MakeAdult(uint_t tsz, uint_t tag, uint_t sz, uint_t sc, const char * who)
{
    EnterExclusive(&GCExclusive);
    FObjHdr * oh = AllocateAdult(tsz, who);
    LeaveExclusive(&GCExclusive);

    InitializeObjHdr(oh, tsz, tag, OBJHDR_GEN_ADULTS, sz, sc);
    return(oh);
}

FObject MakeObject(uint_t tag, uint_t sz, uint_t sc, const char * who, int_t pf)
{
    uint_t tsz = sz;
    FObjHdr * oh;

    tsz += Align[tsz % OBJECT_ALIGNMENT];
    if (tsz == 0)
        tsz = OBJECT_ALIGNMENT;
    tsz += sizeof(FObjHdr);
#ifdef FOMENT_OBJFTR
    tsz += sizeof(FObjFtr);
#endif // FOMENT_OBJFTR

    FAssert(tsz % OBJECT_ALIGNMENT == 0);
    FAssert(tag > 0);
    FAssert(tag < BadDogTag);
    FAssert(tag != FreeTag);
    FAssert(sz >= sizeof(FObject) * sc);

    if (tsz > MAXIMUM_TOTAL_LENGTH)
        RaiseExceptionC(Restriction, who, "object too big", EmptyListObject);
    if (sc > OBJHDR_COUNT_MASK)
        RaiseExceptionC(Restriction, who, "too many slots", EmptyListObject);

    if (CollectorType == GenerationalCollector)
    {
        if (pf || sz > MaximumGenerationalBaby)
            oh = MakeAdult(tsz, tag, sz, sc, who);
        else
            oh = MakeBaby(tsz, tag, sz, sc, who);
    }
    else if (CollectorType == MarkSweepCollector)
        oh = MakeAdult(tsz, tag, sz, sc, who);
    else
    {
        FAssert(CollectorType == NoCollector);
        oh = MakeBaby(tsz, tag, sz, sc, who);
    }

    FAssert(oh != 0);

    FThreadState * ts = GetThreadState();
    BytesAllocated += tsz;
    ts->ObjectsSinceLast += 1;
    ts->BytesSinceLast += tsz;

    if (CollectorType != NoCollector &&
            (ts->ObjectsSinceLast > TriggerObjects || ts->BytesSinceLast > TriggerBytes))
        GCRequired = 1;

#ifdef FOMENT_OBJFTR
    FObjFtr * of = AsObjFtr(oh);
    of->Feet[0] = OBJFTR_FEET;
    of->Feet[1] = OBJFTR_FEET;
#endif // FOMENT_OBJFTR

    return(oh + 1);
}

void RegisterRoot(FObject * root, const char * name)
{
    FAssert(RootsUsed < sizeof(Roots) / sizeof(FObject *));

    Roots[RootsUsed] = root;
    RootNames[RootsUsed] = name;
    RootsUsed += 1;
}

void ModifyVector(FObject obj, uint_t idx, FObject val)
{
    FAssert(VectorP(obj));
    FAssert(idx < VectorLength(obj));

    AsVector(obj)->Vector[idx] = val;
}

void ModifyObject(FObject obj, uint_t off, FObject val)
{
    FAssert(ObjectP(obj));
    FAssert(off % sizeof(FObject) == 0);

    ((FObject *) obj)[off / sizeof(FObject)] = val;
}

void SetFirst(FObject obj, FObject val)
{
    FAssert(PairP(obj));

    AsPair(obj)->First = val;
}

void SetRest(FObject obj, FObject val)
{
    FAssert(PairP(obj));

    AsPair(obj)->Rest = val;
}

void SetBox(FObject bx, FObject val)
{
    FAssert(BoxP(bx));

    AsBox(bx)->Value = val;
}

typedef struct
{
    int_t Index;
    int_t Repeat;
    FObject Object;
} FCheckStack;

#define WALK_STACK_SIZE (1024 * 8)
static uint_t CheckStackPtr;
static FCheckStack CheckStack[WALK_STACK_SIZE];
static const char * CheckFrom;

static const char * IndirectTagString[] =
{
    0,
    "bignum",
    "ratio",
    "complex",
    "flonum",
    "box",
    "pair",
    "string",
    "string-c",
    "vector",
    "bytevector",
    "binary-port",
    "textual-port",
    "procedure",
    "symbol",
    "record-type",
    "record",
    "primitive",
    "thread",
    "exclusive",
    "condition",
    "hash-node",
    "ephemeron",
    "builtin-type",
    "builtin",
    "free"
};

static const char * WhereFrom(FObject obj, int_t * idx)
{
    const char * from;

    if (ObjectP(obj))
    {
        switch (IndirectTag(obj))
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
                from = "thread.index-parameters";
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
            if (IndirectTag(obj) > 0 && IndirectTag(obj) < BadDogTag)
                from = IndirectTagString[IndirectTag(obj)];
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
    if (BuiltinObjectP(obj))
    {
        FMustBe(BuiltinTypeP(AsBuiltin(obj)->BuiltinType));

        printf(" %s", AsBuiltinType(AsBuiltin(obj)->BuiltinType)->Name);
    }
    else
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

            for (uint_t idx = 0; idx < StringLength(obj); idx++)
                putc(AsString(obj)->String[idx], stdout);
        }
    }
}

static void PrintCheckStack()
{
    FMustBe(CheckStackPtr > 0);

    const char * from = CheckFrom;
    int_t idx = CheckStack[0].Index;

    for (uint_t cdx = 0; cdx < CheckStackPtr - 1; cdx++)
    {
        if (idx >= 0)
            printf("%s[" INT_FMT "]", from, idx);
        else
            printf("%s", from);

        idx = CheckStack[cdx + 1].Index;
        from = WhereFrom(CheckStack[cdx].Object, &idx);
        PrintObjectString(CheckStack[cdx].Object);

        if (CheckStack[cdx].Repeat > 1)
            printf(" (repeats " INT_FMT " times)", CheckStack[cdx].Repeat);
        printf("\n");
    }

    FObject obj = CheckStack[CheckStackPtr - 1].Object;
    if (ObjectP(obj))
    {
        if (IndirectTag(obj) > 0 && IndirectTag(obj) < BadDogTag)
        {
            printf("%s: %p", IndirectTagString[IndirectTag(obj)], obj);
            PrintObjectString(obj);
        }
        else
            printf("unknown: %p tag: %x", obj, IndirectTag(obj));
    }
    else
        printf("unknown: %p", obj);
    printf("\n");
}

static int_t CheckFailedCount;
static int_t CheckCount;
static int_t CheckTooDeep;

static void FCheckFailed(const char * fn, int_t ln, const char * expr, FObjHdr * oh)
{
    uint_t idx;

    CheckFailedCount += 1;
    if (CheckFailedCount > 10)
        return;

    printf("\nFCheck: %s (%d)%s\n", expr, (int) ln, fn);

    uint_t tsz = oh->TotalSize();
    const char * tag = "unknown";
    if (oh->Tag() > 0 && oh->Tag() < BadDogTag)
        tag = IndirectTagString[oh->Tag()];
    printf("tsz: " UINT_FMT " osz: " UINT_FMT " blen: " UINT_FMT " slots: " UINT_FMT
            " tag: %s gen: 0x%x", tsz, oh->ObjectSize(),
            oh->ByteLength(), oh->SlotCount(), tag, oh->Generation());
    if (MarkP(oh))
        printf("forward/mark");
    printf(" |");
    for (idx = 0; idx < tsz && idx < 64; idx++)
        printf(" %x", ((uint8_t *) oh)[idx]);
    if (idx < tsz)
        printf(" ... (" UINT_FMT " more)", tsz - idx);
    printf("\n");

    if (CheckStackPtr > 0)
        PrintCheckStack();
}

#define FCheck(expr, oh)\
    if (! (expr)) FCheckFailed(__FILE__, __LINE__, #expr, oh)

static int_t ValidAddress(FObjHdr * oh)
{
    if (oh->Generation() == OBJHDR_GEN_ETERNAL)
        return(1);

    uint_t tsz = oh->TotalSize();
    FThreadState * ts = Threads;
    void * strt = oh;
    void * end = ((char *) oh) + tsz;

    while (ts != 0)
    {
        if (strt >= ts->Babies.Base && strt < ((char *) ts->Babies.Base) + ts->Babies.BottomUsed)
        {
            if (end > ((char *) ts->Babies.Base) + ts->Babies.BottomUsed)
                return(0);
            return(1);
        }

        ts = ts->Next;
    }

    if (Adults.Base != 0 && strt >= Adults.Base && strt < ((char *) Adults.Base + AdultsUsed)
            && end <= ((char *) Adults.Base) + AdultsUsed)
        return(1);

    if (ActiveKids != 0 && strt >= ActiveKids->MemRegion.Base
            && strt < ((char *) ActiveKids->MemRegion.Base + ActiveKids->Used)
            && end <= ((char *) ActiveKids->MemRegion.Base) + ActiveKids->Used)
        return(1);

    return(0);
}

static void CheckObject(FObject obj, int_t idx, int_t ef)
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
    switch (ImmediateTag(obj))
    {
    case 0x00: // ObjectP(obj)
    {
        FCheck(ef == 0 || AsObjHdr(obj)->Generation() == OBJHDR_GEN_ETERNAL, AsObjHdr(obj));

        if (CheckMarkP(obj))
            goto Done;
        SetCheckMark(obj);
        CheckCount += 1;

        FObjHdr * oh = AsObjHdr(obj);
        FCheck(CheckMarkP(obj), oh);
        FCheck(ValidAddress(oh), oh);
        FCheck(IndirectTag(obj) > 0 && IndirectTag(obj) < BadDogTag, oh);
        FCheck(IndirectTag(obj) != FreeTag, oh);
        FCheck(oh->ObjectSize() >= oh->SlotCount() * sizeof(FObject), oh);

#ifdef FOMENT_OBJFTR
        FObjFtr * of = AsObjFtr(oh);

        FCheck(of->Feet[0] == OBJFTR_FEET, oh);
        FCheck(of->Feet[1] == OBJFTR_FEET, oh);
#endif // FOMENT_OBJFTR

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
        else if (AsObjHdr(obj)->SlotCount() > 0)
        {
            FAssert(AsObjHdr(obj)->FlagsAndTag & OBJHDR_HAS_SLOTS);

            for (uint_t idx = 0; idx < AsObjHdr(obj)->SlotCount(); idx++)
                CheckObject(((FObject *) obj)[idx], idx, ef);
        }
        break;
    }

    case FixnumTag: // 0x01
    case CharacterTag: // 0x02
    case MiscellaneousTag: // 0x03
    case SpecialSyntaxTag: // 0x04
    case InstructionTag: // 0x05
    case ValuesCountTag: // 0x06
    case BooleanTag: // 0x07
        break;
    }

Done:
    FMustBe(CheckStackPtr > 0);
    CheckStackPtr -= 1;
}

static void CheckRoot(FObject obj, const char * from, int_t idx)
{
    CheckFrom = from;
    CheckObject(obj, idx, 0);
}

static void CheckThreadState(FThreadState * ts)
{
    CheckRoot(ts->Thread, "thread-state.thread", -1);

    uint_t idx = 0;
    for (FAlive * ap = ts->AliveList; ap != 0; ap = ap->Next, idx++)
        CheckRoot(*ap->Pointer, "thread-state.alive-list", idx);

    for (int_t adx = 0; adx < ts->AStackPtr; adx++)
        CheckRoot(ts->AStack[adx], "thread-state.astack", adx);

    for (int_t cdx = 0; cdx < ts->CStackPtr; cdx++)
        CheckRoot(ts->CStack[- cdx], "thread-state.cstack", cdx);

    CheckRoot(ts->Proc, "thread-state.proc", -1);
    CheckRoot(ts->Frame, "thread-state.frame", -1);
    CheckRoot(ts->DynamicStack, "thread-state.dynamic-stack", -1);
    CheckRoot(ts->Parameters, "thread-state.parameters", -1);

    for (int_t idx = 0; idx < INDEX_PARAMETERS; idx++)
        CheckRoot(ts->IndexParameters[idx], "thread-state.index-parameters", idx);

    CheckRoot(ts->NotifyObject, "thread-state.notify-object", -1);
}

static void CheckMemRegion(FMemRegion * mrgn, uint_t used, uint_t gen)
{
    FObjHdr * oh = (FObjHdr *) mrgn->Base;
    uint_t cnt = 0;

    while (cnt < used)
    {
        FCheck(cnt + sizeof(FObjHdr) <= mrgn->BottomUsed, oh);
        uint_t osz = oh->ObjectSize();
        uint_t tsz = oh->TotalSize();

        FCheck(tsz >= osz + sizeof(FObjHdr), oh);
        FCheck(tsz % OBJECT_ALIGNMENT == 0, oh);

#ifdef FOMENT_OBJFTR
        FCheck(tsz >= osz + sizeof(FObjHdr) + sizeof(FObjFtr), oh);
#endif // FOMENT_OBJFTR

        FCheck(cnt + tsz <= mrgn->BottomUsed, oh);

#ifdef FOMENT_OBJFTR
        FObjFtr * of = AsObjFtr(oh);

        FCheck(of->Feet[0] == OBJFTR_FEET, oh);
        FCheck(of->Feet[1] == OBJFTR_FEET, oh);
#endif // FOMENT_OBJFTR

        FCheck(oh->Generation() == gen, oh);
        FCheck(gen == OBJHDR_GEN_ADULTS || (oh->FlagsAndTag & OBJHDR_MARK_FORWARD) == 0, oh);
        FCheck(oh->SlotCount() * sizeof(FObject) <= oh->ObjectSize(), oh);
        FCheck(oh->Tag() > 0 && oh->Tag() < BadDogTag, oh);
        FCheck(oh->Tag() != FreeTag || oh->Generation() == OBJHDR_GEN_ADULTS, oh);

        ClearCheckMark(oh);
        oh = (FObjHdr *) (((char *) oh) + tsz);
        cnt += tsz;
    }
}

void CheckHeap(const char * fn, int ln)
{
    EnterExclusive(&GCExclusive);

    FMustBe(sizeof(IndirectTagString) / sizeof(char *) == BadDogTag);

    if (VerboseFlag)
        printf("CheckHeap: %s(%d)\n", fn, ln);

    CheckFailedCount = 0;
    CheckCount = 0;
    CheckTooDeep = 0;
    CheckStackPtr = 0;

    if (Adults.Base != 0)
        CheckMemRegion(&Adults, AdultsUsed, OBJHDR_GEN_ADULTS);

    FThreadState * ts = Threads;
    while (ts != 0)
    {
        CheckMemRegion(&ts->Babies, ts->BabiesUsed, OBJHDR_GEN_BABIES);
        ts = ts->Next;
    }

    for (uint_t rdx = 0; rdx < RootsUsed; rdx++)
        CheckRoot(*Roots[rdx], RootNames[rdx], -1);

    ts = Threads;
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

    FTracker * trkr = Trackers;
    while (trkr)
    {
        CheckRoot(trkr->Object, "tracker.object", -1);
        CheckRoot(trkr->Return, "tracker.return", -1);
        CheckRoot(trkr->TConc, "tracker.tconc", -1);

        trkr = trkr->Next;
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

static void ScanObject(FObject * pobj);
static inline void LiveObject(FObject * pobj)
{
    if (ObjectP(*pobj))
        ScanObject(pobj);
}

static void ScanObject(FObject * pobj)
{
Again:
    FAssert(ObjectP(*pobj));

    FObjHdr * oh = AsObjHdr(*pobj);
    uint_t gen = oh->Generation();

    if (EphemeronKeyMarkP(oh))
    {
        FAssert(MarkP(oh) == ForwardP(oh));
        FAssert(MarkP(oh) == 0);

        ClearEphemeronKeyMark(oh);

        FObject key = (FObject) (oh + 1);
        uint_t idx = EqHash(key) % KeyEphemeronMapSize;
        FEphemeron * eph = KeyEphemeronMap[idx];
        FEphemeron ** peph = &KeyEphemeronMap[idx];
        while (eph != 0)
        {
            if (eph->Key == key)
            {
                LiveEphemerons += 1;
                LiveObject(&eph->Key);
                LiveObject(&eph->Datum);
                LiveObject(&eph->HashTable);

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

    if (gen == OBJHDR_GEN_BABIES || gen == OBJHDR_GEN_KIDS)
    {
        if (ForwardP(oh))
        {
            *pobj = oh->Slots()[0];
            return;
        }

        uint_t tsz = oh->TotalSize();
        FObjHdr * noh = 0;

        if (gen == OBJHDR_GEN_BABIES && ActiveKids != 0)
        {
            noh = AllocateKid(tsz);
            memcpy(noh, oh, tsz);
            SetGeneration(noh, OBJHDR_GEN_KIDS);
        }

        if (noh == 0)
        {
            noh = AllocateAdult(tsz, 0);
            memcpy(noh, oh, tsz);
            SetGeneration(noh, OBJHDR_GEN_ADULTS);
            SetMark(noh);
        }

        SetForward(oh);
        oh->Slots()[0] = noh + 1;
        *pobj = noh + 1;
        oh = noh;
    }
    else if (oh->Generation() != OBJHDR_GEN_ETERNAL)
    {
        FAssert(oh->Generation() == OBJHDR_GEN_ADULTS);

        if (MarkP(oh))
            return;

        SetMark(oh);
    }

    if (oh->Tag() != EphemeronTag)
    {
        uint_t sc = oh->SlotCount();
        if (sc > 0)
        {
            FAssert(oh->FlagsAndTag & OBJHDR_HAS_SLOTS);

            uint_t sdx = 0;
            while (sdx < sc - 1)
            {
                LiveObject(oh->Slots() + sdx);
                sdx += 1;
            }

            if (ObjectP(oh->Slots()[sdx]))
            {
                pobj = oh->Slots() + sdx;
                goto Again;
            }
        }
    }
    else
    {
        FEphemeron * eph = (FEphemeron *) (oh + 1);

        FAssert(oh->SlotCount() == 0);

        if (eph->Next != EPHEMERON_BROKEN)
        {
            if (KeyEphemeronMap == 0 || ObjectP(eph->Key) == 0 || AliveP(eph->Key))
            {
                LiveEphemerons += 1;
                LiveObject(&eph->Key);
                LiveObject(&eph->Datum);
                LiveObject(&eph->HashTable);
            }
            else
            {
                FAssert(ObjectP(eph->Key));

                uint_t idx = EqHash(eph->Key) % KeyEphemeronMapSize;
                eph->Next = KeyEphemeronMap[idx];
                KeyEphemeronMap[idx] = eph;

                if (EphemeronKeyMarkP(AsObjHdr(eph->Key)) == 0)
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

                    LiveObject(&grd->Object);
                    LiveObject(&grd->TConc);
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
            LiveObject(&grd->Object);
            LiveObject(&grd->TConc);
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

static FTracker * CollectTrackers()
{
    FTracker * moved = 0;
    FTracker * maybe = Trackers;

    Trackers = 0;
    while (maybe != 0)
    {
        FTracker * trkr = maybe;
        maybe = maybe->Next;

        FAssert(AsObjHdr(trkr->Object)->Generation() == OBJHDR_GEN_BABIES
                || AsObjHdr(trkr->Object)->Generation() == OBJHDR_GEN_KIDS);

        if (ForwardP(AsObjHdr(trkr->Object)) && AliveP(trkr->Return) && AliveP(trkr->TConc))
        {
            LiveObject(&trkr->Return);
            LiveObject(&trkr->TConc);

            trkr->Object = NoValueObject;
            trkr->Next = moved;
            moved = trkr;
        }
        else
            free(trkr);
    }

    return(moved);
}

static void Collect()
{
    FAssert(CollectorType != NoCollector);

    if (VerboseFlag)
        printf("Garbage Collection...\n");

    if (CheckHeapFlag)
        CheckHeap(__FILE__, __LINE__);

    FObjHdr * oh = (FObjHdr *) Adults.Base;
    while ((char *) oh < ((char *) Adults.Base) + AdultsUsed)
    {
        ClearMark(oh);
        oh = (FObjHdr *) (((char *) oh) + oh->TotalSize());
    }

    if (ActiveKids != 0)
    {
        if (ActiveKids == &Kids[0])
            ActiveKids = &Kids[1];
        else
        {
            FAssert(ActiveKids == &Kids[1]);

            ActiveKids = &Kids[0];
        }
        ActiveKids->Used = 0;
    }

    FAssert(KeyEphemeronMap == 0);

    KeyEphemeronMapSize = LiveEphemerons;
    KeyEphemeronMap = (FEphemeron **) malloc(sizeof(FEphemeron *) * KeyEphemeronMapSize);
    if (KeyEphemeronMap != 0)
        memset(KeyEphemeronMap, 0, sizeof(FEphemeron *) * KeyEphemeronMapSize);
    LiveEphemerons = 0;

    for (uint_t rdx = 0; rdx < RootsUsed; rdx++)
        LiveObject(Roots[rdx]);

    FThreadState * ts = Threads;
    while (ts != 0)
    {
        LiveObject(&ts->Thread);

        for (FAlive * ap = ts->AliveList; ap != 0; ap = ap->Next)
            LiveObject(ap->Pointer);

        for (int_t adx = 0; adx < ts->AStackPtr; adx++)
            LiveObject(ts->AStack + adx);

        for (int_t cdx = 0; cdx < ts->CStackPtr; cdx++)
            LiveObject(ts->CStack - cdx);

        LiveObject(&ts->Proc);
        LiveObject(&ts->Frame);
        LiveObject(&ts->DynamicStack);
        LiveObject(&ts->Parameters);

        for (int_t idx = 0; idx < INDEX_PARAMETERS; idx++)
            LiveObject(ts->IndexParameters + idx);

        LiveObject(&ts->NotifyObject);

        ts->ObjectsSinceLast = 0;
        ts->BytesSinceLast = 0;
        ts->BabiesUsed = 0;
        ts = ts->Next;
    }

    FGuardian * final = CollectGuardians();
    FTracker * moved = CollectTrackers();

    BigFreeAdults = 0;
    for (int_t idx = 0; idx < FREE_ADULTS; idx++)
        FreeAdults[idx] = 0;

    oh = (FObjHdr *) Adults.Base;
    while ((char *) oh < ((char *) Adults.Base) + AdultsUsed)
    {
        uint_t tsz = oh->TotalSize();
        if (MarkP(oh) == 0)
        {
            FObjHdr * noh = (FObjHdr *) (((char *) oh) + tsz);
            while ((char *) noh < ((char *) Adults.Base) + AdultsUsed)
            {
                if (MarkP(noh)
                    || ((char *) noh - (char *) oh) + noh->TotalSize() > MAXIMUM_TOTAL_LENGTH)
                    break;
                noh = (FObjHdr *) (((char *) noh) + noh->TotalSize());
            }

            FAssert((uint_t) ((char *) noh - (char *) oh) >= tsz);

            tsz = (char *) noh - (char *) oh;
            uint_t bkt = tsz / OBJECT_ALIGNMENT;
            if (bkt < FREE_ADULTS)
            {
                *oh->Slots() = FreeAdults[bkt];
                FreeAdults[bkt] = oh;
            }
            else
            {
                *oh->Slots() = BigFreeAdults;
                BigFreeAdults = oh;
            }

            InitializeObjHdr(oh, tsz, FreeTag, OBJHDR_GEN_ADULTS, 0, 0);
        }

        oh = (FObjHdr *) (((char *) oh) + tsz);
    }

    while (final != 0)
    {
        FGuardian * grd = final;
        final = final->Next;

        TConcAdd(grd->TConc, grd->Object);
        free(grd);
    }

    while (moved != 0)
    {
        FTracker * trkr = moved;
        moved = moved->Next;

        TConcAdd(trkr->TConc, trkr->Return);
        free(trkr);
    }

    if (KeyEphemeronMap != 0)
    {
        for (uint_t idx = 0; idx < KeyEphemeronMapSize; idx++)
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
        else
        {
            FAssert(0);
        }
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

void InstallTracker(FObject obj, FObject ret, FObject tconc)
{
    if (CollectorType == GenerationalCollector)
    {
        FObjHdr * oh = AsObjHdr(obj);
        if (oh->Generation() == OBJHDR_GEN_BABIES || oh->Generation() == OBJHDR_GEN_KIDS)
        {
            FAssert(ObjectP(obj));
            FAssert(PairP(tconc));
            FAssert(PairP(First(tconc)));
            FAssert(PairP(Rest(tconc)));

            FTracker * trkr = (FTracker *) malloc(sizeof(FTracker));

            if (trkr == 0)
                RaiseExceptionC(Assertion, "install-tracker", "out of memory", EmptyListObject);

            trkr->Object = obj;
            trkr->Return = ret;
            trkr->TConc = tconc;

            EnterExclusive(&GCExclusive);
            trkr->Next = Trackers;
            Trackers = trkr;
            LeaveExclusive(&GCExclusive);
        }
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

int_t EnterThread(FThreadState * ts, FObject thrd, FObject prms, FObject idxprms)
{
    memset(ts, 0, sizeof(FThreadState));

#ifdef FOMENT_WINDOWS
    FAssert(TlsGetValue(TlsIndex) == 0);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    FAssert(pthread_getspecific(ThreadKey) == 0);
#endif // FOMENT_UNIX

    SetThreadState(ts);

    EnterExclusive(&ThreadsExclusive);
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
    LeaveExclusive(&ThreadsExclusive);

    ts->Thread = thrd;
    ts->AliveList = 0;
    ts->ObjectsSinceLast = 0;
    ts->BytesSinceLast = 0;

    if (CollectorType == NoCollector || CollectorType == GenerationalCollector)
    {
        if (InitializeMemRegion(&ts->Babies, MaximumBabiesSize) == 0)
            goto Failed;
        if (GrowMemRegionUp(&ts->Babies, GC_PAGE_SIZE * 8) == 0)
            goto Failed;
        ts->BabiesUsed = 0;
    }

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
    ts->ExceptionCount = 0;
    return(1);

Failed:
    if (ts->Babies.Base != 0)
        DeleteMemRegion(&ts->Babies);
    if (ts->Stack.Base != 0)
        DeleteMemRegion(&ts->Stack);
    return(0);
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

    EnterExclusive(&ThreadsExclusive);
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

    FAssert(ts->Stack.Base != 0);
    DeleteMemRegion(&ts->Stack);

    ts->AStack = 0;
    ts->CStack = 0;
    ts->Thread = NoValueObject;

    if (Collecting && ReadyThreads == TotalThreads)
        WakeCondition(&ReadyCondition); // Just in case a collection is pending.
    LeaveExclusive(&ThreadsExclusive);

    return(tt);
}

int_t SetupCore(FThreadState * ts)
{
    FAssert(sizeof(FObject) == sizeof(int_t));
    FAssert(sizeof(FObject) == sizeof(uint_t));
    FAssert(sizeof(FObject) == sizeof(FImmediate));
    FAssert(sizeof(FObject) == sizeof(char *));
    FAssert(sizeof(FFixnum) <= sizeof(FImmediate));
    FAssert(sizeof(FCh) <= sizeof(FImmediate));
    FAssert(sizeof(FObjHdr) == OBJECT_ALIGNMENT);
    FAssert(sizeof(FObjFtr) == OBJECT_ALIGNMENT);
    FAssert(BadDogTag <= OBJHDR_TAG_MASK + 1);
    FAssert(sizeof(FObject) <= OBJECT_ALIGNMENT);
    FAssert(sizeof(FCString) % OBJECT_ALIGNMENT == 0);
    FAssert(sizeof(FSymbol) % OBJECT_ALIGNMENT == 0);
    FAssert(sizeof(FPrimitive) % OBJECT_ALIGNMENT == 0);
    FAssert(Adults.Base == 0);
    FAssert(AdultsUsed == 0);
    FAssert(ActiveKids == 0);

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

    if (CollectorType == NoCollector && MaximumBabiesSize == 0)
        MaximumBabiesSize = 1024 * 1024 * 256;
    if (CollectorType == MarkSweepCollector && MaximumBabiesSize != 0)
        MaximumBabiesSize = 0;
    if (CollectorType == GenerationalCollector)
    {
        if (MaximumBabiesSize == 0)
            MaximumBabiesSize = 1024 * 1024 * 128;
        if (MaximumKidsSize == 0)
            MaximumKidsSize = MaximumBabiesSize;

        if (InitializeMemRegion(&Kids[0].MemRegion, MaximumKidsSize) == 0)
            return(0);
        if (InitializeMemRegion(&Kids[1].MemRegion, MaximumKidsSize) == 0)
            return(0);
        if (GrowMemRegionUp(&Kids[0].MemRegion, GC_PAGE_SIZE * 8) == 0)
            return(0);
        if (GrowMemRegionUp(&Kids[1].MemRegion, GC_PAGE_SIZE * 8) == 0)
            return(0);
        ActiveKids = &Kids[0];
    }
    if (CollectorType == MarkSweepCollector || CollectorType == GenerationalCollector)
    {
        FAssert(BigFreeAdults == 0);

        if (InitializeMemRegion(&Adults, MaximumAdultsSize) == 0)
            return(0);
        if (GrowMemRegionUp(&Adults, GC_PAGE_SIZE * 8) == 0)
            return(0);

#ifdef FOMENT_DEBUG
        for (uint_t idx = 0; idx < FREE_ADULTS; idx++)
            FAssert(FreeAdults[idx] == 0);
#endif // FOMENT_DEBUG
    }

#ifdef FOMENT_WINDOWS
    TlsIndex = TlsAlloc();
    FAssert(TlsIndex != TLS_OUT_OF_INDEXES);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    pthread_key_create(&ThreadKey, 0);
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

    TotalThreads = 1;
    if (EnterThread(ts, NoValueObject, NoValueObject, NoValueObject) == 0)
        return(0);
    ts->Thread = MakeThread(h, NoValueObject, NoValueObject, NoValueObject);

    RegisterRoot(&CleanupTConc, "cleanup-tconc");
    CleanupTConc = MakeTConc();

    if (CheckHeapFlag)
        CheckHeap(__FILE__, __LINE__);

    return(1);
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
    {
//        AsEphemeron(eph)->Key = key;
        Modify(FEphemeron, eph, Key, key);
    }
}

void EphemeronDatumSet(FObject eph, FObject dat)
{
    FAssert(EphemeronP(eph));

    if (AsEphemeron(eph)->Next != EPHEMERON_BROKEN)
    {
//        AsEphemeron(eph)->Datum = dat;
        Modify(FEphemeron, eph, Datum, dat);
    }
}

Define("ephemeron?", EphemeronPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("ephemeron?", argc);

    return(EphemeronP(argv[0]) ? TrueObject : FalseObject);
}

Define("make-ephemeron", MakeEphemeronPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("make-ephemeron", argc);

    return(MakeEphemeron(argv[0], argv[1], NoValueObject));
}

Define("ephemeron-broken?", EphemeronBrokenPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("ephemeron-broken?", argc);
    EphemeronArgCheck("ephemeron-broken?", argv[0]);

    return(EphemeronBrokenP(argv[0]) ? TrueObject : FalseObject);
}

Define("ephemeron-key", EphemeronKeyPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("ephemeron-key", argc);
    EphemeronArgCheck("ephemeron-key", argv[0]);

    return(AsEphemeron(argv[0])->Key);
}

Define("ephemeron-datum", EphemeronDatumPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("ephemeron-datum", argc);
    EphemeronArgCheck("ephemeron-datum", argv[0]);

    return(AsEphemeron(argv[0])->Datum);
}

Define("set-ephemeron-key!", SetEphemeronKeyPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("set-ephemeron-key!", argc);
    EphemeronArgCheck("set-ephemeron-key!", argv[0]);

    EphemeronKeySet(argv[0], argv[1]);
    return(NoValueObject);
}

Define("set-ephemeron-datum!", SetEphemeronDatumPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("set-ephemeron-datum!", argc);
    EphemeronArgCheck("set-ephemeron-datum!", argv[0]);

    EphemeronDatumSet(argv[0], argv[1]);
    return(NoValueObject);
}

static FObject Primitives[] =
{
    InstallGuardianPrimitive,
    InstallTrackerPrimitive,
    CollectPrimitive,
    EphemeronPPrimitive,
    MakeEphemeronPrimitive,
    EphemeronBrokenPPrimitive,
    EphemeronKeyPrimitive,
    EphemeronDatumPrimitive,
    SetEphemeronKeyPrimitive,
    SetEphemeronDatumPrimitive
};

void SetupGC()
{
    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(Bedrock, BedrockLibrary, Primitives[idx]);
}
