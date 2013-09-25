/*

Foment

*/

#include <string.h>
#include "foment.hpp"

// ---- Vectors ----

static FVector * MakeVector(unsigned int vl, char * who, int * mf)
{
    FVector * nv = (FVector *) MakeObject(sizeof(FVector) + (vl - 1) * sizeof(FObject), VectorTag);
    if (nv == 0)
    {
        nv = (FVector *) MakeMatureObject(sizeof(FVector) + (vl - 1) * sizeof(FObject), who);
        nv->Length = MakeMatureLength(vl, VectorTag);
        *mf = 1;
    }
    else
        nv->Length = MakeLength(vl, VectorTag);

    return(nv);
}

FObject MakeVector(unsigned int vl, FObject * v, FObject obj)
{
    int mf = 0;
    FVector * nv = MakeVector(vl, "make-vector", &mf);

    unsigned int idx;
    if (v == 0)
    {
        if (mf && ObjectP(obj))
            for (idx = 0; idx < vl; idx++)
                ModifyVector(nv, idx, obj);
        else
            for (idx = 0; idx < vl; idx++)
                nv->Vector[idx] = obj;
    }
    else
    {
        if (mf)
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
    int mf = 0;
    unsigned int vl = ListLength(obj);
    FVector * nv = MakeVector(vl, "list->vector", &mf);

    if (mf)
        for (unsigned int idx = 0; idx < vl; idx++)
        {
            ModifyVector(nv, idx, First(obj));
            obj = Rest(obj);
        }
    else
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
    OneArgCheck("vector?", argc);

    return(VectorP(argv[0]) ? TrueObject : FalseObject);
}

Define("make-vector", MakeVectorPrimitive)(int argc, FObject argv[])
{
    OneOrTwoArgsCheck("make-vector", argc);
    NonNegativeArgCheck("make-vector", argv[0]);

    return(MakeVector(AsFixnum(argv[0]), 0, argc == 2 ? argv[1] : NoValueObject));
}

Define("vector", VectorPrimitive)(int argc, FObject argv[])
{
    int mf = 0;
    FObject v = MakeVector(argc, "vector", &mf);

    if (mf)
    {
        for (int adx = 0; adx < argc; adx++)
            ModifyVector(v, adx, argv[adx]);
    }
    else
    {
        for (int adx = 0; adx < argc; adx++)
            AsVector(v)->Vector[adx] = argv[adx];
    }

    return(v);
}

Define("vector-length", VectorLengthPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("vector-length", argc);
    VectorArgCheck("vector-length", argv[0]);

    return(MakeFixnum(VectorLength(argv[0])));
}

Define("vector-ref", VectorRefPrimitive)(int argc, FObject argv[])
{
    TwoArgsCheck("vector-ref", argc);
    VectorArgCheck("vector-ref", argv[0]);
    IndexArgCheck("vector-ref", argv[1], VectorLength(argv[0]));

    return(AsVector(argv[0])->Vector[AsFixnum(argv[1])]);
}

Define("vector-set!", VectorSetPrimitive)(int argc, FObject argv[])
{
    ThreeArgsCheck("vector-set!", argc);
    VectorArgCheck("vector-set!", argv[0]);
    IndexArgCheck("vector-set!", argv[1], VectorLength(argv[0]));

//    AsVector(argv[0])->Vector[AsFixnum(argv[1])] = argv[2];
    ModifyVector(argv[0], AsFixnum(argv[1]), argv[2]);
    return(NoValueObject);
}

Define("vector->list", VectorToListPrimitive)(int argc, FObject argv[])
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

Define("list->vector", ListToVectorPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("list->vector", argc);

    return(ListToVector(argv[0]));
}

Define("vector->string", VectorToStringPrimitive)(int argc, FObject argv[])
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

Define("string->vector", StringToVectorPrimitive)(int argc, FObject argv[])
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

    int mf = 0;
    FObject v = MakeVector(end - strt, "string->vector", &mf);

    for (FFixnum idx = 0; idx < end - strt; idx ++)
        AsVector(v)->Vector[idx] = MakeCharacter(AsString(argv[0])->String[idx + strt]);

    return(v);
}

Define("vector-copy", VectorCopyPrimitive)(int argc, FObject argv[])
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

Define("vector-copy!", VectorCopyModifyPrimitive)(int argc, FObject argv[])
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

