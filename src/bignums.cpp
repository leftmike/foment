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

// ---- Root ----

static FObject MaximumDoubleBignum = NoValueObject;

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
static long_t NBignumSign(FObject num);
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
static void BignumNegate(FBignum * bn);
static FBignum * BignumLeftShift(FBignum * bn, long_t cnt);

static FBignum * MakeBignum(ulong_t dc)
{
    FAssert(dc > 0);
    FAssert(dc < MAXIMUM_DIGIT_COUNT);

    FBignum * bn = (FBignum *) MakeObject(BignumTag, sizeof(FBignum) + (dc - 1) * sizeof(uint32_t),
            0, "%make-bignum");
    mpz_init(bn->MPInteger);
    InstallGuardian(bn, CleanupTConc);

    bn->Sign = 1;
    bn->Used = 1;
    memset(bn->Digits, 0, dc * sizeof(uint32_t));

    FAssert(MaximumDigits(bn) == dc);

    return(bn);
}

static inline FBignum * MakeBignum()
{
    return(MakeBignum(1));
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
    FAssert(BignumP(bn));

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
    FAssert(BignumP(bn));

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

    FAssert(bn->Used > 0);
    FAssert(bn->Used < 2 || bn->Digits[bn->Used - 1] != 0);
    FAssert(bn->Sign == 1 || bn->Sign == -1);
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    return(bn);
}

static FBignum * CopyBignum(FBignum * bn, uint16_t xtr)
{
    FAssert(bn->Used > 0);
    FAssert(bn->Used < 2 || bn->Digits[bn->Used - 1] != 0);
    FAssert(bn->Sign == 1 || bn->Sign == -1);

    FBignum * ret = MakeBignum(bn->Used + xtr);
    mpz_init_set(ret->MPInteger, bn->MPInteger);
//    InstallGuardian(ret, CleanupTConc);

    ret->Sign = bn->Sign;
    memcpy(ret->Digits, bn->Digits, bn->Used * sizeof(uint32_t));
    ret->Used = bn->Used;

    FAssert(ret->Used > 0);
    FAssert(ret->Used < 2 || ret->Digits[ret->Used - 1] != 0);
    FAssert(ret->Sign == 1 || ret->Sign == -1);

    return(ret);
}

static inline FBignum * CopyBignum(FBignum * bn)
{
    return(CopyBignum(bn, 0));
}

FObject CopyBignum(FObject n)
{
    FAssert(BignumP(n));

    return(CopyBignum(AsBignum(n), 0));
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
    if (BignumP(num))
    {
        FBignum * bn = AsBignum(num);

        FAssert(bn->Used > 0);
        FAssert(bn->Used < 2 || bn->Digits[bn->Used - 1] != 0);
        FAssert(bn->Sign == 1 || bn->Sign == -1);

        if (mpz_cmp_si(bn->MPInteger, (long) MINIMUM_FIXNUM) >= 0
                && mpz_cmp_si(bn->MPInteger, (long) MAXIMUM_FIXNUM) <= 0)
        {
            FAssert(bn->Used == 1 && bn->Digits[0] <= MAXIMUM_FIXNUM);
            FAssert((bn->Sign > 0 && bn->Digits[0] == mpz_get_si(bn->MPInteger)) ||
                    (bn->Sign < 0 && bn->Digits[0] == - mpz_get_si(bn->MPInteger)));

            return(MakeFixnum(mpz_get_si(bn->MPInteger)));
        }
    }

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

    FAssert(bn->Used > 0);
    FAssert(bn->Used < 2 || bn->Digits[bn->Used - 1] != 0);
    FAssert(bn->Sign == 1 || bn->Sign == -1);
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);

    return(bn);
}

FObject MakeIntegerFromUInt64(uint64_t n)
{
    if (n <= MAXIMUM_FIXNUM)
        return(MakeFixnum(n));

    return(BignumFromUInt64(n, 0));
}

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

        FAssert(od == d);
    }

    return(od);
}

char * BignumToStringC(FObject bn, long_t rdx)
{
    return(mpz_get_str(0, (int) rdx, AsBignum(bn)->MPInteger));
}

#ifdef FOMENT_DEBUG
static const char DigitTable[] = "0123456789abcdefghijklmnopqrstuvwxyz";

