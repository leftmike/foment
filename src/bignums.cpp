/*

Foment

*/

#include "foment.hpp"
#if defined(FOMENT_BSD) || defined(FOMENT_OSX)
#include <stdlib.h>
#else
#include <malloc.h>
#endif
#include <string.h>
#include <stdio.h>
#include "unicode.hpp"
#include "bignums.hpp"
#include "mini-gmp.h"

#define AsBignum(obj) ((FBignum *) (obj))

#define MAXIMUM_DIGIT_COUNT (((ulong_t) 1 << sizeof(uint16_t) * 8) - 1)

#define UINT32_BITS (sizeof(uint32_t) * 8)
#define UINT64_BITS (sizeof(uint64_t) * 8)

typedef struct
{
    mpz_t MPInteger;
    int16_t Sign;
    uint16_t Used;
    uint32_t Digits[1];
} FBignum;

#ifdef FOMENT_DEBUG
static char * NBignumToStringC(FObject bn, uint32_t rdx);
static inline ulong_t MaximumDigits(FObject bn)
{
    FAssert(BignumP(bn));

    return((ByteLength(bn) - (sizeof(FBignum) - sizeof(uint32_t))) / sizeof(uint32_t));
}
#endif // FOMENT_DEBUG

static void UpdateAddUInt32(FBignum * bn, uint32_t n);
static void UpdateMultiplyUInt32(FBignum * bn, uint32_t n);
#ifdef FOMENT_DEBUG
static uint32_t UpdateDivideUInt32(FBignum * bn, uint32_t d);
#endif // FOMENT_DEBUG
static void UpdateSign(FBignum * bn, int16_t sgn);

static FBignum * MakeBignum(ulong_t dc)
{
//    FAssert(dc > 0);
    FAssert(dc < MAXIMUM_DIGIT_COUNT);

    FBignum * bn = (FBignum *) MakeObject(BignumTag, sizeof(FBignum) + (dc - 1) * sizeof(uint32_t),
            0, "%make-bignum");
    mpz_init(bn->MPInteger);
    InstallGuardian(bn, CleanupTConc);

    bn->Sign = 0;
    bn->Used = 0;
    memset(bn->Digits, 0, dc * sizeof(uint32_t));

    FAssert(MaximumDigits(bn) == dc);

    return(bn);
}

static FBignum * BignumFromUInt64(uint64_t n, ulong_t adc)
{
    FBignum * bn = MakeBignum(2 + adc);
    bn->Digits[0] = n & 0xFFFFFFFF;
    bn->Digits[1] = n >> 32;
    bn->Used = (bn->Digits[1] > 0 ? 2 : 1);
    bn->Sign = 1;

    mpz_set_ui(bn->MPInteger, n >> 32);
    mpz_mul_2exp(bn->MPInteger, bn->MPInteger, 32);
    mpz_add_ui(bn->MPInteger, bn->MPInteger, n & 0xFFFFFFFF);

    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    return(bn);
}

static FBignum * MakeBignumFromLong(long_t n, ulong_t adc)
{
#ifdef FOMENT_32BIT
    FAssert(sizeof(long_t) == sizeof(uint32_t));

    FBignum * bn = MakeBignum(1 + adc);
    mpz_init_set_si(bn->MPInteger, n);
//    InstallGuardian(bn, CleanupTConc);

    if (n >= 0)
        bn->Sign = 1;
    else
    {
        bn->Sign = -1;
        n = -n;
    }

    bn->Digits[0] = n;
    bn->Used = 1;

    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    return(bn);
#endif // FOMENT_32BIT
#ifdef FOMENT_64BIT
    FAssert(sizeof(long_t) == sizeof(uint64_t));

    if (n >= 0)
        return(BignumFromUInt64(n, adc));

    FBignum * bn = BignumFromUInt64(-n, adc);
    mpz_mul_si(bn->MPInteger, bn->MPInteger, -1);
    bn->Sign = -1;

    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    return(bn);
#endif // FOMENT_64BIT
}

FObject MakeBignumFromLong(long_t n)
{
    return(MakeBignumFromLong(n, 0));
}

typedef union {
    double64_t Double;
    struct {
#if FOMENT_LITTLE_ENDIAN
        uint64_t Mantissa:52;
        uint64_t Exponent:11;
        uint64_t Negative:1;
#else // FOMENT_LITTLE_ENDIAN
        uint64_t Negative:1;
        uint64_t Exponent:11;
        uint64_t Mantissa:52;
#endif // FOMENT_LITTLE_ENDIAN
    };
} IEEE754Double;

/*
Decompose double d into mantissa and exponent where
-1022 <= exponent <= 1023
0 <= abs(mantissa) < 2^53
d = mantissa * 2 ^ (exponent - 53)
*/
static inline void DecodeDouble(double64_t d, uint64_t * mantissa, int16_t * exponent,
    int8_t * sign)
{
    IEEE754Double ieee;
    ieee.Double = d;

    FAssert(ieee.Exponent != 0x7FF);

    *mantissa = ieee.Mantissa;
    if (ieee.Exponent > 0)
        *mantissa += (1ULL << 52);
    *exponent = (int16_t) (ieee.Exponent ? ieee.Exponent - 0x3FF - 52 : -0x3FE - 52);
    *sign = ieee.Negative ? -1 : 1;
}

FObject MakeBignumFromDouble(double64_t d)
{
    uint64_t mantissa;
    int16_t exponent;
    int8_t sign;
    DecodeDouble(d, &mantissa, &exponent, &sign);

    FBignum * bn;

    if (exponent <= 0 || mantissa == 0)
    {
        if (exponent < 0)
            exponent = - exponent;
        if (exponent >= (int16_t) sizeof(uint64_t) * 8)
            mantissa = 0;
        else
            mantissa = mantissa >> exponent;
        bn = BignumFromUInt64(mantissa, 0);
    }
    else
    {
        bn = MakeBignum(3 + exponent / UINT32_BITS);
        bn->Used = exponent / UINT32_BITS;
        if (exponent % UINT32_BITS == 0)
        {
            bn->Digits[bn->Used] = mantissa & 0xFFFFFFFF;
            bn->Used += 1;
            bn->Digits[bn->Used] = mantissa >> UINT32_BITS;
            bn->Used += 1;
        }
        else
        {
            int16_t mod = exponent % UINT32_BITS;
            bn->Digits[bn->Used] = (mantissa << mod) & 0xFFFFFFFF;
            bn->Used += 1;
            bn->Digits[bn->Used] = (mantissa >> (UINT32_BITS - mod)) & 0xFFFFFFFF;
            bn->Used += 1;
            bn->Digits[bn->Used] = (uint32_t) (mantissa >> (UINT64_BITS - mod));
            bn->Used += 1;
        }
    }

    while (bn->Used > 1)
    {
        if (bn->Digits[bn->Used - 1] != 0)
            break;
        bn->Used -= 1;
    }

    FAssert(bn->Used > 0);

    if (bn->Used == 1 && bn->Digits[0] == 0)
        bn->Sign = 1;
    else
        bn->Sign = sign;



    mpz_init_set_d(bn->MPInteger, d);
//    InstallGuardian(bn, CleanupTConc);

    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    return(bn);
}

