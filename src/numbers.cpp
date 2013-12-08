/*

Foment

z: complex
x: real
n: integer

-- StringToNumber: need another routine for string->number which checks for radix and exactness
    prefixes
<num> : <complex>
      | <radix> <complex>
      | <exactness> <complex>
      | <radix> <exactness> <complex>
      | <exactness> <radix> <complex>
<radix> : #b | #o | #d | #x
<exactness> : #i | #e

-- check all FAssert(FixnumP(
-- check all BIGRAT_* and *_BIGRAT
-- need guardian for Bignums to free the MPInteger
-- on unix, if gmp is available, use it instead of mini-gmp
-- Normalize: checks for Bignums to convert to Fixnums; and num and den of Ratnums; and
Complex with no imaginary part; and make sure that sign of Ratnum is in num and den is positive
-- handle case of Ratnum num/0
-- ParseComplex: <real> @ <real>

*/

#include <malloc.h>
#include <math.h>
#include <float.h>
#include <stdio.h>
#include "foment.hpp"
#include "unicode.hpp"
#include "numbers.hpp"

#ifdef FOMENT_WINDOWS
#define finite _finite
#define isnan _isnan
#endif // FOMENT_WINDOWS

static FObject MakeBignum()
{
    FBignum * bn = (FBignum *) MakeObject(sizeof(FBignum), BignumTag);
    bn->Reserved = BignumTag;
    mpz_init(bn->MPInteger);

    return(bn);
}

static FObject MakeBignum(FFixnum n)
{
    FBignum * bn = (FBignum *) MakeObject(sizeof(FBignum), BignumTag);
    bn->Reserved = BignumTag;
    mpz_init_set_si(bn->MPInteger, (long) n);

    return(bn);
}

static FObject MakeBignum(double64_t d)
{
    FBignum * bn = (FBignum *) MakeObject(sizeof(FBignum), BignumTag);
    bn->Reserved = BignumTag;
    mpz_init_set_d(bn->MPInteger, d);

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

inline static void BignumModulo(FObject rbn, FObject n, FObject d)
{
    FAssert(BignumP(rbn));
    FAssert(BignumP(n));
    FAssert(BignumP(d));

    mpz_mod(AsBignum(rbn), AsBignum(n), AsBignum(d));
}

inline static FFixnum BignumModuloFixnum(FObject n, FFixnum d)
{
    FAssert(BignumP(n));

    return(mpz_fdiv_ui(AsBignum(n), (unsigned long) d));
}

inline static int_t BignumEqualFixnum(FObject bn, FFixnum n)
{
    FAssert(BignumP(bn));

    return(mpz_cmp_si(AsBignum(bn), (long) n) == 0);
}

inline static FObject Normalize(FObject num)
{
    if (BignumP(num) && mpz_cmp_si(AsBignum(num), (long) MINIMUM_FIXNUM) >= 0
            && mpz_cmp_si(AsBignum(num), (long) MAXIMUM_FIXNUM) <= 0)
        return(MakeFixnum(mpz_get_si(AsBignum(num))));

    return(num);
}

static FObject MakeFlonum(double64_t dbl)
{
    FFlonum * flo = (FFlonum *) MakeObject(sizeof(FFlonum), FlonumTag);
    flo->Double = dbl;

    FObject obj = FlonumObject(flo);
    FAssert(FlonumP(obj));
    FAssert(isnan(dbl) || AsFlonum(obj) == dbl);

    return(obj);
}

#define AsNumerator(z) AsRatnum(z)->Numerator
#define AsDenominator(z) AsRatnum(z)->Denominator

static FObject MakeRatnum(FObject nmr, FObject dnm)
{
    FAssert(FixnumP(nmr) || BignumP(nmr));
    FAssert(FixnumP(dnm) || BignumP(dnm));
    FAssert(AsFixnum(dnm) != 0);

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
    }
    else
    {
        if (FixnumP(nmr))
            nmr = MakeBignum(AsFixnum(nmr));
        if (FixnumP(dnm))
            dnm = MakeBignum(AsFixnum(dnm));

        FObject n = nmr;
        FObject d = dnm;

        while (BignumEqualFixnum(d, 0) == 0)
        {
            FObject t = MakeBignum();
            BignumModulo(t, n, d);
            n = d;
            d = t;
        }

        FAssert(n != dnm);

        BignumDivide(nmr, nmr, n);
        BignumDivide(dnm, dnm, n);

        if (BignumEqualFixnum(dnm, 1))
            return(Normalize(nmr));
    }

    FRatnum * rat = (FRatnum *) MakeObject(sizeof(FRatnum), RatnumTag);
    rat->Numerator = Normalize(nmr);
    rat->Denominator = Normalize(dnm);

    FObject obj = RatnumObject(rat);
    FAssert(RatnumP(obj));
    FAssert(AsRatnum(obj) == rat);

    return(obj);
}

