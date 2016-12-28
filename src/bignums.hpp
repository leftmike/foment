/*

Foment

*/

#ifndef __BIGNUMS_HPP__
#define __BIGNUMS_HPP__

#include <math.h>
#include <float.h>

inline double64_t Truncate(double64_t n)
{
#ifdef FOMENT_WINDOWS
    return(((n) < 0) ? ceil((n)) : floor((n)));
#else // FOMENT_WINDOWS
    return(trunc(n));
#endif // FOMENT_WINDOWS
}

FObject MakeBignum(FFixnum n);
FObject MakeBignum(double64_t d);
FObject MakeBignum(FObject n);
FObject ToBignum(FObject obj); // should be static
FObject Normalize(FObject num); // should be static inline
double64_t BignumToDouble(FObject bn); // check who calls
char * BignumToStringC(FObject bn, FFixnum rdx);
FObject ToExactRatio(double64_t d);
int_t ParseBignum(FCh * s, int_t sl, int_t sdx, FFixnum rdx, FFixnum sgn, FFixnum n,
    FObject * punt);
int_t BignumCompare(FObject bn1, FObject bn2);
int_t BignumCompareFixnum(FObject bn, FFixnum n);
int_t BignumSign(FObject bn);
FObject BignumAdd(FObject bn1, FObject bn2);
FObject BignumAddFixnum(FObject bn, FFixnum n);
FObject BignumMultiply(FObject bn1, FObject bn2);
FObject BignumMultiplyFixnum(FObject bn, FFixnum n);
FObject BignumSubtract(FObject bn1, FObject bn2);
FObject BignumDivide(FObject n, FObject d);
FObject BignumRemainder(FObject n, FObject d);
FFixnum BignumRemainderFixnum(FObject n, FFixnum d);
int_t BignumEqualFixnum(FObject bn, FFixnum n);
FObject BignumExpt(FObject bn, FFixnum e);
FObject BignumSqrt(FObject * rem, FObject bn);
FObject BignumAnd(FObject bn1, FObject bn2);
FObject BignumIOr(FObject bn1, FObject bn2);
FObject BignumXOr(FObject bn1, FObject bn2);
FObject BignumNot(FObject bn);
uint_t BignumBitCount(FObject bn);
uint_t BignumIntegerLength(FObject bn);
uint_t BignumFirstSetBit(FObject bn);
FObject BignumArithmeticShift(FObject bn, FFixnum cnt);

#endif // __BIGNUMS_HPP__
