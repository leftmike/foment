/*

Foment

*/

#include "foment.hpp"

// ---- Vectors ----

FObject MakeVector(int vl, FObject * v, FObject obj)
{
    FVector * nv = (FVector *) MakeObject(VectorTag, sizeof(FVector) + (vl - 1)
            * sizeof(FObject));
    nv->Length = vl;

    int idx;
    if (v == 0)
        for (idx = 0; idx < vl; idx++)
            nv->Vector[idx] = obj;
    else
        for (idx = 0; idx < vl; idx++)
            nv->Vector[idx] = v[idx];

    obj = AsObject(nv);
    FAssert(ObjectLength(obj) == sizeof(FVector) + (vl - 1) * sizeof(FObject));
    return(obj);
}

FObject ListToVector(FObject obj)
{
    int vl = ListLength(obj);
    FVector * nv = (FVector *) MakeObject(VectorTag, sizeof(FVector) + (vl - 1)
            * sizeof(FObject));
    nv->Length = vl;

    int idx;
    for (idx = 0; idx < vl; idx++)
    {
        nv->Vector[idx] = First(obj);
        obj = Rest(obj);
    }

    obj = AsObject(nv);
    FAssert(ObjectLength(obj) == sizeof(FVector) + (vl - 1) * sizeof(FObject));
    return(obj);
}

FObject VectorToList(FObject vec)
{
    FAssert(VectorP(vec));

    FObject lst = EmptyListObject;
    for (int idx = VectorLen(vec) - 1; idx >= 0; idx--)
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

    if (AsFixnum(argv[1]) < 0 || AsFixnum(argv[1]) >= AsVector(argv[0])->Length)
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

    if (AsFixnum(argv[1]) < 0 || AsFixnum(argv[1]) >= AsVector(argv[0])->Length)
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

FObject MakeBytevector(int vl, FByte * v)
{
    FBytevector * nv = (FBytevector *) MakeObject(BytevectorTag, sizeof(FBytevector)
            + (vl - 1) * sizeof(FByte));
    nv->Length = vl;

    if (v != 0)
    {
        int idx;
        for (idx = 0; idx < vl; idx++)
            nv->Vector[idx] = v[idx];
    }

    FObject obj = AsObject(nv);
    FAssert(ObjectLength(obj) == AlignLength(sizeof(FBytevector) + (vl - 1) * sizeof(FByte)));
    return(obj);
}

FObject U8ListToBytevector(FObject obj)
{
    int vl = ListLength(obj);
    FBytevector * nv = (FBytevector *) MakeObject(BytevectorTag, sizeof(FBytevector)
            + (vl - 1) * sizeof(FByte));
    nv->Length = vl;

    int idx;
    for (idx = 0; idx < vl; idx++)
    {
        if (FixnumP(First(obj)) == 0 || AsFixnum(First(obj)) > 0xFF
                || AsFixnum(First(obj)) < 0)
            RaiseExceptionC(R.Assertion, "u8-list->bytevector", "u8-list->bytevector: not a byte",
                    List(First(obj)));
        nv->Vector[idx] = (FByte) AsFixnum(First(obj));
        obj = Rest(obj);
    }

    obj = AsObject(nv);
    FAssert(ObjectLength(obj) == AlignLength(sizeof(FBytevector) + (vl - 1) * sizeof(FByte)));
    return(obj);
}

static int BytevectorEqualP(FObject obj1, FObject obj2)
{
    FAssert(BytevectorP(obj1));
    FAssert(BytevectorP(obj2));

}

unsigned int BytevectorHash(FObject obj)
{
    FAssert(BytevectorP(obj));

    int vl = AsBytevector(obj)->Length;
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
