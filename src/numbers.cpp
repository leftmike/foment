/*

Foment

z: complex
x: real
n: integer

gc and FFlonums

000: indirect object
010: pair
100: ratnum
110: complex
101:
001: pair
xxx010: immediate objects
011: bignum
100: fixnum
101: flonum
110: ratnum
111: complex

21 bits for Unicode characters

ObjectP needs to test for non-immediate objects
need a way to efficient test for combinations of fixnum, ratnum, flonum, and complex or none of
above


-- StringToNumber: need another routine for string->number which checks for radix and exactness
    prefixes
<num> : <complex>
      | <radix> <complex>
      | <exactness> <complex>
      | <radix> <exactness> <complex>
      | <exactness> <radix> <complex>
<radix> : #b | #o | #d | #x
<exactness> : #i | #e

-- handle case of Ratnum num/0
-- ParseUReal: normalize ratnums
-- MakeBignum
-- check for MAXIMUM_FIXNUM or MINIMUM_FIXNUM and make bignum instead: ParseUInteger, ToExact
-- ParseComplex: <real> @ <real>
-- ToExact: convert Ratnum
-- RationalPPrimitive: broken
-- write.cpp: NeedImaginaryPlusSignP updated as more types are added
-- OddPPrimitive and EvenPPrimitive: only work for fixnums

*/

#include <math.h>
#include <float.h>
#include "foment.hpp"
#include "unicode.hpp"

static FObject MakeFlonum(double_t dbl)
{
    FFlonum * flo = (FFlonum *) MakeObject(sizeof(FFlonum), FlonumTag);
    flo->Double = dbl;

    FObject obj = FlonumObject(flo);
    FAssert(FlonumP(obj));
    FAssert(AsFlonum(obj) == dbl);

    return(obj);
}

static FObject MakeBignum(FBigSign bs, FBigDigit * bdv, uint_t bdc)
{
    
    
    
    return(NoValueObject);
}

static FObject MakeRatnum(FObject nmr, FObject dnm)
{
    FAssert(FixnumP(nmr) || BignumP(nmr));
    FAssert(FixnumP(dnm) || BignumP(dnm));

    if (FixnumP(nmr) && AsFixnum(nmr) == 0)
        return(nmr);

    FRatnum * rat = (FRatnum *) MakeObject(sizeof(FRatnum), RatnumTag);
    rat->Numerator = nmr;
    rat->Denominator = dnm;

    FObject obj = RatnumObject(rat);
    FAssert(RatnumP(obj));
    FAssert(AsRatnum(obj) == rat);

    return(obj);
}

static FObject MakeComplex(FObject rl, FObject img)
{
    FAssert(FixnumP(rl) || BignumP(rl) || FlonumP(rl) || RatnumP(rl));
    FAssert(FixnumP(img) || BignumP(img) || FlonumP(img) || RatnumP(img));

    if (FlonumP(rl) || FlonumP(img))
    {
        rl = ToInexact(rl);
        img = ToInexact(img);
    }
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

    return(cmplx);
}

FObject ToInexact(FObject n)
{
    if (FixnumP(n))
        return(MakeFlonum(AsFixnum(n)));
    else if (RatnumP(n))
    {
        FAssert(FixnumP(AsRatnum(n)->Numerator));
        FAssert(FixnumP(AsRatnum(n)->Denominator));

        double d = AsFixnum(AsRatnum(n)->Numerator);
        d /= AsFixnum(AsRatnum(n)->Denominator);
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
        if (FlonumP(AsComplex(n)->Real))
        {
            FAssert(FlonumP(AsComplex(n)->Imaginary));

            return(MakeComplex(ToExact(AsComplex(n)->Real), ToExact(AsComplex(n)->Imaginary)));
        }

        FAssert(FlonumP(AsComplex(n)->Imaginary) == 0);

        return(n);
    }

    FAssert(FixnumP(n) || BignumP(n) || RatnumP(n));

    return(n);
}

