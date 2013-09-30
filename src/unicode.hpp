/*

Foment

*/

#ifndef __UNICODE_HPP__
#define __UNICODE_HPP__

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
