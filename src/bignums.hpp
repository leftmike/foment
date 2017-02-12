/*

Foment

*/

#ifndef __BIGNUMS_HPP__
#define __BIGNUMS_HPP__

#if defined(FOMENT_WINDOWS) && defined(NAN)
#undef NAN
#endif
#ifdef FOMENT_WINDOWS
#include <intrin.h>
#endif // FOMENT_WINDOWS
#include <math.h>
#include <float.h>

// ---- Population Count ----

#ifdef FOMENT_UNIX
#ifdef FOMENT_64BIT
#define PopulationCount(x) __builtin_popcountl(x)
#else // FOMENT_64BIT
#define PopulationCount(x) __builtin_popcount(x)
#endif // FOMENT_64BIT
#endif // FOMENT_UNIX

#ifdef FOMENT_WINDOWS
#ifdef FOMENT_64BIT
#define PopulationCount(x) __popcnt64(x)
#endif // FOMENT_64BIT
#ifdef FOMENT_32BIT
#define PopulationCount(x) __popcnt(x)
#endif // FOMENT_32BIT
#endif // FOMENT_WINDOWS

// popcount_3 from http://en.wikipedia.org/wiki/Hamming_weight#Efficient_implementation

#ifndef PopulationCount
const uint64_t m1  = 0x5555555555555555; //binary: 0101...
const uint64_t m2  = 0x3333333333333333; //binary: 00110011..
const uint64_t m4  = 0x0f0f0f0f0f0f0f0f; //binary:  4 zeros,  4 ones ...
const uint64_t h01 = 0x0101010101010101; //the sum of 256 to the power of 0,1,2,3...

inline unsigned int PopulationCount(uint64_t x)
{
    x -= (x >> 1) & m1;             //put count of each 2 bits into those 2 bits
    x = (x & m2) + ((x >> 2) & m2); //put count of each 4 bits into those 4 bits
    x = (x + (x >> 4)) & m4;        //put count of each 8 bits into those 8 bits
    return (x * h01) >> 56;  //returns left 8 bits of x + (x<<8) + (x<<16) + (x<<24) + ...
}
#endif

// ---- Highest Bit ----

inline unsigned int HighestBitUInt32(uint32_t n)
{
    unsigned int b = 0;
    uint32_t t;

    if ((t = n & 0xFFFF0000) != 0)
    {
        b += 16;
        n = t;
    }
    if ((t = n & 0xFF00FF00) != 0)
    {
        b += 8;
        n = t;
    }
    if ((t = n & 0xF0F0F0F0) != 0)
    {
        b += 4;
        n = t;
    }
    if ((t = n & 0xCCCCCCCC) != 0)
    {
        b += 2;
        n = t;
    }
    return((n & 0xAAAAAAAA) ? b + 1 : b);
}

inline unsigned int HighestBitUInt64(uint64_t n)
{
    unsigned int b = 0;
    uint64_t t;

    if ((t =  n & 0xFFFFFFFF00000000) != 0)
    {
        b += 32;
        n = t;
    }
    if ((t = n & 0xFFFF0000FFFF0000) != 0)
    {
        b += 16;
        n = t;
    }
    if ((t = n & 0xFF00FF00FF00FF00) != 0)
    {
        b += 8;
        n = t;
    }
    if ((t = n & 0xF0F0F0F0F0F0F0F0) != 0)
    {
        b += 4;
        n = t;
    }
    if ((t = n & 0xCCCCCCCCCCCCCCCC) != 0)
    {
        b += 2;
        n = t;
    }
    return((n & 0xAAAAAAAAAAAAAAAA) ? b + 1 : b);
}

inline double64_t Truncate(double64_t n)
{
#ifdef FOMENT_WINDOWS
    return(((n) < 0) ? ceil((n)) : floor((n)));
#else // FOMENT_WINDOWS
    return(trunc(n));
#endif // FOMENT_WINDOWS
}

long_t IsFinite(double64_t d);

FObject MakeBignumFromLong(long_t n);
FObject MakeBignumFromDouble(double64_t d);
FObject CopyBignum(FObject n);
FObject ToBignum(FObject obj); // should be static
FObject Normalize(FObject num); // should be static inline
double64_t BignumToDouble(FObject bn); // check who calls
char * BignumToStringC(FObject bn, uint32_t rdx);
FObject ToExactRatio(double64_t d);
long_t ParseBignum(FCh * s, long_t sl, long_t sdx, long_t rdx, int16_t sgn, long_t n,
    FObject * punt);
long_t BignumCompare(FObject bn1, FObject bn2);
long_t BignumSign(FObject bn);
FObject BignumAdd(FObject bn1, FObject bn2);
FObject BignumAddLong(FObject bn, long_t n);
FObject BignumMultiply(FObject bn1, FObject bn2);
FObject BignumMultiplyLong(FObject bn, long_t n);
FObject BignumSubtract(FObject bn1, FObject bn2);
long_t BignumOddP(FObject n);
ulong_t BignumHash(FObject n);
FObject BignumDivide(FObject n, FObject d);
FObject BignumRemainder(FObject n, FObject d);
FObject BignumSqrt(FObject * rem, FObject bn);
FObject BignumAnd(FObject bn1, FObject bn2);
FObject BignumIOr(FObject bn1, FObject bn2);
FObject BignumXOr(FObject bn1, FObject bn2);
FObject BignumNot(FObject bn);
ulong_t BignumBitCount(FObject bn);
ulong_t BignumIntegerLength(FObject bn);
FObject BignumArithmeticShift(FObject bn, long_t cnt);
void SetupBignums();

#endif // __BIGNUMS_HPP__
