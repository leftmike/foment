/*

Foment

*/

#ifdef FOMENT_UNIX
#include <sys/stat.h>
#include <unistd.h>
#endif // FOMENT_UNIX
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "foment.hpp"

#ifdef FOMENT_UNIX
#ifndef FOMENT_BSD
#include <alloca.h>
#endif // FOMENT_BSD
#endif // FOMENT_UNIX

#ifdef FOMENT_WINDOWS
#include <wchar.h>
#define StringLengthS(s) wcslen(s)
#define StringCompareS(s1, s2) wcscmp(s1, L ## s2)
#define StringToInt(s) _wtoi(s)
#define HexStringToInt32(s) wcstol(s, 0, 16)
#define HexStringToInt64(s) _wcstoui64(s, 0, 16);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
#define StringLengthS(s) strlen(s)
#define StringCompareS(s1, s2) strcmp(s1, s2)
#define StringToInt(s) atoi(s)
#define HexStringToInt32(s) strtol(s, 0, 16)
#define HexStringToInt64(s) strtoll(s, 0, 16);
#endif // FOMENT_UNIX

static int InteractiveFlag = 0;

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

// ---- Configuration ----

typedef enum
{
    NeverConfig,
    EarlyConfig,
    LateConfig,
    AnytimeConfig
} FConfigWhen;

typedef enum
{
    UIntConfig,
    BoolConfig,
    GetConfig,
    SetConfig,
    ActionConfig,
    ArgConfig
} FConfigType;

typedef FObject (*FGetConfigFn)();
typedef void (*FSetConfigFn)(FObject obj);
typedef void (*FActionConfigFn)();
typedef void (*FArgConfigFn)(FChS * s);

typedef struct
{
    char ShortName;
    const char * LongName;
    FConfigWhen When;
    FConfigType Type;
    union
    {
        uint_t * UIntValue;
        uint_t * BoolValue;
        FGetConfigFn GetConfigFn;
        FSetConfigFn SetConfigFn;
        FActionConfigFn ActionConfigFn;
        FArgConfigFn ArgConfigFn;
    };
} FConfigOption;

static FObject GetCollector()
{
    if (CollectorType == NoCollector)
        return(StringCToSymbol("none"));
    else if (CollectorType == MarkSweepCollector)
        return(StringCToSymbol("mark-sweep"));

    FAssert(CollectorType == GenerationalCollector);

    return(StringCToSymbol("generational"));
}

static void SetNoCollector()
{
    CollectorType = NoCollector;
}

static void SetMarkSweep()
{
    CollectorType = MarkSweepCollector;
}

static void SetGenerational()
{
    CollectorType = GenerationalCollector;
}

static FObject GetLibraryPath()
{
    return(R.LibraryPath);
}

static void AppendLibraryPath(FObject obj)
{
    FObject lp = R.LibraryPath;

    for (;;)
    {
        FAssert(PairP(lp));

        if (Rest(lp) == EmptyListObject)
            break;
        lp = Rest(lp);
    }

//    AsPair(lp)->Rest = MakePair(MakeStringS(argv[adx]), EmptyListObject);
    SetRest(lp, MakePair(obj, EmptyListObject));
}

static void PrependLibraryPath(FObject obj)
{
    R.LibraryPath = MakePair(obj, R.LibraryPath);
}

static FObject GetLibraryExtensions()
{
    return(R.LibraryExtensions);
}

static void AddExtension(FObject obj)
{
    R.LibraryExtensions = MakePair(obj, R.LibraryExtensions);
}

static void SetInteractive()
{
    InteractiveFlag = 1;
}

static void SetInteractiveArg(FChS * s)
{
    InteractiveFlag = 1;
}

