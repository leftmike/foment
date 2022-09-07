/*

Foment

*/

#ifdef FOMENT_WINDOWS
#include <windows.h>
#endif // FOMENT_WINDOWS
#ifdef FOMENT_UNIX
#include <pthread.h>
#endif // FOMENT_UNIX
#include <stdio.h>
#include <string.h>
#include "foment.hpp"
#include "syncthrd.hpp"
#include "io.hpp"
#include "unicode.hpp"

// ---- Vectors ----

static inline FVector * MakeVector(ulong_t vl, const char * who)
{
    return((FVector *) MakeObject(VectorTag, sizeof(FVector) + (vl - 1) * sizeof(FObject), vl,
            who));
}

FObject MakeVector(ulong_t vl, FObject * v, FObject obj)
{
    FVector * nv = MakeVector(vl, "make-vector");

    ulong_t idx;
    if (v == 0)
    {
        for (idx = 0; idx < vl; idx++)
            nv->Vector[idx] = obj;
    }
    else
    {
        for (idx = 0; idx < vl; idx++)
            nv->Vector[idx] = v[idx];
    }

    FAssert(VectorLength(nv) == vl);
    return(nv);
}

void WriteVector(FWriteContext * wctx, FObject obj)
{
     wctx->WriteStringC("#(");
     for (ulong_t idx = 0; idx < VectorLength(obj); idx++)
     {
         if (idx > 0)
             wctx->WriteCh(' ');
         wctx->Write(AsVector(obj)->Vector[idx]);
     }

     wctx->WriteCh(')');
}

FObject ListToVector(FObject obj)
{
    ulong_t vl = ListLength("list->vector", obj);
    FVector * nv = MakeVector(vl, "list->vector");

    for (ulong_t idx = 0; idx < vl; idx++)
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
    for (long_t idx = (long_t) VectorLength(vec) - 1; idx >= 0; idx--)
        lst = MakePair(AsVector(vec)->Vector[idx], lst);

    return(lst);
}

Define("vector?", VectorPPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("vector?", argc);

    return(VectorP(argv[0]) ? TrueObject : FalseObject);
}

Define("make-vector", MakeVectorPrimitive)(long_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("make-vector", argc);
    NonNegativeArgCheck("make-vector", argv[0], 1);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(Restriction, "make-vector", "object too big", List(argv[0]));
    return(MakeVector(AsFixnum(argv[0]), 0, argc == 2 ? argv[1] : NoValueObject));
}

Define("vector", VectorPrimitive)(long_t argc, FObject argv[])
{
    FObject v = MakeVector(argc, "vector");

    for (long_t adx = 0; adx < argc; adx++)
        AsVector(v)->Vector[adx] = argv[adx];

    return(v);
}

Define("vector-length", VectorLengthPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("vector-length", argc);
    VectorArgCheck("vector-length", argv[0]);

    return(MakeFixnum(VectorLength(argv[0])));
}

Define("vector-ref", VectorRefPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("vector-ref", argc);
    VectorArgCheck("vector-ref", argv[0]);
    IndexArgCheck("vector-ref", argv[1], VectorLength(argv[0]));

    return(AsVector(argv[0])->Vector[AsFixnum(argv[1])]);
}

Define("vector-set!", VectorSetPrimitive)(long_t argc, FObject argv[])
{
    ThreeArgsCheck("vector-set!", argc);
    VectorArgCheck("vector-set!", argv[0]);
    IndexArgCheck("vector-set!", argv[1], VectorLength(argv[0]));

    AsVector(argv[0])->Vector[AsFixnum(argv[1])] = argv[2];
    return(NoValueObject);
}

Define("vector-swap!", VectorSwapPrimitive)(long_t argc, FObject argv[])
{
    ThreeArgsCheck("vector-swap!", argc);
    VectorArgCheck("vector-swap!", argv[0]);
    IndexArgCheck("vector-swap!", argv[1], VectorLength(argv[0]));
    IndexArgCheck("vector-swap!", argv[2], VectorLength(argv[0]));

    FObject tmp = AsVector(argv[0])->Vector[AsFixnum(argv[1])];
    AsVector(argv[0])->Vector[AsFixnum(argv[1])] = AsVector(argv[0])->Vector[AsFixnum(argv[2])];
    AsVector(argv[0])->Vector[AsFixnum(argv[2])] = tmp;
    return(NoValueObject);
}

Define("vector->list", VectorToListPrimitive)(long_t argc, FObject argv[])
{
    long_t strt;
    long_t end;

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
            end = (long_t) VectorLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (long_t) VectorLength(argv[0]);
    }

    FAssert(end >= strt);

    FObject lst = EmptyListObject;

    for (long_t idx = end; idx > strt; idx--)
        lst = MakePair(AsVector(argv[0])->Vector[idx - 1], lst);

    return(lst);
}

Define("list->vector", ListToVectorPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("list->vector", argc);

    return(ListToVector(argv[0]));
}

