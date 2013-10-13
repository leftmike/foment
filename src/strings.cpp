/*

Foment

*/

#include <string.h>
#include "foment.hpp"
#include "unicode.hpp"

// ---- Strings ----

FObject MakeString(FCh * s, unsigned int sl)
{
    FString * ns = (FString *) MakeObject(sizeof(FString) + sl * sizeof(FCh), StringTag);
    if (ns == 0)
    {
        ns = (FString *) MakeMatureObject(sizeof(FString) + sl * sizeof(FCh), "make-string");
        ns->Length = MakeMatureLength(sl * sizeof(FCh), StringTag);
    }
    else
        ns->Length = MakeLength(sl * sizeof(FCh), StringTag);
    ns->String[sl] = 0;

    if (s != 0)
        for (unsigned int idx = 0; idx < sl; idx++)
            ns->String[idx] = s[idx];

    FObject obj = ns;
    FAssert(StringLength(obj) == sl);
    return(obj);
}

FObject MakeStringCh(unsigned int sl, FCh ch)
{
    FString * s = AsString(MakeString(0, sl));

    for (unsigned int idx = 0; idx < sl; idx++)
        s->String[idx] = ch;

    return(s);
}

FObject MakeStringF(FString * s)
{
    FString * ns = AsString(MakeString(0, StringLength(s)));

    for (unsigned int idx = 0; idx < StringLength(s); idx++)
        ns->String[idx] = s->String[idx];

    return(ns);
}

FObject MakeStringC(char * s)
{
    int sl = strlen(s);
    FString * ns = AsString(MakeString(0, sl));

    int idx;
    for (idx = 0; idx < sl; idx++)
        ns->String[idx] = s[idx];

    return(ns);
}

void StringToC(FObject s, char * b, int bl)
{
    int idx;

    FAssert(StringP(s));

    for (idx = 0; idx < bl - 1; idx++)
    {
        if (idx == (int) StringLength(s))
            break;

        b[idx] = (char) AsString(s)->String[idx];
    }

    FAssert(idx < bl - 1);
    b[idx] = 0;
}

FObject FoldcaseString(FObject s)
{
    FAssert(StringP(s));

    unsigned int sl = StringLength(s);
    unsigned int nsl = 0;

    for (unsigned int idx = 0; idx < sl; idx++)
        nsl += CharFullfoldLength(AsString(s)->String[idx]);

    FString * ns = AsString(MakeString(0, nsl));

    if (nsl == sl)
    {
        for (unsigned int idx = 0; idx < sl; idx++)
            ns->String[idx] = CharFoldcase(AsString(s)->String[idx]);
    }
    else
    {
        unsigned int ndx = 0;
        for (unsigned int idx = 0; idx < sl; idx++)
        {
            unsigned int fcl = CharFullfoldLength(AsString(s)->String[idx]);
            if (fcl == 1)
            {
                ns->String[ndx] = CharFoldcase(AsString(s)->String[idx]);
                ndx += 1;
            }
            else
            {
                FAssert(fcl == 2 || fcl == 3);

                FCh * fc = CharFullfold(AsString(s)->String[idx]);

                ns->String[ndx] = fc[0];
                ndx += 1;
                ns->String[ndx] = fc[1];
                ndx += 1;
                if (fcl == 3)
                {
                    ns->String[ndx] = fc[2];
                    ndx += 1;
                }
            }
        }
    }

    return(ns);
}

static FObject UpcaseString(FObject s)
{
    FAssert(StringP(s));

    unsigned int sl = StringLength(s);
    unsigned int nsl = 0;

    for (unsigned int idx = 0; idx < sl; idx++)
        nsl += CharFullupLength(AsString(s)->String[idx]);

    FString * ns = AsString(MakeString(0, nsl));

    if (nsl == sl)
    {
        for (unsigned int idx = 0; idx < sl; idx++)
            ns->String[idx] = CharUpcase(AsString(s)->String[idx]);
    }
    else
    {
        unsigned int ndx = 0;
        for (unsigned int idx = 0; idx < sl; idx++)
        {
            unsigned int fcl = CharFullupLength(AsString(s)->String[idx]);
            if (fcl == 1)
            {
                ns->String[ndx] = CharUpcase(AsString(s)->String[idx]);
                ndx += 1;
            }
            else
            {
                FAssert(fcl == 2 || fcl == 3);

                FCh * fc = CharFullup(AsString(s)->String[idx]);

                ns->String[ndx] = fc[0];
                ndx += 1;
                ns->String[ndx] = fc[1];
                ndx += 1;
                if (fcl == 3)
                {
                    ns->String[ndx] = fc[2];
                    ndx += 1;
                }
            }
        }
    }

    return(ns);
}

