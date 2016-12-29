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

// ---- Roots ----

FObject CommandLine = EmptyListObject;
FObject FullCommandLine = NoValueObject;
FObject EnvironmentVariables = NoValueObject;

static FObject InteractiveOptions = EmptyListObject;

// ----------------

#ifdef FOMENT_WINDOWS
#include <wchar.h>
#define StringLengthS(s) wcslen(s)
#define StringToInt(s) _wtoi(s)
#define HexStringToInt32(s) wcstol(s, 0, 16)
#define HexStringToInt64(s) _wcstoui64(s, 0, 16);
#define STRING_FORMAT "%S"

int StringCompareS(FChS * s1, const char * s2)
{
    while (*s1 != 0 && *s1 == *s2)
    {
        s1 += 1;
        s2 += 1;
    }

    return(*s1 - *s2);
}
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

static FChS * Appends[16];
static int NumAppends = 0;
static FChS * Includes[16];
static int NumIncludes = 0;

static FObject MakeCommandLine(long_t argc, FChS * argv[])
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
            LibraryPath = MakePair(MakeStringS(prog, pth - prog), LibraryPath);
        else
            LibraryPath = MakePair(MakeStringC("."), LibraryPath);
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
        LibraryPath = MakePair(MakeStringS(prog, pth - prog), LibraryPath);
    else
        LibraryPath = MakePair(MakeStringC("."), LibraryPath);
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

    ExecuteProc(proc);
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
    ULongConfig,
    BoolConfig,
    GetConfig,
    SetConfig,
    ActionConfig,
    ArgConfig
} FConfigType;

typedef FObject (*FGetConfigFn)();
typedef int (*FSetConfigFn)(FChS * s);
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

    ulong_t * UIntValue;
    ulong_t * BoolValue;
    FGetConfigFn GetConfigFn;
    FSetConfigFn SetConfigFn;
    FActionConfigFn ActionConfigFn;
    FArgConfigFn ArgConfigFn;
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
    return(LibraryPath);
}

static int AppendLibraryPath(FChS * s)
{
    if (NumAppends == sizeof(Appends) / sizeof(FChS *))
    {
        printf("error: too many -A and --append\n");
        return(0);
    }

    Appends[NumAppends] = s;
    NumAppends += 1;
    return(1);
}

static int PrependLibraryPath(FChS * s)
{
    if (NumIncludes == sizeof(Includes) / sizeof(FChS *))
    {
        printf("error: too many -I and --prepend\n");
        return(0);
    }

    Includes[NumIncludes] = s;
    NumIncludes += 1;
    return(1);

}

static FObject GetLibraryExtensions()
{
    return(LibraryExtensions);
}

static int AddExtension(FChS * s)
{
    LibraryExtensions = MakePair(MakeStringS(s), LibraryExtensions);
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
    InteractiveOptions = MakePair(MakePair(StringCToSymbol("load"), MakeStringS(s)),
            InteractiveOptions);
    return(1);
}

static int PrintAction(FConfigWhen when, FChS * s)
{
    InteractiveFlag = 1;
    InteractiveOptions = MakePair(MakePair(StringCToSymbol("print"), MakeStringS(s)),
            InteractiveOptions);
    return(1);
}

static int EvalAction(FConfigWhen when, FChS * s)
{
    InteractiveFlag = 1;
    InteractiveOptions = MakePair(MakePair(StringCToSymbol("eval"), MakeStringS(s)),
            InteractiveOptions);
    return(1);
}