static FBignum * CopyBignum(FBignum * bn)
{
    FBignum * ret = MakeBignum(bn->Used);
    mpz_init_set(ret->MPInteger, bn->MPInteger);
//    InstallGuardian(ret, CleanupTConc);

    ret->Sign = bn->Sign;
    memcpy(ret->Digits, bn->Digits, bn->Used * sizeof(uint32_t));
    ret->Used = bn->Used;

    return(ret);
}

FObject CopyBignum(FObject n)
{
    FAssert(BignumP(n));

    return(CopyBignum(AsBignum(n)));
}

void DeleteBignum(FObject obj)
{
    FAssert(BignumP(obj));

    mpz_clear(AsBignum(obj)->MPInteger);
}

FObject ToBignum(FObject obj)
{
    if (FixnumP(obj))
        return(MakeBignumFromLong(AsFixnum(obj)));
    else if (FlonumP(obj))
        return(MakeBignumFromDouble(AsFlonum(obj)));

    FAssert(BignumP(obj));

    return(obj);
}

FObject Normalize(FObject num)
{
    // XXX
    if (BignumP(num) && mpz_cmp_si(AsBignum(num)->MPInteger, (long) MINIMUM_FIXNUM) >= 0
            && mpz_cmp_si(AsBignum(num)->MPInteger, (long) MAXIMUM_FIXNUM) <= 0)
        return(MakeFixnum(mpz_get_si(AsBignum(num)->MPInteger)));

    return(num);
}

FObject MakeIntegerFromInt64(int64_t n)
{
    if (n >= MINIMUM_FIXNUM && n <= MAXIMUM_FIXNUM)
        return(MakeFixnum(n));

    FBignum * bn = BignumFromUInt64(n < 0 ? -n : n, 0);
    if (n < 0)
    {
        mpz_mul_si(bn->MPInteger, bn->MPInteger, -1);
        bn->Sign = -1;
    }

    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    return(bn);
}

FObject MakeIntegerFromUInt64(uint64_t n)
{
    if (n <= MAXIMUM_FIXNUM)
        return(MakeFixnum(n));

    return(BignumFromUInt64(n, 0));
}

#if 0
static inline long_t HighestBit(ulong_t word)
{
    long_t mbit = 0;

    while (word >>= 1)
        mbit += 1;

    return(mbit);
}

static inline int BignumBitSet(FBignum * bn, ulong_t idx)
{
    FAssert(idx < bn->Used * UINT32_BITS);

    return(bn->Digits[idx / UINT32_BITS] & (1 << (idx % UINT32_BITS)));
}

static void BignumCopyBits(FBignum * bn, uint64_t * bits, ulong_t strt, ulong_t cnt)
{
    FAssert(cnt <= sizeof(uint64_t) * 8);
    FAssert(strt + cnt <= bn->Used * UINT32_BITS);

    *bits = 0;
    ulong_t bdx = 0;
    while (cnt > 0)
    {
        if (BignumBitSet(bn, strt + bdx))
            *bits |= (1 << bdx);

        bdx += 1;
        cnt -= 1;
    }
}
#endif // 0

double64_t BignumToDouble(FObject bn)
{
    FAssert(BignumP(bn));

    double64_t od = mpz_get_d(AsBignum(bn)->MPInteger);
    double64_t d = 0;

    if (AsBignum(bn)->Used > 0)
    {
        uint16_t idx = AsBignum(bn)->Used;
        while (idx > 0)
        {
            idx -= 1;

            d = d * (double) (((int64_t) 1) << UINT32_BITS) + AsBignum(bn)->Digits[idx];
        }

        d *= AsBignum(bn)->Sign;

//        FAssert(od == d);
    }

    return(od);
}

char * BignumToStringC(FObject bn, long_t rdx)
{
    return(mpz_get_str(0, (int) rdx, AsBignum(bn)->MPInteger));
}

#ifdef FOMENT_DEBUG
static const char DigitTable[] = "0123456789abcdefghijklmnopqrstuvwxyz";

static char * NBignumToStringC(FObject bn, uint32_t rdx)
{
    if (AsBignum(bn)->Used == 0)
#ifdef FOMENT_WINDOWS
        return(_strdup("0"));
#else // FOMENT_WINDOWS
        return(strdup("0"));
#endif // FOMENT_WINDOWS

    FObject obj = CopyBignum(bn);

    FAssert(BignumP(obj));

    FBignum * tbn = AsBignum(obj);
    char * ret = (char *) malloc(tbn->Used * sizeof(ulong_t) * 8 + 2);
    if (ret == 0)
        return(0);
    char * s = ret;

    while (tbn->Used > 0)
    {
        uint32_t dgt = UpdateDivideUInt32(tbn, rdx);

        FAssert(dgt >= 0 && dgt < (ulong_t) rdx);

        *s = DigitTable[dgt];
        s += 1;

        while (tbn->Used > 0 && tbn->Digits[tbn->Used - 1] == 0)
            tbn->Used -= 1;
    }

    if (tbn->Sign < 0)
    {
        *s = '-';
        s += 1;
    }
    *s = 0;

    char * p = ret;
    while (s > p)
    {
        s -= 1;
        char t = *s;
        *s = *p;
        *p = t;
        p += 1;
    }

    return(ret);
}
#endif // FOMENT_DEBUG

static inline int32_t TensDigit(double64_t n)
{
    return((int32_t) (n - (Truncate(n / 10.0) * 10.0)));
}