static FObject DowncaseString(FObject s)
{
    FAssert(StringP(s));

    unsigned int sl = StringLength(s);
    unsigned int nsl = 0;

    for (unsigned int idx = 0; idx < sl; idx++)
        nsl += CharFulldownLength(AsString(s)->String[idx]);

    FString * ns = AsString(MakeString(0, nsl));

    if (nsl == sl)
    {
        for (unsigned int idx = 0; idx < sl; idx++)
            ns->String[idx] = CharDowncase(AsString(s)->String[idx]);
    }
    else
    {
        unsigned int ndx = 0;
        for (unsigned int idx = 0; idx < sl; idx++)
        {
            unsigned int fcl = CharFulldownLength(AsString(s)->String[idx]);
            if (fcl == 1)
            {
                ns->String[ndx] = CharDowncase(AsString(s)->String[idx]);
                ndx += 1;
            }
            else
            {
                FAssert(fcl == 2 || fcl == 3);

                FCh * fc = CharFulldown(AsString(s)->String[idx]);

                ns->String[ndx] = fc[0];
                ndx += 1;
                ns->String[ndx] = fc[1];
                ndx += 1;
                if (fcl == 3)
                {
                    ns->String[ndx] = fc[2];
                    ndx += 1;
                }
            }
        }
    }

    return(ns);
}

unsigned int ByteLengthHash(char * b, int bl)
{
    unsigned int h = 0;

    for (; bl > 0; b++, bl--)
        h = ((h << 5) + h) + *b;

    return(h);
}

unsigned int StringLengthHash(FCh * s, int sl)
{
    return(ByteLengthHash((char *) s, sl * sizeof(FCh)));
}

unsigned int StringHash(FObject obj)
{
    FAssert(StringP(obj));

    return(StringLengthHash(AsString(obj)->String, StringLength(obj)));
}

static int StringCompare(FString * str1, FString * str2)
{
    for (unsigned int sdx = 0; sdx < StringLength(str1) && sdx < StringLength(str2); sdx++)
        if (str1->String[sdx] != str2->String[sdx])
            return(str1->String[sdx] - str2->String[sdx]);

    return(StringLength(str1) - StringLength(str2));
}

static int StringCiCompare(FString * str1, FString * str2)
{
    for (unsigned int sdx = 0; sdx < StringLength(str1) && sdx < StringLength(str2); sdx++)
    {
        FCh ch1 = CharFoldcase(str1->String[sdx]);
        FCh ch2 = CharFoldcase(str2->String[sdx]);

        if (ch1 != ch2)
            return(ch1 - ch2);
    }

    return(StringLength(str1) - StringLength(str2));
}

int StringEqualP(FObject obj1, FObject obj2)
{
    FAssert(StringP(obj1));
    FAssert(StringP(obj2));

    if (StringCompare(AsString(obj1), AsString(obj2)) == 0)
        return(1);
    return(0);
}

int StringLengthEqualP(FCh * s, int sl, FObject obj)
{
    FAssert(StringP(obj));
    int sdx;

    if (sl !=  StringLength(obj))
        return(0);

    for (sdx = 0; sdx < sl; sdx++)
        if (s[sdx] != AsString(obj)->String[sdx])
            return(0);

    return(1);
}

int StringCEqualP(char * s1, FCh * s2, int sl2)
{
    int sl1 = strlen(s1);
    if (sl1 != sl2)
        return(0);

    for (int sdx = 0; sdx < sl1; sdx++)
        if (s1[sdx] != s2[sdx])
            return(0);

    return(1);
}

Define("string?", StringPPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("string?", argc);

    return(StringP(argv[0]) ? TrueObject : FalseObject);
}

Define("make-string", MakeStringPrimitive)(int argc, FObject argv[])
{
    OneOrTwoArgsCheck("make-string", argc);
    NonNegativeArgCheck("make-string", argv[0]);

    if (argc == 2)
    {
        CharacterArgCheck("make-string", argv[1]);

        return(MakeStringCh(AsFixnum(argv[0]), AsCharacter(argv[1])));
    }

    return(MakeString(0, AsFixnum(argv[0])));
}

