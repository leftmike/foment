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

// Generate code in unicode.hpp.

FCh CharFoldcase(FCh ch);
FCh CharUpcase(FCh ch);
FCh CharDowncase(FCh ch);

#endif // __UNICODE_HPP__