FObject ToExactRatio(double64_t d)
{
    FObject whl = MakeBignumFromDouble(Truncate(d));
    FBignum * rbn = MakeBignumFromLong(0, 1 + 15 * 4 / UINT32_BITS);
    FObject scl = MakeFixnum(1);
    int16_t sgn = (d < 0 ? -1 : 1);
    d = fabs(d - Truncate(d));

    for (long_t idx = 0; d != Truncate(d) && idx < 14; idx++)
    {
        UpdateMultiplyUInt32(rbn, 10);
        d *= 10;
        UpdateAddUInt32(rbn, TensDigit(d));
        d = d - Truncate(d);
        scl = GenericMultiply(scl, MakeFixnum(10));
    }

    UpdateSign(rbn, sgn);

    return(GenericAdd(MakeRatio(rbn, scl), whl));
}

long_t ParseBignum(FCh * s, long_t sl, long_t sdx, long_t rdx, int16_t sgn, long_t n,
    FObject * punt)
{
    FAssert(n > 0);

    ulong_t adc;
    if (rdx == 2)
        adc = ((sl - sdx) / UINT32_BITS) + 1;
    else if (rdx == 8)
        adc = (((sl - sdx) * 3) / UINT32_BITS) + 1;
    else
    {
        FAssert(rdx == 10 || rdx == 16);

        adc = (((sl - sdx) * 4) / UINT32_BITS) + 1;
    }

    FBignum * bn = MakeBignumFromLong(n, adc);

    if (rdx == 16)
    {
        for (n = 0; sdx < sl; sdx++)
        {
            uint32_t dv = DigitValue(s[sdx]);

            if (dv < 0 || dv > 9)
            {
                if (s[sdx] >= 'a' && s[sdx] <= 'f')
                    dv = s[sdx] - 'a' + 10;
                else if (s[sdx] >= 'A' && s[sdx] <= 'F')
                    dv = s[sdx] - 'A' + 10;
                else
                    break;
            }

            UpdateMultiplyUInt32(bn, (uint32_t) rdx);
            UpdateAddUInt32(bn, dv);
        }
    }
    else
    {
        FAssert(rdx == 2 || rdx == 8 || rdx == 10);

        for (n = 0; sdx < sl; sdx++)
        {
            int32_t dv = DigitValue(s[sdx]);
            if (dv >= 0 && dv < rdx)
            {
                UpdateMultiplyUInt32(bn, (uint32_t) rdx);
                UpdateAddUInt32(bn, dv);
            }
            else
                break;
        }
    }

    UpdateSign(bn, sgn);

    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);

    *punt = bn;
    return(sdx);
}

static long_t BignumCompareDigits(uint32_t * digits1, uint16_t used1, int16_t sgn1,
    uint32_t * digits2, uint16_t used2, int16_t sgn2)
{
    if ((sgn1 - sgn2) < 0)
        return(-1);
    else if ((sgn1 - sgn2) > 0)
        return(1);

    if (used1 > used2)
        return(sgn1);
    if (used1 < used2)
        return(sgn1 * -1);

    while (used1 > 0)
    {
        used1 -= 1;
        ulong_t d1 = digits1[used1];
        ulong_t d2 = digits2[used1];
        if (d1 < d2)
            return(sgn1 * -1);
        else if (d1 > d2)
            return(sgn1);
    }

    return(0);
}

#ifdef FOMENT_DEBUG
static long_t NBignumCompare(FObject bn1, FObject bn2)
{
    FAssert(BignumP(bn1));
    FAssert(BignumP(bn2));

    return(BignumCompareDigits(AsBignum(bn1)->Digits, AsBignum(bn1)->Used, AsBignum(bn1)->Sign,
            AsBignum(bn2)->Digits, AsBignum(bn2)->Used, AsBignum(bn2)->Sign));
}

static long_t NBignumCompareZero(FObject bn)
{
    if (AsBignum(bn)->Sign > 0)
        return(AsBignum(bn)->Used == 1 && AsBignum(bn)->Digits[0] == 0 ? 0 : 1);

    FAssert(AsBignum(bn)->Sign < 0);

    return(-1);
}
#endif // FOMENT_DEBUG

long_t BignumCompare(FObject bn1, FObject bn2)
{
    FAssert(BignumP(bn1));
    FAssert(BignumP(bn2));

    long_t ret = mpz_cmp(AsBignum(bn1)->MPInteger, AsBignum(bn2)->MPInteger);
//    FAssert(NBignumCompare(bn1, bn2) == ret);
    return(ret);
}

long_t BignumCompareZero(FObject bn)
{
    FAssert(BignumP(bn));

    long_t ret = mpz_cmp_si(AsBignum(bn)->MPInteger, 0);

//    FAssert(NBignumCompareZero(bn) == ret);
    return(ret);
}

long_t BignumSign(FObject bn)
{
    FAssert(BignumP(bn));

    long_t sgn = mpz_sgn(AsBignum(bn)->MPInteger);
//    FAssert(AsBignum(bn)->Sign == sgn);
    return(sgn);
}

static void UpdateSign(FBignum * bn, int16_t sgn)
{
    FAssert(sgn == 1 || sgn == -1);

    mpz_mul_si(bn->MPInteger, bn->MPInteger, sgn);
    bn->Sign = sgn;
}

static void BignumAddDigits(FBignum * ret, uint32_t * digits1, uint16_t used1, uint32_t * digits2,
    uint16_t used2)
{
    FAssert(used2 <= used1);

    uint16_t idx = 0;
    uint32_t n = 0;
    while (idx < used2)
    {
        uint64_t v = (uint64_t) digits1[idx] + digits2[idx] + n;
        ret->Digits[idx] = v & 0xFFFFFFFF;
        n = v >> UINT32_BITS;
        idx += 1;
    }
    while (idx < used1)
    {
        uint64_t v = (uint64_t) digits1[idx] + n;
        ret->Digits[idx] = v & 0xFFFFFFFF;
        n = v >> UINT32_BITS;
        idx += 1;
    }
    ret->Used = idx;

    if (n > 0)
    {
        FAssert(MaximumDigits(ret) > ret->Used);

        ret->Digits[ret->Used] = n;
        ret->Used += 1;
    }
}