Define("string", StringPrimitive)(int argc, FObject argv[])
{
    FObject s = MakeString(0, argc);

    for (int adx = 0; adx < argc; adx++)
    {
        CharacterArgCheck("string", argv[adx]);

        AsString(s)->String[adx] = AsCharacter(argv[adx]);
    }

    return(s);
}

Define("string-length", StringLengthPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("string-length", argc);
    StringArgCheck("string-length", argv[0]);

    return(MakeFixnum(StringLength(argv[0])));
}

Define("string-ref", StringRefPrimitive)(int argc, FObject argv[])
{
    TwoArgsCheck("string-ref", argc);
    StringArgCheck("string-ref", argv[0]);
    IndexArgCheck("string-ref", argv[1], StringLength(argv[0]));

    return(MakeCharacter(AsString(argv[0])->String[AsFixnum(argv[1])]));
}

Define("string-set!", StringSetPrimitive)(int argc, FObject argv[])
{
    ThreeArgsCheck("string-set!", argc);
    StringArgCheck("string-set!", argv[0]);
    IndexArgCheck("string-set!", argv[1], StringLength(argv[0]));
    CharacterArgCheck("string-set!", argv[2]);

    AsString(argv[0])->String[AsFixnum(argv[1])] = AsCharacter(argv[2]);
    return(NoValueObject);
}

Define("string=?", StringEqualPPrimitive)(int argc, FObject argv[])
{
    AtLeastTwoArgsCheck("string=?", argc);
    StringArgCheck("string=?", argv[0]);

    for (int adx = 1; adx < argc; adx++)
    {
        StringArgCheck("string=?", argv[adx]);

        if (StringCompare(AsString(argv[adx - 1]), AsString(argv[adx])) != 0)
            return(FalseObject);
    }

    return(TrueObject);
}

Define("string<?", StringLessThanPPrimitive)(int argc, FObject argv[])
{
    AtLeastTwoArgsCheck("string<?", argc);
    StringArgCheck("string<?", argv[0]);

    for (int adx = 1; adx < argc; adx++)
    {
        StringArgCheck("string<?", argv[adx]);

        if (StringCompare(AsString(argv[adx - 1]), AsString(argv[adx])) >= 0)
            return(FalseObject);
    }

    return(TrueObject);
}

Define("string>?", StringGreaterThanPPrimitive)(int argc, FObject argv[])
{
    AtLeastTwoArgsCheck("string>?", argc);
    StringArgCheck("string>?", argv[0]);

    for (int adx = 1; adx < argc; adx++)
    {
        StringArgCheck("string>?", argv[adx]);

        if (StringCompare(AsString(argv[adx - 1]), AsString(argv[adx])) <= 0)
            return(FalseObject);
    }

    return(TrueObject);
}

Define("string<=?", StringLessThanEqualPPrimitive)(int argc, FObject argv[])
{
    AtLeastTwoArgsCheck("string<=?", argc);
    StringArgCheck("string<=?", argv[0]);

    for (int adx = 1; adx < argc; adx++)
    {
        StringArgCheck("string<=?", argv[adx]);

        if (StringCompare(AsString(argv[adx - 1]), AsString(argv[adx])) > 0)
            return(FalseObject);
    }

    return(TrueObject);
}

Define("string>=?", StringGreaterThanEqualPPrimitive)(int argc, FObject argv[])
{
    AtLeastTwoArgsCheck("string>=?", argc);
    StringArgCheck("string>=?", argv[0]);

    for (int adx = 1; adx < argc; adx++)
    {
        StringArgCheck("string>=?", argv[adx]);

        if (StringCompare(AsString(argv[adx - 1]), AsString(argv[adx])) < 0)
            return(FalseObject);
    }

    return(TrueObject);
}

Define("string-ci=?", StringCiEqualPPrimitive)(int argc, FObject argv[])
{
    AtLeastTwoArgsCheck("string-ci=?", argc);
    StringArgCheck("string-ci=?", argv[0]);

    for (int adx = 1; adx < argc; adx++)
    {
        StringArgCheck("string-ci=?", argv[adx]);

        if (StringCiCompare(AsString(argv[adx - 1]), AsString(argv[adx])) != 0)
            return(FalseObject);
    }

    return(TrueObject);
}