#define AsReal(z) AsComplex(z)->Real
#define AsImaginary(z) AsComplex(z)->Imaginary

static FObject MakeComplex(FObject rl, FObject img)
{
    FAssert(FixnumP(rl) || BignumP(rl) || FlonumP(rl) || RatnumP(rl));
    FAssert(FixnumP(img) || BignumP(img) || FlonumP(img) || RatnumP(img));

    if (FlonumP(img))
    {
        if (AsFlonum(img) == 0.0)
            return(rl);

        rl = ToInexact(rl);
    }
    else if (FlonumP(rl))
        img = ToInexact(img);
    else if (FixnumP(img))
    {
        if (AsFixnum(img) == 0)
            return(rl);
    }

    FComplex * cmplx = (FComplex *) MakeObject(sizeof(FComplex), ComplexTag);
    cmplx->Real = rl;
    cmplx->Imaginary = img;

    FObject obj = ComplexObject(cmplx);
    FAssert(ComplexP(obj));
    FAssert(AsComplex(obj) == cmplx);

    return(obj);
}

FObject ToInexact(FObject n)
{
    if (FixnumP(n))
        return(MakeFlonum((double64_t) AsFixnum(n)));
    else if (RatnumP(n))
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
    }
    else if (BignumP(n))
        return(MakeFlonum(BignumToDouble(n)));

    FAssert(FlonumP(n));
    return(n);
}

static inline double64_t Truncate(double64_t n)
{
    return(floor(n + 0.5 * ((n < 0 ) ? 1 : 0)));
}

static inline FFixnum TensDigit(double64_t n)
{
    return((FFixnum) (n - (Truncate(n / 10.0) * 10.0)));
}

static FObject GenericMultiply(FObject z1, FObject z2);
static FObject GenericSubtract(FObject z1, FObject z2);
static FObject GenericAdd(FObject z1, FObject z2);

