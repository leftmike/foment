/*

Foment

*/

#include "foment.hpp"

int StringToNumber(FCh * s, int sl, FFixnum * np, FFixnum b)
{
    FAssert(b == 2 || b == 8 || b == 10 || b == 16);

    FFixnum ns;
    FFixnum n;
    int sdx = 0;

    if (sl == 0)
        return(0);

    if (s[sdx] == '#')
    {
        sdx += 1;
        if (sdx == sl)
            return(0);

        if (s[sdx] == 'b')
            b = 2;
        else if (s[sdx] == 'o')
            b = 8;
        else if (s[sdx] == 'd')
            b = 10;
        else if (s[sdx] == 'x')
            b = 16;
        else
            return(0);

        sdx += 1;
        if (sdx == sl)
            return(0);
    }

    ns = 1;
    if (s[sdx] == '-')
    {
        ns = -1;
        sdx += 1;
        if (sdx == sl)
            return(0);
    }
    else if (s[sdx] == '+')
    {
        sdx += 1;
        if (sdx == sl)
            return(0);
    }

    switch (b)
    {
    case 2:
        for (n = 0; sdx < sl; sdx++)
        {
            if (s[sdx] >= '0' && s[sdx] <= '1')
                n = n * 2 + s[sdx] - '0';
            else
                return(0);
        }
        break;

    case 8:
        for (n = 0; sdx < sl; sdx++)
        {
            if (s[sdx] >= '0' && s[sdx] <= '7')
                n = n * 8 + s[sdx] - '0';
            else
                return(0);
        }
        break;

    case 10:
        for (n = 0; sdx < sl; sdx++)
        {
            if (s[sdx] >= '0' && s[sdx] <= '9')
                n = n * 10 + s[sdx] - '0';
            else
                return(0);
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
                return(0);
        }
        break;

    default:
        return(0);
    }

    *np = n * ns;
    return(1);
}

const static char Digits[] = {"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"};

int NumberAsString(FFixnum n, FCh * s, FFixnum b)
{
    int sl = 0;

    if (n < 0)
    {
        s[sl] = '-';
        sl += 1;
        n *= -1;
    }

    if (n >= b)
    {
        sl += NumberAsString(n / b, s + sl, b);
        s[sl] = Digits[n % b];
        sl += 1;
    }
    else
    {
        s[sl] = Digits[n];
        sl += 1;
    }

    return(sl);
}

Define("+", SumPrimitive)(int argc, FObject argv[])
{
    FFixnum ret = 0;
    for (int adx = 0; adx < argc; adx++)
    {
        if (FixnumP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, "+", "expected a fixnum", List(argv[adx]));

        ret += AsFixnum(argv[adx]);
    }

    return(MakeFixnum(ret));
}

Define("*", ProductPrimitive)(int argc, FObject argv[])
{
    FFixnum ret = 1;
    for (int adx = 0; adx < argc; adx++)
    {
        if (FixnumP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, "*", "expected a fixnum", List(argv[adx]));

        ret *= AsFixnum(argv[adx]);
    }

    return(MakeFixnum(ret));
}

Define("-", DifferencePrimitive)(int argc, FObject argv[])
{
    if (argc < 1)
        RaiseExceptionC(R.Assertion, "-", "expected at least one argument", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "-", "expected a fixnum", List(argv[0]));

    if (argc == 1)
        return(MakeFixnum(-AsFixnum(argv[0])));

    FFixnum ret = AsFixnum(argv[0]);
    for (int adx = 1; adx < argc; adx++)
    {
        if (FixnumP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, "-", "expected a fixnum", List(argv[adx]));

        ret -= AsFixnum(argv[adx]);
    }

    return(MakeFixnum(ret));
}

Define("/", QuotientPrimitive)(int argc, FObject argv[])
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

    for (int adx = 1; adx < argc; adx++)
    {
        if (FixnumP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, "/", "expected a fixnum", List(argv[adx]));

        if (AsFixnum(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, "/", "zero not allowed", List(argv[adx]));

        ret /= AsFixnum(argv[adx]);
    }

    return(MakeFixnum(ret));
}

Define("=", EqualPrimitive)(int argc, FObject argv[])
{
    if (argc < 2)
        RaiseExceptionC(R.Assertion, "=", "expected at least two arguments", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "=", "expected a fixnum", List(argv[0]));

    for (int adx = 1; adx < argc; adx++)
    {
        if (FixnumP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, "=", "expected a fixnum", List(argv[adx]));

        if (AsFixnum(argv[adx - 1]) != AsFixnum(argv[adx]))
            return(FalseObject);
    }

    return(TrueObject);
}

Define("<", LessThanPrimitive)(int argc, FObject argv[])
{
    if (argc < 2)
        RaiseExceptionC(R.Assertion, "<", "expected at least two arguments", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "<", "expected a fixnum", List(argv[0]));

    for (int adx = 1; adx < argc; adx++)
    {
        if (FixnumP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, "<", "expected a fixnum", List(argv[adx]));

        if (AsFixnum(argv[adx - 1]) >= AsFixnum(argv[adx]))
            return(FalseObject);
    }

    return(TrueObject);
}

