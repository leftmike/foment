/*

Foment

z: complex
x: real
n: integer
q: rational

*/

#include <math.h>
#include <float.h>
#include <stdio.h>
#include <string.h>
#include "foment.hpp"
#include "unicode.hpp"
#include "numbers.hpp"

#ifdef FOMENT_BSD
#include <stdlib.h>
#else // FOMENT_BSD
#include <malloc.h>
#endif // FOMENT_BSD

#ifdef FOMENT_WINDOWS
#define isfinite _finite
#define isnan _isnan
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
#define sprintf_s snprintf
#ifndef isfinite
#define isfinite finite
#endif // isfinite
#endif // FOMENT_UNIX

static int_t GenericCompare(const char * who, FObject x1, FObject x2, int_t cf);
static int_t GenericSign(FObject x);
static FObject GenericAdd(FObject z1, FObject z2);
static FObject GenericMultiply(FObject z1, FObject z2);
static FObject GenericSubtract(FObject z1, FObject z2);
static FObject GenericDivide(FObject z1, FObject z2);

static inline double64_t Truncate(double64_t n)
{
#ifdef FOMENT_WINDOWS
    return(((n) < 0) ? ceil((n)) : floor((n)));
#else // FOMENT_WINDOWS
    return(trunc(n));
#endif // FOMENT_WINDOWS

//    return(floor(n + 0.5 * ((n < 0 ) ? 1 : 0)));
}

int_t IntegerP(FObject obj)
{
    if (FlonumP(obj))
        return(isfinite(AsFlonum(obj)) && AsFlonum(obj) == Truncate(AsFlonum(obj)));

    return(FixnumP(obj) || BignumP(obj));
}

int_t NonNegativeExactIntegerP(FObject obj, int_t bf)
{
    return((FixnumP(obj) && AsFixnum(obj) >= 0) || (bf && BignumP(obj) && GenericSign(obj) >= 0));
}

static FObject MakeBignum()
{
    FBignum * bn = (FBignum *) MakeObject(sizeof(FBignum), BignumTag);
    bn->Reserved = BignumTag;
    mpz_init(bn->MPInteger);

    InstallGuardian(bn, R.CleanupTConc);

    return(bn);
}

static FObject MakeBignum(FFixnum n)
{
    FBignum * bn = (FBignum *) MakeObject(sizeof(FBignum), BignumTag);
    bn->Reserved = BignumTag;
    mpz_init_set_si(bn->MPInteger, (long) n);

    InstallGuardian(bn, R.CleanupTConc);

    return(bn);
}

static FObject MakeBignum(double64_t d)
{
    FBignum * bn = (FBignum *) MakeObject(sizeof(FBignum), BignumTag);
    bn->Reserved = BignumTag;
    mpz_init_set_d(bn->MPInteger, d);

    InstallGuardian(bn, R.CleanupTConc);

    return(bn);
}

static FObject MakeBignum(FObject n)
{
    FAssert(BignumP(n));

    FBignum * bn = (FBignum *) MakeObject(sizeof(FBignum), BignumTag);
    bn->Reserved = BignumTag;
    mpz_init_set(bn->MPInteger, AsBignum(n));

    InstallGuardian(bn, R.CleanupTConc);

    return(bn);
}

void DeleteBignum(FObject obj)
{
    FAssert(BignumP(obj));

    mpz_clear(AsBignum(obj));
}

static FObject ToBignum(FObject obj)
{
    if (FixnumP(obj))
        return(MakeBignum(AsFixnum(obj)));
    else if (FlonumP(obj))
        return(MakeBignum(AsFlonum(obj)));

    FAssert(BignumP(obj));

    return(obj);
}

inline static FObject Normalize(FObject num)
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

    FObject bn = MakeBignum();
    mpz_set_ui(AsBignum(bn), high);
    mpz_mul_2exp(AsBignum(bn), AsBignum(bn), 32);
    mpz_add_ui(AsBignum(bn), AsBignum(bn), low);
    return(bn);
}

inline static double64_t BignumToDouble(FObject bn)
{
    FAssert(BignumP(bn));

    return(mpz_get_d(AsBignum(bn)));
}

inline static int_t BignumCompare(FObject bn1, FObject bn2)
{
    FAssert(BignumP(bn1));
    FAssert(BignumP(bn2));

    return(mpz_cmp(AsBignum(bn1), AsBignum(bn2)));
}

inline static int_t BignumSign(FObject bn)
{
    FAssert(BignumP(bn));

    return(mpz_sgn(AsBignum(bn)));
}

inline static void BignumAdd(FObject rbn, FObject bn1, FObject bn2)
{
    FAssert(BignumP(rbn));
    FAssert(BignumP(bn1));
    FAssert(BignumP(bn2));

    mpz_add(AsBignum(rbn), AsBignum(bn1), AsBignum(bn2));
}

inline static void BignumAddFixnum(FObject rbn, FObject bn, FFixnum n)
{
    FAssert(BignumP(rbn));
    FAssert(BignumP(bn));

    if (n > 0)
        mpz_add_ui(AsBignum(rbn), AsBignum(bn), (unsigned long) n);
    else
        mpz_sub_ui(AsBignum(rbn), AsBignum(bn), (unsigned long) (- n));
}

inline static void BignumMultipy(FObject rbn, FObject bn1, FObject bn2)
{
    FAssert(BignumP(rbn));
    FAssert(BignumP(bn1));
    FAssert(BignumP(bn2));

    mpz_mul(AsBignum(rbn), AsBignum(bn1), AsBignum(bn2));
}

inline static void BignumMultiplyFixnum(FObject rbn, FObject bn, FFixnum n)
{
    FAssert(BignumP(rbn));
    FAssert(BignumP(bn));

    mpz_mul_si(AsBignum(rbn), AsBignum(bn), (long) n);
}

inline static void BignumSubtract(FObject rbn, FObject bn1, FObject bn2)
{
    FAssert(BignumP(rbn));
    FAssert(BignumP(bn1));
    FAssert(BignumP(bn2));

    mpz_sub(AsBignum(rbn), AsBignum(bn1), AsBignum(bn2));
}

inline static void BignumDivide(FObject rbn, FObject n, FObject d)
{
    FAssert(BignumP(rbn));
    FAssert(BignumP(n));
    FAssert(BignumP(d));

    mpz_tdiv_q(AsBignum(rbn), AsBignum(n), AsBignum(d));
}

inline static void BignumRemainder(FObject rbn, FObject n, FObject d)
{
    FAssert(BignumP(rbn));
    FAssert(BignumP(n));
    FAssert(BignumP(d));

    mpz_tdiv_r(AsBignum(rbn), AsBignum(n), AsBignum(d));
}

inline static FFixnum BignumRemainderFixnum(FObject n, FFixnum d)
{
    FAssert(BignumP(n));
    FAssert(d >= 0);

    return(mpz_tdiv_ui(AsBignum(n), (unsigned long) d));
}

inline static int_t BignumEqualFixnum(FObject bn, FFixnum n)
{
    FAssert(BignumP(bn));

    return(mpz_cmp_si(AsBignum(bn), (long) n) == 0);
}

inline static void BignumExpt(FObject rbn, FObject bn, FFixnum e)
{
    FAssert(BignumP(rbn));
    FAssert(BignumP(bn));
    FAssert(e >= 0);

    mpz_pow_ui(AsBignum(rbn), AsBignum(bn), (unsigned long) e);
}

inline static void BignumSqrt(FObject rt, FObject rem, FObject bn)
{
    FAssert(BignumP(rt));
    FAssert(BignumP(rem));
    FAssert(BignumP(bn));

    mpz_sqrtrem(AsBignum(rt), AsBignum(rem), AsBignum(bn));
}

inline static void BignumAnd(FObject rbn, FObject bn1, FObject bn2)
{
    FAssert(BignumP(rbn));
    FAssert(BignumP(bn1));
    FAssert(BignumP(bn2));

    mpz_and(AsBignum(rbn), AsBignum(bn1), AsBignum(bn2));
}

inline static void BignumIOr(FObject rbn, FObject bn1, FObject bn2)
{
    FAssert(BignumP(rbn));
    FAssert(BignumP(bn1));
    FAssert(BignumP(bn2));

    mpz_ior(AsBignum(rbn), AsBignum(bn1), AsBignum(bn2));
}

inline static void BignumXOr(FObject rbn, FObject bn1, FObject bn2)
{
    FAssert(BignumP(rbn));
    FAssert(BignumP(bn1));
    FAssert(BignumP(bn2));

    mpz_xor(AsBignum(rbn), AsBignum(bn1), AsBignum(bn2));
}

inline static void BignumNot(FObject rbn, FObject bn)
{
    FAssert(BignumP(rbn));
    FAssert(BignumP(bn));

    mpz_com(AsBignum(rbn), AsBignum(bn));
}

inline static uint_t BignumBitCount(FObject bn)
{
    FAssert(BignumP(bn));

    return(mpz_popcount(AsBignum(bn)));
}

inline static uint_t BignumIntegerLength(FObject bn)
{
    FAssert(BignumP(bn));

    return(mpz_sizeinbase(AsBignum(bn), 2));
}

inline static uint_t BignumFirstSetBit(FObject bn)
{
    FAssert(BignumP(bn));

    return(mpz_scan1(AsBignum(bn), 0));
}

inline static FObject BignumArithmeticShift(FObject bn, FFixnum cnt)
{
    FAssert(BignumP(bn));

    if (cnt == 0)
        return(bn);

    if (cnt < 0)
    {
        FObject rbn = MakeBignum();
        mpz_fdiv_q_2exp(AsBignum(rbn), AsBignum(bn), (mp_bitcnt_t) - cnt);
        return(Normalize(rbn));
    }

    FObject rbn = MakeBignum();
    mpz_mul_2exp(AsBignum(rbn), AsBignum(bn), (mp_bitcnt_t) cnt);
    return(rbn);
}

FObject MakeFlonum(double64_t dbl)
{
    FFlonum * flo = (FFlonum *) MakeObject(sizeof(FFlonum), FlonumTag);
    flo->Unused = FlonumTag;
    flo->Double = dbl;

    FAssert(isnan(dbl) || AsFlonum(flo) == dbl);

    return(flo);
}

inline static FObject Abs(FObject n)
{
    return(GenericSign(n) < 0 ? GenericMultiply(n, MakeFixnum(-1)) : n);
}

#define AsNumerator(z) AsRatio(z)->Numerator
#define AsDenominator(z) AsRatio(z)->Denominator

static FObject MakeRatio(FObject nmr, FObject dnm)
{
    FAssert(FixnumP(nmr) || BignumP(nmr));
    FAssert(FixnumP(dnm) || BignumP(dnm));
    FAssert(FixnumP(dnm) == 0 || AsFixnum(dnm) != 0);

    if (FixnumP(nmr) && AsFixnum(nmr) == 0)
        return(nmr);

    if (FixnumP(nmr) && FixnumP(dnm))
    {
        FFixnum n = AsFixnum(nmr);
        FFixnum d = AsFixnum(dnm);

        while (d != 0)
        {
            FFixnum t = n % d;
            n = d;
            d = t;
        }

        nmr = MakeFixnum(AsFixnum(nmr) / n);
        dnm = MakeFixnum(AsFixnum(dnm) / n);

        if (AsFixnum(dnm) == 1)
            return(nmr);

        if (AsFixnum(dnm) < 0)
        {
            dnm = MakeFixnum(AsFixnum(dnm) * -1);
            nmr = MakeFixnum(AsFixnum(nmr) * -1);
        }
    }
    else
    {
        FObject n = ToBignum(Abs(nmr));
        FObject d = ToBignum(Abs(dnm));

        nmr = ToBignum(nmr);
        dnm = ToBignum(dnm);

        while (BignumEqualFixnum(d, 0) == 0)
        {
            FObject t = MakeBignum();
            BignumRemainder(t, n, d);
            n = d;
            d = t;
        }

        BignumDivide(nmr, nmr, n);

        if (BignumCompare(dnm, n) == 0)
            return(Normalize(nmr));

        BignumDivide(dnm, dnm, n);

        if (GenericSign(dnm) < 0)
        {
            FAssert(BignumP(dnm));
            FAssert(BignumP(nmr));

            dnm = MakeBignum(dnm);
            nmr = MakeBignum(nmr);

            BignumMultiplyFixnum(dnm, dnm, -1);
            BignumMultiplyFixnum(nmr, nmr, -1);
        }
    }

    FAssert(GenericSign(dnm) > 0);

    FRatio * rat = (FRatio *) MakeObject(sizeof(FRatio), RatioTag);
    rat->Unused = RatioTag;
    rat->Numerator = Normalize(nmr);
    rat->Denominator = Normalize(dnm);

    return(rat);
}

