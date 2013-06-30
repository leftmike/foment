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

6 reserve bits
1024 = 10 bits
2048
4096 = 12 bits

-- guardians: make-guardian, install-guardian
-- trackers: make-tracker, install-tracker
-- get rid of Hash in Symbols, Records, and RecordTypes
-- merge some of the fields in FProcedure into Reserved
-- gc primitives: collect, <when to collect partial and full>, dump heap
-- fail gracefully if run out of BackRef space: just force a full collection next time
-- growing Scan stack, maybe should be a segment
-- ExecuteState and stacks should be segments
-- fix EqHash to work with objects being moved by gc: use trackers
-- mark collector: mark-compact always full collection
*/

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "foment.hpp"
#include "execute.hpp"
#include "io.hpp"

typedef void * FRaw;
#define ObjectP(obj) ((((FImmediate) (obj)) & 0x3) != 0x3)
#define AsRaw(obj) ((FRaw) (((unsigned int) (obj)) & ~0x3))

typedef enum
{
    HoleSectionTag,
    FreeSectionTag,
    TableSectionTag,
    ZeroSectionTag, // Generation Zero
    OneSectionTag, // Generation One
    MatureSectionTag,
    PairSectionTag
} FSectionTag;

static unsigned char * SectionTable;
static unsigned int UsedSections;

#define SECTION_SIZE 1024 * 16
#define SectionIndex(ptr) ((((unsigned int) (ptr)) - ((unsigned int) SectionTable)) >> 14)
#define SectionPointer(sdx) ((unsigned char *) (((sdx) << 14) + ((unsigned int) SectionTable)))
#define SectionOffset(ptr) (((unsigned int) (ptr)) & 0x3FFF)
#define SectionBase(ptr) ((unsigned char *) (((unsigned int) (ptr)) & ~0x3FFF))

#define MatureP(obj) (SectionTable[SectionIndex(obj)] == MatureSectionTag)
#define MaturePairP(obj) (SectionTable[SectionIndex(obj)] == PairSectionTag)

#define MAXIMUM_YOUNG_LENGTH 1024 * 4

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
unsigned int CollectionCount = 0;
unsigned int PartialPerFull = 4;
unsigned int TriggerBytes = SECTION_SIZE * 4;

static int UsedRoots = 0;
static FObject * Roots[128];

static FExecuteState * ExecuteState = 0;
int GCRequired = 1;

#define GCZeroP(obj) ImmediateP(obj, GCZeroTag)
#define GCOneP(obj) ImmediateP(obj, GCOneTag)

#define Forward(obj) (*(((FObject *) (obj)) - 1))

static FObject ScanStack[1024];
static int ScanIndex;

#define MarkP(obj) (*((unsigned int *) (obj)) & RESERVED_MARK_BIT)
#define SetMark(obj) *((unsigned int *) (obj)) |= RESERVED_MARK_BIT
#define ClearMark(obj) *((unsigned int *) (obj)) &= ~RESERVED_MARK_BIT

typedef struct
{
    FObject * Ref;
    FObject Value;
} FBackRef;

static FBackRef BackRef[1024 * 4];
static int BackRefCount;

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
    if (BytesSinceLast > TriggerBytes)
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

    Forward(obj) = MakeImmediate(tag, GCZeroTag);

    FAssert(AsValue(*pobj) == tag);
    FAssert(GCZeroP(*pobj));

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

    Forward(obj) = MakeImmediate(tag, GCOneTag);

    FAssert(AsValue(*pobj) == tag);
    FAssert(GCOneP(*pobj));

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

