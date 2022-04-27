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

// ---- Version Properties ----

static const char * VersionProperties[] =
{
    "(command \"foment\")",
    "(website \"https://github.com/leftmike/foment\")",
    "(languages r7rs scheme)",
#ifdef FOMENT_WINDOWS
    "(encodings latin-1 ascii utf-8 utf-16)",
#else // FOMENT_WINDOWS
    "(encodings utf-8 ascii latin-1 utf-16)",
#endif // FOMENT_WINDOWS
    "(version \"" FOMENT_VERSION "\")",
#if defined(__DATE__) && defined(__TIME__)
    "(build.date \"" __DATE__ " " __TIME__ "\")",
#endif
#ifdef BUILD_PLATFORM
    "(build.platform \"" BUILD_PLATFORM "\")",
#endif // BUILD_PLATFORM
#ifdef BUILD_BRANCH
    "(build.branch \"" BUILD_BRANCH "\")",
#endif // BUILD_BRANCH
#ifdef BUILD_COMMIT
    "(build.commit \"" BUILD_COMMIT "\")",
#endif // BUILD_COMMIT
    "(scheme.id foment)",
    "(scheme.srfi 1 14 60 106 111 112 124 125 128 133 176 181 192)",
#ifdef C_VERSION
    "(c.version \"" C_VERSION "\")",
#endif // C_VERSION
};

// Size must match BuildProperties in src/genprops.cpp
extern const char * BuildProperties[4];

typedef struct
{
    const char * Type;
    ulong_t Size;
} FTypeSize;

static FTypeSize TypeSizes[] =
{
    {"int", sizeof(int) * 8},
    {"unsigned int", sizeof(unsigned int) * 8},
    {"long_t", sizeof(long_t) * 8},
    {"ulong_t", sizeof(ulong_t) * 8},
    {"int32_t", sizeof(int32_t) * 8},
    {"uint32_t", sizeof(uint32_t) * 8},
    {"int64_t", sizeof(int64_t) * 8},
    {"uint64_t", sizeof(uint64_t) * 8},
    {"void-pointer", sizeof(void *) * 8},
    {"FCh", sizeof(FCh) * 8},
    {"FObject", sizeof(FObject) * 8},
};

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

typedef enum
{
    ProgramMode = 0,
    BatchMode,
    InteractiveMode
} FRunMode;

static FRunMode RunMode = ProgramMode;

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

static int RunProgram(FChS * arg)
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

    ExecuteProc(CompileProgram(nam, port));
    FlushStandardPorts();
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

    FAssert(CollectorType == MarkSweepCollector);

    return(StringCToSymbol("mark-sweep"));
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

static int ShowVersion = 0;
static int VersionAction(FConfigWhen when)
{
#ifdef FOMENT_DEBUG
    printf("Foment Scheme " FOMENT_VERSION " (debug)\n\n");
#else // FOMENT_DEBUG
    printf("Foment Scheme " FOMENT_VERSION "\n\n");
#endif // FOMENT_DEBUG

    for (ulong_t idx = 0; idx < sizeof(VersionProperties) / sizeof(const char *); idx += 1)
        printf("%s\n", VersionProperties[idx]);

    for (ulong_t idx = 0; idx < sizeof(BuildProperties) / sizeof(const char *); idx += 1)
        printf("%s\n", BuildProperties[idx]);

    printf("(c.type-bits");
    for (ulong_t idx = 0; idx < sizeof(TypeSizes) / sizeof(FTypeSize); idx += 1)
        printf(" (%s " ULONG_FMT ")", TypeSizes[idx].Type, TypeSizes[idx].Size);
    printf(")\n");

    ShowVersion = 1;
    return(1);
}

static int SetBatch(FConfigWhen when)
{
    RunMode = BatchMode;
    return(1);
}

static int SetInteractive(FConfigWhen when)
{
    RunMode = InteractiveMode;
    return(1);
}

static int LoadAction(FConfigWhen when, FChS * s)
{
    RunMode = InteractiveMode;
    InteractiveOptions = MakePair(MakePair(StringCToSymbol("load"), MakeStringS(s)),
            InteractiveOptions);
    return(1);
}

static int PrintAction(FConfigWhen when, FChS * s)
{
    RunMode = InteractiveMode;
    InteractiveOptions = MakePair(MakePair(StringCToSymbol("print"), MakeStringS(s)),
            InteractiveOptions);
    return(1);
}