static void BignumSubtractDigits(FBignum * ret, uint32_t * digits1, uint16_t used1,
    uint32_t * digits2, uint16_t used2)
{
    FAssert(used2 <= used1);

    ret->Used = 0;

    uint16_t idx = 0;
    uint32_t b = 0;
    while (idx < used1)
    {
        uint32_t d = idx < used2 ? digits2[idx] : 0;
        if (digits1[idx] >= d + b)
        {
            ret->Digits[idx] = digits1[idx] - (d + b);
            b = 0;
        }
        else
        {
            uint64_t v =  0x100000000UL + digits1[idx] - (d + b);
            ret->Digits[idx] = v & 0xFFFFFFFF;
            b = 1;
        }

        if (ret->Digits[idx] != 0)
            ret->Used = idx + 1;
        idx += 1;
    }

//    FAssert(b == 0);
}

#define SwapDigits(digits1, used1, sgn1, digits2, used2, sgn2) \
{ \
    uint32_t * tdigits = digits1; \
    uint16_t tused = used1; \
    int16_t tsgn = sgn1; \
    digits1 = digits2; \
    used1 = used2; \
    sgn1 = sgn2; \
    digits2 = tdigits; \
    used2 = tused; \
    sgn2 = tsgn; \
}

static FObject BignumAddDigits(uint32_t * digits1, uint16_t used1, int16_t sgn1,
    uint32_t * digits2, uint16_t used2, int16_t sgn2)
{
    if (sgn1 == sgn2)
    {
        if (used1 < used2)
            SwapDigits(digits1, used1, sgn1, digits2, used2, sgn2);

        FBignum * ret = MakeBignum(used1 + 1);
        ret->Sign = sgn1;
        BignumAddDigits(ret, digits1, used1, digits2, used2);
        return(ret);
    }

    if (used1 < used2
            || BignumCompareDigits(digits1, used1, 1, digits2, used2, 1) < 0)
        SwapDigits(digits1, used1, sgn1, digits2, used2, sgn2);

    FBignum * ret = MakeBignum(used1);
    ret->Sign = sgn1;
    BignumSubtractDigits(ret, digits1, used1, digits2, used2);
    return(ret);
}

FObject BignumAdd(FObject bn1, FObject bn2)
{
    FAssert(BignumP(bn1));
    FAssert(BignumP(bn2));

    FObject ret = BignumAddDigits(AsBignum(bn1)->Digits, AsBignum(bn1)->Used, AsBignum(bn1)->Sign,
            AsBignum(bn2)->Digits, AsBignum(bn2)->Used, AsBignum(bn2)->Sign);
    mpz_add(AsBignum(ret)->MPInteger, AsBignum(bn1)->MPInteger, AsBignum(bn2)->MPInteger);

//    FAssert(strcmp(BignumToStringC(ret, 10), NBignumToStringC(ret, 10)) == 0);

    return(ret);
}

static void UpdateAddUInt32(FBignum * bn, uint32_t n)
{
    mpz_add_ui(bn->MPInteger, bn->MPInteger, n);

    uint16_t idx = 0;
    while (idx < bn->Used && n > 0)
    {
        uint64_t v = bn->Digits[idx] + (uint64_t) n;
        bn->Digits[idx] = v & 0xFFFFFFFF;
        n = v >> UINT32_BITS;
        idx += 1;
    }

    if (n > 0)
    {
        FAssert(MaximumDigits(bn) > bn->Used);

        bn->Digits[bn->Used] = n;
        bn->Used += 1;
    }

    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
}

FObject BignumAddLong(FObject bn, long_t n)
{
    FAssert(BignumP(bn));

    int16_t nsgn = 1;
    if (n < 0)
    {
        nsgn = -1;
        n = -n;
    }

#ifdef FOMENT_32BIT
    uint32_t digits[1];
    digits[0] = n;
#endif // FOMENT_32BIT
#ifdef FOMENT_64BIT
    uint32_t digits[2];
    digits[0] = n & 0xFFFFFFFF;
    digits[1] = n >> 32;
#endif // FOMENT_64BIT

    FObject ret = BignumAddDigits(AsBignum(bn)->Digits, AsBignum(bn)->Used, AsBignum(bn)->Sign,
            digits, sizeof(digits) / sizeof(uint32_t), nsgn);
#if defined(FOMENT_WINDOWS) && defined(FOMENT_64BIT)
    FObject bn2 = MakeIntegerFromInt64(n * nsgn);
    if (BignumP(bn2))
        mpz_add(AsBignum(ret)->MPInteger, AsBignum(bn)->MPInteger, AsBignum(bn2)->MPInteger);
    else
#endif
    {
    if (nsgn > 0)
        mpz_add_ui(AsBignum(ret)->MPInteger, AsBignum(bn)->MPInteger, (long) n);
    else
        mpz_sub_ui(AsBignum(ret)->MPInteger, AsBignum(bn)->MPInteger, (long) n);
    }

//    FAssert(strcmp(BignumToStringC(ret, 10), NBignumToStringC(ret, 10)) == 0);
    return(ret);
}

FObject BignumSubtract(FObject bn1, FObject bn2)
{
    FAssert(BignumP(bn1));
    FAssert(BignumP(bn2));

    FObject ret = BignumAddDigits(AsBignum(bn1)->Digits, AsBignum(bn1)->Used,
            AsBignum(bn1)->Sign, AsBignum(bn2)->Digits, AsBignum(bn2)->Used,
            AsBignum(bn2)->Sign * -1);
    mpz_sub(AsBignum(ret)->MPInteger, AsBignum(bn1)->MPInteger, AsBignum(bn2)->MPInteger);

//    FAssert(strcmp(BignumToStringC(ret, 10), NBignumToStringC(ret, 10)) == 0);
    return(ret);
}

static FObject BignumMultiplyDigits(uint32_t * digits1, uint16_t used1, uint32_t * digits2,
    int16_t used2)
{
    FBignum * ret = MakeBignum(used1 + used2);
    for (uint16_t idx1 = 0; idx1 < used1; idx1++)
    {
        uint32_t n = 0;
        for (uint16_t idx2 = 0; idx2 < used2; idx2++)
        {
            uint64_t v = (uint64_t) digits1[idx1] * digits2[idx2] + ret->Digits[idx1 + idx2] + n;
            ret->Digits[idx1 + idx2] = v & 0xFFFFFFFF;
            n = v >> UINT32_BITS;
        }

        if (n > 0)
        {
            FAssert(ret->Digits[idx1 + used2] == 0);

            ret->Digits[idx1 + used2] = n;
        }
    }

    ret->Used = used1 + used2;
    while (ret->Used > 1)
    {
        if (ret->Digits[ret->Used - 1] != 0)
            break;
        ret->Used -= 1;
    }

    return(ret);
}

