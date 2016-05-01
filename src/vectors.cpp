/*

Foment

*/

#include <string.h>
#include "foment.hpp"
#include "unicode.hpp"

// ---- Vectors ----

static inline FVector * MakeVector(uint_t vl, const char * who)
{
    return((FVector *) MakeObject(VectorTag, sizeof(FVector) + (vl - 1) * sizeof(FObject), vl,
            who));
}

FObject MakeVector(uint_t vl, FObject * v, FObject obj)
{
    FVector * nv = MakeVector(vl, "make-vector");

    uint_t idx;
    if (v == 0)
    {
        if (MatureP(nv) && ObjectP(obj))
            for (idx = 0; idx < vl; idx++)
                ModifyVector(nv, idx, obj);
        else
            for (idx = 0; idx < vl; idx++)
                nv->Vector[idx] = obj;
    }
    else
    {
        if (MatureP(nv))
            for (idx = 0; idx < vl; idx++)
                ModifyVector(nv, idx, v[idx]);
        else
            for (idx = 0; idx < vl; idx++)
                nv->Vector[idx] = v[idx];
    }

    FAssert(VectorLength(nv) == vl);
    return(nv);
}

FObject ListToVector(FObject obj)
{
    uint_t vl = ListLength("list->vector", obj);
    FVector * nv = MakeVector(vl, "list->vector");

    if (MatureP(nv))
        for (uint_t idx = 0; idx < vl; idx++)
        {
            ModifyVector(nv, idx, First(obj));
            obj = Rest(obj);
        }
    else
        for (uint_t idx = 0; idx < vl; idx++)
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
    for (int_t idx = (int_t) VectorLength(vec) - 1; idx >= 0; idx--)
        lst = MakePair(AsVector(vec)->Vector[idx], lst);

    return(lst);
}

Define("vector?", VectorPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("vector?", argc);

    return(VectorP(argv[0]) ? TrueObject : FalseObject);
}

Define("make-vector", MakeVectorPrimitive)(int_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("make-vector", argc);
    NonNegativeArgCheck("make-vector", argv[0], 0);

    return(MakeVector(AsFixnum(argv[0]), 0, argc == 2 ? argv[1] : NoValueObject));
}

Define("vector", VectorPrimitive)(int_t argc, FObject argv[])
{
    FObject v = MakeVector(argc, "vector");

    if (MatureP(v))
    {
        for (int_t adx = 0; adx < argc; adx++)
            ModifyVector(v, adx, argv[adx]);
    }
    else
    {
        for (int_t adx = 0; adx < argc; adx++)
            AsVector(v)->Vector[adx] = argv[adx];
    }

    return(v);
}

Define("vector-length", VectorLengthPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("vector-length", argc);
    VectorArgCheck("vector-length", argv[0]);

    return(MakeFixnum(VectorLength(argv[0])));
}

Define("vector-ref", VectorRefPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("vector-ref", argc);
    VectorArgCheck("vector-ref", argv[0]);
    IndexArgCheck("vector-ref", argv[1], VectorLength(argv[0]));

    return(AsVector(argv[0])->Vector[AsFixnum(argv[1])]);
}

Define("vector-set!", VectorSetPrimitive)(int_t argc, FObject argv[])
{
    ThreeArgsCheck("vector-set!", argc);
    VectorArgCheck("vector-set!", argv[0]);
    IndexArgCheck("vector-set!", argv[1], VectorLength(argv[0]));

//    AsVector(argv[0])->Vector[AsFixnum(argv[1])] = argv[2];
    ModifyVector(argv[0], AsFixnum(argv[1]), argv[2]);
    return(NoValueObject);
}

Define("vector->list", VectorToListPrimitive)(int_t argc, FObject argv[])
{
    FFixnum strt;
    FFixnum end;

    OneToThreeArgsCheck("vector->list", argc);
    VectorArgCheck("vector->list", argv[0]);

    if (argc > 1)
    {
        IndexArgCheck("vector->list", argv[1], VectorLength(argv[0]));

        strt = AsFixnum(argv[1]);

        if (argc > 2)
        {
            EndIndexArgCheck("vector->list", argv[2], strt, VectorLength(argv[0]));

            end = AsFixnum(argv[2]);
        }
        else
            end = (FFixnum) VectorLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (FFixnum) VectorLength(argv[0]);
    }

    FAssert(end >= strt);

    FObject lst = EmptyListObject;

    for (FFixnum idx = end; idx > strt; idx--)
        lst = MakePair(AsVector(argv[0])->Vector[idx - 1], lst);

    return(lst);
}

