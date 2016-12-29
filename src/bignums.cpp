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

#define AsBignum(obj) ((FBignum *) (obj))->MPInteger
#define AsNBignum(obj) ((FBignum *) (obj))

#ifdef FOMENT_32BIT
#define MAXIMUM_DIGIT_COUNT (((ulong_t) 1 << sizeof(uint16_t) * 8) - 1)
#endif // FOMENT_32BIT
#ifdef FOMENT_64BIT
#define MAXIMUM_DIGIT_COUNT (((ulong_t) 1 << sizeof(uint32_t) * 8) - 1)
#endif // FOMENT_64BIT

#define HALF_BITS (sizeof(ulong_t) * 4)
#define LO_MASK (((ulong_t) 1 << HALF_BITS) - 1)
#define HI_HALF(digit) (((digit) >> HALF_BITS) & LO_MASK)
#define LO_HALF(digit) ((digit) & LO_MASK)

typedef struct
{
    mpz_t MPInteger;
    int8_t Sign;
#ifdef FOMENT_32BIT
    int8_t Pad;
    uint16_t Used;
#endif // FOMENT_32BIT
#ifdef FOMENT_64BIT
    int8_t Pad[3];
    uint32_t Used;
#endif // FOMENT_64BIT
    ulong_t Digits[1];
} FBignum;

static char * NBignumToStringC(FObject bn, long_t rdx);
static inline ulong_t DigitCount(FObject bn)
{
    FAssert(BignumP(bn));

    return((ByteLength(bn) - (sizeof(FBignum) - sizeof(ulong_t))) / sizeof(ulong_t));
}

static inline void SetDigitsUsed(FBignum * bn)
{
    bn->Used = DigitCount(bn);
    while (bn->Used > 0)
    {
        if (bn->Digits[bn->Used - 1] != 0)
            break;
        bn->Used -= 1;
    }
}

static void BignumAddFixnum(FObject rbn, FObject bn, long_t n);
static void BignumMultiplyFixnum(FObject rbn, FObject bn, long_t n);

static FBignum * MakeNBignum(ulong_t bc) // Change to MakeBignum
{
//    FAssert(bc > 0);
    FAssert(bc < MAXIMUM_DIGIT_COUNT);

    FBignum * bn = (FBignum *) MakeObject(BignumTag, sizeof(FBignum) + (bc - 1) * sizeof(ulong_t),
            0, "%make-bignum");
    mpz_init(bn->MPInteger);
    InstallGuardian(bn, CleanupTConc);

    bn->Sign = 0;
    bn->Used = 0;
    memset(bn->Digits, 0, bc * sizeof(ulong_t));

    FAssert(DigitCount(bn) == bc);

    return(bn);
}

FObject MakeBignumFromFixnum(long_t n)
{
    FAssert(sizeof(long_t) == sizeof(ulong_t));

    FBignum * bn = MakeNBignum(1);
    mpz_init_set_si(bn->MPInteger, (long) n);
//    InstallGuardian(bn, CleanupTConc);

    if (n >= 0)
        bn->Sign = 1;
    else
    {
        bn->Sign = -1;
        n = -n;
    }

    bn->Digits[0] = n;
    SetDigitsUsed(bn);

    FAssert(bn->Used == 1 || bn->Digits[0] == 0);
    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);

    return(bn);
}

FObject MakeBignumFromDouble(double64_t d)
{
    FBignum * bn = MakeNBignum(0);
    mpz_init_set_d(bn->MPInteger, d);
//    InstallGuardian(bn, CleanupTConc);

    return(bn);
}

FObject CopyBignum(FObject n)
{
    FAssert(BignumP(n));

    FBignum * bn = MakeNBignum(AsNBignum(n)->Used);
    mpz_init_set(bn->MPInteger, AsBignum(n));
//    InstallGuardian(bn, CleanupTConc);

    bn->Sign = AsNBignum(n)->Sign;
    memcpy(bn->Digits, AsNBignum(n)->Digits, AsNBignum(n)->Used * sizeof(ulong_t));
    SetDigitsUsed(bn);

    return(bn);
}

void DeleteBignum(FObject obj)
{
    FAssert(BignumP(obj));

    mpz_clear(AsBignum(obj));
}

FObject ToBignum(FObject obj)
{
    if (FixnumP(obj))
        return(MakeBignumFromFixnum(AsFixnum(obj)));
    else if (FlonumP(obj))
        return(MakeBignumFromDouble(AsFlonum(obj)));

    FAssert(BignumP(obj));

    return(obj);
}

