/*

Foment

- char-alphabetic?: [Alphabetic] use DerivedCoreProperties.txt
- char-numeric?: [Numeric_Digit] same as digit-value
- char-whitespace?: [White_Space] use PropList.txt
- char-upper-case?: [Uppercase] use DerivedCoreProperties.txt
- char-lower-case?: [Lowercase] use DerivedCoreProperties.txt
- digit-value: use extracted\DerivedNumericType.txt with Nd at bottom of file
- char-upcase: use UnicodeData.txt
- char-downcase: use UnicodeData.txt
- char-foldcase: use CaseFolding.txt

character range: 0x0000 to 0x10FFFF which is 21 bits

*/

#include <string.h>
#include "foment.hpp"

// ---- Unicode ----

int DigitValue(FCh ch)
{
    // Return 0 to 9 for digit values and -1 if not a digit.

    if (ch < 0x1D00)
    {
        switch (ch >> 8)
        {
        case 0x00:
            if (ch >= 0x0030 && ch <= 0x0039)
                return(ch - 0x0030);
            return(-1);

        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
            return(-1);

        case 0x06:
            if (ch >= 0x0660 && ch <= 0x0669)
                return(ch - 0x0660);
            if (ch >= 0x06F0 && ch <= 0x06F9)
                return(ch - 0x06F0);
            return(-1);

        case 0x07:
            if (ch >= 0x07C0 && ch <= 0x07C9)
                return(ch - 0x07C0);
            return(-1);

        case 0x08:
            return(-1);

        case 0x09:
            if (ch >= 0x0966 && ch <= 0x096F)
                return(ch - 0x0966);
            if (ch >= 0x09E6 && ch <= 0x09EF)
                return(ch - 0x09E6);
            return(-1);

        case 0x0A:
            if (ch >= 0x0A66 && ch <= 0x0A6F)
                return(ch - 0x0A66);
            if (ch >= 0x0AE6 && ch <= 0x0AEF)
                return(ch - 0x0AE6);
            return(-1);

        case 0x0B:
            if (ch >= 0x0B66 && ch <= 0x0B6F)
                return(ch - 0x0B66);
            if (ch >= 0x0BE6 && ch <= 0x0BEF)
                return(ch - 0x0BE6);
            return(-1);

        case 0x0C:
            if (ch >= 0x0C66 && ch <= 0x0C6F)
                return(ch - 0x0C66);
            if (ch >= 0x0CE6 && ch <= 0x0CEF)
                return(ch - 0x0CE6);
            return(-1);

        case 0x0D:
            if (ch >= 0x0D66 && ch <= 0x0D6F)
                return(ch - 0x0D66);
            return(-1);

        case 0x0E:
            if (ch >= 0x0E50 && ch <= 0x0E59)
                return(ch - 0x0E50);
            if (ch >= 0x0ED0 && ch <= 0x0ED9)
                return(ch - 0x0ED0);
            return(-1);

        case 0x0F:
            if (ch >= 0x0F20 && ch <= 0x0F29)
                return(ch - 0x0F20);
            return(-1);

        case 0x10:
            if (ch >= 0x1040 && ch <= 0x1049)
                return(ch - 0x1040);
            if (ch >= 0x1090 && ch <= 0x1099)
                return(ch - 0x1090);
            return(-1);

        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x16:
            return(-1);

        case 0x17:
            if (ch >= 0x17E0 && ch <= 0x17E9)
                return(ch - 0x17E0);
            return(-1);

        case 0x18:
            if (ch >= 0x1810 && ch <= 0x1819)
                return(ch - 0x1810);
            return(-1);

        case 0x19:
            if (ch >= 0x1946 && ch <= 0x194F)
                return(ch - 0x1946);
            if (ch >= 0x19D0 && ch <= 0x19D9)
                return(ch - 0x19D0);
            return(-1);

        case 0x1A:
            if (ch >= 0x1A80 && ch <= 0x1A89)
                return(ch - 0x1A80);
            if (ch >= 0x1A90 && ch <= 0x1A99)
                return(ch - 0x1A90);
            return(-1);

        case 0x1B:
            if (ch >= 0x1B50 && ch <= 0x1B59)
                return(ch - 0x1B50);
            if (ch >= 0x1BB0 && ch <= 0x1BB9)
                return(ch - 0x1BB0);
            return(-1);

        case 0x1C:
            if (ch >= 0x1C40 && ch <= 0x1C49)
                return(ch - 0x1C40);
            if (ch >= 0x1C50 && ch <= 0x1C59)
                return(ch - 0x1C50);
            return(-1);
        }
    }

    if (ch < 0xA620)
        return(-1);

    if (ch < 0xAC00)
    {
        switch (ch >> 8)
        {
        case 0xA6:
            if (ch >= 0xA620 && ch <= 0xA629)
                return(ch - 0xA620);
            return(-1);

        case 0xA7:
            return(-1);

        case 0xA8:
            if (ch >= 0xA8D0 && ch <= 0xA8D9)
                return(ch - 0xA8D0);
            return(-1);

        case 0xA9:
            if (ch >= 0xA900 && ch <= 0xA909)
                return(ch - 0xA900);
            if (ch >= 0xA9D0 && ch <= 0xA9D9)
                return(ch - 0xA9D0);
            return(-1);

        case 0xAA:
            if (ch >= 0xAA50 && ch <= 0xAA59)
                return(ch - 0xAA50);
            return(-1);

        case 0xAB:
            if (ch >= 0xABF0 && ch <= 0xABF9)
                return(ch - 0xABF0);
            return(-1);
        }
    }

    if (ch >= 0xFF10 && ch <= 0xFF19)
        return(ch - 0xFF10);

    if (ch >= 0x104A0 && ch <= 0x104A9)
        return(ch - 0x104A0);

    if (ch >= 0x11066 && ch <= 0x1106F)
        return(ch - 0x11066);

    if (ch >= 0x110F0 && ch <= 0x110F9)
        return(ch - 0x110F0);

    if (ch >= 0x11136 && ch <= 0x1113F)
        return(ch - 0x11136);

    if (ch >= 0x111D0 && ch <= 0x111D9)
        return(ch - 0x111D0);

    if (ch >= 0x116C0 && ch <= 0x116C9)
        return(ch - 0x116C0);

    if (ch >= 0x1D7CE && ch <= 0x1D7FF)
        return(ch - 0x1D7CE);

    return(-1);
}