static FConfigOption ConfigOptions[] =
{
    {0, 0, "library-path", 0, 0, 0, NeverConfig, GetConfig, 0, 0, GetLibraryPath, 0, 0, 0},
    {'A', 0, "append", 0, "directory",
"        Append the specified directory to the list of directories to search\n"
"        when loading libraries.",
        LateConfig, SetConfig, 0, 0, 0, AppendLibraryPath, 0, 0},
    {'I', 0, "prepend", 0, "directory",
"        Prepend the specified directory to the list of directories to search\n"
"        when loading libraries.",
        LateConfig, SetConfig, 0, 0, 0, PrependLibraryPath, 0, 0},

    {0, 0, "library-extensions", 0, 0, 0,
        NeverConfig, GetConfig, 0, 0, GetLibraryExtensions, 0, 0, 0},
    {'X', 0, "extension", 0, "extension",
"        Add the specified extension to the list of filename extensions to try\n"
"        when loading libraries.",
        LateConfig, SetConfig, 0, 0, 0, AddExtension, 0, 0},

    {0, 0, "verbose", 0, 0,
"        Turn on verbose logging.",
        AnytimeConfig, BoolConfig, 0, &VerboseFlag, 0, 0, 0, 0},
    {0, 0, "random-seed", 0, "seed",
"        Use the specified seed for the random number generator; otherwise the\n"
"        current time is used.",
        EarlyConfig, ULongConfig, &RandomSeed, 0, 0, 0, 0, 0},
    {'h', '?', "help", "usage", 0,
"        Prints out the usage information for foment.",
        EarlyConfig, ActionConfig, 0, 0, 0, 0, UsageAction, 0},
    {0, 0, "version", 0, 0,
"        Prints out the current version number of foment.",
        EarlyConfig, ActionConfig, 0, 0, 0, 0, VersionAction, 0},

    {0, 0, "collector", 0, 0, 0, NeverConfig, GetConfig, 0, 0, GetCollector, 0, 0, 0},
    {0, 0, "no-collector", 0, 0,
"        No garbage collector.",
        EarlyConfig, ActionConfig, 0, 0, 0, 0, SetNoCollector, 0},
    {0, 0, "mark-sweep", 0, 0,
"        Use the mark and sweep garbage collector.",
        EarlyConfig, ActionConfig, 0, 0, 0, 0, SetMarkSweep, 0},
    {0, 0, "generational", 0, 0,
"        Use the generational + mark and sweep garbage collector.",
        EarlyConfig, ActionConfig, 0, 0, 0, 0, SetGenerational, 0},
    {0, 0, "check-heap", 0, 0,
"        Check the heap before and after garbage collection.",
        AnytimeConfig, BoolConfig, 0, &CheckHeapFlag, 0, 0, 0, 0},

    {0, 0, "maximum-stack-size", 0, "number-of-bytes",
"        Use the specified number-of-bytes as the maximum stack size for each\n"
"        thread.",
        EarlyConfig, ULongConfig, &MaximumStackSize, 0, 0, 0, 0, 0},
    {0, 0, "maximum-babies-size", 0, "number-of-bytes",
"        Use the specified number-of-bytes as the maximum size of generation\n"
"        zero for each thread.",
        EarlyConfig, ULongConfig, &MaximumBabiesSize, 0, 0, 0, 0, 0},
    {0, 0, "maximum-kids-size", 0, "number-of-bytes",
"        Use the specified number-of-bytes as the maximum size of generation\n"
"        one; this space is shared by all threads.",
        EarlyConfig, ULongConfig, &MaximumKidsSize, 0, 0, 0, 0, 0},
    {0, 0, "maximum-adults-size", 0, "number-of-bytes",
"        Use the specified number-of-bytes as the maximum size of the mark and\n"
"        sweep generation.",
        EarlyConfig, ULongConfig, &MaximumAdultsSize, 0, 0, 0, 0, 0},
    {0, 0, "maximum-generational-baby", 0, "number-of-bytes",
"        When using the generational collector, new objects larger than the\n"
"        specified number-of-bytes are allocated in the mark and sweep\n"
"        generation rather than generation zero.",
        AnytimeConfig, ULongConfig, &MaximumGenerationalBaby, 0, 0, 0, 0, 0},
    {0, 0, "trigger-bytes", 0, "number-of-bytes",
"        Trigger garbage collection after at least the specified\n"
"        number-of-bytes have been allocated since the last collection",
        AnytimeConfig, ULongConfig, &TriggerBytes, 0, 0, 0, 0, 0},
    {0, 0, "trigger-objects", 0, "number-of-objects",
"        Trigger garbage collection after at least the specified\n"
"        number-of-objects have been allocated since the last collection",
        AnytimeConfig, ULongConfig, &TriggerObjects, 0, 0, 0, 0, 0},
    {0, 0, "partial-per-full", 0, "number",
"        Perform the specified number of partial garbage collections\n"
"        before performing a full collection.",
        AnytimeConfig, ULongConfig, &PartialPerFull, 0, 0, 0, 0, 0},

    {'i', 0, "interactive", "repl", 0,
"        Run foment in an interactive session (repl).",
        LateConfig, ActionConfig, 0, 0, 0, 0, SetInteractive, 0},
    {'l', 0, "load", 0, "filename",
"        Load the specified filename using the scheme procedure, load.",
        LateConfig, ArgConfig, 0, 0, 0, 0, 0, LoadAction},
    {'p', 0, "print", 0, "expression",
"        Evaluate and print the results of the specified expression.",
        LateConfig, ArgConfig, 0, 0, 0, 0, 0, PrintAction},
    {'e', 0, "eval", "evaluate", "expression",
"        Evaluate the specified expression.",
        LateConfig, ArgConfig, 0, 0, 0, 0, 0, EvalAction}
};