Define("vector->string", VectorToStringPrimitive)(long_t argc, FObject argv[])
{
    long_t strt;
    long_t end;

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
            end = (long_t) VectorLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (long_t) VectorLength(argv[0]);
    }

    FAssert(end >= strt);

    FObject s = MakeString(0, end - strt);

    for (long_t idx = 0; idx < end - strt; idx ++)
    {
        CharacterArgCheck("vector->string", AsVector(argv[0])->Vector[idx + strt]);

        AsString(s)->String[idx] = AsCharacter(AsVector(argv[0])->Vector[idx + strt]);
    }

    return(s);
}

Define("string->vector", StringToVectorPrimitive)(long_t argc, FObject argv[])
{
    long_t strt;
    long_t end;

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
            end = (long_t) StringLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (long_t) StringLength(argv[0]);
    }

    FAssert(end >= strt);

    FObject v = MakeVector(end - strt, "string->vector");
    for (long_t idx = 0; idx < end - strt; idx ++)
        AsVector(v)->Vector[idx] = MakeCharacter(AsString(argv[0])->String[idx + strt]);

    return(v);
}

Define("vector-copy", VectorCopyPrimitive)(long_t argc, FObject argv[])
{
    long_t strt;
    long_t end;

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
            end = (long_t) VectorLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (long_t) VectorLength(argv[0]);
    }

    FAssert(end >= strt);

    return(MakeVector(end - strt, AsVector(argv[0])->Vector + strt, NoValueObject));
}

Define("vector-copy!", VectorCopyModifyPrimitive)(long_t argc, FObject argv[])
{
    long_t strt;
    long_t end;

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
            end = (long_t) VectorLength(argv[2]);
    }
    else
    {
        strt = 0;
        end = (long_t) VectorLength(argv[2]);
    }

    if ((long_t) VectorLength(argv[0]) - AsFixnum(argv[1]) < end - strt)
        RaiseExceptionC(Assertion, "vector-copy!", "expected a valid index", List(argv[1]));

    FAssert(end >= strt);

    long_t at = AsFixnum(argv[1]);

    if (at > strt)
    {
        for (long_t idx = end - strt - 1; idx >= 0; idx--)
            AsVector(argv[0])->Vector[idx + at] = AsVector(argv[2])->Vector[idx + strt];
    }
    else
    {
        for (long_t idx = 0; idx < end - strt; idx++)
            AsVector(argv[0])->Vector[idx + at] = AsVector(argv[2])->Vector[idx + strt];
    }

    return(NoValueObject);
}

Define("vector-reverse-copy", VectorReverseCopyPrimitive)(long_t argc, FObject argv[])
{
    long_t strt;
    long_t end;

    OneToThreeArgsCheck("vector-reverse-copy", argc);
    VectorArgCheck("vector-reverse-copy", argv[0]);

    if (argc > 1)
    {
        IndexArgCheck("vector-reverse-copy", argv[1], VectorLength(argv[0]));

        strt = AsFixnum(argv[1]);

        if (argc > 2)
        {
            EndIndexArgCheck("vector-reverse-copy", argv[2], strt, VectorLength(argv[0]));

            end = AsFixnum(argv[2]);
        }
        else
            end = (long_t) VectorLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (long_t) VectorLength(argv[0]);
    }

    FAssert(end >= strt);

    ulong_t idx;
    ulong_t vl = end - strt;
    FObject * v = AsVector(argv[0])->Vector + strt;

    FVector * nv = MakeVector(vl, "vector-reverse-copy");
    for (idx = 0; idx < vl; idx++)
        nv->Vector[idx] = v[vl - idx - 1];
    return(nv);
}

Define("vector-reverse-copy!", VectorReverseCopyModifyPrimitive)(long_t argc, FObject argv[])
{
    long_t strt;
    long_t end;

    ThreeToFiveArgsCheck("vector-reverse-copy!", argc);
    VectorArgCheck("vector-reverse-copy!", argv[0]);
    IndexArgCheck("vector-reverse-copy!", argv[1], VectorLength(argv[0]));
    VectorArgCheck("vector-reverse-copy!", argv[2]);

    if (argc > 3)
    {
        IndexArgCheck("vector-reverse-copy!", argv[3], VectorLength(argv[2]));

        strt = AsFixnum(argv[3]);

        if (argc > 4)
        {
            EndIndexArgCheck("vector-reverse-copy!", argv[4], strt, VectorLength(argv[2]));

            end = AsFixnum(argv[4]);
        }
        else
            end = (long_t) VectorLength(argv[2]);
    }
    else
    {
        strt = 0;
        end = (long_t) VectorLength(argv[2]);
    }

    if ((long_t) VectorLength(argv[0]) - AsFixnum(argv[1]) < end - strt)
        RaiseExceptionC(Assertion, "vector-reverse-copy!", "expected a valid index", List(argv[1]));

    FAssert(end >= strt);

    long_t at = AsFixnum(argv[1]);

    if (at > strt)
    {
        for (long_t idx = 0; idx < end - strt; idx++)
            AsVector(argv[0])->Vector[idx + at] = AsVector(argv[2])->Vector[end - idx - 1];
    }
    else
    {
        for (long_t idx = end - strt - 1; idx >= 0; idx--)
            AsVector(argv[0])->Vector[idx + at] = AsVector(argv[2])->Vector[end - idx - 1];
    }

    return(NoValueObject);
}