Define(">", GreaterThanPrimitive)(int argc, FObject argv[])
{
    if (argc < 2)
        RaiseExceptionC(R.Assertion, ">", "expected at least two arguments", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, ">", "expected a fixnum", List(argv[0]));

    for (int adx = 1; adx < argc; adx++)
    {
        if (FixnumP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, ">", "expected a fixnum", List(argv[adx]));

        if (AsFixnum(argv[adx - 1]) <= AsFixnum(argv[adx]))
            return(FalseObject);
    }

    return(TrueObject);
}

Define("<=", LessThanEqualPrimitive)(int argc, FObject argv[])
{
    if (argc < 2)
        RaiseExceptionC(R.Assertion, "<=", "expected at least two arguments", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "<=", "expected a fixnum", List(argv[0]));

    for (int adx = 1; adx < argc; adx++)
    {
        if (FixnumP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, "<=", "expected a fixnum", List(argv[adx]));

        if (AsFixnum(argv[adx - 1]) > AsFixnum(argv[adx]))
            return(FalseObject);
    }

    return(TrueObject);
}

Define(">=", GreaterThanEqualPrimitive)(int argc, FObject argv[])
{
    if (argc < 2)
        RaiseExceptionC(R.Assertion, ">=", "expected at least two arguments", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, ">=", "expected a fixnum", List(argv[0]));

    for (int adx = 1; adx < argc; adx++)
    {
        if (FixnumP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, ">=", "expected a fixnum", List(argv[adx]));

        if (AsFixnum(argv[adx - 1]) < AsFixnum(argv[adx]))
            return(FalseObject);
    }

    return(TrueObject);
}

Define("zero?", ZeroPPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "zero?", "expected one argument", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "zero?", "expected a fixnum", List(argv[0]));

    return(AsFixnum(argv[0]) == 0 ? TrueObject : FalseObject);
}

Define("positive?", PositivePPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "positive?", "expected one argument",
                EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "positive?", "expected a fixnum", List(argv[0]));

    return(AsFixnum(argv[0]) > 0 ? TrueObject : FalseObject);
}

Define("negative?", NegativePPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "negative?", "expected one argument",
                EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "negative?", "expected a fixnum", List(argv[0]));

    return(AsFixnum(argv[0]) < 0 ? TrueObject : FalseObject);
}

Define("odd?", OddPPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "odd?", "expected one argument", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "odd?", "expected a fixnum", List(argv[0]));

    return(AsFixnum(argv[0]) % 2 != 0 ? TrueObject : FalseObject);
}

Define("even?", EvenPPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "even?", "expected one argument", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "even?", "expected a fixnum", List(argv[0]));

    return(AsFixnum(argv[0]) % 2 == 0 ? TrueObject : FalseObject);
}

Define("exact-integer?", ExactIntegerPPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "exact-integer?", "expected one argument",
                EmptyListObject);

        return(FixnumP(argv[0]) ? TrueObject : FalseObject);
}

Define("expt", ExptPrimitive)(int argc, FObject argv[])
{
    if (argc != 2)
        RaiseExceptionC(R.Assertion, "expt", "expected two arguments", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "expt", "expected a fixnum", List(argv[0]));

    if (FixnumP(argv[1]) == 0)
        RaiseExceptionC(R.Assertion, "expt", "expected a fixnum", List(argv[1]));

    int x = AsFixnum(argv[0]);
    int n = AsFixnum(argv[1]);
    int ret = 1;
    while (n > 0)
    {
        ret *= x;
        n -= 1;
    }

    return(MakeFixnum(ret));
}

Define("abs", AbsPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "abs", "expected one argument", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "abs", "expected a fixnum", List(argv[0]));

    return(AsFixnum(argv[0]) < 0 ? MakeFixnum(- AsFixnum(argv[0])) : argv[0]);
}

Define("sqrt", SqrtPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "sqrt", "expected one argument", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "sqrt", "expected a fixnum", List(argv[0]));

    int n = AsFixnum(argv[0]);
    int x = 1;

    while (x * x < n)
        x += 1;

    return(MakeFixnum(x));
}

Define("number->string", NumberToStringPrimitive)(int argc, FObject argv[])
{
    if (argc != 2)
        RaiseExceptionC(R.Assertion, "number->string", "expected two arguments", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "number->string", "expected a fixnum", List(argv[0]));

    if (FixnumP(argv[1]) == 0)
        RaiseExceptionC(R.Assertion, "number->string", "expected a fixnum", List(argv[1]));

    FCh s[16];
    int sl = NumberAsString(AsFixnum(argv[0]), s, AsFixnum(argv[1]));
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
    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
