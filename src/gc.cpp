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

-- pointer tag for pairs
-- pointer tag for doubles

PairTag       01
DoubleTag     10

-- young objects start with an FObject containing the TypeTag or a forwarding pointer

-- remove explicit unsigned int from Pairs
-- merge some of the fields in FProcedure into Reserved
-- merge Hash into Reserved in FSymbol

-- mark collector: don't use malloc/free for memory allocator
-- mark collector: mark-compact always full collection
*/

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include "foment.hpp"
#include "execute.hpp"
#include "io.hpp"

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

unsigned int BytesAllocated;
unsigned int CollectionCount;

typedef struct _FSection
{
    struct _FSection * Next;
    unsigned int Used;
    unsigned int Size;
    char Space[1];
} FSection;

static FSection * ActiveSection;
static FSection * CurrentSections;
static FSection * ReserveSections;

static int UsedRoots = 0;
static FObject * Roots[128];
static unsigned short Hash = 0;

static FExecuteState * ExecuteState = 0;
int GCRequired = 0;

#define GCSECTION_SIZE (64 * 1024 - 256)
#define GCSECTION_COUNT 4
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

    BytesAllocated += len + sizeof(FObjectHeader);

    FObject obj = (FObject) (oh + 1);

    FAssert(oh == AsObjectHeader(obj));
    FAssert(ObjectP(obj));
//    FAssert(ObjectTag(obj) == tag);

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
    FAssert(ObjectP(obj));
    FAssert(off % sizeof(FObject) == 0);

    ((FObject *) obj)[off / sizeof(FObject)] = val;

    if (MatureP(obj) && ObjectP(val) && MatureP(val) == 0)
        RecordModify(obj);
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