Define("vector-append", VectorAppendPrimitive)(long_t argc, FObject argv[])
{
    long_t len = 0;

    for (long_t adx = 0; adx < argc; adx++)
    {
        VectorArgCheck("vector-append", argv[adx]);

        len += VectorLength(argv[adx]);
    }

    FObject v = MakeVector(len, "vector-append");
    long_t idx = 0;

    for (long_t adx = 0; adx < argc; adx++)
    {
        for (long_t vdx = 0; vdx < (long_t) VectorLength(argv[adx]); vdx++)
            AsVector(v)->Vector[idx + vdx] = AsVector(argv[adx])->Vector[vdx];

        idx += VectorLength(argv[adx]);
    }

    return(v);
}

Define("vector-append-subvectors", VectorAppendSubvectorsPrimitive)(long_t argc, FObject argv[])
{
    if (argc % 3 != 0)
        RaiseExceptionC(Assertion, "vector-append-subvectors",
                        "expected a multiple of 3 arguments", EmptyListObject);

    ulong_t vl = 0;
    for (long_t adx = 0; adx < argc; adx += 3)
    {
        VectorArgCheck("vector-append-subvectors", argv[adx]);
        IndexArgCheck("vector-append-subvectors", argv[adx + 1], VectorLength(argv[adx]));
        long_t strt = AsFixnum(argv[adx + 1]);
        EndIndexArgCheck("vector-append-subvectors", argv[adx + 2], strt, VectorLength(argv[adx]));
        long_t end = AsFixnum(argv[adx + 2]);

        FAssert(end >= strt);

        vl += (end - strt);
    }

    ulong_t idx = 0;
    FVector * nv = MakeVector(vl, "vector-append-subvectors");
    for (long_t adx = 0; adx < argc; adx += 3)
    {
        long_t strt = AsFixnum(argv[adx + 1]);
        long_t end = AsFixnum(argv[adx + 2]);
        while (strt < end)
        {
            nv->Vector[idx] = AsVector(argv[adx])->Vector[strt];
            strt += 1;
            idx += 1;
        }
    }
    return(nv);
}

Define("vector-fill!", VectorFillPrimitive)(long_t argc, FObject argv[])
{
    long_t strt;
    long_t end;

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
            end = (long_t) VectorLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (long_t) VectorLength(argv[0]);
    }

    FAssert(end >= strt);

    for (long_t idx = strt; idx < end; idx++)
        AsVector(argv[0])->Vector[idx] = argv[1];

    return(NoValueObject);
}

Define("vector-reverse!", VectorReversePrimitive)(long_t argc, FObject argv[])
{
    long_t strt;
    long_t end;

    OneToThreeArgsCheck("vector-reverse!", argc);
    VectorArgCheck("vector-reverse!", argv[0]);

    if (argc > 1)
    {
        IndexArgCheck("vector-reverse!", argv[1], VectorLength(argv[0]));

        strt = AsFixnum(argv[1]);

        if (argc > 2)
        {
            EndIndexArgCheck("vector-reverse!", argv[2], strt, VectorLength(argv[0]));

            end = AsFixnum(argv[2]);
        }
        else
            end = (long_t) VectorLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (long_t) VectorLength(argv[0]);
    }

    FAssert(end >= strt);

    end -= 1;
    FObject * v = AsVector(argv[0])->Vector;
    while (strt < end)
    {
        FObject tmp = v[strt];
        v[strt] = v[end];
        v[end] = tmp;
        strt += 1;
        end -= 1;
    }
    return(NoValueObject);
}

// ---- Bytevectors ----

static inline FBytevector * MakeBytevector(ulong_t vl, const char * who)
{
    FBytevector * bv = (FBytevector *) MakeObject(BytevectorTag,
            sizeof(FBytevector) + (vl - 1) * sizeof(FByte), 0, who);
    bv->Length = vl;

    return(bv);
}

FObject MakeBytevector(ulong_t vl)
{
    return(MakeBytevector(vl, "make-bytevector"));
}

void WriteBytevector(FWriteContext * wctx, FObject obj)
{
    FCh s[8];
    long_t sl;

    wctx->WriteStringC("#u8(");
    for (ulong_t idx = 0; idx < BytevectorLength(obj); idx++)
    {
        if (idx > 0)
            wctx->WriteCh(' ');

        sl = FixnumAsString((long_t) AsBytevector(obj)->Vector[idx], s, 10);
        wctx->WriteString(s, sl);
    }

    wctx->WriteCh(')');
}

