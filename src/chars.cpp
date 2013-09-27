/*

Foment

- char-upper-case?: [Uppercase] use DerivedCoreProperties.txt
- char-lower-case?: [Lowercase] use DerivedCoreProperties.txt
- char-upcase: use UnicodeData.txt
- char-downcase: use UnicodeData.txt

character range: 0x0000 to 0x10FFFF which is 21 bits

*/

#include "foment.hpp"
#include "unicode.hpp"

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

Define("char-alphabetic?", CharAlphabeticPPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("char-alphabetic?", argc);
    CharacterArgCheck("char-alphabetic?", argv[0]);

    return(AlphabeticP(AsCharacter(argv[0])) ? TrueObject : FalseObject);
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

Define("char-whitespace?", CharWhitespacePPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("char-whitespace?", argc);
    CharacterArgCheck("char-whitespace?", argv[0]);

    return(WhitespaceP(AsCharacter(argv[0])) ? TrueObject : FalseObject);
}

Define("char-upper-case?", CharUpperCasePPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("char-upper-case?", argc);
    CharacterArgCheck("char-upper-case?", argv[0]);

    return(UppercaseP(AsCharacter(argv[0])) ? TrueObject : FalseObject);
}

Define("char-lower-case?", CharLowerCasePPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("char-lower-case?", argc);
    CharacterArgCheck("char-lower-case?", argv[0]);

    return(LowercaseP(AsCharacter(argv[0])) ? TrueObject : FalseObject);
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

Define("char-upcase", CharUpcasePrimitive)(int argc, FObject argv[])
{
    OneArgCheck("char-upcase", argc);
    CharacterArgCheck("char-upcase", argv[0]);

    return(MakeCharacter(CharUpcase(AsCharacter(argv[0]))));
}

Define("char-downcase", CharDowncasePrimitive)(int argc, FObject argv[])
{
    OneArgCheck("char-downcase", argc);
    CharacterArgCheck("char-downcase", argv[0]);

    return(MakeCharacter(CharDowncase(AsCharacter(argv[0]))));
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
    &CharAlphabeticPPrimitive,
    &CharNumericPPrimitive,
    &CharWhitespacePPrimitive,
    &CharUpperCasePPrimitive,
    &CharLowerCasePPrimitive,
    &DigitValuePrimitive,
    &CharToIntegerPrimitive,
    &IntegerToCharPrimitive,
    &CharUpcasePrimitive,
    &CharDowncasePrimitive,
    &CharFoldcasePrimitive
};

void SetupCharacters()
{
    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