Define("list->vector", ListToVectorPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("list->vector", argc);

    return(ListToVector(argv[0]));
}

Define("vector->string", VectorToStringPrimitive)(int_t argc, FObject argv[])
{
    FFixnum strt;
    FFixnum end;

    OneToThreeArgsCheck("vector->string", argc);
    VectorArgCheck("vector->string", argv[0]);

    if (argc > 1)
    {
        IndexArgCheck("vector->string", argv[1], VectorLength(argv[0]));

        strt = AsFixnum(argv[1]);

        if (argc > 2)
        {
            EndIndexArgCheck("vector->string", argv[2], strt, VectorLength(argv[0]));

            end = AsFixnum(argv[2]);
        }
        else
            end = (FFixnum) VectorLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (FFixnum) VectorLength(argv[0]);
    }

    FAssert(end >= strt);

    FObject s = MakeString(0, end - strt);

    for (FFixnum idx = 0; idx < end - strt; idx ++)
    {
        CharacterArgCheck("vector->string", AsVector(argv[0])->Vector[idx + strt]);

        AsString(s)->String[idx] = AsCharacter(AsVector(argv[0])->Vector[idx + strt]);
    }

    return(s);
}

Define("string->vector", StringToVectorPrimitive)(int_t argc, FObject argv[])
{
    FFixnum strt;
    FFixnum end;

    OneToThreeArgsCheck("string->vector", argc);
    StringArgCheck("string->vector", argv[0]);

    if (argc > 1)
    {
        IndexArgCheck("string->vector", argv[1], StringLength(argv[0]));

        strt = AsFixnum(argv[1]);

        if (argc > 2)
        {
            EndIndexArgCheck("string->vector", argv[2], strt, StringLength(argv[0]));

            end = AsFixnum(argv[2]);
        }
        else
            end = (FFixnum) StringLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (FFixnum) StringLength(argv[0]);
    }

    FAssert(end >= strt);

    FObject v = MakeVector(end - strt, "string->vector");
    for (FFixnum idx = 0; idx < end - strt; idx ++)
        AsVector(v)->Vector[idx] = MakeCharacter(AsString(argv[0])->String[idx + strt]);

    return(v);
}

Define("vector-copy", VectorCopyPrimitive)(int_t argc, FObject argv[])
{
    FFixnum strt;
    FFixnum end;

    OneToThreeArgsCheck("vector-copy", argc);
    VectorArgCheck("vector-copy", argv[0]);

    if (argc > 1)
    {
        IndexArgCheck("vector-copy", argv[1], VectorLength(argv[0]));

        strt = AsFixnum(argv[1]);

        if (argc > 2)
        {
            EndIndexArgCheck("vector-copy", argv[2], strt, VectorLength(argv[0]));

            end = AsFixnum(argv[2]);
        }
        else
            end = (FFixnum) VectorLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (FFixnum) VectorLength(argv[0]);
    }

    FAssert(end >= strt);

    return(MakeVector(end - strt, AsVector(argv[0])->Vector + strt, NoValueObject));
}

Define("vector-copy!", VectorCopyModifyPrimitive)(int_t argc, FObject argv[])
{
    FFixnum strt;
    FFixnum end;

    ThreeToFiveArgsCheck("vector-copy!", argc);
    VectorArgCheck("vector-copy!", argv[0]);
    IndexArgCheck("vector-copy!", argv[1], VectorLength(argv[0]));
    VectorArgCheck("vector-copy!", argv[2]);

    if (argc > 3)
    {
        IndexArgCheck("vector-copy!", argv[3], VectorLength(argv[2]));

        strt = AsFixnum(argv[3]);

        if (argc > 4)
        {
            EndIndexArgCheck("vector-copy!", argv[4], strt, VectorLength(argv[2]));

            end = AsFixnum(argv[4]);
        }
        else
            end = (FFixnum) VectorLength(argv[2]);
    }
    else
    {
        strt = 0;
        end = (FFixnum) VectorLength(argv[2]);
    }

    if ((FFixnum) VectorLength(argv[0]) - AsFixnum(argv[1]) < end - strt)
        RaiseExceptionC(R.Assertion, "vector-copy!", "expected a valid index", List(argv[1]));

    FAssert(end >= strt);

    FFixnum at = AsFixnum(argv[1]);

    if (at > strt)
    {
        for (FFixnum idx = end - strt; idx > 0; idx--)
            ModifyVector(argv[0], idx + at - 1, AsVector(argv[2])->Vector[idx + strt - 1]);
    }
    else
    {
        for (FFixnum idx = 0; idx < end - strt; idx++)
            ModifyVector(argv[0], idx + at, AsVector(argv[2])->Vector[idx + strt]);
    }

    return(NoValueObject);
}

