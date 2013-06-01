/*

Foment

*/

#include <stdio.h>
#include <string.h>
#include "foment.hpp"

#ifdef FOMENT_TEST
int RunTest(FObject env, int argc, char * argv[]);
#endif // FOMENT_TEST

static void LoadFile(FObject fn, FObject env)
{
    try
    {
        FObject port = OpenInputFile(fn, 1);

        for (;;)
        {
            FObject obj = Read(port, 1, 0);
            if (obj == EndOfFileObject)
                break;
            FObject ret = Eval(obj, env);
            if (ret != NoValueObject)
            {
                WritePretty(StandardOutput, ret, 0);
                PutCh(StandardOutput, '\n');
            }
        }
    }
    catch (FObject obj)
    {
        if (ExceptionP(obj) == 0)
            PutStringC(StandardOutput, "exception: ");
        WritePretty(StandardOutput, obj, 0);
        PutCh(StandardOutput, '\n');
    }
}

static int RunRepl(FObject env)
{
    int ln = 1;
    for (;;)
    {
        try
        {
            FCh s[16];
            int sl;

            PutCh(StandardOutput, '{');
            sl = NumberAsString(BytesAllocated, s, 10);
            PutString(StandardOutput, s, sl);
            PutStringC(StandardOutput, "} ");
            BytesAllocated = 0;

            sl = NumberAsString(ln, s, 10);
            PutString(StandardOutput, s, sl);
            PutStringC(StandardOutput, " =] ");

            FObject obj = Read(StandardInput, 1, 0);
            if (obj == EndOfFileObject)
                break;
            FObject ret = Eval(obj, env);
            if (ret != NoValueObject)
            {
                WritePretty(StandardOutput, ret, 0);
                PutCh(StandardOutput, '\n');
            }
        }
        catch (FObject obj)
        {
            if (ExceptionP(obj) == 0)
                PutStringC(StandardOutput, "exception: ");
            WritePretty(StandardOutput, obj, 0);
            PutCh(StandardOutput, '\n');
        }

        ln = GetLocation(StandardInput) + 1;
    }

    return(0);
}

static int Usage()
{
    printf(
//        "run a program:\n"
//        "    foment [OPTION]... PROGRAM [ARG]...\n"
        "compile and run a program:\n"
        "    foment [OPTION]... FILE [ARG]...\n"
//        "compile a program:\n"
//        "    foment -c [OPTION]... FILE\n"
        "interactive session (repl):\n"
        "    foment -i [OPTION]... [ARG]...\n\n"
//        "    -o PROGRAM        PROGRAM to output when compiling\n"
//        "    -d                debug: break on startup\n"
        "    -e EXPR           evaluate an expression\n"
        "    -p EXPR           evaluate and print an expression\n"
        "    -l FILE           load FILE\n"
        "    -A DIR            append a library search directory\n"
        "    -I DIR            prepend a library search directory\n"
        "    -L                interactive like library\n"
        );

    return(1);
}

typedef enum
{
#ifdef FOMENT_TEST
    TestMode,
#endif // FOMENT_TEST
    ProgramMode,
    ReplMode
} FMode;

int main(int argc, char * argv[])
{
#ifdef FOMENT_DEBUG
    printf("Foment (Debug) Scheme 0.1\n");
#else // FOMENT_DEBUG
    printf("Foment Scheme 0.1\n");
#endif // FOMENT_DEBUG

    try
    {
        SetupFoment(argc, argv);
    }
    catch (FObject obj)
    {
        printf("Unexpected exception: SetupFoment: %x\n", obj);
        WritePretty(StandardOutput, obj, 0);
        return(1);
    }

    try
    {
        FAssert(argc >= 1);

        FMode mode = ProgramMode;
        int adx = 1;
        while (adx < argc)
        {
            if (argv[adx][0] != '-')
                break;

            if (argv[adx][1] == 0 || argv[adx][2] != 0)
            {
                printf("error: unknown switch: %s\n", argv[adx]);
                return(Usage());
            }

            if (argv[adx][1] == 'i')
                mode = ReplMode;
            else if (argv[adx][1] == 'L')
                Config.InteractiveLikeLibrary = 1;
#ifdef FOMENT_TEST
            else if (argv[adx][1] == 't')
                mode = TestMode;
#endif // FOMENT_TEST
            else
            {
                adx += 1;
                if (adx == argc)
                {
                    printf("error: expected an argument following %s\n", argv[adx - 1]);
                    return(Usage());
                }

                switch (argv[adx - 1][1])
                {
                case 'e':
                case 'p':
                {
                    FObject ret = Eval(ReadStringC(argv[adx], 1), GetInteractionEnv());
                    if (argv[adx - 1][1] == 'p')
                    {
                        WritePretty(StandardOutput, ret, 0);
                        PutCh(StandardOutput, '\n');
                    }

                    break;
                }

                case 'l':
                    LoadFile(MakeStringC(argv[adx]), GetInteractionEnv());
                    break;

                case 'A':
                {
                    FObject lp = LibraryPath;

                    for (;;)
                    {
                        FAssert(PairP(lp));

                        if (Rest(lp) == EmptyListObject)
                            break;
                        lp = Rest(lp);
                    }

                    AsPair(lp)->Rest = MakePair(MakeStringC(argv[adx]), EmptyListObject);
                    break;
                }

                case 'I':
                    LibraryPath = MakePair(MakeStringC(argv[adx]), LibraryPath);
                    break;

                default:
                    printf("error: unknown switch: %s\n", argv[adx]);
                    return(Usage());
                }
            }

            adx += 1;
        }

        FAssert(adx <= argc);
        CommandLine = MakeCommandLine(argc - adx, argv + adx);

        if (mode == ReplMode || adx == argc)
            return(RunRepl(GetInteractionEnv()));
#ifdef FOMENT_TEST
        else if (mode == TestMode)
        {
            FObject env = MakeEnvironment(List(StringCToSymbol("test")), TrueObject);
            EnvironmentImportLibrary(env,
                    List(StringCToSymbol("scheme"), StringCToSymbol("base")));

            return(RunTest(env, argc - adx, argv + adx));
        }
#endif // FOMENT_TEST

        FAssert(mode == ProgramMode);

        if (adx < argc)
        {
            char * s = strrchr(argv[adx], PathCh);
            if (s != 0)
            {
                *s = 0;
                LibraryPath = MakePair(MakeStringC(argv[adx]), LibraryPath);
                *s = PathCh;
            }
        }
        
        
        
        return(0);
    }
    catch (FObject obj)
    {
        if (ExceptionP(obj) == 0)
            PutStringC(StandardOutput, "exception: ");
        WritePretty(StandardOutput, obj, 0);
        PutCh(StandardOutput, '\n');

        return(1);
    }
}