static int_t ParseUInteger(FCh * s, int_t sl, int_t sdx, FFixnum rdx, FFixnum sgn, FObject * punt)
{
    // <uinteger> : <digit> <digit> ...

    FAssert(sdx < sl);

    FFixnum n;
    int_t strt = sdx;

    switch (rdx)
    {
    case 2:
        for (n = 0; sdx < sl; sdx++)
        {
            int_t dv = DigitValue(s[sdx]);
            if (dv >= 0 && dv <= 1)
                n = n * 2 + dv;
            else
                break;
        }
        break;

    case 8:
        for (n = 0; sdx < sl; sdx++)
        {
            int_t dv = DigitValue(s[sdx]);
            if (dv >= 0 && dv <= 7)
                n = n * 8 + dv;
            else
                break;
        }
        break;

    case 10:
        for (n = 0; sdx < sl; sdx++)
        {
            int_t dv = DigitValue(s[sdx]);
            if (dv >= 0 && dv <= 9)
                n = n * 10 + dv;
            else
                break;
        }
        break;

    case 16:
        for (n = 0; sdx < sl; sdx++)
        {
            int_t dv = DigitValue(s[sdx]);
            if (dv >= 0 && dv <= 9)
                n = n * 16 + dv;
            else if (s[sdx] >= 'a' && s[sdx] <= 'f')
                n = n * 16 + s[sdx] - 'a' + 10;
            else if (s[sdx] >= 'A' && s[sdx] <= 'F')
                n = n * 16 + s[sdx] - 'A' + 10;
            else
                break;
        }
        break;

    default:
        FAssert(0);

        return(-1);
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

    double_t d = AsFixnum(whl);

    if (s[sdx] == '.')
    {
        double_t scl = 0.1;

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

const static char Digits[] = {"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"};

int_t NumberAsString(FFixnum n, FCh * s, FFixnum rdx)
{
    int_t sl = 0;

    if (n < 0)
    {
        s[sl] = '-';
        sl += 1;
        n *= -1;
    }

    if (n >= rdx)
    {
        sl += NumberAsString(n / rdx, s + sl, rdx);
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

static int_t GenericEqual(FObject z1, FObject z2)
{
    if (FixnumP(z1))
        return(FixnumP(z2) && AsFixnum(z1) == AsFixnum(z2));
    else if (FlonumP(z1))
        return(FlonumP(z2) && AsFlonum(z1) == AsFlonum(z2));
    else if (RatnumP(z1))
        return(RatnumP(z2) && GenericEqual(AsRatnum(z1)->Numerator, AsRatnum(z2)->Numerator)
                && GenericEqual(AsRatnum(z1)->Denominator, AsRatnum(z2)->Denominator));

    FAssert(ComplexP(z1));

    return(ComplexP(z2) && GenericEqual(AsComplex(z1)->Real, AsComplex(z2)->Real)
            && GenericEqual(AsComplex(z1)->Imaginary, AsComplex(z2)->Imaginary));
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

static FObject GenericAdd(FObject z1, FObject z2)
{
    
    
    
    return(MakeFixnum(AsFixnum(z1) + AsFixnum(z2)));
}

static FObject GenericMultiply(FObject z1, FObject z2)
{
    
    
    
    return(MakeFixnum(AsFixnum(z1) * AsFixnum(z2)));
}

static FObject GenericSubract(FObject z1, FObject z2)
{
    
    
    
    return(MakeFixnum(AsFixnum(z1) - AsFixnum(z2)));
}

static FObject GenericDivide(FObject z1, FObject z2)
{
    
    
    
    return(MakeFixnum(AsFixnum(z1) / AsFixnum(z2)));
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
        obj = AsComplex(obj)->Real;

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

    return((FlonumP(argv[0]) || (ComplexP(argv[0]) && FlonumP(AsComplex(argv[0])->Real)))
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
    else if (ComplexP(argv[0]) && FlonumP(AsComplex(argv[0])->Real))
    {
        FAssert(FlonumP(AsComplex(argv[0])->Imaginary));

        return((_finite(AsFlonum(AsComplex(argv[0])->Real))
                && _finite(AsFlonum(AsComplex(argv[0])->Imaginary))) ? TrueObject : FalseObject);
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
    else if (ComplexP(argv[0]) && FlonumP(AsComplex(argv[0])->Real))
    {
        FAssert(FlonumP(AsComplex(argv[0])->Imaginary));

        return((InfiniteP(AsFlonum(AsComplex(argv[0])->Real))
                || InfiniteP(AsFlonum(AsComplex(argv[0])->Imaginary))) ? TrueObject : FalseObject);
    }

    return(FalseObject);
}

Define("nan?", NanPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("nan?", argc);
    NumberArgCheck("nan?", argv[0]);

    if (FlonumP(argv[0]))
        return(_isnan(AsFlonum(argv[0])) ? TrueObject : FalseObject);
    else if (ComplexP(argv[0]) && FlonumP(AsComplex(argv[0])->Real))
    {
        FAssert(FlonumP(AsComplex(argv[0])->Imaginary));

        return((_isnan(AsFlonum(AsComplex(argv[0])->Real))
                || _isnan(AsFlonum(AsComplex(argv[0])->Imaginary))) ? TrueObject : FalseObject);
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

    NumberArgCheck("+", argv[0]);
    FObject ret = argv[0];

    for (int_t adx = 1; adx < argc; adx++)
    {
        NumberArgCheck("+", argv[adx]);

        ret = GenericAdd(ret, argv[adx]);
    }

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
        return(GenericSubract(MakeFixnum(0), argv[0]));

    FObject ret = argv[0];
    for (int_t adx = 1; adx < argc; adx++)
    {
        NumberArgCheck("-", argv[adx]);

        ret = GenericSubract(ret, argv[adx]);
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
    int_t sl = NumberAsString(AsFixnum(argv[0]), s, AsFixnum(argv[1]));
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
    FAssert(sizeof(FBigSign) == sizeof(FBigDigit));

    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
