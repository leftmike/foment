/*

Foment

*/

#include "foment.hpp"

// ---- Vectors ----

static FVector * MakeVector(unsigned int vl, char * who)
{
    FVector * nv = (FVector *) MakeObject(sizeof(FVector) + (vl - 1) * sizeof(FObject), VectorTag);
    if (nv == 0)
    {
        nv = (FVector *) MakeMatureObject(sizeof(FVector) + (vl - 1) * sizeof(FObject), who);
        nv->Length = MakeMatureLength(vl, VectorTag);
    }
    else
        nv->Length = MakeLength(vl, VectorTag);

    return(nv);
}

FObject MakeVector(unsigned int vl, FObject * v, FObject obj)
{
    FVector * nv = MakeVector(vl, "make-vector");

    unsigned int idx;
    if (v == 0)
        for (idx = 0; idx < vl; idx++)
            nv->Vector[idx] = obj;
    else
        for (idx = 0; idx < vl; idx++)
            nv->Vector[idx] = v[idx];

    FAssert(VectorLength(nv) == vl);
    return(nv);
}

FObject ListToVector(FObject obj)
{
    unsigned int vl = ListLength(obj);
    FVector * nv = MakeVector(vl, "list->vector");

    for (unsigned int idx = 0; idx < vl; idx++)
    {
        nv->Vector[idx] = First(obj);
        obj = Rest(obj);
    }

    FAssert(VectorLength(nv) == vl);
    return(nv);
}

FObject VectorToList(FObject vec)
{
    FAssert(VectorP(vec));

    FObject lst = EmptyListObject;
    for (int idx = (int) VectorLength(vec) - 1; idx >= 0; idx--)
        lst = MakePair(AsVector(vec)->Vector[idx], lst);

    return(lst);
}

Define("vector?", VectorPPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "vector?", "vector?: expected one argument", EmptyListObject);

    return(VectorP(argv[0]) ? TrueObject : FalseObject);
}

Define("make-vector", MakeVectorPrimitive)(int argc, FObject argv[])
{
    if (argc < 1 || argc > 2)
        RaiseExceptionC(R.Assertion, "make-vector", "make-vector: expected one or two arguments",
                EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "make-vector",
                "make-vector: expected a fixnum", List(argv[0]));

    return(MakeVector(AsFixnum(argv[0]), 0, argc == 2 ? argv[1] : NoValueObject));
}

Define("vector-ref", VectorRefPrimitive)(int argc, FObject argv[])
{
    if (argc != 2)
        RaiseExceptionC(R.Assertion, "vector-ref", "vector-ref: expected two arguments",
                EmptyListObject);

    if (VectorP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "vector-ref", "vector-ref: expected a vector", List(argv[0]));

    if (FixnumP(argv[1]) == 0)
        RaiseExceptionC(R.Assertion, "vector-ref", "vector-ref: expected a fixnum", List(argv[1]));

    if (AsFixnum(argv[1]) < 0 || AsFixnum(argv[1]) >= (FFixnum) VectorLength(argv[0]))
        RaiseExceptionC(R.Assertion, "vector-ref", "vector-ref: invalid index", List(argv[1]));

    return(AsVector(argv[0])->Vector[AsFixnum(argv[1])]);
}

Define("vector-set!", VectorSetPrimitive)(int argc, FObject argv[])
{
    if (argc != 3)
        RaiseExceptionC(R.Assertion, "vector-set!", "vector-set!: expected three arguments",
                EmptyListObject);

    if (VectorP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "vector-set!",
                "vector-set!: expected a vector", List(argv[0]));

    if (FixnumP(argv[1]) == 0)
        RaiseExceptionC(R.Assertion, "vector-set!",
                "vector-set!: expected a fixnum", List(argv[1]));

    if (AsFixnum(argv[1]) < 0 || AsFixnum(argv[1]) >= (FFixnum) VectorLength(argv[0]))
        RaiseExceptionC(R.Assertion, "vector-set!", "vector-set!: invalid index", List(argv[1]));

//    AsVector(argv[0])->Vector[AsFixnum(argv[1])] = argv[2];
    ModifyVector(argv[0], AsFixnum(argv[1]), argv[2]);
    return(NoValueObject);
}

Define("list->vector", ListToVectorPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "list->vector", "list->vector: expected one argument",
                EmptyListObject);

    return(ListToVector(argv[0]));
}

// ---- Bytevectors ----

static FBytevector * MakeBytevector(unsigned int vl, char * who)
{
    FBytevector * nv = (FBytevector *) MakeObject(sizeof(FBytevector) + (vl - 1) * sizeof(FByte),
            BytevectorTag);
    if (nv == 0)
    {
        nv = (FBytevector *) MakeMatureObject(sizeof(FBytevector) + (vl - 1) * sizeof(FByte), who);
        nv->Length = MakeMatureLength(vl, BytevectorTag);
    }
    else
        nv->Length = MakeLength(vl, BytevectorTag);

    return(nv);
}

FObject MakeBytevector(unsigned int vl, FByte * v)
{
    FBytevector * nv = MakeBytevector(vl, "make-bytevector");

    if (v != 0)
        for (unsigned int idx = 0; idx < vl; idx++)
            nv->Vector[idx] = v[idx];

    FAssert(BytevectorLength(nv) == vl);
    return(nv);
}

FObject U8ListToBytevector(FObject obj)
{
    unsigned int vl = ListLength(obj);
    FBytevector * nv = MakeBytevector(vl, "list->bytevector");

    for (unsigned int idx = 0; idx < vl; idx++)
    {
        if (FixnumP(First(obj)) == 0 || AsFixnum(First(obj)) > 0xFF
                || AsFixnum(First(obj)) < 0)
            RaiseExceptionC(R.Assertion, "u8-list->bytevector", "u8-list->bytevector: not a byte",
                    List(First(obj)));
        nv->Vector[idx] = (FByte) AsFixnum(First(obj));
        obj = Rest(obj);
    }

    FAssert(BytevectorLength(nv) == vl);
    return(nv);
}

static int BytevectorEqualP(FObject obj1, FObject obj2)
{
    FAssert(BytevectorP(obj1));
    FAssert(BytevectorP(obj2));
    
    
    
}

unsigned int BytevectorHash(FObject obj)
{
    FAssert(BytevectorP(obj));

    int vl = BytevectorLength(obj);
    FByte * v = AsBytevector(obj)->Vector;
    const char * p;
    unsigned int h = 0;

    vl *= sizeof(FByte);
    for (p = (const char *) v; vl > 0; p++, vl--)
        h = ((h << 5) + h) + *p;

    return(h);
}

static FPrimitive * Primitives[] =
{
    &VectorPPrimitive,
    &MakeVectorPrimitive,
    &VectorRefPrimitive,
    &VectorSetPrimitive,
    &ListToVectorPrimitive
};

void SetupVectors()
{
    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