static char * NBignumToStringC(FObject num, uint32_t rdx)
{
    FAssert(BignumP(num));

    FBignum * bn = AsBignum(num);

    FAssert(bn->Used > 0);
    FAssert(bn->Used < 2 || bn->Digits[bn->Used - 1] != 0);
    FAssert(bn->Sign == 1 || bn->Sign == -1);

    FBignum * tbn = CopyBignum(bn);
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

    if (NBignumSign(num) < 0)
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

    if (sgn < 0)
        BignumNegate(rbn);

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
            int32_t dv = DigitValue(s[sdx]);

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

    if (sgn < 0)
        BignumNegate(bn);

    FAssert(bn->Used > 0);
    FAssert(bn->Used < 2 || bn->Digits[bn->Used - 1] != 0);
    FAssert(bn->Sign == 1 || bn->Sign == -1);
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

static long_t NBignumSign(FObject num)
{
    FAssert(BignumP(num));

    FBignum * bn = AsBignum(num);

    FAssert(bn->Used > 0);
    FAssert(bn->Used < 2 || bn->Digits[bn->Used - 1] != 0);
    FAssert(bn->Sign == 1 || bn->Sign == -1);

    if (AsBignum(bn)->Used == 1 && AsBignum(bn)->Digits[0] == 0)
        return(0);
    else if (AsBignum(bn)->Sign > 0)
        return(1);

    FAssert(AsBignum(bn)->Sign < 0);

    return(-1);
}
#endif // FOMENT_DEBUG

long_t BignumCompare(FObject bn1, FObject bn2)
{
    FAssert(BignumP(bn1));
    FAssert(BignumP(bn2));

    long_t ret = mpz_cmp(AsBignum(bn1)->MPInteger, AsBignum(bn2)->MPInteger);

#ifdef FOMENT_DEBUG
    if (NBignumCompare(bn1, bn2) != ret)
    {
        printf("BignumCompare(%s, %s) = " LONG_FMT "\n", BignumToStringC(bn1, 10),
                BignumToStringC(bn2, 10), ret);
        printf("NBignumCompare(%s, %s) = " LONG_FMT "\n\n", NBignumToStringC(bn1, 10),
                NBignumToStringC(bn2, 10), NBignumCompare(bn1, bn2));
    }
#endif // FOMENT_DEBUG
//    FAssert(NBignumCompare(bn1, bn2) == ret);

    return(ret);
}

long_t BignumSign(FObject bn)
{
    FAssert(BignumP(bn));

    long_t sgn = mpz_sgn(AsBignum(bn)->MPInteger);

#ifdef FOMENT_DEBUG
    if (NBignumSign(bn) != sgn)
        printf("BignumSign\nmpz: " LONG_FMT " %s\nnew: " LONG_FMT " %s\n\n", sgn,
                BignumToStringC(bn, 10), NBignumSign(bn), NBignumToStringC(bn, 10));
#endif // FOMENT_DEBUG
    FAssert(NBignumSign(bn) == sgn);

    return(sgn);
}

static void BignumNegate(FBignum * bn)
{
    FAssert(bn->Used > 0);
    FAssert(bn->Used < 2 || bn->Digits[bn->Used - 1] != 0);
    FAssert(bn->Sign == 1 || bn->Sign == -1);

    mpz_neg(bn->MPInteger, bn->MPInteger);
    if (bn->Used > 1 || bn->Digits[0] != 0)
        bn->Sign *= -1;
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

    FAssert(ret->Used > 0);
    FAssert(ret->Used < 2 || ret->Digits[ret->Used - 1] != 0);
    FAssert(ret->Sign == 1 || ret->Sign == -1);
}

static void DigitsSubtract(FBignum * ret, uint32_t * digits1, uint16_t used1,
    uint32_t * digits2, uint16_t used2)
{
    FAssert(used2 <= used1);

    ret->Used = 1;

    uint16_t idx = 0;
    uint32_t b = 0;
    while (idx < used1)
    {
        uint32_t d = idx < used2 ? digits2[idx] : 0;
        if (digits1[idx] >= b && digits1[idx] - b >= d)
        {
            ret->Digits[idx] = digits1[idx] - b - d;
            b = 0;
        }
        else
        {
            uint64_t v =  0x100000000UL + digits1[idx] - b - d;
            ret->Digits[idx] = v & 0xFFFFFFFF;
            b = 1;
        }

        if (ret->Digits[idx] != 0)
            ret->Used = idx + 1;
        idx += 1;
    }

    FAssert(ret->Used > 0);
    FAssert(ret->Used < 2 || ret->Digits[ret->Used - 1] != 0);
    FAssert(ret->Sign == 1 || ret->Sign == -1);
// XXX
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

static FBignum * BignumAddDigits(uint32_t * digits1, uint16_t used1, int16_t sgn1,
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
    DigitsSubtract(ret, digits1, used1, digits2, used2);
    return(ret);
}

FObject BignumAdd(FObject bn1, FObject bn2)
{
    FAssert(BignumP(bn1));
    FAssert(BignumP(bn2));

    FBignum * ret = BignumAddDigits(AsBignum(bn1)->Digits, AsBignum(bn1)->Used, AsBignum(bn1)->Sign,
            AsBignum(bn2)->Digits, AsBignum(bn2)->Used, AsBignum(bn2)->Sign);
    mpz_add(ret->MPInteger, AsBignum(bn1)->MPInteger, AsBignum(bn2)->MPInteger);

    FAssert(ret->Used > 0);
    FAssert(ret->Used < 2 || ret->Digits[ret->Used - 1] != 0);
    FAssert(ret->Sign == 1 || ret->Sign == -1);
    FAssert(strcmp(BignumToStringC(ret, 10), NBignumToStringC(ret, 10)) == 0);

    return(ret);
}

static void DigitsAddUInt32(FBignum * bn, uint16_t idx, uint32_t n)
{
    while (idx < bn->Used && n > 0)
    {
        uint64_t v = bn->Digits[idx] + (uint64_t) n;
        bn->Digits[idx] = v & 0xFFFFFFFF;
        n = v >> UINT32_BITS;
        idx += 1;
    }

    if (n > 0)
    {
        FAssert(MaximumDigits(bn) > idx);

        bn->Digits[idx] = n;
        bn->Used = idx + 1;
    }
}

static void UpdateAddUInt32(FBignum * bn, uint32_t n)
{
    mpz_add_ui(bn->MPInteger, bn->MPInteger, n);

    if (bn->Sign >= 0)
        DigitsAddUInt32(bn, 0, n);
    else if (bn->Used == 1)
    {
        if (n >= bn->Digits[0])
        {
            bn->Digits[0] = n - bn->Digits[0];
            bn->Sign = 1;
        }
        else
            bn->Digits[0] -= n;
    }
    else
    {
        FAssert(bn->Used > 1);

        uint16_t idx = 0;
        while (n > 0)
        {
            if (bn->Digits[idx] >= n)
            {
                bn->Digits[idx] -= n;
                break;
            }
            else
            {
                uint64_t v =  0x100000000UL + bn->Digits[idx] - n;
                bn->Digits[idx] = v & 0xFFFFFFFF;
                n = 1;
            }

            idx += 1;

            FAssert(idx < bn->Used);
        }
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
    uint16_t used = 1;
#endif // FOMENT_32BIT
#ifdef FOMENT_64BIT
    uint32_t digits[2];
    digits[0] = n & 0xFFFFFFFF;
    digits[1] = n >> 32;
    uint16_t used = (digits[1] != 0 ? 2 : 1);
#endif // FOMENT_64BIT

    FBignum * ret = BignumAddDigits(AsBignum(bn)->Digits, AsBignum(bn)->Used, AsBignum(bn)->Sign,
            digits, used, nsgn);
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

#ifdef FOMENT_DEBUG
    if (strcmp(BignumToStringC(ret, 10), NBignumToStringC(ret, 10)) != 0)
    {
        printf("BignumAddLong\nmpz: %s + " LONG_FMT " = %s\n", BignumToStringC(bn, 10), n * nsgn,
                BignumToStringC(ret, 10));
        printf("new: %s + " LONG_FMT " = %s\n\n", NBignumToStringC(bn, 10), n * nsgn,
                NBignumToStringC(ret, 10));
    }
#endif // FOMENT_DEBUG
    FAssert(ret->Used > 0);
    FAssert(ret->Used < 2 || ret->Digits[ret->Used - 1] != 0);
    FAssert(ret->Sign == 1 || ret->Sign == -1);
//    FAssert(strcmp(BignumToStringC(ret, 10), NBignumToStringC(ret, 10)) == 0);

    return(ret);
}

FObject BignumSubtract(FObject bn1, FObject bn2)
{
    FAssert(BignumP(bn1));
    FAssert(BignumP(bn2));

    FBignum * ret = BignumAddDigits(AsBignum(bn1)->Digits, AsBignum(bn1)->Used,
            AsBignum(bn1)->Sign, AsBignum(bn2)->Digits, AsBignum(bn2)->Used,
            AsBignum(bn2)->Sign * -1);
    mpz_sub(AsBignum(ret)->MPInteger, AsBignum(bn1)->MPInteger, AsBignum(bn2)->MPInteger);

#ifdef FOMENT_DEBUG
    if (strcmp(BignumToStringC(ret, 10), NBignumToStringC(ret, 10)) != 0)
    {
        printf("BignumSubtract\nmpz: %s - %s = %s\n", BignumToStringC(bn1, 10),
                BignumToStringC(bn2, 10), BignumToStringC(ret, 10));
        printf("new: %s - %s = %s\n\n", NBignumToStringC(bn1, 10), NBignumToStringC(bn2, 10),
                NBignumToStringC(ret, 10));
    }
#endif // FOMENT_DEBUG
    FAssert(ret->Used > 0);
    FAssert(ret->Used < 2 || ret->Digits[ret->Used - 1] != 0);
    FAssert(ret->Sign == 1 || ret->Sign == -1);
//    FAssert(strcmp(BignumToStringC(ret, 10), NBignumToStringC(ret, 10)) == 0);

    return(ret);
}

static FBignum * BignumMultiplyDigits(uint32_t * digits1, uint16_t used1, uint32_t * digits2,
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

    FBignum * ret = BignumMultiplyDigits(AsBignum(bn1)->Digits, AsBignum(bn1)->Used,
            AsBignum(bn2)->Digits, AsBignum(bn2)->Used);
    AsBignum(ret)->Sign = AsBignum(bn1)->Sign * AsBignum(bn2)->Sign;
    mpz_mul(AsBignum(ret)->MPInteger, AsBignum(bn1)->MPInteger, AsBignum(bn2)->MPInteger);

#ifdef FOMENT_DEBUG
    if (strcmp(BignumToStringC(ret, 10), NBignumToStringC(ret, 10)) != 0)
    {
        printf("BignumMultiply\nmpz: %s * %s = %s\n", BignumToStringC(bn1, 10),
                BignumToStringC(bn2, 10), BignumToStringC(ret, 10));
        printf("new: %s * %s = %s\n\n", NBignumToStringC(bn1, 10), NBignumToStringC(bn2, 10),
                NBignumToStringC(ret, 10));
    }
#endif // FOMENT_DEBUG
    FAssert(ret->Used > 0);
    FAssert(ret->Used < 2 || ret->Digits[ret->Used - 1] != 0);
    FAssert(ret->Sign == 1 || ret->Sign == -1);
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

    FBignum * ret = BignumMultiplyDigits(AsBignum(bn)->Digits, AsBignum(bn)->Used, digits,
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

#ifdef FOMENT_DEBUG
    if (strcmp(BignumToStringC(ret, 10), NBignumToStringC(ret, 10)) != 0)
    {
        printf("BignumMultiplyLong\nmpz: %s * " LONG_FMT " = %s\n", BignumToStringC(bn, 10), n,
                BignumToStringC(ret, 10));
        printf("new: %s * " LONG_FMT " = %s\n\n", NBignumToStringC(bn, 10), n,
                NBignumToStringC(ret, 10));
    }
#endif // FOMENT_DEBUG
    FAssert(ret->Used > 0);
    FAssert(ret->Used < 2 || ret->Digits[ret->Used - 1] != 0);
    FAssert(ret->Sign == 1 || ret->Sign == -1);
//    FAssert(strcmp(BignumToStringC(ret, 10), NBignumToStringC(ret, 10)) == 0);

    return(ret);
}

#ifdef FOMENT_DEBUG
static long_t BignumOddP(FBignum * bn)
{
    FAssert(bn->Used > 0);
    FAssert(bn->Used < 2 || bn->Digits[bn->Used - 1] != 0);
    FAssert(bn->Sign == 1 || bn->Sign == -1);

    return(bn->Used != 0 && (bn->Digits[0] & 0x1));
}
#endif // FOMENT_DEBUG

long_t BignumOddP(FObject n)
{
    FAssert(BignumP(n));

    FAssert((mpz_tdiv_ui(AsBignum(n)->MPInteger, 2) != 0) == BignumOddP(AsBignum(n)));

    return(mpz_tdiv_ui(AsBignum(n)->MPInteger, 2) != 0);
}

ulong_t BignumHash(FObject n)
{
    FAssert(BignumP(n));

    // XXX
    return(mpz_tdiv_ui(AsBignum(n)->MPInteger, MAXIMUM_FIXNUM));
}

static ulong_t DigitsIntegerLength(FBignum * bn)
{
    uint16_t idx = bn->Used;
    while (idx > 0)
    {
        idx -= 1;
        if (bn->Digits[idx] != 0)
            return(idx * UINT32_BITS + HighestBitUInt32(bn->Digits[idx]) + 1);
    }

    return(0);
}

static long_t DigitsCompareShift(FBignum * n, FBignum * d)
{
    ulong_t nbl = DigitsIntegerLength(n);
    ulong_t dbl = DigitsIntegerLength(d);

    if (nbl < dbl)
        return(-1);
    else if (nbl == dbl)
    {
        FAssert(n->Used == d->Used);

        uint16_t idx = n->Used;
        while (idx > 0)
        {
            idx -= 1;
            if (n->Digits[idx] != d->Digits[idx])
                break;
        }

        return(n->Digits[idx] < d->Digits[idx] ? -1 : 0);
    }

    return(nbl - dbl - 1);
}

/*
n -= (d << m)
*/
static void UpdateShiftSubtract(FBignum * n, FBignum * d, long_t cnt)
{
    long_t dcnt = cnt / UINT32_BITS;
    long_t bcnt = cnt % UINT32_BITS;

    uint32_t s = 0;
    uint16_t idx = 0;
    uint32_t b = 0;
    while (idx < d->Used)
    {
        FAssert(idx + dcnt < n->Used);

        s |= d->Digits[idx] << bcnt;
        if (n->Digits[idx + dcnt] >= b && n->Digits[idx + dcnt] - b >= s)
        {
            n->Digits[idx + dcnt] = n->Digits[idx + dcnt] - b - s;
            b = 0;
        }
        else
        {
            uint64_t v =  0x100000000UL + n->Digits[idx + dcnt] - b - s;
            n->Digits[idx + dcnt] = v & 0xFFFFFFFF;
            b = 1;
        }

        if (bcnt > 0)
            s = d->Digits[idx] >> (UINT32_BITS - bcnt);
        else
            s = 0;
        idx += 1;
    }

    if (s > 0 || b > 0)
    {
        FAssert(idx + dcnt < n->Used);
        FAssert(n->Digits[idx + dcnt] >= s && n->Digits[idx + dcnt] - s >= b);

        n->Digits[idx + dcnt] = n->Digits[idx + dcnt] - b - s;
    }
    else
    {
        FAssert(b == 0);
    }

    while (n->Used > 1 && n->Digits[n->Used - 1] == 0)
        n->Used -= 1;
}

/*
q = 0;
while (n > d)
{
    m = IntegerLength(n) - IntegerLength(d);
    n -= (d << m);
    q += (1 << m);
}
return(q); // remainder is in n
*/
static FBignum * DigitsDivideRemainder(FBignum * n, FBignum * d)
{
    FAssert(n->Used > 0);
    FAssert(n->Used < 2 || n->Digits[n->Used - 1] != 0);
    FAssert(n->Sign == 1 || n->Sign == -1);

    FAssert(d->Used > 0);
    FAssert(d->Used < 2 || d->Digits[d->Used - 1] != 0);
    FAssert(d->Sign == 1 || d->Sign == -1);

#ifdef FOMENT_DEBUG
//    printf("mpz: %s / %s\n", BignumToStringC(n, 10), BignumToStringC(d, 10));
//    printf("new: %s / %s\n", NBignumToStringC(n, 10), NBignumToStringC(d, 10));
#endif // FOMENT_DEBUG
    FBignum * q = MakeBignum(n->Used - d->Used + 1);
    for (;;)
    {
        long_t m = DigitsCompareShift(n, d);
        if (m < 0)
            break;

        UpdateShiftSubtract(n, d, m);
        DigitsAddUInt32(q, (uint16_t) (m / UINT32_BITS), 1 << (m % UINT32_BITS));
    }

    q->Sign = n->Sign * d->Sign;

    FAssert(n->Used > 0);
    FAssert(n->Used < 2 || n->Digits[n->Used - 1] != 0);
    FAssert(n->Sign == 1 || n->Sign == -1);

    FAssert(q->Used > 0);
    FAssert(q->Used < 2 || q->Digits[q->Used - 1] != 0);
    FAssert(q->Sign == 1 || q->Sign == -1);

    return(q);
}

static FBignum * BignumDivide(FBignum * n, FBignum * d)
{
    if (d->Used == 1 && d->Digits[0] == 1)
        return(CopyBignum(n));
    else if (n->Used < d->Used)
        return(MakeBignum());

    return(DigitsDivideRemainder(CopyBignum(n), d));
}

FObject BignumDivide(FObject n, FObject d)
{
    FAssert(BignumP(n));
    FAssert(BignumP(d));

    FBignum * q = BignumDivide(AsBignum(n), AsBignum(d));
    mpz_tdiv_q(q->MPInteger, AsBignum(n)->MPInteger, AsBignum(d)->MPInteger);

#ifdef FOMENT_DEBUG
    if (strcmp(BignumToStringC(q, 10), NBignumToStringC(q, 10)) != 0)
    {
        printf("BignumDivide\nmpz: %s / %s\n", BignumToStringC(n, 10), BignumToStringC(d, 10));
        printf("new: %s / %s\n", NBignumToStringC(n, 10), NBignumToStringC(d, 10));
        printf("mpz: %s new: %s\n\n", BignumToStringC(q, 10), NBignumToStringC(q, 10));
    }
#endif // FOMENT_DEBUG
//    FAssert(strcmp(BignumToStringC(q, 10), NBignumToStringC(q, 10)) == 0);

    return(q);
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

static FBignum * BignumRemainder(FBignum * n, FBignum * d)
{
    FBignum * rem = CopyBignum(n);
    if (d->Used > 0 && n->Used >= d->Used)
        DigitsDivideRemainder(rem, AsBignum(d));
    return(rem);
}

FObject BignumRemainder(FObject n, FObject d)
{
    FAssert(BignumP(n));
    FAssert(BignumP(d));

    FBignum * rem = BignumRemainder(AsBignum(n), AsBignum(d));
    mpz_tdiv_r(rem->MPInteger, AsBignum(n)->MPInteger, AsBignum(d)->MPInteger);

    FAssert(strcmp(BignumToStringC(rem, 10), NBignumToStringC(rem, 10)) == 0);

    return(rem);
}

FObject BignumSqrt(FObject * rem, FObject bn)
{
    FAssert(BignumP(bn));
    FAssert(BignumSign(bn) > 0);

    FObject ret;
    if (BignumCompare(bn, MaximumDoubleBignum) < 0)
    {
        ret = MakeBignumFromDouble(sqrt(BignumToDouble(bn)));
        *rem = BignumSubtract(bn, BignumMultiply(ret, ret));
    }
    else
    {
        // Use Newton - Rhapson
        double64_t d = BignumToDouble(bn);
        FObject x = IsFinite(sqrt(d)) ? MakeBignumFromDouble(sqrt(d)) :
                BignumLeftShift(AsBignum(MakeBignumFromLong(1)), (BignumIntegerLength(bn) / 2));
        for (;;)
        {
            FObject xsq = BignumMultiply(x, x);
            if (BignumCompare(bn, xsq) < 0)
                x = BignumDivide(BignumAdd(xsq, bn), BignumMultiplyLong(x, 2));
            else
            {
                if (BignumCompare(bn,
                        BignumAddLong(BignumAdd(xsq, BignumMultiplyLong(x, 2)), 1)) < 0)
                {
                    ret = x;
                    *rem = BignumSubtract(bn, xsq);
                    break;
                }
                x = BignumDivide(BignumAdd(xsq, bn), BignumMultiplyLong(x, 2));
            }
        }
    }

    mpz_sqrtrem(AsBignum(ret)->MPInteger, AsBignum(*rem)->MPInteger, AsBignum(bn)->MPInteger);

#ifdef FOMENT_DEBUG
    if (strcmp(BignumToStringC(ret, 10), NBignumToStringC(ret, 10)) != 0)
    {
        printf("BignumSqrt\nmpz: %s\nnew: %s\n", BignumToStringC(bn, 10), BignumToStringC(bn, 10));
        printf("mpz: %s\nnew: %s\n", BignumToStringC(ret, 10), NBignumToStringC(ret, 10));
        printf("mpz: %s\nnew: %s\n\n", BignumToStringC(*rem, 10), NBignumToStringC(*rem, 10));
    }
#endif // FOMENT_DEBUG
    FAssert(BignumP(ret));
    FAssert(AsBignum(ret)->Used > 0);
    FAssert(AsBignum(ret)->Used < 2 || AsBignum(ret)->Digits[AsBignum(ret)->Used - 1] != 0);
    FAssert(AsBignum(ret)->Sign == 1 || AsBignum(ret)->Sign == -1);

    FAssert(BignumP(*rem));
    FAssert(AsBignum(*rem)->Used > 0);
    FAssert(AsBignum(*rem)->Used < 2 || AsBignum(*rem)->Digits[AsBignum(*rem)->Used - 1] != 0);
    FAssert(AsBignum(*rem)->Sign == 1 || AsBignum(*rem)->Sign == -1);
//    FAssert(strcmp(BignumToStringC(ret, 10), NBignumToStringC(ret, 10)) == 0);
//    FAssert(strcmp(BignumToStringC(*rem, 10), NBignumToStringC(*rem, 10)) == 0);

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
    mpz_and(ret->MPInteger, AsBignum(bn1)->MPInteger, AsBignum(bn2)->MPInteger);

    FAssert(ret->Used > 0);
    FAssert(ret->Used < 2 || ret->Digits[ret->Used - 1] != 0);
    FAssert(ret->Sign == 1 || ret->Sign == -1);
    FAssert(strcmp(BignumToStringC(ret, 10), NBignumToStringC(ret, 10)) == 0);

    return(ret);
}

static FBignum * BignumIOr(FBignum * bn1, FBignum * bn2)
{
    uint16_t dc = bn1->Used > bn2->Used ? bn1->Used : bn2->Used;
    uint16_t xtr1 = bn1->Used;
    uint16_t xtr2 = bn2->Used;
    if (bn1->Sign < 0)
    {
        bn1 = BignumComplement(CopyBignum(bn1));
        dc = bn1->Used;
        xtr2 = 0;
    }
    if (bn2->Sign < 0)
    {
        bn2 = BignumComplement(CopyBignum(bn2));
        if (bn1->Sign < 0 && bn2->Used > bn1->Used)
            dc = bn1->Used;
        else
            dc = bn2->Used;
        xtr1 = 0;
    }

    FBignum * ret = MakeBignum(dc);
    uint16_t idx = 0;
    while (idx < bn1->Used && idx < bn2->Used)
    {
        ret->Digits[idx] = bn1->Digits[idx] | bn2->Digits[idx];
        idx += 1;
    }

    if (idx < xtr1)
        while (idx < bn1->Used)
        {
            ret->Digits[idx] = bn1->Digits[idx];
            idx += 1;
        }
    else if (idx < xtr2)
        while (idx < bn2->Used)
        {
            ret->Digits[idx] = bn2->Digits[idx];
            idx += 1;
        }
    ret->Used = idx;
    if (bn1->Sign < 0 || bn2->Sign < 0)
    {
        ret->Sign = -1;
        BignumComplement(ret);
    }
    else
        ret->Sign = 1;

    return(ret);
}

FObject BignumIOr(FObject bn1, FObject bn2)
{
    FAssert(BignumP(bn1));
    FAssert(BignumP(bn2));

    FBignum * ret = BignumIOr(AsBignum(bn1), AsBignum(bn2));
    mpz_ior(ret->MPInteger, AsBignum(bn1)->MPInteger, AsBignum(bn2)->MPInteger);

    FAssert(ret->Used > 0);
    FAssert(ret->Used < 2 || ret->Digits[ret->Used - 1] != 0);
    FAssert(ret->Sign == 1 || ret->Sign == -1);
    FAssert(strcmp(BignumToStringC(ret, 10), NBignumToStringC(ret, 10)) == 0);

    return(ret);
}

FObject BignumXOr(FObject bn1, FObject bn2)
{
    FAssert(BignumP(bn1));
    FAssert(BignumP(bn2));

    FObject ret = BignumAnd(BignumIOr(bn1, bn2), BignumNot(BignumAnd(bn1, bn2)));
    mpz_xor(AsBignum(ret)->MPInteger, AsBignum(bn1)->MPInteger, AsBignum(bn2)->MPInteger);

    FAssert(AsBignum(ret)->Used > 0);
    FAssert(AsBignum(ret)->Used < 2 || AsBignum(ret)->Digits[AsBignum(ret)->Used - 1] != 0);
    FAssert(AsBignum(ret)->Sign == 1 || AsBignum(ret)->Sign == -1);
    FAssert(strcmp(BignumToStringC(ret, 10), NBignumToStringC(ret, 10)) == 0);

    return(ret);
}

FObject BignumNot(FObject bn)
{
    FAssert(BignumP(bn));

    FBignum * ret = CopyBignum(AsBignum(bn), 1);
    UpdateAddUInt32(ret, 1);
    BignumNegate(ret);

    FAssert(ret->Used > 0);
    FAssert(ret->Used < 2 || ret->Digits[ret->Used - 1] != 0);
    FAssert(ret->Sign == 1 || ret->Sign == -1);
    FAssert(strcmp(BignumToStringC(ret, 10), NBignumToStringC(ret, 10)) == 0);

    return(ret);
}

static ulong_t BignumBitCount(FBignum * bn)
{
    if (bn->Sign < 0)
        bn = BignumComplement(CopyBignum(bn));

    ulong_t bc = 0;
    uint16_t idx = 0;
    while (idx < bn->Used)
    {
        bc += PopulationCount(bn->Digits[idx]);
        idx += 1;
    }

    if (bn->Sign < 0)
        bc = bn->Used * UINT32_BITS - bc;

    return(bc);
}

ulong_t BignumBitCount(FObject bn)
{
    FAssert(BignumP(bn));

    ulong_t bc = BignumBitCount(AsBignum(bn));

    FAssert(NBignumSign(bn) < 0 || bc == mpz_popcount(AsBignum(bn)->MPInteger));

    return(bc);
}

#ifdef FOMENT_DEBUG
static ulong_t BignumIntegerLength(FBignum * bn)
{
    if (bn->Sign < 0)
    {
        bn = CopyBignum(bn, 1);
        UpdateAddUInt32(bn, 1);
    }

    return(DigitsIntegerLength(bn));
}
#endif // FOMENT_DEBUG

ulong_t BignumIntegerLength(FObject bn)
{
    FAssert(BignumP(bn));

#ifdef FOMENT_DEBUG
    ulong_t il = BignumIntegerLength(AsBignum(bn));
#endif // FOMENT_DEBUG

    FAssert(BignumSign(bn) == 0 || il == mpz_sizeinbase(AsBignum(bn)->MPInteger, 2));

    return(mpz_sizeinbase(AsBignum(bn)->MPInteger, 2));
}

static FBignum * BignumLeftShift(FBignum * bn, long_t cnt)
{
    FAssert(cnt > 0);

    long_t dcnt = cnt / UINT32_BITS;
    long_t bcnt = cnt % UINT32_BITS;

    if (bcnt == 0)
    {
        FBignum * ret = MakeBignum(bn->Used + dcnt);

        for (uint16_t idx = 0; idx < bn->Used; idx++)
            ret->Digits[idx + dcnt] = bn->Digits[idx];

        ret->Used = bn->Used + (uint16_t) dcnt;
        ret->Sign = bn->Sign;
        return(ret);
    }

    FBignum * ret = MakeBignum(bn->Used + dcnt + 1);

    ret->Digits[bn->Used + dcnt] = bn->Digits[bn->Used - 1] >> (UINT32_BITS - bcnt);
    for (uint16_t idx = 1; idx < bn->Used; idx++)
        ret->Digits[idx + dcnt] = (bn->Digits[idx] << bcnt) |
                (bn->Digits[idx - 1] >> (UINT32_BITS - bcnt));
    ret->Digits[dcnt] = bn->Digits[0] << bcnt;

    ret->Used = bn->Used + (uint16_t) dcnt;
    if (ret->Digits[ret->Used] != 0)
        ret->Used += 1;

    ret->Sign = bn->Sign;
    return(ret);
}

static FBignum * BignumRightShift(FBignum * bn, long_t cnt)
{
    FAssert(cnt > 0);

    uint16_t dcnt = (uint16_t) (cnt / UINT32_BITS);
    long_t bcnt = cnt % UINT32_BITS;

    if (bn->Used <= dcnt)
        return(MakeBignum());
    else if (bcnt == 0)
    {
        FBignum * ret = MakeBignum(bn->Used - dcnt + (bn->Sign < 0 ? 1 : 0));
        for (uint16_t idx = dcnt; idx < bn->Used; idx++)
            ret->Digits[idx - dcnt] = bn->Digits[idx];

        ret->Used = bn->Used - (uint16_t) dcnt;
        ret->Sign = bn->Sign;

        FAssert(ret->Used > 0);
        FAssert(ret->Used < 2 || ret->Digits[ret->Used - 1] != 0);
        FAssert(ret->Sign == 1 || ret->Sign == -1);

        return(ret);
    }

    FBignum * ret = MakeBignum(bn->Used - dcnt + 1);
    uint16_t idx = dcnt;
    while (idx < bn->Used - 1)
    {
        ret->Digits[idx - dcnt] = (bn->Digits[idx] >> bcnt) |
                (bn->Digits[idx + 1] << (UINT32_BITS - bcnt));
        idx += 1;
    }
    ret->Digits[idx - dcnt] = bn->Digits[idx] >> bcnt;

    ret->Sign = bn->Sign;
    ret->Used = bn->Used - (uint16_t) dcnt;
    while (ret->Used > 1)
    {
        if (ret->Digits[ret->Used - 1] != 0)
            break;
        ret->Used -= 1;
    }

    FAssert(ret->Used > 0);
    FAssert(ret->Used < 2 || ret->Digits[ret->Used - 1] != 0);
    FAssert(ret->Sign == 1 || ret->Sign == -1);

    return(ret);
}

FObject BignumArithmeticShift(FObject num, long_t cnt)
{
    FAssert(BignumP(num));

    FBignum * bn = AsBignum(num);

    FAssert(bn->Used > 0);
    FAssert(bn->Used < 2 || bn->Digits[bn->Used - 1] != 0);
    FAssert(bn->Sign == 1 || bn->Sign == -1);

    if (cnt == 0)
        return(bn);

    if (cnt < 0)
    {
        FBignum * ret = BignumRightShift(bn, - cnt);
        if (bn->Sign < 0)
            DigitsAddUInt32(ret, 0, 1);

        mpz_fdiv_q_2exp(ret->MPInteger, bn->MPInteger, (mp_bitcnt_t) - cnt);

        FAssert(ret->Used > 0);
        FAssert(ret->Used < 2 || ret->Digits[ret->Used - 1] != 0);
        FAssert(ret->Sign == 1 || ret->Sign == -1);
        FAssert(strcmp(BignumToStringC(ret, 10), NBignumToStringC(ret, 10)) == 0);

        return(ret);
    }

    FBignum * ret = BignumLeftShift(bn, cnt);
    mpz_mul_2exp(ret->MPInteger, bn->MPInteger, (mp_bitcnt_t) cnt);

    FAssert(ret->Used > 0);
    FAssert(ret->Used < 2 || ret->Digits[ret->Used - 1] != 0);
    FAssert(ret->Sign == 1 || ret->Sign == -1);
    FAssert(strcmp(BignumToStringC(ret, 10), NBignumToStringC(ret, 10)) == 0);

    return(ret);
}

#ifdef FOMENT_DEBUG
#ifdef FOMENT_UNIX
#define sprintf_s snprintf
#endif // FOMENT_UNIX
static void TestBignums()
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

    FAssert(BignumSign(MakeBignumFromLong(1)) > 0);
    FAssert(NBignumSign(MakeBignumFromLong(1)) > 0);
    FAssert(BignumSign(MakeBignumFromLong(-1)) < 0);
    FAssert(NBignumSign(MakeBignumFromLong(-1)) < 0);
    FAssert(BignumSign(MakeBignumFromLong(0)) == 0);
    FAssert(NBignumSign(MakeBignumFromLong(0)) == 0);

    FObject bn;
    bn = BignumAddLong(MakeBignumFromDouble(123456.789E10), 10);
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumAddLong(MakeBignumFromDouble(123456.789E10), -10);
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumAddLong(MakeBignumFromDouble(123456.789E10), 0xFFFFFFF);
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
#ifdef FOMENT_64BIT
    bn = BignumAddLong(MakeBignumFromDouble(123456.789E10), 0x1000000000000L);
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumAddLong(MakeBignumFromDouble(123456.789E100), 0x1000000000000L);
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
#else // FOMENT_64BIT
    bn = BignumAddLong(MakeBignumFromDouble(123456.789E10), 0x10000000);
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumAddLong(MakeBignumFromDouble(123456.789E100), 0x10000000);
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
#ifdef FOMENT_64BIT
    bn = BignumMultiply(MakeBignumFromLong(123456789012345678L),
            MakeBignumFromLong(123456789012345678L));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumMultiply(MakeBignumFromLong(-123456789012345678L),
            MakeBignumFromLong(123456789012345678L));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumMultiply(MakeBignumFromLong(123456789012345678L),
            MakeBignumFromLong(-123456789012345678L));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumMultiply(MakeBignumFromLong(-123456789012345678L),
            MakeBignumFromLong(-123456789012345678L));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumMultiply(MakeBignumFromLong(123456789012345678L),
            MakeBignumFromDouble(2828282.5789E10));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
#else // FOMENT_64BIT
    bn = BignumMultiply(MakeBignumFromLong(123456789),
            MakeBignumFromLong(123456789));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumMultiply(MakeBignumFromLong(-123456789),
            MakeBignumFromLong(123456789));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumMultiply(MakeBignumFromLong(123456789),
            MakeBignumFromLong(-123456789));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumMultiply(MakeBignumFromLong(-123456789),
            MakeBignumFromLong(-123456789));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumMultiply(MakeBignumFromLong(123456789),
            MakeBignumFromDouble(2828282.5789E10));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
#endif // FOMENT_64BIT

    FAssert(strcmp(NBignumToStringC(BignumAnd(MakeBignumFromDouble(1234567890E10),
            MakeBignumFromDouble(987654321E9)), 10), "654297430996617216") == 0);
    FAssert(strcmp(NBignumToStringC(BignumAnd(MakeBignumFromDouble(-1234567890E10),
            MakeBignumFromDouble(987654321E9)), 10), "333356890003384320") == 0);
#ifdef FOMENT_64BIT
    FAssert(strcmp(NBignumToStringC(BignumAnd(
            BignumMultiply(MakeBignumFromLong(123456789012345678L), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(123456789987654321L), MakeBignumFromDouble(7777E33))),
            10), "764365356449759452798730384693400720346950920044544") == 0);
    FAssert(strcmp(NBignumToStringC(BignumAnd(
            BignumMultiply(MakeBignumFromLong(-123456789012345678L), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(123456789987654321L), MakeBignumFromDouble(7777E33))),
            10), "959359090377537933306560371696748505119465178377748480") == 0);
    FAssert(strcmp(NBignumToStringC(BignumAnd(
            BignumMultiply(MakeBignumFromLong(123456789012345678L), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(-123456789987654321L), MakeBignumFromDouble(7777E33))),
            10), "410717112421698380931919236597841253094682326969876480") == 0);
    FAssert(strcmp(NBignumToStringC(BignumAnd(
            BignumMultiply(MakeBignumFromLong(-123456789012345678L), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(-123456789987654321L), MakeBignumFromDouble(7777E33))),
            10), "-1370840568155686073691278338679278436568011586622455808") == 0);
#else // FOMENT_64BIT
    FAssert(strcmp(NBignumToStringC(BignumAnd(
            BignumMultiply(MakeBignumFromLong(123456789), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(123456789), MakeBignumFromDouble(7777E33))),
            10), "44732543145818245773025467046032184845533184") == 0);
    FAssert(strcmp(NBignumToStringC(BignumAnd(
            BignumMultiply(MakeBignumFromLong(-123456789), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(123456789), MakeBignumFromDouble(7777E33))),
            10), "915390904907181792569333328296534201912524800") == 0);
    FAssert(strcmp(NBignumToStringC(BignumAnd(
            BignumMultiply(MakeBignumFromLong(123456789), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(-123456789), MakeBignumFromDouble(7777E33))),
            10), "366748934591181749837693680967048996457545728") == 0);
    FAssert(strcmp(NBignumToStringC(BignumAnd(
            BignumMultiply(MakeBignumFromLong(-123456789), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(-123456789), MakeBignumFromDouble(7777E33))),
            10), "-1326872382644181788180052476309615383215603712") == 0);
#endif // FOMENT_64BIT

    FAssert(strcmp(NBignumToStringC(BignumIOr(MakeBignumFromDouble(1234567890E10),
            MakeBignumFromDouble(987654321E9)), 10), "12679035790003382784") == 0);
    FAssert(strcmp(NBignumToStringC(BignumIOr(MakeBignumFromDouble(-1234567890E10),
            MakeBignumFromDouble(987654321E9)), 10), "-11691381469003384320") == 0);
#ifdef FOMENT_64BIT
    FAssert(strcmp(NBignumToStringC(BignumIOr(
            BignumMultiply(MakeBignumFromLong(123456789012345678L), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(123456789012345678L), MakeBignumFromDouble(7777E33))),
            10), "1370840486086155469332456750722651724563727392805224448") == 0);
    FAssert(strcmp(NBignumToStringC(BignumIOr(
            BignumMultiply(MakeBignumFromLong(-123456789012345678L), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(123456789012345678L), MakeBignumFromDouble(7777E33))),
            10), "-410717037937143093184097951545849737787210931770490880") == 0);
    FAssert(strcmp(NBignumToStringC(BignumIOr(
            BignumMultiply(MakeBignumFromLong(123456789012345678L), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(-123456789012345678L), MakeBignumFromDouble(7777E33))),
            10), "-959359008308007328947738783740117070748698114915303424") == 0);
    FAssert(strcmp(NBignumToStringC(BignumIOr(
            BignumMultiply(MakeBignumFromLong(-123456789012345678L), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(-123456789012345678L), MakeBignumFromDouble(7777E33))),
            10), "-764439841005047200620015436684916027818346119430144") == 0);
#else // FOMENT_64BIT
    FAssert(strcmp(NBignumToStringC(BignumIOr(
            BignumMultiply(MakeBignumFromLong(123456789), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(123456789), MakeBignumFromDouble(7777E33))),
            10), "1326872382644181788180051295717994665804300288") == 0);
    FAssert(strcmp(NBignumToStringC(BignumIOr(
            BignumMultiply(MakeBignumFromLong(-123456789), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(123456789), MakeBignumFromDouble(7777E33))),
            10), "-366748934591181749837692500375428279046242304") == 0);
    FAssert(strcmp(NBignumToStringC(BignumIOr(
            BignumMultiply(MakeBignumFromLong(123456789), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(-123456789), MakeBignumFromDouble(7777E33))),
            10), "-915390904907181792569334508888154919323828224") == 0);
    FAssert(strcmp(NBignumToStringC(BignumIOr(
            BignumMultiply(MakeBignumFromLong(-123456789), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(-123456789), MakeBignumFromDouble(7777E33))),
            10), "-44732543145818245773024286454411467434229760") == 0);
#endif // FOMENT_64BIT

    FAssert(strcmp(NBignumToStringC(BignumXOr(MakeBignumFromDouble(1234567890E10),
            MakeBignumFromDouble(987654321E9)), 10), "12024738359006765568") == 0);
    FAssert(strcmp(NBignumToStringC(BignumXOr(MakeBignumFromDouble(-1234567890E10),
            MakeBignumFromDouble(987654321E9)), 10), "-12024738359006768640") == 0);
#ifdef FOMENT_64BIT
    FAssert(strcmp(NBignumToStringC(BignumXOr(
            BignumMultiply(MakeBignumFromLong(123456789012345678L), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(123456789987654321L), MakeBignumFromDouble(7777E33))),
            10), "1370076202799236314238479608294585035847664635702411264") == 0);
    FAssert(strcmp(NBignumToStringC(BignumXOr(
            BignumMultiply(MakeBignumFromLong(-123456789012345678L), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(123456789987654321L), MakeBignumFromDouble(7777E33))),
            10), "-1370076202799236314238479608294589758214147505347624960") == 0);
    FAssert(strcmp(NBignumToStringC(BignumXOr(
            BignumMultiply(MakeBignumFromLong(123456789012345678L), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(-123456789987654321L), MakeBignumFromDouble(7777E33))),
            10), "-1370076202799236314238479608294589758214147505347624960") == 0);
    FAssert(strcmp(NBignumToStringC(BignumXOr(
            BignumMultiply(MakeBignumFromLong(-123456789012345678L), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(-123456789987654321L), MakeBignumFromDouble(7777E33))),
            10), "1370076202799236314238479608294585035847664635702411264") == 0);
#else // FOMENT_64BIT
    FAssert(strcmp(NBignumToStringC(BignumXOr(
            BignumMultiply(MakeBignumFromLong(123456789), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(123456789), MakeBignumFromDouble(7777E33))),
            10), "1282139839498363542407025828671962480958767104") == 0);
    FAssert(strcmp(NBignumToStringC(BignumXOr(
            BignumMultiply(MakeBignumFromLong(-123456789), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(123456789), MakeBignumFromDouble(7777E33))),
            10), "-1282139839498363542407025828671962480958767104") == 0);
    FAssert(strcmp(NBignumToStringC(BignumXOr(
            BignumMultiply(MakeBignumFromLong(123456789), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(-123456789), MakeBignumFromDouble(7777E33))),
            10), "-1282139839498363542407028189855203915781373952") == 0);
    FAssert(strcmp(NBignumToStringC(BignumXOr(
            BignumMultiply(MakeBignumFromLong(-123456789), MakeBignumFromDouble(3333E33)),
            BignumMultiply(MakeBignumFromLong(-123456789), MakeBignumFromDouble(7777E33))),
            10), "1282139839498363542407028189855203915781373952") == 0);
#endif // FOMENT_64BIT

#ifdef FOMENT_64BIT
    FAssert(strcmp(NBignumToStringC(BignumNot(MakeBignumFromLong(123456789012345678L)), 10),
            "-123456789012345679") == 0);
    FAssert(strcmp(NBignumToStringC(BignumNot(MakeBignumFromLong(-123456789012345678L)), 10),
            "123456789012345677") == 0);
    FAssert(strcmp(NBignumToStringC(BignumNot(MakeBignumFromLong(0xFFFFFFFFFFFFFFFFL)), 10),
            "0") == 0);
#else // FOMENT_64BIT
    FAssert(strcmp(NBignumToStringC(BignumNot(MakeBignumFromLong(123456789)), 10),
            "-123456790") == 0);
    FAssert(strcmp(NBignumToStringC(BignumNot(MakeBignumFromLong(-123456789)), 10),
            "123456788") == 0);
    FAssert(strcmp(NBignumToStringC(BignumNot(MakeBignumFromLong(0xFFFFFFFF)), 10),
            "0") == 0);
#endif // FOMENT_64BIT
    FAssert(strcmp(NBignumToStringC(BignumNot(MakeBignumFromLong(0)), 10), "-1") == 0);
    FAssert(strcmp(NBignumToStringC(BignumNot(MakeBignumFromLong(-1)), 10), "0") == 0);
    FAssert(strcmp(NBignumToStringC(BignumNot(MakeBignumFromDouble(987654321E23)), 10),
            "-98765432099999994374931018153985") == 0);
    FAssert(strcmp(NBignumToStringC(BignumNot(MakeBignumFromDouble(-987654321E23)), 10),
            "98765432099999994374931018153983") == 0);

#ifdef FOMENT_64BIT
    FAssert(BignumBitCount(
            BignumMultiply(MakeBignumFromLong(123456789012345678L), MakeBignumFromDouble(3333E33)))
            == 51);
    FAssert(BignumBitCount(
            BignumMultiply(MakeBignumFromLong(123456789987654321L), MakeBignumFromDouble(7777E33)))
            == 55);
    FAssert(BignumBitCount(
            BignumMultiply(MakeBignumFromLong(-123456789012345678L), MakeBignumFromDouble(3333E33)))
            == 121);
    FAssert(BignumBitCount(
            BignumMultiply(MakeBignumFromLong(-123456789987654321L), MakeBignumFromDouble(7777E33)))
            == 125);
#else // FOMENT_64BIT
    FAssert(BignumBitCount(
            BignumMultiply(MakeBignumFromLong(123456789), MakeBignumFromDouble(3333E33)))
            == 38);
    FAssert(BignumBitCount(
            BignumMultiply(MakeBignumFromLong(123456789), MakeBignumFromDouble(7777E33)))
            == 37);
    FAssert(BignumBitCount(
            BignumMultiply(MakeBignumFromLong(-123456789), MakeBignumFromDouble(3333E33)))
            == 107);
    FAssert(BignumBitCount(
            BignumMultiply(MakeBignumFromLong(-123456789), MakeBignumFromDouble(7777E33)))
            == 107);
#endif // FOMENT_64BIT

//    FAssert(BignumIntegerLength(MakeBignumFromLong(0)) == 0);
    FAssert(BignumIntegerLength(MakeBignumFromLong(1)) == 1);
#ifdef FOMENT_64BIT
    FAssert(BignumIntegerLength(MakeBignumFromLong(123456789987654321L)) == 57);
    FAssert(BignumIntegerLength(MakeBignumFromLong(-123456789987654321L)) == 57);
#else // FOMENT_64BIT
    FAssert(BignumIntegerLength(MakeBignumFromLong(123456789)) == 27);
    FAssert(BignumIntegerLength(MakeBignumFromLong(-123456789)) == 27);
#endif // FOMENT_64BIT
    FAssert(BignumIntegerLength(MakeBignumFromDouble(666E66)) == 229);
    FAssert(BignumIntegerLength(MakeBignumFromDouble(-666E66)) == 229);

#ifdef FOMENT_64BIT
    FAssert(strcmp(NBignumToStringC(
            BignumArithmeticShift(MakeBignumFromLong(123456789987654321L), 123), 10),
            "1312817772170632175479824592261381132821730046842503168") == 0);
    FAssert(strcmp(NBignumToStringC(
            BignumArithmeticShift(MakeBignumFromLong(123456789987654321L), 128), 10),
            "42010168709460229615354386952364196250295361498960101376") == 0);
    FAssert(strcmp(NBignumToStringC(
            BignumArithmeticShift(MakeBignumFromLong(-123456789987654321L), 123), 10),
            "-1312817772170632175479824592261381132821730046842503168") == 0);
    FAssert(strcmp(NBignumToStringC(
            BignumArithmeticShift(MakeBignumFromLong(-123456789987654321L), 128), 10),
            "-42010168709460229615354386952364196250295361498960101376") == 0);
    FAssert(strcmp(NBignumToStringC(
            BignumArithmeticShift(MakeBignumFromLong(0x5555555555555555L), 64), 16),
            "55555555555555550000000000000000") == 0);
    FAssert(strcmp(NBignumToStringC(
            BignumArithmeticShift(MakeBignumFromLong(0x5555555555555555L), 61), 16),
            "aaaaaaaaaaaaaaaa000000000000000") == 0);
    FAssert(strcmp(NBignumToStringC(
            BignumArithmeticShift(MakeBignumFromLong(0x5555555555555555L), 66), 16),
            "155555555555555540000000000000000") == 0);
    FAssert(strcmp(NBignumToStringC(
            BignumArithmeticShift(MakeBignumFromLong(-0x5555555555555555L), 64), 16),
            "-55555555555555550000000000000000") == 0);
    FAssert(strcmp(NBignumToStringC(
            BignumArithmeticShift(MakeBignumFromLong(-0x5555555555555555L), 61), 16),
            "-aaaaaaaaaaaaaaaa000000000000000") == 0);
    FAssert(strcmp(NBignumToStringC(
            BignumArithmeticShift(MakeBignumFromLong(-0x5555555555555555L), 66), 16),
            "-155555555555555540000000000000000") == 0);
#else // FOMENT_64BIT
    FAssert(strcmp(NBignumToStringC(
            BignumArithmeticShift(MakeBignumFromLong(123456789), 123), 10),
            "1312817761668089986430689004301926249332211712") == 0);
    FAssert(strcmp(NBignumToStringC(
            BignumArithmeticShift(MakeBignumFromLong(123456789), 128), 10),
            "42010168373378879565782048137661639978630774784") == 0);
    FAssert(strcmp(NBignumToStringC(
            BignumArithmeticShift(MakeBignumFromLong(-123456789), 123), 10),
            "-1312817761668089986430689004301926249332211712") == 0);
    FAssert(strcmp(NBignumToStringC(
            BignumArithmeticShift(MakeBignumFromLong(-123456789), 128), 10),
            "-42010168373378879565782048137661639978630774784") == 0);
    FAssert(strcmp(NBignumToStringC(
            BignumArithmeticShift(MakeBignumFromLong(0x55555555), 64), 16),
            "555555550000000000000000") == 0);
    FAssert(strcmp(NBignumToStringC(
            BignumArithmeticShift(MakeBignumFromLong(0x55555555), 61), 16),
            "aaaaaaaa000000000000000") == 0);
    FAssert(strcmp(NBignumToStringC(
            BignumArithmeticShift(MakeBignumFromLong(0x55555555), 66), 16),
            "1555555540000000000000000") == 0);
    FAssert(strcmp(NBignumToStringC(
            BignumArithmeticShift(MakeBignumFromLong(-0x55555555), 64), 16),
            "-555555550000000000000000") == 0);
    FAssert(strcmp(NBignumToStringC(
            BignumArithmeticShift(MakeBignumFromLong(-0x55555555), 61), 16),
            "-aaaaaaaa000000000000000") == 0);
    FAssert(strcmp(NBignumToStringC(
            BignumArithmeticShift(MakeBignumFromLong(-0x55555555), 66), 16),
            "-1555555540000000000000000") == 0);
#endif // FOMENT_64BIT

    FAssert(strcmp(NBignumToStringC(
            BignumArithmeticShift(MakeBignumFromDouble(123456789E123), -128), 10),
            "362806895100397833620635889672491320190768919845254931617368881349570157396831979594644979712") == 0);
    FAssert(strcmp(NBignumToStringC(
            BignumArithmeticShift(MakeBignumFromDouble(123456789E123), -127), 10),
            "725613790200795667241271779344982640381537839690509863234737762699140314793663959189289959424") == 0);
    FAssert(strcmp(NBignumToStringC(
            BignumArithmeticShift(MakeBignumFromDouble(123456789E123), -130), 10),
            "90701723775099458405158972418122830047692229961313732904342220337392539349207994898661244928") == 0);
    FAssert(strcmp(NBignumToStringC(
            BignumArithmeticShift(MakeBignumFromDouble(-123456789E45), -128), 10),
            "-362806895100398") == 0);
    FAssert(strcmp(NBignumToStringC(
            BignumArithmeticShift(MakeBignumFromDouble(-123456789E45), -129), 10),
            "-181403447550199") == 0);
    FAssert(strcmp(NBignumToStringC(
            BignumArithmeticShift(MakeBignumFromDouble(-123456789E45), -126), 10),
            "-1451227580401592") == 0);

    FAssert(BignumOddP(MakeBignumFromLong(123456789)));
    FAssert(BignumOddP(MakeBignumFromLong(12345678)) == 0);

    bn = BignumDivide(MakeBignumFromLong(123456789123456L), MakeBignumFromLong(123));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumDivide(MakeBignumFromDouble(123456789E234), MakeBignumFromLong(12345678));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);

    bn = BignumRemainder(MakeBignumFromLong(123456789123456L), MakeBignumFromLong(123));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumRemainder(MakeBignumFromDouble(123456789E234), MakeBignumFromLong(12345678));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);

    bn = BignumDivide(MakeBignumFromLong(-12345678), MakeBignumFromLong(123));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumRemainder(MakeBignumFromLong(-12345678), MakeBignumFromLong(123));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);

    bn = BignumDivide(MakeBignumFromLong(12345678), MakeBignumFromLong(-123));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumRemainder(MakeBignumFromLong(12345678), MakeBignumFromLong(-123));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);

    bn = BignumDivide(MakeBignumFromLong(-12345678), MakeBignumFromLong(-123));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumRemainder(MakeBignumFromLong(-12345678), MakeBignumFromLong(-123));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);

    bn = BignumDivide(MakeBignumFromLong(350032021), MakeBignumFromLong(118300067));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    bn = BignumRemainder(MakeBignumFromLong(350032021), MakeBignumFromLong(118300067));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);

    FObject rem;
    bn = BignumSqrt(&rem, MakeBignumFromDouble(12345678E123));
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    FAssert(strcmp(BignumToStringC(rem, 10), NBignumToStringC(rem, 10)) == 0);
    bn = BignumSqrt(&rem,
            BignumAdd(BignumMultiply(MakeBignumFromLong(12345678), MakeBignumFromLong(12345678)),
                    MakeBignumFromLong(98765)));
    FAssert(strcmp(NBignumToStringC(bn, 10), "12345678") == 0);
    FAssert(strcmp(NBignumToStringC(rem, 10), "98765") == 0);
}
#endif // FOMENT_DEBUG

void SetupBignums()
{
    RegisterRoot(&MaximumDoubleBignum, "maximum-double-bignum");
    MaximumDoubleBignum = MakeIntegerFromUInt64(0x0010000000000000ULL); // 2 ^ 52

    FAssert(BignumP(MaximumDoubleBignum));

#ifdef FOMENT_DEBUG
    TestBignums();
#endif // FOMENT_DEBUG
}