FObject ToExact(FObject n)
{
    if (FlonumP(n))
    {
        double64_t d = AsFlonum(n);

        if (isnan(d) || finite(d) == 0)
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

        for (int_t idx = 0; d != Truncate(d) && idx < 15; idx++)
        {
            BignumMultiplyFixnum(rbn, rbn, 10);
            d *= 10;
            BignumAddFixnum(rbn, rbn, TensDigit(d));
            d = d - Truncate(d);
            scl = GenericMultiply(scl, MakeFixnum(10));
        }

        BignumMultiplyFixnum(rbn, rbn, sgn);

        return(GenericAdd(MakeRatnum(rbn, scl), whl));
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

    FAssert(FixnumP(n) || BignumP(n) || RatnumP(n));

    return(n);
}

static int_t ParseBignum(FCh * s, int_t sl, int_t sdx, FFixnum rdx, FFixnum sgn, FFixnum n,
    FObject * punt)
{
    FAssert(n > 0);

    FObject bn = MakeBignum(sgn * n);

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

    FAssert(FixnumP(whl));
    FAssert(s[sdx] == '.' || s[sdx] == 'e' || s[sdx] == 'E');

    double64_t d = (double64_t) AsFixnum(whl);

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

        *purl = MakeRatnum(*purl, dnm);
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

FObject StringToNumber(FCh * s, int_t sl, FFixnum rdx)
{
    FAssert(rdx == 2 || rdx == 8 || rdx == 10 || rdx == 16);

    if (sl == 0)
        return(FalseObject);

    FObject n;

    if (ParseComplex(s, sl, rdx, &n) < 0)
        return(FalseObject);

    return(n);
}

const static char Digits[] = {"0123456789ABCDEF"};

int_t FixnumAsString(FFixnum n, FCh * s, FFixnum rdx)
{
    FAssert(rdx <= sizeof(Digits));

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
    else if (RatnumP(n))
        return(NeedImaginaryPlusSignP(AsRatnum(n)->Numerator));
    else if (FlonumP(n))
        return(isnan(AsFlonum(n)) == 0 && finite(AsFlonum(n)) != 0 && AsFlonum(n) >= 0.0);

    FAssert(BignumP(n));

    return(mpz_cmp_si(AsBignum(n), 0) >= 0);
}

static void WriteNumber(FObject port, FObject obj, FFixnum rdx)
{
    FAssert(NumberP(obj));

    if (FixnumP(obj))
    {
        FCh s[16];
        int_t sl = FixnumAsString(AsFixnum(obj), s, 10);

        WriteString(port, s, sl);
    }
    else if (RatnumP(obj))
    {
        WriteNumber(port, AsRatnum(obj)->Numerator, rdx);
        WriteCh(port, '/');
        WriteNumber(port, AsRatnum(obj)->Denominator, rdx);
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
        double64_t d = AsFlonum(obj);

        if (isnan(d))
            WriteStringC(port, "+nan.0");
        else if (finite(d) == 0)
            WriteStringC(port, d > 0 ? "+inf.0" : "-inf.0");
        else
        {
            char s[64];
            sprintf_s(s, sizeof(s), "%g", d);
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
        FCh s[16];
        int_t sl = FixnumAsString(AsFixnum(obj), s, 10);

        return(MakeString(s, sl));
    }
    else if (FlonumP(obj))
    {
        double64_t d = AsFlonum(obj);

        if (isnan(d))
            return(MakeStringC("+nan.0"));
        else if (finite(d) == 0)
            return(MakeStringC(d > 0 ? "+inf.0" : "-inf.0"));
        else
        {
            char s[64];
            sprintf_s(s, sizeof(s), "%g", d);
            return(MakeStringC(s));
        }
    }

    FAssert(RatnumP(obj) || ComplexP(obj) || BignumP(obj));

    FObject port = MakeStringOutputPort();
    WriteNumber(port, obj, rdx);
    return(GetOutputString(port));
}

/*
This code assumes a very specific layout of immediate type tags: Fixnums, Ratnums, Complex,
and Flonum all have immediate type tags with 0x4 set. No other immediate type tags have 0x4 set.

In determining the operation, Bignums and Ratnums end up together. Both have 0x0 as their
low two bits.
*/
static inline int_t BothNumberP(FObject z1, FObject z2)
{
    return(((((FImmediate) (z1)) & 0x4) + (((FImmediate) (z2)) & 0x4)) == 0x8 ||
            (NumberP(z1) && NumberP(z2)));
}

#define BinaryNumberOp(z1, z2) ((((FImmediate) (z1)) & 0x3) << 2) | (((FImmediate) (z2)) & 0x3)

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

#define UnaryNumberOp(z) (((FImmediate) (z)) & 0x3)

static const int_t UOP_BIGRAT = 0x0;  // 0b0000
static const int_t UOP_COMPLEX = 0x1; // 0b0001
static const int_t UOP_FLOAT = 0x2;   // 0b0010
static const int_t UOP_FIXED = 0x3;   // 0b0011

static int_t GenericSign(FObject x)
{
    switch(UnaryNumberOp(x))
    {
        case UOP_BIGRAT:
            if (RatnumP(x))
                return(GenericSign(AsNumerator(x)));
            else
                return(BignumSign(x));

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

static int_t GenericCompare(char * who, FObject x1, FObject x2, int_t cf)
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
        case BOP_BIGRAT_FIXED:
            return(GenericSign(GenericSubtract(x1, x2)));

        case BOP_COMPLEX_BIGRAT:
        case BOP_COMPLEX_COMPLEX:
        case BOP_COMPLEX_FLOAT:
        case BOP_COMPLEX_FIXED:
            break;

        case BOP_FLOAT_BIGRAT:
            return(GenericSign(GenericSubtract(x1, x2)));

        case BOP_FLOAT_COMPLEX:
            break;

        case BOP_FLOAT_FLOAT:
        {
            double64_t n = AsFlonum(x1) - AsFlonum(x2);
            return(n > 0.0 ? 1 : (n < 0.0 ? -1 : 0));
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

    if (cf)
    {
        NumberArgCheck(who, x1);
        NumberArgCheck(who, x2);

        FObject r1 = ComplexP(x1) ? AsReal(x1) : x1;
        FObject i1 = ComplexP(x1) ? AsImaginary(x1) : MakeFixnum(0);
        FObject r2 = ComplexP(x2) ? AsReal(x2) : x2;
        FObject i2 = ComplexP(x2) ? AsImaginary(x2) : MakeFixnum(0);

        int_t ret = GenericCompare(who, r1, r2, 0);
        return(ret == 0 ? GenericCompare(who, i1, i2, 0) : ret);
    }
    else
    {
        RealArgCheck(who, x1);
        RealArgCheck(who, x2);
    }

    FAssert(0);

    return(0);
}

static FObject GenericAdd(FObject z1, FObject z2)
{
    if (BothNumberP(z1, z2))
    {
        switch (BinaryNumberOp(z1, z2))
        {
        case BOP_BIGRAT_BIGRAT:
            if (RatnumP(z1))
            {
                if (RatnumP(z2))
                    return(MakeRatnum(GenericAdd(
                            GenericMultiply(AsNumerator(z1), AsDenominator(z2)),
                            GenericMultiply(AsNumerator(z2), AsDenominator(z1))),
                            GenericMultiply(AsDenominator(z1), AsDenominator(z2))));

                return(MakeRatnum(GenericAdd(
                        GenericMultiply(AsDenominator(z1), z2), AsNumerator(z1)),
                        AsDenominator(z1)));
            }
            else if (RatnumP(z2))
                return(MakeRatnum(GenericAdd(
                        GenericMultiply(AsDenominator(z2), z1), AsNumerator(z2)),
                        AsDenominator(z2)));
            else
            {
                FObject rbn = MakeBignum();
                BignumAdd(rbn, z1, z2);
                return(rbn);
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
            if (RatnumP(z1))
                return(MakeRatnum(GenericAdd(AsNumerator(z1),
                        GenericMultiply(AsDenominator(z1), z2)), AsDenominator(z1)));
            else
            {
                FAssert(BignumP(z1));

                FObject rbn = MakeBignum();
                BignumAddFixnum(rbn, z1, AsFixnum(z2));
                return(rbn);
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
            if (RatnumP(z2))
                return(MakeRatnum(GenericAdd(AsNumerator(z2),
                        GenericMultiply(AsDenominator(z2), z1)), AsDenominator(z2)));
            else
            {
                FAssert(BignumP(z2));
                FObject rbn = MakeBignum();
                BignumAddFixnum(rbn, z2, AsFixnum(z1));
                return(rbn);
            }

        case BOP_FIXED_COMPLEX:
            return(MakeComplex(GenericAdd(z1, AsReal(z2)), AsImaginary(z2)));

        case BOP_FIXED_FLOAT:
            return(MakeFlonum(AsFixnum(z1) + AsFlonum(z2)));

        case BOP_FIXED_FIXED:
        {
            int64_t n = AsFixnum(z1) + AsFixnum(z2);
            if (n < MINIMUM_FIXNUM || n > MAXIMUM_FIXNUM)
                printf("Number too big or small\n");

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
            if (RatnumP(z1))
            {
                if (RatnumP(z2))
                    return(MakeRatnum(GenericMultiply(AsNumerator(z1), AsNumerator(z2)),
                            GenericMultiply(AsDenominator(z1), AsDenominator(z2))));

                return(MakeRatnum(GenericMultiply(AsNumerator(z1), z2), AsDenominator(z1)));
            }
            else if (RatnumP(z2))
                return(MakeRatnum(GenericMultiply(AsNumerator(z2), z1), AsDenominator(z2)));
            else
            {
                FObject rbn = MakeBignum();
                BignumMultipy(rbn, z1, z2);
                return(rbn);
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
            if (RatnumP(z1))
                return(MakeRatnum(GenericMultiply(AsNumerator(z1), z2), AsDenominator(z1)));
            else
            {
                FAssert(BignumP(z1));

                FObject rbn = MakeBignum();
                BignumMultiplyFixnum(rbn, z1, AsFixnum(z2));
                return(rbn);
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
            FAssert(FixnumP(AsNumerator(z2)));
            FAssert(FixnumP(AsDenominator(z2)));

            return(MakeRatnum(MakeFixnum(AsFixnum(z1) * AsFixnum(AsNumerator(z2))),
                    AsDenominator(z2)));

        case BOP_FIXED_COMPLEX:
            return(MakeComplex(GenericMultiply(z1, AsReal(z2)),
                    GenericMultiply(z1, AsImaginary(z2))));

        case BOP_FIXED_FLOAT:
            return(MakeFlonum(AsFixnum(z1) * AsFlonum(z2)));

        case BOP_FIXED_FIXED:
        {
            int64_t n = AsFixnum(z1) * AsFixnum(z2);
            if (n < MINIMUM_FIXNUM || n > MAXIMUM_FIXNUM)
                printf("number too big or small\n");

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
            if (RatnumP(z1))
            {
                if (RatnumP(z2))
                    return(MakeRatnum(GenericSubtract(
                            GenericMultiply(AsNumerator(z1), AsDenominator(z2)),
                            GenericMultiply(AsNumerator(z2), AsDenominator(z1))),
                            GenericMultiply(AsDenominator(z1), AsDenominator(z2))));

                return(MakeRatnum(GenericSubtract(
                        GenericMultiply(AsDenominator(z1), z2), AsNumerator(z1)),
                        AsDenominator(z1)));
            }
            else if (RatnumP(z2))
                return(MakeRatnum(GenericSubtract(
                        GenericMultiply(z1, AsDenominator(z2)), AsNumerator(z2)),
                        AsDenominator(z2)));
            else
            {
                FObject rbn = MakeBignum();
                BignumSubtract(rbn, z1, z2);
                return(rbn);
            }

        case BOP_BIGRAT_COMPLEX:
            return(MakeComplex(GenericSubtract(z1, AsReal(z2)), AsImaginary(z2)));

        case BOP_BIGRAT_FLOAT:
        {
            FObject flo = ToInexact(z1);

            FAssert(FlonumP(flo));

            return(MakeFlonum(AsFlonum(flo) - AsFlonum(z2)));
        }

        case BOP_BIGRAT_FIXED:
            FAssert(FixnumP(AsNumerator(z1)));
            FAssert(FixnumP(AsDenominator(z1)));

            return(MakeRatnum(MakeFixnum(AsFixnum(AsNumerator(z1)) - AsFixnum(z2)
                    * AsFixnum(AsDenominator(z1))), AsDenominator(z1)));

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
            return(MakeComplex(GenericSubtract(z1, AsReal(z2)), AsImaginary(z2)));

        case BOP_FLOAT_FLOAT:
            return(MakeFlonum(AsFlonum(z1) - AsFlonum(z2)));

        case BOP_FLOAT_FIXED:
            return(MakeFlonum(AsFlonum(z1) - AsFixnum(z2)));

        case BOP_FIXED_BIGRAT:
            FAssert(FixnumP(AsNumerator(z2)));
            FAssert(FixnumP(AsDenominator(z2)));

            return(MakeRatnum(MakeFixnum(AsFixnum(z1) * AsFixnum(AsDenominator(z2))
                    - AsFixnum(AsNumerator(z2))), AsDenominator(z2)));

        case BOP_FIXED_COMPLEX:
            return(MakeComplex(GenericSubtract(z1, AsReal(z2)), AsImaginary(z2)));

        case BOP_FIXED_FLOAT:
            return(MakeFlonum(AsFixnum(z1) - AsFlonum(z2)));

        case BOP_FIXED_FIXED:
        {
            int64_t n = AsFixnum(z1) - AsFixnum(z2);
            if (n < MINIMUM_FIXNUM || n > MAXIMUM_FIXNUM)
                printf("Number too big or small\n");

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

static FObject GenericDivide(FObject z1, FObject z2)
{
    if (BothNumberP(z1, z2))
    {
        switch (BinaryNumberOp(z1, z2))
        {
        case BOP_BIGRAT_BIGRAT:
            if (RatnumP(z1))
            {
                if (RatnumP(z2))
                    return(MakeRatnum(GenericMultiply(AsNumerator(z1), AsDenominator(z2)),
                            GenericMultiply(AsDenominator(z1), AsNumerator(z2))));

                return(MakeRatnum(AsNumerator(z1), GenericMultiply(AsDenominator(z1), z2)));
            }
            else if (RatnumP(z2))
                return(MakeRatnum(AsNumerator(z2), GenericMultiply(z1, AsDenominator(z2))));
            else
            {
                FObject rbn = MakeBignum();
                BignumDivide(rbn, z1, z2);
                return(rbn);
            }

        case BOP_BIGRAT_COMPLEX:
            return(MakeComplex(GenericDivide(z1, AsReal(z2)),
                    GenericDivide(z1, AsImaginary(z2))));

        case BOP_BIGRAT_FLOAT:
        {
            FObject flo = ToInexact(z1);

            FAssert(FlonumP(flo));

            return(MakeFlonum(AsFlonum(flo) / AsFlonum(z2)));
        }

        case BOP_BIGRAT_FIXED:
            FAssert(FixnumP(AsNumerator(z1)));
            FAssert(FixnumP(AsDenominator(z1)));

            return(MakeRatnum(AsNumerator(z1),
                    MakeFixnum(AsFixnum(AsDenominator(z1)) * AsFixnum(z2))));

        case BOP_COMPLEX_BIGRAT:
            return(MakeComplex(GenericDivide(AsReal(z1), z2),
                    GenericDivide(AsImaginary(z1), z2)));

        case BOP_COMPLEX_COMPLEX:
        {
            FObject cpx = MakeComplex(AsReal(z2), GenericSubtract(MakeFixnum(0), AsImaginary(z2)));

            FAssert(ComplexP(GenericMultiply(z2, cpx)) == 0);

            return(GenericDivide(GenericMultiply(z1, cpx), GenericMultiply(z2, cpx)));
        }

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
            return(MakeComplex(GenericDivide(z1, AsReal(z2)),
                    GenericDivide(z1, AsImaginary(z2))));

        case BOP_FLOAT_FLOAT:
            return(MakeFlonum(AsFlonum(z1) / AsFlonum(z2)));

        case BOP_FLOAT_FIXED:
            return(MakeFlonum(AsFlonum(z1) / AsFixnum(z2)));

        case BOP_FIXED_BIGRAT:
            FAssert(FixnumP(AsNumerator(z2)));
            FAssert(FixnumP(AsDenominator(z2)));

            return(MakeRatnum(AsNumerator(z2),
                    MakeFixnum(AsFixnum(z1) * AsFixnum(AsDenominator(z2)))));

        case BOP_FIXED_COMPLEX:
            return(MakeComplex(GenericDivide(z1, AsReal(z2)),
                    GenericDivide(z1, AsImaginary(z2))));

        case BOP_FIXED_FLOAT:
            return(MakeFlonum(AsFixnum(z1) / AsFlonum(z2)));

        case BOP_FIXED_FIXED:
            return(MakeRatnum(z1, z2));

        default:
            FAssert(0);
        }
    }

    NumberArgCheck("/", z1);
    NumberArgCheck("/", z2);

    FAssert(0);
    return(NoValueObject);
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

Define("rational?", RationalPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("rational?", argc);

    if (FlonumP(argv[0]))
        return((isnan(AsFlonum(argv[0])) || finite(AsFlonum(argv[0])) == 0) ? FalseObject :
                TrueObject);

    return(RealP(argv[0]) ? TrueObject : FalseObject);
}

Define("integer?", IntegerPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("integer?", argc);

    if (FlonumP(argv[0]))
        return(AsFlonum(argv[0]) == Truncate(AsFlonum(argv[0])) ? TrueObject : FalseObject);

    return((FixnumP(argv[0]) || BignumP(argv[0])) ? TrueObject : FalseObject);
}

static int_t ExactP(FObject obj)
{
    if (ComplexP(obj))
        obj = AsReal(obj);

    return(FixnumP(obj) || BignumP(obj) || RatnumP(obj));
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
        return(finite(AsFlonum(argv[0])) ? TrueObject : FalseObject);
    else if (ComplexP(argv[0]) && FlonumP(AsReal(argv[0])))
    {
        FAssert(FlonumP(AsImaginary(argv[0])));

        return((finite(AsFlonum(AsReal(argv[0])))
                && finite(AsFlonum(AsImaginary(argv[0])))) ? TrueObject : FalseObject);
    }

    return(TrueObject);
}

static inline int_t InfiniteP(double64_t d)
{
    return(isnan(d) == 0 && finite(d) == 0);
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
        if (GenericCompare("<", argv[adx - 1], argv[adx], 1) >= 0)
            return(FalseObject);

    return(TrueObject);
}

Define(">", GreaterThanPrimitive)(int_t argc, FObject argv[])
{
    AtLeastTwoArgsCheck(">", argc);

    for (int_t adx = 1; adx < argc; adx++)
        if (GenericCompare(">", argv[adx - 1], argv[adx], 1) <= 0)
            return(FalseObject);

    return(TrueObject);
}

Define("<=", LessThanEqualPrimitive)(int_t argc, FObject argv[])
{
    AtLeastTwoArgsCheck("<=", argc);

    for (int_t adx = 1; adx < argc; adx++)
        if (GenericCompare("<=", argv[adx - 1], argv[adx], 1) > 0)
            return(FalseObject);

    return(TrueObject);
}

Define(">=", GreaterThanEqualPrimitive)(int_t argc, FObject argv[])
{
    AtLeastTwoArgsCheck(">=", argc);

    for (int_t adx = 1; adx < argc; adx++)
        if (GenericCompare(">=", argv[adx - 1], argv[adx], 1) < 0)
            return(FalseObject);

    return(TrueObject);
}

Define("zero?", ZeroPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("zero?", argc);
    NumberArgCheck("zero?", argv[0]);

    if (FixnumP(argv[0]))
        return(AsFixnum(argv[0]) == 0 ? TrueObject : FalseObject);
    else if (FlonumP(argv[0]))
        return(AsFlonum(argv[0]) == 0.0 ? TrueObject : FalseObject);

    return(FalseObject);
}

Define("positive?", PositivePPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("positive?", argc);

    return(GenericCompare("positive?", argv[0], MakeFixnum(0), 0) > 0 ? TrueObject : FalseObject);
}

Define("negative?", NegativePPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("negative?", argc);

    return(GenericCompare("negative?", argv[0], MakeFixnum(0), 0) < 0 ? TrueObject : FalseObject);
}

Define("odd?", OddPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("odd?", argc);
    IntegerArgCheck("odd?", argv[0]);

    if (FixnumP(argv[0]))
        return(AsFixnum(argv[0]) % 2 != 0 ? TrueObject : FalseObject);

    FAssert(BignumP(argv[0]));

    return(BignumModuloFixnum(argv[0], 2) != 0 ? TrueObject : FalseObject);
}

Define("even?", EvenPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("even?", argc);
    IntegerArgCheck("even?", argv[0]);

    if (FixnumP(argv[0]))
        return(AsFixnum(argv[0]) % 2 == 0 ? TrueObject : FalseObject);

    FAssert(BignumP(argv[0]));

    return(BignumModuloFixnum(argv[0], 2) == 0 ? TrueObject : FalseObject);
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

Define("exact", ExactPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("exact", argc);
    NumberArgCheck("exact", argv[0]);

    return(ToExact(argv[0]));
}

Define("inexact", InexactPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("inexact", argc);
    NumberArgCheck("inexact", argv[0]);

    return(ToInexact(argv[0]));
}

Define("expt", ExptPrimitive)(int_t argc, FObject argv[])
{
    if (argc != 2)
        RaiseExceptionC(R.Assertion, "expt", "expected two arguments", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "expt", "expected a fixnum", List(argv[0]));

    if (FixnumP(argv[1]) == 0)
        RaiseExceptionC(R.Assertion, "expt", "expected a fixnum", List(argv[1]));

    int_t x = AsFixnum(argv[0]);
    int_t n = AsFixnum(argv[1]);
    int_t ret = 1;
    while (n > 0)
    {
        ret *= x;
        n -= 1;
    }

    return(MakeFixnum(ret));
}

Define("abs", AbsPrimitive)(int_t argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "abs", "expected one argument", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "abs", "expected a fixnum", List(argv[0]));

    return(AsFixnum(argv[0]) < 0 ? MakeFixnum(- AsFixnum(argv[0])) : argv[0]);
}

Define("sqrt", SqrtPrimitive)(int_t argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "sqrt", "expected one argument", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "sqrt", "expected a fixnum", List(argv[0]));

    int_t n = AsFixnum(argv[0]);
    int_t x = 1;

    while (x * x < n)
        x += 1;

    return(MakeFixnum(x));
}

Define("number->string", NumberToStringPrimitive)(int_t argc, FObject argv[])
{
    if (argc != 2)
        RaiseExceptionC(R.Assertion, "number->string", "expected two arguments", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "number->string", "expected a fixnum", List(argv[0]));

    if (FixnumP(argv[1]) == 0)
        RaiseExceptionC(R.Assertion, "number->string", "expected a fixnum", List(argv[1]));

    FCh s[16];
    int_t sl = FixnumAsString(AsFixnum(argv[0]), s, AsFixnum(argv[1]));
    return(MakeString(s, sl));
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
    
    
    &AddPrimitive,
    &MultiplyPrimitive,
    &SubtractPrimitive,
    &DividePrimitive,
    
    
    &ExactPrimitive,
    &InexactPrimitive,
    &ExptPrimitive,
    &AbsPrimitive,
    &SqrtPrimitive,
    &NumberToStringPrimitive
};

void SetupNumbers()
{
    FAssert(sizeof(double64_t) == 8);

    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
