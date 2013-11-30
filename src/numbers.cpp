/*

Foment

z: complex
x: real
n: integer

00: ratnum
01: complex
10: flonum
11: fixnum

(z1 & 0x3) << 2 | (z2 & 0x3)

-- NumberToString
-- StringToNumber: need another routine for string->number which checks for radix and exactness
    prefixes
<num> : <complex>
      | <radix> <complex>
      | <exactness> <complex>
      | <radix> <exactness> <complex>
      | <exactness> <radix> <complex>
<radix> : #b | #o | #d | #x
<exactness> : #i | #e

-- maybe have Ratnums and Bigrats
-- handle case of Ratnum num/0
-- ParseUReal: normalize ratnums
-- MakeBignum
-- check for MAXIMUM_FIXNUM or MINIMUM_FIXNUM and make bignum instead: ToExact
-- ParseComplex: <real> @ <real>
-- ToExact: convert Ratnum
-- RationalPPrimitive: broken
-- write.cpp: NeedImaginaryPlusSignP updated as more types are added
-- OddPPrimitive and EvenPPrimitive: only work for fixnums

*/

#include <math.h>
#include <float.h>
#include <stdio.h>
#include "foment.hpp"
#include "unicode.hpp"
#include "mini-gmp.h"

#ifdef FOMENT_UNIX
#define _finite finite
#define _isnan isnan
#endif // FOMENT_UNIX

static FObject MakeFlonum(double64_t dbl)
{
    FFlonum * flo = (FFlonum *) MakeObject(sizeof(FFlonum), FlonumTag);
    flo->Double = dbl;

    FObject obj = FlonumObject(flo);
    FAssert(FlonumP(obj));
    FAssert(AsFlonum(obj) == dbl);

    return(obj);
}

#define AsNumerator(z) AsRatnum(z)->Numerator
#define AsDenominator(z) AsRatnum(z)->Denominator

