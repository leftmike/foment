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
        FObject obj = NoValueObject;
        FObject exp = NoValueObject;

        PushRoot(&env);
        PushRoot(&port);
        PushRoot(&obj);
        PushRoot(&exp);

        for (;;)
        {
            int ln = GetLocation(port) + 1;
            obj = Read(port, 1, 0);
            if (obj == EndOfFileObject)
                break;
            if (PairP(obj) && IdentifierP(First(obj))
                    && AsIdentifier(First(obj))->Symbol == MustEqualSymbol)
            {
                if (PairP(Rest(obj)) == 0 || PairP(Rest(Rest(obj))) == 0 ||
                        Rest(Rest(Rest(obj))) != EmptyListObject)
                {
                    PutStringC(R.StandardOutput,
                            "test: expected (must-equal <expected> <expression>): ");
                    WritePretty(R.StandardOutput, SyntaxToDatum(obj), 0);
                    PutCh(R.StandardOutput, '\n');

                    return(1);
                }

                FObject ret = Eval(First(Rest(Rest(obj))), env);
                exp = SyntaxToDatum(First(Rest(obj)));
                if (EqualP(ret, exp) == 0)
                {
                    FCh s[16];
                    int sl;

                    PutStringC(R.StandardOutput, fn);
                    PutCh(R.StandardOutput, '@');
                    sl = NumberAsString(ln, s, 10);
                    PutString(R.StandardOutput, s, sl);

                    PutStringC(R.StandardOutput, " must-equal: expected: ");
                    WritePretty(R.StandardOutput, exp, 0);
                    PutStringC(R.StandardOutput, " got: ");
                    WritePretty(R.StandardOutput, ret, 0);
                    PutCh(R.StandardOutput, '\n');

                    return(1);
                }
            }
            else if (PairP(obj) && IdentifierP(First(obj))
                    && AsIdentifier(First(obj))->Symbol == MustRaiseSymbol)
            {
                if (PairP(Rest(obj)) == 0 || PairP(Rest(Rest(obj))) == 0 ||
                        Rest(Rest(Rest(obj))) != EmptyListObject)
                {
                    PutStringC(R.StandardOutput,
                            "test: expected (must-raise <exception> <expression>): ");
                    WritePretty(R.StandardOutput, SyntaxToDatum(obj), 0);
                    PutCh(R.StandardOutput, '\n');

                    return(1);
                }

                exp = SyntaxToDatum(First(Rest(obj)));
                if (PairP(exp) == 0)
                {
                    PutStringC(R.StandardOutput,
                            "test: expected (<type> [<who>]) for <exception>: ");
                    WritePretty(R.StandardOutput, exp, 0);
                    PutCh(R.StandardOutput, '\n');

                    return(1);
                }

                try
                {
                    Eval(First(Rest(Rest(obj))), env);

                    FCh s[16];
                    int sl;

                    PutStringC(R.StandardOutput, fn);
                    PutCh(R.StandardOutput, '@');
                    sl = NumberAsString(ln, s, 10);
                    PutString(R.StandardOutput, s, sl);

                    PutStringC(R.StandardOutput, " must-raise: no exception raised\n");

                    return(1);
                }
                catch (FObject exc)
                {
                    if (ExceptionP(exc) == 0 || AsException(exc)->Type != First(exp)
                            || (PairP(Rest(exp)) && AsException(exc)->Who != First(Rest(exp))))
                    {
                        FCh s[16];
                        int sl;

                        PutStringC(R.StandardOutput, fn);
                        PutCh(R.StandardOutput, '@');
                        sl = NumberAsString(ln, s, 10);
                        PutString(R.StandardOutput, s, sl);

                        PutStringC(R.StandardOutput, " must-raise: expected: ");
                        WritePretty(R.StandardOutput, exp, 0);
                        PutStringC(R.StandardOutput, " got: ");
                        WritePretty(R.StandardOutput, exc, 0);
                        PutCh(R.StandardOutput, '\n');

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
            PutStringC(R.StandardOutput, "exception: ");
        WritePretty(R.StandardOutput, obj, 0);
        PutCh(R.StandardOutput, '\n');

        return(1);
    }

    PopRoot();
    PopRoot();
    PopRoot();
    PopRoot();

    return(0);
}

int RunTest(FObject env, int argc, char * argv[])
{
    int ret;

    PushRoot(&env);

    MustEqualSymbol = StringCToSymbol("must-equal");
    PushRoot(&MustEqualSymbol);

    MustRaiseSymbol = StringCToSymbol("must-raise");
    PushRoot(&MustRaiseSymbol);

    PutStringC(R.StandardOutput, "Testing.\n");

    for (int adx = 0; adx < argc; adx++)
    {
        PutStringC(R.StandardOutput, "    ");
        PutStringC(R.StandardOutput, argv[adx]);
        PutCh(R.StandardOutput, '\n');

        ret = TestFile(argv[adx], env);
        if (ret != 0)
        {
            PutStringC(R.StandardOutput, "Failed.");
            return(ret);
        }
    }

    PutStringC(R.StandardOutput, "Passed.\n");

    FCh s[16];
    int sl;

    PutStringC(R.StandardOutput, "Bytes Allocated: ");
    sl = NumberAsString(BytesAllocated, s, 10);
    PutString(R.StandardOutput, s, sl);
    PutCh(R.StandardOutput, '\n');

    AllowGC();

    return(0);
}