static FObject RatioDivide(FObject obj)
{
    FAssert(RatioP(obj));

    if (FixnumP(AsNumerator(obj)) && FixnumP(AsDenominator(obj)))
        return(MakeFixnum(AsFixnum(AsNumerator(obj)) / AsFixnum(AsDenominator(obj))));

    FObject rbn = MakeBignum();

    BignumDivide(rbn, ToBignum(AsNumerator(obj)), ToBignum(AsDenominator(obj)));
    return(Normalize(rbn));
}

#define AsReal(z) AsComplex(z)->Real
#define AsImaginary(z) AsComplex(z)->Imaginary

static FObject MakeComplex(FObject rl, FObject img)
{
    FAssert(FixnumP(rl) || BignumP(rl) || FlonumP(rl) || RatioP(rl));
    FAssert(FixnumP(img) || BignumP(img) || FlonumP(img) || RatioP(img));

    if (FixnumP(img) && AsFixnum(img) == 0)
        return(rl);

    if (FlonumP(img))
        rl = ToInexact(rl);
    else if (FlonumP(rl))
        img = ToInexact(img);

    FComplex * cmplx = (FComplex *) MakeObject(sizeof(FComplex), ComplexTag);
    cmplx->Unused = ComplexTag;
    cmplx->Real = rl;
    cmplx->Imaginary = img;

    return(cmplx);
}

static inline FObject MakeComplex(double64_t rl, double64_t img)
{
    return(MakeComplex(MakeFlonum(rl), MakeFlonum(img)));
}

static FObject MakePolar(FObject r, FObject phi)
{
    r = ToInexact(r);
    phi = ToInexact(phi);

    FAssert(FlonumP(r));
    FAssert(FlonumP(phi));

    return(MakeComplex(AsFlonum(r) * cos(AsFlonum(phi)), AsFlonum(r) * sin(AsFlonum(phi))));
}

FObject ToInexact(FObject n)
{
    if (FixnumP(n))
        return(MakeFlonum((double64_t) AsFixnum(n)));
    else if (RatioP(n))
    {
        FAssert(FixnumP(AsNumerator(n)) || BignumP(AsNumerator(n)));
        FAssert(FixnumP(AsDenominator(n)) || BignumP(AsDenominator(n)));

        double64_t d = FixnumP(AsNumerator(n)) ? (double64_t) AsFixnum(AsNumerator(n))
                : BignumToDouble(AsNumerator(n));
        d /= FixnumP(AsDenominator(n)) ? AsFixnum(AsDenominator(n))
                : BignumToDouble(AsDenominator(n));
        return(MakeFlonum(d));
    }
    else if (ComplexP(n))
    {
        if (FlonumP(AsReal(n)) == 0)
            return(MakeComplex(ToInexact(AsReal(n)), ToInexact(AsImaginary(n))));

        return(n);
    }
    else if (BignumP(n))
        return(MakeFlonum(BignumToDouble(n)));

    FAssert(FlonumP(n));
    return(n);
}

static inline FFixnum TensDigit(double64_t n)
{
    return((FFixnum) (n - (Truncate(n / 10.0) * 10.0)));
}

