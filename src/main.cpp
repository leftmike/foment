/*

Foment

*/

#ifdef FOMENT_UNIX
#include <sys/stat.h>
#include <unistd.h>
#endif // FOMENT_UNIX
#include <stdio.h>
#include <string.h>
#include "foment.hpp"

#ifdef FOMENT_UNIX
#ifdef FOMENT_BSD
#include <stdlib.h>
#else // FOMENT_BSD
#include <alloca.h>
#endif // FOMENT_BSD
#endif // FOMENT_UNIX

#ifdef FOMENT_WINDOWS
#include <wchar.h>
#define StringLengthS(s) wcslen(s)
#define StringCompareS(s1, s2) wcscmp(s1, L ## s2)
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
#define StringLengthS(s) strlen(s)
#define StringCompareS(s1, s2) strcmp(s1, s2)
#endif // FOMENT_UNIX

static int Usage()
{
    printf(
        "compile and run the program in FILE:\n"
        "    foment [OPTION]... FILE [ARG]...\n"
        "    -A DIR            append a library search directory\n"
        "    -I DIR            prepend a library search directory\n"
        "    -X EXT            add EXT as a possible filename extensions for libraries\n"
        "interactive session (repl):\n"
        "    foment [OPTION]... [FLAG]... [ARG]...\n\n"
        "    -i                interactive session\n"
        "    -e EXPR           evaluate an expression\n"
        "    -p EXPR           evaluate and print an expression\n"
        "    -l FILE           load FILE\n"
        );

    return(-1);
}

static int MissingArgument(FChS * arg)
{
#ifdef FOMENT_WINDOWS
    printf("error: expected an argument following %S\n", arg);
#endif // FOMENT_WINDOWS
#ifdef FOMENT_UNIX
    printf("error: expected an argument following %s\n", arg);
#endif // FOMENT_UNIX
    return(Usage());
}