FObject Normalize(FObject num)
{
    if (BignumP(num) && mpz_cmp_si(AsBignum(num), (long) MINIMUM_FIXNUM) >= 0
            && mpz_cmp_si(AsBignum(num), (long) MAXIMUM_FIXNUM) <= 0)
        return(MakeFixnum(mpz_get_si(AsBignum(num))));

    return(num);
}

FObject MakeInteger(int64_t n)
{
    if (n >= MINIMUM_FIXNUM && n <= MAXIMUM_FIXNUM)
        return(MakeFixnum(n));

    if (n > 0)
        return(MakeInteger(n >> 32, n & 0xFFFFFFFF));

    n = -n;
    FObject bn = MakeInteger(n >> 32, n & 0xFFFFFFFF);
    FAssert(BignumP(bn));
    mpz_mul_si(AsBignum(bn), AsBignum(bn), -1);
    AsNBignum(bn)->Sign = -1;
    return(bn);
}

FObject MakeIntegerU(uint64_t n)
{
    if (n <= MAXIMUM_FIXNUM)
        return(MakeFixnum(n));

    return(MakeInteger(n >> 32, n & 0xFFFFFFFF));
}

FObject MakeInteger(uint32_t high, uint32_t low)
{
    if (high == 0 && low <= MAXIMUM_FIXNUM)
        return(MakeFixnum(low));

    FObject bn = MakeNBignum(1);
    AsNBignum(bn)->Digits[0] = (((uint64_t) high) << 32) | ((uint64_t) low);
    SetDigitsUsed(AsNBignum(bn));

    mpz_set_ui(AsBignum(bn), high);
    mpz_mul_2exp(AsBignum(bn), AsBignum(bn), 32);
    mpz_add_ui(AsBignum(bn), AsBignum(bn), low);

    FAssert(strcmp(BignumToStringC(bn, 10), NBignumToStringC(bn, 10)) == 0);
    return(bn);
}

double64_t BignumToDouble(FObject bn)
{
    FAssert(BignumP(bn));

    return(mpz_get_d(AsBignum(bn)));
}

/*
Destructively divide bn by digit; the quotient is left in bn and the remainder is returned;
hdigit must fit in half a ulong_t. The quotient is not normalized.
*/
static ulong_t BignumHDigitDivide(FBignum * bn, ulong_t hdigit)
{
    FAssert(hdigit <= ((ulong_t) 1 << HALF_BITS) - 1);

    ulong_t q0 = 0;
    ulong_t r0 = 0;
    ulong_t q1, r1;

    for (ulong_t idx = bn->Used - 1; idx > 0; idx--)
    {
        q1 = bn->Digits[idx] / hdigit + q0;
        r1 = ((bn->Digits[idx] % hdigit) << HALF_BITS) + HI_HALF(bn->Digits[idx - 1]);
        q0 = ((r1 / hdigit) << HALF_BITS);
        r0 = r1 % hdigit;
        bn->Digits[idx] = q1;
        bn->Digits[idx - 1] = (r0 << HALF_BITS) + LO_HALF(bn->Digits[idx - 1]);
    }

    q1 = bn->Digits[0] / hdigit + q0;
    r1 = bn->Digits[0] % hdigit;
    bn->Digits[0] = q1;

    return(r1);
}

char * BignumToStringC(FObject bn, long_t rdx)
{
    return(mpz_get_str(0, (int) rdx, AsBignum(bn)));
}

static const char DigitTable[] = "0123456789abcdefghijklmnopqrstuvwxyz";

