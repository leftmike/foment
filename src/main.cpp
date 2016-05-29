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
#define STRING_FORMAT "%S"
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
#define StringLengthS(s) strlen(s)
#define StringCompareS(s1, s2) strcmp(s1, s2)
#define StringToInt(s) atoi(s)
#define HexStringToInt32(s) strtol(s, 0, 16)
#define HexStringToInt64(s) strtoll(s, 0, 16);
#define STRING_FORMAT "%s"
#endif // FOMENT_UNIX

static int InteractiveFlag = 0;

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

static int ProgramMode(FChS * arg)
{
    FObject nam = MakeStringS(arg);
    FObject port = OpenInputFile(nam);
    if (TextualPortP(port) == 0)
    {
        printf("error: unable to open program: " STRING_FORMAT "\n", arg);
        return(1);
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
typedef int (*FSetConfigFn)(FObject obj);
typedef int (*FActionConfigFn)(FConfigWhen when);
typedef int (*FArgConfigFn)(FConfigWhen when, FChS * s);

typedef struct
{
    char ShortName;
    char AltShortName;
    const char * LongName;
    const char * AltLongName;
    const char * Argument;
    const char * Description;
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

static int SetNoCollector(FConfigWhen when)
{
    CollectorType = NoCollector;
    return(1);
}

static int SetMarkSweep(FConfigWhen when)
{
    CollectorType = MarkSweepCollector;
    return(1);
}

static int SetGenerational(FConfigWhen when)
{
    CollectorType = GenerationalCollector;
    return(1);
}

static FObject GetLibraryPath()
{
    return(R.LibraryPath);
}

static int AppendLibraryPath(FObject obj)
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

    return(1);
}

static int PrependLibraryPath(FObject obj)
{
    R.LibraryPath = MakePair(obj, R.LibraryPath);

    return(1);
}

static FObject GetLibraryExtensions()
{
    return(R.LibraryExtensions);
}

static int AddExtension(FObject obj)
{
    R.LibraryExtensions = MakePair(obj, R.LibraryExtensions);
    return(1);
}

static void Usage();
static int UsageAction(FConfigWhen when)
{
    Usage();
    exit(0);
}

static int VersionAction(FConfigWhen when)
{
    printf("foment-" FOMENT_VERSION "\n");
    exit(0);
}

static int SetInteractive(FConfigWhen when)
{
    InteractiveFlag = 1;
    return(1);
}

static int LoadAction(FConfigWhen when, FChS * s)
{
    InteractiveFlag = 1;
    R.InteractiveOptions = MakePair(MakePair(StringCToSymbol("load"), MakeStringS(s)),
            R.InteractiveOptions);
    return(1);
}

static int PrintAction(FConfigWhen when, FChS * s)
{
    InteractiveFlag = 1;
    R.InteractiveOptions = MakePair(MakePair(StringCToSymbol("print"), MakeStringS(s)),
            R.InteractiveOptions);
    return(1);
}

static int EvalAction(FConfigWhen when, FChS * s)
{
    InteractiveFlag = 1;
    R.InteractiveOptions = MakePair(MakePair(StringCToSymbol("eval"), MakeStringS(s)),
            R.InteractiveOptions);
    return(1);
}

static FConfigOption ConfigOptions[] =
{
    {0, 0, "library-path", 0, 0, 0, NeverConfig, GetConfig, {.GetConfigFn = GetLibraryPath}},
    {'A', 0, "append", 0, "directory",
"        Append the specified directory to the list of directories to search\n"
"        when loading libraries.",
        LateConfig, SetConfig, {.SetConfigFn = AppendLibraryPath}},
    {'I', 0, "prepend", 0, "directory",
"        Prepend the specified directory to the list of directories to search\n"
"        when loading libraries.",
        LateConfig, SetConfig, {.SetConfigFn = PrependLibraryPath}},

    {0, 0, "library-extensions", 0, 0, 0,
        NeverConfig, GetConfig, {.GetConfigFn = GetLibraryExtensions}},
    {'X', 0, "extension", 0, "extension",
"        Add the specified extension to the list of filename extensions to try\n"
"        when loading libraries.",
        LateConfig, SetConfig, {.SetConfigFn = AddExtension}},

    {0, 0, "verbose", 0, 0,
"        Turn on verbose logging.",
        AnytimeConfig, BoolConfig, {.BoolValue = &VerboseFlag}},
    {0, 0, "random-seed", 0, "seed",
"        Use the specified seed for the random number generator; otherwise the\n"
"        current time is used.",
        EarlyConfig, UIntConfig, {.UIntValue = &RandomSeed}},
    {'h', '?', "help", "usage", 0,
"        Prints out the usage information for foment.",
        EarlyConfig, ActionConfig, {.ActionConfigFn = UsageAction}},
    {0, 0, "version", 0, 0,
"        Prints out the current version number of foment.",
        EarlyConfig, ActionConfig, {.ActionConfigFn = VersionAction}},

    {0, 0, "collector", 0, 0, 0, NeverConfig, GetConfig, {.GetConfigFn = GetCollector}},
    {0, 0, "no-collector", 0, 0,
"        No garbage collector.",
        EarlyConfig, ActionConfig, {.ActionConfigFn = SetNoCollector}},
    {0, 0, "mark-sweep", 0, 0,
"        Use the mark and sweep garbage collector.",
        EarlyConfig, ActionConfig, {.ActionConfigFn = SetMarkSweep}},
    {0, 0, "generational", 0, 0,
"        Use the generational + mark and sweep garbage collector.",
        EarlyConfig, ActionConfig, {.ActionConfigFn = SetGenerational}},
    {0, 0, "check-heap", 0, 0,
"        Check the heap before and after garbage collection.",
        AnytimeConfig, BoolConfig, {.BoolValue = &CheckHeapFlag}},

    {0, 0, "maximum-stack-size", 0, "number-of-bytes",
"        Use the specified number-of-bytes as the maximum stack size for each\n"
"        thread.",
        EarlyConfig, UIntConfig, {.UIntValue = &MaximumStackSize}},
    {0, 0, "maximum-babies-size", 0, "number-of-bytes",
"        Use the specified number-of-bytes as the maximum size of generation\n"
"        zero for each thread.",
        EarlyConfig, UIntConfig, {.UIntValue = &MaximumBabiesSize}},
    {0, 0, "maximum-kids-size", 0, "number-of-bytes",
"        Use the specified number-of-bytes as the maximum size of generation\n"
"        one; this space is shared by all threads.",
        EarlyConfig, UIntConfig, {.UIntValue = &MaximumKidsSize}},
    {0, 0, "maximum-adults-size", 0, "number-of-bytes",
"        Use the specified number-of-bytes as the maximum size of the mark and\n"
"        sweep generation.",
        EarlyConfig, UIntConfig, {.UIntValue = &MaximumAdultsSize}},
    {0, 0, "maximum-generational-baby", 0, "number-of-bytes",
"        When using the generational collector, new objects larger than the\n"
"        specified number-of-bytes are allocated in the mark and sweep\n"
"        generation rather than generation zero.",
        AnytimeConfig, UIntConfig, {.UIntValue = &MaximumGenerationalBaby}},
    {0, 0, "trigger-bytes", 0, "number-of-bytes",
"        Trigger garbage collection after at least the specified\n"
"        number-of-bytes have been allocated since the last collection",
        AnytimeConfig, UIntConfig, {.UIntValue = &TriggerBytes}},
    {0, 0, "trigger-objects", 0, "number-of-objects",
"        Trigger garbage collection after at least the specified\n"
"        number-of-objects have been allocated since the last collection",
        AnytimeConfig, UIntConfig, {.UIntValue = &TriggerObjects}},
    {0, 0, "partial-per-full", 0, "number",
"        Perform the specified number of partial garbage collections\n"
"        before performing a full collection.",
        AnytimeConfig, UIntConfig, {.UIntValue = &PartialPerFull}},

    {'i', 0, "interactive", "repl", 0,
"        Run foment in an interactive session (repl).",
        LateConfig, ActionConfig, {.ActionConfigFn = SetInteractive}},
    {'l', 0, "load", 0, "filename",
"        Load the specified filename using the scheme procedure, load.",
        LateConfig, ArgConfig, {.ArgConfigFn = LoadAction}},
    {'p', 0, "print", 0, "expression",
"        Evaluate and print the results of the specified expression.",
        LateConfig, ArgConfig, {.ArgConfigFn = PrintAction}},
    {'e', 0, "eval", "evaluate", "expression",
"        Evaluate the specified expression.",
        LateConfig, ArgConfig, {.ArgConfigFn = EvalAction}}
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
            FAssert(ConfigOptions[idx].LongName != 0);

            ret = MakePair(MakePair(StringCToSymbol(ConfigOptions[idx].LongName),
                    MakeIntegerU(*ConfigOptions[idx].UIntValue)), ret);
            break;

        case BoolConfig:
            FAssert(ConfigOptions[idx].LongName != 0);

            ret = MakePair(MakePair(StringCToSymbol(ConfigOptions[idx].LongName),
                    *ConfigOptions[idx].BoolValue ? TrueObject : FalseObject), ret);
            break;

        case GetConfig:
            FAssert(ConfigOptions[idx].LongName != 0);

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
        if (ConfigOptions[idx].LongName != 0
                && StringCToSymbol(ConfigOptions[idx].LongName) == argv[0])
        {
            cfg = ConfigOptions + idx;
            if (cfg->Type != GetConfig)
                break;
        }
    }

    if (cfg == 0)
        RaiseExceptionC(Assertion, "set-config!", "expected a config option", List(argv[0]));

    if (cfg->When != AnytimeConfig)
        RaiseExceptionC(Assertion, "set-config!", "option may not be configured now",
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
        RaiseExceptionC(Assertion, "set-config!", "option may not be configured now",
                List(argv[0]));
        break;
    }

    return(NoValueObject);
}

Define("%interactive-options", InteractiveOptionsPrimitive)(int_t argc, FObject argv[])
{
    FMustBe(argc == 0);

    return(R.InteractiveOptions);
}

static void Usage()
{
    printf("Usage:\n");
    printf("    foment <option> ... [--] <program> <program-arg> ...\n\n");

    for (int cdx = 0; cdx < sizeof(ConfigOptions) / sizeof(FConfigOption); cdx++)
    {
        FConfigOption * cfg = ConfigOptions + cdx;

        if (cfg->When == NeverConfig || cfg->Type == GetConfig)
            continue;

        if (cfg->ShortName != 0)
        {
            if (cfg->Argument != 0)
                printf("    -%c <%s>\n", cfg->ShortName, cfg->Argument);
            else
                printf("    -%c\n", cfg->ShortName);
        }
        if (cfg->AltShortName != 0)
        {
            if (cfg->Argument != 0)
                printf("    -%c <%s>\n", cfg->AltShortName, cfg->Argument);
            else
                printf("    -%c\n", cfg->AltShortName);
        }

        if (cfg->LongName != 0)
        {
            if (cfg->Argument != 0)
                printf("    --%s <%s>\n", cfg->LongName, cfg->Argument);
            else
                printf("    --%s\n", cfg->LongName);
        }
        if (cfg->AltLongName != 0)
        {
            if (cfg->Argument != 0)
                printf("    --%s <%s>\n", cfg->AltLongName, cfg->Argument);
            else
                printf("    --%s\n", cfg->AltLongName);
        }

        if (cfg->Description != 0)
            printf("%s\n", cfg->Description);

        printf("\n");
    }

    printf("Notes:\n");
    printf("    Program mode is assumed unless there is no <program> or at\n");
    printf("    least one of -i (--interactive, --repl), -p (--print), or -e\n");
    printf("    (--eval, --evaluate) are specified.\n\n");
    printf("    Use -- to indicate the end of options; this is unnecessary in\n");
    printf("    program mode unless <program> starts with - or --.\n\n");
}

static FConfigOption * FindShortName(FChS sn)
{
    for (int cdx = 0; cdx < sizeof(ConfigOptions) / sizeof(FConfigOption); cdx++)
        if (ConfigOptions[cdx].When != NeverConfig && ConfigOptions[cdx].Type != GetConfig)
        {
            if (ConfigOptions[cdx].ShortName == sn || ConfigOptions[cdx].AltShortName == sn)
                return(ConfigOptions + cdx);
        }
    return(0);
}

static FConfigOption * FindLongName(FChS * ln)
{
    for (int cdx = 0; cdx < sizeof(ConfigOptions) / sizeof(FConfigOption); cdx++)
        if (ConfigOptions[cdx].When != NeverConfig && ConfigOptions[cdx].Type != GetConfig)
        {
            if (ConfigOptions[cdx].LongName != 0
                    && StringCompareS(ln, ConfigOptions[cdx].LongName) == 0)
                return(ConfigOptions + cdx);
            else if (ConfigOptions[cdx].AltLongName != 0
                    && StringCompareS(ln, ConfigOptions[cdx].AltLongName) == 0)
                return(ConfigOptions + cdx);
        }
    return(0);
}

static FPrimitive * Primitives[] =
{
    &ConfigPrimitive,
    &SetConfigPrimitive,
    &InteractiveOptionsPrimitive
};

void SetupMain()
{
    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}

int ProcessOptions(FConfigWhen when, int argc, FChS * argv[], int * pdx)
{
    int adx = 1;
    while (adx < argc)
    {
        FConfigOption * cfg = 0;

        if (argv[adx][0] == '-')
        {
            if (argv[adx][1] == '-')
            {
                if (argv[adx][2] == 0)
                {
                    if (adx + 1 == argc)
                        InteractiveFlag = 1;

                    *pdx = adx + 1;
                    break;
                }

                cfg = FindLongName(argv[adx] + 2);
            }
            else if (argv[adx][1] != 0 && argv[adx][2] == 0)
                cfg = FindShortName(argv[adx][1]);
        }
        else
        {
            *pdx = adx;
            break;
        }

        if (cfg == 0)
        {
            Usage();
            printf("error: unknown option: " STRING_FORMAT "\n", argv[adx]);
            return(0);
        }

        adx += 1;
        if (cfg->Type != ActionConfig && cfg->Type != BoolConfig)
        {
            if (adx == argc)
            {
                printf("error: expected an argument following " STRING_FORMAT "\n", argv[adx - 1]);
                return(0);
            }
            adx += 1;
        }

        if (cfg->When == when || cfg->When == AnytimeConfig)
        {
            switch (cfg->Type)
            {
            case UIntConfig:
                *cfg->UIntValue = StringToInt(argv[adx - 1]);
                break;

            case BoolConfig:
                *cfg->BoolValue = 1;
                break;

            case GetConfig:
                FAssert(cfg->When == NeverConfig);
                break;

            case SetConfig:
                FAssert(cfg->When == AnytimeConfig);

                // Handle SetConfig with AnytimeConfig as LateConfig.
                if (when == LateConfig && cfg->SetConfigFn(MakeStringS(argv[adx])) == 0)
                    return(0);
                break;

            case ActionConfig:
                if (cfg->ActionConfigFn(when) == 0)
                    return(0);
                break;

            case ArgConfig:
                if (cfg->ArgConfigFn(when, argv[adx - 1]) == 0)
                    return(0);
                break;

            default:
                FAssert(0);
                break;
            }
        }
    }

    return(1);
}

#ifdef FOMENT_WINDOWS
int wmain(int argc, FChS * argv[])
#endif // FOMENT_WINDOWS
#ifdef FOMENT_UNIX
int main(int argc, FChS * argv[])
#endif // FOMENT_UNIX
{
    int pdx = 0;

    if (ProcessOptions(EarlyConfig, argc, argv, &pdx) == 0)
        return(1);

    FThreadState ts;

    try
    {
        if (SetupFoment(&ts) == 0)
        {
            printf("error: out of memory setting up foment\n");
            return(1);
        }
    }
    catch (FObject obj)
    {
        printf("error: unexpected exception setting up foment: %p\n", obj);
        WriteSimple(R.StandardOutput, obj, 0);
        return(1);
    }

    FAssert(argc >= 1);

    try
    {
        R.InteractiveOptions = EmptyListObject;
        if (ProcessOptions(LateConfig, argc, argv, &pdx) == 0)
            return(1);

        R.FullCommandLine = MakeCommandLine(argc, argv);
        if (pdx == 0)
            R.CommandLine = EmptyListObject;
        else
        {
            FAssert(pdx < argc);

            R.CommandLine = MakeCommandLine(argc - pdx, argv + pdx);
        }

        if (InteractiveFlag == 0 && pdx > 0)
        {
            AddToLibraryPath(argv[pdx]);
            return(ProgramMode(argv[pdx]));
        }

        R.LibraryPath = ReverseListModify(MakePair(MakeStringC("."), R.LibraryPath));

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
        return(1);
    }
}