FObject BignumMultiply(FObject bn1, FObject bn2)
{
    FAssert(BignumP(bn1));
    FAssert(BignumP(bn2));

    FObject ret = BignumMultiplyDigits(AsBignum(bn1)->Digits, AsBignum(bn1)->Used,
            AsBignum(bn2)->Digits, AsBignum(bn2)->Used);
    AsBignum(ret)->Sign = AsBignum(bn1)->Sign * AsBignum(bn2)->Sign;
    mpz_mul(AsBignum(ret)->MPInteger, AsBignum(bn1)->MPInteger, AsBignum(bn2)->MPInteger);

//    FAssert(strcmp(BignumToStringC(ret, 10), NBignumToStringC(ret, 10)) == 0);
    return(ret);
}

static void UpdateMultiplyUInt32(FBignum * bn, uint32_t n)
{
    mpz_mul_si(bn->MPInteger, bn->MPInteger, n);

    uint16_t idx = 0;
    uint32_t carry = 0;
    while (idx < bn->Used)
    {
        uint64_t v = bn->Digits[idx] * (uint64_t) n + carry;
        bn->Digits[idx] = v & 0xFFFFFFFF;
        carry = v >> UINT32_BITS;
        idx += 1;
    }

    if (carry > 0)
    {
        FAssert(MaximumDigits(bn) > bn->Used);

        bn->Digits[bn->Used] = carry;
        bn->Used += 1;
    }

    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
}

FObject BignumMultiplyLong(FObject bn, long_t n)
{
    FAssert(BignumP(bn));

    int16_t sgn = AsBignum(bn)->Sign;
    long_t an = n;
    if (n < 0)
    {
        sgn *= -1;
        an = -n;
    }

#ifdef FOMENT_32BIT
    uint32_t digits[1];
    digits[0] = an;
#endif // FOMENT_32BIT
#ifdef FOMENT_64BIT
    uint32_t digits[2];
    digits[0] = an & 0xFFFFFFFF;
    digits[1] = an >> 32;
#endif // FOMENT_64BIT

    FObject ret = BignumMultiplyDigits(AsBignum(bn)->Digits, AsBignum(bn)->Used, digits,
            sizeof(digits) / sizeof(uint32_t));
    AsBignum(ret)->Sign = sgn;
#if defined(FOMENT_WINDOWS) && defined(FOMENT_64BIT)
    FObject bn2 = MakeIntegerFromInt64(n);
    if (BignumP(bn2))
        mpz_mul(AsBignum(ret)->MPInteger, AsBignum(bn)->MPInteger, AsBignum(bn2)->MPInteger);
    else
#endif
    {
    mpz_mul_si(AsBignum(ret)->MPInteger, AsBignum(bn)->MPInteger, (long) n);
    }

//    FAssert(strcmp(BignumToStringC(ret, 10), NBignumToStringC(ret, 10)) == 0);
    return(ret);
}

FObject BignumDivide(FObject n, FObject d)
{
    FAssert(BignumP(n));
    FAssert(BignumP(d));

    FObject ret = MakeBignum(0);
    // XXX
    mpz_tdiv_q(AsBignum(ret)->MPInteger, AsBignum(n)->MPInteger, AsBignum(d)->MPInteger);
    return(ret);
}

#ifdef FOMENT_DEBUG
/*
Destructively divide bn by d; the quotient is left in bn and the remainder is returned.
The quotient is not normalized.
*/
static uint32_t UpdateDivideUInt32(FBignum * bn, uint32_t d)
{
    uint32_t q;
    uint32_t r = 0;
    uint64_t n = 0;

    uint16_t idx = bn->Used;
    while (idx > 0)
    {
        idx -= 1;

        n = (n << UINT32_BITS) + bn->Digits[idx];
        q = (uint32_t) (n / d);
        r = (uint32_t) (n - ((uint64_t) q) * d);
        bn->Digits[idx] = q;
        n = r;
    }

    return(r);
}
#endif // FOMENT_DEBUG

FObject BignumRemainder(FObject n, FObject d)
{
    FAssert(BignumP(n));
    FAssert(BignumP(d));

    FObject ret = MakeBignum(0);
    // XXX
    mpz_tdiv_r(AsBignum(ret)->MPInteger, AsBignum(n)->MPInteger, AsBignum(d)->MPInteger);
    return(ret);
}

long_t BignumRemainderLong(FObject n, long_t d)
{
    FAssert(BignumP(n));
    FAssert(d >= 0);

    // XXX
    return(mpz_tdiv_ui(AsBignum(n)->MPInteger, (unsigned long) d));
}

FObject BignumExpt(FObject bn, long_t e)
{
    FAssert(BignumP(bn));
    FAssert(e >= 0);

    FObject ret = MakeBignum(0);
    // XXX
    mpz_pow_ui(AsBignum(ret)->MPInteger, AsBignum(bn)->MPInteger, (unsigned long) e);
    return(ret);
}

FObject BignumSqrt(FObject * rem, FObject bn)
{
    FAssert(BignumP(bn));

    FObject ret = MakeBignum(0);
    *rem = MakeBignum(0);
    // XXX
    mpz_sqrtrem(AsBignum(ret)->MPInteger, AsBignum(*rem)->MPInteger, AsBignum(bn)->MPInteger);
    return(ret);
}

static FBignum * BignumComplement(FBignum * bn)
{
    uint32_t n = 1;
    for (uint16_t idx = 0; idx < bn->Used; idx++)
    {
        uint64_t v = (uint32_t) ~bn->Digits[idx] + (uint64_t) n;
        bn->Digits[idx] = v & 0xFFFFFFFF;
        n = v >> UINT32_BITS;

        FAssert(n == 0 || n == 1);
    }

    return(bn);
}