static char * NBignumToStringC(FObject bn, long_t rdx)
{
    if (AsNBignum(bn)->Used == 0)
        return(strdup("0"));

    FObject obj = CopyBignum(bn);

    FAssert(BignumP(obj));

    FBignum * tbn = AsNBignum(obj);
    char * ret = (char *) malloc(tbn->Used * sizeof(ulong_t) * 8 + 2);
    if (ret == 0)
        return(0);
    char * s = ret;

    while (tbn->Used > 0)
    {
        ulong_t dgt = BignumHDigitDivide(tbn, rdx);

        FAssert(dgt >= 0 && dgt < rdx);

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

static inline long_t TensDigit(double64_t n)
{
    return((long_t) (n - (Truncate(n / 10.0) * 10.0)));
}

FObject ToExactRatio(double64_t d)
{
    FObject whl = MakeBignumFromDouble(Truncate(d));
    FObject rbn = MakeBignumFromFixnum((long_t) 0);
    FObject scl = MakeFixnum(1);
    long_t sgn = (d < 0 ? -1 : 1);
    d = fabs(d - Truncate(d));

    for (long_t idx = 0; d != Truncate(d) && idx < 14; idx++)
    {
        BignumMultiplyFixnum(rbn, rbn, 10);
        d *= 10;
        BignumAddFixnum(rbn, rbn, TensDigit(d));
        d = d - Truncate(d);
        scl = GenericMultiply(scl, MakeFixnum(10));
    }

    BignumMultiplyFixnum(rbn, rbn, sgn);

    return(GenericAdd(MakeRatio(rbn, scl), whl));
}

long_t ParseBignum(FCh * s, long_t sl, long_t sdx, long_t rdx, long_t sgn, long_t n,
    FObject * punt)
{
    FAssert(n > 0);

    FObject bn = MakeBignumFromFixnum(n);

    if (rdx == 16)
    {
        for (n = 0; sdx < sl; sdx++)
        {
            long_t dv = DigitValue(s[sdx]);

            if (dv < 0 || dv > 9)
            {
                if (s[sdx] >= 'a' && s[sdx] <= 'f')
                    dv = s[sdx] - 'a' + 10;
                else if (s[sdx] >= 'A' && s[sdx] <= 'F')
                    dv = s[sdx] - 'A' + 10;
                else
                    break;
            }

            BignumMultiplyFixnum(bn, bn, rdx);
            BignumAddFixnum(bn, bn, dv);
        }
    }
    else
    {
        FAssert(rdx == 2 || rdx == 8 || rdx == 10);

        for (n = 0; sdx < sl; sdx++)
        {
            long_t dv = DigitValue(s[sdx]);
            if (dv >= 0 && dv < rdx)
            {
                BignumMultiplyFixnum(bn, bn, rdx);
                BignumAddFixnum(bn, bn, dv);
            }
            else
                break;
        }
    }

    BignumMultiplyFixnum(bn, bn, sgn);

    *punt = bn;
    return(sdx);
}

long_t BignumCompare(FObject bn1, FObject bn2)
{
    FAssert(BignumP(bn1));
    FAssert(BignumP(bn2));

    return(mpz_cmp(AsBignum(bn1), AsBignum(bn2)));
}

long_t BignumCompareFixnum(FObject bn, long_t n)
{
    FAssert(BignumP(bn));

    return(mpz_cmp_si(AsBignum(bn), n));
}

long_t BignumSign(FObject bn)
{
    FAssert(BignumP(bn));

    return(mpz_sgn(AsBignum(bn)));
}

FObject BignumAdd(FObject bn1, FObject bn2)
{
    FAssert(BignumP(bn1));
    FAssert(BignumP(bn2));

    FObject ret = MakeNBignum(0);
    mpz_add(AsBignum(ret), AsBignum(bn1), AsBignum(bn2));
    return(ret);
}

static void BignumAddFixnum(FObject rbn, FObject bn, long_t n)
{
    FAssert(BignumP(rbn));
    FAssert(BignumP(bn));

    if (n > 0)
        mpz_add_ui(AsBignum(rbn), AsBignum(bn), (unsigned long) n);
    else
        mpz_sub_ui(AsBignum(rbn), AsBignum(bn), (unsigned long) (- n));
}

FObject BignumAddFixnum(FObject bn, long_t n)
{
    FAssert(BignumP(bn));

    FObject ret = MakeNBignum(0);
    if (n > 0)
        mpz_add_ui(AsBignum(ret), AsBignum(bn), (unsigned long) n);
    else
        mpz_sub_ui(AsBignum(ret), AsBignum(bn), (unsigned long) (- n));
    return(ret);
}

FObject BignumMultiply(FObject bn1, FObject bn2)
{
    FAssert(BignumP(bn1));
    FAssert(BignumP(bn2));

    FObject ret = MakeNBignum(0);
    mpz_mul(AsBignum(ret), AsBignum(bn1), AsBignum(bn2));
    return(ret);
}

static void BignumMultiplyFixnum(FObject rbn, FObject bn, long_t n)
{
    FAssert(BignumP(rbn));
    FAssert(BignumP(bn));

    mpz_mul_si(AsBignum(rbn), AsBignum(bn), (long) n);
}

FObject BignumMultiplyFixnum(FObject bn, long_t n)
{
    FAssert(BignumP(bn));

    FObject ret = MakeNBignum(0);
    mpz_mul_si(AsBignum(ret), AsBignum(bn), (long) n);
    return(ret);
}

FObject BignumSubtract(FObject bn1, FObject bn2)
{
    FAssert(BignumP(bn1));
    FAssert(BignumP(bn2));

    FObject ret = MakeNBignum(0);
    mpz_sub(AsBignum(ret), AsBignum(bn1), AsBignum(bn2));
    return(ret);
}

FObject BignumDivide(FObject n, FObject d)
{
    FAssert(BignumP(n));
    FAssert(BignumP(d));

    FObject ret = MakeNBignum(0);
    mpz_tdiv_q(AsBignum(ret), AsBignum(n), AsBignum(d));
    return(ret);
}

FObject BignumRemainder(FObject n, FObject d)
{
    FAssert(BignumP(n));
    FAssert(BignumP(d));

    FObject ret = MakeNBignum(0);
    mpz_tdiv_r(AsBignum(ret), AsBignum(n), AsBignum(d));
    return(ret);
}

long_t BignumRemainderFixnum(FObject n, long_t d)
{
    FAssert(BignumP(n));
    FAssert(d >= 0);

    return(mpz_tdiv_ui(AsBignum(n), (unsigned long) d));
}

long_t BignumEqualFixnum(FObject bn, long_t n)
{
    FAssert(BignumP(bn));

    return(mpz_cmp_si(AsBignum(bn), (long) n) == 0);
}

FObject BignumExpt(FObject bn, long_t e)
{
    FAssert(BignumP(bn));
    FAssert(e >= 0);

    FObject ret = MakeNBignum(0);
    mpz_pow_ui(AsBignum(ret), AsBignum(bn), (unsigned long) e);
    return(ret);
}

FObject BignumSqrt(FObject * rem, FObject bn)
{
    FAssert(BignumP(bn));

    FObject ret = MakeNBignum(0);
    *rem = MakeNBignum(0);
    mpz_sqrtrem(AsBignum(ret), AsBignum(*rem), AsBignum(bn));
    return(ret);
}

FObject BignumAnd(FObject bn1, FObject bn2)
{
    FAssert(BignumP(bn1));
    FAssert(BignumP(bn2));

    FObject ret = MakeNBignum(0);
    mpz_and(AsBignum(ret), AsBignum(bn1), AsBignum(bn2));
    return(ret);
}

FObject BignumIOr(FObject bn1, FObject bn2)
{
    FAssert(BignumP(bn1));
    FAssert(BignumP(bn2));

    FObject ret = MakeNBignum(0);
    mpz_ior(AsBignum(ret), AsBignum(bn1), AsBignum(bn2));
    return(ret);
}

FObject BignumXOr(FObject bn1, FObject bn2)
{
    FAssert(BignumP(bn1));
    FAssert(BignumP(bn2));

    FObject ret = MakeNBignum(0);
    mpz_xor(AsBignum(ret), AsBignum(bn1), AsBignum(bn2));
    return(ret);
}

FObject BignumNot(FObject bn)
{
    FAssert(BignumP(bn));

    FObject ret = MakeNBignum(0);
    mpz_com(AsBignum(ret), AsBignum(bn));
    return(ret);
}

ulong_t BignumBitCount(FObject bn)
{
    FAssert(BignumP(bn));

    return(mpz_popcount(AsBignum(bn)));
}

ulong_t BignumIntegerLength(FObject bn)
{
    FAssert(BignumP(bn));

    return(mpz_sizeinbase(AsBignum(bn), 2));
}

ulong_t BignumFirstSetBit(FObject bn)
{
    FAssert(BignumP(bn));

    return(mpz_scan1(AsBignum(bn), 0));
}

FObject BignumArithmeticShift(FObject bn, long_t cnt)
{
    FAssert(BignumP(bn));

    if (cnt == 0)
        return(bn);

    if (cnt < 0)
    {
        FObject rbn = MakeNBignum(0);
        mpz_fdiv_q_2exp(AsBignum(rbn), AsBignum(bn), (mp_bitcnt_t) - cnt);
        return(Normalize(rbn));
    }

    FObject rbn = MakeNBignum(0);
    mpz_mul_2exp(AsBignum(rbn), AsBignum(bn), (mp_bitcnt_t) cnt);
    return(rbn);
}