Define("vector-append", VectorAppendPrimitive)(int_t argc, FObject argv[])
{
    int_t len = 0;

    for (int_t adx = 0; adx < argc; adx++)
    {
        VectorArgCheck("vector-append", argv[adx]);

        len += VectorLength(argv[adx]);
    }

    FObject v = MakeVector(len, "vector-append");
    int_t idx = 0;

    for (int_t adx = 0; adx < argc; adx++)
    {
        for (int_t vdx = 0; vdx < (int_t) VectorLength(argv[adx]); vdx++)
//            AsVector(v)->Vector[idx + vdx] = AsVector(argv[adx])->Vector[vdx];
            ModifyVector(v, idx + vdx, AsVector(argv[adx])->Vector[vdx]);

        idx += VectorLength(argv[adx]);
    }

    return(v);
}

Define("vector-fill!", VectorFillPrimitive)(int_t argc, FObject argv[])
{
    FFixnum strt;
    FFixnum end;

    TwoToFourArgsCheck("vector-fill!", argc);
    VectorArgCheck("vector-fill!", argv[0]);

    if (argc > 2)
    {
        IndexArgCheck("vector-fill!", argv[2], VectorLength(argv[0]));

        strt = AsFixnum(argv[2]);

        if (argc > 3)
        {
            EndIndexArgCheck("vector-fill!", argv[3], strt, VectorLength(argv[0]));

            end = AsFixnum(argv[3]);
        }
        else
            end = (FFixnum) VectorLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (FFixnum) VectorLength(argv[0]);
    }

    FAssert(end >= strt);

    for (FFixnum idx = strt; idx < end; idx++)
        ModifyVector(argv[0], idx, argv[1]);

    return(NoValueObject);
}

// ---- Bytevectors ----

static inline FBytevector * MakeBytevector(uint_t vl, const char * who)
{
    return((FBytevector *) MakeObject(BytevectorTag,
            sizeof(FBytevector) + (vl - 1) * sizeof(FByte), 0, who));
}

FObject MakeBytevector(uint_t vl)
{
    return(MakeBytevector(vl, "make-bytevector"));
}

FObject U8ListToBytevector(FObject obj)
{
    uint_t vl = ListLength("list->bytevector", obj);
    FBytevector * nv = MakeBytevector(vl, "list->bytevector");

    for (uint_t idx = 0; idx < vl; idx++)
    {
        if (FixnumP(First(obj)) == 0 || AsFixnum(First(obj)) > 0xFF
                || AsFixnum(First(obj)) < 0)
            RaiseExceptionC(R.Assertion, "u8-list->bytevector", "not a byte", List(First(obj)));
        nv->Vector[idx] = (FByte) AsFixnum(First(obj));
        obj = Rest(obj);
    }

    FAssert(BytevectorLength(nv) == vl);
    return(nv);
}

uint_t BytevectorHash(FObject obj)
{
    FAssert(BytevectorP(obj));

    int_t vl = BytevectorLength(obj);
    FByte * v = AsBytevector(obj)->Vector;
    const char * p;
    uint_t h = 0;

    vl *= sizeof(FByte);
    for (p = (const char *) v; vl > 0; p++, vl--)
        h = ((h << 5) + h) + *p;

    return(h);
}

