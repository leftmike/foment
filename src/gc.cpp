/*

Foment

Garbage Collection:
-- C code does not have to worry about it if it doesn't want to, other than Modify
-- #define AllowGC() if (GCRequired) ReadyToGC(<this thread>)
-- StartAllowGC() and StopAllowGC() to denote a block of code where GC can occur; typically
    around a potentially lengthy IO operation
-- Root(obj) used to tell GC about a temporary root object; exceptions and return need to
    stop rooting objects; rooting extra objects is ok, if it simplifies the C code
-- generational collector
-- copying collector for 1st generation
-- mark-release or mark-compact for 2nd generation
-- each thread has an allocation block for the 1st generation: no syncronization necessary to
    allocate an object
-- all object modifications need to check to see if a older generation is now pointing to a younger
    generation
-- GC does not occur during an allocation, only at AllowGC points for all threads

-- after full gc, mature sections and pair sections are scanned for free objects
-- sizes of objects
-- large objects
-- scan mature and free unmarked objects
-- merge some of the fields in FProcedure into Reserved
-- merge Hash into Reserved in FSymbol
-- make room in NumFields for a Hash in Record and RecordType
-- partial collections
-- gc primitives

-- fix EqHash to work with objects being moved by gc
-- mark collector: mark-compact always full collection
*/

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "foment.hpp"
#include "execute.hpp"
#include "io.hpp"

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
#define SectionPointer(sdx) ((void *) (((sdx) << 14) + ((unsigned int ) SectionTable)))
#define SectionOffset(ptr) (((unsigned int) (ptr)) & 0x3FFF)

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

#define PairMarkP(ps, so)\
    (((((char *) (ps)) + PAIR_MB_OFFSET)[so / (sizeof(FPair) * 8)])\
    & (1 << ((so / sizeof(FPair)) % 8)))
#define SetPairMark(ps, so)\
    (((char *) (ps)) + PAIR_MB_OFFSET)[so / (sizeof(FPair) * 8)]\
    |= (1 << ((so / sizeof(FPair)) % 8))

unsigned int BytesAllocated;
unsigned int CollectionCount;

static int UsedRoots = 0;
static FObject * Roots[128];

static FExecuteState * ExecuteState = 0;
int GCRequired = 1;

#define GCZeroP(obj) ImmediateP(obj, GCZeroTag)
#define GCOneP(obj) ImmediateP(obj, GCOneTag)
#define GCZeroOneP(obj) ((((FImmediate) (obj)) & 0xF) == 0xB)

#define Forward(obj) (*(((FObject *) (obj)) - 1))

typedef void * FRaw;
#define ObjectP(obj) ((((FImmediate) (obj)) & 0x3) != 0x3)
#define AsRaw(obj) ((FRaw) (((unsigned int) (obj)) & ~0x3))

static FObject ScanStack[1024];
static int ScanIndex;

#define MarkP(obj) (*((unsigned int *) (obj)) & RESERVED_MARK_BIT)
#define SetMark(obj) *((unsigned int *) (obj)) |= RESERVED_MARK_BIT
#define ClearMark(obj) *((unsigned int *) (obj)) &= ~RESERVED_MARK_BIT

static unsigned int Sizes[1024 * 8];