FObject U8ListToBytevector(FObject obj)
{
    ulong_t vl = ListLength("list->bytevector", obj);
    FBytevector * nv = MakeBytevector(vl, "list->bytevector");

    for (ulong_t idx = 0; idx < vl; idx++)
    {
        if (FixnumP(First(obj)) == 0 || AsFixnum(First(obj)) > 0xFF
                || AsFixnum(First(obj)) < 0)
            RaiseExceptionC(Assertion, "u8-list->bytevector", "not a byte", List(First(obj)));
        nv->Vector[idx] = (FByte) AsFixnum(First(obj));
        obj = Rest(obj);
    }

    FAssert(BytevectorLength(nv) == vl);
    return(nv);
}

Define("bytevector?", BytevectorPPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("bytevector?", argc);

    return(BytevectorP(argv[0]) ? TrueObject : FalseObject);
}

Define("make-bytevector", MakeBytevectorPrimitive)(long_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("make-bytevector", argc);
    NonNegativeArgCheck("make-bytevector", argv[0], 1);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(Restriction, "make-bytevector", "object too big", argv[0]);

    if (argc == 2)
        ByteArgCheck("make-bytevector", argv[1]);

    FObject bv = MakeBytevector(AsFixnum(argv[0]));

    if (argc == 2)
        for (long_t bdx = 0; bdx < AsFixnum(argv[0]); bdx++)
            AsBytevector(bv)->Vector[bdx] = (FByte) AsFixnum(argv[1]);

    return(bv);
}

Define("bytevector", BytevectorPrimitive)(long_t argc, FObject argv[])
{
    FObject bv = MakeBytevector(argc);

    for (long_t adx = 0; adx < argc; adx++)
    {
        ByteArgCheck("bytevector", argv[adx]);

        AsBytevector(bv)->Vector[adx] = (FByte) AsFixnum(argv[adx]);
    }

    return(bv);
}

Define("bytevector-length", BytevectorLengthPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("bytevector-length", argc);
    BytevectorArgCheck("bytevector-length",argv[0]);

    return(MakeFixnum(BytevectorLength(argv[0])));
}

Define("bytevector-u8-ref", BytevectorU8RefPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("bytevector-u8-ref", argc);
    BytevectorArgCheck("bytevector-u8-ref", argv[0]);
    IndexArgCheck("bytevector-u8-ref", argv[1], BytevectorLength(argv[0]));

    return(MakeFixnum(AsBytevector(argv[0])->Vector[AsFixnum(argv[1])]));
}

Define("bytevector-u8-set!", BytevectorU8SetPrimitive)(long_t argc, FObject argv[])
{
    ThreeArgsCheck("bytevector-u8-set!", argc);
    BytevectorArgCheck("bytevector-u8-set!", argv[0]);
    IndexArgCheck("bytevector-u8-set!", argv[1], BytevectorLength(argv[0]));
    ByteArgCheck("bytevector-u8-set!", argv[2]);

    AsBytevector(argv[0])->Vector[AsFixnum(argv[1])] = (FByte) AsFixnum(argv[2]);
    return(NoValueObject);
}

Define("bytevector-copy", BytevectorCopyPrimitive)(long_t argc, FObject argv[])
{
    long_t strt;
    long_t end;

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
            end = (long_t) BytevectorLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (long_t) BytevectorLength(argv[0]);
    }

    FAssert(end >= strt);

    FObject bv = MakeBytevector(end - strt);
    memcpy(AsBytevector(bv)->Vector, AsBytevector(argv[0])->Vector + strt, end - strt);

    return(bv);
}

Define("bytevector-copy!", BytevectorCopyModifyPrimitive)(long_t argc, FObject argv[])
{
    long_t strt;
    long_t end;

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
            end = (long_t) BytevectorLength(argv[2]);
    }
    else
    {
        strt = 0;
        end = (long_t) BytevectorLength(argv[2]);
    }

    if ((long_t) BytevectorLength(argv[0]) - AsFixnum(argv[1]) < end - strt)
        RaiseExceptionC(Assertion, "bytevector-copy!", "expected a valid index", List(argv[1]));

    FAssert(end >= strt);

    memmove(AsBytevector(argv[0])->Vector + AsFixnum(argv[1]),
            AsBytevector(argv[2])->Vector + strt, end - strt);

    return(NoValueObject);
}

Define("bytevector-append", BytevectorAppendPrimitive)(long_t argc, FObject argv[])
{
    long_t len = 0;

    for (long_t adx = 0; adx < argc; adx++)
    {
        BytevectorArgCheck("bytevector-append", argv[adx]);

        len += BytevectorLength(argv[adx]);
    }

    FObject bv = MakeBytevector(len);
    long_t idx = 0;

    for (long_t adx = 0; adx < argc; adx++)
    {
        memcpy(AsBytevector(bv)->Vector + idx, AsBytevector(argv[adx])->Vector,
                BytevectorLength(argv[adx]));
        idx += BytevectorLength(argv[adx]);
    }

    return(bv);
}

