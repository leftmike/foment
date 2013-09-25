/*

Foment

- char-alphabetic?: [Alphabetic] use DerivedCoreProperties.txt
- char-whitespace?: [White_Space] use PropList.txt
- char-upper-case?: [Uppercase] use DerivedCoreProperties.txt
- char-lower-case?: [Lowercase] use DerivedCoreProperties.txt
- char-upcase: use UnicodeData.txt
- char-downcase: use UnicodeData.txt

character range: 0x0000 to 0x10FFFF which is 21 bits

*/

#include "foment.hpp"
#include "unidata.hpp"

// ---- Unicode ----

static int DigitValue(FCh ch)
{
    // From:
    // DerivedNumericType-6.2.0.txt
    // Date: 2012-08-13, 19:20:20 GMT [MD]

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

FCh CharFoldcase(FCh ch)
{
    // From:
    // CaseFolding-6.2.0.txt
    // Date: 2012-08-14, 17:54:49 GMT [MD]

    if (ch <= 0x0556)
        return(Foldcase0x0000To0x0556[ch]);

    switch (ch >> 8)
    {
    case 0x10:
        if (ch >= 0x10A0 && ch <= 0x10CD)
            return(Foldcase0x10A0To10CD[ch - 0x10A0]);
        break;

    case 0x1E:
    case 0x1F:
        if (ch >= 0x1E00 && ch <= 0x1FFC)
            return(Foldcase0x1E00To0x1FFC[ch - 0x1E00]);
        break;

    case 0x21:
        if (ch >= 0x2126 && ch <= 0x2183)
            return(Foldcase0x2126To0x2183[ch - 0x2126]);
        break;

    case 0x24:
        if (ch >= 0x24B6 && ch <= 0x24CF)
            return(Foldcase0x24B6To0x24CF[ch - 0x24B6]);
        break;

    case 0x2C:
        if (ch >= 0x2C00 && ch <= 0x2CF2)
            return(Foldcase0x2C00To0x2CF2[ch - 0x2C00]);
        break;

    case 0xA6:
    case 0xA7:
        if (ch >= 0xA640 && ch <= 0xA7AA)
            return(Foldcase0xA640To0xA7AA[ch - 0xA640]);
        break;

    case 0xFF:
        if (ch >= 0xFF21 && ch <= 0xFF3A)
            return(Foldcase0xFF21To0xFF3A[ch - 0xFF21]);
        break;

    case 0x104:
        if (ch >= 0x10400 && ch <= 0x10427)
            return(Foldcase0x10400To0x10427[ch - 0x10400]);
        break;
    }

    return(ch);
}

// ---- Characters ----

Define("char?", CharPPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("char?", argc);

    return(CharacterP(argv[0]) ? TrueObject : FalseObject);
}

Define("char=?", CharEqualPPrimitive)(int argc, FObject argv[])
{
    AtLeastTwoArgsCheck("char=?", argc);
    CharacterArgCheck("char=?", argv[0]);

    for (int adx = 1; adx < argc; adx++)
    {
        CharacterArgCheck("char=?", argv[adx]);

        if (AsCharacter(argv[adx - 1]) != AsCharacter(argv[adx]))
            return(FalseObject);
    }

    return(TrueObject);
}

Define("char<?", CharLessThanPPrimitive)(int argc, FObject argv[])
{
    AtLeastTwoArgsCheck("char<?", argc);
    CharacterArgCheck("char<?", argv[0]);

    for (int adx = 1; adx < argc; adx++)
    {
        CharacterArgCheck("char<?", argv[adx]);

        if (AsCharacter(argv[adx - 1]) >= AsCharacter(argv[adx]))
            return(FalseObject);
    }

    return(TrueObject);
}

Define("char>?", CharGreaterThanPPrimitive)(int argc, FObject argv[])
{
    AtLeastTwoArgsCheck("char>?", argc);
    CharacterArgCheck("char>?", argv[0]);

    for (int adx = 1; adx < argc; adx++)
    {
        CharacterArgCheck("char>?", argv[adx]);

        if (AsCharacter(argv[adx - 1]) <= AsCharacter(argv[adx]))
            return(FalseObject);
    }

    return(TrueObject);
}

Define("char<=?", CharLessThanEqualPPrimitive)(int argc, FObject argv[])
{
    AtLeastTwoArgsCheck("char<=?", argc);
    CharacterArgCheck("char<=?", argv[0]);

    for (int adx = 1; adx < argc; adx++)
    {
        CharacterArgCheck("char<=?", argv[adx]);

        if (AsCharacter(argv[adx - 1]) > AsCharacter(argv[adx]))
            return(FalseObject);
    }

    return(TrueObject);
}

Define("char>=?", CharGreaterThanEqualPPrimitive)(int argc, FObject argv[])
{
    AtLeastTwoArgsCheck("char>=?", argc);
    CharacterArgCheck("char>=?", argv[0]);

    for (int adx = 1; adx < argc; adx++)
    {
        CharacterArgCheck("char>=?", argv[adx]);

        if (AsCharacter(argv[adx - 1]) < AsCharacter(argv[adx]))
            return(FalseObject);
    }

    return(TrueObject);
}

Define("char-ci=?", CharCiEqualPPrimitive)(int argc, FObject argv[])
{
    AtLeastTwoArgsCheck("char-ci=?", argc);
    CharacterArgCheck("char-ci=?", argv[0]);

    for (int adx = 1; adx < argc; adx++)
    {
        CharacterArgCheck("char-ci=?", argv[adx]);

        if (CharFoldcase(AsCharacter(argv[adx - 1])) != CharFoldcase(AsCharacter(argv[adx])))
            return(FalseObject);
    }

    return(TrueObject);
}

Define("char-ci<?", CharCiLessThanPPrimitive)(int argc, FObject argv[])
{
    AtLeastTwoArgsCheck("char-ci<?", argc);
    CharacterArgCheck("char-ci<?", argv[0]);

    for (int adx = 1; adx < argc; adx++)
    {
        CharacterArgCheck("char-ci<?", argv[adx]);

        if (CharFoldcase(AsCharacter(argv[adx - 1])) >= CharFoldcase(AsCharacter(argv[adx])))
            return(FalseObject);
    }

    return(TrueObject);
}

Define("char-ci>?", CharCiGreaterThanPPrimitive)(int argc, FObject argv[])
{
    AtLeastTwoArgsCheck("char-ci>?", argc);
    CharacterArgCheck("char-ci>?", argv[0]);

    for (int adx = 1; adx < argc; adx++)
    {
        CharacterArgCheck("char-ci>?", argv[adx]);

        if (CharFoldcase(AsCharacter(argv[adx - 1])) <= CharFoldcase(AsCharacter(argv[adx])))
            return(FalseObject);
    }

    return(TrueObject);
}

Define("char-ci<=?", CharCiLessThanEqualPPrimitive)(int argc, FObject argv[])
{
    AtLeastTwoArgsCheck("char-ci<=?", argc);
    CharacterArgCheck("char-ci<=?", argv[0]);

    for (int adx = 1; adx < argc; adx++)
    {
        CharacterArgCheck("char-ci<=?", argv[adx]);

        if (CharFoldcase(AsCharacter(argv[adx - 1])) > CharFoldcase(AsCharacter(argv[adx])))
            return(FalseObject);
    }

    return(TrueObject);
}

Define("char-ci>=?", CharCiGreaterThanEqualPPrimitive)(int argc, FObject argv[])
{
    AtLeastTwoArgsCheck("char-ci>=?", argc);
    CharacterArgCheck("char-ci>=?", argv[0]);

    for (int adx = 1; adx < argc; adx++)
    {
        CharacterArgCheck("char-ci>=?", argv[adx]);

        if (CharFoldcase(AsCharacter(argv[adx - 1])) < CharFoldcase(AsCharacter(argv[adx])))
            return(FalseObject);
    }

    return(TrueObject);
}

Define("char-numeric?", CharNumericPPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("char-numeric?", argc);
    CharacterArgCheck("char-numeric?", argv[0]);

    int dv = DigitValue(AsCharacter(argv[0]));
    if (dv < 0 || dv > 9)
        return(FalseObject);
    return(TrueObject);
}

Define("digit-value", DigitValuePrimitive)(int argc, FObject argv[])
{
    OneArgCheck("digit-value", argc);
    CharacterArgCheck("digit-value", argv[0]);

    int dv = DigitValue(AsCharacter(argv[0]));
    if (dv < 0 || dv > 9)
        return(FalseObject);
    return(MakeFixnum(dv));
}

Define("char->integer", CharToIntegerPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("char->integer", argc);
    CharacterArgCheck("char->integer", argv[0]);

    return(MakeFixnum(AsCharacter(argv[0])));
}

Define("integer->char", IntegerToCharPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("integer->char", argc);
    FixnumArgCheck("integer->char", argv[0]);

    return(MakeCharacter(AsFixnum(argv[0])));
}

Define("char-foldcase", CharFoldcasePrimitive)(int argc, FObject argv[])
{
    OneArgCheck("char-foldcase", argc);
    CharacterArgCheck("char-foldcase", argv[0]);

    return(MakeCharacter(CharFoldcase(AsCharacter(argv[0]))));
}

static FPrimitive * Primitives[] =
{
    &CharPPrimitive,
    &CharEqualPPrimitive,
    &CharLessThanPPrimitive,
    &CharGreaterThanPPrimitive,
    &CharLessThanEqualPPrimitive,
    &CharGreaterThanEqualPPrimitive,
    &CharCiEqualPPrimitive,
    &CharCiLessThanPPrimitive,
    &CharCiGreaterThanPPrimitive,
    &CharCiLessThanEqualPPrimitive,
    &CharCiGreaterThanEqualPPrimitive,
    
    &CharNumericPPrimitive,
    
    &DigitValuePrimitive,
    &CharToIntegerPrimitive,
    &IntegerToCharPrimitive,
    
    &CharFoldcasePrimitive
};

void SetupCharacters()
{
    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