Define("vector-append", VectorAppendPrimitive)(int argc, FObject argv[])
{
    int len = 0;

    for (int adx = 0; adx < argc; adx++)
    {
        VectorArgCheck("vector-append", argv[adx]);

        len += VectorLength(argv[adx]);
    }

    int mf = 0;
    FObject v = MakeVector(len, "vector-append", &mf);
    int idx = 0;

    for (int adx = 0; adx < argc; adx++)
    {
        for (int vdx = 0; vdx < (int) VectorLength(argv[adx]); vdx++)
            AsVector(v)->Vector[idx + vdx] = AsVector(argv[adx])->Vector[vdx];

        idx += VectorLength(argv[adx]);
    }

    return(v);
}

Define("vector-fill!", VectorFillPrimitive)(int argc, FObject argv[])
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

    FAssert(BytevectorLength(nv) == vl);
    return(nv);
}

FObject MakeBytevector(unsigned int vl)
{
    return(MakeBytevector(vl, "make-bytevector"));
}

FObject U8ListToBytevector(FObject obj)
{
    unsigned int vl = ListLength(obj);
    FBytevector * nv = MakeBytevector(vl, "list->bytevector");

    for (unsigned int idx = 0; idx < vl; idx++)
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

Define("bytevector?", BytevectorPPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("bytevector?", argc);

    return(BytevectorP(argv[0]) ? TrueObject : FalseObject);
}

Define("make-bytevector", MakeBytevectorPrimitive)(int argc, FObject argv[])
{
    OneOrTwoArgsCheck("make-bytevector", argc);
    NonNegativeArgCheck("make-bytevector", argv[0]);

    if (argc == 2)
        ByteArgCheck("make-bytevector", argv[1]);

    FObject bv = MakeBytevector(AsFixnum(argv[0]));

    if (argc == 2)
        for (int bdx = 0; bdx < AsFixnum(argv[0]); bdx++)
            AsBytevector(bv)->Vector[bdx] = (FByte) AsFixnum(argv[1]);

    return(bv);
}

Define("bytevector", BytevectorPrimitive)(int argc, FObject argv[])
{
    FObject bv = MakeBytevector(argc);

    for (int adx = 0; adx < argc; adx++)
    {
        ByteArgCheck("bytevector", argv[adx]);

        AsBytevector(bv)->Vector[adx] = (FByte) AsFixnum(argv[adx]);
    }

    return(bv);
}

Define("bytevector-length", BytevectorLengthPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("bytevector-length", argc);
    BytevectorArgCheck("bytevector-length",argv[0]);

    return(MakeFixnum(BytevectorLength(argv[0])));
}

Define("bytevector-u8-ref", BytevectorU8RefPrimitive)(int argc, FObject argv[])
{
    TwoArgsCheck("bytevector-u8-ref", argc);
    BytevectorArgCheck("bytevector-u8-ref", argv[0]);
    IndexArgCheck("bytevector-u8-ref", argv[1], BytevectorLength(argv[0]));

    return(MakeFixnum(AsBytevector(argv[0])->Vector[AsFixnum(argv[1])]));
}

Define("bytevector-u8-set!", BytevectorU8SetPrimitive)(int argc, FObject argv[])
{
    ThreeArgsCheck("bytevector-u8-set!", argc);
    BytevectorArgCheck("bytevector-u8-set!", argv[0]);
    IndexArgCheck("bytevector-u8-set!", argv[1], BytevectorLength(argv[0]));
    ByteArgCheck("bytevector-u8-set!", argv[2]);

    AsBytevector(argv[0])->Vector[AsFixnum(argv[1])] = (FByte) AsFixnum(argv[2]);
    return(NoValueObject);
}

Define("bytevector-copy", BytevectorCopyPrimitive)(int argc, FObject argv[])
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

Define("bytevector-copy!", BytevectorCopyModifyPrimitive)(int argc, FObject argv[])
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

Define("bytevector-append", BytevectorAppendPrimitive)(int argc, FObject argv[])
{
    int len = 0;

    for (int adx = 0; adx < argc; adx++)
    {
        BytevectorArgCheck("bytevector-append", argv[adx]);

        len += BytevectorLength(argv[adx]);
    }

    FObject bv = MakeBytevector(len);
    int idx = 0;

    for (int adx = 0; adx < argc; adx++)
    {
        memcpy(AsBytevector(bv)->Vector + idx, AsBytevector(argv[adx])->Vector,
                BytevectorLength(argv[adx]));
        idx += BytevectorLength(argv[adx]);
    }

    return(bv);
}

static const unsigned char Utf8TrailingBytes[256] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 4, 4,
    4, 4, 5, 5, 5, 5
};

