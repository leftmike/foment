/*

Foment

-- gc and FFlonums
-- StringToNumber: need another routine for string->number which checks for radix and exactness
    prefixes
-- reading numbers: add conversions to exact and inexact
-- reading numbers: some tokens beginning with - or + are not actually identifiers
-- ParseReal: return inf.0 and nan.0 correctly

*/

#include "foment.hpp"
#include "unicode.hpp"

static FObject MakeFlonum(double_t dbl)
{
    FFlonum * flo = (FFlonum *) MakeObject(sizeof(FFlonum), FlonumTag);
    flo->Reserved = FlonumTag;
    flo->Double = dbl;

    return(flo);
}

static FObject MakeBignum(FBigSign bs, FBigDigit * bdv, uint_t bdc)
{
    
    
    
    return(NoValueObject);
}

static const char * RatnumFieldsC[] = {"numerator", "denominator"};

static FObject MakeRatnum(FObject nmr, FObject dnm)
{
    FAssert(sizeof(FRatnum) == sizeof(RatnumFieldsC) + sizeof(FRecord));
    FAssert(FixnumP(nmr) || BignumP(nmr));
    FAssert(FixnumP(dnm) || BignumP(dnm));

    FRatnum * rat = (FRatnum *) MakeRecord(R.RatnumRecordType);
    rat->Numerator = nmr;
    rat->Denominator = dnm;

    return(rat);
}

static const char * ComplexFieldsC[] = {"real", "imaginary"};

static FObject MakeComplex(FObject rl, FObject img)
{
    FAssert(sizeof(FComplex) == sizeof(ComplexFieldsC) + sizeof(FRecord));
    FAssert(FixnumP(rl) || BignumP(rl) || FlonumP(rl) || RatnumP(rl));
    FAssert(FixnumP(img) || BignumP(img) || FlonumP(img) || RatnumP(img));

    FComplex * cx = (FComplex *) MakeRecord(R.ComplexRecordType);
    cx->Real = rl;
    cx->Imaginary = img;

    return(cx);
}

/*
<num> : <prefix> <complex>
<complex> : <real>
          | <real> @ <real>
          | <real> + <ureal> i
          | <real> - <ureal> i
          | <real> + i
          | <real> - i
          | <real> <infnan> i
          | <real> i
          | + i
          | - i
<real> : <ureal>
       | + <ureal>
       | - <ureal>
       | <infnan>
<ureal> : <uinteger>
        | <uinteger> / <uinteger>
        | <decimal10>
<uinteger> : <digit> <digit> ...
<decimal10> : <digit> <digit> ... <suffix>
            | . <digit> <digit> ... <suffix>
            | <digit> <digit> ... . <digit> ... <suffix>
<infnan> : +inf.0 | -inf.0 | +nan.0 | -nan.0
<suffix> : <empty>
         | e <digit> <digit> ...
         | e + <digit> <digit> ...
         | e - <digit> <digit> ...

*/

static int_t ParseUInteger(FCh * s, int_t sl, int_t sdx, FFixnum rdx, FObject * punt)
{
    FAssert(sdx < sl);

    
    
    return(sdx);
}

