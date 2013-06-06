/*

Foment

*/

#include "foment.hpp"

static FObject MustEqualSymbol;
static FObject MustRaiseSymbol;

static int TestFile(char * fn, FObject env)
{
    try
    {
        FObject port = OpenInputFile(MakeStringC(fn), 1);

        for (;;)
        {
            int ln = GetLocation(port) + 1;
            FObject obj = Read(port, 1, 0);
            if (obj == EndOfFileObject)
                break;
            if (PairP(obj) && IdentifierP(First(obj))
                    && AsIdentifier(First(obj))->Symbol == MustEqualSymbol)
            {
                if (PairP(Rest(obj)) == 0 || PairP(Rest(Rest(obj))) == 0 ||
                        Rest(Rest(Rest(obj))) != EmptyListObject)
                {
                    PutStringC(StandardOutput,
                            "test: expected (must-equal <expected> <expression>): ");
                    WritePretty(StandardOutput, SyntaxToDatum(obj), 0);
                    PutCh(StandardOutput, '\n');

                    return(1);
                }

                FObject ret = Eval(First(Rest(Rest(obj))), env);
                FObject exp = SyntaxToDatum(First(Rest(obj)));
                if (EqualP(ret, exp) == 0)
                {
                    FCh s[16];
                    int sl;

                    PutStringC(StandardOutput, fn);
                    PutCh(StandardOutput, '@');
                    sl = NumberAsString(ln, s, 10);
                    PutString(StandardOutput, s, sl);

                    PutStringC(StandardOutput, " must-equal: expected: ");
                    WritePretty(StandardOutput, exp, 0);
                    PutStringC(StandardOutput, " got: ");
                    WritePretty(StandardOutput, ret, 0);
                    PutCh(StandardOutput, '\n');

                    return(1);
                }
            }
            else if (PairP(obj) && IdentifierP(First(obj))
                    && AsIdentifier(First(obj))->Symbol == MustRaiseSymbol)
            {
                if (PairP(Rest(obj)) == 0 || PairP(Rest(Rest(obj))) == 0 ||
                        Rest(Rest(Rest(obj))) != EmptyListObject)
                {
                    PutStringC(StandardOutput,
                            "test: expected (must-raise <exception> <expression>): ");
                    WritePretty(StandardOutput, SyntaxToDatum(obj), 0);
                    PutCh(StandardOutput, '\n');

                    return(1);
                }

                FObject exp = SyntaxToDatum(First(Rest(obj)));
                if (PairP(exp) == 0)
                {
                    PutStringC(StandardOutput,
                            "test: expected (<type> [<who>]) for <exception>: ");
                    WritePretty(StandardOutput, exp, 0);
                    PutCh(StandardOutput, '\n');

                    return(1);
                }

                try
                {
                    Eval(First(Rest(Rest(obj))), env);

                    FCh s[16];
                    int sl;

                    PutStringC(StandardOutput, fn);
                    PutCh(StandardOutput, '@');
                    sl = NumberAsString(ln, s, 10);
                    PutString(StandardOutput, s, sl);

                    PutStringC(StandardOutput, " must-raise: no exception raised\n");

                    return(1);
                }
                catch (FObject exc)
                {
                    if (ExceptionP(exc) == 0 || AsException(exc)->Type != First(exp)
                            || (PairP(Rest(exp)) && AsException(exc)->Who != First(Rest(exp))))
                    {
                        FCh s[16];
                        int sl;

                        PutStringC(StandardOutput, fn);
                        PutCh(StandardOutput, '@');
                        sl = NumberAsString(ln, s, 10);
                        PutString(StandardOutput, s, sl);

                        PutStringC(StandardOutput, " must-raise: expected: ");
                        WritePretty(StandardOutput, exp, 0);
                        PutStringC(StandardOutput, " got: ");
                        WritePretty(StandardOutput, exc, 0);
                        PutCh(StandardOutput, '\n');

                        return(1);
                    }
                }
            }
            else
                Eval(obj, env);
        }
    }
    catch (FObject obj)
    {
        if (ExceptionP(obj) == 0)
            PutStringC(StandardOutput, "exception: ");
        WritePretty(StandardOutput, obj, 0);
        PutCh(StandardOutput, '\n');

        return(1);
    }

    return(0);
}

int RunTest(FObject env, int argc, char * argv[])
{
    int ret;

    MustEqualSymbol = StringCToSymbol("must-equal");
    MustRaiseSymbol = StringCToSymbol("must-raise");

    PutStringC(StandardOutput, "Testing.\n");

    for (int adx = 0; adx < argc; adx++)
    {
        PutStringC(StandardOutput, "    ");
        PutStringC(StandardOutput, argv[adx]);
        PutCh(StandardOutput, '\n');

        ret = TestFile(argv[adx], env);
        if (ret != 0)
        {
            PutStringC(StandardOutput, "Failed.");
            return(ret);
        }
    }

    PutStringC(StandardOutput, "Passed.\n");

    FCh s[16];
    int sl;

    PutStringC(StandardOutput, "Bytes Allocated: ");
    sl = NumberAsString(BytesAllocated, s, 10);
    PutString(StandardOutput, s, sl);
    PutCh(StandardOutput, '\n');

AllowGC();

    return(0);
}