Define("utf8->string", Utf8ToStringPrimitive)(long_t argc, FObject argv[])
{
    long_t strt;
    long_t end;

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
            end = (long_t) BytevectorLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (long_t) BytevectorLength(argv[0]);
    }

    FAssert(end >= strt);

    return(ConvertUtf8ToString(AsBytevector(argv[0])->Vector + strt, end - strt));
}

Define("string->utf8", StringToUtf8Primitive)(long_t argc, FObject argv[])
{
    long_t strt;
    long_t end;

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
            end = (long_t) StringLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (long_t) StringLength(argv[0]);
    }

    FAssert(end >= strt);

    return(ConvertStringToUtf8(AsString(argv[0])->String + strt, end - strt, 0));
}

// ---- SRFI 207: String-notated bytevectors ----

Define("make-bytestring", MakeBytestringPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("make-bytestring", argc);

    FObject lst = argv[0];
    long_t bsl = 0;

    while (PairP(lst))
    {
        FObject arg = First(lst);
        if (FixnumP(arg))
        {
            if (AsFixnum(arg) < 0 || AsFixnum(arg) > 255)
                RaiseExceptionC(Assertion, "make-bytestring", BytestringErrorSymbol,
                        "expected an integer between 0 and 155", List(arg));

            bsl += 1;
        }
        else if (CharacterP(arg))
        {
            if (AsCharacter(arg) >= 128)
                RaiseExceptionC(Assertion, "make-bytestring", BytestringErrorSymbol,
                        "expected an ascii character", List(arg));

            bsl += 1;
        }
        else if (BytevectorP(arg))
            bsl += BytevectorLength(arg);
        else if (StringP(arg))
            bsl += StringLength(arg);
        else
            RaiseExceptionC(Assertion, "make-bytestring", BytestringErrorSymbol,
                    "expected a character, integer, bytevector, or a string", List(arg));

        lst = Rest(lst);
    }

    if (lst != EmptyListObject)
        RaiseExceptionC(Assertion, "make-bytestring", "expected a valid list", List(argv[0]));

    FObject bv = MakeBytevector(bsl);
    FByte * bs = AsBytevector(bv)->Vector;
    long_t bdx = 0;

    lst = argv[0];
    while (PairP(lst))
    {
        FObject arg = First(lst);
        if (FixnumP(arg))
        {
            bs[bdx] = (FByte) AsFixnum(arg);
            bdx += 1;
        }
        else if (CharacterP(arg))
        {
            bs[bdx] = (FByte) AsCharacter(arg);
            bdx += 1;
        }
        else if (BytevectorP(arg))
        {
            memcpy(bs + bdx, AsBytevector(arg)->Vector, BytevectorLength(arg));
            bdx += BytevectorLength(arg);
        }
        else if (StringP(arg))
        {
            long_t sl = StringLength(arg);
            for (long_t sdx = 0; sdx < sl; sdx += 1)
            {
                FCh ch = AsString(arg)->String[sdx];
                if (ch >= 128)
                    RaiseExceptionC(Assertion, "make-bytestring", BytestringErrorSymbol,
                            "expected a string of ascii characters", List(arg));
                bs[bdx + sdx] = (FByte) ch;
            }

            bdx += sl;
        }
        else
        {
            FAssert(0);
        }

        lst = Rest(lst);
    }

    return(bv);
}

const char * hexdigit = "0123456789abcdef";

Define("bytevector->hex-string", BytevectorToHexStringPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("bytevector->hex-string", argc);
    BytevectorArgCheck("bytevector->hex-string", argv[0]);

    ulong_t vl = BytevectorLength(argv[0]);
    FObject ret = MakeStringCh(vl * 2, 0);
    FCh * s = AsString(ret)->String;
    FByte * v = AsBytevector(argv[0])->Vector;

    for (ulong_t vdx = 0; vdx < vl; vdx += 1)
    {
        s[vdx * 2] = hexdigit[(v[vdx] >> 4) & 0x0F];
        s[vdx * 2 + 1] = hexdigit[v[vdx] & 0x0F];
    }

    return(ret);
}

static FByte ConvertHexDigit(FCh ch)
{
    if (ch >= '0' && ch <= '9')
        return(ch - '0');
    else if (ch >= 'a' && ch <= 'f')
        return(ch - 'a' + 10);
    else if (ch >= 'A' && ch <= 'F')
        return(ch - 'A' + 10);

    RaiseExceptionC(Assertion, "hex-string->bytevector", BytestringErrorSymbol,
            "expected a hexidecimal digit", List(MakeCharacter(ch)));
    return(0);
}

