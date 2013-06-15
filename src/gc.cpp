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

-- use a table lookup for fix length objects:
int FixedLength[] = {sizeof(FPair), ...};

-- mark collector: add generations
-- mark collector: mark-sweep always full collection
-- mark collector: mark-compact always full collection
-- mark collector: partial and full collections
*/

//#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include "foment.hpp"
#include "execute.hpp"
#include "io.hpp"

unsigned int BytesAllocated;

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
int GCRequired = 1;

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
        int len = sizeof(FString) + sizeof(FCh) * AsString(obj)->Length;
        len += Align4[len % 4];
        return(len);
    }

    case VectorTag:
        FAssert(AsVector(obj)->Length >= 0);

        return(sizeof(FVector) + sizeof(FObject) * (AsVector(obj)->Length - 1));

    case BytevectorTag:
    {
        int len = sizeof(FBytevector) + sizeof(FByte) * (AsBytevector(obj)->Length - 1);
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
        return(sizeof(FRecordType) + sizeof(FObject) * (AsRecordType(obj)->NumFields - 1));

    case RecordTag:
        return(sizeof(FGenericRecord) + sizeof(FObject)
                * (AsGenericRecord(obj)->Record.NumFields - 1));

    case PrimitiveTag:
        return(sizeof(FPrimitive));

    default:
        FAssert(0);
    }

    return(0);
}

