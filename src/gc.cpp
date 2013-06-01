/*

Foment

*/

#include <malloc.h>
#include "foment.hpp"

unsigned int BytesAllocated;

void GarbageCollect()
{
    // EqHash and EqvHash assume that objects don't move (ie. change addresses)
    
    
    
    
}

const static unsigned int Align4[4] = {0, 3, 2, 1};

FObject MakeObject(FObjectTag tag, unsigned int sz)
{
    FAssert(tag < BadDogTag);

    unsigned int len = sz;
    len += Align4[len % 4];
    FAssert(len % 4 == 0);
    FAssert(len >= sz);

    FObjectHeader * oh = (FObjectHeader *) malloc(len + sizeof(FObjectHeader));
    oh->Hash = 0;
    oh->Tag = tag;
    oh->GCFlags = 0;

    BytesAllocated += len + sizeof(FObjectHeader);

    FObject obj = (FObject) (oh + 1);

    FAssert(oh == AsObjectHeader(obj));
    FAssert(ObjectP(obj));
    FAssert(ObjectTag(obj) == tag);

    return(obj);
/*
    FObjectBase base = (FObjectBase) malloc(len + sizeof(FObjectBase));
    base[0] = ((len / 4) << 8) | (tag & 0xFF);

    BytesAllocated += len + sizeof(FObjectBase);

    FObject obj = (FObject) (base + 1);
    FAssert(ObjectP(obj));
    FAssert(ObjectTag(obj) == tag);
    FAssert(ObjectLength(obj) == len);
    return(obj);
*/
}

void ModifyObject(FObject obj, int off, FObject val)
{
    FAssert(off % sizeof(FObject) == 0);

    ((FObject *) obj)[off / sizeof(FObject)] = val;
    
    
    
}

void SetupGC()
{
    FAssert(sizeof(FObject) == sizeof(FImmediate));
    FAssert(sizeof(FObject) == sizeof(char *));
    FAssert(sizeof(FFixnum) <= sizeof(FImmediate));
    FAssert(sizeof(FCh) <= sizeof(FImmediate));

    BytesAllocated = 0;
}