// ---- Characters ----

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

    unsigned int sl = StringLength(s);
    FString * ns = AsString(MakeString(0, sl));

    for (unsigned int idx = 0; idx < sl; idx++)
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

    return(StringLengthHash(AsString(obj)->String, StringLength(obj)));
}

static int ChCompare(FCh ch1, FCh ch2)
{
    return(ch1 - ch2);
}

static int ChCiCompare(FCh ch1, FCh ch2)
{
    return(ChDownCase(ch1) - ChDownCase(ch2));
}

int StringCompare(FString * str1, FString * str2)
{
    for (unsigned int sdx = 0; sdx < StringLength(str1) && sdx < StringLength(str2); sdx++)
        if (str1->String[sdx] != str2->String[sdx])
            return(str1->String[sdx] - str2->String[sdx]);

    return(StringLength(str1) - StringLength(str2));
}

static int StringCiCompare(FString * str1, FString * str2)
{
    for (unsigned int sdx = 0; sdx < StringLength(str1) && sdx < StringLength(str2); sdx++)
        if (ChCiCompare(str1->String[sdx], str2->String[sdx]))
            return(ChCiCompare(str1->String[sdx], str2->String[sdx]));

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

int StringCEqualP(char * s, FObject obj)
{
    FAssert(StringP(obj));

    int sl = strlen(s);
    if (sl != StringLength(obj))
        return(0);

    for (int sdx = 0; sdx < sl; sdx++)
        if (s[sdx] != AsString(obj)->String[sdx])
            return(0);

    return(1);
}

Define("string?", StringPPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "string?", "expected one argument", EmptyListObject);

    return(StringP(argv[0]) ? TrueObject : FalseObject);
}

Define("string-hash", StringHashPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "string-hash", "expected one argument", EmptyListObject);
    if (StringP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "string-hash", "expected a string", List(argv[0]));

    return(MakeFixnum(StringHash(argv[0])));
}

Define("string=?", StringEqualPPrimitive)(int argc, FObject argv[])
{
    if (argc != 2)
        RaiseExceptionC(R.Assertion, "string=?", "expected two arguments", EmptyListObject);
    if (StringP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "string=?", "expected a string", List(argv[0]));
    if (StringP(argv[1]) == 0)
        RaiseExceptionC(R.Assertion, "string=?", "expected a string", List(argv[1]));

    return(StringEqualP(argv[0], argv[1]) ? TrueObject : FalseObject);
}

Define("string->symbol", StringToSymbolPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "string->symbol", "expected one argument", EmptyListObject);
    if (StringP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "string->symbol", "expected a string", List(argv[0]));

    return(StringToSymbol(argv[0]));
}

static FPrimitive * Primitives[] =
{
    &StringPPrimitive,
    &StringHashPrimitive,
    &StringEqualPPrimitive,
    &StringToSymbolPrimitive
};

void SetupStrings()
{
    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