static FConfigOption ConfigOptions[] =
{
    {0, "library-path", NeverConfig, GetConfig, {.GetConfigFn = GetLibraryPath}},
    {'A', "append", LateConfig, SetConfig, {.SetConfigFn = AppendLibraryPath}},
    {'I', "prepend", LateConfig, SetConfig, {.SetConfigFn = PrependLibraryPath}},

    {0, "library-extensions", NeverConfig, GetConfig, {.GetConfigFn = GetLibraryExtensions}},
    {'X', "extension", LateConfig, SetConfig, {.SetConfigFn = AddExtension}},

    {'v', "verbose", AnytimeConfig, BoolConfig, {.BoolValue = &VerboseFlag}},
    {0, "random-seed", EarlyConfig, UIntConfig, {.UIntValue = &RandomSeed}},

    {0, "collector", NeverConfig, GetConfig, {.GetConfigFn = GetCollector}},
    {0, "no-collector", EarlyConfig, ActionConfig, {.ActionConfigFn = SetNoCollector}},
    {0, "mark-sweep", EarlyConfig, ActionConfig, {.ActionConfigFn = SetMarkSweep}},
    {0, "generational", EarlyConfig, ActionConfig, {.ActionConfigFn = SetGenerational}},
    {0, "check-heap", AnytimeConfig, BoolConfig, {.BoolValue = &CheckHeapFlag}},

    {0, "maximum-stack-size", EarlyConfig, UIntConfig, {.UIntValue = &MaximumStackSize}},
    {0, "maximum-babies-size", EarlyConfig, UIntConfig, {.UIntValue = &MaximumBabiesSize}},
    {0, "maximum-kids-size", EarlyConfig, UIntConfig, {.UIntValue = &MaximumKidsSize}},
    {0, "maximum-adults-size", EarlyConfig, UIntConfig, {.UIntValue = &MaximumAdultsSize}},
    {0, "maximum-generational-baby", AnytimeConfig, UIntConfig,
            {.UIntValue = &MaximumGenerationalBaby}},
    {0, "trigger-bytes", AnytimeConfig, UIntConfig, {.UIntValue = &TriggerBytes}},
    {0, "trigger-objects", AnytimeConfig, UIntConfig, {.UIntValue = &TriggerObjects}},
    {0, "partial-per-full", AnytimeConfig, UIntConfig, {.UIntValue = &PartialPerFull}},

    {'i', "interactive", EarlyConfig, ActionConfig, {.ActionConfigFn = SetInteractive}},
    {0, "repl", EarlyConfig, ActionConfig, {.ActionConfigFn = SetInteractive}},
    {'l', "load", EarlyConfig, ArgConfig, {.ArgConfigFn = SetInteractiveArg}},
    {'p', "print", EarlyConfig, ArgConfig, {.ArgConfigFn = SetInteractiveArg}},
    {'e', "eval", EarlyConfig, ArgConfig, {.ArgConfigFn = SetInteractiveArg}},
    {0, "evaluate", EarlyConfig, ArgConfig, {.ArgConfigFn = SetInteractiveArg}}
};

Define("config", ConfigPrimitive)(int_t argc, FObject argv[])
{
    ZeroArgsCheck("config", argc);

    FObject ret = EmptyListObject;

    for (int_t idx = 0; idx < sizeof(ConfigOptions) / sizeof(FConfigOption); idx++)
    {
        switch (ConfigOptions[idx].Type)
        {
        case UIntConfig:
            ret = MakePair(MakePair(StringCToSymbol(ConfigOptions[idx].LongName),
                    MakeIntegerU(*ConfigOptions[idx].UIntValue)), ret);
            break;

        case BoolConfig:
            ret = MakePair(MakePair(StringCToSymbol(ConfigOptions[idx].LongName),
                    *ConfigOptions[idx].BoolValue ? TrueObject : FalseObject), ret);
            break;

        case GetConfig:
            ret = MakePair(MakePair(StringCToSymbol(ConfigOptions[idx].LongName),
                    ConfigOptions[idx].GetConfigFn()), ret);
            break;

        case SetConfig:
        case ActionConfig:
        case ArgConfig:
            break;

        default:
            FAssert(0);
        }
    }

    return(ret);
}