static void * AllocateSection(unsigned int cnt, FSectionTag tag)
{
    unsigned int sdx;

    if (cnt == 1)
    {
        for (sdx = 0; sdx < UsedSections; sdx++)
            if (SectionTable[sdx] == FreeSectionTag)
            {
                SectionTable[sdx] = tag;
                return(SectionPointer(sdx));
            }
    }

    FAssert(cnt > 0);

    sdx = UsedSections;
    UsedSections += cnt;

    FAssert(UsedSections <= SECTION_SIZE);

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

    FAssert(len < sizeof(Sizes) / sizeof(unsigned int));
    Sizes[len] += 1;

    BytesAllocated += len;

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
FObject CopyObject(unsigned int len, unsigned int tag)
{
    FAssert(len % 4 == 0);
    FAssert(len >= sizeof(FObject));

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

FObject MakeMature(unsigned int len, unsigned int tag)
{
    FAssert(len % 4 == 0);

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

    fo = (FFreeObject *) AllocateSection(4, MatureSectionTag);
    fo->Next = 0;
    fo->Length = MakeLength(SECTION_SIZE * 4 - len, GCFreeTag);
    FreeMature = fo;
    return(((char *) FreeMature) + ByteLength(fo));
}

FObject MakeMaturePair()
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

void ModifyVector(FObject obj, unsigned int idx, FObject val)
{
    FAssert(VectorP(obj));
    FAssert(idx < VectorLength(obj));

    AsVector(obj)->Vector[idx] = val;
    
    
    
}

void ModifyObject(FObject obj, int off, FObject val)
{
    FAssert(IndirectP(obj));
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

static void ScanObject(FObject * pobj)
{
    FObject raw = AsRaw(*pobj);
    unsigned int sdx = SectionIndex(raw);

    if (SectionTable[sdx] == MatureSectionTag)
    {
        if (MarkP(raw) == 0)
        {
            SetMark(raw);

            FAssert(ScanIndex < sizeof(ScanStack) / sizeof(FObject));

            ScanStack[ScanIndex] = *pobj;
            ScanIndex += 1;
        }
    }
    else if (SectionTable[sdx] == PairSectionTag)
    {
        if (PairMarkP(SectionPointer(sdx), SectionOffset(raw)) == 0)
        {
            SetPairMark(SectionPointer(sdx), SectionOffset(raw));

            FAssert(ScanIndex < sizeof(ScanStack) / sizeof(FObject));

            ScanStack[ScanIndex] = *pobj;
            ScanIndex += 1;
        }
    }
    else if (GCZeroOneP(Forward(raw)))
    {
        unsigned int tag = AsValue(Forward(raw));
        unsigned int len = ObjectSize(raw, tag);
        FObject nobj = GCZeroP(Forward(raw)) ? CopyObject(len, tag) :
            (tag == PairTag ? MakeMaturePair() : MakeMature(len, tag));
//            MakeMature(len, tag);
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
    }
    else
        *pobj = Forward(raw);
}

static void ScanChildren(FRaw raw, unsigned int tag)
{
    switch (tag)
    {
    case PairTag:
    {
        FPair * pr = (FPair *) raw;

        if (ObjectP(pr->First))
            ScanObject(&pr->First);
        if (ObjectP(pr->Rest))
            ScanObject(&pr->Rest);
        break;
    }

    case BoxTag:
        if (ObjectP(AsBox(raw)->Value))
            ScanObject(&(AsBox(raw)->Value));
        break;

    case StringTag:
        break;

    case VectorTag:
        for (unsigned int vdx = 0; vdx < VectorLength(raw); vdx++)
            if (ObjectP(AsVector(raw)->Vector[vdx]))
                ScanObject(AsVector(raw)->Vector + vdx);
        break;

    case BytevectorTag:
        break;

    case PortTag:
        if (ObjectP(AsPort(raw)->Name))
            ScanObject(&(AsPort(raw)->Name));
        if (ObjectP(AsPort(raw)->Object))
            ScanObject(&(AsPort(raw)->Object));
        break;

    case ProcedureTag:
        if (ObjectP(AsProcedure(raw)->Name))
            ScanObject(&(AsProcedure(raw)->Name));
        if (ObjectP(AsProcedure(raw)->Code))
            ScanObject(&(AsProcedure(raw)->Code));
        if (ObjectP(AsProcedure(raw)->RestArg))
            ScanObject(&(AsProcedure(raw)->RestArg));
        break;

    case SymbolTag:
        if (ObjectP(AsSymbol(raw)->String))
            ScanObject(&(AsSymbol(raw)->String));
        if (ObjectP(AsSymbol(raw)->Hash))
            ScanObject(&(AsSymbol(raw)->Hash));
        break;

    case RecordTypeTag:
        for (unsigned int fdx = 0; fdx < RecordTypeNumFields(raw); fdx++)
            if (ObjectP(AsRecordType(raw)->Fields[fdx]))
                ScanObject(AsRecordType(raw)->Fields + fdx);
        break;

    case RecordTag:
        for (unsigned int fdx = 0; fdx < RecordNumFields(raw); fdx++)
            if (ObjectP(AsGenericRecord(raw)->Fields[fdx]))
                ScanObject(AsGenericRecord(raw)->Fields + fdx);
        break;

    case PrimitiveTag:
        break;

    default:
        FAssert(0);
    }
}

void Collect()
{
//printf("Collecting...");
    CollectionCount += 1;

    ScanIndex = 0;

    ActiveZero->Next = GenerationZero;
    GenerationZero = ActiveZero;
    ActiveZero = 0;

    FYoungSection * go = GenerationOne;
    GenerationOne = AllocateYoung(0, OneSectionTag);

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

    FObject * rv = (FObject *) &R;
    for (int rdx = 0; rdx < sizeof(FRoots) / sizeof(FObject); rdx++)
        if (ObjectP(rv[rdx]))
            ScanObject(rv + rdx);

    for (int rdx = 0; rdx < UsedRoots; rdx++)
        if (ObjectP(*Roots[rdx]))
            ScanObject(Roots[rdx]);

    if (ExecuteState != 0)
    {
        for (int adx = 0; adx < ExecuteState->AStackPtr; adx++)
            if (ObjectP(ExecuteState->AStack[adx]))
                ScanObject(ExecuteState->AStack + adx);

        for (int cdx = 0; cdx < ExecuteState->CStackPtr; cdx++)
            if (ObjectP(ExecuteState->CStack[cdx]))
                ScanObject(ExecuteState->CStack + cdx);

        if (ObjectP(ExecuteState->Proc))
            ScanObject(&ExecuteState->Proc);
        if (ObjectP(ExecuteState->Frame))
            ScanObject(&ExecuteState->Frame);
    }

    for (;;)
    {
        while (ScanIndex > 0)
        {
            ScanIndex -= 1;
            FObject obj = ScanStack[ScanIndex];

            FAssert(ObjectP(obj));

            if (PairP(obj))
                ScanChildren(AsRaw(obj), PairTag);
            else if (FlonumP(obj))
                ScanChildren(AsRaw(obj), FlonumTag);
            else
            {
                FAssert(IndirectP(obj));

                ScanChildren(obj, IndirectTag(obj));
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

                ScanChildren(obj, tag);
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

//printf("Done.\n");
}

void SetupGC()
{
    FAssert(sizeof(FObject) == sizeof(FImmediate));
    FAssert(sizeof(FObject) == sizeof(char *));
    FAssert(sizeof(FFixnum) <= sizeof(FImmediate));
    FAssert(sizeof(FCh) <= sizeof(FImmediate));

    FAssert(SECTION_SIZE == 1024 * 16);
    FAssert(PAIR_MB_OFFSET / (sizeof(FPair) * 8) <= PAIR_MB_SIZE);

    BytesAllocated = 0;
    CollectionCount = 0;

    SectionTable = (unsigned char *) VirtualAlloc(0, SECTION_SIZE * SECTION_SIZE, MEM_RESERVE,
            PAGE_READWRITE);
    FAssert(SectionTable != 0);

    VirtualAlloc(SectionTable, SECTION_SIZE, MEM_COMMIT, PAGE_READWRITE);

    unsigned int sdx;
    for (sdx = 0; sdx < SECTION_SIZE; sdx++)
        SectionTable[sdx] = HoleSectionTag;

    FAssert(SectionIndex(SectionTable) == 0);

    UsedSections = 1;

    SectionTable[0] = TableSectionTag;

    ActiveZero = AllocateYoung(0, ZeroSectionTag);

    for (unsigned int idx = 0; idx < sizeof(Sizes) / sizeof(unsigned int); idx++)
        Sizes[idx] = 0;
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
}

#if 0
typedef struct
{
    unsigned short Hash;
    unsigned char Tag;
    unsigned char GCFlags;
} FObjectHeader;

#define AsObjectHeader(obj) (((FObjectHeader *) (((FImmediate) (obj)) & ~0x3)) - 1)

// GCFlags in FObjectHeader

#define GCAGEMASK  0x0F
#define GCMODIFIED 0x10
#define GCMARK     0x20
#define GCMATURE   0x40
#define GCFORWARD  0x80

#define SetAge(obj, age)\
    AsObjectHeader(obj)->GCFlags = ((AsObjectHeader(obj)->GCFlags & ~GCAGEMASK) | (age))
#define GetAge(obj) (AsObjectHeader(obj)->GCFlags & GCAGEMASK)

#define SetModified(obj) AsObjectHeader(obj)->GCFlags |= GCMODIFIED
#define ClearModified(obj) AsObjectHeader(obj)->GCFlags &= ~GCMODIFIED
#define ModifiedP(obj) (AsObjectHeader(obj)->GCFlags & GCMODIFIED)

#define SetMark(obj) AsObjectHeader(obj)->GCFlags |= GCMARK
#define ClearMark(oh) oh->GCFlags &= ~GCMARK
#define MarkP(obj) (AsObjectHeader(obj)->GCFlags & GCMARK)

#define SetMature(obj) AsObjectHeader(obj)->GCFlags |= GCMATURE
#define MatureP(obj) (AsObjectHeader(obj)->GCFlags & GCMATURE)

#define SetForward(obj) AsObjectHeader(obj)->GCFlags |= GCFORWARD
#define ForwardedP(obj) (AsObjectHeader(obj)->GCFlags & GCFORWARD)

#define GCMATURE_AGE 2
#define GCMAXIMUM_YOUNG_SIZE 1024
#define GCFULL_PARTIAL 3

typedef struct _FMature
{
    struct _FMature * Next;
} FMature;

static FMature * Mature;

static FObject ScanStack[1024];
static int ScanIndex;

static FObject Modified[1024 * 4];
static int ModifiedCount;

const static unsigned int Align4[4] = {0, 3, 2, 1};

#ifdef FOMENT_DEBUG
int AlignLength(int len)
{
    return(len + Align4[len % 4]);
}

#endif // FOMENT_DEBUG

int ObjectLength(FObject obj)
{
    FAssert(ObjectP(obj));

    switch (ObjectTag(obj))
    {
    case PairTag:
        return(sizeof(FPair));

    case BoxTag:
        return(sizeof(FBox));

    case StringTag:
    {
        int len = sizeof(FString) + sizeof(FCh) * StringLength(obj);
        len += Align4[len % 4];
        return(len);
    }

    case VectorTag:
        FAssert(VectorLength(obj) >= 0);

        return(sizeof(FVector) + sizeof(FObject) * (VectorLength(obj) - 1));

    case BytevectorTag:
    {
        int len = sizeof(FBytevector) + sizeof(FByte) * (BytevectorLength(obj) - 1);
        len += Align4[len % 4];
        return(len);
    }

    case PortTag:
        return(sizeof(FPort));

    case ProcedureTag:
        return(sizeof(FProcedure));

    case SymbolTag:
        return(sizeof(FSymbol));

    case RecordTypeTag:
        return(sizeof(FRecordType) + sizeof(FObject) * (RecordTypeNumFields(obj) - 1));

    case RecordTag:
        return(sizeof(FGenericRecord) + sizeof(FObject) * (RecordNumFields(obj) - 1));

    case PrimitiveTag:
        return(sizeof(FPrimitive));

    default:
        FAssert(0);
    }

    return(0);
}

static FObjectHeader * AllocateNursery(unsigned int len)
{
    FAssert(len % 4 == 0);
    FAssert(len >= sizeof(FObject));

    if (ActiveSection->Used + len + sizeof(FObjectHeader) > ActiveSection->Size)
    {
        if (ActiveSection->Next == 0)
            return(0);

        ActiveSection = ActiveSection->Next;

        FAssert(ActiveSection->Used == 0);
    }

    FObjectHeader * oh = (FObjectHeader *) (ActiveSection->Space + ActiveSection->Used);
    ActiveSection->Used += len + sizeof(FObjectHeader);

    if (ActiveSection->Next == 0 && ActiveSection->Used > (ActiveSection->Size * 3) / 4)
        GCRequired = 1;

    return(oh);
}

static FObjectHeader * AllocateMature(unsigned int len)
{
    FMature * m = (FMature *) malloc(sizeof(FMature) + len + sizeof(FObjectHeader));
    m->Next = Mature;
    Mature = m;

    return((FObjectHeader *) (m + 1));
}

static void RecordModify(FObject obj);
FObject MakeObject(FObjectTag tag, unsigned int sz)
{
    FAssert(tag < BadDogTag);

    unsigned int len = sz;
    len += Align4[len % 4];

    FAssert(len >= sz);

    BytesAllocated += len;
/*    FObjectHeader * oh = AllocateNursery(len);

    FAssert(oh != 0);

    oh->GCFlags = 0;
*/
    FObjectHeader * oh = 0;

    if (len <= GCMAXIMUM_YOUNG_SIZE)
    {
        oh = AllocateNursery(len);
        if (oh != 0)
            oh->GCFlags = 0;
    }

    if (oh == 0)
    {
        oh = AllocateMature(len);
        oh->GCFlags = GCMATURE | GCMATURE_AGE;

        RecordModify((FObject) (oh + 1));
    }

    oh->Hash = Hash;
    Hash += 1;
    oh->Tag = tag;

    FObject obj = (FObject) (oh + 1);

    FAssert(oh == AsObjectHeader(obj));
    FAssert(ObjectP(obj));
//    FAssert(ObjectTag(obj) == tag);

    return(obj);
}

static void RecordModify(FObject obj)
{
    FAssert(ModifiedCount < sizeof(Modified) / sizeof(FObject));
    FAssert(ObjectP(obj));
    FAssert(MatureP(obj));

    if (ModifiedP(obj) == 0)
    {
        SetModified(obj);

        Modified[ModifiedCount] = obj;
        ModifiedCount += 1;
    }
}

void ModifyVector(FObject obj, unsigned int idx, FObject val)
{
    FAssert(VectorP(obj));
    FAssert(idx < VectorLength(obj));

    AsVector(obj)->Vector[idx] = val;

    if (MatureP(obj) && ObjectP(val) && MatureP(val) == 0)
        RecordModify(obj);
}

void ModifyObject(FObject obj, int off, FObject val)
{
    FAssert(PairP(obj) == 0);
    FAssert(ObjectP(obj));
    FAssert(off % sizeof(FObject) == 0);

    ((FObject *) obj)[off / sizeof(FObject)] = val;

    if (MatureP(obj) && ObjectP(val) && MatureP(val) == 0)
        RecordModify(obj);
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

static void ScanObject(FObject * pobj, int fcf)
{
    FObject obj = *pobj;

    FAssert(ObjectP(obj));

    if (MatureP(obj))
    {
        FAssert(ForwardedP(obj) == 0);

        if (fcf)
        {
            if (MarkP(obj) == 0)
            {
                SetMark(obj);

                FAssert(ScanIndex < sizeof(ScanStack) / sizeof(FObject));

                ScanStack[ScanIndex] = obj;
                ScanIndex += 1;
            }
        }
    }
    else if (ForwardedP(obj))
    {
        FAssert(MatureP(obj) == 0);

        *pobj = *((FObject *) obj);
    }
    else
    {
        int len = ObjectLength(obj);
        FObjectHeader * oh;

        FAssert(GetAge(obj) < GCMATURE_AGE);

        SetAge(obj, GetAge(obj) + 1);

        FAssert(MarkP(obj) == 0);
        FAssert(MatureP(obj) == 0);
        FAssert(ForwardedP(obj) == 0);

        if (GetAge(obj) == GCMATURE_AGE)
            oh = AllocateMature(len);
        else
            oh = AllocateNursery(len);

        FObject nobj = (FObject) (oh + 1);

        memcpy(oh, AsObjectHeader(obj), len + sizeof(FObjectHeader));

        FAssert(ObjectLength(nobj) == ObjectLength(obj));

        SetForward(obj);
        *((FObject *) obj) = nobj;
        *pobj = nobj;

        if (GetAge(nobj) == GCMATURE_AGE)
        {
            SetMark(nobj);
            SetMature(nobj);

            FAssert(ScanIndex < sizeof(ScanStack) / sizeof(FObject));

            ScanStack[ScanIndex] = nobj;
            ScanIndex += 1;
        }
    }
}

static void CheckModified(FObject obj)
{
    switch (ObjectTag(obj))
    {
    case PairTag:
        if (ObjectP(First(obj)) && MatureP(First(obj)) == 0)
            RecordModify(obj);
        if (ObjectP(Rest(obj)) && MatureP(Rest(obj)) == 0)
            RecordModify(obj);
        break;

    case BoxTag:
        if (ObjectP(AsBox(obj)->Value) && MatureP(AsBox(obj)->Value) == 0)
            RecordModify(obj);
        break;

    case StringTag:
        break;

    case VectorTag:
        for (unsigned int vdx = 0; vdx < VectorLength(obj); vdx++)
            if (ObjectP(AsVector(obj)->Vector[vdx])
                    && MatureP(AsVector(obj)->Vector[vdx]) == 0)
            {
                RecordModify(obj);
                break;
            }
        break;

    case BytevectorTag:
        break;

    case PortTag:
        if (ObjectP(AsPort(obj)->Name) && MatureP(AsPort(obj)->Name) == 0)
            RecordModify(obj);
        if (ObjectP(AsPort(obj)->Object) && MatureP(AsPort(obj)->Object) == 0)
            RecordModify(obj);
        break;

    case ProcedureTag:
        if (ObjectP(AsProcedure(obj)->Name) && MatureP(AsProcedure(obj)->Name) == 0)
            RecordModify(obj);
        if (ObjectP(AsProcedure(obj)->Code) && MatureP(AsProcedure(obj)->Code) == 0)
            RecordModify(obj);
        if (ObjectP(AsProcedure(obj)->RestArg) && MatureP(AsProcedure(obj)->RestArg) == 0)
            RecordModify(obj);
        break;

    case SymbolTag:
        if (ObjectP(AsSymbol(obj)->String) && MatureP(AsSymbol(obj)->String) == 0)
            RecordModify(obj);
        if (ObjectP(AsSymbol(obj)->Hash) && MatureP(AsSymbol(obj)->Hash) == 0)
            RecordModify(obj);
        break;

    case RecordTypeTag:
        for (unsigned int fdx = 0; fdx < RecordTypeNumFields(obj); fdx++)
            if (ObjectP(AsRecordType(obj)->Fields[fdx])
                    && MatureP(AsRecordType(obj)->Fields[fdx]) == 0)
            {
                RecordModify(obj);
                break;
            }
        break;

    case RecordTag:
        for (unsigned int fdx = 0; fdx < RecordNumFields(obj); fdx++)
            if (ObjectP(AsGenericRecord(obj)->Fields[fdx])
                    && MatureP(AsGenericRecord(obj)->Fields[fdx]) == 0)
            {
                RecordModify(obj);
                break;
            }
        break;

    case PrimitiveTag:
        break;

    default:
        FAssert(0);
    }
}

static void ScanChildren(FObject obj, int fcf)
{
    FAssert(ObjectP(obj));

    switch (ObjectTag(obj))
    {
    case PairTag:
        if (ObjectP(First(obj)))
            ScanObject(&(AsPair(obj)->First), fcf);
        if (ObjectP(Rest(obj)))
            ScanObject(&(AsPair(obj)->Rest), fcf);
        break;

    case BoxTag:
        if (ObjectP(AsBox(obj)->Value))
            ScanObject(&(AsBox(obj)->Value), fcf);
        break;

    case StringTag:
        break;

    case VectorTag:
        for (unsigned int vdx = 0; vdx < VectorLength(obj); vdx++)
            if (ObjectP(AsVector(obj)->Vector[vdx]))
                ScanObject(AsVector(obj)->Vector + vdx, fcf);
        break;

    case BytevectorTag:
        break;

    case PortTag:
        if (ObjectP(AsPort(obj)->Name))
            ScanObject(&(AsPort(obj)->Name), fcf);
        if (ObjectP(AsPort(obj)->Object))
            ScanObject(&(AsPort(obj)->Object), fcf);
        break;

    case ProcedureTag:
        if (ObjectP(AsProcedure(obj)->Name))
            ScanObject(&(AsProcedure(obj)->Name), fcf);
        if (ObjectP(AsProcedure(obj)->Code))
            ScanObject(&(AsProcedure(obj)->Code), fcf);
        if (ObjectP(AsProcedure(obj)->RestArg))
            ScanObject(&(AsProcedure(obj)->RestArg), fcf);
        break;

    case SymbolTag:
        if (ObjectP(AsSymbol(obj)->String))
            ScanObject(&(AsSymbol(obj)->String), fcf);
        if (ObjectP(AsSymbol(obj)->Hash))
            ScanObject(&(AsSymbol(obj)->Hash), fcf);
        break;

    case RecordTypeTag:
        for (unsigned int fdx = 0; fdx < RecordTypeNumFields(obj); fdx++)
            if (ObjectP(AsRecordType(obj)->Fields[fdx]))
                ScanObject(AsRecordType(obj)->Fields + fdx, fcf);
        break;

    case RecordTag:
        for (unsigned int fdx = 0; fdx < RecordNumFields(obj); fdx++)
            if (ObjectP(AsGenericRecord(obj)->Fields[fdx]))
                ScanObject(AsGenericRecord(obj)->Fields + fdx, fcf);
        break;

    case PrimitiveTag:
        break;

    default:
        FAssert(0);
    }

    if (MatureP(obj))
        CheckModified(obj);
}

static void Collect(int fcf)
{
    if (fcf)
    {
printf("Full Collection...");

        FMature * m;

        for (m = Mature; m != 0; m = m->Next)
        {
            FObjectHeader * oh = (FObjectHeader *) (m + 1);
            ClearMark(oh);
        }
    }
    else
printf("Partial Collection...");

    ScanIndex = 0;

    FSection * sec = ReserveSections;
    ReserveSections = CurrentSections;
    CurrentSections = sec;
    ActiveSection = CurrentSections;

    FAssert(sec == CurrentSections);

    while (sec != 0)
    {
        sec->Used = 0;
        sec = sec->Next;
    }

    FObject * rv = (FObject *) &R;
    for (int rdx = 0; rdx < sizeof(FRoots) / sizeof(FObject); rdx++)
        if (ObjectP(rv[rdx]))
            ScanObject(rv + rdx, fcf);

    for (int rdx = 0; rdx < UsedRoots; rdx++)
        if (ObjectP(*Roots[rdx]))
            ScanObject(Roots[rdx], fcf);

    if (ExecuteState != 0)
    {
        for (int adx = 0; adx < ExecuteState->AStackPtr; adx++)
            if (ObjectP(ExecuteState->AStack[adx]))
                ScanObject(ExecuteState->AStack + adx, fcf);

        for (int cdx = 0; cdx < ExecuteState->CStackPtr; cdx++)
            if (ObjectP(ExecuteState->CStack[cdx]))
                ScanObject(ExecuteState->CStack + cdx, fcf);

        if (ObjectP(ExecuteState->Proc))
            ScanObject(&ExecuteState->Proc, fcf);
        if (ObjectP(ExecuteState->Frame))
            ScanObject(&ExecuteState->Frame, fcf);
    }

    if (fcf)
    {
        while (ModifiedCount > 0)
        {
            ModifiedCount -= 1;

            FAssert(ObjectP(Modified[ModifiedCount]));
            FAssert(ModifiedP(Modified[ModifiedCount]));

            ClearModified(Modified[ModifiedCount]);
        }
    }
    else
    {
        for (int idx = 0; idx < ModifiedCount; idx++)
        {
            FAssert(ObjectP(Modified[idx]));
            FAssert(ModifiedP(Modified[idx]));

            ScanChildren(Modified[idx], fcf);
        }
    }

    sec = CurrentSections;
    char * sp = sec->Space;
    while (ScanIndex > 0 || sec != ActiveSection
            || sp < ActiveSection->Space + ActiveSection->Used)
    {
        while (ScanIndex > 0)
        {
            ScanIndex -= 1;
            FObject obj = ScanStack[ScanIndex];

            FAssert(ObjectP(obj));
            FAssert(MatureP(obj));
            FAssert(GetAge(obj) == GCMATURE_AGE);

            ScanChildren(obj, fcf);
        }

        while (sp < sec->Space + sec->Used)
        {
            FObjectHeader * oh = (FObjectHeader *) sp;
            FObject obj = (FObject) (oh + 1);
            ScanChildren(obj, fcf);

            sp += ObjectLength(obj) + sizeof(FObjectHeader);
        }

        FAssert(sp == sec->Space + sec->Used);

        if (sec != ActiveSection)
        {
            sec = sec->Next;
            sp = sec->Space;
        }
    }

    if (fcf)
    {
        FMature ** pm = &Mature;
        FMature * m = Mature;
        while (m != 0)
        {
            FObjectHeader * oh = (FObjectHeader *) (m + 1);

            if ((oh->GCFlags & GCMARK) == 0)
            {
                *pm = m->Next;
                free(m);
                m = *pm;
            }
            else
            {
                pm = &m->Next;
                m = m->Next;
            }
        }
    }

    printf("Done.\n");
}

void Collect()
{
#if 0
    CollectionCount += 1;

    FAssert(GCRequired != 0);
    GCRequired = 0;

    Collect(CollectionCount % GCFULL_PARTIAL == 0);

#ifdef FOMENT_GCCHK
    FSection * sec = ReserveSections;
    while (sec != 0)
    {
        memset(sec->Space, 0, sec->Size);
        sec = sec->Next;
    }
#endif // FOMENT_GCCHK
#endif // 0
}

void SetupGC()
{
    FAssert(sizeof(FObject) == sizeof(FImmediate));
    FAssert(sizeof(FObject) == sizeof(char *));
    FAssert(sizeof(FFixnum) <= sizeof(FImmediate));
    FAssert(sizeof(FCh) <= sizeof(FImmediate));
    FAssert(GCMATURE_AGE <= GCAGEMASK);

    BytesAllocated = 0;
    CollectionCount = 0;

    CurrentSections = 0;
    ReserveSections = 0;
    for (int idx = 0; idx < GCSECTION_COUNT; idx++)
    {
        FSection * s = (FSection *) malloc(GCSECTION_SIZE + sizeof(FSection) - sizeof(char));

        FAssert(s != 0);

        s->Next = CurrentSections;
        s->Used = 0;
        s->Size = GCSECTION_SIZE;
        CurrentSections = s;

        s = (FSection *) malloc(GCSECTION_SIZE + sizeof(FSection) - sizeof(char));

        FAssert(s != 0);

        s->Next = ReserveSections;
        s->Used = 0;
        s->Size = GCSECTION_SIZE;
        ReserveSections = s;
    }

    ActiveSection = CurrentSections;

    Mature = 0;

    ModifiedCount = 0;
}
#endif // 0