static int EvalAction(FConfigWhen when, FChS * s)
{
    RunMode = InteractiveMode;
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
    {'v', 'V', "version", 0, 0,
"        Prints out version information about foment.",
        EarlyConfig, ActionConfig, 0, 0, 0, 0, VersionAction, 0},

    {0, 0, "collector", 0, 0, 0, NeverConfig, GetConfig, 0, 0, GetCollector, 0, 0, 0},
    {0, 0, "no-collector", 0, 0,
"        No garbage collector.",
        EarlyConfig, ActionConfig, 0, 0, 0, 0, SetNoCollector, 0},
    {0, 0, "mark-sweep", 0, 0,
"        Use the mark and sweep garbage collector.",
        EarlyConfig, ActionConfig, 0, 0, 0, 0, SetMarkSweep, 0},
    {0, 0, "check-heap", 0, 0,
"        Check the heap before and after garbage collection.",
        AnytimeConfig, BoolConfig, 0, &CheckHeapFlag, 0, 0, 0, 0},

    {0, 0, "maximum-stack-size", 0, "number-of-bytes",
"        Use the specified number-of-bytes as the maximum stack size for each\n"
"        thread.",
        EarlyConfig, ULongConfig, &MaximumStackSize, 0, 0, 0, 0, 0},
    {0, 0, "maximum-heap-size", 0, "number-of-bytes",
"        Use the specified number-of-bytes as the maximum size of the heap.",
        EarlyConfig, ULongConfig, &MaximumHeapSize, 0, 0, 0, 0, 0},
    {0, 0, "trigger-bytes", 0, "number-of-bytes",
"        Trigger garbage collection after at least the specified\n"
"        number-of-bytes have been allocated since the last collection",
        AnytimeConfig, ULongConfig, &TriggerBytes, 0, 0, 0, 0, 0},
    {0, 0, "trigger-objects", 0, "number-of-objects",
"        Trigger garbage collection after at least the specified\n"
"        number-of-objects have been allocated since the last collection",
        AnytimeConfig, ULongConfig, &TriggerObjects, 0, 0, 0, 0, 0},
    {'b', 0, "batch", 0, 0,
"        Run foment in batch mode: standard input is treated as a program.",
        LateConfig, ActionConfig, 0, 0, 0, 0, SetBatch, 0},
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
                    MakeIntegerFromUInt64(*ConfigOptions[idx].UIntValue)), ret);
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
    // Should only be called once at interactive startup.
    FMustBe(argc == 0);

    return(ReverseListModify(InteractiveOptions));
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

Define("version-alist", VersionAlistPrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("version-alist", argc);

    FObject lst = EmptyListObject;

    for (ulong_t idx = 0; idx < sizeof(VersionProperties) / sizeof(const char *); idx += 1)
        lst = MakePair(Read(MakeStringCInputPort(VersionProperties[idx])), lst);

    lst = MakePair(MakePair(StringCToSymbol("scheme.features"), Features), lst);
    lst = MakePair(MakePair(StringCToSymbol("scheme.path"), LibraryPath), lst);
    lst = MakePair(MakePair(StringCToSymbol("scheme.extensions"), LibraryExtensions), lst);

    FObject types = EmptyListObject;
    for (ulong_t idx = 0; idx < sizeof(TypeSizes) / sizeof(FTypeSize); idx += 1)
        types = MakePair(MakePair(StringCToSymbol(TypeSizes[idx].Type),
                MakePair(MakeFixnum(TypeSizes[idx].Size), EmptyListObject)), types);

    lst = MakePair(MakePair(StringCToSymbol("c.type-bits"), ReverseListModify(types)), lst);
    return(ReverseListModify(lst));
}

static FObject Primitives[] =
{
    ConfigPrimitive,
    SetConfigPrimitive,
    InteractiveOptionsPrimitive,
    VersionAlistPrimitive
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
                        RunMode = InteractiveMode;

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
        if ((BinaryPortP(StandardOutput) || TextualPortP(StandardOutput))
                && OutputPortOpenP(StandardOutput))
            WriteSimple(StandardOutput, obj, 0);
        FlushStandardPorts();
        return(1);
    }

    FAssert(argc >= 1);

    try
    {
        if (ProcessOptions(LateConfig, argc, argv, &pdx) == 0)
        {
            FlushStandardPorts();
            return(1);
        }

        FullCommandLine = MakeCommandLine(argc, argv);
        if (pdx > 0)
        {
            FAssert(pdx < argc);

            CommandLine = MakeCommandLine(argc - pdx, argv + pdx);
        }
        else if (RunMode == ProgramMode)
            RunMode = InteractiveMode;

        if (RunMode == ProgramMode)
            AddToLibraryPath(argv[pdx]);
        else if (RunMode == InteractiveMode)
            LibraryPath = ReverseListModify(MakePair(MakeStringC("."), LibraryPath));
        LibraryPathOptions();

        if (ShowVersion != 0)
        {
            WriteSimple(StandardOutput, MakePair(StringCToSymbol("scheme.features"), Features), 0);
            WriteStringC(StandardOutput, "\n");
            WriteSimple(StandardOutput, MakePair(StringCToSymbol("scheme.path"), LibraryPath), 0);
            WriteStringC(StandardOutput, "\n");
            WriteSimple(StandardOutput,
                    MakePair(StringCToSymbol("scheme.extensions"), LibraryExtensions), 0);
            WriteStringC(StandardOutput, "\n");
        }
        else
        {
            switch (RunMode)
            {
            case ProgramMode:
                return(RunProgram(argv[pdx]));

            case BatchMode:
                if (TextualPortP(StandardInput) == 0)
                {
                    printf("error: standard input is not a textual port\n");
                    return(1);
                }
                ExecuteProc(CompileProgram(AsGenericPort(StandardInput)->Name, StandardInput));
                break;

            case InteractiveMode:
                ExecuteProc(InteractiveThunk);
                break;

            default:
                FAssert(0);
            }
        }

        FlushStandardPorts();
        return(0);
    }
    catch (FObject obj)
    {
        if (ExceptionP(obj) == 0)
            WriteStringC(StandardOutput, "exception: ");
        WriteSimple(StandardOutput, obj, 0);
        WriteCh(StandardOutput, '\n');
        FlushStandardPorts();
        return(1);
    }
}