int_t BytevectorCompare(FObject obj1, FObject obj2)
{
    FAssert(BytevectorP(obj1));
    FAssert(BytevectorP(obj2));

    if (BytevectorLength(obj1) != BytevectorLength(obj2))
        return(BytevectorLength(obj1) < BytevectorLength(obj2) ? -1 : 1);

    for (uint_t sdx = 0; sdx < BytevectorLength(obj1) && sdx < BytevectorLength(obj2); sdx++)
        if (AsBytevector(obj1)->Vector[sdx] != AsBytevector(obj2)->Vector[sdx])
            return(AsBytevector(obj1)->Vector[sdx] < AsBytevector(obj2)->Vector[sdx] ? -1 : 1);

    return(0);
}

Define("bytevector?", BytevectorPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("bytevector?", argc);

    return(BytevectorP(argv[0]) ? TrueObject : FalseObject);
}

Define("make-bytevector", MakeBytevectorPrimitive)(int_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("make-bytevector", argc);
    NonNegativeArgCheck("make-bytevector", argv[0], 0);

    if (argc == 2)
        ByteArgCheck("make-bytevector", argv[1]);

    FObject bv = MakeBytevector(AsFixnum(argv[0]));

    if (argc == 2)
        for (int_t bdx = 0; bdx < AsFixnum(argv[0]); bdx++)
            AsBytevector(bv)->Vector[bdx] = (FByte) AsFixnum(argv[1]);

    return(bv);
}

Define("bytevector", BytevectorPrimitive)(int_t argc, FObject argv[])
{
    FObject bv = MakeBytevector(argc);

    for (int_t adx = 0; adx < argc; adx++)
    {
        ByteArgCheck("bytevector", argv[adx]);

        AsBytevector(bv)->Vector[adx] = (FByte) AsFixnum(argv[adx]);
    }

    return(bv);
}

Define("bytevector-length", BytevectorLengthPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("bytevector-length", argc);
    BytevectorArgCheck("bytevector-length",argv[0]);

    return(MakeFixnum(BytevectorLength(argv[0])));
}

Define("bytevector-u8-ref", BytevectorU8RefPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("bytevector-u8-ref", argc);
    BytevectorArgCheck("bytevector-u8-ref", argv[0]);
    IndexArgCheck("bytevector-u8-ref", argv[1], BytevectorLength(argv[0]));

    return(MakeFixnum(AsBytevector(argv[0])->Vector[AsFixnum(argv[1])]));
}

Define("bytevector-u8-set!", BytevectorU8SetPrimitive)(int_t argc, FObject argv[])
{
    ThreeArgsCheck("bytevector-u8-set!", argc);
    BytevectorArgCheck("bytevector-u8-set!", argv[0]);
    IndexArgCheck("bytevector-u8-set!", argv[1], BytevectorLength(argv[0]));
    ByteArgCheck("bytevector-u8-set!", argv[2]);

    AsBytevector(argv[0])->Vector[AsFixnum(argv[1])] = (FByte) AsFixnum(argv[2]);
    return(NoValueObject);
}

Define("bytevector-copy", BytevectorCopyPrimitive)(int_t argc, FObject argv[])
{
    FFixnum strt;
    FFixnum end;

    OneToThreeArgsCheck("bytevector-copy", argc);
    BytevectorArgCheck("bytevector-copy", argv[0]);

    if (argc > 1)
    {
        IndexArgCheck("bytevector-copy", argv[1], BytevectorLength(argv[0]));

        strt = AsFixnum(argv[1]);

        if (argc > 2)
        {
            EndIndexArgCheck("bytevector-copy", argv[2], strt, BytevectorLength(argv[0]));

            end = AsFixnum(argv[2]);
        }
        else
            end = (FFixnum) BytevectorLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (FFixnum) BytevectorLength(argv[0]);
    }

    FAssert(end >= strt);

    FObject bv = MakeBytevector(end - strt);
    memcpy(AsBytevector(bv)->Vector, AsBytevector(argv[0])->Vector + strt, end - strt);

    return(bv);
}