static FObject MakeRatnum(FObject nmr, FObject dnm)
{
    FAssert(FixnumP(nmr));
    FAssert(FixnumP(dnm));
    FAssert(AsFixnum(dnm) != 0);

    if (AsFixnum(nmr) == 0)
        return(nmr);

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

    FRatnum * rat = (FRatnum *) MakeObject(sizeof(FRatnum), RatnumTag);
    rat->Numerator = nmr;
    rat->Denominator = dnm;

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

static FObject MakeBignum(FFixnum sgn, uint_t dc)
{
    FAssert(dc > 0);

    FBignum * bn = (FBignum *) MakeObject(sizeof(FBignum) + (dc - 1) * sizeof(FFixnum), BignumTag);
    if (bn == 0)
    {
        bn = (FBignum *) MakeMatureObject(sizeof(FBignum) + (dc - 1) * sizeof(FFixnum), "bignum");
        bn->Length = MakeMatureLength(dc, BignumTag) | (sgn < 0 ? BIGNUM_FLAG_NEGATIVE : 0);
    }
    else
        bn->Length = MakeLength(dc, BignumTag) | (sgn < 0 ? BIGNUM_FLAG_NEGATIVE : 0);

    return(bn);
}

static FObject BignumFixnumAdd(FObject bn, FFixnum fn)
{
    FAssert(BignumP(bn));

    FFixnum carry = fn;

    for (uint_t idx = 0; idx < BignumLength(bn); idx++)
    {
        FFixnum n = AsBignum(bn)->Digits[idx];
        AsBignum(bn)->Digits[idx] += carry;

        carry = (n > (MAXIMUM_FIXNUM - carry));
        if (carry == 0)
            break;
    }
    
    
    
    
    return(bn);
}

static FObject BignumFixnumMultiply(FObject bn, FFixnum fn, FObject rbn)
{
    FAssert(BignumP(bn));
    FAssert(fn > 0);

    if (BignumP(rbn) == 0 || BignumLength(rbn) < BignumLength(bn))
        rbn = MakeBignum(BignumNegativeP(bn) ? -1 : 1, BignumLength(bn));

    FFixnum carry = 0;

    for (uint_t idx = 0; idx < BignumLength(bn); idx++)
    {
        int64_t n = AsBignum(bn)->Digits[idx] * fn + carry;
        AsBignum(rbn)->Digits[idx] = (FFixnum) n;
        carry = n >> (sizeof(int32_t) * 8);
    }
    
    
    
    return(rbn);
}

FObject ToInexact(FObject n)
{
    if (FixnumP(n))
        return(MakeFlonum(AsFixnum(n)));
    else if (RatnumP(n))
    {
        FAssert(FixnumP(AsNumerator(n)));
        FAssert(FixnumP(AsDenominator(n)));

        double d = AsFixnum(AsNumerator(n));
        d /= AsFixnum(AsDenominator(n));
        return(MakeFlonum(d));
    }

    FAssert(FlonumP(n));
    return(n);
}

static inline double Truncate(double n)
{
    return(floor(n + 0.5 * ((n < 0 ) ? 1 : 0)));
}

FObject ToExact(FObject n)
{
    if (FlonumP(n))
    {
        if (AsFlonum(n) == Truncate(AsFlonum(n)))
            return(MakeFixnum(AsFlonum(n)));
        
        
        
        return(n);
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

    FObject bn = MakeBignum(sgn, 1);
    AsBignum(bn)->Digits[0] = n;

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

            bn = BignumFixnumMultiply(bn, rdx, bn);
            bn = BignumFixnumAdd(bn, dv);
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
                bn = BignumFixnumMultiply(bn, rdx, bn);
                bn = BignumFixnumAdd(bn, dv);
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

    double64_t d = AsFixnum(whl);

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

FObject NumberToString(FObject obj, FFixnum rdx)
{
    
    
    
    return(NoValueObject);
}

static int_t GenericEqual(FObject z1, FObject z2)
{
    if (FixnumP(z1))
        return(FixnumP(z2) && AsFixnum(z1) == AsFixnum(z2));
    else if (FlonumP(z1))
        return(FlonumP(z2) && AsFlonum(z1) == AsFlonum(z2));
    else if (RatnumP(z1))
        return(RatnumP(z2) && GenericEqual(AsNumerator(z1), AsNumerator(z1))
                && GenericEqual(AsDenominator(z1), AsDenominator(z2)));

    FAssert(ComplexP(z1));

    return(ComplexP(z2) && GenericEqual(AsReal(z1), AsReal(z2))
            && GenericEqual(AsImaginary(z1), AsImaginary(z2)));
}

static int_t GenericLess(FObject x1, FObject x2)
{
    if (FixnumP(x1))
        return(FixnumP(x2) && AsFixnum(x1) < AsFixnum(x2));
    else if (FlonumP(x1))
        return(FlonumP(x2) && AsFlonum(x1) < AsFlonum(x2));

    FAssert(RatnumP(x1));
    
    
    
    FAssert(0);
    return(0);
}

static int_t GenericGreater(FObject x1, FObject x2)
{
    if (FixnumP(x1))
        return(FixnumP(x2) && AsFixnum(x1) > AsFixnum(x2));
    else if (FlonumP(x1))
        return(FlonumP(x2) && AsFlonum(x1) > AsFlonum(x2));

    FAssert(RatnumP(x1));
    
    
    
    FAssert(0);
    return(0);
}

#define QuickNumberP(z1, z2) ((((FImmediate) (z1)) & 0x4) + (((FImmediate) (z2)) & 0x4)) == 0x8
#define QuickOp(z1, z2) ((((FImmediate) (z1)) & 0x3) << 2) | (((FImmediate) (z2)) & 0x3)

static const int_t OP_RAT_RAT = 0x0; // 0b0000
static const int_t OP_RAT_CPX = 0x1; // 0b0001
static const int_t OP_RAT_FLO = 0x2; // 0b0010
static const int_t OP_RAT_FIX = 0x3; // 0b0011
static const int_t OP_CPX_RAT = 0x4; // 0b0100
static const int_t OP_CPX_CPX = 0x5; // 0b0101
static const int_t OP_CPX_FLO = 0x6; // 0b0110
static const int_t OP_CPX_FIX = 0x7; // 0b0111
static const int_t OP_FLO_RAT = 0x8; // 0b1000
static const int_t OP_FLO_CPX = 0x9; // 0b1001
static const int_t OP_FLO_FLO = 0xA; // 0b1010
static const int_t OP_FLO_FIX = 0xB; // 0b1011
static const int_t OP_FIX_RAT = 0xC; // 0b1100
static const int_t OP_FIX_CPX = 0xD; // 0b1101
static const int_t OP_FIX_FLO = 0xE; // 0b1110
static const int_t OP_FIX_FIX = 0xF; // 0b1111

static FObject GenericAdd(FObject z1, FObject z2)
{
    if (QuickNumberP(z1, z2))
    {
        switch (QuickOp(z1, z2))
        {
        case OP_RAT_RAT:
            FAssert(FixnumP(AsNumerator(z1)));
            FAssert(FixnumP(AsDenominator(z1)));
            FAssert(FixnumP(AsNumerator(z2)));
            FAssert(FixnumP(AsDenominator(z2)));

            return(MakeRatnum(MakeFixnum(AsFixnum(AsNumerator(z1)) * AsFixnum(AsDenominator(z2))
                    + AsFixnum(AsNumerator(z2)) * AsFixnum(AsDenominator(z1))),
                    MakeFixnum(AsFixnum(AsDenominator(z1)) * AsFixnum(AsDenominator(z2)))));

        case OP_RAT_CPX:
            return(MakeComplex(GenericAdd(z1, AsReal(z2)), AsImaginary(z2)));

        case OP_RAT_FLO:
        {
            FObject flo = ToInexact(z1);

            FAssert(FlonumP(flo));

            return(MakeFlonum(AsFlonum(flo) + AsFlonum(z2)));
        }

        case OP_RAT_FIX:
            FAssert(FixnumP(AsNumerator(z1)));
            FAssert(FixnumP(AsDenominator(z1)));

            return(MakeRatnum(MakeFixnum(AsFixnum(AsNumerator(z1)) + AsFixnum(z2)
                    * AsFixnum(AsDenominator(z1))), AsDenominator(z1)));

        case OP_CPX_RAT:
            return(MakeComplex(GenericAdd(AsReal(z1), z2), AsImaginary(z1)));

        case OP_CPX_CPX:
            return(MakeComplex(GenericAdd(AsReal(z1), AsReal(z2)),
                    GenericAdd(AsImaginary(z1), AsImaginary(z2))));

        case OP_CPX_FLO:
        case OP_CPX_FIX:
            return(MakeComplex(GenericAdd(AsReal(z1), z2), AsImaginary(z1)));

        case OP_FLO_RAT:
        {
            FObject flo = ToInexact(z2);

            FAssert(FlonumP(flo));

            return(MakeFlonum(AsFlonum(z1) + AsFlonum(flo)));
        }

        case OP_FLO_CPX:
            return(MakeComplex(GenericAdd(z1, AsReal(z2)), AsImaginary(z2)));

        case OP_FLO_FLO:
            return(MakeFlonum(AsFlonum(z1) + AsFlonum(z2)));

        case OP_FLO_FIX:
            return(MakeFlonum(AsFlonum(z1) + AsFixnum(z2)));

        case OP_FIX_RAT:
            FAssert(FixnumP(AsNumerator(z2)));
            FAssert(FixnumP(AsDenominator(z2)));

            return(MakeRatnum(MakeFixnum(AsFixnum(z1) * AsFixnum(AsDenominator(z2))
                    + AsFixnum(AsNumerator(z2))), AsDenominator(z2)));

        case OP_FIX_CPX:
            return(MakeComplex(GenericAdd(z1, AsReal(z2)), AsImaginary(z2)));

        case OP_FIX_FLO:
            return(MakeFlonum(AsFixnum(z1) + AsFlonum(z2)));

        case OP_FIX_FIX:
        {
            FFixnum n = AsFixnum(z1) + AsFixnum(z2);
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

static FObject GenericSubtract(FObject z1, FObject z2);
static FObject GenericMultiply(FObject z1, FObject z2)
{
    if (QuickNumberP(z1, z2))
    {
        switch (QuickOp(z1, z2))
        {
        case OP_RAT_RAT:
            FAssert(FixnumP(AsNumerator(z1)));
            FAssert(FixnumP(AsDenominator(z1)));
            FAssert(FixnumP(AsNumerator(z2)));
            FAssert(FixnumP(AsDenominator(z2)));

            return(MakeRatnum(MakeFixnum(AsFixnum(AsNumerator(z1)) * AsFixnum(AsNumerator(z2))),
                    MakeFixnum(AsFixnum(AsDenominator(z1)) * AsFixnum(AsDenominator(z2)))));

        case OP_RAT_CPX:
            return(MakeComplex(GenericMultiply(z1, AsReal(z2)),
                    GenericMultiply(z1, AsImaginary(z2))));

        case OP_RAT_FLO:
        {
            FObject flo = ToInexact(z1);

            FAssert(FlonumP(flo));

            return(MakeFlonum(AsFlonum(flo) * AsFlonum(z2)));
        }

        case OP_RAT_FIX:
            FAssert(FixnumP(AsNumerator(z1)));
            FAssert(FixnumP(AsDenominator(z1)));

            return(MakeRatnum(MakeFixnum(AsFixnum(AsNumerator(z1)) * AsFixnum(z2)),
                    AsDenominator(z1)));

        case OP_CPX_RAT:
            return(MakeComplex(GenericMultiply(AsReal(z1), z2),
                    GenericMultiply(AsImaginary(z1), z2)));

        case OP_CPX_CPX:
            return(MakeComplex(GenericSubtract(GenericMultiply(AsReal(z1), AsReal(z2)),
                    GenericMultiply(AsImaginary(z1), AsImaginary(z2))),
                    GenericAdd(GenericMultiply(AsReal(z1), AsImaginary(z2)),
                    GenericMultiply(AsImaginary(z1), AsReal(z2)))));

        case OP_CPX_FLO:
        case OP_CPX_FIX:
            return(MakeComplex(GenericMultiply(AsReal(z1), z2),
                    GenericMultiply(AsImaginary(z1), z2)));

        case OP_FLO_RAT:
        {
            FObject flo = ToInexact(z2);

            FAssert(FlonumP(flo));

            return(MakeFlonum(AsFlonum(z1) * AsFlonum(flo)));
        }

        case OP_FLO_CPX:
            return(MakeComplex(GenericMultiply(z1, AsReal(z2)),
                    GenericMultiply(z1, AsImaginary(z2))));

        case OP_FLO_FLO:
            return(MakeFlonum(AsFlonum(z1) * AsFlonum(z2)));

        case OP_FLO_FIX:
            return(MakeFlonum(AsFlonum(z1) * AsFixnum(z2)));

        case OP_FIX_RAT:
            FAssert(FixnumP(AsNumerator(z2)));
            FAssert(FixnumP(AsDenominator(z2)));

            return(MakeRatnum(MakeFixnum(AsFixnum(z1) * AsFixnum(AsNumerator(z2))),
                    AsDenominator(z2)));

        case OP_FIX_CPX:
            return(MakeComplex(GenericMultiply(z1, AsReal(z2)),
                    GenericMultiply(z1, AsImaginary(z2))));

        case OP_FIX_FLO:
            return(MakeFlonum(AsFixnum(z1) * AsFlonum(z2)));

        case OP_FIX_FIX:
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
    if (QuickNumberP(z1, z2))
    {
        switch (QuickOp(z1, z2))
        {
        case OP_RAT_RAT:
            FAssert(FixnumP(AsNumerator(z1)));
            FAssert(FixnumP(AsDenominator(z1)));
            FAssert(FixnumP(AsNumerator(z2)));
            FAssert(FixnumP(AsDenominator(z2)));

            return(MakeRatnum(MakeFixnum(AsFixnum(AsNumerator(z1)) * AsFixnum(AsDenominator(z2))
                    - AsFixnum(AsNumerator(z2)) * AsFixnum(AsDenominator(z1))),
                    MakeFixnum(AsFixnum(AsDenominator(z1)) * AsFixnum(AsDenominator(z2)))));

        case OP_RAT_CPX:
            return(MakeComplex(GenericSubtract(z1, AsReal(z2)), AsImaginary(z2)));

        case OP_RAT_FLO:
        {
            FObject flo = ToInexact(z1);

            FAssert(FlonumP(flo));

            return(MakeFlonum(AsFlonum(flo) - AsFlonum(z2)));
        }

        case OP_RAT_FIX:
            FAssert(FixnumP(AsNumerator(z1)));
            FAssert(FixnumP(AsDenominator(z1)));

            return(MakeRatnum(MakeFixnum(AsFixnum(AsNumerator(z1)) - AsFixnum(z2)
                    * AsFixnum(AsDenominator(z1))), AsDenominator(z1)));

        case OP_CPX_RAT:
            return(MakeComplex(GenericSubtract(AsReal(z1), z2), AsImaginary(z1)));

        case OP_CPX_CPX:
            return(MakeComplex(GenericSubtract(AsReal(z1), AsReal(z2)),
                    GenericSubtract(AsImaginary(z1), AsImaginary(z2))));

        case OP_CPX_FLO:
        case OP_CPX_FIX:
            return(MakeComplex(GenericSubtract(AsReal(z1), z2), AsImaginary(z1)));

        case OP_FLO_RAT:
        {
            FObject flo = ToInexact(z2);

            FAssert(FlonumP(flo));

            return(MakeFlonum(AsFlonum(z1) - AsFlonum(flo)));
        }

        case OP_FLO_CPX:
            return(MakeComplex(GenericSubtract(z1, AsReal(z2)), AsImaginary(z2)));

        case OP_FLO_FLO:
            return(MakeFlonum(AsFlonum(z1) - AsFlonum(z2)));

        case OP_FLO_FIX:
            return(MakeFlonum(AsFlonum(z1) - AsFixnum(z2)));

        case OP_FIX_RAT:
            FAssert(FixnumP(AsNumerator(z2)));
            FAssert(FixnumP(AsDenominator(z2)));

            return(MakeRatnum(MakeFixnum(AsFixnum(z1) * AsFixnum(AsDenominator(z2))
                    - AsFixnum(AsNumerator(z2))), AsDenominator(z2)));

        case OP_FIX_CPX:
            return(MakeComplex(GenericSubtract(z1, AsReal(z2)), AsImaginary(z2)));

        case OP_FIX_FLO:
            return(MakeFlonum(AsFixnum(z1) - AsFlonum(z2)));

        case OP_FIX_FIX:
        {
            FFixnum n = AsFixnum(z1) - AsFixnum(z2);
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
    if (QuickNumberP(z1, z2))
    {
        switch (QuickOp(z1, z2))
        {
        case OP_RAT_RAT:
            FAssert(FixnumP(AsNumerator(z1)));
            FAssert(FixnumP(AsDenominator(z1)));
            FAssert(FixnumP(AsNumerator(z2)));
            FAssert(FixnumP(AsDenominator(z2)));

            return(MakeRatnum(MakeFixnum(AsFixnum(AsNumerator(z1)) * AsFixnum(AsDenominator(z2))),
                    MakeFixnum(AsFixnum(AsDenominator(z1)) * AsFixnum(AsNumerator(z2)))));

        case OP_RAT_CPX:
            return(MakeComplex(GenericDivide(z1, AsReal(z2)),
                    GenericDivide(z1, AsImaginary(z2))));

        case OP_RAT_FLO:
        {
            FObject flo = ToInexact(z1);

            FAssert(FlonumP(flo));

            return(MakeFlonum(AsFlonum(flo) / AsFlonum(z2)));
        }

        case OP_RAT_FIX:
            FAssert(FixnumP(AsNumerator(z1)));
            FAssert(FixnumP(AsDenominator(z1)));

            return(MakeRatnum(AsNumerator(z1),
                    MakeFixnum(AsFixnum(AsDenominator(z1)) * AsFixnum(z2))));

        case OP_CPX_RAT:
            return(MakeComplex(GenericDivide(AsReal(z1), z2),
                    GenericDivide(AsImaginary(z1), z2)));

        case OP_CPX_CPX:
        {
            FObject cpx = MakeComplex(AsReal(z2), GenericSubtract(MakeFixnum(0), AsImaginary(z2)));

            FAssert(ComplexP(GenericMultiply(z2, cpx)) == 0);

            return(GenericDivide(GenericMultiply(z1, cpx), GenericMultiply(z2, cpx)));
        }

        case OP_CPX_FLO:
        case OP_CPX_FIX:
            return(MakeComplex(GenericDivide(AsReal(z1), z2),
                    GenericDivide(AsImaginary(z1), z2)));

        case OP_FLO_RAT:
        {
            FObject flo = ToInexact(z2);

            FAssert(FlonumP(flo));

            return(MakeFlonum(AsFlonum(z1) / AsFlonum(flo)));
        }

        case OP_FLO_CPX:
            return(MakeComplex(GenericDivide(z1, AsReal(z2)),
                    GenericDivide(z1, AsImaginary(z2))));

        case OP_FLO_FLO:
            return(MakeFlonum(AsFlonum(z1) / AsFlonum(z2)));

        case OP_FLO_FIX:
            return(MakeFlonum(AsFlonum(z1) / AsFixnum(z2)));

        case OP_FIX_RAT:
            FAssert(FixnumP(AsNumerator(z2)));
            FAssert(FixnumP(AsDenominator(z2)));

            return(MakeRatnum(AsNumerator(z2),
                    MakeFixnum(AsFixnum(z1) * AsFixnum(AsDenominator(z2)))));

        case OP_FIX_CPX:
            return(MakeComplex(GenericDivide(z1, AsReal(z2)),
                    GenericDivide(z1, AsImaginary(z2))));

        case OP_FIX_FLO:
            return(MakeFlonum(AsFixnum(z1) / AsFlonum(z2)));

        case OP_FIX_FIX:
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
    
    
    
    return(FalseObject);
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
        return(_finite(AsFlonum(argv[0])) ? TrueObject : FalseObject);
    else if (ComplexP(argv[0]) && FlonumP(AsReal(argv[0])))
    {
        FAssert(FlonumP(AsImaginary(argv[0])));

        return((_finite(AsFlonum(AsReal(argv[0])))
                && _finite(AsFlonum(AsImaginary(argv[0])))) ? TrueObject : FalseObject);
    }

    return(TrueObject);
}

static inline int_t InfiniteP(double d)
{
    return(_isnan(d) == 0 && _finite(d) == 0);
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
        return(_isnan(AsFlonum(argv[0])) ? TrueObject : FalseObject);
    else if (ComplexP(argv[0]) && FlonumP(AsReal(argv[0])))
    {
        FAssert(FlonumP(AsImaginary(argv[0])));

        return((_isnan(AsFlonum(AsReal(argv[0])))
                || _isnan(AsFlonum(AsImaginary(argv[0])))) ? TrueObject : FalseObject);
    }

    return(FalseObject);
}

Define("=", EqualPrimitive)(int_t argc, FObject argv[])
{
    AtLeastTwoArgsCheck("=", argc);
    NumberArgCheck("=", argv[0]);

    for (int_t adx = 1; adx < argc; adx++)
    {
        NumberArgCheck("=", argv[adx]);

        if (argv[adx - 1] != argv[adx] && GenericEqual(argv[adx - 1], argv[adx]) == 0)
            return(FalseObject);
    }

    return(TrueObject);
}

Define("<", LessThanPrimitive)(int_t argc, FObject argv[])
{
    AtLeastTwoArgsCheck("<", argc);
    RealArgCheck("<", argv[0]);

    for (int_t adx = 1; adx < argc; adx++)
    {
        NumberArgCheck("<", argv[adx]);

        if (GenericLess(argv[adx - 1], argv[adx]) == 0)
            return(FalseObject);
    }

    return(TrueObject);
}

Define(">", GreaterThanPrimitive)(int_t argc, FObject argv[])
{
    AtLeastTwoArgsCheck(">", argc);
    RealArgCheck(">", argv[0]);

    for (int_t adx = 1; adx < argc; adx++)
    {
        NumberArgCheck(">", argv[adx]);

        if (GenericGreater(argv[adx - 1], argv[adx]) == 0)
            return(FalseObject);
    }

    return(TrueObject);
}

Define("<=", LessThanEqualPrimitive)(int_t argc, FObject argv[])
{
    AtLeastTwoArgsCheck("<=", argc);
    RealArgCheck("<=", argv[0]);

    for (int_t adx = 1; adx < argc; adx++)
    {
        NumberArgCheck("<=", argv[adx]);

        if (GenericGreater(argv[adx - 1], argv[adx]))
            return(FalseObject);
    }

    return(TrueObject);
}

Define(">=", GreaterThanEqualPrimitive)(int_t argc, FObject argv[])
{
    AtLeastTwoArgsCheck(">=", argc);
    RealArgCheck(">=", argv[0]);

    for (int_t adx = 1; adx < argc; adx++)
    {
        NumberArgCheck(">=", argv[adx]);

        if (GenericLess(argv[adx - 1], argv[adx]))
            return(FalseObject);
    }

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
    RealArgCheck("positive?", argv[0]);

    return(GenericGreater(argv[0], MakeFixnum(0)) ? TrueObject : FalseObject);
}

Define("negative?", NegativePPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("negative?", argc);
    RealArgCheck("negative?", argv[0]);

    return(GenericLess(argv[0], MakeFixnum(0)) ? TrueObject : FalseObject);
}

Define("odd?", OddPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("odd?", argc);
    IntegerArgCheck("odd?", argv[0]);

    return(AsFixnum(argv[0]) % 2 != 0 ? TrueObject : FalseObject);
}

Define("even?", EvenPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("even?", argc);
    IntegerArgCheck("even?", argv[0]);

    return(AsFixnum(argv[0]) % 2 == 0 ? TrueObject : FalseObject);
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
    
    
    &ExptPrimitive,
    &AbsPrimitive,
    &SqrtPrimitive,
    &NumberToStringPrimitive
};

void SetupNumbers()
{
    mpz_t n;
    mpz_t m;
    mpz_init_set_str(m, "12345678901234567890", 10);
    mpz_init_set_str(n, "12345678901234567890", 10);
    mpz_mul(n, n, m);
    
    printf("%s\n", mpz_get_str(0, 10, n));
    
    mpz_clear(n);

    FAssert(sizeof(double64_t) == 8);

    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
