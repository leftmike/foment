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

Define("vector-length", VectorLengthPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "vector-length", "vector-length: expected one argument",
                EmptyListObject);

    if (VectorP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "vector-length",
                "vector-length: expected a vector", List(argv[0]));

    return(MakeFixnum(VectorLength(argv[0])));
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
            RaiseExceptionC(R.Assertion, "u8-list->bytevector", "u8-list->bytevector: not a byte",
                    List(First(obj)));
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
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "bytevector?", "bytevector?: expected one argument",
                EmptyListObject);

    return(BytevectorP(argv[0]) ? TrueObject : FalseObject);
}

static inline int ByteP(FObject obj)
{
    return(FixnumP(obj) && AsFixnum(obj) >= 0 && AsFixnum(obj) < 256);
}

Define("make-bytevector", MakeBytevectorPrimitive)(int argc, FObject argv[])
{
    if (argc < 1 || argc > 2)
        RaiseExceptionC(R.Assertion, "make-bytevector",
                "make-bytevector: expected one or two arguments", EmptyListObject);

    if (FixnumP(argv[0]) == 0 || AsFixnum(argv[0]) < 0)
        RaiseExceptionC(R.Assertion, "make-bytevector",
                "make-bytevector: expected an exact non-negative integer", List(argv[0]));

    if (argc == 2 && ByteP(argv[1]) == 0)
        RaiseExceptionC(R.Assertion, "make-bytevector", "make-bytevector: expected a byte",
                List(argv[1]));

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
        if (ByteP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, "bytevector", "bytevector: expected a byte",
                    List(argv[adx]));

        AsBytevector(bv)->Vector[adx] = (FByte) AsFixnum(argv[adx]);
    }

    return(bv);
}

Define("bytevector-length", BytevectorLengthPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "bytevector-length",
                "bytevector-length: expected one argument", EmptyListObject);

    if (BytevectorP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "bytevector-length",
                "bytevector-length: expected a bytevector", List(argv[0]));

    return(MakeFixnum(BytevectorLength(argv[0])));
}

Define("bytevector-u8-ref", BytevectorU8RefPrimitive)(int argc, FObject argv[])
{
    if (argc != 2)
        RaiseExceptionC(R.Assertion, "bytevector-u8-ref",
                "bytevector-u8-ref: expected two arguments", EmptyListObject);

    if (BytevectorP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "bytevector-u8-ref",
                "bytevector-u8-ref: expected a bytevector", List(argv[0]));

    if (FixnumP(argv[1]) == 0 || AsFixnum(argv[1]) < 0
            || AsFixnum(argv[1]) >= (int) BytevectorLength(argv[0]))
        RaiseExceptionC(R.Assertion, "bytevector-u8-ref",
                "bytevector-u8-ref: expected a valid index", List(argv[1]));

    return(MakeFixnum(AsBytevector(argv[0])->Vector[AsFixnum(argv[1])]));
}

Define("bytevector-u8-set!", BytevectorU8SetPrimitive)(int argc, FObject argv[])
{
    if (argc != 3)
        RaiseExceptionC(R.Assertion, "bytevector-u8-set!",
                "bytevector-u8-set!: expected three arguments", EmptyListObject);

    if (BytevectorP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "bytevector-u8-set!",
                "bytevector-u8-set!: expected a bytevector", List(argv[0]));

    if (FixnumP(argv[1]) == 0 || AsFixnum(argv[1]) < 0
            || AsFixnum(argv[1]) >= (int) BytevectorLength(argv[0]))
        RaiseExceptionC(R.Assertion, "bytevector-u8-set!",
                "bytevector-u8-set!: expected a valid index", List(argv[1]));

    if (ByteP(argv[2]) == 0)
        RaiseExceptionC(R.Assertion, "bytevector-u8-set!", "bytevector-u8-set!: expected a byte",
                    List(argv[2]));

    AsBytevector(argv[0])->Vector[AsFixnum(argv[1])] = (FByte) AsFixnum(argv[2]);
    return(NoValueObject);
}

Define("bytevector-copy", BytevectorCopyPrimitive)(int argc, FObject argv[])
{
    int strt;
    int end;

    if (argc < 1 || argc > 3)
        RaiseExceptionC(R.Assertion, "bytevector-copy",
                "bytevector-copy: expected one to three arguments", EmptyListObject);

    if (BytevectorP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "bytevector-copy",
                "bytevector-copy: expected a bytevector", List(argv[0]));

    if (argc > 1)
    {
        if (FixnumP(argv[1]) == 0 || AsFixnum(argv[1]) < 0
            || AsFixnum(argv[1]) >= (int) BytevectorLength(argv[0]))
            RaiseExceptionC(R.Assertion, "bytevector-copy",
                    "bytevector-copy: expected a valid index", List(argv[1]));

        strt = AsFixnum(argv[1]);

        if (argc > 2)
        {
            if (FixnumP(argv[2]) == 0 || AsFixnum(argv[2]) < strt
                || AsFixnum(argv[2]) >= (int) BytevectorLength(argv[0]))
                RaiseExceptionC(R.Assertion, "bytevector-copy",
                        "bytevector-copy: expected a valid index", List(argv[2]));

            end = AsFixnum(argv[2]);
        }
        else
            end = (int) BytevectorLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (int) BytevectorLength(argv[0]);
    }

    FAssert(end >= strt);

    FObject bv = MakeBytevector(end - strt);
    memcpy(AsBytevector(bv)->Vector, AsBytevector(argv[0])->Vector + strt, end - strt);

    return(bv);
}