static FBignum * BignumAnd(FBignum * bn1, FBignum * bn2)
{
    uint16_t dc = bn1->Used > bn2->Used ? bn2->Used : bn1->Used;
    if (bn1->Sign < 0)
    {
        bn1 = BignumComplement(CopyBignum(bn1));
        dc = bn2->Used;
    }
    if (bn2->Sign < 0)
    {
        bn2 = BignumComplement(CopyBignum(bn2));
        if (bn1->Sign < 0 && bn2->Used > bn1->Used)
            dc = bn2->Used;
        else
            dc = bn1->Used;
    }

    FBignum * ret = MakeBignum(dc);
    uint16_t idx = 0;
    while (idx < bn1->Used && idx < bn2->Used)
    {
        ret->Digits[idx] = bn1->Digits[idx] & bn2->Digits[idx];
        idx += 1;
    }

    if (bn2->Sign < 0 && idx < bn1->Used)
        while (idx < bn1->Used)
        {
            ret->Digits[idx] = bn1->Digits[idx];
            idx += 1;
        }
    else if (bn1->Sign < 0 && idx < bn2->Used)
        while (idx < bn2->Used)
        {
            ret->Digits[idx] = bn2->Digits[idx];
            idx += 1;
        }
    ret->Used = idx;
    if (bn1->Sign < 0 && bn2->Sign < 0)
    {
        ret->Sign = -1;
        BignumComplement(ret);
    }
    else
        ret->Sign = 1;

    return(ret);
}

FObject BignumAnd(FObject bn1, FObject bn2)
{
    FAssert(BignumP(bn1));
    FAssert(BignumP(bn2));

    FBignum * ret = BignumAnd(AsBignum(bn1), AsBignum(bn2));
    mpz_and(AsBignum(ret)->MPInteger, AsBignum(bn1)->MPInteger, AsBignum(bn2)->MPInteger);

    FAssert(strcmp(BignumToStringC(ret, 10), NBignumToStringC(ret, 10)) == 0);

    return(ret);
}

FObject BignumIOr(FObject bn1, FObject bn2)
{
    FAssert(BignumP(bn1));
    FAssert(BignumP(bn2));

    FObject ret = MakeBignum(0);
    // XXX
    mpz_ior(AsBignum(ret)->MPInteger, AsBignum(bn1)->MPInteger, AsBignum(bn2)->MPInteger);
    return(ret);
}

FObject BignumXOr(FObject bn1, FObject bn2)
{
    FAssert(BignumP(bn1));
    FAssert(BignumP(bn2));

    FObject ret = MakeBignum(0);
    // XXX
    mpz_xor(AsBignum(ret)->MPInteger, AsBignum(bn1)->MPInteger, AsBignum(bn2)->MPInteger);
    return(ret);
}

FObject BignumNot(FObject bn)
{
    FAssert(BignumP(bn));

    FObject ret = MakeBignum(0);
    // XXX
    mpz_com(AsBignum(ret)->MPInteger, AsBignum(bn)->MPInteger);
    return(ret);
}

ulong_t BignumBitCount(FObject bn)
{
    FAssert(BignumP(bn));

    // XXX
    return(mpz_popcount(AsBignum(bn)->MPInteger));
}

ulong_t BignumIntegerLength(FObject bn)
{
    FAssert(BignumP(bn));

    // XXX
    return(mpz_sizeinbase(AsBignum(bn)->MPInteger, 2));
}

ulong_t BignumFirstSetBit(FObject bn)
{
    FAssert(BignumP(bn));

    // XXX
    return(mpz_scan1(AsBignum(bn)->MPInteger, 0));
}

FObject BignumArithmeticShift(FObject bn, long_t cnt)
{
    FAssert(BignumP(bn));

    if (cnt == 0)
        return(bn);

    // XXX
    if (cnt < 0)
    {
        FObject rbn = MakeBignum(0);
        mpz_fdiv_q_2exp(AsBignum(rbn)->MPInteger, AsBignum(bn)->MPInteger, (mp_bitcnt_t) - cnt);
        return(Normalize(rbn));
    }

    FObject rbn = MakeBignum(0);
    mpz_mul_2exp(AsBignum(rbn)->MPInteger, AsBignum(bn)->MPInteger, (mp_bitcnt_t) cnt);
    return(rbn);
}