Define("bytevector-copy!", BytevectorCopyModifyPrimitive)(int_t argc, FObject argv[])
{
    FFixnum strt;
    FFixnum end;

    ThreeToFiveArgsCheck("bytevector-copy!", argc);
    BytevectorArgCheck("bytevector-copy!", argv[0]);
    IndexArgCheck("bytevector-copy!", argv[1], BytevectorLength(argv[0]));
    BytevectorArgCheck("bytevector-copy!", argv[2]);

    if (argc > 3)
    {
        IndexArgCheck("bytevector-copy!", argv[3], BytevectorLength(argv[2]));

        strt = AsFixnum(argv[3]);

        if (argc > 4)
        {
            EndIndexArgCheck("bytevector-copy!", argv[4], strt, BytevectorLength(argv[2]));

            end = AsFixnum(argv[4]);
        }
        else
            end = (FFixnum) BytevectorLength(argv[2]);
    }
    else
    {
        strt = 0;
        end = (FFixnum) BytevectorLength(argv[2]);
    }

    if ((FFixnum) BytevectorLength(argv[0]) - AsFixnum(argv[1]) < end - strt)
        RaiseExceptionC(R.Assertion, "bytevector-copy!", "expected a valid index", List(argv[1]));

    FAssert(end >= strt);

    memmove(AsBytevector(argv[0])->Vector + AsFixnum(argv[1]),
            AsBytevector(argv[2])->Vector + strt, end - strt);

    return(NoValueObject);
}

Define("bytevector-append", BytevectorAppendPrimitive)(int_t argc, FObject argv[])
{
    int_t len = 0;

    for (int_t adx = 0; adx < argc; adx++)
    {
        BytevectorArgCheck("bytevector-append", argv[adx]);

        len += BytevectorLength(argv[adx]);
    }

    FObject bv = MakeBytevector(len);
    int_t idx = 0;

    for (int_t adx = 0; adx < argc; adx++)
    {
        memcpy(AsBytevector(bv)->Vector + idx, AsBytevector(argv[adx])->Vector,
                BytevectorLength(argv[adx]));
        idx += BytevectorLength(argv[adx]);
    }

    return(bv);
}

Define("utf8->string", Utf8ToStringPrimitive)(int_t argc, FObject argv[])
{
    int_t strt;
    int_t end;

    OneToThreeArgsCheck("utf8->string", argc);
    BytevectorArgCheck("utf8->string", argv[0]);

    if (argc > 1)
    {
        IndexArgCheck("utf8->string", argv[1], BytevectorLength(argv[0]));

        strt = AsFixnum(argv[1]);

        if (argc > 2)
        {
            EndIndexArgCheck("utf8->string", argv[2], strt, BytevectorLength(argv[0]));

            end = AsFixnum(argv[2]);
        }
        else
            end = (FFixnum) BytevectorLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (FFixnum) BytevectorLength(argv[0]);
    }

    FAssert(end >= strt);

    return(ConvertUtf8ToString(AsBytevector(argv[0])->Vector + strt, end - strt));
}

Define("string->utf8", StringToUtf8Primitive)(int_t argc, FObject argv[])
{
    int_t strt;
    int_t end;

    OneToThreeArgsCheck("string->utf8", argc);
    StringArgCheck("string->utf8", argv[0]);

    if (argc > 1)
    {
        IndexArgCheck("string->utf8", argv[1], StringLength(argv[0]));

        strt = AsFixnum(argv[1]);

        if (argc > 2)
        {
            EndIndexArgCheck("string->utf8", argv[2], strt, StringLength(argv[0]));

            end = AsFixnum(argv[2]);
        }
        else
            end = (FFixnum) StringLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (FFixnum) StringLength(argv[0]);
    }

    FAssert(end >= strt);

    return(ConvertStringToUtf8(AsString(argv[0])->String + strt, end - strt, 0));
}

static FPrimitive * Primitives[] =
{
    &VectorPPrimitive,
    &MakeVectorPrimitive,
    &VectorPrimitive,
    &VectorLengthPrimitive,
    &VectorRefPrimitive,
    &VectorSetPrimitive,
    &VectorToListPrimitive,
    &ListToVectorPrimitive,
    &VectorToStringPrimitive,
    &StringToVectorPrimitive,
    &VectorCopyPrimitive,
    &VectorCopyModifyPrimitive,
    &VectorAppendPrimitive,
    &VectorFillPrimitive,
    &BytevectorPPrimitive,
    &MakeBytevectorPrimitive,
    &BytevectorPrimitive,
    &BytevectorLengthPrimitive,
    &BytevectorU8RefPrimitive,
    &BytevectorU8SetPrimitive,
    &BytevectorCopyPrimitive,
    &BytevectorCopyModifyPrimitive,
    &BytevectorAppendPrimitive,
    &Utf8ToStringPrimitive,
    &StringToUtf8Primitive
};

void SetupVectors()
{
    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