FObject ToExact(FObject n)
{
    if (FlonumP(n))
    {
        double64_t d = AsFlonum(n);

        if (isnan(d) || isfinite(d) == 0)
            RaiseExceptionC(R.Assertion, "exact", "expected a finite number", List(n));

        if (d == Truncate(d))
        {
            if (d > MAXIMUM_FIXNUM || d < MINIMUM_FIXNUM)
                return(MakeBignum(d));

            return(MakeFixnum(d));
        }

        FObject whl = MakeBignum(Truncate(d));
        FObject rbn = MakeBignum((FFixnum) 0);
        FObject scl = MakeFixnum(1);
        FFixnum sgn = (d < 0 ? -1 : 1);
        d = fabs(d - Truncate(d));

        for (int_t idx = 0; d != Truncate(d) && idx < 14; idx++)
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
    else if (ComplexP(n))
    {
        if (FlonumP(AsReal(n)))
        {
            FAssert(FlonumP(AsImaginary(n)));

            return(MakeComplex(ToExact(AsReal(n)), ToExact(AsImaginary(n))));
        }

        FAssert(FlonumP(AsImaginary(n)) == 0);

        return(n);
    }

    FAssert(FixnumP(n) || BignumP(n) || RatioP(n));

    return(n);
}

static int_t ParseBignum(FCh * s, int_t sl, int_t sdx, FFixnum rdx, FFixnum sgn, FFixnum n,
    FObject * punt)
{
    FAssert(n > 0);

    FObject bn = MakeBignum(n);

    if (rdx == 16)
    {
        for (n = 0; sdx < sl; sdx++)
        {
            int_t dv = DigitValue(s[sdx]);

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
            int_t dv = DigitValue(s[sdx]);
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

static int_t ParseUInteger(FCh * s, int_t sl, int_t sdx, FFixnum rdx, FFixnum sgn, FObject * punt)
{
    // <uinteger> : <digit> <digit> ...

    FAssert(sdx < sl);

    FFixnum n;
    int_t strt = sdx;

    if (rdx == 16)
    {
        for (n = 0; sdx < sl; sdx++)
        {
            int64_t t;
            int_t dv = DigitValue(s[sdx]);

            if (dv >= 0 && dv <= 9)
                t = n * 16 + dv;
            else if (s[sdx] >= 'a' && s[sdx] <= 'f')
                t = n * 16 + s[sdx] - 'a' + 10;
            else if (s[sdx] >= 'A' && s[sdx] <= 'F')
                t = n * 16 + s[sdx] - 'A' + 10;
            else
                break;

            if (t < MINIMUM_FIXNUM || t > MAXIMUM_FIXNUM)
                return(ParseBignum(s, sl, sdx, rdx, sgn, n, punt));
            n = (FFixnum) t;
        }
    }
    else
    {
        FAssert(rdx == 2 || rdx == 8 || rdx == 10);

        for (n = 0; sdx < sl; sdx++)
        {
            int_t dv = DigitValue(s[sdx]);
            if (dv >= 0 && dv < rdx)
            {
                int64_t t = n * rdx + dv;
                if (t < MINIMUM_FIXNUM || t > MAXIMUM_FIXNUM)
                    return(ParseBignum(s, sl, sdx, rdx, sgn, n, punt));
                n = (FFixnum) t;
            }
            else
                break;
        }
    }

    if (sdx == strt)
        return(-1);

    *punt = MakeFixnum(n * sgn);

    return(sdx);
}

static int_t ParseDecimal10(FCh * s, int_t sl, int_t sdx, FFixnum sgn, FObject whl,
    FObject * pdc10)
{
    // <decimal10> : <uinteger> ... <suffix>
    //            | . <uinteger> ...
    //            | . <uinteger> ... <suffix>
    //            | <uinteger> ... . <digit> ...
    //            | <uinteger> ... . <digit> ... <suffix>
    //
    // <suffix> : e <digit> <digit> ...
    //          | e + <digit> <digit> ...
    //          | e - <digit> <digit> ...

    FAssert(FixnumP(whl) || BignumP(whl));
    FAssert(s[sdx] == '.' || s[sdx] == 'e' || s[sdx] == 'E');

    double64_t d = FixnumP(whl) ? (double64_t) AsFixnum(whl) : BignumToDouble(whl);

    if (sgn < 0)
        d *= -1;

    if (s[sdx] == '.')
    {
        double64_t scl = 0.1;

        sdx += 1;
        while (sdx < sl)
        {
            int_t dv = DigitValue(s[sdx]);
            if (dv >= 0 && dv <= 9)
            {
                d += dv * scl;
                scl *= 0.1;
            }
            else
                break;

            sdx += 1;
        }
    }

    if (sdx < sl && (s[sdx] == 'e' || s[sdx] == 'E'))
    {
        FObject e;
        FFixnum sgn = 1;

        sdx += 1;
        if (s[sdx] == '-')
        {
            sgn = -1;
            sdx += 1;
        }
        else if (s[sdx] == '+')
            sdx += 1;

        sdx = ParseUInteger(s, sl, sdx, 10, 1, &e);
        if (sdx < 0)
            return(sdx);

        if (BignumP(e))
            return(-1);

        FAssert(FixnumP(e));

        if (AsFixnum(e) != 0)
            d *= pow(10, (double) (AsFixnum(e) * sgn));
    }

    *pdc10 = MakeFlonum(d * sgn);
    return(sdx);
}

static int_t ParseUReal(FCh * s, int_t sl, int_t sdx, FFixnum rdx, FFixnum sgn, FObject * purl)
{
    // <ureal> : <uinteger>
    //         | <uinteger> / <uinteger>
    //         | <decimal10>

    FAssert(sdx < sl);

    if (s[sdx] == '.')
        return(ParseDecimal10(s, sl, sdx, sgn, MakeFixnum(0), purl));

    sdx = ParseUInteger(s, sl, sdx, rdx, sgn, purl);
    if (sdx < 0 || sdx == sl)
        return(sdx);

    FAssert(sdx < sl);

    if (s[sdx] == '/')
    {
        FObject dnm;
        sdx = ParseUInteger(s, sl, sdx + 1, rdx, 1, &dnm);
        if (sdx < 0)
            return(-1);

        if (FixnumP(dnm) && AsFixnum(dnm) == 0)
            return(-1);

        *purl = MakeRatio(*purl, dnm);
        return(sdx);
    }
    else if ((s[sdx] == '.' || s[sdx] == 'e' || s[sdx] == 'E') && rdx == 10)
        return(ParseDecimal10(s, sl, sdx, sgn, *purl, purl));

    return(sdx);
}

static int_t ParseReal(FCh * s, int_t sl, int_t sdx, FFixnum rdx, FObject * prl)
{
    // <real> : <ureal>
    //        | + <ureal>
    //        | - <ureal>
    //        | <infnan>
    //
    // <infnan> : +inf.0 | -inf.0 | +nan.0 | -nan.0

    FAssert(sdx < sl);

    if (sdx + 6 <= sl && (s[sdx] == '-' || s[sdx] == '+'))
    {
        if (CharDowncase(s[sdx + 1]) == 'i' && CharDowncase(s[sdx + 2]) == 'n'
                && CharDowncase(s[sdx + 3]) == 'f' && s[sdx + 4] == '.' && s[sdx + 5] == '0')
        {
            // +inf.0 | -inf.0

            *prl = MakeFlonum(s[sdx] == '+' ? POSITIVE_INFINITY : NEGATIVE_INFINITY);
            sdx += 6;
            return(sdx);
        }
        else if (CharDowncase(s[sdx + 1]) == 'n' && CharDowncase(s[sdx + 2]) == 'a'
                && CharDowncase(s[sdx + 3]) == 'n' && s[sdx + 4] == '.' && s[sdx + 5] == '0')
        {
            // +nan.0 | -nan.0

            *prl = MakeFlonum(NAN);
            sdx += 6;
            return(sdx);
        }
    }

    FFixnum sgn = 1;

    if (s[sdx] == '-')
    {
        sgn = -1;
        sdx += 1;
    }
    else if (s[sdx] == '+')
    {
        FAssert(sgn == 1);

        sdx += 1;
    }

    if (sdx == sl)
        return(-1);

    FObject url;

    sdx = ParseUReal(s, sl, sdx, rdx, sgn, &url);
    if (sdx < 0)
        return(-1);

    *prl = url;
    return(sdx);
}

static int_t ParseComplex(FCh * s, int_t sl, FFixnum rdx, FObject * pcmplx)
{
    // <complex> : <real>
    //           | <real> @ <real>
    //           | <real> + <ureal> i
    //           | <real> - <ureal> i
    //           | <real> + i
    //           | <real> - i
    //           | <real> <infnan> i
    //           | <real> i
    //           | + i
    //           | - i

    if (sl == 2)
    {
        // -i | +i

        if (s[0] == '-' && (s[1] == 'i' || s[1] == 'I'))
        {
            *pcmplx = MakeComplex(MakeFixnum(0), MakeFixnum(-1));
            return(sl);
        }
        else if (s[0] == '+' && (s[1] == 'i' || s[1] == 'I'))
        {
            *pcmplx = MakeComplex(MakeFixnum(0), MakeFixnum(1));
            return(sl);
        }
    }

    int_t sdx = ParseReal(s, sl, 0, rdx, pcmplx);
    if (sdx < 0 || sdx == sl)
        return(sdx);

    if (s[sdx] == '@')
    {
        // <real> @ <real>

        FObject phi;
        sdx = ParseReal(s, sl, sdx + 1, rdx, &phi);
        if (sdx != sl)
            return(-1);

        *pcmplx = MakePolar(*pcmplx, phi);
        return(sl);
    }
    else if (sdx + 2 == sl)
    {
        // <real> + i
        // <real> - i

        if (s[sdx] == '-' && (s[sdx + 1] == 'i' || s[sdx + 1] == 'I'))
        {
            *pcmplx = MakeComplex(*pcmplx, MakeFixnum(-1));
            return(sl);
        }
        else if (s[sdx] == '+' && (s[sdx + 1] == 'i' || s[sdx + 1] == 'I'))
        {
            *pcmplx = MakeComplex(*pcmplx, MakeFixnum(1));
            return(sl);
        }
    }
    else if (sdx + 1 == sl && (s[sdx] == 'i' || s[sdx] == 'I'))
    {
        // <real> i

        *pcmplx = MakeComplex(MakeFixnum(0), *pcmplx);
        return(sl);
    }
    else if (s[sdx] == '+' || s[sdx] == '-')
    {
        // <real> + <ureal> i
        // <real> - <ureal> i
        // <real> <infnan> i

        FObject img;
        sdx = ParseReal(s, sl, sdx, rdx, &img);
        if (sdx + 1 != sl || (s[sdx] != 'i' && s[sdx] != 'I'))
            return(-1);

        *pcmplx = MakeComplex(*pcmplx, img);
        return(sl);
    }

    return(-1);
}

#define EXACTNESS_NONE 0
#define EXACTNESS_EXACT 1
#define EXACTNESS_INEXACT 2

FObject StringToNumber(FCh * s, int_t sl, FFixnum rdx)
{
    FAssert(rdx == 2 || rdx == 8 || rdx == 10 || rdx == 16);

    if (sl == 0)
        return(FalseObject);

    int_t epf = EXACTNESS_NONE;
    int_t rpf = 0;
    int_t sdx = 0;

    while (sdx < sl && s[sdx] == '#')
    {
        sdx += 1;
        if (sdx == sl)
            return(FalseObject);

        switch (s[sdx])
        {
        case 'i':
        case 'I':
            if (epf != EXACTNESS_NONE)
                return(FalseObject);
            epf = EXACTNESS_INEXACT;
            break;

        case 'e':
        case 'E':
            if (epf != EXACTNESS_NONE)
                return(FalseObject);
            epf = EXACTNESS_EXACT;
            break;

        case 'b':
        case 'B':
            if (rpf)
                return(FalseObject);
            rdx = 2;
            break;

        case 'o':
        case 'O':
            if (rpf)
                return(FalseObject);
            rdx = 8;
            break;

        case 'd':
        case 'D':
            if (rpf)
                return(FalseObject);
            rdx = 10;
            break;

        case 'x':
        case 'X':
            if (rpf)
                return(FalseObject);
            rdx = 16;
            break;

        default:
            return(FalseObject);
        }

        sdx += 1;
    }

    FObject n;

    if (ParseComplex(s + sdx, sl - sdx, rdx, &n) < 0)
        return(FalseObject);

    if (epf == EXACTNESS_NONE)
        return(n);
    else if (epf == EXACTNESS_EXACT)
        return(ToExact(n));

    FAssert(epf == EXACTNESS_INEXACT);

    return(ToInexact(n));
}

const static char Digits[] = {"0123456789abcdef"};

int_t FixnumAsString(FFixnum n, FCh * s, FFixnum rdx)
{
    FAssert(rdx <= (FFixnum) sizeof(Digits));

    int_t sl = 0;

    if (n < 0)
    {
        s[sl] = '-';
        sl += 1;
        n *= -1;
    }

    if (n >= rdx)
    {
        sl += FixnumAsString(n / rdx, s + sl, rdx);
        s[sl] = Digits[n % rdx];
        sl += 1;
    }
    else
    {
        s[sl] = Digits[n];
        sl += 1;
    }

    return(sl);
}

static int_t NeedImaginaryPlusSignP(FObject n)
{
    if (FixnumP(n))
        return(AsFixnum(n) >= 0);
    else if (RatioP(n))
        return(NeedImaginaryPlusSignP(AsRatio(n)->Numerator));
    else if (FlonumP(n))
        return(isnan(AsFlonum(n)) == 0 && isfinite(AsFlonum(n)) != 0 && AsFlonum(n) >= 0.0);

    FAssert(BignumP(n));

    return(mpz_cmp_si(AsBignum(n), 0) >= 0);
}

static void WriteNumber(FObject port, FObject obj, FFixnum rdx)
{
    FAssert(NumberP(obj));

    if (FixnumP(obj))
    {
        FCh s[32];
        int_t sl = FixnumAsString(AsFixnum(obj), s, rdx);

        WriteString(port, s, sl);
    }
    else if (RatioP(obj))
    {
        WriteNumber(port, AsRatio(obj)->Numerator, rdx);
        WriteCh(port, '/');
        WriteNumber(port, AsRatio(obj)->Denominator, rdx);
    }
    else if (ComplexP(obj))
    {
        WriteNumber(port, AsComplex(obj)->Real, rdx);
        if (NeedImaginaryPlusSignP(AsComplex(obj)->Imaginary))
            WriteCh(port, '+');
        if (FixnumP(AsComplex(obj)->Imaginary) == 0 || AsFixnum(AsComplex(obj)->Imaginary) != 1)
        {
            if (FixnumP(AsComplex(obj)->Imaginary) && AsFixnum(AsComplex(obj)->Imaginary) == -1)
                WriteCh(port, '-');
            else
                WriteNumber(port, AsComplex(obj)->Imaginary, rdx);
        }
        WriteCh(port, 'i');
    }
    else if (FlonumP(obj))
    {
        if (rdx != 10)
            RaiseExceptionC(R.Assertion, "number->string", "radix for decimal numbers must be 10",
                    List(obj, MakeFixnum(rdx)));

        double64_t d = AsFlonum(obj);

        if (isnan(d))
            WriteStringC(port, "+nan.0");
        else if (isfinite(d) == 0)
            WriteStringC(port, d > 0 ? "+inf.0" : "-inf.0");
        else
        {
            char s[128];
            int_t idx = sprintf_s(s, sizeof(s), "%.14g", d);

            if (d == Truncate(d) && strchr(s, '.') == 0)
            {
                s[idx] = '.';
                idx += 1;
                s[idx] = '0';
                idx += 1;
                s[idx] = 0;
            }

            WriteStringC(port, s);
        }
    }
    else
    {
        FAssert(BignumP(obj));

        char * s = mpz_get_str(0, (int) rdx, AsBignum(obj));
        WriteStringC(port, s);
        free(s);
    }
}

FObject NumberToString(FObject obj, FFixnum rdx)
{
    FAssert(NumberP(obj));

    if (FixnumP(obj))
    {
        FCh s[32];
        int_t sl = FixnumAsString(AsFixnum(obj), s, rdx);

        return(MakeString(s, sl));
    }
    else if (FlonumP(obj))
    {
        if (rdx != 10)
            RaiseExceptionC(R.Assertion, "number->string", "radix for decimal numbers must be 10",
                    List(obj, MakeFixnum(rdx)));

        double64_t d = AsFlonum(obj);

        if (isnan(d))
            return(MakeStringC("+nan.0"));
        else if (isfinite(d) == 0)
            return(MakeStringC(d > 0 ? "+inf.0" : "-inf.0"));
        else
        {
            char s[128];
            int_t idx = sprintf_s(s, sizeof(s), "%.14g", d);

            if (d == Truncate(d) && strchr(s, '.') == 0)
            {
                s[idx] = '.';
                idx += 1;
                s[idx] = '0';
                idx += 1;
                s[idx] = 0;
            }

            return(MakeStringC(s));
        }
    }

    FAssert(RatioP(obj) || ComplexP(obj) || BignumP(obj));

    FObject port = MakeStringOutputPort();
    WriteNumber(port, obj, rdx);
    return(GetOutputString(port));
}

/*
This code assumes a very specific layout of immediate type tags: Fixnums, Ratios, Complex,
and Flonum all have immediate type tags with 0x4 set. No other immediate type tags have 0x4 set.

In determining the operation, Bignums and Ratios end up together. Both have 0x0 as their
low two bits.
*/
static inline int_t BothNumberP(FObject z1, FObject z2)
{
//    return(((((FImmediate) (z1)) & 0x4) + (((FImmediate) (z2)) & 0x4)) == 0x8 ||
//            (NumberP(z1) && NumberP(z2)));
    return(NumberP(z1) && NumberP(z2));
}

//#define BinaryNumberOp(z1, z2) ((((FImmediate) (z1)) & 0x3) << 2) | (((FImmediate) (z2)) & 0x3)
static inline int_t BinaryNumberOp(FObject z1, FObject z2)
{
    int_t op;

    if (ComplexP(z1))
        op = 0x1 << 2;
    else if (FlonumP(z1))
        op = 0x2 << 2;
    else if (FixnumP(z1))
        op = 0x3 << 2;
    else
        op = 0;

    if (ComplexP(z2))
        op |= 0x1;
    else if (FlonumP(z2))
        op |= 0x2;
    else if (FixnumP(z2))
        op |= 0x3;

    return(op);
}

static const int_t BOP_BIGRAT_BIGRAT = 0x0;   // 0b0000
static const int_t BOP_BIGRAT_COMPLEX = 0x1;  // 0b0001
static const int_t BOP_BIGRAT_FLOAT = 0x2;    // 0b0010
static const int_t BOP_BIGRAT_FIXED = 0x3;    // 0b0011
static const int_t BOP_COMPLEX_BIGRAT = 0x4;  // 0b0100
static const int_t BOP_COMPLEX_COMPLEX = 0x5; // 0b0101
static const int_t BOP_COMPLEX_FLOAT = 0x6;   // 0b0110
static const int_t BOP_COMPLEX_FIXED = 0x7;   // 0b0111
static const int_t BOP_FLOAT_BIGRAT = 0x8;    // 0b1000
static const int_t BOP_FLOAT_COMPLEX = 0x9;   // 0b1001
static const int_t BOP_FLOAT_FLOAT = 0xA;     // 0b1010
static const int_t BOP_FLOAT_FIXED = 0xB;     // 0b1011
static const int_t BOP_FIXED_BIGRAT = 0xC;    // 0b1100
static const int_t BOP_FIXED_COMPLEX = 0xD;   // 0b1101
static const int_t BOP_FIXED_FLOAT = 0xE;     // 0b1110
static const int_t BOP_FIXED_FIXED = 0xF;     // 0b1111

//#define UnaryNumberOp(z) (((FImmediate) (z)) & 0x3)
static inline int_t UnaryNumberOp(FObject z)
{
    int_t op;

    if (ComplexP(z))
        op = 0x1;
    else if (FlonumP(z))
        op = 0x2;
    else if (FixnumP(z))
        op = 0x3;
    else
        op = 0;

    return(op);
}

static const int_t UOP_BIGRAT = 0x0;  // 0b0000
static const int_t UOP_COMPLEX = 0x1; // 0b0001
static const int_t UOP_FLOAT = 0x2;   // 0b0010
static const int_t UOP_FIXED = 0x3;   // 0b0011

static int_t GenericSign(FObject x)
{
    switch(UnaryNumberOp(x))
    {
        case UOP_BIGRAT:
            if (RatioP(x))
                return(GenericSign(AsNumerator(x)));
            else
            {
                FAssert(BignumP(x));

                return(BignumSign(x));
            }

        case UOP_COMPLEX:
            break;

        case UOP_FLOAT:
            return(AsFlonum(x) > 0.0 ? 1 : (AsFlonum(x) < 0.0 ? -1 : 0));

        case UOP_FIXED:
            return(AsFixnum(x) > 0 ? 1 : (AsFixnum(x) < 0 ? -1 : 0));
    }

    FAssert(ComplexP(x) == 0);

    return(0);
}

int_t GenericEqvP(FObject x1, FObject x2)
{
    if (BothNumberP(x1, x2))
    {
        switch (BinaryNumberOp(x1, x2))
        {
        case BOP_BIGRAT_BIGRAT:
            if (RatioP(x1))
                return(RatioP(x2) && GenericCompare("eqv?", x1, x2, 0) == 0);
            else
            {
                FAssert(BignumP(x1));

                return(BignumP(x2) && BignumCompare(x1, x2) == 0);
            }

        case BOP_BIGRAT_COMPLEX:
        case BOP_BIGRAT_FLOAT:
        case BOP_BIGRAT_FIXED:
        case BOP_COMPLEX_BIGRAT:
            break;

        case BOP_COMPLEX_COMPLEX:
            return(GenericCompare("eqv?", x1, x2, 1) == 0);

        case BOP_COMPLEX_FLOAT:
        case BOP_COMPLEX_FIXED:
        case BOP_FLOAT_BIGRAT:
        case BOP_FLOAT_COMPLEX:
            break;

        case BOP_FLOAT_FLOAT:
            return(AsFlonum(x1) == AsFlonum(x2));

        case BOP_FLOAT_FIXED:
        case BOP_FIXED_BIGRAT:
        case BOP_FIXED_COMPLEX:
        case BOP_FIXED_FLOAT:
            break;

        case BOP_FIXED_FIXED:
            return(AsFixnum(x1) == AsFixnum(x2));

        default:
            FAssert(0);
        }
    }

    return(0);
}

static int_t GenericCompare(const char * who, FObject x1, FObject x2, int_t cf)
{
    if (BothNumberP(x1, x2))
    {
        switch (BinaryNumberOp(x1, x2))
        {
        case BOP_BIGRAT_BIGRAT:
            return(GenericSign(GenericSubtract(x1, x2)));

        case BOP_BIGRAT_COMPLEX:
            break;

        case BOP_BIGRAT_FLOAT:
            return(GenericSign(GenericSubtract(x1, ToExact(x2))));

        case BOP_BIGRAT_FIXED:
            return(GenericSign(GenericSubtract(x1, x2)));

        case BOP_COMPLEX_BIGRAT:
        case BOP_COMPLEX_COMPLEX:
        case BOP_COMPLEX_FLOAT:
        case BOP_COMPLEX_FIXED:
            break;

        case BOP_FLOAT_BIGRAT:
            return(GenericSign(GenericSubtract(ToExact(x1), x2)));

        case BOP_FLOAT_COMPLEX:
            break;

        case BOP_FLOAT_FLOAT:
        {
            double64_t n = AsFlonum(x1) - AsFlonum(x2);
            return(n > 0.0 ? 1 : (n == 0.0 ? 0 : -1));
        }

        case BOP_FLOAT_FIXED:
        {
            double64_t n = AsFlonum(x1) - AsFixnum(x2);
            return(n > 0.0 ? 1 : (n < 0.0 ? -1 : 0));
        }

        case BOP_FIXED_BIGRAT:
            return(GenericSign(GenericSubtract(x1, x2)));

        case BOP_FIXED_COMPLEX:
            break;

        case BOP_FIXED_FLOAT:
        {
            double64_t n = AsFixnum(x1) - AsFlonum(x2);
            return(n > 0.0 ? 1 : (n < 0.0 ? -1 : 0));
        }

        case BOP_FIXED_FIXED:
        {
            int64_t n = AsFixnum(x1) - AsFixnum(x2);
            return(n > 0 ? 1 : (n < 0 ? -1 : 0));
        }

        default:
            FAssert(0);
        }
    }

    if (cf == 0)
    {
        RealArgCheck(who, x1);
        RealArgCheck(who, x2);

        FAssert(0);
    }

    NumberArgCheck(who, x1);
    NumberArgCheck(who, x2);

    FObject r1 = ComplexP(x1) ? AsReal(x1) : x1;
    FObject i1 = ComplexP(x1) ? AsImaginary(x1) : MakeFixnum(0);
    FObject r2 = ComplexP(x2) ? AsReal(x2) : x2;
    FObject i2 = ComplexP(x2) ? AsImaginary(x2) : MakeFixnum(0);

    int_t ret = GenericCompare(who, r1, r2, 0);
    return(ret == 0 ? GenericCompare(who, i1, i2, 0) : ret);
}

uint_t NumberHash(FObject z)
{
    switch(UnaryNumberOp(z))
    {
        case UOP_BIGRAT:
            if (RatioP(z))
                return(NumberHash(AsNumerator(z)) + (NumberHash(AsDenominator(z)) << 7));
            else
            {
                FAssert(BignumP(z));

                return(BignumRemainderFixnum(z, MAXIMUM_FIXNUM));
            }

        case UOP_COMPLEX:
            return(NumberHash(AsReal(z)) + (NumberHash(AsImaginary(z)) << 7));

        case UOP_FLOAT:
            return((uint_t) AsFlonum(z));

        case UOP_FIXED:
            return(AsFixnum(z));
    }

    return(0);
}

int_t NumberCompare(FObject obj1, FObject obj2)
{
    int_t ret;

    if (ComplexP(obj1) || ComplexP(obj2))
    {
        FObject r1 = ComplexP(obj1) ? AsReal(obj1) : obj1;
        FObject i1 = ComplexP(obj1) ? AsImaginary(obj1) : MakeFixnum(0);
        FObject r2 = ComplexP(obj2) ? AsReal(obj2) : obj2;
        FObject i2 = ComplexP(obj2) ? AsImaginary(obj2) : MakeFixnum(0);

        ret = GenericCompare(0, r1, r2, 0);
        if (ret == 0)
            ret = GenericCompare(0, i1, i2, 0);
    }
    else
    {
        FAssert(RealP(obj1));
        FAssert(RealP(obj2));

        ret = GenericCompare(0, obj1, obj2, 0);
    }

    if (ret == 0)
        return(0);
    return(ret < 0 ? -1 : 1);
}

static FObject GenericAdd(FObject z1, FObject z2)
{
    if (BothNumberP(z1, z2))
    {
        switch (BinaryNumberOp(z1, z2))
        {
        case BOP_BIGRAT_BIGRAT:
            if (RatioP(z1))
            {
                if (RatioP(z2))
                    return(MakeRatio(GenericAdd(
                            GenericMultiply(AsNumerator(z1), AsDenominator(z2)),
                            GenericMultiply(AsNumerator(z2), AsDenominator(z1))),
                            GenericMultiply(AsDenominator(z1), AsDenominator(z2))));

                return(MakeRatio(GenericAdd(
                        GenericMultiply(AsDenominator(z1), z2), AsNumerator(z1)),
                        AsDenominator(z1)));
            }
            else if (RatioP(z2))
                return(MakeRatio(GenericAdd(
                        GenericMultiply(AsDenominator(z2), z1), AsNumerator(z2)),
                        AsDenominator(z2)));
            else
            {
                FAssert(BignumP(z1));
                FAssert(BignumP(z2));

                FObject rbn = MakeBignum();
                BignumAdd(rbn, z1, z2);
                return(Normalize(rbn));
            }

        case BOP_BIGRAT_COMPLEX:
            return(MakeComplex(GenericAdd(z1, AsReal(z2)), AsImaginary(z2)));

        case BOP_BIGRAT_FLOAT:
        {
            FObject flo = ToInexact(z1);

            FAssert(FlonumP(flo));

            return(MakeFlonum(AsFlonum(flo) + AsFlonum(z2)));
        }

        case BOP_BIGRAT_FIXED:
            if (RatioP(z1))
                return(MakeRatio(GenericAdd(AsNumerator(z1),
                        GenericMultiply(AsDenominator(z1), z2)), AsDenominator(z1)));
            else
            {
                FAssert(BignumP(z1));

                FObject rbn = MakeBignum();
                BignumAddFixnum(rbn, z1, AsFixnum(z2));
                return(Normalize(rbn));
            }

        case BOP_COMPLEX_BIGRAT:
            return(MakeComplex(GenericAdd(AsReal(z1), z2), AsImaginary(z1)));

        case BOP_COMPLEX_COMPLEX:
            return(MakeComplex(GenericAdd(AsReal(z1), AsReal(z2)),
                    GenericAdd(AsImaginary(z1), AsImaginary(z2))));

        case BOP_COMPLEX_FLOAT:
        case BOP_COMPLEX_FIXED:
            return(MakeComplex(GenericAdd(AsReal(z1), z2), AsImaginary(z1)));

        case BOP_FLOAT_BIGRAT:
        {
            FObject flo = ToInexact(z2);

            FAssert(FlonumP(flo));

            return(MakeFlonum(AsFlonum(z1) + AsFlonum(flo)));
        }

        case BOP_FLOAT_COMPLEX:
            return(MakeComplex(GenericAdd(z1, AsReal(z2)), AsImaginary(z2)));

        case BOP_FLOAT_FLOAT:
            return(MakeFlonum(AsFlonum(z1) + AsFlonum(z2)));

        case BOP_FLOAT_FIXED:
            return(MakeFlonum(AsFlonum(z1) + AsFixnum(z2)));

        case BOP_FIXED_BIGRAT:
            if (RatioP(z2))
                return(MakeRatio(GenericAdd(AsNumerator(z2),
                        GenericMultiply(AsDenominator(z2), z1)), AsDenominator(z2)));
            else
            {
                FAssert(BignumP(z2));

                FObject rbn = MakeBignum();
                BignumAddFixnum(rbn, z2, AsFixnum(z1));
                return(Normalize(rbn));
            }

        case BOP_FIXED_COMPLEX:
            return(MakeComplex(GenericAdd(z1, AsReal(z2)), AsImaginary(z2)));

        case BOP_FIXED_FLOAT:
            return(MakeFlonum(AsFixnum(z1) + AsFlonum(z2)));

        case BOP_FIXED_FIXED:
        {
            int64_t n = AsFixnum(z1) + AsFixnum(z2);
            if (n < MINIMUM_FIXNUM || n > MAXIMUM_FIXNUM)
            {
                FObject bn = MakeBignum(AsFixnum(z1));
                BignumAddFixnum(bn, bn, AsFixnum(z2));
                return(Normalize(bn));
            }

            return(MakeFixnum(n));
        }

        default:
            FAssert(0);
        }
    }

    NumberArgCheck("+", z1);
    NumberArgCheck("+", z2);

    FAssert(0);

    return(NoValueObject);
}

static FObject GenericMultiply(FObject z1, FObject z2)
{
    if (BothNumberP(z1, z2))
    {
        switch (BinaryNumberOp(z1, z2))
        {
        case BOP_BIGRAT_BIGRAT:
            if (RatioP(z1))
            {
                if (RatioP(z2))
                    return(MakeRatio(GenericMultiply(AsNumerator(z1), AsNumerator(z2)),
                            GenericMultiply(AsDenominator(z1), AsDenominator(z2))));

                return(MakeRatio(GenericMultiply(AsNumerator(z1), z2), AsDenominator(z1)));
            }
            else if (RatioP(z2))
                return(MakeRatio(GenericMultiply(AsNumerator(z2), z1), AsDenominator(z2)));
            else
            {
                FAssert(BignumP(z1));
                FAssert(BignumP(z2));

                FObject rbn = MakeBignum();
                BignumMultipy(rbn, z1, z2);
                return(Normalize(rbn));
            }

        case BOP_BIGRAT_COMPLEX:
            return(MakeComplex(GenericMultiply(z1, AsReal(z2)),
                    GenericMultiply(z1, AsImaginary(z2))));

        case BOP_BIGRAT_FLOAT:
        {
            FObject flo = ToInexact(z1);

            FAssert(FlonumP(flo));

            return(MakeFlonum(AsFlonum(flo) * AsFlonum(z2)));
        }

        case BOP_BIGRAT_FIXED:
            if (RatioP(z1))
                return(MakeRatio(GenericMultiply(AsNumerator(z1), z2), AsDenominator(z1)));
            else
            {
                FAssert(BignumP(z1));

                FObject rbn = MakeBignum();
                BignumMultiplyFixnum(rbn, z1, AsFixnum(z2));
                return(Normalize(rbn));
            }

        case BOP_COMPLEX_BIGRAT:
            return(MakeComplex(GenericMultiply(AsReal(z1), z2),
                    GenericMultiply(AsImaginary(z1), z2)));

        case BOP_COMPLEX_COMPLEX:
            return(MakeComplex(GenericSubtract(GenericMultiply(AsReal(z1), AsReal(z2)),
                    GenericMultiply(AsImaginary(z1), AsImaginary(z2))),
                    GenericAdd(GenericMultiply(AsReal(z1), AsImaginary(z2)),
                    GenericMultiply(AsImaginary(z1), AsReal(z2)))));

        case BOP_COMPLEX_FLOAT:
        case BOP_COMPLEX_FIXED:
            return(MakeComplex(GenericMultiply(AsReal(z1), z2),
                    GenericMultiply(AsImaginary(z1), z2)));

        case BOP_FLOAT_BIGRAT:
        {
            FObject flo = ToInexact(z2);

            FAssert(FlonumP(flo));

            return(MakeFlonum(AsFlonum(z1) * AsFlonum(flo)));
        }

        case BOP_FLOAT_COMPLEX:
            return(MakeComplex(GenericMultiply(z1, AsReal(z2)),
                    GenericMultiply(z1, AsImaginary(z2))));

        case BOP_FLOAT_FLOAT:
            return(MakeFlonum(AsFlonum(z1) * AsFlonum(z2)));

        case BOP_FLOAT_FIXED:
            return(MakeFlonum(AsFlonum(z1) * AsFixnum(z2)));

        case BOP_FIXED_BIGRAT:
            if (RatioP(z2))
                return(MakeRatio(GenericMultiply(AsNumerator(z2), z1), AsDenominator(z2)));
            else
            {
                FAssert(BignumP(z2));

                FObject rbn = MakeBignum();
                BignumMultiplyFixnum(rbn, z2, AsFixnum(z1));
                return(Normalize(rbn));
            }

        case BOP_FIXED_COMPLEX:
            return(MakeComplex(GenericMultiply(z1, AsReal(z2)),
                    GenericMultiply(z1, AsImaginary(z2))));

        case BOP_FIXED_FLOAT:
            return(MakeFlonum(AsFixnum(z1) * AsFlonum(z2)));

        case BOP_FIXED_FIXED:
        {
            int64_t n = AsFixnum(z1) * AsFixnum(z2);
            if (n < MINIMUM_FIXNUM || n > MAXIMUM_FIXNUM)
            {
                FObject bn = MakeBignum(AsFixnum(z1));
                BignumMultiplyFixnum(bn, bn, AsFixnum(z2));
                return(Normalize(bn));
            }

            return(MakeFixnum(n));
        }

        default:
            FAssert(0);
        }
    }

    NumberArgCheck("*", z1);
    NumberArgCheck("*", z2);

    FAssert(0);
    return(NoValueObject);
}

static FObject GenericSubtract(FObject z1, FObject z2)
{
    if (BothNumberP(z1, z2))
    {
        switch (BinaryNumberOp(z1, z2))
        {
        case BOP_BIGRAT_BIGRAT:
            if (RatioP(z1))
            {
                if (RatioP(z2))
                    return(MakeRatio(GenericSubtract(
                            GenericMultiply(AsNumerator(z1), AsDenominator(z2)),
                            GenericMultiply(AsNumerator(z2), AsDenominator(z1))),
                            GenericMultiply(AsDenominator(z1), AsDenominator(z2))));

                return(MakeRatio(GenericSubtract(
                        AsNumerator(z1), GenericMultiply(AsDenominator(z1), z2)),
                        AsDenominator(z1)));
            }
            else if (RatioP(z2))
                return(MakeRatio(GenericSubtract(
                        GenericMultiply(z1, AsDenominator(z2)), AsNumerator(z2)),
                        AsDenominator(z2)));
            else
            {
                FAssert(BignumP(z1));
                FAssert(BignumP(z2));

                FObject rbn = MakeBignum();
                BignumSubtract(rbn, z1, z2);
                return(Normalize(rbn));
            }

        case BOP_BIGRAT_COMPLEX:
            return(MakeComplex(GenericSubtract(z1, AsReal(z2)),
                    GenericSubtract(MakeFixnum(0), AsImaginary(z2))));

        case BOP_BIGRAT_FLOAT:
        {
            FObject flo = ToInexact(z1);

            FAssert(FlonumP(flo));

            return(MakeFlonum(AsFlonum(flo) - AsFlonum(z2)));
        }

        case BOP_BIGRAT_FIXED:
            if (RatioP(z1))
                return(MakeRatio(GenericSubtract(AsNumerator(z1),
                        GenericMultiply(AsDenominator(z1), z2)), AsDenominator(z1)));
            else
            {
                FAssert(BignumP(z1));

                FObject rbn = MakeBignum();
                BignumAddFixnum(rbn, z1, - AsFixnum(z2));
                return(Normalize(rbn));
            }

        case BOP_COMPLEX_BIGRAT:
            return(MakeComplex(GenericSubtract(AsReal(z1), z2), AsImaginary(z1)));

        case BOP_COMPLEX_COMPLEX:
            return(MakeComplex(GenericSubtract(AsReal(z1), AsReal(z2)),
                    GenericSubtract(AsImaginary(z1), AsImaginary(z2))));

        case BOP_COMPLEX_FLOAT:
        case BOP_COMPLEX_FIXED:
            return(MakeComplex(GenericSubtract(AsReal(z1), z2), AsImaginary(z1)));

        case BOP_FLOAT_BIGRAT:
        {
            FObject flo = ToInexact(z2);

            FAssert(FlonumP(flo));

            return(MakeFlonum(AsFlonum(z1) - AsFlonum(flo)));
        }

        case BOP_FLOAT_COMPLEX:
            return(MakeComplex(GenericSubtract(z1, AsReal(z2)),
                    GenericSubtract(MakeFlonum(0), AsImaginary(z2))));

        case BOP_FLOAT_FLOAT:
            return(MakeFlonum(AsFlonum(z1) - AsFlonum(z2)));

        case BOP_FLOAT_FIXED:
            return(MakeFlonum(AsFlonum(z1) - AsFixnum(z2)));

        case BOP_FIXED_BIGRAT:
            if (RatioP(z2))
                return(MakeRatio(GenericSubtract(GenericMultiply(AsDenominator(z2), z1),
                        AsNumerator(z2)), AsDenominator(z2)));
            else
            {
                FAssert(BignumP(z2));

                FObject rbn = MakeBignum(z2);
                BignumMultiplyFixnum(rbn, rbn, -1);
                BignumAddFixnum(rbn, rbn, AsFixnum(z1));
                return(Normalize(rbn));
            }

        case BOP_FIXED_COMPLEX:
            return(MakeComplex(GenericSubtract(z1, AsReal(z2)),
                    GenericSubtract(MakeFixnum(0), AsImaginary(z2))));

        case BOP_FIXED_FLOAT:
            return(MakeFlonum(AsFixnum(z1) - AsFlonum(z2)));

        case BOP_FIXED_FIXED:
        {
            int64_t n = AsFixnum(z1) - AsFixnum(z2);
            if (n < MINIMUM_FIXNUM || n > MAXIMUM_FIXNUM)
            {
                FObject bn = MakeBignum(AsFixnum(z1));
                BignumAddFixnum(bn, bn, - AsFixnum(z2));
                return(Normalize(bn));
            }

            return(MakeFixnum(n));
        }

        default:
            FAssert(0);
        }
    }

    NumberArgCheck("-", z1);
    NumberArgCheck("-", z2);

    FAssert(0);
    return(NoValueObject);
}

static FObject ComplexDivide(FObject r1, FObject i1, FObject r2, FObject i2)
{
/*
(r1 + i1 i) / (r2 + i2 i) =
    (r1 * r2 + i1 * i2) / (r2 * r2 + i2 * i2)
    + (i1 * r2 - r1 * i2) / (r2 * r2 + i2 * i2) i
*/

    FObject dnm = GenericAdd(GenericMultiply(r2, r2), GenericMultiply(i2, i2));

    return(MakeComplex(
        GenericDivide(GenericAdd(GenericMultiply(r1, r2), GenericMultiply(i1, i2)), dnm),
        GenericDivide(GenericSubtract(GenericMultiply(i1, r2), GenericMultiply(r1, i2)), dnm)));
}

static FObject GenericDivide(FObject z1, FObject z2)
{
/*
WriteSimple(R.StandardOutput, z1, 0);
WriteCh(R.StandardOutput, '/');
WriteSimple(R.StandardOutput, z2, 0);
WriteCh(R.StandardOutput, '\n');
*/
    if (BothNumberP(z1, z2))
    {
        if (ComplexP(z2) == 0 && GenericSign(z2) == 0)
            RaiseExceptionC(R.Assertion, "/", "divide by zero", List(z1, z2));

        switch (BinaryNumberOp(z1, z2))
        {
        case BOP_BIGRAT_BIGRAT:
            if (RatioP(z1))
            {
                if (RatioP(z2))
                    return(MakeRatio(GenericMultiply(AsNumerator(z1), AsDenominator(z2)),
                            GenericMultiply(AsDenominator(z1), AsNumerator(z2))));

                return(MakeRatio(AsNumerator(z1), GenericMultiply(AsDenominator(z1), z2)));
            }
            else if (RatioP(z2))
                return(MakeRatio(GenericMultiply(z1, AsDenominator(z2)), AsNumerator(z2)));
            else
            {
                FAssert(BignumP(z1));
                FAssert(BignumP(z2));

                return(MakeRatio(MakeBignum(z1), MakeBignum(z2)));
            }

        case BOP_BIGRAT_COMPLEX:
            return(ComplexDivide(z1, MakeFixnum(0), AsReal(z2), AsImaginary(z2)));

        case BOP_BIGRAT_FLOAT:
        {
            FObject flo = ToInexact(z1);

            FAssert(FlonumP(flo));

            return(MakeFlonum(AsFlonum(flo) / AsFlonum(z2)));
        }

        case BOP_BIGRAT_FIXED:
            if (RatioP(z1))
                return(MakeRatio(AsNumerator(z1), GenericMultiply(AsDenominator(z1), z2)));
            else
                return(MakeRatio(MakeBignum(z1), z2));

        case BOP_COMPLEX_BIGRAT:
            return(MakeComplex(GenericDivide(AsReal(z1), z2),
                    GenericDivide(AsImaginary(z1), z2)));

        case BOP_COMPLEX_COMPLEX:
            return(ComplexDivide(AsReal(z1), AsImaginary(z1), AsReal(z2), AsImaginary(z2)));

        case BOP_COMPLEX_FLOAT:
        case BOP_COMPLEX_FIXED:
            return(MakeComplex(GenericDivide(AsReal(z1), z2),
                    GenericDivide(AsImaginary(z1), z2)));

        case BOP_FLOAT_BIGRAT:
        {
            FObject flo = ToInexact(z2);

            FAssert(FlonumP(flo));

            return(MakeFlonum(AsFlonum(z1) / AsFlonum(flo)));
        }

        case BOP_FLOAT_COMPLEX:
            return(ComplexDivide(z1, MakeFlonum(0.0), AsReal(z2), AsImaginary(z2)));

        case BOP_FLOAT_FLOAT:
            return(MakeFlonum(AsFlonum(z1) / AsFlonum(z2)));

        case BOP_FLOAT_FIXED:
            return(MakeFlonum(AsFlonum(z1) / AsFixnum(z2)));

        case BOP_FIXED_BIGRAT:
            if (RatioP(z2))
                return(MakeRatio(GenericMultiply(AsDenominator(z2), z1), AsNumerator(z2)));
            else
                return(MakeRatio(z1, MakeBignum(z2)));

        case BOP_FIXED_COMPLEX:
            return(ComplexDivide(z1, MakeFixnum(0), AsReal(z2), AsImaginary(z2)));

        case BOP_FIXED_FLOAT:
            return(MakeFlonum(AsFixnum(z1) / AsFlonum(z2)));

        case BOP_FIXED_FIXED:
            return(MakeRatio(z1, z2));

        default:
            FAssert(0);
        }
    }

    NumberArgCheck("/", z1);
    NumberArgCheck("/", z2);

    FAssert(0);
    return(NoValueObject);
}

static FObject GenericExp(FObject z)
{
    if (ComplexP(z))
    {
        FObject x = ToInexact(AsReal(z));
        FObject y = ToInexact(AsImaginary(z));

        FAssert(FlonumP(x));
        FAssert(FlonumP(y));

        double64_t a = AsFlonum(x);
        double64_t b = AsFlonum(y);

        return(MakeComplex(exp(a) * cos(b), exp(a) * sin(b)));
    }

    z = ToInexact(z);

    FAssert(FlonumP(z));

    return(MakeFlonum(exp(AsFlonum(z))));
}

static FObject GenericLog(FObject z)
{
    if (ComplexP(z))
    {
        FObject x = ToInexact(AsReal(z));
        FObject y = ToInexact(AsImaginary(z));

        FAssert(FlonumP(x));
        FAssert(FlonumP(y));

        double64_t a = AsFlonum(x);
        double64_t b = AsFlonum(y);

        return(MakeComplex(log(sqrt(a * a + b * b)), atan2(b, a)));
    }

    z = ToInexact(z);

    FAssert(FlonumP(z));

    if (AsFlonum(z) < 0.0)
        return(GenericLog(MakeComplex(z, MakeFlonum(0.0))));

    return(MakeFlonum(log(AsFlonum(z))));
}

static FObject GenericSine(FObject z)
{
    if (ComplexP(z))
    {
        FObject x = ToInexact(AsReal(z));
        FObject y = ToInexact(AsImaginary(z));

        FAssert(FlonumP(x));
        FAssert(FlonumP(y));

        double64_t a = AsFlonum(x);
        double64_t b = AsFlonum(y);

        return(MakeComplex(sin(a) * cosh(b), cos(a) * sinh(b)));
    }

    z = ToInexact(z);

    FAssert(FlonumP(z));

    return(MakeFlonum(sin(AsFlonum(z))));
}

static FObject GenericCosine(FObject z)
{
    if (ComplexP(z))
    {
        FObject x = ToInexact(AsReal(z));
        FObject y = ToInexact(AsImaginary(z));

        FAssert(FlonumP(x));
        FAssert(FlonumP(y));

        double64_t a = AsFlonum(x);
        double64_t b = AsFlonum(y);

        return(MakeComplex(cos(a) * cosh(b), - sin(a) * sinh(b)));
    }

    z = ToInexact(z);

    FAssert(FlonumP(z));

    return(MakeFlonum(cos(AsFlonum(z))));
}

static FObject GenericTangent(FObject z)
{
    if (ComplexP(z))
    {
        FObject x = ToInexact(AsReal(z));
        FObject y = ToInexact(AsImaginary(z));

        FAssert(FlonumP(x));
        FAssert(FlonumP(y));

        double64_t a = AsFlonum(x);
        double64_t b = AsFlonum(y);

        return(MakeComplex(sin(2 * a) / (cos(2 * a) + cosh(2 * b)),
                sinh(2 * b) / (cos(2 * a) + cosh(2 * b))));
    }

    z = ToInexact(z);

    FAssert(FlonumP(z));

    return(MakeFlonum(tan(AsFlonum(z))));
}

static FObject GenericSqrt(FObject z);
static FObject GenericInverseSine(FObject z)
{
    if (ComplexP(z))
    {
        // -i * log(i * z + sqrt(1 - z^2))

        return(GenericMultiply(MakeComplex(MakeFixnum(0), MakeFixnum(-1)),
                GenericLog(
                    GenericAdd(
                        GenericMultiply(MakeComplex(MakeFixnum(0), MakeFixnum(1)), z),
                        GenericSqrt(GenericSubtract(MakeFixnum(1), GenericMultiply(z, z)))))));
    }

    z = ToInexact(z);

    FAssert(FlonumP(z));

    return(MakeFlonum(asin(AsFlonum(z))));
}

static FObject GenericInverseCosine(FObject z)
{
    if (ComplexP(z))
    {
        // pi / 2 - sin^(-1)(z)

        return(GenericSubtract(MakeFlonum(acos(-1.0) / 2), GenericInverseSine(z)));
    }

    z = ToInexact(z);

    FAssert(FlonumP(z));

    return(MakeFlonum(acos(AsFlonum(z))));
}

static FObject GenericInverseTangent(FObject z)
{
    if (ComplexP(z))
    {
        // (log(1 + i * z) - log(1 - i * z))/(2i)

        FObject i = MakeComplex(MakeFixnum(0), MakeFixnum(1));

        return(GenericDivide(GenericSubtract(
                GenericLog(GenericAdd(MakeFixnum(1), GenericMultiply(i, z))),
                GenericLog((GenericSubtract(MakeFixnum(1), GenericMultiply(i, z))))),
                MakeComplex(MakeFixnum(0), MakeFixnum(2))));
    }

    z = ToInexact(z);

    FAssert(FlonumP(z));

    return(MakeFlonum(atan(AsFlonum(z))));
}

static FObject GenericSqrt(FObject z)
{
    if (ComplexP(z))
    {
        FObject x = ToInexact(AsReal(z));
        FObject y = ToInexact(AsImaginary(z));

        FAssert(FlonumP(x));
        FAssert(FlonumP(y));

        double64_t a = AsFlonum(x);
        double64_t b = AsFlonum(y);
        double64_t r = sqrt(a * a + b * b);

        FAssert(b != 0.0);

        return(MakeComplex(sqrt((a + r) / 2), (b < 0 ? -1 : 1) * sqrt((r - a) / 2)));
    }

    if (BignumP(z))
    {
        FObject rt = MakeBignum();
        FObject rem = MakeBignum();

        BignumSqrt(rt, rem, z);
        if (GenericSign(rem) == 0)
            return(Normalize(rt));

        // Fall through.
    }

    z = ToInexact(z);

    FAssert(FlonumP(z));

    double64_t d = AsFlonum(z);
    if (d < 0)
    {
        double64_t rt = sqrt(- d);
        if (rt == Truncate(rt))
            return(MakeComplex(MakeFlonum(0.0), ToExact(MakeFlonum(rt))));

        return(MakeComplex(0.0, rt));
    }

    double64_t rt = sqrt(d);
    if (rt == Truncate(rt))
        return(ToExact(MakeFlonum(rt)));

    return(MakeFlonum(rt));
}

Define("number?", NumberPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("number?", argc);

    return(NumberP(argv[0]) ? TrueObject : FalseObject);
}

Define("complex?", ComplexPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("complex?", argc);

    return(NumberP(argv[0]) ? TrueObject : FalseObject);
}

Define("real?", RealPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("real?", argc);

    return(RealP(argv[0]) ? TrueObject : FalseObject);
}

int_t RationalP(FObject obj)
{
    if (FlonumP(obj))
        return((isnan(AsFlonum(obj)) || isfinite(AsFlonum(obj)) == 0) ? 0 : 1);

    return(RealP(obj));
}

Define("rational?", RationalPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("rational?", argc);

    return(RationalP(argv[0]) ? TrueObject : FalseObject);
}

Define("integer?", IntegerPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("integer?", argc);

    return(IntegerP(argv[0]) ? TrueObject : FalseObject);
}

static int_t ExactP(FObject obj)
{
    if (ComplexP(obj))
        obj = AsReal(obj);

    return(FixnumP(obj) || BignumP(obj) || RatioP(obj));
}

Define("exact?", ExactPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("exact?", argc);
    NumberArgCheck("exact?", argv[0]);

    return(ExactP(argv[0]) ? TrueObject : FalseObject);
}

Define("inexact?", InexactPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("inexact?", argc);
    NumberArgCheck("inexact?", argv[0]);

    return((FlonumP(argv[0]) || (ComplexP(argv[0]) && FlonumP(AsReal(argv[0]))))
            ? TrueObject : FalseObject);
}

Define("exact-integer?", ExactIntegerPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("exact-integer?", argc);
    NumberArgCheck("exact-integer?", argv[0]);

    return((FixnumP(argv[0]) || BignumP(argv[0])) ? TrueObject : FalseObject);
}

Define("finite?", FinitePPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("finite?", argc);
    NumberArgCheck("finite?", argv[0]);

    if (FlonumP(argv[0]))
        return(isfinite(AsFlonum(argv[0])) ? TrueObject : FalseObject);
    else if (ComplexP(argv[0]) && FlonumP(AsReal(argv[0])))
    {
        FAssert(FlonumP(AsImaginary(argv[0])));

        return((isfinite(AsFlonum(AsReal(argv[0])))
                && isfinite(AsFlonum(AsImaginary(argv[0])))) ? TrueObject : FalseObject);
    }

    return(TrueObject);
}

static inline int_t InfiniteP(double64_t d)
{
    return(isnan(d) == 0 && isfinite(d) == 0);
}

Define("infinite?", InfinitePPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("infinite?", argc);
    NumberArgCheck("infinite?", argv[0]);

    if (FlonumP(argv[0]))
        return(InfiniteP(AsFlonum(argv[0])) ? TrueObject : FalseObject);
    else if (ComplexP(argv[0]) && FlonumP(AsReal(argv[0])))
    {
        FAssert(FlonumP(AsImaginary(argv[0])));

        return((InfiniteP(AsFlonum(AsReal(argv[0])))
                || InfiniteP(AsFlonum(AsImaginary(argv[0])))) ? TrueObject : FalseObject);
    }

    return(FalseObject);
}

Define("nan?", NanPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("nan?", argc);
    NumberArgCheck("nan?", argv[0]);

    if (FlonumP(argv[0]))
        return(isnan(AsFlonum(argv[0])) ? TrueObject : FalseObject);
    else if (ComplexP(argv[0]) && FlonumP(AsReal(argv[0])))
    {
        FAssert(FlonumP(AsImaginary(argv[0])));

        return((isnan(AsFlonum(AsReal(argv[0])))
                || isnan(AsFlonum(AsImaginary(argv[0])))) ? TrueObject : FalseObject);
    }

    return(FalseObject);
}

Define("=", EqualPrimitive)(int_t argc, FObject argv[])
{
    AtLeastTwoArgsCheck("=", argc);

    for (int_t adx = 1; adx < argc; adx++)
        if (GenericCompare("=", argv[adx - 1], argv[adx], 1) != 0)
            return(FalseObject);

    return(TrueObject);
}

Define("<", LessThanPrimitive)(int_t argc, FObject argv[])
{
    AtLeastTwoArgsCheck("<", argc);

    for (int_t adx = 1; adx < argc; adx++)
        if (GenericCompare("<", argv[adx - 1], argv[adx], 0) >= 0)
            return(FalseObject);

    return(TrueObject);
}

Define(">", GreaterThanPrimitive)(int_t argc, FObject argv[])
{
    AtLeastTwoArgsCheck(">", argc);

    for (int_t adx = 1; adx < argc; adx++)
        if (GenericCompare(">", argv[adx - 1], argv[adx], 0) <= 0)
            return(FalseObject);

    return(TrueObject);
}

Define("<=", LessThanEqualPrimitive)(int_t argc, FObject argv[])
{
    AtLeastTwoArgsCheck("<=", argc);

    for (int_t adx = 1; adx < argc; adx++)
        if (GenericCompare("<=", argv[adx - 1], argv[adx], 0) > 0)
            return(FalseObject);

    return(TrueObject);
}

Define(">=", GreaterThanEqualPrimitive)(int_t argc, FObject argv[])
{
    AtLeastTwoArgsCheck(">=", argc);

    for (int_t adx = 1; adx < argc; adx++)
        if (GenericCompare(">=", argv[adx - 1], argv[adx], 0) < 0)
            return(FalseObject);

    return(TrueObject);
}

static int_t ZeroP(FObject obj)
{
    if (FixnumP(obj))
        return(AsFixnum(obj) == 0);
    else if (FlonumP(obj))
        return(AsFlonum(obj) == 0.0);
    else if (ComplexP(obj))
        return(ZeroP(AsReal(obj)) && ZeroP(AsImaginary(obj)));

    return(0);
}

Define("zero?", ZeroPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("zero?", argc);
    NumberArgCheck("zero?", argv[0]);

    return(ZeroP(argv[0]) ? TrueObject : FalseObject);
}

Define("positive?", PositivePPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("positive?", argc);
    RealArgCheck("positive?", argv[0]);

    return(GenericSign(argv[0]) > 0 ? TrueObject : FalseObject);
}

Define("negative?", NegativePPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("negative?", argc);
    RealArgCheck("negative?", argv[0]);

    return(GenericSign(argv[0]) < 0 ? TrueObject : FalseObject);
}

static int_t OddP(FObject obj)
{
    if (FixnumP(obj))
        return(AsFixnum(obj) % 2 != 0);
    else if (FlonumP(obj))
        return(fmod(AsFlonum(obj), 2.0) != 0.0);

    FAssert(BignumP(obj));

    return(BignumRemainderFixnum(obj, 2) != 0);
}

Define("odd?", OddPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("odd?", argc);
    IntegerArgCheck("odd?", argv[0]);

    return(OddP(argv[0]) ? TrueObject : FalseObject);
}

Define("even?", EvenPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("even?", argc);
    IntegerArgCheck("even?", argv[0]);

    return(OddP(argv[0]) ? FalseObject : TrueObject);
}

Define("max", MaxPrimitive)(int_t argc, FObject argv[])
{
    AtLeastOneArgCheck("max", argc);
    RealArgCheck("max", argv[0]);

    FObject max = argv[0];
    int_t ief = FlonumP(argv[0]);

    for (int_t adx = 1; adx < argc; adx++)
    {
        if (FlonumP(argv[adx]))
            ief = 1;

        if (GenericCompare("max", max, argv[adx], 0) < 0)
            max = argv[adx];
    }

    return(ief ? ToInexact(max) : max);
}

Define("min", MinPrimitive)(int_t argc, FObject argv[])
{
    AtLeastOneArgCheck("min", argc);
    RealArgCheck("min", argv[0]);

    FObject min = argv[0];
    int_t ief = FlonumP(argv[0]);

    for (int_t adx = 1; adx < argc; adx++)
    {
        if (FlonumP(argv[adx]))
            ief = 1;

        if (GenericCompare("min", min, argv[adx], 0) > 0)
            min = argv[adx];
    }

    return(ief ? ToInexact(min) : min);
}

Define("+", AddPrimitive)(int_t argc, FObject argv[])
{
    if (argc == 0)
        return(MakeFixnum(0));
    else if (argc == 1)
    {
        NumberArgCheck("+", argv[0]);
        return(argv[0]);
    }

    FObject ret = argv[0];
    for (int_t adx = 1; adx < argc; adx++)
        ret = GenericAdd(ret, argv[adx]);

    return(ret);
}

Define("*", MultiplyPrimitive)(int_t argc, FObject argv[])
{
    if (argc == 0)
        return(MakeFixnum(1));

    NumberArgCheck("*", argv[0]);
    FObject ret = argv[0];

    for (int_t adx = 1; adx < argc; adx++)
    {
        NumberArgCheck("*", argv[adx]);

        ret = GenericMultiply(ret, argv[adx]);
    }

    return(ret);
}

Define("-", SubtractPrimitive)(int_t argc, FObject argv[])
{
    AtLeastOneArgCheck("-", argc);
    NumberArgCheck("-", argv[0]);

    if (argc == 1)
        return(GenericSubtract(MakeFixnum(0), argv[0]));

    FObject ret = argv[0];
    for (int_t adx = 1; adx < argc; adx++)
    {
        NumberArgCheck("-", argv[adx]);

        ret = GenericSubtract(ret, argv[adx]);
    }

    return(ret);
}

Define("/", DividePrimitive)(int_t argc, FObject argv[])
{
    AtLeastOneArgCheck("/", argc);
    NumberArgCheck("/", argv[0]);

    if (argc == 1)
        return(GenericDivide(MakeFixnum(1), argv[0]));

    FObject ret = argv[0];
    for (int_t adx = 1; adx < argc; adx++)
    {
        NumberArgCheck("/", argv[adx]);

        ret = GenericDivide(ret, argv[adx]);
    }

    return(ret);
}

Define("abs", AbsPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("abs", argc);
    RealArgCheck("abs", argv[0]);

    return(Abs(argv[0]));
}

Define("floor-quotient", FloorQuotientPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("floor-quotient", argc);
    IntegerArgCheck("floor-quotient", argv[0]);
    IntegerArgCheck("floor-quotient", argv[1]);

    if (FixnumP(argv[0]) && FixnumP(argv[1]))
    {
        if (GenericSign(argv[0]) * GenericSign(argv[1]) < 0)
            return(MakeFixnum((AsFixnum(argv[0]) / AsFixnum(argv[1])) - 1));

        return(MakeFixnum(AsFixnum(argv[0]) / AsFixnum(argv[1])));
    }

    FObject n = ToBignum(argv[0]);
    FObject d = ToBignum(argv[1]);
    FObject rbn = MakeBignum();

    BignumDivide(rbn, n, d);

    if (GenericSign(argv[0]) * GenericSign(argv[1]) < 0)
        BignumAddFixnum(rbn, rbn, -1);

    return(FlonumP(argv[0]) || FlonumP(argv[1]) ? ToInexact(rbn) : Normalize(rbn));
}

Define("truncate-quotient", TruncateQuotientPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("truncate-quotient", argc);
    IntegerArgCheck("truncate-quotient", argv[0]);
    IntegerArgCheck("truncate-quotient", argv[1]);

    if (FixnumP(argv[0]) && FixnumP(argv[1]))
        return(MakeFixnum(AsFixnum(argv[0]) / AsFixnum(argv[1])));

    FObject n = ToBignum(argv[0]);
    FObject d = ToBignum(argv[1]);
    FObject rbn = MakeBignum();

    BignumDivide(rbn, n, d);
    return(FlonumP(argv[0]) || FlonumP(argv[1]) ? ToInexact(rbn) : Normalize(rbn));
}

static FObject TruncateRemainder(FObject n, FObject d)
{
    if (FixnumP(n) && FixnumP(d))
        return(MakeFixnum(AsFixnum(n) % AsFixnum(d)));

    FObject num = ToBignum(n);
    FObject den = ToBignum(d);
    FObject rbn = MakeBignum();

    BignumRemainder(rbn, num, den);
    return(FlonumP(n) || FlonumP(d) ? ToInexact(rbn) : Normalize(rbn));
}

Define("truncate-remainder", TruncateRemainderPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("truncate-remainder", argc);
    IntegerArgCheck("truncate-remainder", argv[0]);
    IntegerArgCheck("truncate-remainder", argv[1]);

    return(TruncateRemainder(argv[0], argv[1]));
}

static FObject Gcd(FObject a, FObject b)
{
    if (GenericSign(a) == 0)
        return(b);

    if (GenericSign(b) == 0)
        return(a);

    for (;;)
    {
        FAssert(IntegerP(a));
        FAssert(IntegerP(b));
        FAssert(GenericSign(a) > 0);
        FAssert(GenericSign(b) > 0);

        int_t cmp = GenericCompare("gcd", a, b, 0);
        if (cmp == 0)
            break;

        if (cmp > 0)
            a = GenericSubtract(a, b);
        else
            b = GenericSubtract(b, a);
    }

    return(a);
}

Define("gcd", GcdPrimitive)(int_t argc, FObject argv[])
{
    if (argc == 0)
        return(MakeFixnum(0));

    IntegerArgCheck("gcd", argv[0]);

    FObject ret = Abs(argv[0]);
    for (int_t adx = 1; adx < argc; adx++)
    {
        IntegerArgCheck("gcd", argv[adx]);

        ret = Gcd(ret, Abs(argv[adx]));
    }

    return(ret);
}

static FObject Lcm(FObject a, FObject b)
{
    if (GenericSign(a) == 0 && GenericSign(b) == 0)
        return(MakeFixnum(0));

    FAssert(GenericSign(a) >= 0);
    FAssert(GenericSign(b) >= 0);

    return(GenericMultiply(GenericDivide(a, Gcd(a, b)), b));
}

Define("lcm", LcmPrimitive)(int_t argc, FObject argv[])
{
    if (argc == 0)
        return(MakeFixnum(1));

    IntegerArgCheck("lcm", argv[0]);

    FObject ret = Abs(argv[0]);
    for (int_t adx = 1; adx < argc; adx++)
    {
        IntegerArgCheck("lcm", argv[adx]);

        ret = Lcm(ret, Abs(argv[adx]));
    }

    return(ret);
}

Define("numerator", NumeratorPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("numerator", argc);
    RationalArgCheck("numerator", argv[0]);

    FObject obj = argv[0];

    if (FlonumP(obj))
        obj = ToExact(obj);

    if (RatioP(obj))
        return(FlonumP(argv[0]) ? ToInexact(AsNumerator(obj)) : AsNumerator(obj));

    FAssert(FixnumP(obj) || BignumP(obj));

    return(argv[0]);
}

Define("denominator", DenominatorPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("denominator", argc);
    RationalArgCheck("denominator", argv[0]);

    FObject obj = argv[0];

    if (FlonumP(obj))
        obj = ToExact(obj);

    if (RatioP(obj))
        return(FlonumP(argv[0]) ? ToInexact(AsDenominator(obj)) : AsDenominator(obj));

    FAssert(FixnumP(obj) || BignumP(obj));

    return(FlonumP(argv[0]) ? MakeFlonum(1.0) : MakeFixnum(1));
}

Define("floor", FloorPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("floor", argc);
    RealArgCheck("floor", argv[0]);

    if (FlonumP(argv[0]))
        return(MakeFlonum(floor(AsFlonum(argv[0]))));
    else if (RatioP(argv[0]))
    {
        FObject ret = RatioDivide(argv[0]);

        if (GenericSign(ret) < 0)
            return(GenericSubtract(ret, MakeFixnum(1)));

        return(ret);
    }

    return(argv[0]);
}

Define("ceiling", CeilingPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("ceiling", argc);
    RealArgCheck("ceiling", argv[0]);

    if (FlonumP(argv[0]))
        return(MakeFlonum(ceil(AsFlonum(argv[0]))));
    else if (RatioP(argv[0]))
    {
        FObject ret = RatioDivide(argv[0]);

        if (GenericSign(argv[0]) > 0)
            return(GenericAdd(ret, MakeFixnum(1)));

        return(ret);
    }

    return(argv[0]);
}

Define("truncate", TruncatePrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("truncate", argc);
    RealArgCheck("truncate", argv[0]);

    if (FlonumP(argv[0]))
        return(MakeFlonum(Truncate(AsFlonum(argv[0]))));
    else if (RatioP(argv[0]))
        return(RatioDivide(argv[0]));

    return(argv[0]);
}

Define("round", RoundPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("round", argc);
    RealArgCheck("round", argv[0]);

    FObject obj = argv[0];

    if (FlonumP(argv[0]))
        obj = ToExact(argv[0]);

    if (RatioP(obj))
    {
        FObject ret = RatioDivide(obj);

        if (AsDenominator(obj) == MakeFixnum(2) && OddP(ret))
            ret = GenericAdd(ret, GenericSign(ret) > 0 ? MakeFixnum(1) : MakeFixnum(-1));
        else
        {
            FObject tmp = GenericMultiply(TruncateRemainder(AsNumerator(obj),
                    AsDenominator(obj)), MakeFixnum(2));

            if (GenericSign(tmp) < 0)
                tmp = GenericMultiply(tmp, MakeFixnum(-1));

            if (GenericCompare("round", tmp, AsDenominator(obj), 0) > 0)
                ret = GenericAdd(ret, GenericSign(ret) > 0 ? MakeFixnum(1) : MakeFixnum(-1));
        }

        return(FlonumP(argv[0]) ? ToInexact(ret) : ret);
    }

    return(argv[0]);
}

Define("exp", ExpPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("exp", argc);
    NumberArgCheck("exp", argv[0]);

    return(GenericExp(argv[0]));
}

Define("log", LogPrimitive)(int_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("log", argc);
    NumberArgCheck("log", argv[0]);

    if (argc == 1)
        return(GenericLog(argv[0]));

    FAssert(argc == 2);

    NumberArgCheck("log", argv[1]);

    return(GenericDivide(GenericLog(argv[0]), GenericLog(argv[1])));
}

Define("sin", SinPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("sin", argc);
    NumberArgCheck("sin", argv[0]);

    return(GenericSine(argv[0]));
}

Define("cos", CosPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("cos", argc);
    NumberArgCheck("cos", argv[0]);

    return(GenericCosine(argv[0]));
}

Define("tan", TanPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("tan", argc);
    NumberArgCheck("tan", argv[0]);

    return(GenericTangent(argv[0]));
}

Define("asin", ASinPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("asin", argc);
    NumberArgCheck("asin", argv[0]);

    return(GenericInverseSine(argv[0]));
}

Define("acos", ACosPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("acos", argc);
    NumberArgCheck("acos", argv[0]);

    return(GenericInverseCosine(argv[0]));
}

Define("atan", ATanPrimitive)(int_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("atan", argc);

    if (argc == 1)
    {
        NumberArgCheck("atan", argv[0]);

        return(GenericInverseTangent(argv[0]));
    }

    FAssert(argc == 2);

    RealArgCheck("atan", argv[0]);
    RealArgCheck("atan", argv[1]);

    FObject z1 = ToInexact(argv[0]);
    FObject z2 = ToInexact(argv[1]);

    FAssert(FlonumP(z1));
    FAssert(FlonumP(z2));

    return(MakeFlonum(atan2(AsFlonum(z1), AsFlonum(z2))));
}

Define("square", SquarePrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("square", argc);
    NumberArgCheck("square", argv[0]);

    return(GenericMultiply(argv[0], argv[0]));
}

Define("sqrt", SqrtPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("sqrt", argc);
    NumberArgCheck("sqrt", argv[0]);

    return(GenericSqrt(argv[0]));
}

Define("%exact-integer-sqrt", ExactIntegerSqrtPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("exact-integer-sqrt", argc);
    NonNegativeArgCheck("exact-integer-sqrt", argv[0], 1);

    if (FixnumP(argv[0]))
    {
        FFixnum rt = (FFixnum) sqrt((double64_t) AsFixnum(argv[0]));

        return(MakePair(MakeFixnum(rt), MakeFixnum(AsFixnum(argv[0]) - rt * rt)));
    }

    FObject rt = MakeBignum();
    FObject rem = MakeBignum();

    BignumSqrt(rt, rem, argv[0]);
    return(MakePair(Normalize(rt), Normalize(rem)));
}

Define("expt", ExptPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("expt", argc);
    NumberArgCheck("expt", argv[0]);
    NumberArgCheck("expt", argv[1]);

    if ((FixnumP(argv[0]) && AsFixnum(argv[0]) == 0)
            || (FlonumP(argv[0]) && AsFlonum(argv[0]) == 0.0))
    {
        if (ComplexP(argv[1]))
        {
            if (GenericSign(AsReal(argv[1])) > 0)
                return(FlonumP(argv[0]) || FlonumP(AsReal(argv[1])) ? MakeFlonum(0.0)
                        : MakeFixnum(0));
        }
        else
        {
            int_t sgn = GenericSign(argv[1]);

            if (sgn == 0)
                return(FlonumP(argv[0]) || FlonumP(argv[1]) ? MakeFlonum(1.0) : MakeFixnum(1));
            else if (sgn > 0)
                return(FlonumP(argv[0]) || FlonumP(argv[1]) ? MakeFlonum(0.0) : MakeFixnum(0));
        }

        RaiseExceptionC(R.Assertion, "expt",
                "(expt 0 z): z must be zero or have a positive real part", List(argv[1]));
    }
    else if (FixnumP(argv[1]))
    {
        FFixnum e = AsFixnum(argv[1]);
        if (e < 0)
            e = - e;

        if (FixnumP(argv[0]) || BignumP(argv[0]))
        {
            FObject rbn = MakeBignum();
            BignumExpt(rbn, ToBignum(argv[0]), e);
            return(AsFixnum(argv[1]) < 0 ? MakeRatio(MakeFixnum(1), rbn) : Normalize(rbn));
        }

        FObject ret = MakeFixnum(1);
        FObject n = argv[0];

        while (e > 0)
        {
            if (e & 0x1)
                ret = GenericMultiply(ret, n);

            n = GenericMultiply(n, n);
            e = e >> 1;
        }

        if (AsFixnum(argv[1]) < 0)
            ret = GenericDivide(MakeFixnum(1), ret);

        return(FlonumP(argv[0]) ? ToInexact(ret) : ret);
    }
    else if (ComplexP(argv[0]) || ComplexP(argv[1]))
        return(GenericExp(GenericMultiply(argv[1], GenericLog(argv[0]))));

    FObject b = ToInexact(argv[0]);
    FObject e = ToInexact(argv[1]);

    FAssert(FlonumP(b));
    FAssert(FlonumP(e));

    if (AsFlonum(b) < 0.0)
        return(GenericExp(GenericMultiply(argv[1], GenericLog(argv[0]))));

    return(MakeFlonum(pow(AsFlonum(b), AsFlonum(e))));
}

Define("make-rectangular", MakeRectangularPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("make-rectangular", argc);
    RealArgCheck("make-rectangular", argv[0]);
    RealArgCheck("make-rectangular", argv[1]);

    return(MakeComplex(argv[0], argv[1]));
}

Define("make-polar", MakePolarPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("make-polar", argc);
    RealArgCheck("make-polar", argv[0]);
    RealArgCheck("make-polar", argv[1]);

    return(MakePolar(argv[0], argv[1]));
}

Define("real-part", RealPartPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("real-part", argc);
    NumberArgCheck("real-part", argv[0]);

    if (ComplexP(argv[0]))
        return(AsReal(argv[0]));

    return(argv[0]);
}

Define("imag-part", ImagPartPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("imag-part", argc);
    NumberArgCheck("imag-part", argv[0]);

    if (ComplexP(argv[0]))
        return(AsImaginary(argv[0]));
    else if (FlonumP(argv[0]))
        return(MakeFlonum(0.0));

    return(MakeFixnum(0));
}

Define("inexact", InexactPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("inexact", argc);
    NumberArgCheck("inexact", argv[0]);

    return(ToInexact(argv[0]));
}

Define("exact", ExactPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("exact", argc);
    NumberArgCheck("exact", argv[0]);

    return(ToExact(argv[0]));
}

Define("number->string", NumberToStringPrimitive)(int_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("number->string", argc);
    NumberArgCheck("number->string", argv[0]);

    FFixnum rdx = 10;

    if (argc == 2)
    {
        if (FixnumP(argv[1]) == 0 || (AsFixnum(argv[1]) != 2 && AsFixnum(argv[1]) != 8
                && AsFixnum(argv[1]) != 10 && AsFixnum(argv[1]) != 16))
            RaiseExceptionC(R.Assertion, "number->string", "expected radix of 2, 8, 10, or 16",
                    List(argv[1]));

        rdx = AsFixnum(argv[1]);
    }

    return(NumberToString(argv[0], rdx));
}

Define("string->number", StringToNumberPrimitive)(int_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("string->number", argc);
    StringArgCheck("string->number", argv[0]);

    FFixnum rdx = 10;

    if (argc == 2)
    {
        if (FixnumP(argv[1]) == 0 || (AsFixnum(argv[1]) != 2 && AsFixnum(argv[1]) != 8
                && AsFixnum(argv[1]) != 10 && AsFixnum(argv[1]) != 16))
            RaiseExceptionC(R.Assertion, "string->number", "expected radix of 2, 8, 10, or 16",
                    List(argv[1]));

        rdx = AsFixnum(argv[1]);
    }

    return(StringToNumber(AsString(argv[0])->String, StringLength(argv[0]), rdx));
}

// ---- SRFI 60: Integers As Bits ----

Define("bitwise-and", BitwiseAndPrimitive)(int_t argc, FObject argv[])
{
    if (argc == 0)
        return(MakeFixnum(-1));

    IntegerArgCheck("bitwise-and", argv[0]);
    FObject ret = BignumP(argv[0]) ? MakeBignum(argv[0]) : argv[0];

    for (int_t adx = 1; adx < argc; adx++)
    {
        IntegerArgCheck("bitwise-and", argv[adx]);

        if (BignumP(ret) || BignumP(argv[adx]))
        {
            ret = ToBignum(ret);
            BignumAnd(ret, ret, ToBignum(argv[adx]));
        }
        else
        {
            FAssert(FixnumP(ret));
            FAssert(FixnumP(argv[adx]));

            ret = MakeFixnum(AsFixnum(ret) & AsFixnum(argv[adx]));
        }
    }

    return(Normalize(ret));
}

Define("bitwise-ior", BitwiseIOrPrimitive)(int_t argc, FObject argv[])
{
    if (argc == 0)
        return(MakeFixnum(0));

    IntegerArgCheck("bitwise-ior", argv[0]);
    FObject ret = BignumP(argv[0]) ? MakeBignum(argv[0]) : argv[0];

    for (int_t adx = 1; adx < argc; adx++)
    {
        IntegerArgCheck("bitwise-ior", argv[adx]);

        if (BignumP(ret) || BignumP(argv[adx]))
        {
            ret = ToBignum(ret);
            BignumIOr(ret, ret, ToBignum(argv[adx]));
        }
        else
        {
            FAssert(FixnumP(ret));
            FAssert(FixnumP(argv[adx]));

            ret = MakeFixnum(AsFixnum(ret) | AsFixnum(argv[adx]));
        }
    }

    return(Normalize(ret));
}

Define("bitwise-xor", BitwiseXOrPrimitive)(int_t argc, FObject argv[])
{
    if (argc == 0)
        return(MakeFixnum(0));

    IntegerArgCheck("bitwise-xor", argv[0]);
    FObject ret = BignumP(argv[0]) ? MakeBignum(argv[0]) : argv[0];

    for (int_t adx = 1; adx < argc; adx++)
    {
        IntegerArgCheck("bitwise-xor", argv[adx]);

        if (BignumP(ret) || BignumP(argv[adx]))
        {
            ret = ToBignum(ret);
            BignumXOr(ret, ret, ToBignum(argv[adx]));
        }
        else
        {
            FAssert(FixnumP(ret));
            FAssert(FixnumP(argv[adx]));

            ret = MakeFixnum(AsFixnum(ret) ^ AsFixnum(argv[adx]));
        }
    }

    return(Normalize(ret));
}

Define("bitwise-not", BitwiseNotPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("bitwise-not", argc);
    IntegerArgCheck("bitwise-not", argv[0]);

    if (BignumP(argv[0]))
    {
        FObject ret = MakeBignum();
        BignumNot(ret, argv[0]);
        return(Normalize(ret));
    }

    return(MakeFixnum(~AsFixnum(argv[0])));
}


/*
BitCount was adapted from Chibi Scheme which adapted it from
http://graphics.stanford.edu/~seander/bithacks.html
*/
static uint_t BitCount(uint_t n)
{
    n -= ((n >> 1) & (uint_t) ~(uint_t)0/3);
    n = ((n & (uint_t) ~(uint_t)0/15*3) + ((n >> 2) & (uint_t) ~(uint_t)0/15*3));
    n = (n + (n >> 4)) & (uint_t) ~(uint_t)0/255*15;

    return ((uint_t)(n * ((uint_t) ~(uint_t)0/255)) >> (sizeof(n) - 1) * 8);
}

Define("bit-count", BitCountPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("bit-count", argc);
    IntegerArgCheck("bit-count", argv[0]);

    if (BignumP(argv[0]))
        return(MakeFixnum(BignumBitCount(argv[0])));

    int_t n = AsFixnum(argv[0]);
    return(MakeFixnum(BitCount(n < 0 ? ~n : n)));
}

Define("integer-length", IntegerLengthPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("integer-length", argc);
    IntegerArgCheck("integer-length", argv[0]);

    if (BignumP(argv[0]))
        return(MakeFixnum(BignumIntegerLength(argv[0])));

    uint_t n = AsFixnum(argv[0]);
    for (int_t idx = sizeof(uint_t) * 8 - 1; idx >= 0; idx--)
        if (n & ((uint_t) 1 << idx))
            return(MakeFixnum(idx + 1));

    return(MakeFixnum(0));
}

Define("first-set-bit", FirstSetBitPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("first-set-bit", argc);
    IntegerArgCheck("first-set-bit", argv[0]);

    if (BignumP(argv[0]))
        return(MakeFixnum(BignumFirstSetBit(argv[0])));

    uint_t n = AsFixnum(argv[0]);
    for (uint_t idx = 0; idx < sizeof(uint_t) * 8; idx++)
        if (n & ((uint_t) 1 << idx))
            return(MakeFixnum(idx));

    return(MakeFixnum(-1));
}

Define("arithmetic-shift", ArithmeticShiftPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("arithmetic-shift", argc);
    IntegerArgCheck("arithmetic-shift", argv[0]);
    FixnumArgCheck("arithmetic-shift", argv[1]);

    FFixnum cnt = AsFixnum(argv[1]);
    if (cnt == 0)
        return(argv[0]);

    if (FixnumP(argv[0]))
    {
        if (cnt < 0)
            return(MakeFixnum((int64_t) AsFixnum(argv[0]) >> -cnt));

        int64_t n = AsFixnum(argv[0]) << cnt;
        if ((n >> cnt) == AsFixnum(argv[0]) && n >= MINIMUM_FIXNUM && n <= MAXIMUM_FIXNUM)
            return(MakeFixnum(n));
    }

    return(BignumArithmeticShift(ToBignum(argv[0]), cnt));
}

Define("number-compare", NumberComparePrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("number-compare", argc);
    NumberArgCheck("number-compare", argv[0]);
    NumberArgCheck("number-compare", argv[1]);

    return(MakeFixnum(NumberCompare(argv[0], argv[1])));
}

Define("number-hash", NumberHashPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("number-hash", argc);
    NumberArgCheck("number-hash", argv[0]);

    return(MakeFixnum(NumberHash(argv[0]) & MAXIMUM_FIXNUM));
}

static FPrimitive * Primitives[] =
{
    &NumberPPrimitive,
    &ComplexPPrimitive,
    &RealPPrimitive,
    &RationalPPrimitive,
    &IntegerPPrimitive,
    &ExactPPrimitive,
    &InexactPPrimitive,
    &ExactIntegerPPrimitive,
    &FinitePPrimitive,
    &InfinitePPrimitive,
    &NanPPrimitive,
    &EqualPrimitive,
    &LessThanPrimitive,
    &GreaterThanPrimitive,
    &LessThanEqualPrimitive,
    &GreaterThanEqualPrimitive,
    &ZeroPPrimitive,
    &PositivePPrimitive,
    &NegativePPrimitive,
    &OddPPrimitive,
    &EvenPPrimitive,
    &MaxPrimitive,
    &MinPrimitive,
    &AddPrimitive,
    &MultiplyPrimitive,
    &SubtractPrimitive,
    &DividePrimitive,
    &AbsPrimitive,
    &FloorQuotientPrimitive,
    &TruncateQuotientPrimitive,
    &TruncateRemainderPrimitive,
    &GcdPrimitive,
    &LcmPrimitive,
    &NumeratorPrimitive,
    &DenominatorPrimitive,
    &FloorPrimitive,
    &CeilingPrimitive,
    &TruncatePrimitive,
    &RoundPrimitive,
    &ExpPrimitive,
    &LogPrimitive,
    &SinPrimitive,
    &CosPrimitive,
    &TanPrimitive,
    &ASinPrimitive,
    &ACosPrimitive,
    &ATanPrimitive,
    &SquarePrimitive,
    &SqrtPrimitive,
    &ExactIntegerSqrtPrimitive,
    &ExptPrimitive,
    &MakeRectangularPrimitive,
    &MakePolarPrimitive,
    &RealPartPrimitive,
    &ImagPartPrimitive,
    &InexactPrimitive,
    &ExactPrimitive,
    &NumberToStringPrimitive,
    &StringToNumberPrimitive,
    &BitwiseAndPrimitive,
    &BitwiseIOrPrimitive,
    &BitwiseXOrPrimitive,
    &BitwiseNotPrimitive,
    &BitCountPrimitive,
    &IntegerLengthPrimitive,
    &FirstSetBitPrimitive,
    &ArithmeticShiftPrimitive,
    &NumberComparePrimitive,
    &NumberHashPrimitive
};

void SetupNumbers()
{
    FAssert(sizeof(double64_t) == 8);

    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