Define("hex-string->bytevector", HexStringToBytevectorPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("hex-string->bytevector", argc);
    StringArgCheck("hex-string->bytevector", argv[0]);

    ulong_t sl = StringLength(argv[0]);

    if (sl % 2 != 0)
        RaiseExceptionC(Assertion, "hex-string->bytevector", BytestringErrorSymbol,
                "expected a string of pairs of hexidecimal digits", List(argv[0]));

    FObject ret = MakeBytevector(sl / 2, "hex-string->bytevector");
    FCh * s = AsString(argv[0])->String;
    FByte * v = AsBytevector(ret)->Vector;

    for (ulong_t sdx = 0; sdx < sl; sdx += 2)
        v[sdx / 2] = (ConvertHexDigit(s[sdx]) << 4) | ConvertHexDigit(s[sdx + 1]) ;

    return(ret);
}

const char * base64digit = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static FCh Base64Digit(ulong_t d, FCh digits[2])
{
    FAssert(d < 64);

    if (d > 61)
        return(digits[d - 62]);
    return(base64digit[d]);
}

Define("bytevector->base64", BytevectorToBase64Primitive)(long_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("bytevector->base64", argc);
    BytevectorArgCheck("bytevector->base64", argv[0]);

    FCh digits[2] = {'+', '/'};

    if (argc == 2)
    {
        if (StringP(argv[1]) == 0 || StringLength(argv[1]) != 2)
            RaiseExceptionC(Assertion, "bytevector->base64", "expected a two character string",
                    List(argv[1]));

        digits[0] = AsString(argv[1])->String[0];
        digits[1] = AsString(argv[1])->String[1];
    }

    ulong_t vl = BytevectorLength(argv[0]);
    FObject ret = MakeStringCh((vl / 3) * 4 + (vl % 3 == 0 ? 0 : 4), 0);
    FCh * s = AsString(ret)->String;
    FByte * v = AsBytevector(argv[0])->Vector;

    ulong_t sdx = 0;
    for (ulong_t vdx = 3; vdx <= vl; vdx += 3)
    {
        ulong_t d = (v[vdx - 3] << 16) | (v[vdx - 2] << 8) | v[vdx - 1];
        s[sdx] = Base64Digit((d >> 18) & 0x3F, digits);
        s[sdx + 1] = Base64Digit((d >> 12) & 0x3F, digits);
        s[sdx + 2] = Base64Digit((d >> 6) & 0x3F, digits);
        s[sdx + 3] = Base64Digit(d & 0x3F, digits);
        sdx += 4;
    }

    if (vl % 3 == 1)
    {
        ulong_t d = v[vl - 1] << 16;
        s[sdx] = Base64Digit((d >> 18) & 0x3F, digits);
        s[sdx + 1] = Base64Digit((d >> 12) & 0x3F, digits);
        s[sdx + 2] = '=';
        s[sdx + 3] = '=';
    } else if (vl % 3 == 2)
    {
        ulong_t d = (v[vl - 2] << 16) | (v[vl - 1] <<  8);
        s[sdx] = Base64Digit((d >> 18) & 0x3F, digits);
        s[sdx + 1] = Base64Digit((d >> 12) & 0x3F, digits);
        s[sdx + 2] = Base64Digit((d >> 6) & 0x3F, digits);
        s[sdx + 3] = '=';
    }

    return(ret);
}

static ulong_t SkipWhitespace(FCh * s, ulong_t sl, ulong_t sdx)
{
    while (sdx < sl && WhitespaceP(s[sdx]))
        sdx += 1;

    return(sdx);
}

static ulong_t ConvertBase64(FCh ch, FCh digits[2])
{
    if (ch == digits[0])
        return(62);
    else if (ch == digits[1])
        return(63);
    else if (ch >= 'A' && ch <= 'Z')
        return(ch - 'A');
    else if (ch >= 'a' && ch <= 'z')
        return(ch - 'a' + 26);
    else if (ch >= '0' && ch <= '9')
        return(ch - '0' + 52);

    RaiseExceptionC(Assertion, "base64->bytevector", BytestringErrorSymbol,
            "expected a base64 digit", List(MakeCharacter(ch)));
    return(0);
}

