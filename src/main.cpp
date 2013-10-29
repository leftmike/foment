/*

Foment

*/

#include <stdio.h>
#include <string.h>
#include "foment.hpp"

#if 0
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
            int_t sl;

            WriteCh(R.StandardOutput, '{');
            sl = NumberAsString(BytesAllocated, s, 10);
            WriteString(R.StandardOutput, s, sl);
            WriteStringC(R.StandardOutput, "}");
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
#endif // 0

static int Usage()
{
    printf(
        "compile and run the program in FILE:\n"
        "    foment [OPTION]... FILE [ARG]...\n"
        "    -A DIR            append a library search directory\n"
        "    -I DIR            prepend a library search directory\n"
        "interactive session (repl):\n"
        "    foment [OPTION]... [FLAG]... [ARG]...\n\n"
        "    -i                interactive session\n"
        "    -e EXPR           evaluate an expression\n"
        "    -p EXPR           evaluate and print an expression\n"
        "    -l FILE           load FILE\n"
        );

    return(-1);
}

static int MissingArgument(wchar_t * arg)
{
    printf("error: expected an argument following %S\n", arg);
    return(Usage());
}

static FObject MakeInvocation(int argc, wchar_t * argv[])
{
    uint_t sl = -1;

    for (int adx = 0; adx < argc; adx++)
        sl += wcslen(argv[adx]) + 1;

    FObject s = MakeString(0, sl);
    uint_t sdx = 0;

    for (int adx = 0; adx < argc; adx++)
    {
        sl = wcslen(argv[adx]);
        for (uint_t idx = 0; idx < sl; idx++)
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

static int ProgramMode(int adx, int argc, wchar_t * argv[])
{
    FAssert(adx < argc);

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

    FObject nam = MakeStringS(argv[adx]);

    adx += 1;
    R.CommandLine = MakePair(MakeInvocation(adx, argv), MakeCommandLine(argc - adx, argv + adx));

    FObject port = OpenInputFile(nam);
    if (TextualPortP(port) == 0)
    {
        printf("error: unable to open program: %S\n", argv[adx]);
        return(Usage());
    }

    FObject proc = CompileProgram(nam, port);

    ExecuteThunk(proc);
    return(0);
}

int wmain(int argc, wchar_t * argv[])
{
#ifdef FOMENT_DEBUG
    printf("Foment (Debug) Scheme 0.1\n");
#else // FOMENT_DEBUG
    printf("Foment Scheme 0.1\n");
#endif // FOMENT_DEBUG

    int adx = 1;
    while (adx < argc)
    {
        if (wcscmp(argv[adx], L"-no-inline-procedures") == 0)
            InlineProcedures = 0;
        else if (wcscmp(argv[adx], L"-no-inline-imports") == 0)
            InlineImports = 0;

        adx += 1;
    }

    FThreadState ts;

    try
    {
        SetupFoment(&ts, argc, argv);
    }
    catch (FObject obj)
    {
        printf("Unexpected exception: SetupFoment: %p\n", obj);
        Write(R.StandardOutput, obj, 0);
        return(1);
    }

    FAssert(argc >= 1);

    try
    {
        int adx = 1;
        while (adx < argc)
        {
            if (wcscmp(argv[adx], L"-A") == 0)
            {
                adx += 1;

                if (adx == argc)
                    return(MissingArgument(argv[adx - 1]));

                FObject lp = R.LibraryPath;

                for (;;)
                {
                    FAssert(PairP(lp));

                    if (Rest(lp) == EmptyListObject)
                        break;
                    lp = Rest(lp);
                }

//                AsPair(lp)->Rest = MakePair(MakeStringS(argv[adx]), EmptyListObject);
                SetRest(lp, MakePair(MakeStringS(argv[adx]), EmptyListObject));

                adx += 1;
            }
            else if (wcscmp(argv[adx], L"-I") == 0)
            {
                adx += 1;

                if (adx == argc)
                    return(MissingArgument(argv[adx - 1]));

                R.LibraryPath = MakePair(MakeStringS(argv[adx]), R.LibraryPath);

                adx += 1;
            }
            else if (wcscmp(argv[adx], L"-no-inline-procedures") == 0
                    || wcscmp(argv[adx], L"-no-inline-imports") == 0)
                adx += 1;
            else if (argv[adx][0] != '-')
                return(ProgramMode(adx, argc, argv));
            else
                break;
        }

        R.CommandLine = MakePair(MakeInvocation(adx, argv),
                MakeCommandLine(argc - adx, argv + adx));

        ExecuteThunk(R.InteractiveThunk);
        return(0);
//        return(RunRepl(GetInteractionEnv()));
    }
    catch (FObject obj)
    {
        if (ExceptionP(obj) == 0)
            WriteStringC(R.StandardOutput, "exception: ");
        Write(R.StandardOutput, obj, 0);
        WriteCh(R.StandardOutput, '\n');

        return(-1);
    }
}