Define("set-config!", SetConfigPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("set-config!", argc);
    SymbolArgCheck("set-config!", argv[0]);

    FConfigOption * cfg = 0;
    for (int_t idx = 0; idx < sizeof(ConfigOptions) / sizeof(FConfigOption); idx++)
    {
        if (StringCToSymbol(ConfigOptions[idx].LongName) == argv[0])
        {
            cfg = ConfigOptions + idx;
            if (cfg->Type != GetConfig)
                break;
        }
    }

    if (cfg == 0)
        RaiseExceptionC(R.Assertion, "set-config!", "expected a config option", List(argv[0]));

    if (cfg->When != AnytimeConfig)
        RaiseExceptionC(R.Assertion, "set-config!", "option may not be configured now",
                List(argv[0]));

    switch (cfg->Type)
    {
    case UIntConfig:
        FixnumArgCheck("set-config!", argv[1]);
        *cfg->UIntValue = AsFixnum(argv[1]);
        break;

    case BoolConfig:
        BooleanArgCheck("set-config!", argv[1]);
        *cfg->BoolValue = argv[1] == TrueObject ? 1 : 0;
        break;

    case SetConfig:
        StringArgCheck("set-config!", argv[1]);
        cfg->SetConfigFn(argv[1]);
        break;

    default:
        RaiseExceptionC(R.Assertion, "set-config!", "option may not be configured now",
                List(argv[0]));
        break;
    }

    return(NoValueObject);
}

static FPrimitive * Primitives[] =
{
    &ConfigPrimitive,
    &SetConfigPrimitive
};

void SetupMain()
{
    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
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
        else if (StringCompareS(argv[adx], "--check-heap") == 0)
        {
            CheckHeapFlag = 1;
            adx += 1;
        }
        else if (StringCompareS(argv[adx], "--verbose") == 0)
        {
            VerboseFlag = 1;
            adx += 1;
        }
        else if (StringCompareS(argv[adx], "--random-seed") == 0)
        {
            adx += 1;

            if (adx < argc)
            {
                RandomSeed = StringToInt(argv[adx]);
                adx += 1;
            }
        }
        else
            break;
    }

    FThreadState ts;

    try
    {
        if (SetupFoment(&ts) == 0)
        {
            printf("SetupFoment: out of memory\n");
            return(1);
        }

        if (pdx > 0)
            AddToLibraryPath(argv[pdx]);
    }
    catch (FObject obj)
    {
        printf("Unexpected exception: SetupFoment: %p\n", obj);
        WriteSimple(R.StandardOutput, obj, 0);
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

                AppendLibraryPath(MakeStringS(argv[adx]));
                adx += 1;
            }
            else if (StringCompareS(argv[adx], "-I") == 0)
            {
                adx += 1;
                if (adx == argc)
                    return(MissingArgument(argv[adx - 1]));

                PrependLibraryPath(MakeStringS(argv[adx]));
                adx += 1;
            }
            else if (StringCompareS(argv[adx], "-X") == 0)
            {
                adx += 1;
                if (adx == argc)
                    return(MissingArgument(argv[adx - 1]));

                AddExtension(MakeStringS(argv[adx]));
                adx += 1;
            }
            else if (StringCompareS(argv[adx], "--check-heap") == 0
                    || StringCompareS(argv[adx], "--verbose") == 0)
                adx += 1;
            else if (StringCompareS(argv[adx], "--random-seed") == 0)
                adx += 2;
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
    }
    catch (FObject obj)
    {
        if (ExceptionP(obj) == 0)
            WriteStringC(R.StandardOutput, "exception: ");
        WriteSimple(R.StandardOutput, obj, 0);
        WriteCh(R.StandardOutput, '\n');
        return(-1);
    }
}
