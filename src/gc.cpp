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

#if defined(FOMENT_BSD) || defined(__APPLE__)
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

#define PAGE_SIZE 4096

#define MAXIMUM_OBJECT_LENGTH OBJHDR_SIZE_MASK
#define MAXIMUM_SLOT_COUNT OBJHDR_SLOT_COUNT_MASK

#define OBJECT_ALIGNMENT 8
const static uint_t Align[8] = {0, 7, 6, 5, 4, 3, 2, 1};

FCollectorType CollectorType = MarkSweepCollector;
uint_t MaximumStackSize = 1024 * 1024 * 4 * sizeof(FObject);
uint_t MaximumBabiesSize = 0;
uint_t MaximumKidsSize = 0;
uint_t MaximumGenerationalBaby = 1024 * 64;

#ifdef FOMENT_64BIT
uint_t MaximumAdultsSize = 1024 * 1024 * 1024 * 4UL;
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

#ifdef FOMENT_WINDOWS
unsigned int TlsIndex;
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
pthread_key_t ThreadKey;
#endif // FOMENT_UNIX

#define FOMENT_OBJFTR 1

#ifdef FOMENT_OBJFTR
#define OBJFTR_FEET 0xDEADFEE7

typedef struct
{
    uint32_t Feet[2];
} FObjFtr;

#define OBJECT_HDRFTR_LENGTH (sizeof(FObjHdr) + sizeof(FObjFtr))
#else // FOMENT_OBJFTR
#define OBJECT_HDRFTR_LENGTH sizeof(FObjHdr)
#endif // FOMENT_OBJFTR

typedef struct _Guardian
{
    struct _Guardian * Next;
    FObject Object;
    FObject TConc;
} FGuardian;

static FGuardian * Guardians;

