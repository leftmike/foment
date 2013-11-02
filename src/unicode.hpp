/*

Foment

*/

#ifndef __UNICODE_HPP__
#define __UNICODE_HPP__

extern unsigned char Utf8TrailingBytes[256];

#define ConvertToSystem(obj, ss)\
    SCh __ssbuf[256];\
    FAssert(StringP(obj));\
    ss = ConvertToStringS(AsString(obj)->String, StringLength(obj), __ssbuf,\
            sizeof(__ssbuf) / sizeof(SCh))

SCh * ConvertToStringS(FCh * s, uint_t sl, SCh * b, uint_t bl);

FCh ConvertUtf8ToCh(FByte * b, uint_t bl);
FObject ConvertUtf8ToString(FByte * b, uint_t bl);
FObject ConvertStringToUtf8(FCh * s, uint_t sl, int_t ztf);
FCh ConvertUtf16ToCh(FCh16 * s, uint_t sl);
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