static void RecordBackRef(FObject * ref, FObject val)
{
    FAssert(*ref == val);

    BackRef[BackRefCount].Ref = ref;
    BackRef[BackRefCount].Value = val;
    BackRefCount += 1;

    FAssert(BackRefCount < sizeof(BackRef) / sizeof(FBackRef));
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

static void ScanObject(FObject * pobj, int fcf, int mf)
{
    FObject raw = AsRaw(*pobj);
    unsigned int sdx = SectionIndex(raw);

    FAssert(sdx < UsedSections);

    if (SectionTable[sdx] == MatureSectionTag)
    {
        if (fcf && MarkP(raw) == 0)
        {
            SetMark(raw);

            FAssert(ScanIndex < sizeof(ScanStack) / sizeof(FObject));

            ScanStack[ScanIndex] = *pobj;
            ScanIndex += 1;
        }
    }
    else if (SectionTable[sdx] == PairSectionTag)
    {
        if (fcf && PairMarkP(raw) == 0)
        {
            SetPairMark(raw);

            FAssert(ScanIndex < sizeof(ScanStack) / sizeof(FObject));

            ScanStack[ScanIndex] = *pobj;
            ScanIndex += 1;
        }
    }
    else if (GCZeroP(Forward(raw)))
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
    else if (GCOneP(Forward(raw)))
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
//        else if (tag == FlonumTag)
//            nobj = FlonumObject(nobj);
        else
            SetMark(nobj);

        FAssert(ScanIndex < sizeof(ScanStack) / sizeof(FObject));

        ScanStack[ScanIndex] = nobj;
        ScanIndex += 1;

        Forward(raw) = nobj;
        *pobj = nobj;
    }
/*    else if (GCZeroOneP(Forward(raw)))
    {
        unsigned int tag = AsValue(Forward(raw));
        unsigned int len = ObjectSize(raw, tag);
        FObject nobj = GCZeroP(Forward(raw)) ? CopyObject(len, tag) :
            (tag == PairTag ? MakeMaturePair() : MakeMature(len));
//            MakeMature(len);
        memcpy(nobj, raw, len);

        if (tag == PairTag)
            nobj = PairObject(nobj);
        else if (tag == FlonumTag)
            nobj = FlonumObject(nobj);

        if (GCOneP(Forward(raw)))
        {
            FAssert(ScanIndex < sizeof(ScanStack) / sizeof(FObject));

            ScanStack[ScanIndex] = nobj;
            ScanIndex += 1;
        }

        Forward(raw) = nobj;
        *pobj = nobj;
    }*/
    else
        *pobj = Forward(raw);
}

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
            if (ObjectP(AsVector(raw)->Vector[vdx]))
                ScanObject(AsVector(raw)->Vector + vdx, fcf, mf);
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

static void Collect(int fcf)
{
    if (fcf)
printf("Full Collection...");
    else
printf("Partial Collection...");

    ScanIndex = 0;

    ActiveZero->Next = GenerationZero;
    GenerationZero = ActiveZero;
    ActiveZero = 0;

    FYoungSection * go = GenerationOne;
    GenerationOne = AllocateYoung(0, OneSectionTag);

    if (fcf)
    {
        BackRefCount = 0;

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
        for (int idx = 0; idx < BackRefCount; idx++)
        {
            FAssert(ObjectP(*BackRef[idx].Ref));

            if (*BackRef[idx].Ref == BackRef[idx].Value)
            {
                ScanObject(BackRef[idx].Ref, fcf, 0);
                BackRef[idx].Value = *BackRef[idx].Ref;
            }
        }
    }

    for (;;)
    {
        while (ScanIndex > 0)
        {
            ScanIndex -= 1;
            FObject obj = ScanStack[ScanIndex];

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

                FAssert(GCOneP(*pobj));

                unsigned int tag = AsValue(*pobj);
                FObject obj = (FObject) (pobj + 1);

                ScanChildren(obj, tag, fcf);
                ys->Scan += ObjectSize(obj, tag) + sizeof(FObject);
            }

            ys = ys->Next;
        }

        if (GenerationOne->Scan == GenerationOne->Used && ScanIndex == 0)
            break;
    }

    ActiveZero = AllocateYoung(0, ZeroSectionTag);

    while (GenerationZero != 0)
    {
        FYoungSection * ys = GenerationZero;
        GenerationZero = GenerationZero->Next;
        FreeSection(ys);
    }

    while (go != 0)
    {
        FYoungSection * ys = go;
        go = go->Next;
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

printf("Done.\n");
}

void Collect()
{
    CollectionCount += 1;

    FAssert(GCRequired != 0);
    GCRequired = 0;
    BytesSinceLast = 0;

    Collect(CollectionCount % PartialPerFull == 0);
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
    FAssert(sizeof(FRecordType) + sizeof(FObject) * (MAXIMUM_RECORD_FIELDS - 1)
            <= MAXIMUM_YOUNG_LENGTH);

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

    BackRefCount = 0;

    for (unsigned int idx = 0; idx < sizeof(Sizes) / sizeof(unsigned int); idx++)
        Sizes[idx] = 0;
}


Define("install-guardian", InstallGuardianPrimitive)(int argc, FObject argv[])
{
    if (argc != 2)
        RaiseExceptionC(R.Assertion, "install-guardian",
                "install-guardian: expected two arguments", EmptyListObject);

    if (PairP(argv[1]) == 0 || PairP(First(argv[1])) == 0 || PairP(Rest(argv[1])) == 0)
        RaiseExceptionC(R.Assertion, "install-guardian",
                "install-guardian: expected a tconc", List(argv[1]));

    
    
    MakePair(argv[0], argv[1]);
    
    
    return(NoValueObject);
}

static FPrimitive * Primitives[] =
{
    &InstallGuardianPrimitive
};

void SetupMM()
{
    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}

void DumpSizes()
{
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
}