Define("bytevector-copy!", BytevectorCopyModifyPrimitive)(int argc, FObject argv[])
{
    int strt;
    int end;

    if (argc < 3 || argc > 5)
        RaiseExceptionC(R.Assertion, "bytevector-copy!",
                "bytevector-copy!: expected three to five arguments", EmptyListObject);

    if (BytevectorP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "bytevector-copy!",
                "bytevector-copy!: expected a bytevector", List(argv[0]));

    if (FixnumP(argv[1]) == 0 || AsFixnum(argv[1]) < 0
        || AsFixnum(argv[1]) >= (int) BytevectorLength(argv[0]))
        RaiseExceptionC(R.Assertion, "bytevector-copy!",
                "bytevector-copy!: expected a valid index", List(argv[1]));

    if (BytevectorP(argv[2]) == 0)
        RaiseExceptionC(R.Assertion, "bytevector-copy!",
                "bytevector-copy!: expected a bytevector", List(argv[2]));

    if (argc > 3)
    {
        if (FixnumP(argv[3]) == 0 || AsFixnum(argv[3]) < 0
            || AsFixnum(argv[3]) >= (int) BytevectorLength(argv[2]))
            RaiseExceptionC(R.Assertion, "bytevector-copy!",
                    "bytevector-copy!: expected a valid index", List(argv[3]));

        strt = AsFixnum(argv[3]);

        if (argc > 4)
        {
            if (FixnumP(argv[4]) == 0 || AsFixnum(argv[4]) < strt
                || AsFixnum(argv[4]) >= (int) BytevectorLength(argv[2]))
                RaiseExceptionC(R.Assertion, "bytevector-copy!",
                        "bytevector-copy!: expected a valid index", List(argv[4]));

            end = AsFixnum(argv[4]);
        }
        else
            end = (int) BytevectorLength(argv[2]);
    }
    else
    {
        strt = 0;
        end = (int) BytevectorLength(argv[2]);
    }

    if ((int) BytevectorLength(argv[0]) - AsFixnum(argv[1]) < end - strt)
        RaiseExceptionC(R.Assertion, "bytevector-copy!",
                "bytevector-copy!: expected a valid index", List(argv[1]));

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
        if (BytevectorP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, "bytevector-append",
                    "bytevector-append: expected a bytevector", List(argv[adx]));

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

    if (argc < 1 || argc > 3)
        RaiseExceptionC(R.Assertion, "utf8->string",
                "utf8->string: expected one to three arguments", EmptyListObject);

    if (BytevectorP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "utf8->string",
                "utf8->string: expected a bytevector", List(argv[0]));

    if (argc > 1)
    {
        if (FixnumP(argv[1]) == 0 || AsFixnum(argv[1]) < 0
            || AsFixnum(argv[1]) >= (int) BytevectorLength(argv[0]))
            RaiseExceptionC(R.Assertion, "utf8->string",
                    "utf8->string: expected a valid index", List(argv[1]));

        strt = AsFixnum(argv[1]);

        if (argc > 2)
        {
            if (FixnumP(argv[2]) == 0 || AsFixnum(argv[2]) < strt
                || AsFixnum(argv[2]) >= (int) BytevectorLength(argv[0]))
                RaiseExceptionC(R.Assertion, "utf8->string",
                        "utf8->string: expected a valid index", List(argv[2]));

            end = AsFixnum(argv[2]);
        }
        else
            end = (int) BytevectorLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (int) BytevectorLength(argv[0]);
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

    if (argc < 1 || argc > 3)
        RaiseExceptionC(R.Assertion, "string->utf8",
                "string->utf8: expected one to three arguments", EmptyListObject);

    if (StringP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "string->utf8",
                "string->utf8: expected a string", List(argv[0]));

    if (argc > 1)
    {
        if (FixnumP(argv[1]) == 0 || AsFixnum(argv[1]) < 0
            || AsFixnum(argv[1]) >= (int) StringLength(argv[0]))
            RaiseExceptionC(R.Assertion, "string->utf8",
                    "string->utf8: expected a valid index", List(argv[1]));

        strt = AsFixnum(argv[1]);

        if (argc > 2)
        {
            if (FixnumP(argv[2]) == 0 || AsFixnum(argv[2]) < strt
                || AsFixnum(argv[2]) >= (int) StringLength(argv[0]))
                RaiseExceptionC(R.Assertion, "string->utf8",
                        "string->utf8: expected a valid index", List(argv[2]));

            end = AsFixnum(argv[2]);
        }
        else
            end = (int) StringLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (int) StringLength(argv[0]);
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
    &VectorLengthPrimitive,
    &VectorRefPrimitive,
    &VectorSetPrimitive,
    &ListToVectorPrimitive,
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
