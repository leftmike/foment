/*

Foment

*/

#ifndef __UNICODE_HPP__
#define __UNICODE_HPP__

#define UNI_SUR_HIGH_START  (UTF32)0xD800
#define UNI_SUR_HIGH_END    (UTF32)0xDBFF
#define UNI_SUR_LOW_START   (UTF32)0xDC00
#define UNI_SUR_LOW_END     (UTF32)0xDFFF

extern unsigned char Utf8TrailingBytes[256];

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

// Generate code in unicode.hpp.

FCh CharFoldcase(FCh ch);
FCh CharUpcase(FCh ch);
FCh CharDowncase(FCh ch);

#endif // __UNICODE_HPP__
