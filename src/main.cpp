/*

Foment

*/

#include <stdio.h>
#include <string.h>
#include "foment.hpp"

#ifdef FOMENT_TEST
int RunTest(FObject env, int argc, SCh * argv[]);
#endif // FOMENT_TEST

static void LoadFile(FObject fn, FObject env)
{
    try
    {
        FObject port = OpenInputFile(fn);
        if (TextualPortP(port) == 0)
            RaiseExceptionC(R.Assertion, "open-input-file", "can not open file for reading",
                    List(fn));

        PushRoot(&port);
        PushRoot(&env);

        WantIdentifiersPort(port, 1);

        for (;;)
        {
            FObject obj = Read(port);
            if (obj == EndOfFileObject)
                break;
            FObject ret = Eval(obj, env);
            if (ret != NoValueObject)
            {
                Write(R.StandardOutput, ret, 0);
                WriteCh(R.StandardOutput, '\n');
            }
        }
    }
    catch (FObject obj)
    {
        if (ExceptionP(obj) == 0)
            WriteStringC(R.StandardOutput, "exception: ");
        Write(R.StandardOutput, obj, 0);
        WriteCh(R.StandardOutput, '\n');
    }

    PopRoot();
    PopRoot();
}

static int RunRepl(FObject env)
{
    PushRoot(&env);

    WantIdentifiersPort(R.StandardInput, 1);

    for (;;)
    {
        try
        {
            FCh s[16];
            int sl;

            WriteCh(R.StandardOutput, '{');
            sl = NumberAsString(BytesAllocated, s, 10);
            WriteString(R.StandardOutput, s, sl);
            WriteStringC(R.StandardOutput, "} ");
            BytesAllocated = 0;

//            sl = NumberAsString(GetLineColumn(R.StandardInput, 0), s, 10);
//            WriteString(R.StandardOutput, s, sl);
            WriteStringC(R.StandardOutput, " =] ");

            FObject obj = Read(R.StandardInput);
            if (obj == EndOfFileObject)
                break;
            FObject ret = Eval(obj, env);
            if (ret != NoValueObject)
            {
                Write(R.StandardOutput, ret, 0);
                WriteCh(R.StandardOutput, '\n');
            }
        }
        catch (FObject obj)
        {
            if (ExceptionP(obj) == 0)
                WriteStringC(R.StandardOutput, "exception: ");
            Write(R.StandardOutput, obj, 0);
            WriteCh(R.StandardOutput, '\n');
        }
    }

    PopRoot();

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

static FObject MakeInvocation(int argc, wchar_t * argv[])
{
    int sl = -1;

    for (int adx = 0; adx < argc; adx++)
        sl += wcslen(argv[adx]) + 1;

    FObject s = MakeString(0, sl);
    int sdx = 0;

    for (int adx = 0; adx < argc; adx++)
    {
        sl = wcslen(argv[adx]);
        for (int idx = 0; idx < sl; idx++)
        {
            AsString(s)->String[sdx] = argv[adx][idx];
            sdx += 1;
        }

        if (adx + 1 < argc)
        {
            AsString(s)->String[sdx] = ' ';
            sdx += 1;
        }
    }

    return(s);
}

typedef enum
{
#ifdef FOMENT_TEST
    TestMode,
#endif // FOMENT_TEST
    ProgramMode,
    ReplMode
} FMode;

int wmain(int argc, wchar_t * argv[])
{
#ifdef FOMENT_DEBUG
    printf("Foment (Debug) Scheme 0.1\n");
#else // FOMENT_DEBUG
    printf("Foment Scheme 0.1\n");
#endif // FOMENT_DEBUG

    FThreadState ts;

    try
    {
        SetupFoment(&ts, argc, argv);
    }
    catch (FObject obj)
    {
        printf("Unexpected exception: SetupFoment: %x\n", obj);
        Write(R.StandardOutput, obj, 0);
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
                    FObject ret = Eval(Read(MakeStringInputPort(MakeStringS(argv[adx]))),
                            GetInteractionEnv());
                    if (argv[adx - 1][1] == 'p')
                    {
                        Write(R.StandardOutput, ret, 0);
                        WriteCh(R.StandardOutput, '\n');
                    }

                    break;
                }

                case 'l':
                    LoadFile(MakeStringS(argv[adx]), GetInteractionEnv());
                    break;

                case 'A':
                {
                    FObject lp = R.LibraryPath;

                    for (;;)
                    {
                        FAssert(PairP(lp));

                        if (Rest(lp) == EmptyListObject)
                            break;
                        lp = Rest(lp);
                    }

//                    AsPair(lp)->Rest = MakePair(MakeStringS(argv[adx]), EmptyListObject);
                    SetRest(lp, MakePair(MakeStringS(argv[adx]), EmptyListObject));
                    break;
                }

                case 'I':
                    R.LibraryPath = MakePair(MakeStringS(argv[adx]), R.LibraryPath);
                    break;

                default:
                    printf("error: unknown switch: %s\n", argv[adx]);
                    return(Usage());
                }
            }

            adx += 1;
        }

        FAssert(adx <= argc);
        R.CommandLine = MakePair(MakeInvocation(adx, argv),
                MakeCommandLine(argc - adx, argv + adx));

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
            SCh * s = argv[adx];
            SCh * pth = 0;

            while (*s)
            {
                if (*s == PathCh)
                    pth = s;

                s += 1;
            }

            if (pth != 0)
                R.LibraryPath = MakePair(MakeStringS(argv[adx], pth - argv[adx]), R.LibraryPath);
        }
        
        
        
        return(0);
    }
    catch (FObject obj)
    {
        if (ExceptionP(obj) == 0)
            WriteStringC(R.StandardOutput, "exception: ");
        Write(R.StandardOutput, obj, 0);
        WriteCh(R.StandardOutput, '\n');

        return(1);
    }
}
