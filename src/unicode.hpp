/*

Foment

*/

#ifndef __UNICODE_HPP__
#define __UNICODE_HPP__

#define MaximumUnicodeCharacter 0x0010FFFF
#define UnicodeReplacementCharacter 0xFFFD

#define Utf16HighSurrogateStart 0xD800
#define Utf16HighSurrogateEnd 0xDBFF
#define Utf16LowSurrogateStart 0xDC00
#define Utf16LowSurrogateEnd 0xDFFF
#define Utf16HalfShift 10
#define Utf16HalfBase 0x0010000
#define Utf16HalfMask 0x3FF

extern unsigned char Utf8TrailingBytes[256];

FCh ConvertUtf8ToCh(FByte * b, uint_t bl);
FObject ConvertUtf8ToString(FByte * b, uint_t bl);
FObject ConvertStringToUtf8(FCh * s, uint_t sl, int_t ztf);
FObject ConvertUtf16ToString(FCh16 * b, uint_t bl);
FObject ConvertStringToUtf16(FCh * s, uint_t sl, int_t ztf);

inline FObject ConvertStringToUtf8(FObject s)
{
    FAssert(StringP(s));

    return(ConvertStringToUtf8(AsString(s)->String, StringLength(s), 1));
}

inline FObject ConvertStringToUtf16(FObject s)
{
    FAssert(StringP(s));

    return(ConvertStringToUtf16(AsString(s)->String, StringLength(s), 1));
}

int WhitespaceP(FCh ch);
int DigitValue(FCh ch);
unsigned int AlphabeticP(FCh ch);
unsigned int UppercaseP(FCh ch);
unsigned int LowercaseP(FCh ch);

unsigned int CharFullfoldLength(FCh ch);
FCh * CharFullfold(FCh ch);

unsigned int CharFullupLength(FCh ch);
FCh * CharFullup(FCh ch);

unsigned int CharFulldownLength(FCh ch);
FCh * CharFulldown(FCh ch);

// Generated code in unicode.hpp.

FCh CharFoldcase(FCh ch);
FCh CharUpcase(FCh ch);
FCh CharDowncase(FCh ch);

#endif // __UNICODE_HPP__