Define("string-ci<?", StringCiLessThanPPrimitive)(int argc, FObject argv[])
{
    AtLeastTwoArgsCheck("string-ci<?", argc);
    StringArgCheck("string-ci<?", argv[0]);

    for (int adx = 1; adx < argc; adx++)
    {
        StringArgCheck("string-ci<?", argv[adx]);

        if (StringCiCompare(AsString(argv[adx - 1]), AsString(argv[adx])) >= 0)
            return(FalseObject);
    }

    return(TrueObject);
}

Define("string-ci>?", StringCiGreaterThanPPrimitive)(int argc, FObject argv[])
{
    AtLeastTwoArgsCheck("string-ci>?", argc);
    StringArgCheck("string-ci>?", argv[0]);

    for (int adx = 1; adx < argc; adx++)
    {
        StringArgCheck("string-ci>?", argv[adx]);

        if (StringCiCompare(AsString(argv[adx - 1]), AsString(argv[adx])) <= 0)
            return(FalseObject);
    }

    return(TrueObject);
}

Define("string-ci<=?", StringCiLessThanEqualPPrimitive)(int argc, FObject argv[])
{
    AtLeastTwoArgsCheck("string-ci<=?", argc);
    StringArgCheck("string-ci<=?", argv[0]);

    for (int adx = 1; adx < argc; adx++)
    {
        StringArgCheck("string-ci<=?", argv[adx]);

        if (StringCiCompare(AsString(argv[adx - 1]), AsString(argv[adx])) > 0)
            return(FalseObject);
    }

    return(TrueObject);
}

Define("string-ci>=?", StringCiGreaterThanEqualPPrimitive)(int argc, FObject argv[])
{
    AtLeastTwoArgsCheck("string-ci>=?", argc);
    StringArgCheck("string-ci>=?", argv[0]);

    for (int adx = 1; adx < argc; adx++)
    {
        StringArgCheck("string-ci>=?", argv[adx]);

        if (StringCiCompare(AsString(argv[adx - 1]), AsString(argv[adx])) < 0)
            return(FalseObject);
    }

    return(TrueObject);
}

Define("string-upcase", StringUpcasePrimitive)(int argc, FObject argv[])
{
    OneArgCheck("string-upcase", argc);
    StringArgCheck("string-upcase", argv[0]);

    return(UpcaseString(argv[0]));
}

Define("string-downcase", StringDowncasePrimitive)(int argc, FObject argv[])
{
    OneArgCheck("string-downcase", argc);
    StringArgCheck("string-downcase", argv[0]);

    return(DowncaseString(argv[0]));
}

Define("string-foldcase", StringFoldcasePrimitive)(int argc, FObject argv[])
{
    OneArgCheck("string-foldcase", argc);
    StringArgCheck("string-foldcase", argv[0]);

    return(FoldcaseString(argv[0]));
}

Define("string-append", StringAppendPrimitive)(int argc, FObject argv[])
{
    int sl = 0;

    for (int adx = 0; adx < argc; adx++)
    {
        StringArgCheck("string-append", argv[adx]);

        sl += StringLength(argv[adx]);
    }

    FObject s = MakeString(0, sl);

    int sdx = 0;
    for (int adx = 0; adx < argc; adx++)
    {
        sl = StringLength(argv[adx]);
        memcpy(AsString(s)->String + sdx, AsString(argv[adx])->String, sl * sizeof(FCh));
        sdx += sl;
    }

    return(s);
}

