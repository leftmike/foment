/*

Foment

*/

#ifndef __BIGNUMS_HPP__
#define __BIGNUMS_HPP__

#if defined(FOMENT_WINDOWS) && defined(NAN)
#undef NAN
#endif
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

FObject MakeBignumFromFixnum(long_t n);
FObject MakeBignumFromDouble(double64_t d);
FObject CopyBignum(FObject n);
FObject ToBignum(FObject obj); // should be static
FObject Normalize(FObject num); // should be static inline
double64_t BignumToDouble(FObject bn); // check who calls
char * BignumToStringC(FObject bn, long_t rdx);
FObject ToExactRatio(double64_t d);
long_t ParseBignum(FCh * s, long_t sl, long_t sdx, long_t rdx, long_t sgn, long_t n,
    FObject * punt);
long_t BignumCompare(FObject bn1, FObject bn2);
long_t BignumCompareFixnum(FObject bn, long_t n);
long_t BignumSign(FObject bn);
FObject BignumAdd(FObject bn1, FObject bn2);
FObject BignumAddFixnum(FObject bn, long_t n);
FObject BignumMultiply(FObject bn1, FObject bn2);
FObject BignumMultiplyFixnum(FObject bn, long_t n);
FObject BignumSubtract(FObject bn1, FObject bn2);
FObject BignumDivide(FObject n, FObject d);
FObject BignumRemainder(FObject n, FObject d);
long_t BignumRemainderFixnum(FObject n, long_t d);
long_t BignumEqualFixnum(FObject bn, long_t n);
FObject BignumExpt(FObject bn, long_t e);
FObject BignumSqrt(FObject * rem, FObject bn);
FObject BignumAnd(FObject bn1, FObject bn2);
FObject BignumIOr(FObject bn1, FObject bn2);
FObject BignumXOr(FObject bn1, FObject bn2);
FObject BignumNot(FObject bn);
ulong_t BignumBitCount(FObject bn);
ulong_t BignumIntegerLength(FObject bn);
ulong_t BignumFirstSetBit(FObject bn);
FObject BignumArithmeticShift(FObject bn, long_t cnt);

#endif // __BIGNUMS_HPP__