Define("base64->bytevector", Base64ToBytevectorPrimitive)(long_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("base64->bytevector", argc);
    StringArgCheck("base64->bytevector", argv[0]);

    FCh digits[2] = {'+', '/'};

    if (argc == 2)
    {
        if (StringP(argv[1]) == 0 || StringLength(argv[1]) != 2)
            RaiseExceptionC(Assertion, "base64->bytevector", "expected a two character string",
                    List(argv[1]));

        digits[0] = AsString(argv[1])->String[0];
        digits[1] = AsString(argv[1])->String[1];
    }

    ulong_t sl = StringLength(argv[0]);
    FCh * s = AsString(argv[0])->String;
    ulong_t sdx = 0;
    FObject lst = EmptyListObject;
    for (;;)
    {
        ulong_t d = 0;
        long_t eq = 0;
        for (long_t cnt = 0; cnt < 4; cnt += 1)
        {
            sdx = SkipWhitespace(s, sl, sdx);
            if (sdx == sl)
            {
                if (cnt > 0)
                    goto Failed;

                return(U8ListToBytevector(ReverseListModify(lst)));
            }

            if (eq > 0 && s[sdx] != '=')
                goto Failed;

            if (cnt > 1 && s[sdx] == '=')
            {
                d = d << 6;
                eq += 1;
            }
            else
                d = (d << 6) | ConvertBase64(s[sdx], digits);
            sdx += 1;
        }

        if (eq > 2)
            goto Failed;
        else if (eq > 0)
        {
            sdx = SkipWhitespace(s, sl, sdx);
            if (sdx < sl)
                goto Failed;
        }

        lst = MakePair(MakeFixnum((d >> 16) & 0xFF), lst);
        if (eq < 2)
            lst = MakePair(MakeFixnum((d >> 8) & 0xFF), lst);
        if (eq < 1)
            lst = MakePair(MakeFixnum(d & 0xFF), lst);
    }

Failed:
    RaiseExceptionC(Assertion, "base64->bytevector", BytestringErrorSymbol,
            "expected a base64 string", List(argv[0]));
    return(NoValueObject);
}

Define("bytestring->list", BytestringToListPrimitive)(long_t argc, FObject argv[])
{
    OneToThreeArgsCheck("bytestring->list", argc);
    BytevectorArgCheck("bytestring->list", argv[0]);

    long_t vdx = 0;
    long_t end = BytevectorLength(argv[0]);
    if (argc > 1)
    {
        IndexArgCheck("bytestring->list", argv[1], end);

        vdx = AsFixnum(argv[1]);
    }
    if (argc > 2)
    {
        EndIndexArgCheck("bytestring->list", argv[1], vdx, end);

        end = AsFixnum(argv[2]);
    }

    FByte * v = AsBytevector(argv[0])->Vector;
    FObject lst = EmptyListObject;
    while (vdx < end)
    {
        if (v[vdx] >= 32 && v[vdx] <= 127)
            lst = MakePair(MakeCharacter(v[vdx]), lst);
        else
            lst = MakePair(MakeFixnum(v[vdx]), lst);

        vdx += 1;
    }

    return(ReverseListModify(lst));
}

static int BytevectorCompare(FObject bv1, FObject bv2)
{
    FAssert(BytevectorP(bv1));
    FAssert(BytevectorP(bv2));

    ulong_t bvl1 = BytevectorLength(bv1);
    ulong_t bvl2 = BytevectorLength(bv2);

    if (bvl1 < bvl2)
    {
        int ret = memcmp(AsBytevector(bv1)->Vector, AsBytevector(bv2)->Vector, bvl1);
        return(ret > 0 ? 1 : -1);
    }
    else if (bvl1 > bvl2)
    {
        int ret = memcmp(AsBytevector(bv1)->Vector, AsBytevector(bv2)->Vector, bvl2);
        return(ret < 0 ? -1 : 1);
    }
    return(memcmp(AsBytevector(bv1)->Vector, AsBytevector(bv2)->Vector, bvl1));
}

Define("bytevector=?", BytevectorEqualPPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("bytevector=?", argc);
    BytevectorArgCheck("bytevector=?", argv[0]);
    BytevectorArgCheck("bytevector=?", argv[1]);

    return(BytevectorCompare(argv[0], argv[1]) == 0 ? TrueObject : FalseObject);
}

Define("bytevector<?", BytevectorLessThanPPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("bytevector<?", argc);
    BytevectorArgCheck("bytevector<?", argv[0]);
    BytevectorArgCheck("bytevector<?", argv[1]);

    return(BytevectorCompare(argv[0], argv[1]) < 0 ? TrueObject : FalseObject);
}

Define("bytevector>?", BytevectorGreaterThanPPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("bytevector>?", argc);
    BytevectorArgCheck("bytevector>?", argv[0]);
    BytevectorArgCheck("bytevector>?", argv[1]);

    return(BytevectorCompare(argv[0], argv[1]) > 0 ? TrueObject : FalseObject);
}

Define("bytevector<=?", BytevectorLessThanEqualPPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("bytevector<=?", argc);
    BytevectorArgCheck("bytevector<=?", argv[0]);
    BytevectorArgCheck("bytevector<=?", argv[1]);

    return(BytevectorCompare(argv[0], argv[1]) <= 0 ? TrueObject : FalseObject);
}

Define("bytevector>=?", BytevectorGreaterThanEqualPPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("bytevector>=?", argc);
    BytevectorArgCheck("bytevector>=?", argv[0]);
    BytevectorArgCheck("bytevector>=?", argv[1]);

    return(BytevectorCompare(argv[0], argv[1]) >= 0 ? TrueObject : FalseObject);
}