static const unsigned long Utf8Magic[6] =
{
    0x00000000UL,
    0x00003080UL,
    0x000E2080UL,
    0x03C82080UL,
    0xFA082080UL,
    0x82082080UL
};

static const unsigned char Utf8ByteMark[7] =
{
    0x00,
    0x00,
    0xC0,
    0xE0,
    0xF0,
    0xF8,
    0xFC
};

static int ChLengthOfUtf8(FByte * bv, int bvl)
{
    int sl = 0;

    for (int bdx = 0; bdx < bvl; sl++)
        bdx += Utf8TrailingBytes[bv[bdx]] + 1;

    return(sl);
}

static FCh Utf8ToCh(FByte * bv)
{
    int eb;
    FCh ch;

    eb = Utf8TrailingBytes[*bv];
    ch = 0;
    switch (eb)
    {
        case 3:
            ch += *bv;
            ch <<= 6;
            bv += 1;

        case 2:
            ch += *bv;
            ch <<= 6;
            bv += 1;

        case 1:
            ch += *bv;
            ch <<= 6;
            bv += 1;

        case 0:
            ch += *bv;
    }

    ch -= Utf8Magic[eb];
    if (ch > 0x7FFFFFFF)
        ch = 0x0000FFFD;

    return(ch);
}

static void Utf8ToString(FByte * bv, int bvl, FCh * s)
{
    int eb;

    for (int bdx = 0; bdx < bvl; s++)
    {
        eb = Utf8TrailingBytes[bv[bdx]];
        if (bdx + eb >= bvl)
            break;

        *s = Utf8ToCh(bv + bdx);
        bdx += eb + 1;
    }
}

static int Utf8LengthOfCh(FCh * s, int sl)
{
    int bvl = 0;

    for (int sdx = 0; sdx < sl; sdx++)
    {
        if (s[sdx] < 0x80UL)
            bvl += 1;
        else if (s[sdx] < 0x800UL)
            bvl += 2;
        else if (s[sdx] < 0x10000UL)
            bvl += 3;
        else if (s[sdx] < 0x200000UL)
            bvl += 4;
        else
            bvl += 2;
    }

    return(bvl);
}

static void StringToUtf8(FCh * s, int sl, FByte * bv)
{
    for (int sdx = 0; sdx < sl; sdx++)
    {
        FCh ch = s[sdx];
        int btw;

        if (ch < 0x80UL)
            btw = 1;
        else if (ch < 0x800UL)
            btw = 2;
        else if (ch < 0x10000UL)
            btw = 3;
        else if (ch < 0x200000UL)
            btw = 4;
        else
        {
            btw = 2;
            ch = 0x0000FFFDUL;
        }

        switch (btw)
        {
            case 4:
                bv[3] = (unsigned char) ((ch | 0x80UL) & 0xBFUL);
                ch >>= 6;

            case 3:
                bv[2] = (unsigned char) ((ch | 0x80UL) & 0xBFUL);
                ch >>= 6;

            case 2:
                bv[1] = (unsigned char) ((ch | 0x80UL) & 0xBFUL);
                ch >>= 6;

            case 1:
                bv[0] = (unsigned char) (ch | Utf8ByteMark[btw]);
        }

        bv += btw;
    }
}

Define("utf8->string", Utf8ToStringPrimitive)(int argc, FObject argv[])
{
    int strt;
    int end;

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

    int sl = ChLengthOfUtf8(AsBytevector(argv[0])->Vector + strt, end - strt);
    FObject s = MakeString(0, sl);

    Utf8ToString(AsBytevector(argv[0])->Vector + strt, end - strt, AsString(s)->String);

    return(s);
}

Define("string->utf8", StringToUtf8Primitive)(int argc, FObject argv[])
{
    int strt;
    int end;

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

    int bvl = Utf8LengthOfCh(AsString(argv[0])->String + strt, end - strt);
    FObject bv = MakeBytevector(bvl);

    StringToUtf8(AsString(argv[0])->String + strt, end - strt, AsBytevector(bv)->Vector);

    return(bv);
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
    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