Define("config", ConfigPrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("config", argc);

    FObject ret = EmptyListObject;

    for (ulong_t idx = 0; idx < sizeof(ConfigOptions) / sizeof(FConfigOption); idx++)
    {
        switch (ConfigOptions[idx].Type)
        {
        case ULongConfig:
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

Define("set-config!", SetConfigPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("set-config!", argc);
    SymbolArgCheck("set-config!", argv[0]);

    FConfigOption * cfg = 0;
    for (ulong_t idx = 0; idx < sizeof(ConfigOptions) / sizeof(FConfigOption); idx++)
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
    case ULongConfig:
        FixnumArgCheck("set-config!", argv[1]);
        *cfg->UIntValue = AsFixnum(argv[1]);
        break;

    case BoolConfig:
        BooleanArgCheck("set-config!", argv[1]);
        *cfg->BoolValue = argv[1] == TrueObject ? 1 : 0;
        break;

    default:
        RaiseExceptionC(Assertion, "set-config!", "option may not be configured now",
                List(argv[0]));
        break;
    }

    return(NoValueObject);
}

Define("%interactive-options", InteractiveOptionsPrimitive)(long_t argc, FObject argv[])
{
    FMustBe(argc == 0);

    return(InteractiveOptions);
}

static void Usage()
{
    printf("Usage:\n");
    printf("    foment <option> ... [--] <program> <program-arg> ...\n\n");

    for (ulong_t cdx = 0; cdx < sizeof(ConfigOptions) / sizeof(FConfigOption); cdx++)
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
    for (ulong_t cdx = 0; cdx < sizeof(ConfigOptions) / sizeof(FConfigOption); cdx++)
        if (ConfigOptions[cdx].When != NeverConfig && ConfigOptions[cdx].Type != GetConfig)
        {
            if (ConfigOptions[cdx].ShortName == sn || ConfigOptions[cdx].AltShortName == sn)
                return(ConfigOptions + cdx);
        }
    return(0);
}

static FConfigOption * FindLongName(FChS * ln)
{
    for (ulong_t cdx = 0; cdx < sizeof(ConfigOptions) / sizeof(FConfigOption); cdx++)
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

static FObject Primitives[] =
{
    ConfigPrimitive,
    SetConfigPrimitive,
    InteractiveOptionsPrimitive
};

void SetupMain()
{
    for (ulong_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(Bedrock, BedrockLibrary, Primitives[idx]);
}

void LibraryPathOptions()
{
    int idx;

    if (NumAppends > 0)
    {
        FObject lp = LibraryPath;

        for (;;)
        {
            FAssert(PairP(lp));

            if (Rest(lp) == EmptyListObject)
                break;
            lp = Rest(lp);
        }

        for (idx = 0; idx < NumAppends; idx++)
        {
//            AsPair(lp)->Rest = MakePair(MakeStringS(Appends[idx]), EmptyListObject);
            SetRest(lp, MakePair(MakeStringS(Appends[idx]), EmptyListObject));
            lp = Rest(lp);
        }
    }

    for (idx = 0; idx < NumIncludes; idx++)
        LibraryPath = MakePair(MakeStringS(Includes[idx]), LibraryPath);
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
            case ULongConfig:
                *cfg->UIntValue = StringToInt(argv[adx - 1]);
                break;

            case BoolConfig:
                *cfg->BoolValue = 1;
                break;

            case GetConfig:
                FAssert(cfg->When == NeverConfig);
                break;

            case SetConfig:
                FAssert(cfg->When == LateConfig);

                if (cfg->SetConfigFn(argv[adx - 1]) == 0)
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

    RegisterRoot(&CommandLine, "command-line");
    RegisterRoot(&FullCommandLine, "full-command-line");
    RegisterRoot(&EnvironmentVariables, "environment-variables");
    RegisterRoot(&InteractiveOptions, "interactive-options");

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
        WriteSimple(StandardOutput, obj, 0);
        return(1);
    }

    FAssert(argc >= 1);

    try
    {
        if (ProcessOptions(LateConfig, argc, argv, &pdx) == 0)
            return(1);

        FullCommandLine = MakeCommandLine(argc, argv);
        if (pdx != 0)
        {
            FAssert(pdx < argc);

            CommandLine = MakeCommandLine(argc - pdx, argv + pdx);
        }

        if (InteractiveFlag == 0 && pdx > 0)
        {
            AddToLibraryPath(argv[pdx]);
            LibraryPathOptions();
            return(ProgramMode(argv[pdx]));
        }

        LibraryPath = ReverseListModify(MakePair(MakeStringC("."), LibraryPath));
        LibraryPathOptions();

        ExecuteProc(InteractiveThunk);
        ExitFoment();
        return(0);
    }
    catch (FObject obj)
    {
        if (ExceptionP(obj) == 0)
            WriteStringC(StandardOutput, "exception: ");
        WriteSimple(StandardOutput, obj, 0);
        WriteCh(StandardOutput, '\n');
        return(1);
    }
}
