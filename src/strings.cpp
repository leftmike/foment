/*

Foment

*/

#include <string.h>
#include "foment.hpp"

// ---- Strings ----

FObject MakeString(FCh * s, int sl)
{
    FString * ns = (FString *) MakeObject(StringTag, sizeof(FString) + sl * sizeof(FCh));
    ns->Length = sl;
    ns->String[ns->Length] = 0;

    if (s != 0)
    {
        int idx;
        for (idx = 0; idx < sl; idx++)
            ns->String[idx] = s[idx];
    }

    FObject obj = AsObject(ns);
    FAssert(ObjectLength(obj) == AlignLength(sizeof(FString) + sl * sizeof(FCh)));
    return(obj);
}

FObject MakeStringCh(int sl, FCh ch)
{
    FString * s = AsString(MakeString(0, sl));

    int idx;
    for (idx = 0; idx < sl; idx++)
        s->String[idx] = ch;

    return(AsObject(s));
}

FObject MakeStringF(FString * s)
{
    FString * ns = AsString(MakeString(0, s->Length));

    int idx;
    for (idx = 0; idx < s->Length; idx++)
        ns->String[idx] = s->String[idx];

    return(AsObject(ns));
}

FObject MakeStringC(char * s)
{
    int sl = strlen(s);
    FString * ns = AsString(MakeString(0, sl));

    int idx;
    for (idx = 0; idx < sl; idx++)
        ns->String[idx] = s[idx];

    return(AsObject(ns));
}

void StringToC(FObject s, char * b, int bl)
{
    int idx;

    FAssert(StringP(s));

    for (idx = 0; idx < bl - 1; idx++)
    {
        if (idx == AsString(s)->Length)
            break;

        b[idx] = (char) AsString(s)->String[idx];
    }

    FAssert(idx < bl - 1);
    b[idx] = 0;
}

int StringAsNumber(FCh * s, int sl, FFixnum * np)
{
    FFixnum ns;
    FFixnum n;
    int sdx;

    ns = 1;
    sdx = 0;
    if (s[0] == '-')
    {
        ns = -1;
        if (sl == 1)
            return(0);
        sdx += 1;
    }
    else if (s[0] == '+')
    {
        if (sl == 1)
            return(0);
        sdx += 1;
    }

    for (n = 0; sdx < sl; sdx++)
    {
        if (s[sdx] >= '0' && s[sdx] <= '9')
            n = n * 10 + s[sdx] - '0';
        else
            return(0);
    }

    *np = n * ns;
    return(1);
}

const static char Digits[] = {"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"};

int NumberAsString(FFixnum n, FCh * s, FFixnum b)
{
    int sl = 0;

    if (n < 0)
    {
        s[sl] = '-';
        sl += 1;
        n *= -1;
    }

    if (n >= b)
    {
        sl += NumberAsString(n / b, s + sl, b);
        s[sl] = Digits[n % b];
        sl += 1;
    }
    else
    {
        s[sl] = Digits[n];
        sl += 1;
    }

    return(sl);
}

int ChAlphabeticP(FCh ch)
{
    if (ch >= 'a' && ch <= 'z' || ch >= 'A' && ch <= 'Z')
        return(1);
    return(0);
}

int ChNumericP(FCh ch)
{
    if (ch >= '0' && ch <= '9')
        return(1);
    return(0);
}

int ChWhitespaceP(FCh ch)
{
    // space, tab, line feed, form feed, carriage return
    if (ch == 32 || ch == 9 || ch == 10 || ch == 12 || ch == 13)
        return(1);
    return(0);
}

FCh ChUpCase(FCh ch)
{
    if (ch >= 'a' && ch <= 'z')
        return(ch - 'a' + 'A');
    return(ch);
}

FCh ChDownCase(FCh ch)
{
    if (ch >= 'A' && ch <= 'Z')
        return(ch - 'A' + 'a');
    return(ch);
}

FObject FoldCaseString(FObject s)
{
    FAssert(StringP(s));

    int sl = AsString(s)->Length;
    FString * ns = AsString(MakeString(0, sl));

    for (int idx = 0; idx < sl; idx++)
        ns->String[idx] = ChDownCase(AsString(s)->String[idx]);

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

    return(StringLengthHash(AsString(obj)->String, AsString(obj)->Length));
}

static int ChCompare(FCh ch1, FCh ch2)
{
    return(ch1 - ch2);
}

static int ChCiCompare(FCh ch1, FCh ch2)
{
    return(ChDownCase(ch1) - ChDownCase(ch2));
}

static int StringCompare(FString * str1, FString * str2)
{
    int sdx;

    for (sdx = 0; sdx < str1->Length && sdx < str2->Length; sdx++)
        if (str1->String[sdx] != str2->String[sdx])
            return(str1->String[sdx] - str2->String[sdx]);

    return(str1->Length - str2->Length);
}

static int StringCiCompare(FString * str1, FString * str2)
{
    int sdx;

    for (sdx = 0; sdx < str1->Length && sdx < str2->Length; sdx++)
        if (ChCiCompare(str1->String[sdx], str2->String[sdx]))
            return(ChCiCompare(str1->String[sdx], str2->String[sdx]));

    return(str1->Length - str2->Length);
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

    if (sl !=  AsString(obj)->Length)
        return(0);

    for (sdx = 0; sdx < sl; sdx++)
        if (s[sdx] != AsString(obj)->String[sdx])
            return(0);

    return(1);
}

int StringCEqualP(char * s, FObject obj)
{
    FAssert(StringP(obj));

    int sl = strlen(s);
    if (sl != AsString(obj)->Length)
        return(0);

    for (int sdx = 0; sdx < sl; sdx++)
        if (s[sdx] != AsString(obj)->String[sdx])
            return(0);

    return(1);
}

Define("string-hash", StringHashPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(Assertion, "string-hash", "string-hash: expected one argument",
                EmptyListObject);
    if (StringP(argv[0]) == 0)
        RaiseExceptionC(Assertion, "string-hash", "string-hash: expected a string", List(argv[0]));

    return(MakeFixnum(StringHash(argv[0])));
}

Define("string=?", StringEqualPPrimitive)(int argc, FObject argv[])
{
    if (argc != 2)
        RaiseExceptionC(Assertion, "string=?", "string=?: expected two arguments",
                EmptyListObject);
    if (StringP(argv[0]) == 0)
        RaiseExceptionC(Assertion, "string=?", "string=?: expected a string", List(argv[0]));
    if (StringP(argv[1]) == 0)
        RaiseExceptionC(Assertion, "string=?", "string=?: expected a string", List(argv[1]));

    return(StringEqualP(argv[0], argv[1]) ? TrueObject : FalseObject);
}

static FPrimitive * Primitives[] =
{
    &StringEqualPPrimitive,
    &StringHashPrimitive
};

void SetupStrings()
{
    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(Bedrock, BedrockLibrary, Primitives[idx]);
}
