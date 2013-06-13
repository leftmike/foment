/*

Foment

*/

#include "foment.hpp"

Define("+", SumPrimitive)(int argc, FObject argv[])
{
    FFixnum ret = 0;
    for (int adx = 0; adx < argc; adx++)
    {
        if (FixnumP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, "+", "+: expected a fixnum", List(argv[adx]));

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
            RaiseExceptionC(R.Assertion, "*", "*: expected a fixnum", List(argv[adx]));

        ret *= AsFixnum(argv[adx]);
    }

    return(MakeFixnum(ret));
}

Define("-", DifferencePrimitive)(int argc, FObject argv[])
{
    if (argc < 1)
        RaiseExceptionC(R.Assertion, "-", "-: expected at least one argument", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "-", "-: expected a fixnum", List(argv[0]));

    if (argc == 1)
        return(MakeFixnum(-AsFixnum(argv[0])));

    FFixnum ret = AsFixnum(argv[0]);
    for (int adx = 1; adx < argc; adx++)
    {
        if (FixnumP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, "-", "-: expected a fixnum", List(argv[adx]));

        ret -= AsFixnum(argv[adx]);
    }

    return(MakeFixnum(ret));
}

Define("/", QuotientPrimitive)(int argc, FObject argv[])
{
    if (argc < 1)
        RaiseExceptionC(R.Assertion, "/", "/: expected at least one argument", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "/", "/: expected a fixnum", List(argv[0]));

    if (argc == 1)
    {
        if (AsFixnum(argv[0]) == 0)
            RaiseExceptionC(R.Assertion, "/", "/: zero not allowed", List(argv[0]));

        return(MakeFixnum(1 / AsFixnum(argv[0])));
    }

    FFixnum ret = AsFixnum(argv[0]);

    for (int adx = 1; adx < argc; adx++)
    {
        if (FixnumP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, "/", "/: expected a fixnum", List(argv[adx]));

        if (AsFixnum(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, "/", "/: zero not allowed", List(argv[adx]));

        ret /= AsFixnum(argv[adx]);
    }

    return(MakeFixnum(ret));
}

Define("=", EqualPrimitive)(int argc, FObject argv[])
{
    if (argc < 2)
        RaiseExceptionC(R.Assertion, "=", "=: expected at least two arguments", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "=", "=: expected a fixnum", List(argv[0]));

    for (int adx = 1; adx < argc; adx++)
    {
        if (FixnumP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, "=", "=: expected a fixnum", List(argv[adx]));

        if (AsFixnum(argv[adx - 1]) != AsFixnum(argv[adx]))
            return(FalseObject);
    }

    return(TrueObject);
}

Define("<", LessThanPrimitive)(int argc, FObject argv[])
{
    if (argc < 2)
        RaiseExceptionC(R.Assertion, "<", "<: expected at least two arguments", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "<", "<: expected a fixnum", List(argv[0]));

    for (int adx = 1; adx < argc; adx++)
    {
        if (FixnumP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, "<", "<: expected a fixnum", List(argv[adx]));

        if (AsFixnum(argv[adx - 1]) >= AsFixnum(argv[adx]))
            return(FalseObject);
    }

    return(TrueObject);
}

Define(">", GreaterThanPrimitive)(int argc, FObject argv[])
{
    if (argc < 2)
        RaiseExceptionC(R.Assertion, ">", ">: expected at least two arguments", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, ">", ">: expected a fixnum", List(argv[0]));

    for (int adx = 1; adx < argc; adx++)
    {
        if (FixnumP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, ">", ">: expected a fixnum", List(argv[adx]));

        if (AsFixnum(argv[adx - 1]) <= AsFixnum(argv[adx]))
            return(FalseObject);
    }

    return(TrueObject);
}

Define("<=", LessThanEqualPrimitive)(int argc, FObject argv[])
{
    if (argc < 2)
        RaiseExceptionC(R.Assertion, "<=", "<=: expected at least two arguments", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "<=", "<=: expected a fixnum", List(argv[0]));

    for (int adx = 1; adx < argc; adx++)
    {
        if (FixnumP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, "<=", "<=: expected a fixnum", List(argv[adx]));

        if (AsFixnum(argv[adx - 1]) > AsFixnum(argv[adx]))
            return(FalseObject);
    }

    return(TrueObject);
}

Define(">=", GreaterThanEqualPrimitive)(int argc, FObject argv[])
{
    if (argc < 2)
        RaiseExceptionC(R.Assertion, ">=", ">=: expected at least two arguments", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, ">=", ">=: expected a fixnum", List(argv[0]));

    for (int adx = 1; adx < argc; adx++)
    {
        if (FixnumP(argv[adx]) == 0)
            RaiseExceptionC(R.Assertion, ">=", ">=: expected a fixnum", List(argv[adx]));

        if (AsFixnum(argv[adx - 1]) < AsFixnum(argv[adx]))
            return(FalseObject);
    }

    return(TrueObject);
}

Define("zero?", ZeroPPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "zero?", "zero?: expected one argument", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "zero?", "zero?: expected a fixnum", List(argv[0]));

    return(AsFixnum(argv[0]) == 0 ? TrueObject : FalseObject);
}

Define("odd?", OddPPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "odd?", "odd?: expected one argument", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "odd?", "odd?: expected a fixnum", List(argv[0]));

    return(AsFixnum(argv[0]) % 2 != 0 ? TrueObject : FalseObject);
}

Define("even?", EvenPPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "even?", "even?: expected one argument", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "even?", "even?: expected a fixnum", List(argv[0]));

    return(AsFixnum(argv[0]) % 2 == 0 ? TrueObject : FalseObject);
}

Define("expt", ExptPrimitive)(int argc, FObject argv[])
{
    if (argc != 2)
        RaiseExceptionC(R.Assertion, "expt", "expt: expected two arguments", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "expt", "expt: expected a fixnum", List(argv[0]));

    if (FixnumP(argv[1]) == 0)
        RaiseExceptionC(R.Assertion, "expt", "expt: expected a fixnum", List(argv[1]));

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
        RaiseExceptionC(R.Assertion, "abs", "abs: expected one argument", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "abs", "abs: expected a fixnum", List(argv[0]));

    return(AsFixnum(argv[0]) < 0 ? MakeFixnum(- AsFixnum(argv[0])) : argv[0]);
}

Define("sqrt", SqrtPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "sqrt", "sqrt: expected one argument", EmptyListObject);

    if (FixnumP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "sqrt", "sqrt: expected a fixnum", List(argv[0]));

    int n = AsFixnum(argv[0]);
    int x = 1;

    while (x * x < n)
        x += 1;

    return(MakeFixnum(x));
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
    &OddPPrimitive,
    &EvenPPrimitive,
    &ExptPrimitive,
    &AbsPrimitive,
    &SqrtPrimitive
};

void SetupNumbers()
{
    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
