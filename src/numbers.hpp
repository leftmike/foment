/*

Foment

*/

#ifndef __NUMBERS_HPP__
#define __NUMBERS_HPP__

#include "mini-gmp.h"

#define AsBignum(obj) ((FBignum *) (obj))->MPInteger

typedef struct
{
    uint_t Reserved;
    mpz_t MPInteger;
} FBignum;

void DeleteBignum(FObject obj);

#endif // __NUMBERS_HPP__