#ifdef FOMENT_DEBUG
void TestBignums()
{
    char buf[128];

    FAssert(strcmp(NBignumToStringC(MakeBignumFromLong(0), 10), "0") == 0);
    sprintf_s(buf, sizeof(buf), LONG_FMT, MAXIMUM_FIXNUM);
    FAssert(strcmp(NBignumToStringC(MakeBignumFromLong(MAXIMUM_FIXNUM), 10), buf) == 0);
    sprintf_s(buf, sizeof(buf), LONG_FMT, MINIMUM_FIXNUM);
    FAssert(strcmp(NBignumToStringC(MakeBignumFromLong(MINIMUM_FIXNUM), 10), buf) == 0);

    sprintf_s(buf, sizeof(buf), LONG_FMT, MAXIMUM_FIXNUM + 1);
    FAssert(strcmp(NBignumToStringC(BignumFromUInt64(MAXIMUM_FIXNUM + 1, 0), 10), buf) == 0);
    FAssert(strcmp(NBignumToStringC(BignumFromUInt64(0xFFFFFFFFFFFFFFFUL, 0), 16),
            "fffffffffffffff") == 0);
    FAssert(strcmp(NBignumToStringC(BignumFromUInt64(0xFFFFFFFFFFFFFFFFUL, 0), 16),
            "ffffffffffffffff") == 0);
    FAssert(strcmp(NBignumToStringC(BignumFromUInt64(0x1234567890abcdefUL, 0), 16),
            "1234567890abcdef") == 0);

    FAssert(BignumToDouble(MakeBignumFromDouble(0.0)) == 0.0);
    FAssert(BignumToDouble(MakeBignumFromDouble(0.1)) == 0.0);
    FAssert(BignumToDouble(MakeBignumFromDouble(0.01)) == 0.0);
    FAssert(BignumToDouble(MakeBignumFromDouble(1.0)) == 1.0);
    FAssert(BignumToDouble(MakeBignumFromDouble(10.0)) == 10.0);
    FAssert(BignumToDouble(MakeBignumFromDouble(100.0)) == 100.0);
    FAssert(BignumToDouble(MakeBignumFromDouble(1234567890.0)) == 1234567890.0);
    FAssert(BignumToDouble(MakeBignumFromDouble(678.0E200)) == 678.0E200);
    FAssert(BignumToDouble(MakeBignumFromDouble(-0.0)) == 0.0);
    FAssert(BignumToDouble(MakeBignumFromDouble(-0.1)) == 0.0);
    FAssert(BignumToDouble(MakeBignumFromDouble(-0.01)) == 0.0);
    FAssert(BignumToDouble(MakeBignumFromDouble(-1.0)) == -1.0);
    FAssert(BignumToDouble(MakeBignumFromDouble(-10.0)) == -10.0);
    FAssert(BignumToDouble(MakeBignumFromDouble(-100.0)) == -100.0);
    FAssert(BignumToDouble(MakeBignumFromDouble(-1234567890.0)) == -1234567890.0);
    FAssert(BignumToDouble(MakeBignumFromDouble(-678.0E200)) == -678.0E200);

    FAssert(BignumToDouble(MakeBignumFromDouble(1.234567890123456)) == 1.0);
    FAssert(BignumToDouble(MakeBignumFromDouble(12.34567890123456)) == 12.0);
    FAssert(BignumToDouble(MakeBignumFromDouble(123.4567890123456)) == 123.0);
    FAssert(BignumToDouble(MakeBignumFromDouble(1234.567890123456)) == 1234.0);
    FAssert(BignumToDouble(MakeBignumFromDouble(12345.67890123456)) == 12345.0);
    FAssert(BignumToDouble(MakeBignumFromDouble(123456.7890123456)) == 123456.0);

    for (int idx = 0; idx <= 512; idx++)
    {
        double64_t d = 1234567890123456.0;

        FAssert(BignumToDouble(MakeBignumFromDouble(d)) == d);

        d *= 10.0;
    }

    FAssert(BignumCompare(MakeBignumFromDouble(1.0), MakeBignumFromDouble(1.0E100)) < 0);
    FAssert(NBignumCompare(MakeBignumFromDouble(1.0), MakeBignumFromDouble(1.0E100)) < 0);
    FAssert(BignumCompare(MakeBignumFromDouble(1.0E100), MakeBignumFromDouble(1.0)) > 0);
    FAssert(NBignumCompare(MakeBignumFromDouble(1.0E100), MakeBignumFromDouble(1.0)) > 0);
    FAssert(BignumCompare(MakeBignumFromDouble(-1.0), MakeBignumFromDouble(-1.0E100)) > 0);
    FAssert(NBignumCompare(MakeBignumFromDouble(-1.0), MakeBignumFromDouble(-1.0E100)) > 0);
    FAssert(BignumCompare(MakeBignumFromDouble(-1.0E100), MakeBignumFromDouble(-1.0)) < 0);
    FAssert(NBignumCompare(MakeBignumFromDouble(-1.0E100), MakeBignumFromDouble(-1.0)) < 0);
    FAssert(BignumCompare(MakeBignumFromDouble(-1.0), MakeBignumFromDouble(1.0)) < 0);
    FAssert(NBignumCompare(MakeBignumFromDouble(-1.0), MakeBignumFromDouble(1.0)) < 0);
    FAssert(BignumCompare(MakeBignumFromDouble(1.0), MakeBignumFromDouble(-1.0)) > 0);
    FAssert(NBignumCompare(MakeBignumFromDouble(1.0), MakeBignumFromDouble(-1.0)) > 0);

    FAssert(BignumCompare(MakeBignumFromDouble(0.0), MakeBignumFromDouble(0.0)) == 0);
    FAssert(NBignumCompare(MakeBignumFromDouble(0.0), MakeBignumFromDouble(0.0)) == 0);
    FAssert(BignumCompare(MakeBignumFromDouble(1.0), MakeBignumFromDouble(1.0)) == 0);
    FAssert(NBignumCompare(MakeBignumFromDouble(1.0), MakeBignumFromDouble(1.0)) == 0);
    FAssert(BignumCompare(MakeBignumFromDouble(-1.0), MakeBignumFromDouble(-1.0)) == 0);
    FAssert(NBignumCompare(MakeBignumFromDouble(-1.0), MakeBignumFromDouble(-1.0)) == 0);
    FAssert(BignumCompare(MakeBignumFromDouble(123456789012345.0),
            MakeBignumFromDouble(123456789012345.0)) == 0);
    FAssert(NBignumCompare(MakeBignumFromDouble(123456789012345.0),
            MakeBignumFromDouble(123456789012345.0)) == 0);
    FAssert(BignumCompare(MakeBignumFromDouble(-123456789012345.0),
            MakeBignumFromDouble(-123456789012345.0)) == 0);
    FAssert(NBignumCompare(MakeBignumFromDouble(-123456789012345.0),
            MakeBignumFromDouble(-123456789012345.0)) == 0);

    FAssert(BignumCompare(MakeBignumFromDouble(2.0), MakeBignumFromDouble(1.0)) > 0);
    FAssert(NBignumCompare(MakeBignumFromDouble(2.0), MakeBignumFromDouble(1.0)) > 0);
    FAssert(BignumCompare(MakeBignumFromDouble(-2.0), MakeBignumFromDouble(-1.0)) < 0);
    FAssert(NBignumCompare(MakeBignumFromDouble(-2.0), MakeBignumFromDouble(-1.0)) < 0);
    FAssert(BignumCompare(MakeBignumFromDouble(123456789012345.0),
            MakeBignumFromDouble(12345678901234.0)) > 0);
    FAssert(NBignumCompare(MakeBignumFromDouble(123456789012345.0),
            MakeBignumFromDouble(12345678901234.0)) > 0);
    FAssert(BignumCompare(MakeBignumFromDouble(-123456789012345.0),
            MakeBignumFromDouble(-12345678901234.0)) < 0);
    FAssert(NBignumCompare(MakeBignumFromDouble(-123456789012345.0),
            MakeBignumFromDouble(-12345678901234.0)) < 0);
    FAssert(BignumCompare(MakeBignumFromDouble(1234567891.0),
            MakeBignumFromDouble(1234567890.0)) > 0);
    FAssert(NBignumCompare(MakeBignumFromDouble(1234567891.0),
            MakeBignumFromDouble(1234567890.0)) > 0);

    FAssert(BignumCompareZero(MakeBignumFromLong(1)) > 0);
    FAssert(NBignumCompareZero(MakeBignumFromLong(1)) > 0);
    FAssert(BignumCompareZero(MakeBignumFromLong(-1)) < 0);
    FAssert(NBignumCompareZero(MakeBignumFromLong(-1)) < 0);
    FAssert(BignumCompareZero(MakeBignumFromLong(0)) == 0);
    FAssert(NBignumCompareZero(MakeBignumFromLong(0)) == 0);

    FObject bn;
    bn = BignumAddLong(MakeBignumFromDouble(123456.789E10), 10);
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumAddLong(MakeBignumFromDouble(123456.789E10), 0xFFFFFFF);
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
#ifdef FOMENT_64BIT
    bn = BignumAddLong(MakeBignumFromDouble(123456.789E10), 0x1000000000000L);
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumAddLong(MakeBignumFromDouble(123456.789E100), 0x1000000000000L);
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
#endif // FOMENT_64BIT

    bn = BignumAdd(MakeBignumFromDouble(1234567.89E20), MakeBignumFromDouble(2828282.5789E20));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumAdd(MakeBignumFromDouble(1234567.89E10), MakeBignumFromDouble(2828282.5789E20));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumAdd(MakeBignumFromDouble(1234567.89E100), MakeBignumFromDouble(2828282.5789E20));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumAdd(MakeBignumFromDouble(1234567.89E10), MakeBignumFromDouble(2828282.5789E200));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumAdd(MakeBignumFromDouble(1234567.89), MakeBignumFromDouble(2828282.5789E20));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumAdd(MakeBignumFromDouble(1234567.89E10), MakeBignumFromDouble(2828282.5789E10));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);

    bn = BignumAdd(MakeBignumFromDouble(-1234567.89E20), MakeBignumFromDouble(2828282.5789E20));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumAdd(MakeBignumFromDouble(1234567.89E10), MakeBignumFromDouble(-2828282.5789E20));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumAdd(MakeBignumFromDouble(-1234567.89E100), MakeBignumFromDouble(-2828282.5789E20));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumAdd(MakeBignumFromDouble(-1234567.89E10), MakeBignumFromDouble(2828282.5789E200));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumAdd(MakeBignumFromDouble(1234567.89), MakeBignumFromDouble(-2828282.5789E20));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumAdd(MakeBignumFromDouble(-1234567.89E10), MakeBignumFromDouble(2828282.5789E10));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);

    bn = BignumSubtract(MakeBignumFromDouble(1234567.89E20), MakeBignumFromDouble(2828282.5789E20));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumSubtract(MakeBignumFromDouble(1234567.89E10), MakeBignumFromDouble(2828282.5789E20));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumSubtract(MakeBignumFromDouble(1234567.89E100), MakeBignumFromDouble(2828282.5789E20));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumSubtract(MakeBignumFromDouble(1234567.89E10), MakeBignumFromDouble(2828282.5789E200));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumSubtract(MakeBignumFromDouble(1234567.89), MakeBignumFromDouble(2828282.5789E20));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumSubtract(MakeBignumFromDouble(1234567.89E10), MakeBignumFromDouble(2828282.5789E10));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);

    bn = BignumSubtract(MakeBignumFromDouble(-1234567.89E20), MakeBignumFromDouble(2828282.5789E20));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumSubtract(MakeBignumFromDouble(1234567.89E10), MakeBignumFromDouble(-2828282.5789E20));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumSubtract(MakeBignumFromDouble(-1234567.89E100),
            MakeBignumFromDouble(-2828282.5789E20));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumSubtract(MakeBignumFromDouble(-1234567.89E10),
            MakeBignumFromDouble(2828282.5789E200));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumSubtract(MakeBignumFromDouble(1234567.89), MakeBignumFromDouble(-2828282.5789E20));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumSubtract(MakeBignumFromDouble(-1234567.89E10), MakeBignumFromDouble(2828282.5789E10));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);

    bn = BignumMultiply(MakeBignumFromDouble(1234567.89E10), MakeBignumFromDouble(2828282.5789E10));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumMultiply(MakeBignumFromLong(123456789012345678L),
            MakeBignumFromLong(123456789012345678L));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumMultiply(MakeBignumFromLong(-123456789012345678L),
            MakeBignumFromLong(123456789012345678L));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumMultiply(MakeBignumFromLong(123456789012345678),
            MakeBignumFromLong(-123456789012345678L));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumMultiply(MakeBignumFromLong(-123456789012345678L),
            MakeBignumFromLong(-123456789012345678L));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumMultiply(MakeBignumFromLong(123456789012345678L),
            MakeBignumFromDouble(2828282.5789E10));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);

    FAssert(strcmp(BignumToStringC(BignumAnd(MakeBignumFromDouble(1234567890E10),
            MakeBignumFromDouble(987654321E9)), 10), "654297430996617216") == 0);
    FAssert(strcmp(BignumToStringC(BignumAnd(MakeBignumFromDouble(-1234567890E10),
            MakeBignumFromDouble(987654321E9)), 10), "333356890003384320") == 0);
    FAssert(strcmp(BignumToStringC(BignumAnd(
            BignumMultiply(MakeBignumFromLong(123456789012345678L), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(123456789987654321L), MakeBignumFromDouble(7777E33))),
            10), "764365356449759452798730384693400720346950920044544") == 0);
    FAssert(strcmp(BignumToStringC(BignumAnd(
            BignumMultiply(MakeBignumFromLong(-123456789012345678L), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(123456789987654321L), MakeBignumFromDouble(7777E33))),
            10), "959359090377537933306560371696748505119465178377748480") == 0);
    FAssert(strcmp(BignumToStringC(BignumAnd(
            BignumMultiply(MakeBignumFromLong(123456789012345678L), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(-123456789987654321L), MakeBignumFromDouble(7777E33))),
            10), "410717112421698380931919236597841253094682326969876480") == 0);
    FAssert(strcmp(BignumToStringC(BignumAnd(
            BignumMultiply(MakeBignumFromLong(-123456789012345678L), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(-123456789987654321L), MakeBignumFromDouble(7777E33))),
            10), "-1370840568155686073691278338679278436568011586622455808") == 0);
}
#endif // FOMENT_DEBUG
