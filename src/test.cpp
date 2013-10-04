/*

Foment

*/

#include "foment.hpp"

static FObject MustEqualSymbol;
static FObject MustRaiseSymbol;

static int TestFile(SCh * fn, FObject env)
{
    try
    {
        FObject sfn = MakeStringS(fn);
        FObject port = OpenInputFile(sfn);
        if (TextualPortP(port) == 0)
            RaiseExceptionC(R.Assertion, "open-input-file", "can not open file for reading",
                    List(sfn));

        FObject obj = NoValueObject;
        FObject exp = NoValueObject;

        PushRoot(&env);
        PushRoot(&sfn);
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
                    WriteStringC(R.StandardOutput,
                            "test: expected (must-equal <expected> <expression>): ");
                    WritePretty(R.StandardOutput, SyntaxToDatum(obj), 0);
                    WriteCh(R.StandardOutput, '\n');

                    return(1);
                }

                FObject ret = Eval(First(Rest(Rest(obj))), env);
                exp = SyntaxToDatum(First(Rest(obj)));
                if (EqualP(ret, exp) == 0)
                {
                    FCh s[16];
                    int sl;

                    Write(R.StandardOutput, sfn, 1);
                    WriteCh(R.StandardOutput, '@');
                    sl = NumberAsString(ln, s, 10);
                    WriteString(R.StandardOutput, s, sl);

                    WriteStringC(R.StandardOutput, " must-equal: expected: ");
                    WritePretty(R.StandardOutput, exp, 0);
                    WriteStringC(R.StandardOutput, " got: ");
                    WritePretty(R.StandardOutput, ret, 0);
                    WriteCh(R.StandardOutput, '\n');

                    return(1);
                }
            }
            else if (PairP(obj) && IdentifierP(First(obj))
                    && AsIdentifier(First(obj))->Symbol == MustRaiseSymbol)
            {
                if (PairP(Rest(obj)) == 0 || PairP(Rest(Rest(obj))) == 0 ||
                        Rest(Rest(Rest(obj))) != EmptyListObject)
                {
                    WriteStringC(R.StandardOutput,
                            "test: expected (must-raise <exception> <expression>): ");
                    WritePretty(R.StandardOutput, SyntaxToDatum(obj), 0);
                    WriteCh(R.StandardOutput, '\n');

                    return(1);
                }

                exp = SyntaxToDatum(First(Rest(obj)));
                if (PairP(exp) == 0)
                {
                    WriteStringC(R.StandardOutput,
                            "test: expected (<type> [<who>]) for <exception>: ");
                    WritePretty(R.StandardOutput, exp, 0);
                    WriteCh(R.StandardOutput, '\n');

                    return(1);
                }

                try
                {
                    Eval(First(Rest(Rest(obj))), env);

                    FCh s[16];
                    int sl;

                    Write(R.StandardOutput, sfn, 1);
                    WriteCh(R.StandardOutput, '@');
                    sl = NumberAsString(ln, s, 10);
                    WriteString(R.StandardOutput, s, sl);

                    WriteStringC(R.StandardOutput, " must-raise: no exception raised\n");

                    return(1);
                }
                catch (FObject exc)
                {
                    if (ExceptionP(exc) == 0 || AsException(exc)->Type != First(exp)
                            || (PairP(Rest(exp)) && AsException(exc)->Who != First(Rest(exp))))
                    {
                        FCh s[16];
                        int sl;

                        Write(R.StandardOutput, sfn, 1);
                        WriteCh(R.StandardOutput, '@');
                        sl = NumberAsString(ln, s, 10);
                        WriteString(R.StandardOutput, s, sl);

                        WriteStringC(R.StandardOutput, " must-raise: expected: ");
                        WritePretty(R.StandardOutput, exp, 0);
                        WriteStringC(R.StandardOutput, " got: ");
                        WritePretty(R.StandardOutput, exc, 0);
                        WriteCh(R.StandardOutput, '\n');

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
            WriteStringC(R.StandardOutput, "exception: ");
        WritePretty(R.StandardOutput, obj, 0);
        WriteCh(R.StandardOutput, '\n');

        return(1);
    }

    PopRoot();
    PopRoot();
    PopRoot();
    PopRoot();
    PopRoot();

    return(0);
}

int RunTest(FObject env, int argc, SCh * argv[])
{
    int ret;

    PushRoot(&env);

    MustEqualSymbol = StringCToSymbol("must-equal");
    PushRoot(&MustEqualSymbol);

    MustRaiseSymbol = StringCToSymbol("must-raise");
    PushRoot(&MustRaiseSymbol);

    WriteStringC(R.StandardOutput, "Testing.\n");

    for (int adx = 0; adx < argc; adx++)
    {
        WriteStringC(R.StandardOutput, "    ");
        Write(R.StandardOutput, MakeStringS(argv[adx]), 1);
        WriteCh(R.StandardOutput, '\n');

        ret = TestFile(argv[adx], env);
        if (ret != 0)
        {
            WriteStringC(R.StandardOutput, "Failed.");
            return(ret);
        }
    }

    WriteStringC(R.StandardOutput, "Passed.\n");

    FCh s[16];
    int sl;

    WriteStringC(R.StandardOutput, "Bytes Allocated: ");
    sl = NumberAsString(BytesAllocated, s, 10);
    WriteString(R.StandardOutput, s, sl);
    WriteStringC(R.StandardOutput, " Collection Count: ");
    sl = NumberAsString(CollectionCount, s, 10);
    WriteString(R.StandardOutput, s, sl);
    WriteCh(R.StandardOutput, '\n');

    return(0);
}
