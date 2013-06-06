/*

Foment

*/

#include <stdio.h>

#include <windows.h>
#include <malloc.h>
#include "foment.hpp"
#include "io.hpp"

unsigned int BytesAllocated;
static char * HeapBase;
static char * HeapPointer;

void GarbageCollect()
{
    // EqHash and EqvHash assume that objects don't move (ie. change addresses)
    
    
    
    
}

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

//    default:
//        FAssert(0);
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

//    FObjectHeader * oh = (FObjectHeader *) malloc(len + sizeof(FObjectHeader));
    FObjectHeader * oh = (FObjectHeader *) HeapPointer;
    HeapPointer += len + sizeof(FObjectHeader);

    oh->Hash = 0;
    oh->Tag = tag;
    oh->GCFlags = 0;

    BytesAllocated += len + sizeof(FObjectHeader);

    FObject obj = (FObject) (oh + 1);

    FAssert(oh == AsObjectHeader(obj));
    FAssert(ObjectP(obj));
    FAssert(ObjectTag(obj) == tag);

    return(obj);
}

/*
//    AsPair(argv[0])->Rest = argv[1];
    Modify(FPair, argv[0], Rest, argv[1]);

AllowGC does a check of all checksums
*/

#ifdef FOMENT_GCCHK
void AllowGC()
{
    char * hp = HeapBase;

    while (hp < HeapPointer)
    {
        FObjectHeader * oh = (FObjectHeader *) hp;
        FObject obj = (FObject) (oh + 1);

        if (StringP(obj) || BytevectorP(obj))
        {
            if (AsObjectHeader(obj)->CheckSum != 0)
            {
                PutStringC(StandardOutput, "CheckSum != 0: ");
                WritePretty(StandardOutput, obj, 0);
                PutCh(StandardOutput, '\n');

                FAssert(0);
            }
        }
        else
        {
            if (AsObjectHeader(obj)->CheckSum != ByteLengthHash((char *) obj, ObjectLength(obj)))
            {
                PutStringC(StandardOutput, "CheckSum Bad: ");
                WritePretty(StandardOutput, obj, 0);
                PutCh(StandardOutput, '\n');

                FAssert(0);
            }
        }

        hp += ObjectLength(obj) + sizeof(FObjectHeader);
    }

    FAssert(hp == HeapPointer);
}

FObject AsObject(FObject obj)
{
    if (StringP(obj) || BytevectorP(obj))
        AsObjectHeader(obj)->CheckSum = 0;
    else
        AsObjectHeader(obj)->CheckSum = ByteLengthHash((char *) obj, ObjectLength(obj));

    return(obj);
}
#endif // FOMENT_GCCHK

void ModifyVector(FObject obj, int idx, FObject val)
{
    FAssert(VectorP(obj));
    FAssert(idx >= 0 && idx < AsVector(obj)->Length);

    AsVector(obj)->Vector[idx] = val;
    
    
    
    AsObject(obj);
}

void ModifyObject(FObject obj, int off, FObject val)
{
    FAssert(off % sizeof(FObject) == 0);

    ((FObject *) obj)[off / sizeof(FObject)] = val;
    
    
    
    AsObject(obj);
}

void SetupGC()
{
    FAssert(sizeof(FObject) == sizeof(FImmediate));
    FAssert(sizeof(FObject) == sizeof(char *));
    FAssert(sizeof(FFixnum) <= sizeof(FImmediate));
    FAssert(sizeof(FCh) <= sizeof(FImmediate));

    BytesAllocated = 0;

    HeapBase = (char *) VirtualAlloc(0, 1024 * 1024 * 5, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    FAssert(HeapBase != 0);

    HeapPointer = HeapBase;
}