FObject MakeObject(FObjectTag tag, unsigned int sz)
{
    FAssert(tag < BadDogTag);

    unsigned int len = sz;
    len += Align4[len % 4];
    FAssert(len % 4 == 0);
    FAssert(len >= sz);
    FAssert(len >= sizeof(FObject));

    if (ActiveSection->Used + len + sizeof(FObjectHeader) > ActiveSection->Size)
    {
        ActiveSection = ActiveSection->Next;

        FAssert(ActiveSection != 0);
        FAssert(ActiveSection->Used == 0);
    }

    FObjectHeader * oh = (FObjectHeader *) (ActiveSection->Space + ActiveSection->Used);
    ActiveSection->Used += len + sizeof(FObjectHeader);

    oh->Hash = Hash;
    Hash += 1;
    oh->Tag = tag;
    oh->GCFlags = 0;

    BytesAllocated += len + sizeof(FObjectHeader);

    FObject obj = (FObject) (oh + 1);

    FAssert(oh == AsObjectHeader(obj));
    FAssert(ObjectP(obj));
    FAssert(ObjectTag(obj) == tag);

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

#ifdef FOMENT_GCCHK
void VerifyCheckSums()
{
    FSection * sec = CurrentSections;

    while (sec != 0)
    {
        char * sp = sec->Space;

        while (sp < sec->Space + sec->Used)
        {
            FObjectHeader * oh = (FObjectHeader *) sp;
            FObject obj = (FObject) (oh + 1);

            if (StringP(obj) || BytevectorP(obj))
            {
                if (AsObjectHeader(obj)->CheckSum != 0)
                {
                    PutStringC(R.StandardOutput, "CheckSum != 0: ");
                    WritePretty(R.StandardOutput, obj, 0);
                    PutCh(R.StandardOutput, '\n');

                    FAssert(0);
                }
            }
            else
            {
                if (AsObjectHeader(obj)->CheckSum
                        != ByteLengthHash((char *) obj, ObjectLength(obj)))
                {
                    PutStringC(R.StandardOutput, "CheckSum Bad: ");
                    WritePretty(R.StandardOutput, obj, 0);
//                    Write(R.StandardOutput, obj, 0);
                    PutCh(R.StandardOutput, '\n');

                    FAssert(0);
                }
            }

            sp += ObjectLength(obj) + sizeof(FObjectHeader);
        }

        FAssert(sp == sec->Space + sec->Used);

        sec = sec->Next;
    }
}

void CheckSumObject(FObject obj)
{
    if (StringP(obj) || BytevectorP(obj))
        AsObjectHeader(obj)->CheckSum = 0;
    else
        AsObjectHeader(obj)->CheckSum = ByteLengthHash((char *) obj, ObjectLength(obj));
}

FObject AsObject(FObject obj)
{
    CheckSumObject(obj);
    return(obj);
}
#endif // FOMENT_GCCHK

void ModifyVector(FObject obj, int idx, FObject val)
{
    FAssert(VectorP(obj));
    FAssert(idx >= 0 && idx < AsVector(obj)->Length);

    AsVector(obj)->Vector[idx] = val;
    
    
    
#ifdef FOMENT_GCCHK
    CheckSumObject(obj);
#endif // FOMENT_GCCHK
}

void ModifyObject(FObject obj, int off, FObject val)
{
    FAssert(off % sizeof(FObject) == 0);

    ((FObject *) obj)[off / sizeof(FObject)] = val;
    
    
    
#ifdef FOMENT_GCCHK
    CheckSumObject(obj);
#endif // FOMENT_GCCHK
}

#define GCFORWARD 0x80

#define FowardedP(obj) (AsObjectHeader(obj)->GCFlags & GCFORWARD)

static void Alive(FObject * pobj)
{
    FObject obj = *pobj;

    if (ObjectP(obj))
    {
        if (FowardedP(obj))
            *pobj = *((FObject *) obj);
        else
        {
            int len = ObjectLength(obj);

            if (ActiveSection->Used + len + sizeof(FObjectHeader) > ActiveSection->Size)
            {
                ActiveSection = ActiveSection->Next;

                FAssert(ActiveSection != 0);
                FAssert(ActiveSection->Used == 0);
            }

            FObjectHeader * oh = (FObjectHeader *) (ActiveSection->Space + ActiveSection->Used);
            ActiveSection->Used += len + sizeof(FObjectHeader);

            FObject nobj = (FObject) (oh + 1);

            memcpy(oh, AsObjectHeader(obj), len + sizeof(FObjectHeader));

            FAssert(ObjectLength(nobj) == ObjectLength(obj));

            AsObjectHeader(obj)->GCFlags |= GCFORWARD;
            *((FObject *) obj) = nobj;
            *pobj = nobj;
        }
    }
}

void Collect()
{
#ifdef FOMENT_GCCHK
    VerifyCheckSums();
#endif // FOMENT_GCCHK

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
        Alive(rv + rdx);

    for (int rdx = 0; rdx < UsedRoots; rdx++)
        Alive(Roots[rdx]);

    if (ExecuteState != 0)
    {
        for (int adx = 0; adx < ExecuteState->AStackPtr; adx++)
            Alive(ExecuteState->AStack + adx);

        for (int cdx = 0; cdx < ExecuteState->CStackPtr; cdx++)
            Alive(ExecuteState->CStack + cdx);

        Alive(&ExecuteState->Proc);
        Alive(&ExecuteState->Frame);
    }

    sec = CurrentSections;
    while (sec != 0)
    {
        char * sp = sec->Space;

        while (sp < sec->Space + sec->Used)
        {
            FObjectHeader * oh = (FObjectHeader *) sp;
            FObject obj = (FObject) (oh + 1);

            if (ObjectP(obj))
            {
                switch (ObjectTag(obj))
                {
                case PairTag:
                    Alive(&(AsPair(obj)->First));
                    Alive(&(AsPair(obj)->Rest));
                    break;

                case BoxTag:
                    Alive(&(AsBox(obj)->Value));
                    break;

                case StringTag:
                    break;

                case VectorTag:
                    for (int vdx = 0; vdx < AsVector(obj)->Length; vdx++)
                        Alive(AsVector(obj)->Vector + vdx);
                    break;

                case BytevectorTag:
                    break;

                case PortTag:
                    Alive(&(AsPort(obj)->Name));
                    Alive(&(AsPort(obj)->Object));
                    break;

                case ProcedureTag:
                    Alive(&(AsProcedure(obj)->Name));
                    Alive(&(AsProcedure(obj)->Code));
                    Alive(&(AsProcedure(obj)->RestArg));
                    break;

                case SymbolTag:
                    Alive(&(AsSymbol(obj)->String));
                    Alive(&(AsSymbol(obj)->Hash));
                    break;

                case RecordTypeTag:
                    Alive(&(AsRecordType(obj)->Name));
                    for (int fdx = 0; fdx < AsRecordType(obj)->NumFields; fdx++)
                        Alive(AsRecordType(obj)->Fields + fdx);
                    break;

                case RecordTag:
                    Alive(&(AsGenericRecord(obj)->Record.RecordType));
                    for (int fdx = 0; fdx < AsGenericRecord(obj)->Record.NumFields; fdx++)
                        Alive(AsGenericRecord(obj)->Fields + fdx);
                    break;

                case PrimitiveTag:
                    break;

                default:
                    FAssert(0);
                }

#ifdef FOMENT_GCCHK
                CheckSumObject(obj);
#endif // FOMENT_GCCHK
            }

            sp += ObjectLength(obj) + sizeof(FObjectHeader);
        }

        FAssert(sp == sec->Space + sec->Used);

        sec = sec->Next;
    }
}

#define SECTION_SIZE (128 * 1024 - 256)
#define SECTION_COUNT 16

void SetupGC()
{
    FAssert(sizeof(FObject) == sizeof(FImmediate));
    FAssert(sizeof(FObject) == sizeof(char *));
    FAssert(sizeof(FFixnum) <= sizeof(FImmediate));
    FAssert(sizeof(FCh) <= sizeof(FImmediate));

    BytesAllocated = 0;

    CurrentSections = 0;
    ReserveSections = 0;
    for (int idx = 0; idx < SECTION_COUNT; idx++)
    {
        FSection * s = (FSection *) malloc(SECTION_SIZE + sizeof(FSection) - sizeof(char));

        FAssert(s != 0);

        s->Next = CurrentSections;
        s->Used = 0;
        s->Size = SECTION_SIZE;
        CurrentSections = s;

        s = (FSection *) malloc(SECTION_SIZE + sizeof(FSection) - sizeof(char));

        FAssert(s != 0);

        s->Next = ReserveSections;
        s->Used = 0;
        s->Size = SECTION_SIZE;
        ReserveSections = s;
    }

    ActiveSection = CurrentSections;
}