static inline uint_t RoundToPageSize(uint_t cnt)
{
    if (cnt % PAGE_SIZE != 0)
    {
        cnt += PAGE_SIZE - (cnt % PAGE_SIZE);

        FAssert(cnt % PAGE_SIZE == 0);
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
    FAssert(mrgn->MaximumSize % PAGE_SIZE == 0);
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
    FAssert(mrgn->TopUsed % PAGE_SIZE == 0);
    FAssert(mrgn->BottomUsed % PAGE_SIZE == 0);
    FAssert(mrgn->MaximumSize % PAGE_SIZE == 0);
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
    FAssert(mrgn->TopUsed % PAGE_SIZE == 0);
    FAssert(mrgn->BottomUsed % PAGE_SIZE == 0);
    FAssert(mrgn->MaximumSize % PAGE_SIZE == 0);
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

static inline void SetMark(FObjHdr * oh)
{
    oh->Flags |= OBJHDR_MARK_FORWARD;
}

static inline void ClearMark(FObjHdr * oh)
{
    oh->Flags &= ~OBJHDR_MARK_FORWARD;
}

static inline int MarkP(FObjHdr * oh)
{
    return(oh->Flags & OBJHDR_MARK_FORWARD);
}

static inline void SetCheckMark(FObject obj)
{
    AsObjHdr(obj)->CheckMark |= OBJHDR_CHECK_MARK;
}

static inline void ClearCheckMark(FObjHdr * oh)
{
    oh->CheckMark &= ~OBJHDR_CHECK_MARK;
}

static inline int CheckMarkP(FObject obj)
{
    return(AsObjHdr(obj)->CheckMark & OBJHDR_CHECK_MARK);
}

static inline uint_t ObjectLength(uint_t sz)
{
    uint_t len = sz;
    len += Align[len % OBJECT_ALIGNMENT];

    FAssert(len >= sz);
    FAssert(len % OBJECT_ALIGNMENT == 0);

    if (len == 0)
        len = OBJECT_ALIGNMENT;

    FAssert(len >= sizeof(FObject));

    len += sizeof(FObjHdr);

    FAssert(len % OBJECT_ALIGNMENT == 0);

#ifdef FOMENT_OBJFTR
    len += sizeof(FObjFtr);

    FAssert(len % OBJECT_ALIGNMENT == 0);
#endif // FOMENT_OBJFTR

    return(len);
}

#ifdef FOMENT_OBJFTR
FObjFtr * AsObjFtr(FObjHdr * oh)
{
    return(((FObjFtr *) (((char *) oh) + ObjectLength(oh->Size()))) - 1);
}
#endif // FOMENT_OBJFTR

static FObjHdr * MakeBaby(uint_t len, uint_t tag, uint_t sz, uint_t sc, const char * who)
{
    FThreadState * ts = GetThreadState();

    if (ts->BabiesUsed + len > ts->Babies.BottomUsed)
    {
        if (ts->BabiesUsed + len > ts->Babies.MaximumSize)
            RaiseExceptionC(R.Assertion, who, "babies too small; increase maximum-babies-size",
                    EmptyListObject);

        uint_t gsz = PAGE_SIZE * 8;
        if (len > gsz)
            gsz = len;
        if (gsz > ts->Babies.MaximumSize - ts->Babies.BottomUsed)
            gsz = ts->Babies.MaximumSize - ts->Babies.BottomUsed;

        if (GrowMemRegionUp(&ts->Babies, ts->Babies.BottomUsed + gsz) == 0)
            RaiseExceptionC(R.Assertion, who, "babies: out of memory", EmptyListObject);
    }

    FObjHdr * oh = (FObjHdr *) (((char *) ts->Babies.Base) + ts->BabiesUsed);
    ts->BabiesUsed += len;

    oh->FlagsAndSize = sz;
    oh->TagAndSlotCount = (tag << OBJHDR_TAG_SHIFT) | sc;

    return(oh);
}

static FObjHdr * MakeAdult(uint_t len, uint_t tag, uint_t sz, uint_t sc, const char * who)
{
    uint_t bkt = len / OBJECT_ALIGNMENT;
    FObjHdr * oh = 0;

    EnterExclusive(&GCExclusive);
    if (bkt < FREE_ADULTS)
    {
        if (FreeAdults[bkt] != 0)
        {
            oh = FreeAdults[bkt];
            FreeAdults[bkt] = (FObjHdr *) *oh->Slots();

            FAssert(ObjectLength(oh->Size()) == len);
            FAssert(oh->Tag() == FreeTag);
            FAssert(oh->Generation() == OBJHDR_GEN_ADULTS);
        }
    }
    else
    {
        FObjHdr * foh = BigFreeAdults;
        FObjHdr ** pfoh = &BigFreeAdults;
        while (foh != 0)
        {
            FAssert(foh->Size() % OBJECT_ALIGNMENT == 0);
            FAssert(foh->Tag() == FreeTag);
            FAssert(foh->Generation() == OBJHDR_GEN_ADULTS);

            uint_t flen = ObjectLength(foh->Size());
            if (flen == len)
            {
                *pfoh = (FObjHdr *) *foh->Slots();
                break;
            }
            else if (flen >= len + FREE_ADULTS * OBJECT_ALIGNMENT)
            {
                FAssert(foh->Size() > len);
                FAssert(foh->Size() - len >= OBJECT_ALIGNMENT);

                oh = (FObjHdr *) (((char *) foh) + flen - len);
                foh->FlagsAndSize = (foh->Size() - len) | (foh->FlagsAndSize & ~OBJHDR_SIZE_MASK);

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
        if (AdultsUsed + len > Adults.BottomUsed)
        {
            if (AdultsUsed + len > Adults.MaximumSize)
            {
                LeaveExclusive(&GCExclusive);
                RaiseExceptionC(R.Assertion, who, "adults too small; increase maximum-adults-size",
                        EmptyListObject);
            }

            uint_t gsz = 1024 * 1024;
            if (len > gsz)
                gsz = len;
            if (gsz > Adults.MaximumSize - Adults.BottomUsed)
                gsz = Adults.MaximumSize - Adults.BottomUsed;
            if (GrowMemRegionUp(&Adults, Adults.BottomUsed + gsz) == 0)
            {
                LeaveExclusive(&GCExclusive);
                RaiseExceptionC(R.Assertion, who, "adults: out of memory", EmptyListObject);
            }
        }

        oh = (FObjHdr *) (((char *) Adults.Base) + AdultsUsed);
        AdultsUsed += len;
    }
    LeaveExclusive(&GCExclusive);

    oh->FlagsAndSize = sz | OBJHDR_GEN_ADULTS;
    oh->TagAndSlotCount = (tag << OBJHDR_TAG_SHIFT) | sc;

    return(oh);
}

FObject MakeObject(uint_t tag, uint_t sz, uint_t sc, const char * who, int_t pf)
{
    uint_t len = ObjectLength(sz);
    FObjHdr * oh;

    FAssert(tag > 0);
    FAssert(tag < BadDogTag);
    FAssert(tag != FreeTag);
    FAssert(sz >= sizeof(FObject) * sc);

    if (len > MAXIMUM_OBJECT_LENGTH)
        RaiseExceptionC(R.Restriction, who, "object too big", EmptyListObject);
    if (sc > MAXIMUM_SLOT_COUNT)
        RaiseExceptionC(R.Restriction, who, "too many slots", EmptyListObject);

    if (CollectorType == GenerationalCollector)
    {
        if (pf || sz > MaximumGenerationalBaby)
            oh = MakeAdult(len, tag, sz, sc, who);
        else
            oh = MakeBaby(len, tag, sz, sc, who);
    }
    else if (CollectorType == MarkSweepCollector)
        oh = MakeAdult(len, tag, sz, sc, who);
    else
    {
        FAssert(CollectorType == NoCollector);
        oh = MakeBaby(len, tag, sz, sc, who);
    }

    FAssert(oh != 0);

    FThreadState * ts = GetThreadState();
    BytesAllocated += len;
    ts->ObjectsSinceLast += 1;
    ts->BytesSinceLast += len;

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

void PushRoot(FObject * rt)
{
    FThreadState * ts = GetThreadState();

    ts->RootsUsed += 1;

    FAssert(ts->RootsUsed < sizeof(ts->Roots) / sizeof(FObject *));

    ts->Roots[ts->RootsUsed - 1] = rt;
}

void PopRoot()
{
    FThreadState * ts = GetThreadState();

    FAssert(ts->RootsUsed > 0);

    ts->RootsUsed -= 1;
}

void ClearRoots()
{
    FThreadState * ts = GetThreadState();

    ts->RootsUsed = 0;
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

static const char * RootNames[] =
{
    "bedrock",
    "bedrock-library",
    "features",
    "command-line",
    "full-command-line",
    "library-path",
    "library-extensions",
    "environment-variables",

    "symbol-hash-tree",

    "comparator-record-type",
    "anyp-primitive",
    "no-hash-primitive",
    "no-compare-primitive",
    "eq-comparator",
    "default-comparator",

    "hash-map-record-type",
    "hash-set-record-type",
    "hash-bag-record-type",
    "exception-record-type",

    "assertion",
    "restriction",
    "lexical",
    "syntax",
    "error",

    "loaded-libraries",

    "syntactic-env-record-type",
    "binding-record-type",
    "identifier-record-type",
    "lambda-record-type",
    "case-lambda-record-type",
    "inline-variable-record-type",
    "reference-record-type",

    "else-reference",
    "arrow-reference",
    "library-reference",
    "and-reference",
    "or-reference",
    "not-reference",
    "quasiquote-reference",
    "unquote-reference",
    "unquote-splicing-reference",
    "cons-reference",
    "append-reference",
    "list-to-vector-reference",
    "ellipsis-reference",
    "underscore-reference",

    "tag-symbol",
    "use-pass-symbol",
    "constant-pass-symbol",
    "analysis-pass-symbol",
    "interaction-env",

    "standard-input",
    "standard-output",
    "standard-error",
    "quote-symbol",
    "quasiquote-symbol",
    "unquote-symbol",
    "unquote-splicing-symbol",
    "file-error-symbol",
    "current-symbol",
    "end-symbol",

    "syntax-rules-record-type",
    "pattern-variable-record-type",
    "pattern-repeat-record-type",
    "template-repeat-record-type",
    "syntax-rule-record-type",

    "environment-record-type",
    "global-record-type",
    "library-record-type",
    "no-value-primitive",
    "library-startup-list",

    "wrong-number-of-arguments",
    "not-callable",
    "unexpected-number-of-values",
    "undefined-message",
    "execute-thunk",
    "raise-handler",
    "notify-handler",
    "interactive-thunk",
    "exception-handler-symbol",
    "notify-handler-symbol",
    "sig-int-symbol",

    "dynamic-record-type",
    "continuation-record-type",

    "cleanup-tconc",

    "define-library-symbol",
    "import-symbol",
    "include-library-declarations-symbol",
    "cond-expand-symbol",
    "export-symbol",
    "begin-symbol",
    "include-symbol",
    "include-ci-symbol",
    "only-symbol",
    "except-symbol",
    "prefix-symbol",
    "rename-symbol",
    "aka-symbol",

    "datum-reference-record-type"
};

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
    "hash-tree",
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

        case HashTreeTag:
            from = "hash-tree.buckets";
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
        obj = AsSymbol(obj)->String;

    if (StringP(obj))
    {
        printf(" ");

        for (int_t idx = 0; idx < StringLength(obj); idx++)
            putc(AsString(obj)->String[idx], stdout);
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
            printf("%s[%lld]", from, idx);
        else
            printf("%s", from);

        idx = CheckStack[cdx + 1].Index;
        from = WhereFrom(CheckStack[cdx].Object, &idx);
        PrintObjectString(CheckStack[cdx].Object);

        if (CheckStack[cdx].Repeat > 1)
            printf(" (repeats %lld times)", CheckStack[cdx].Repeat);
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

static int_t CheckCount;
static int_t CheckTooDeep;

static int_t ValidAddress(FObjHdr * oh)
{
    uint_t len = ObjectLength(oh->Size());
    FThreadState * ts = Threads;
    void * strt = oh;
    void * end = ((char *) oh) + len;

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

    return(0);
}

static void CheckObject(FObject obj, int_t idx)
{
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
        if (CheckMarkP(obj))
            goto Done;
        SetCheckMark(obj);
        CheckCount += 1;

        FObjHdr * oh = AsObjHdr(obj);
        FMustBe(CheckMarkP(obj));
        FMustBe(ValidAddress(oh));
        FMustBe(IndirectTag(obj) > 0 && IndirectTag(obj) < BadDogTag);
        FMustBe(IndirectTag(obj) != FreeTag);
        FMustBe(oh->Size() >= oh->SlotCount() * sizeof(FObject));

#ifdef FOMENT_OBJFTR
        FObjFtr * of = AsObjFtr(oh);

        FMustBe(of->Feet[0] == OBJFTR_FEET);
        FMustBe(of->Feet[1] == OBJFTR_FEET);
#endif // FOMENT_OBJFTR

        if (PairP(obj))
        {
            CheckObject(AsPair(obj)->First, 0);

            FMustBe(CheckStackPtr > 0);

            if (CheckStackPtr > 1 && PairP(CheckStack[CheckStackPtr - 2].Object)
                    && CheckStack[CheckStackPtr - 2].Index == 1)
            {
                CheckStack[CheckStackPtr - 1].Repeat += 1;
                obj = AsPair(obj)->Rest;
                goto Again;
            }
            else
                CheckObject(AsPair(obj)->Rest, 1);
        }
        else if (AsObjHdr(obj)->SlotCount() > 0)
        {
            for (uint_t idx = 0; idx < AsObjHdr(obj)->SlotCount(); idx++)
                CheckObject(((FObject *) obj)[idx], idx);
        }
        break;
    }

    case FixnumTag: // 0x01
    case CharacterTag: // 0x02
    case MiscellaneousTag: // 0x03
    case SpecialSyntaxTag: // 0x04
    case InstructionTag: // 0x05
    case ValuesCountTag: // 0x06
        break;

    case UnusedTag: // 0x07
        PrintCheckStack();
        FMustBe(0);
        break;
    }

Done:
    FMustBe(CheckStackPtr > 0);
    CheckStackPtr -= 1;
}

static void CheckRoot(FObject obj, const char * from, int_t idx)
{
    CheckFrom = from;
    CheckObject(obj, idx);
}

static void CheckThreadState(FThreadState * ts)
{
    CheckRoot(ts->Thread, "thread-state.thread", -1);

    uint_t idx = 0;
    for (FAlive * ap = ts->AliveList; ap != 0; ap = ap->Next, idx++)
        CheckRoot(*ap->Pointer, "thread-state.alive-list", idx);

    for (uint_t rdx = 0; rdx < ts->RootsUsed; rdx++)
        CheckRoot(*ts->Roots[rdx], "thread-state.roots", rdx);

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

void CheckMemRegion(FMemRegion * mrgn, uint_t used, uint_t gen)
{
    FObjHdr * oh = (FObjHdr *) mrgn->Base;
    uint_t cnt = 0;

    while (cnt < used)
    {
        FMustBe(cnt + sizeof(FObjHdr) <= mrgn->BottomUsed);
        uint_t sz = oh->Size();
        uint_t len = ObjectLength(sz);

        FMustBe(len >= sz + sizeof(FObjHdr));
        FMustBe(len % OBJECT_ALIGNMENT == 0);

#ifdef FOMENT_OBJFTR
        FMustBe(len >= sz + sizeof(FObjHdr) + sizeof(FObjFtr));
#endif // FOMENT_OBJFTR

        FMustBe(cnt + len <= mrgn->BottomUsed);

#ifdef FOMENT_OBJFTR
        FObjFtr * of = AsObjFtr(oh);

        FMustBe(of->Feet[0] == OBJFTR_FEET);
        FMustBe(of->Feet[1] == OBJFTR_FEET);
#endif // FOMENT_OBJFTR

        FMustBe(oh->Generation() == gen);
        FMustBe(gen == OBJHDR_GEN_ADULTS || (oh->Flags & OBJHDR_MARK_FORWARD) == 0);
        FMustBe(oh->SlotCount() * sizeof(FObject) <= oh->Size());
        FMustBe(oh->Tag() > 0 && oh->Tag() < BadDogTag);
        FMustBe(oh->Tag() != FreeTag || oh->Generation() == OBJHDR_GEN_ADULTS);

        ClearCheckMark(oh);
        oh = (FObjHdr *) (((char *) oh) + len);
        cnt += len;
    }

}

void CheckHeap(const char * fn, int ln)
{
    EnterExclusive(&GCExclusive);

    FMustBe(sizeof(RootNames) == sizeof(FRoots));
    FMustBe(sizeof(IndirectTagString) / sizeof(char *) == BadDogTag);

    if (VerboseFlag)
        printf("CheckHeap: %s(%d)\n", fn, ln);
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

    FObject * rv = (FObject *) &R;
    for (uint_t rdx = 0; rdx < sizeof(FRoots) / sizeof(FObject); rdx++)
        CheckRoot(rv[rdx], RootNames[rdx], -1);

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

#if 0
    FTracker * track = YoungTrackers;
    while (track)
    {
        CheckRoot(track->Object, "tracker.object");
        CheckRoot(track->Return, "tracker.return");
        CheckRoot(track->TConc, "tracker.tconc");

        track = track->Next;
    }
#endif // 0
    
    if (CheckTooDeep > 0)
        printf("CheckHeap: %d object too deep to walk\n", (int) CheckTooDeep);
    if (VerboseFlag)
        printf("CheckHeap: %d active objects\n", (int) CheckCount);

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
//    uint_t gen = oh->Generation();

    FAssert(oh->Generation() == OBJHDR_GEN_ADULTS);

    if (MarkP(oh))
        return;

    SetMark(oh);

    uint_t sc = oh->SlotCount();
    if (sc > 0)
    {
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

            if (MarkP(AsObjHdr(grd->TConc)))
            {
                if (MarkP(AsObjHdr(grd->Object)))
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

            FAssert(MarkP(AsObjHdr(grd->TConc)));

            grd->Next = final;
            final = grd;
            LiveObject(&grd->Object);
            LiveObject(&grd->TConc);
        }
    }

    while (maybe)
    {
        FAssert(MarkP(AsObjHdr(maybe->Object)) == 0);
        FAssert(MarkP(AsObjHdr(maybe->TConc)) == 0);

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

    if (CheckHeapFlag)
        CheckHeap(__FILE__, __LINE__);

    FObjHdr * oh = (FObjHdr *) Adults.Base;
    while ((char *) oh < ((char *) Adults.Base) + AdultsUsed)
    {
        ClearMark(oh);
        oh = (FObjHdr *) (((char *) oh) + ObjectLength(oh->Size()));
    }

    FObject * rv = (FObject *) &R;
    for (uint_t rdx = 0; rdx < sizeof(FRoots) / sizeof(FObject); rdx++)
        LiveObject(rv + rdx);

    FThreadState * ts = Threads;
    while (ts != 0)
    {
        LiveObject(&ts->Thread);

        for (FAlive * ap = ts->AliveList; ap != 0; ap = ap->Next)
            LiveObject(ap->Pointer);

        for (uint_t rdx = 0; rdx < ts->RootsUsed; rdx++)
            LiveObject(ts->Roots[rdx]);

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
        ts = ts->Next;
    }

    FGuardian * final = CollectGuardians();
    
    // Trackers
    
    BigFreeAdults = 0;
    for (int_t idx = 0; idx < FREE_ADULTS; idx++)
        FreeAdults[idx] = 0;

    oh = (FObjHdr *) Adults.Base;
    while ((char *) oh < ((char *) Adults.Base) + AdultsUsed)
    {
        uint_t len = ObjectLength(oh->Size());
        if (MarkP(oh) == 0)
        {
            FObjHdr * noh = (FObjHdr *) (((char *) oh) + len);
            while ((char *) noh < ((char *) Adults.Base) + AdultsUsed)
            {
                if (MarkP(noh))
                    break;
                noh = (FObjHdr *) (((char *) noh) + ObjectLength(noh->Size()));
            }

            FAssert((char *) noh - (char *) oh >= len);

            len = (char *) noh - (char *) oh;
            uint_t bkt = len / OBJECT_ALIGNMENT;
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

            oh->FlagsAndSize = (len - OBJECT_HDRFTR_LENGTH) | OBJHDR_GEN_ADULTS;
            oh->TagAndSlotCount = FreeTag << OBJHDR_TAG_SHIFT;
        }
        oh = (FObjHdr *) (((char *) oh) + len);
    }

    while (final != 0)
    {
        FGuardian * grd = final;
        final = final->Next;

        TConcAdd(grd->TConc, grd->Object);
        free(grd);
    }

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
        EnterExclusive(&GCExclusive);

        FAssert(ObjectP(obj));
        FAssert(PairP(tconc));
        FAssert(PairP(First(tconc)));
        FAssert(PairP(Rest(tconc)));

        FGuardian * grd = (FGuardian *) malloc(sizeof(FGuardian));
        if (grd == 0)
            RaiseExceptionC(R.Assertion, "install-guardian", "out of memory", EmptyListObject);

        grd->Object = obj;
        grd->TConc = tconc;
        grd->Next = Guardians;
        Guardians = grd;
        LeaveExclusive(&GCExclusive);
    }
}

void InstallTracker(FObject obj, FObject ret, FObject tconc)
{
    if (CollectorType == GenerationalCollector)
    {
        
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
    ts->RootsUsed = 0;

    if (CollectorType == NoCollector || CollectorType == GenerationalCollector)
    {
        if (InitializeMemRegion(&ts->Babies, MaximumBabiesSize) == 0)
            goto Failed;
        if (GrowMemRegionUp(&ts->Babies, PAGE_SIZE * 8) == 0)
            goto Failed;
        ts->BabiesUsed = 0;
    }

    if (InitializeMemRegion(&ts->Stack, MaximumStackSize) == 0)
        goto Failed;

    if (GrowMemRegionUp(&ts->Stack, PAGE_SIZE) == 0
            || GrowMemRegionDown(&ts->Stack, PAGE_SIZE) == 0)
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
    FAssert(sizeof(FObjHdr) % OBJECT_ALIGNMENT == 0);
    FAssert(sizeof(FObjHdr) >= OBJECT_ALIGNMENT);
    FAssert(OBJHDR_SIZE_MASK >= OBJHDR_SLOT_COUNT_MASK * sizeof(FObject));
    FAssert(sizeof(FObject) <= OBJECT_ALIGNMENT);
    FAssert(Adults.Base == 0);
    FAssert(AdultsUsed == 0);

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
        MaximumBabiesSize = 1024 * 1024 * 128;
    if (CollectorType == GenerationalCollector && MaximumBabiesSize == 0)
        MaximumBabiesSize = 1024 * 1024 * 2;
    if (CollectorType == MarkSweepCollector || CollectorType == GenerationalCollector)
    {
        FAssert(BigFreeAdults == 0);

        if (InitializeMemRegion(&Adults, MaximumAdultsSize) == 0)
            return(0);
        if (GrowMemRegionUp(&Adults, PAGE_SIZE * 8) == 0)
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

    R.CleanupTConc = MakeTConc();

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

static FPrimitive * Primitives[] =
{
    &InstallGuardianPrimitive,
    &InstallTrackerPrimitive,
    &CollectPrimitive
};

void SetupGC()
{
    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}

#if 0
// Original Code

#define PAGE_SIZE 4096

#define MAXIMUM_OBJECT_LENGTH OBJHDR_SIZE_MASK
#define MAXIMUM_SLOT_COUNT OBJHDR_SLOT_COUNT_MASK

#define OBJECT_ALIGNMENT 8
const static uint_t Align[8] = {0, 7, 6, 5, 4, 3, 2, 1};

FCollectorType CollectorType = NullCollector;
uint_t MaximumStackSize = 1024 * 1024 * 4 * sizeof(FObject);
uint_t MaximumBabiesSize = 0;

volatile uint_t BytesAllocated = 0;
volatile int_t GCRequired;
OSExclusive GCExclusive;
volatile uint_t TotalThreads;
FThreadState * Threads;

#ifdef FOMENT_WINDOWS
unsigned int TlsIndex;
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
pthread_key_t ThreadKey;
#endif // FOMENT_UNIX

#define FOMENT_OBJFTR 1
#define OBJFTR_FEET 0xDEADFEE7

typedef struct
{
    uint32_t Feet[2];
} FObjFtr;

int_t MatureObjectP(FObject obj)
{
    return(0);
//    return(ObjectP(obj) && MatureP(obj));
}

static inline uint_t RoundToPageSize(uint_t cnt)
{
    if (cnt % PAGE_SIZE != 0)
    {
        cnt += PAGE_SIZE - (cnt % PAGE_SIZE);

        FAssert(cnt % PAGE_SIZE == 0);
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
    FAssert(mrgn->MaximumSize % PAGE_SIZE == 0);
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
    FAssert(mrgn->TopUsed % PAGE_SIZE == 0);
    FAssert(mrgn->BottomUsed % PAGE_SIZE == 0);
    FAssert(mrgn->MaximumSize % PAGE_SIZE == 0);
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
    FAssert(mrgn->TopUsed % PAGE_SIZE == 0);
    FAssert(mrgn->BottomUsed % PAGE_SIZE == 0);
    FAssert(mrgn->MaximumSize % PAGE_SIZE == 0);
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

static inline void SetCheckMark(FObject obj)
{
    AsObjHdr(obj)->CheckMark |= OBJHDR_CHECK_MARK;
}

static inline void ClearCheckMark(FObjHdr * oh)
{
    oh->CheckMark &= ~OBJHDR_CHECK_MARK;
}

static inline int CheckMarkP(FObject obj)
{
    return(AsObjHdr(obj)->CheckMark & OBJHDR_CHECK_MARK);
}

#if 0
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
    return(0);
//    return(SectionTable[SectionIndex(obj)] == MatureSectionTag);
}

static inline FSectionTag SectionTag(FObject obj)
{
    return((FSectionTag) SectionTable[SectionIndex(obj)]);
}

#define MAXIMUM_YOUNG_LENGTH (1024 * 4)

#define MakeLength(len, tag) (len)

static FYoungSection * GenerationZero;
static FYoungSection * GenerationOne;

typedef struct _FFreeObject
{
    uint_t Length;
    struct _FFreeObject * Next;
} FFreeObject;

static FFreeObject * FreeMature = 0;

static volatile uint_t BytesSinceLast = 0;
static uint_t ObjectsSinceLast = 0;
uint_t CollectionCount = 0;
static uint_t PartialCount = 0;
static uint_t PartialPerFull = 4;
static uint_t TriggerBytes = SECTION_SIZE * 8;
static uint_t TriggerObjects = TriggerBytes / (sizeof(FObject) * 16);
static uint_t MaximumBackRefFraction = 128;

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

inline int_t MarkP(FObject obj)
{
    FAssert(ObjectP(obj));

    return(AsObjHdr(obj)->Flags & OBJHDR_FLAG_ONE);
}

inline void SetMark(FObject obj)
{
    FAssert(ObjectP(obj));

    AsObjHdr(obj)->Flags |= OBJHDR_FLAG_ONE;
}

inline void ClearMark(FObject obj)
{
    FAssert(ObjectP(obj));

    AsObjHdr(obj)->Flags &= ~OBJHDR_FLAG_ONE;
}

static inline int_t AliveP(FObject obj)
{
    obj = AsRaw(obj);
    unsigned char st = SectionTable[SectionIndex(obj)];

    if (st == MatureSectionTag)
        return(MarkP(obj));
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

static OSCondition ReadyCondition;
static OSCondition DoneCondition;

static volatile int_t GCHappening;
static volatile uint_t WaitThreads;
static volatile uint_t CollectThreads;

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

static uint_t ObjectSize(FObject obj, uint_t tag, int ln)
{
    uint_t len;

    switch (tag)
    {
    case PairTag:
        FAssert(PairP(obj));

        len = sizeof(FPair);
        break;

    case RatioTag:
        FAssert(RatioP(obj));

        len = sizeof(FRatio);
        break;

    case ComplexTag:
        FAssert(ComplexP(obj));

        len = sizeof(FComplex);
        break;

    case FlonumTag:
        FAssert(FlonumP(obj));

        len = sizeof(FFlonum);
        break;

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
static FObject MakeObject(uint_t tag, uint_t sz, uint_t sc)
{
    FAssert(tag > 0);
    FAssert(tag < BadDogTag);
    FAssert(sz >= sizeof(FObject) * sc);

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

    FAssert((((uint_t) (obj)) & 0x7) == 0);

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
#endif // 0

FObject MakeObject(uint_t tag, uint_t sz, uint_t sc, const char * who, int_t pf)
{
    FAssert(tag > 0);
    FAssert(tag < BadDogTag);
    FAssert(sz >= sizeof(FObject) * sc);

    if (sz > MAXIMUM_OBJECT_LENGTH)
        RaiseExceptionC(R.Restriction, who, "object too big", EmptyListObject);
    if (sc > MAXIMUM_SLOT_COUNT)
        RaiseExceptionC(R.Restriction, who, "too many slots", EmptyListObject);

    uint_t len = sz;
    len += Align[len % OBJECT_ALIGNMENT];

    FAssert(len >= sz);
    FAssert(len % OBJECT_ALIGNMENT == 0);

    len += sizeof(FObjHdr);

    FAssert(len % OBJECT_ALIGNMENT == 0);

#ifdef FOMENT_OBJFTR
    len += sizeof(FObjFtr);

    FAssert(len % OBJECT_ALIGNMENT == 0);
#endif // FOMENT_OBJFTR

    FThreadState * ts = GetThreadState();

    if (ts->BabiesUsed + len > ts->Babies.BottomUsed)
    {
        if (ts->BabiesUsed + len > ts->Babies.MaximumSize)
            RaiseExceptionC(R.Assertion, who, "babies too small; increase maximum-babies-size",
                    EmptyListObject);

        uint_t gsz = PAGE_SIZE * 8;
        if (len > gsz)
            gsz = len;
        if (gsz > ts->Babies.MaximumSize - ts->Babies.BottomUsed)
            gsz = ts->Babies.MaximumSize - ts->Babies.BottomUsed;

        if (GrowMemRegionUp(&ts->Babies, ts->Babies.BottomUsed + gsz) == 0)
            RaiseExceptionC(R.Assertion, who, "babies: out of memory", EmptyListObject);
    }

    FObjHdr * oh = (FObjHdr *) (((char *) ts->Babies.Base) + ts->BabiesUsed);
    oh->FlagsAndSize = sz;
    oh->TagAndSlotCount = (tag << OBJHDR_TAG_SHIFT) | sc;

    ts->BabiesUsed += len;
    BytesAllocated += len;
    ts->ObjectsSinceLast += 1;

#ifdef FOMENT_OBJFTR
    FObjFtr * of = ((FObjFtr *) (((char *) oh) + len)) - 1;
    of->Feet[0] = OBJFTR_FEET;
    of->Feet[1] = OBJFTR_FEET;
#endif // FOMENT_OBJFTR

    return(oh + 1);
/*
    FObject obj = 0;

    if (pf == 0)
        obj = MakeObject(tag, sz, sc);
    if (obj == 0)
        obj = MakeMatureObject(tag, sz, sc, who);
    return(obj);
*/
}

void PushRoot(FObject * rt)
{
    FThreadState * ts = GetThreadState();

    ts->RootsUsed += 1;

    FAssert(ts->RootsUsed < sizeof(ts->Roots) / sizeof(FObject *));

    ts->Roots[ts->RootsUsed - 1] = rt;
}

void PopRoot()
{
    FThreadState * ts = GetThreadState();

    FAssert(ts->RootsUsed > 0);

    ts->RootsUsed -= 1;
}

void ClearRoots()
{
    FThreadState * ts = GetThreadState();

    ts->RootsUsed = 0;
}

#if 0
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
#endif // 0

void ModifyVector(FObject obj, uint_t idx, FObject val)
{
    FAssert(VectorP(obj));
    FAssert(idx < VectorLength(obj));

    AsVector(obj)->Vector[idx] = val;

#if 0
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
#endif // 0
}

void ModifyObject(FObject obj, uint_t off, FObject val)
{
    FAssert(ObjectP(obj));
    FAssert(off % sizeof(FObject) == 0);

    ((FObject *) obj)[off / sizeof(FObject)] = val;

#if 0
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
#endif // 0
}

void SetFirst(FObject obj, FObject val)
{
    FAssert(PairP(obj));

    AsPair(obj)->First = val;

#if 0
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
#endif // 0
}

void SetRest(FObject obj, FObject val)
{
    FAssert(PairP(obj));

    AsPair(obj)->Rest = val;

#if 0
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
#endif // 0
}

void SetBox(FObject bx, FObject val)
{
    FAssert(BoxP(bx));

    AsBox(bx)->Value = val;

#if 0
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
#endif // 0
}

#if 0
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
    else if (SectionTable[sdx] == ZeroSectionTag)
    {
        if (GCTagP(AsForward(raw)))
        {
            uint_t tag = AsValue(AsForward(raw));
            uint_t len = ObjectSize(raw, tag, __LINE__);
            FObject nobj = CopyObject(len, tag);
            memcpy(nobj, raw, len);

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
            FObject nobj = MakeMature(len);

            memcpy(nobj, raw, len);
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

            ScanChildren(obj, IndirectTag(obj), fcf);
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
#endif // 0

static const char * RootNames[] =
{
    "bedrock",
    "bedrock-library",
    "features",
    "command-line",
    "library-path",
    "library-extensions",
    "environment-variables",

    "symbol-hash-tree",

    "comparator-record-type",
    "anyp-primitive",
    "no-hash-primitive",
    "no-compare-primitive",
    "eq-comparator",
    "default-comparator",

    "hash-map-record-type",
    "hash-set-record-type",
    "hash-bag-record-type",
    "exception-record-type",

    "assertion",
    "restriction",
    "lexical",
    "syntax",
    "error",

    "loaded-libraries",

    "syntactic-env-record-type",
    "binding-record-type",
    "identifier-record-type",
    "lambda-record-type",
    "case-lambda-record-type",
    "inline-variable-record-type",
    "reference-record-type",

    "else-reference",
    "arrow-reference",
    "library-reference",
    "and-reference",
    "or-reference",
    "not-reference",
    "quasiquote-reference",
    "unquote-reference",
    "unquote-splicing-reference",
    "cons-reference",
    "append-reference",
    "list-to-vector-reference",
    "ellipsis-reference",
    "underscore-reference",

    "tag-symbol",
    "use-pass-symbol",
    "constant-pass-symbol",
    "analysis-pass-symbol",
    "interaction-env",

    "standard-input",
    "standard-output",
    "standard-error",
    "quote-symbol",
    "quasiquote-symbol",
    "unquote-symbol",
    "unquote-splicing-symbol",
    "file-error-symbol",
    "current-symbol",
    "end-symbol",

    "syntax-rules-record-type",
    "pattern-variable-record-type",
    "pattern-repeat-record-type",
    "template-repeat-record-type",
    "syntax-rule-record-type",

    "environment-record-type",
    "global-record-type",
    "library-record-type",
    "no-value-primitive",
    "library-startup-list",

    "wrong-number-of-arguments",
    "not-callable",
    "unexpected-number-of-values",
    "undefined-message",
    "execute-thunk",
    "raise-handler",
    "notify-handler",
    "interactive-thunk",
    "exception-handler-symbol",
    "notify-handler-symbol",
    "sig-int-symbol",

    "dynamic-record-type",
    "continuation-record-type",

    "cleanup-tconc",

    "define-library-symbol",
    "import-symbol",
    "include-library-declarations-symbol",
    "cond-expand-symbol",
    "export-symbol",
    "begin-symbol",
    "include-symbol",
    "include-ci-symbol",
    "only-symbol",
    "except-symbol",
    "prefix-symbol",
    "rename-symbol",
    "aka-symbol",

    "datum-reference-record-type"
};

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
    "hash-tree"
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

        case HashTreeTag:
            from = "hash-tree.buckets";
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
        obj = AsSymbol(obj)->String;

    if (StringP(obj))
    {
        printf(" ");

        for (int_t idx = 0; idx < StringLength(obj); idx++)
            putc(AsString(obj)->String[idx], stdout);
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
            printf("%s[%lld]", from, idx);
        else
            printf("%s", from);

        idx = CheckStack[cdx + 1].Index;
        from = WhereFrom(CheckStack[cdx].Object, &idx);
        PrintObjectString(CheckStack[cdx].Object);

        if (CheckStack[cdx].Repeat > 1)
            printf(" (repeats %lld times)", CheckStack[cdx].Repeat);
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

static int_t CheckCount;
static int_t CheckTooDeep;

static void CheckObject(FObject obj, int_t idx)
{
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
    switch (((FImmediate) (obj)) & 0x7)
    {
    case 0x00: // ObjectP(obj)
    {
        if (CheckMarkP(obj))
            goto Done;
        SetCheckMark(obj);
        CheckCount += 1;

        FMustBe(CheckMarkP(obj));

        
        
        // static inline uint_t ObjectLength(uint_t sz) and use everywhere
        // FObjFtr * AsObjFtr(FObjHdr * oh)
        // validate slot count, tags, etc
        
        

        if (PairP(obj))
        {
            CheckObject(AsPair(obj)->First, 0);

            FMustBe(CheckStackPtr > 0);

            if (CheckStackPtr > 1 && PairP(CheckStack[CheckStackPtr - 2].Object)
                    && CheckStack[CheckStackPtr - 2].Index == 1)
            {
                CheckStack[CheckStackPtr - 1].Repeat += 1;
                obj = AsPair(obj)->Rest;
                goto Again;
            }
            else
                CheckObject(AsPair(obj)->Rest, 1);
        }
        else if (AsObjHdr(obj)->SlotCount() > 0)
        {
            for (uint_t idx = 0; idx < AsObjHdr(obj)->SlotCount(); idx++)
                CheckObject(((FObject *) obj)[idx], idx);
        }
        break;
    }

    case UnusedTag1: // 0bxxxxx001
    case UnusedTag2: // 0bxxxxx010
        PrintCheckStack();
        FMustBe(0);
        break;

    case DoNotUse: // 0bxxxxx011
        switch (((FImmediate) (obj)) & 0x7F)
        {
        case CharacterTag: // 0bx0001011
        case MiscellaneousTag: // 0bx0011011
        case SpecialSyntaxTag: // 0bx0101011
        case InstructionTag: // 0bx0111011
        case ValuesCountTag: // 0bx1001011
            break;

        case UnusedTag6: // 0bx1011011
        case UnusedTag7: // 0bx1101011
        case UnusedTag8: // 0bx1111011
            PrintCheckStack();
            FMustBe(0);
            break;
        }
        break;

    case UnusedTag3: // 0bxxxxx100
    case UnusedTag4: // 0bxxxxx101
    case UnusedTag5: // 0bxxxxx110
        PrintCheckStack();
        FMustBe(0);
        break;

    case FixnumTag: // 0bxxxx0111
        break;
    }

Done:
    FMustBe(CheckStackPtr > 0);
    CheckStackPtr -= 1;
}

static void CheckRoot(FObject obj, const char * from, int_t idx)
{
    CheckFrom = from;
    CheckObject(obj, idx);
}

static void CheckThreadState(FThreadState * ts)
{
    CheckRoot(ts->Thread, "thread-state.thread", -1);

    uint_t idx = 0;
    for (FAlive * ap = ts->AliveList; ap != 0; ap = ap->Next, idx++)
        CheckRoot(*ap->Pointer, "thread-state.alive-list", idx);

    for (uint_t rdx = 0; rdx < ts->RootsUsed; rdx++)
        CheckRoot(*ts->Roots[rdx], "thread-state.roots", rdx);

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

void CheckMemRegion(FMemRegion * mrgn, uint_t used, uint32_t gen)
{
    FObjHdr * oh = (FObjHdr *) mrgn->Base;
    uint_t cnt = 0;

    while (cnt < used)
    {
        FMustBe(cnt + sizeof(FObjHdr) <= mrgn->BottomUsed);
        uint_t sz = oh->Size();
        uint_t len = sz;
        len += Align[len % OBJECT_ALIGNMENT];

        FMustBe(len >= sz);
        FMustBe(len % OBJECT_ALIGNMENT == 0);

        len += sizeof(FObjHdr);

        FMustBe(len % OBJECT_ALIGNMENT == 0);

#ifdef FOMENT_OBJFTR
        len += sizeof(FObjFtr);

        FMustBe(len % OBJECT_ALIGNMENT == 0);
#endif // FOMENT_OBJFTR

        FMustBe(cnt + len <= mrgn->BottomUsed);

#ifdef FOMENT_OBJFTR
        FObjFtr * of = ((FObjFtr *) (((char *) oh) + len)) - 1;

        FMustBe(of->Feet[0] == OBJFTR_FEET);
        FMustBe(of->Feet[1] == OBJFTR_FEET);
#endif // FOMENT_OBJFTR

        FMustBe(oh->Generation() == gen);
        FMustBe(gen == OBJHDR_GEN_ADULTS || (oh->Flags & OBJHDR_MARK_FORWARD) == 0);
        FMustBe(oh->SlotCount() * sizeof(FObject) <= oh->Size());
        FMustBe(oh->Tag() > 0 && oh->Tag() < BadDogTag);

        ClearCheckMark(oh);
        oh = (FObjHdr *) (((char *) oh) + len);
        cnt += len;
    }
}

void CheckHeap(const char * fn, int ln)
{
    FThreadState * ts;

    FMustBe(sizeof(RootNames) == sizeof(FRoots));

    if (VerboseFlag)
        printf("CheckHeap: %s(%d)\n", fn, ln);
    CheckCount = 0;
    CheckTooDeep = 0;
    CheckStackPtr = 0;

    ts = Threads;
    while (ts != 0)
    {
        CheckMemRegion(&ts->Babies, ts->BabiesUsed, OBJHDR_GEN_BABIES);
        ts = ts->Next;
    }

    FObject * rv = (FObject *) &R;
    for (uint_t rdx = 0; rdx < sizeof(FRoots) / sizeof(FObject); rdx++)
        CheckRoot(rv[rdx], RootNames[rdx], -1);

    ts = Threads;
    while (ts != 0)
    {
        CheckThreadState(ts);
        ts = ts->Next;
    }

#if 0
    FTracker * track = YoungTrackers;
    while (track)
    {
        CheckRoot(track->Object, "tracker.object");
        CheckRoot(track->Return, "tracker.return");
        CheckRoot(track->TConc, "tracker.tconc");

        track = track->Next;
    }

    FGuardian * guard = YoungGuardians;
    while (guard)
    {
        CheckRoot(guard->Object, "guardian.object");
        CheckRoot(guard->TConc, "guardian.tconc");

        guard = guard->Next;
    }

    guard = MatureGuardians;
    while (guard)
    {
        CheckRoot(guard->Object, "guardian.object");
        CheckRoot(guard->TConc, "guardian.tconc");

        guard = guard->Next;
    }
#endif // 0
    
    if (CheckTooDeep > 0)
        printf("CheckHeap: %d object too deep to walk\n", (int) CheckTooDeep);
    if (VerboseFlag)
        printf("CheckHeap: %d active objects\n", (int) CheckCount);
}

#if 0
static void Collect(int_t fcf)
{
    if (VerboseFlag)
    {
        if (fcf)
            printf("Full Collection...\n");
        else
            printf("Partial Collection...\n");
    }

    if (CheckHeapFlag)
        CheckHeap(__FILE__, __LINE__);

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

        for (uint_t rdx = 0; rdx < ts->RootsUsed; rdx++)
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

    if (CheckHeapFlag)
        CheckHeap(__FILE__, __LINE__);
    if (VerboseFlag)
        printf("Collection Done.\n");
}

void FailedGC()
{
    if (GCHappening == 0)
        CheckHeap(__FILE__, __LINE__);
    printf("SectionTable: %p\n", SectionTable);
}
#endif // 0

void EnterWait()
{
#if 0
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
#endif // 0
}

void LeaveWait()
{
#if 0
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
#endif // 0
}

void Collect()
{
#if 0
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
#endif // 0
}

void InstallGuardian(FObject obj, FObject tconc)
{
#if 0
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
#endif // 0
}

void InstallTracker(FObject obj, FObject ret, FObject tconc)
{
#if 0
    EnterExclusive(&GCExclusive);

    FAssert(ObjectP(obj));
    FAssert(PairP(tconc));
    FAssert(PairP(First(tconc)));
    FAssert(PairP(Rest(tconc)));

    if (MatureP(obj) == 0)
        YoungTrackers = MakeTracker(YoungTrackers, obj, ret, tconc);

    LeaveExclusive(&GCExclusive);
#endif // 0
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
    ts->RootsUsed = 0;

    if (CollectorType == NullCollector || CollectorType == GenerationalCollector)
    {
        if (InitializeMemRegion(&ts->Babies, MaximumBabiesSize) == 0)
            goto Failed;
        if (GrowMemRegionUp(&ts->Babies, PAGE_SIZE * 8) == 0)
            goto Failed;
        ts->BabiesUsed = 0;
    }

    if (InitializeMemRegion(&ts->Stack, MaximumStackSize) == 0)
        goto Failed;

    if (GrowMemRegionUp(&ts->Stack, PAGE_SIZE) == 0
            || GrowMemRegionDown(&ts->Stack, PAGE_SIZE) == 0)
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

    FAssert(ts->Stack.Base != 0);
    DeleteMemRegion(&ts->Stack);

    ts->AStack = 0;
    ts->CStack = 0;
    ts->Thread = NoValueObject;

#if 0
    if (ts->ActiveZero != 0)
    {
        ts->ActiveZero->Next = GenerationZero;
        GenerationZero = ts->ActiveZero;
        ts->ActiveZero = 0;
    }
#endif // 0

    LeaveExclusive(&GCExclusive);
#if 0
    WakeCondition(&ReadyCondition); // Just in case a collection is pending.
#endif // 0

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
    FAssert(sizeof(FObjHdr) % OBJECT_ALIGNMENT == 0);
    FAssert(sizeof(FObjHdr) >= OBJECT_ALIGNMENT);
    FAssert(OBJHDR_SIZE_MASK >= OBJHDR_SLOT_COUNT_MASK * sizeof(FObject));

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

    if (CollectorType == NullCollector && MaximumBabiesSize == 0)
        MaximumBabiesSize = 1024 * 1024 * 128;
    if (CollectorType == GenerationalCollector && MaximumBabiesSize == 0)
        MaximumBabiesSize = 1024 * 1024 * 2;

#ifdef FOMENT_WINDOWS
#if 0
    SectionTable = (unsigned char *) VirtualAlloc(SectionTableBase, SECTION_SIZE * MaximumSections,
            MEM_RESERVE, PAGE_READWRITE);

    if (SectionTable == 0)
        return(0);

    VirtualAlloc(SectionTable, MaximumSections, MEM_COMMIT, PAGE_READWRITE);
#endif // 0

    TlsIndex = TlsAlloc();
    FAssert(TlsIndex != TLS_OUT_OF_INDEXES);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
#if 0
    SectionTable = (unsigned char *) mmap(SectionTableBase, (SECTION_SIZE + 1) * MaximumSections,
            PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS |
            (SectionTableBase == 0 ? 0 : MAP_FIXED), -1 ,0);

    if (SectionTable == 0)
        return(0);

    if (SectionTable != SectionBase(SectionTable))
        SectionTable += (SECTION_SIZE - SectionOffset(SectionTable));

    FAssert(SectionTable != 0);

    mprotect(SectionTable, MaximumSections, PROT_READ | PROT_WRITE);
#endif // 0

    pthread_key_create(&ThreadKey, 0);
#endif // FOMENT_UNIX

#if 0
    if (SectionTableBase != 0)
        printf("SectionTable: %p\n", SectionTable);

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
#endif // 0

    InitializeExclusive(&GCExclusive);

#if 0
    InitializeCondition(&ReadyCondition);
    InitializeCondition(&DoneCondition);
#endif // 0

    TotalThreads = 1;

#if 0
    WaitThreads = 0;
    Threads = 0;
    GCRequired = 1;
    GCHappening = 0;
    FullGCRequired = 0;
#endif // 0

#ifdef FOMENT_WINDOWS
    HANDLE h = OpenThread(STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | 0x3FF, 0,
            GetCurrentThreadId());
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    pthread_t h = pthread_self();
#endif // FOMENT_UNIX

#if 0
    for (uint_t idx = 0; idx < sizeof(Sizes) / sizeof(uint_t); idx++)
        Sizes[idx] = 0;
#endif // 0

    if (EnterThread(ts, NoValueObject, NoValueObject, NoValueObject) == 0)
        return(0);
    ts->Thread = MakeThread(h, NoValueObject, NoValueObject, NoValueObject);

    R.CleanupTConc = MakeTConc();

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

    EnterExclusive(&GCExclusive);
    GCRequired = 1;
#if 0
    if (argc > 0 && argv[0] != FalseObject)
        FullGCRequired = 1;
#endif // 0
    LeaveExclusive(&GCExclusive);

    CheckForGC();
    return(NoValueObject);
}

#if 0
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
#endif // 0

static FPrimitive * Primitives[] =
{
    &InstallGuardianPrimitive,
    &InstallTrackerPrimitive,
    &CollectPrimitive
#if 0
    &PartialPerFullPrimitive,
    &TriggerBytesPrimitive,
    &TriggerObjectsPrimitive,
    &DumpGCPrimitive
#endif // 0
};

void SetupGC()
{
    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
#endif // 0