Define("string->list", StringToListPrimitive)(int argc, FObject argv[])
{
    FFixnum strt;
    FFixnum end;

    OneToThreeArgsCheck("string->list", argc);
    StringArgCheck("string->list", argv[0]);

    if (argc > 1)
    {
        IndexArgCheck("string->list", argv[1], StringLength(argv[0]));

        strt = AsFixnum(argv[1]);

        if (argc > 2)
        {
            EndIndexArgCheck("string->list", argv[2], strt, StringLength(argv[0]));

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

    FObject lst = EmptyListObject;

    for (FFixnum idx = end; idx > strt; idx--)
        lst = MakePair(MakeCharacter(AsString(argv[0])->String[idx - 1]), lst);

    return(lst);
}

Define("list->string", ListToStringPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("list->string", argc);

    FObject lst = argv[0];
    int sl = ListLength(lst);
    FObject s = MakeString(0, sl);
    int sdx = 0;

    while (PairP(lst))
    {
        CharacterArgCheck("list->string", First(lst));

        AsString(s)->String[sdx] = AsCharacter(First(lst));
        sdx += 1;

        lst = Rest(lst);
    }

    FAssert(lst == EmptyListObject);

    return(s);
}

Define("string-copy", StringCopyPrimitive)(int argc, FObject argv[])
{
    FFixnum strt;
    FFixnum end;

    OneToThreeArgsCheck("string-copy", argc);
    StringArgCheck("string-copy", argv[0]);

    if (argc > 1)
    {
        IndexArgCheck("string-copy", argv[1], StringLength(argv[0]));

        strt = AsFixnum(argv[1]);

        if (argc > 2)
        {
            EndIndexArgCheck("string-copy", argv[2], strt, StringLength(argv[0]));

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

    return(MakeString(AsString(argv[0])->String + strt, end - strt));
}

Define("string-copy!", StringCopyModifyPrimitive)(int argc, FObject argv[])
{
    FFixnum strt;
    FFixnum end;

    ThreeToFiveArgsCheck("string-copy!", argc);
    StringArgCheck("string-copy!", argv[0]);
    IndexArgCheck("string-copy!", argv[1], StringLength(argv[0]));
    StringArgCheck("string-copy!", argv[2]);

    if (argc > 3)
    {
        IndexArgCheck("string-copy!", argv[3], StringLength(argv[2]));

        strt = AsFixnum(argv[3]);

        if (argc > 4)
        {
            EndIndexArgCheck("string-copy!", argv[4], strt, StringLength(argv[2]));

            end = AsFixnum(argv[4]);
        }
        else
            end = (FFixnum) StringLength(argv[2]);
    }
    else
    {
        strt = 0;
        end = (FFixnum) StringLength(argv[2]);
    }

    if ((FFixnum) StringLength(argv[0]) - AsFixnum(argv[1]) < end - strt)
        RaiseExceptionC(R.Assertion, "string-copy!", "expected a valid index", List(argv[1]));

    FAssert(end >= strt);

    memmove(AsString(argv[0])->String + AsFixnum(argv[1]),
            AsString(argv[2])->String + strt, (end - strt) * sizeof(FCh));

    return(NoValueObject);
}

Define("string-fill!", StringFillPrimitive)(int argc, FObject argv[])
{
    FFixnum strt;
    FFixnum end;

    TwoToFourArgsCheck("string-fill!", argc);
    StringArgCheck("string-fill!", argv[0]);
    CharacterArgCheck("string-fill!", argv[1]);

    if (argc > 2)
    {
        IndexArgCheck("string-fill!", argv[2], StringLength(argv[0]));

        strt = AsFixnum(argv[2]);

        if (argc > 3)
        {
            EndIndexArgCheck("string-fill!", argv[3], strt, StringLength(argv[0]));

            end = AsFixnum(argv[3]);
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

    FCh ch = AsCharacter(argv[1]);
    for (FFixnum idx = strt; idx < end; idx++)
        AsString(argv[0])->String[idx] = ch;

    return(NoValueObject);
}

static FPrimitive * Primitives[] =
{
    &StringPPrimitive,
    &MakeStringPrimitive,
    &StringPrimitive,
    &StringLengthPrimitive,
    &StringRefPrimitive,
    &StringSetPrimitive,
    &StringEqualPPrimitive,
    &StringLessThanPPrimitive,
    &StringGreaterThanPPrimitive,
    &StringLessThanEqualPPrimitive,
    &StringGreaterThanEqualPPrimitive,
    &StringCiEqualPPrimitive,
    &StringCiLessThanPPrimitive,
    &StringCiGreaterThanPPrimitive,
    &StringCiLessThanEqualPPrimitive,
    &StringCiGreaterThanEqualPPrimitive,
    &StringUpcasePrimitive,
    &StringDowncasePrimitive,
    &StringFoldcasePrimitive,
    &StringAppendPrimitive,
    &StringToListPrimitive,
    &ListToStringPrimitive,
    &StringCopyPrimitive,
    &StringCopyModifyPrimitive,
    &StringFillPrimitive
};

void SetupStrings()
{
    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