static int_t ParseUReal(FCh * s, int_t sl, int_t sdx, FFixnum rdx, FObject * purl)
{
    FAssert(sdx < sl);

    FFixnum n;
    int_t odx = sdx;

    switch (rdx)
    {
    case 2:
        for (n = 0; sdx < sl; sdx++)
        {
            if (s[sdx] >= '0' && s[sdx] <= '1')
                n = n * 2 + s[sdx] - '0';
            else
                break;
        }
        break;

    case 8:
        for (n = 0; sdx < sl; sdx++)
        {
            if (s[sdx] >= '0' && s[sdx] <= '7')
                n = n * 8 + s[sdx] - '0';
            else
                break;
        }
        break;

    case 10:
        for (n = 0; sdx < sl; sdx++)
        {
            if (s[sdx] >= '0' && s[sdx] <= '9')
                n = n * 10 + s[sdx] - '0';
            else
                break;
        }
        break;

    case 16:
        for (n = 0; sdx < sl; sdx++)
        {
            if (s[sdx] >= '0' && s[sdx] <= '9')
                n = n * 16 + s[sdx] - '0';
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

    if (sdx == odx)
        return(-1);

    *purl = MakeFixnum(n);
    
    
    return(sdx);
}

static int_t ParseReal(FCh * s, int_t sl, int_t sdx, FFixnum rdx, FObject * prl)
{
    FAssert(sdx < sl);

    if (sdx + 6 <= sl && (s[sdx] == '-' || s[sdx] == '+'))
    {
        if (CharDowncase(s[sdx + 1]) == 'i' && CharDowncase(s[sdx + 2]) == 'n'
                && CharDowncase(s[sdx + 3]) == 'f' && s[sdx + 4] == '.' && s[sdx + 5] == '0')
        {
            
            
            *prl = MakeFixnum(s[sdx] == '-' ? -1 : 1);
            
            
            sdx += 6;
            return(sdx);
        }
        else if (CharDowncase(s[sdx + 1]) == 'n' && CharDowncase(s[sdx + 2]) == 'a'
                && CharDowncase(s[sdx + 3]) == 'n' && s[sdx + 4] == '.' && s[sdx + 5] == '0')
        {
            
            
            *prl = MakeFixnum(s[sdx] == '-' ? -1 : 1);
            
            
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

    sdx = ParseUReal(s, sl, sdx, rdx, &url);
    if (sdx < 0)
        return(-1);
    
    
    if (sgn == -1)
        *prl = MakeFixnum(AsFixnum(url) * -1);
    else
        *prl = url;
    
    
    return(sdx);
}

FObject StringToNumber(FCh * s, int_t sl, FFixnum rdx)
{
    FAssert(rdx == 2 || rdx == 8 || rdx == 10 || rdx == 16);

    if (sl == 0)
        return(FalseObject);

    FObject n;

    if (ParseReal(s, sl, 0, rdx, &n) < 0)
        return(FalseObject);

    return(n);
/*
    FFixnum ns;
    FFixnum n;
    int_t sdx = 0;

    ns = 1;
    if (s[sdx] == '-')
    {
        ns = -1;
        sdx += 1;
        if (sdx == sl)
            return(FalseObject);
    }
    else if (s[sdx] == '+')
    {
        sdx += 1;
        if (sdx == sl)
            return(FalseObject);
    }

    switch (rdx)
    {
    case 2:
        for (n = 0; sdx < sl; sdx++)
        {
            if (s[sdx] >= '0' && s[sdx] <= '1')
                n = n * 2 + s[sdx] - '0';
            else
                return(FalseObject);
        }
        break;

    case 8:
        for (n = 0; sdx < sl; sdx++)
        {
            if (s[sdx] >= '0' && s[sdx] <= '7')
                n = n * 8 + s[sdx] - '0';
            else
                return(FalseObject);
        }
        break;

    case 10:
        for (n = 0; sdx < sl; sdx++)
        {
            if (s[sdx] >= '0' && s[sdx] <= '9')
                n = n * 10 + s[sdx] - '0';
            else
                return(FalseObject);
        }
        break;

    case 16:
        for (n = 0; sdx < sl; sdx++)
        {
            if (s[sdx] >= '0' && s[sdx] <= '9')
                n = n * 16 + s[sdx] - '0';
            else if (s[sdx] >= 'a' && s[sdx] <= 'f')
                n = n * 16 + s[sdx] - 'a' + 10;
            else if (s[sdx] >= 'A' && s[sdx] <= 'F')
                n = n * 16 + s[sdx] - 'A' + 10;
            else
                return(FalseObject);
        }
        break;

    default:
        return(FalseObject);
    }

    return(MakeFixnum(n * ns));
*/
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

Define("+", SumPrimitive)(int_t argc, FObject argv[])
{
    FFixnum ret = 0;
    for (int_t adx = 0; adx < argc; adx++)
    {
        if (FixnumP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, "+", "expected a fixnum", List(argv[adx]));

        ret += AsFixnum(argv[adx]);
    }

    return(MakeFixnum(ret));
}

Define("*", ProductPrimitive)(int_t argc, FObject argv[])
{
    FFixnum ret = 1;
    for (int_t adx = 0; adx < argc; adx++)
    {
        if (FixnumP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, "*", "expected a fixnum", List(argv[adx]));

        ret *= AsFixnum(argv[adx]);
    }

    return(MakeFixnum(ret));
}

Define("-", DifferencePrimitive)(int_t argc, FObject argv[])
{
    if (argc < 1)
        RaiseExceptionC(R.Assertion, "-", "expected at least one argument", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "-", "expected a fixnum", List(argv[0]));

    if (argc == 1)
        return(MakeFixnum(-AsFixnum(argv[0])));

    FFixnum ret = AsFixnum(argv[0]);
    for (int_t adx = 1; adx < argc; adx++)
    {
        if (FixnumP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, "-", "expected a fixnum", List(argv[adx]));

        ret -= AsFixnum(argv[adx]);
    }

    return(MakeFixnum(ret));
}

Define("/", QuotientPrimitive)(int_t argc, FObject argv[])
{
    if (argc < 1)
        RaiseExceptionC(R.Assertion, "/", "expected at least one argument", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "/", "expected a fixnum", List(argv[0]));

    if (argc == 1)
    {
        if (AsFixnum(argv[0]) == 0)
            RaiseExceptionC(R.Assertion, "/", "zero not allowed", List(argv[0]));

        return(MakeFixnum(1 / AsFixnum(argv[0])));
    }

    FFixnum ret = AsFixnum(argv[0]);

    for (int_t adx = 1; adx < argc; adx++)
    {
        if (FixnumP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, "/", "expected a fixnum", List(argv[adx]));

        if (AsFixnum(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, "/", "zero not allowed", List(argv[adx]));

        ret /= AsFixnum(argv[adx]);
    }

    return(MakeFixnum(ret));
}

Define("=", EqualPrimitive)(int_t argc, FObject argv[])
{
    if (argc < 2)
        RaiseExceptionC(R.Assertion, "=", "expected at least two arguments", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "=", "expected a fixnum", List(argv[0]));

    for (int_t adx = 1; adx < argc; adx++)
    {
        if (FixnumP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, "=", "expected a fixnum", List(argv[adx]));

        if (AsFixnum(argv[adx - 1]) != AsFixnum(argv[adx]))
            return(FalseObject);
    }

    return(TrueObject);
}

Define("<", LessThanPrimitive)(int_t argc, FObject argv[])
{
    if (argc < 2)
        RaiseExceptionC(R.Assertion, "<", "expected at least two arguments", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "<", "expected a fixnum", List(argv[0]));

    for (int_t adx = 1; adx < argc; adx++)
    {
        if (FixnumP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, "<", "expected a fixnum", List(argv[adx]));

        if (AsFixnum(argv[adx - 1]) >= AsFixnum(argv[adx]))
            return(FalseObject);
    }

    return(TrueObject);
}

Define(">", GreaterThanPrimitive)(int_t argc, FObject argv[])
{
    if (argc < 2)
        RaiseExceptionC(R.Assertion, ">", "expected at least two arguments", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, ">", "expected a fixnum", List(argv[0]));

    for (int_t adx = 1; adx < argc; adx++)
    {
        if (FixnumP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, ">", "expected a fixnum", List(argv[adx]));

        if (AsFixnum(argv[adx - 1]) <= AsFixnum(argv[adx]))
            return(FalseObject);
    }

    return(TrueObject);
}

Define("<=", LessThanEqualPrimitive)(int_t argc, FObject argv[])
{
    if (argc < 2)
        RaiseExceptionC(R.Assertion, "<=", "expected at least two arguments", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "<=", "expected a fixnum", List(argv[0]));

    for (int_t adx = 1; adx < argc; adx++)
    {
        if (FixnumP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, "<=", "expected a fixnum", List(argv[adx]));

        if (AsFixnum(argv[adx - 1]) > AsFixnum(argv[adx]))
            return(FalseObject);
    }

    return(TrueObject);
}

Define(">=", GreaterThanEqualPrimitive)(int_t argc, FObject argv[])
{
    if (argc < 2)
        RaiseExceptionC(R.Assertion, ">=", "expected at least two arguments", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, ">=", "expected a fixnum", List(argv[0]));

    for (int_t adx = 1; adx < argc; adx++)
    {
        if (FixnumP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, ">=", "expected a fixnum", List(argv[adx]));

        if (AsFixnum(argv[adx - 1]) < AsFixnum(argv[adx]))
            return(FalseObject);
    }

    return(TrueObject);
}

Define("zero?", ZeroPPrimitive)(int_t argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "zero?", "expected one argument", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "zero?", "expected a fixnum", List(argv[0]));

    return(AsFixnum(argv[0]) == 0 ? TrueObject : FalseObject);
}

Define("positive?", PositivePPrimitive)(int_t argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "positive?", "expected one argument",
                EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "positive?", "expected a fixnum", List(argv[0]));

    return(AsFixnum(argv[0]) > 0 ? TrueObject : FalseObject);
}

Define("negative?", NegativePPrimitive)(int_t argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "negative?", "expected one argument",
                EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "negative?", "expected a fixnum", List(argv[0]));

    return(AsFixnum(argv[0]) < 0 ? TrueObject : FalseObject);
}

Define("odd?", OddPPrimitive)(int_t argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "odd?", "expected one argument", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "odd?", "expected a fixnum", List(argv[0]));

    return(AsFixnum(argv[0]) % 2 != 0 ? TrueObject : FalseObject);
}

Define("even?", EvenPPrimitive)(int_t argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "even?", "expected one argument", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "even?", "expected a fixnum", List(argv[0]));

    return(AsFixnum(argv[0]) % 2 == 0 ? TrueObject : FalseObject);
}

Define("exact-integer?", ExactIntegerPPrimitive)(int_t argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "exact-integer?", "expected one argument",
                EmptyListObject);

        return(FixnumP(argv[0]) ? TrueObject : FalseObject);
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
    &SumPrimitive,
    &ProductPrimitive,
    &DifferencePrimitive,
    &QuotientPrimitive,
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
    &ExactIntegerPPrimitive,
    &ExptPrimitive,
    &AbsPrimitive,
    &SqrtPrimitive,
    &NumberToStringPrimitive
};

void SetupNumbers()
{
    FAssert(sizeof(FBigSign) == sizeof(FBigDigit));

    R.RatnumRecordType = MakeRecordTypeC("ratnum",
            sizeof(RatnumFieldsC) / sizeof(char *), RatnumFieldsC);
    R.ComplexRecordType = MakeRecordTypeC("complex",
            sizeof(ComplexFieldsC) / sizeof(char *), ComplexFieldsC);

    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