static FCh ReadBytestringCh(FObject port)
{
    FCh ch;
    if (ReadCh(port, &ch) == 0)
        RaiseExceptionC(Lexical, "read-textual-bytestring", BytestringErrorSymbol,
                "unexpected end-of-file reading bytestring", List(port));

    return(ch);
}

Define("read-textual-bytestring", ReadTextualBytestringPrimitive)(long_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("read-textual-bytestring", argc);
    FObject port = (argc == 2 ? argv[1] : CurrentInputPort());
    TextualInputPortArgCheck("read-textual-bytestring", port);

    if (argv[0] == FalseObject)
    {
        if (ReadBytestringCh(port) != '"')
            RaiseExceptionC(Lexical, "read-textual-bytestring", BytestringErrorSymbol,
                    "expected \" starting bytestring", List(port));
    }
    else
    {
        if (ReadBytestringCh(port) != '#')
            goto MissingPrefix;
        if (ReadBytestringCh(port) != 'u')
            goto MissingPrefix;
        if (ReadBytestringCh(port) != '8')
            goto MissingPrefix;
        if (ReadBytestringCh(port) != '"')
            goto MissingPrefix;
    }

    return(ReadBytestring(port));

MissingPrefix:
    RaiseExceptionC(Lexical, "read-textual-bytestring", BytestringErrorSymbol,
            "expected #u8\" starting bytestring", List(port));
    return(NoValueObject);
}

Define("write-textual-bytestring", WriteTextualBytestringPrimitive)(long_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("write-textual-bytestring", argc);
    BytevectorArgCheck("write-textual-bytestring", argv[0]);
    FObject port = (argc == 2 ? argv[1] : CurrentOutputPort());
    TextualOutputPortArgCheck("write-textual-bytestring", port);

    WriteStringC(port, "#u8\"");
    for (ulong_t idx = 0; idx < BytevectorLength(argv[0]); idx += 1)
    {
        FByte b = AsBytevector(argv[0])->Vector[idx];
        switch (b)
        {
        case 0x07: WriteStringC(port, "\\a"); break;
        case 0x08: WriteStringC(port, "\\b"); break;
        case 0x09: WriteStringC(port, "\\t"); break;
        case 0x0A: WriteStringC(port, "\\n"); break;
        case 0x0D: WriteStringC(port, "\\r"); break;
        case 0x22: WriteStringC(port, "\\\""); break;
        case 0x5C: WriteStringC(port, "\\\\"); break;
        case 0x7C: WriteStringC(port, "\\|"); break;
        default:
            if (b >= 0x20 && b <= 0x7E)
                WriteCh(port, b);
            else
            {
                FCh s[4];
                long_t sl = FixnumAsString((long_t) b, s, 16);

                WriteStringC(port, "\\x");
                WriteString(port, s, sl);
                WriteCh(port, ';');
            }
        }
    }
    WriteCh(port, '"');
    return(NoValueObject);
}

static FObject Primitives[] =
{
    VectorPPrimitive,
    MakeVectorPrimitive,
    VectorPrimitive,
    VectorLengthPrimitive,
    VectorRefPrimitive,
    VectorSetPrimitive,
    VectorSwapPrimitive,
    VectorToListPrimitive,
    ListToVectorPrimitive,
    VectorToStringPrimitive,
    StringToVectorPrimitive,
    VectorCopyPrimitive,
    VectorCopyModifyPrimitive,
    VectorReverseCopyPrimitive,
    VectorReverseCopyModifyPrimitive,
    VectorAppendPrimitive,
    VectorAppendSubvectorsPrimitive,
    VectorFillPrimitive,
    VectorReversePrimitive,
    BytevectorPPrimitive,
    MakeBytevectorPrimitive,
    BytevectorPrimitive,
    BytevectorLengthPrimitive,
    BytevectorU8RefPrimitive,
    BytevectorU8SetPrimitive,
    BytevectorCopyPrimitive,
    BytevectorCopyModifyPrimitive,
    BytevectorAppendPrimitive,
    Utf8ToStringPrimitive,
    StringToUtf8Primitive,
    MakeBytestringPrimitive,
    BytevectorToHexStringPrimitive,
    HexStringToBytevectorPrimitive,
    BytevectorToBase64Primitive,
    Base64ToBytevectorPrimitive,
    BytestringToListPrimitive,
    BytevectorEqualPPrimitive,
    BytevectorLessThanPPrimitive,
    BytevectorGreaterThanPPrimitive,
    BytevectorLessThanEqualPPrimitive,
    BytevectorGreaterThanEqualPPrimitive,
    ReadTextualBytestringPrimitive,
    WriteTextualBytestringPrimitive
};

void SetupVectors()
{
    for (ulong_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(Bedrock, BedrockLibrary, Primitives[idx]);
}