static FObject MakeInvocation(int argc, FChS * argv[])
{
    uint_t sl = -1;

    for (int adx = 0; adx < argc; adx++)
        sl += StringLengthS(argv[adx]) + 1;

    FObject s = MakeString(0, sl);
    uint_t sdx = 0;

    for (int adx = 0; adx < argc; adx++)
    {
        sl = StringLengthS(argv[adx]);
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

static FObject MakeCommandLine(int_t argc, FChS * argv[])
{
    FObject cl = EmptyListObject;

    while (argc > 0)
    {
        argc -= 1;
        cl = MakePair(MakeStringS(argv[argc]), cl);
    }

    return(cl);
}

#ifdef FOMENT_UNIX
static void AddToLibraryPath(FChS * prog)
{
    struct stat st;

    if (lstat(prog, &st) != -1 && S_ISLNK(st.st_mode))
    {
        FChS * link = (FChS *) alloca(st.st_size + 1);
        size_t ret = readlink(prog, link, st.st_size + 1);
        link[ret] = 0;

        AddToLibraryPath(link);
    }
    else
    {
        FChS * s = prog;
        FChS * pth = 0;
        while (*s)
        {
            if (PathChP(*s))
                pth = s;

            s += 1;
        }

        if (pth != 0)
            R.LibraryPath = MakePair(MakeStringS(prog, pth - prog), R.LibraryPath);
        else
            R.LibraryPath = MakePair(MakeStringC("."), R.LibraryPath);
    }
}
#else // FOMENT_UNIX

static void AddToLibraryPath(FChS * prog)
{
    FChS * s = prog;
    FChS * pth = 0;
    while (*s)
    {
        if (PathChP(*s))
            pth = s;

        s += 1;
    }

    if (pth != 0)
        R.LibraryPath = MakePair(MakeStringS(prog, pth - prog), R.LibraryPath);
    else
        R.LibraryPath = MakePair(MakeStringC("."), R.LibraryPath);
}
#endif // FOMENT_UNIX

static int ProgramMode(int adx, int argc, FChS * argv[])
{
    FAssert(adx < argc);

    FObject nam = MakeStringS(argv[adx]);

    adx += 1;
    R.CommandLine = MakePair(MakeInvocation(adx, argv), MakeCommandLine(argc - adx, argv + adx));

    FObject port;
    {
        FDontWait dw;

        port = OpenInputFile(nam);
        if (TextualPortP(port) == 0)
        {
#ifdef FOMENT_WINDOWS
            printf("error: unable to open program: %S\n", argv[adx - 1]);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
            printf("error: unable to open program: %s\n", argv[adx - 1]);
#endif // FOMENT_UNIX
            return(Usage());
        }
    }

    FCh ch;

    // Skip #!/usr/local/bin/foment

    if (PeekCh(port, &ch) && ch == '#')
        ReadLine(port);

    FObject proc = CompileProgram(nam, port);

    ExecuteThunk(proc);
    ExitFoment();
    return(0);
}

#ifdef FOMENT_WINDOWS
int wmain(int argc, FChS * argv[])
#endif // FOMENT_WINDOWS
#ifdef FOMENT_UNIX
int main(int argc, char * argv[])
#endif // FOMENT_UNIX
{
    int_t pdx = 0;
    int adx = 1;
    while (adx < argc)
    {
        if (StringCompareS(argv[adx], "-A") == 0)
            adx += 2;
        else if (StringCompareS(argv[adx], "-I") == 0)
            adx += 2;
        else if (StringCompareS(argv[adx], "-X") == 0)
            adx += 2;
        else if (argv[adx][0] != '-')
        {
            pdx = adx;
            break;
        }
        else if (StringCompareS(argv[adx], "-no-inline-procedures") == 0)
        {
            InlineProcedures = 0;
            adx += 1;
        }
        else if (StringCompareS(argv[adx], "-no-inline-imports") == 0)
        {
            InlineImports = 0;
            adx += 1;
        }
        else if (StringCompareS(argv[adx], "--validate-heap") == 0)
        {
            ValidateHeap = 1;
            adx += 1;
        }
#ifdef FOMENT_WINDOWS
        else if (StringCompareS(argv[adx], "--section-table") == 0)
        {
            adx += 1;

            if (adx < argc)
            {
#ifdef FOMENT_32BIT
                SectionTableBase = (void *) wcstol(argv[adx], 0, 16);
#endif // FOMENT_32BIT
#ifdef FOMENT_64BIT
                SectionTableBase = (void *) _wcstoui64(argv[adx], 0, 16);
#endif // FOMENT_64BIT

                adx += 1;
            }
        }
#endif // FOMENT_WINDOWS
        else
            break;
    }

    FThreadState ts;

    try
    {
        SetupFoment(&ts);

        if (pdx > 0)
        {
            AddToLibraryPath(argv[pdx]);
        }
    }
    catch (FObject obj)
    {
        printf("Unexpected exception: SetupFoment: %p\n", obj);
        WriteSimple(R.StandardOutput, obj, 0);

        if (ValidateHeap)
        {
            FailedGC();
            FailedExecute();
        }
        return(1);
    }

    FAssert(argc >= 1);

    try
    {
        int adx = 1;
        while (adx < argc)
        {
            if (StringCompareS(argv[adx], "-A") == 0)
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
            else if (StringCompareS(argv[adx], "-I") == 0)
            {
                adx += 1;

                if (adx == argc)
                    return(MissingArgument(argv[adx - 1]));

                R.LibraryPath = MakePair(MakeStringS(argv[adx]), R.LibraryPath);

                adx += 1;
            }
            else if (StringCompareS(argv[adx], "-X") == 0)
            {
                adx += 1;

                if (adx == argc)
                    return(MissingArgument(argv[adx - 1]));

                R.LibraryExtensions = MakePair(MakeStringS(argv[adx]), R.LibraryExtensions);

                adx += 1;
            }
            else if (StringCompareS(argv[adx], "-no-inline-procedures") == 0
                    || StringCompareS(argv[adx], "-no-inline-imports") == 0
                    || StringCompareS(argv[adx], "--validate-heap") == 0)
                adx += 1;
#ifdef FOMENT_WINDOWS
            else if (StringCompareS(argv[adx], "--section-table") == 0)
                adx += 2;
#endif // FOMENT_WINDOWS
            else if (argv[adx][0] != '-')
                return(ProgramMode(adx, argc, argv));
            else
                break;
        }

        R.LibraryPath = ReverseListModify(MakePair(MakeStringC("."), R.LibraryPath));
        R.CommandLine = MakePair(MakeInvocation(adx, argv),
                MakeCommandLine(argc - adx, argv + adx));

        ExecuteThunk(R.InteractiveThunk);
        ExitFoment();
        return(0);
//        return(RunRepl(GetInteractionEnv()));
    }
    catch (FObject obj)
    {
        if (ExceptionP(obj) == 0)
            WriteStringC(R.StandardOutput, "exception: ");
        WriteSimple(R.StandardOutput, obj, 0);
        WriteCh(R.StandardOutput, '\n');

        if (ValidateHeap)
        {
            FailedGC();
            FailedExecute();
        }
        return(-1);
    }
}
