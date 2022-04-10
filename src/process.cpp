/*

Foment

- %subprocess: textual vs binary port when a pipe is created
*/

#ifdef FOMENT_WINDOWS
#include <windows.h>
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif // FOMENT_UNIX

#include <stdio.h>
#include "foment.hpp"
#include "syncthrd.hpp"
#include "io.hpp"
#include "unicode.hpp"

#if defined(FOMENT_BSD) || defined(FOMENT_OSX)
extern char ** environ;
#else
#include <malloc.h>
#endif

// ----------------

EternalSymbol(StdoutSymbol, "stdout");

// ---- Subprocesses ----

typedef struct
{
#ifdef FOMENT_WINDOWS
    DWORD ProcessID;
    HANDLE ProcessHandle;
    DWORD Status;
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    pid_t ProcessID;
    int Status;
    int Done;
#endif // FOMENT_UNIX
} FSubprocess;

#define AsSubprocess(obj) ((FSubprocess *) (obj))
#define SubprocessP(obj) (ObjectTag(obj) == SubprocessTag)

#ifdef FOMENT_WINDOWS
static FObject MakeSubprocess(DWORD pid, HANDLE ph, const char * who)
{
    FSubprocess * sub = (FSubprocess *) MakeObject(SubprocessTag, sizeof(FSubprocess), 0, who);
    sub->ProcessID = pid;
    sub->ProcessHandle = ph;
    return(sub);
}
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
static FObject MakeSubprocess(pid_t pid, const char * who)
{
    FSubprocess * sub = (FSubprocess *) MakeObject(SubprocessTag, sizeof(FSubprocess), 0, who);
    sub->ProcessID = pid;
    sub->Status = 0;
    sub->Done = 0;
    return(sub);
}
#endif // FOMENT_UNIX

void WriteSubprocess(FWriteContext * wctx, FObject obj)
{
    FCh s[16];
    long_t sl = FixnumAsString((long_t) obj, s, 16);

    wctx->WriteStringC("#<subprocess: ");
    wctx->WriteString(s, sl);
    wctx->WriteStringC(" pid: ");

    FAssert(SubprocessP(obj));

    sl = FixnumAsString((long_t) AsSubprocess(obj)->ProcessID, s, 10);
    wctx->WriteString(s, sl);
    wctx->WriteCh('>');
}

inline void SubprocessArgCheck(const char * who, FObject obj)
{
    if (SubprocessP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a subprocess", List(obj));
}

static void MakePipe(FFileHandle * ifh, FFileHandle * ofh, const char * who)
{
#ifdef FOMENT_WINDOWS
    if (CreatePipe(ifh, ofh, 0, 0) == 0)
        RaiseExceptionC(Assertion, who, "create pipe failed", List(MakeFixnum(GetLastError())));
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    int pipefd[2];

    if (pipe(pipefd) != 0)
        RaiseExceptionC(Assertion, who, "pipe system call failed", List(MakeFixnum(errno)));
    *ifh = pipefd[0];
    *ofh = pipefd[1];
#endif // FOMENT_UNIX
}

static FChS * ConvertStringToSystem(FObject s)
{
    FAssert(StringP(s));

#ifdef FOMENT_WINDOWS
        FObject bv = ConvertStringToUtf16(s);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
        FObject bv = ConvertStringToUtf8(s);
#endif // FOMENT_UNIX

        FAssert(BytevectorP(bv));

        return((FChS *) AsBytevector(bv)->Vector);
}

typedef struct
{
    // Optional file handles to use for stdout, stdin, and stderr in the subprocess.
    FFileHandle ChildStdout; // Output file handle.
    FFileHandle ChildStdin; // Input file handle.
    FFileHandle ChildStderr; // Output file handle.
    int UseStdout; // Use stdout for stderr.

    // These are only used if the corresponding Child* above is not specified.
    FFileHandle PipeStdout; // Input file handle of a pipe connected to stdout in the subprocess.
    FFileHandle PipeStdin; // Output file handle of a pipe connected to stdin in the subprocess.
    FFileHandle PipeStderr; // Input file handle of a pipe connected to stderr in the subprocess.
} FSubprocessFileHandles;

#ifdef FOMENT_WINDOWS
static FChS * CopyArg(FChS * cmd, FChS * arg)
{
    *cmd = '"';
    cmd += 1;

    while (*arg != 0)
    {
        if (*arg == '"') // || *arg == '\\')
        {
            *cmd = '\\';
            cmd += 1;
        }
        *cmd = *arg;
        cmd += 1;
        arg += 1;
    }

    *cmd = '"';
    cmd += 1;
    return(cmd);
}

static void ChildArgs(long_t argc, FObject argv[], FChS ** app, FChS ** cmdline)
{
    long_t cmdlen = 1;
    long_t adx;

    for (adx = 0; adx < argc; adx++)
        cmdlen += StringLength(argv[adx]) * 2 + 3;

    FObject bv = MakeBytevector(cmdlen * sizeof(FChS));
    FAssert(BytevectorP(bv));
    FChS * cmd = (FChS *) AsBytevector(bv)->Vector;
    *cmdline = cmd;

    for (adx = 0; adx < argc; adx++)
    {
        FChS * arg = ConvertStringToSystem(argv[adx]);
        if (adx == 0)
        {
            *app = arg;
            FChS * s = wcsrchr(arg, '\\');
            if (s != 0)
                arg = s + 1;
            cmd = CopyArg(cmd, arg);
        }
        else
        {
            *cmd = ' ';
            cmd += 1;
            cmd = CopyArg(cmd, arg);
        }
    }
    *cmd = 0;
}
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
static void ChildFailed(char * cmd, const char * call)
{
    fprintf(stderr, "error: %s: %s: %d\n", cmd, call, errno);
    fflush(stderr);
    exit(1);
}

static void ChildProcess(FSubprocessFileHandles * sfh, FFileHandle chldin, FFileHandle chldout,
    FFileHandle chlderr, long_t argc, FObject argv[])
{
    FAssert(argc > 0);

    char ** args = (char **) malloc(sizeof(char *) * (argc + 1));
    char * cmd = ConvertStringToSystem(argv[0]);
    char * s = strrchr(cmd, '/');
    if (s == 0)
        args[0] = cmd;
    else
        args[0] = strdup(s + 1);

    for (long_t adx = 1; adx < argc; adx += 1)
        args[adx] = ConvertStringToSystem(argv[adx]);
    args[argc] = 0;

    if (sfh->PipeStdout != INVALID_FILE_HANDLE)
        close(sfh->PipeStdout);
    if (sfh->PipeStdin != INVALID_FILE_HANDLE)
        close(sfh->PipeStdin);
    if (sfh->UseStdout == 0 && sfh->PipeStderr != INVALID_FILE_HANDLE)
        close(sfh->PipeStderr);

    while (chldin < 3)
        chldin = dup(chldin);
    while (chldout < 3)
        chldout = dup(chldout);
    while (chlderr < 3)
        chlderr = dup(chlderr);

    close(0);
    if (dup2(chldin, 0) != 0)
        ChildFailed(cmd, "dup2");
    close(1);
    if (dup2(chldout, 1) != 1)
        ChildFailed(cmd, "dup2");
    close(2);
    if (dup2(chlderr, 2) != 2)
        ChildFailed(cmd, "dup2");
    close(chldin);
    close(chldout);
    close(chlderr);

    execve(cmd, args, environ);
    ChildFailed(cmd, "execve");
}
#endif // FOMENT_UNIX

static FObject Subprocess(FSubprocessFileHandles * sfh, long_t argc, FObject argv[],
    const char * who)
{
    FFileHandle chldout, chldin, chlderr;

    if (sfh->ChildStdout == INVALID_FILE_HANDLE)
    {
        MakePipe(&(sfh->PipeStdout), &chldout, who);
#ifdef FOMENT_WINDOWS
        SetHandleInformation(chldout, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
#endif // FOMENT_WINDOWS
    }
    else
        chldout = sfh->ChildStdout;

    if (sfh->ChildStdin == INVALID_FILE_HANDLE)
    {
        MakePipe(&chldin, &(sfh->PipeStdin), who);
#ifdef FOMENT_WINDOWS
        SetHandleInformation(chldin, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
#endif // FOMENT_WINDOWS
    }
    else
        chldin = sfh->ChildStdin;

    if (sfh->UseStdout)
        chlderr = chldout;
    else if (sfh->ChildStderr == INVALID_FILE_HANDLE)
    {
        MakePipe(&(sfh->PipeStderr), &chlderr, who);
#ifdef FOMENT_WINDOWS
        SetHandleInformation(chlderr, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
#endif // FOMENT_WINDOWS
    }
    else
        chlderr = sfh->ChildStderr;

    FlushStandardPorts();

#ifdef FOMENT_WINDOWS
    FChS *app;
    FChS *cmdline;
    ChildArgs(argc, argv, &app, &cmdline);

    STARTUPINFOW si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = chldin;
    si.hStdOutput = chldout;
    si.hStdError = chlderr;

    PROCESS_INFORMATION pi;
    BOOL ret = CreateProcessW(app, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    if (ret == 0)
        RaiseExceptionC(Assertion, who, "create process system call failed",
                List(MakeIntegerFromUInt64(GetLastError())));

    CloseHandle(pi.hThread);

    if (sfh->PipeStdout != INVALID_FILE_HANDLE)
        CloseHandle(chldout);

    if (sfh->PipeStdin != INVALID_FILE_HANDLE)
        CloseHandle(chldin);

    if (sfh->UseStdout == 0 && sfh->PipeStderr != INVALID_FILE_HANDLE)
        CloseHandle(chlderr);

    return(MakeSubprocess(pi.dwProcessId, pi.hProcess, who));
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    RestoreConsole();

    pid_t pid = fork();
    if (pid < 0)
        RaiseExceptionC(Assertion, who, "fork system call failed", List(MakeFixnum(errno)));

    if (pid == 0)
    {
        ChildProcess(sfh, chldin, chldout, chlderr, argc, argv);

        // Never returns.
        FAssert(0);
    }

    // Parent.

    SetupConsoleAgain();

    if (sfh->PipeStdout != INVALID_FILE_HANDLE)
        close(chldout);

    if (sfh->PipeStdin != INVALID_FILE_HANDLE)
        close(chldin);

    if (sfh->UseStdout == 0 && sfh->PipeStderr != INVALID_FILE_HANDLE)
        close(chlderr);

    return(MakeSubprocess(pid, who));
#endif // FOMENT_UNIX
}

Define("%subprocess", SubprocessPrimitive)(long_t argc, FObject argv[])
{
    AtLeastFiveArgsCheck("%subprocess", argc);
    BooleanArgCheck("%subprocess", argv[0]);

    for (long_t adx = 4; adx < argc; adx += 1)
        StringArgCheck("%subprocess", argv[adx]);

    FSubprocessFileHandles sfh;

    if (argv[1] == FalseObject)
        sfh.ChildStdout = INVALID_FILE_HANDLE;
    else
    {
        OutputPortArgCheck("%subprocess", argv[1]);

        sfh.ChildStdout = GetFileHandle(argv[1]);
        if (argv[0] == FalseObject && sfh.ChildStdout == INVALID_FILE_HANDLE)
            RaiseExceptionC(Assertion, "%subprocess", "expected an output file handle port",
                    List(argv[1]));
    }

    if (argv[2] == FalseObject)
        sfh.ChildStdin = INVALID_FILE_HANDLE;
    else
    {
        InputPortArgCheck("%subprocess", argv[2]);

        sfh.ChildStdin = GetFileHandle(argv[2]);
        if (argv[0] == FalseObject && sfh.ChildStdin == INVALID_FILE_HANDLE)
            RaiseExceptionC(Assertion, "%subprocess", "expected an input file handle port",
                    List(argv[2]));
    }

    sfh.ChildStderr = INVALID_FILE_HANDLE;
    sfh.UseStdout = 0;

    if (argv[3] == StdoutSymbol)
        sfh.UseStdout = 1;
    else if (argv[3] != FalseObject)
    {
        OutputPortArgCheck("%subprocess", argv[3]);

        sfh.ChildStderr = GetFileHandle(argv[3]);
        if (argv[0] == FalseObject && sfh.ChildStderr == INVALID_FILE_HANDLE)
            RaiseExceptionC(Assertion, "%subprocess", "expected an output file handle port",
                    List(argv[3]));
    }

    sfh.PipeStdout = INVALID_FILE_HANDLE;
    sfh.PipeStdin = INVALID_FILE_HANDLE;
    sfh.PipeStderr = INVALID_FILE_HANDLE;

    FObject sub = Subprocess(&sfh, argc - 4, argv + 4, "%subprocess");

    FAssert(SubprocessP(sub));

    FObject out = (sfh.ChildStdout == INVALID_FILE_HANDLE) ? OpenInputPipe(sfh.PipeStdout) :
        FalseObject;

    FObject in = (sfh.ChildStdin == INVALID_FILE_HANDLE) ? OpenOutputPipe(sfh.PipeStdin) :
        FalseObject;

    FObject err = (sfh.UseStdout == 0 && sfh.ChildStderr == INVALID_FILE_HANDLE) ?
        OpenInputPipe(sfh.PipeStderr) : FalseObject;

    return(List(sub, out, in, err));
}

Define("subprocess?", SubprocessPPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("subprocess?", argc);

    return(SubprocessP(argv[0]) ? TrueObject : FalseObject);
}

Define("subprocess-wait", SubprocessWaitPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("subprocess-wait", argc);
    SubprocessArgCheck("subprocess-wait", argv[0]);

    FSubprocess * sub = AsSubprocess(argv[0]);

#ifdef FOMENT_WINDOWS
    if (sub->ProcessHandle != INVALID_HANDLE_VALUE)
    {
        if (WaitForSingleObject(sub->ProcessHandle, INFINITE) == WAIT_FAILED)
            RaiseExceptionC(Assertion, "subprocess-wait",
                    "wait for single object system call failed",
                    List(MakeIntegerFromUInt64(GetLastError())));

        DWORD status;
        GetExitCodeProcess(sub->ProcessHandle, &status);
        sub->Status = status;

        CloseHandle(sub->ProcessHandle);
        sub->ProcessHandle = INVALID_HANDLE_VALUE;
    }
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    if (sub->Done == 0)
    {
        int status;
        if (waitpid(sub->ProcessID, &status, 0) < 0)
            RaiseExceptionC(Assertion, "subprocess-wait", "waitpid failed",
                    List(MakeFixnum(errno)));

        sub->Done = 1;
        sub->Status = status;
    }
#endif // FOMENT_UNIX

    return(NoValueObject);
}

Define("subprocess-status", SubprocessStatusPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("subprocess-status", argc);
    SubprocessArgCheck("subprocess-status", argv[0]);

    FSubprocess * sub = AsSubprocess(argv[0]);

#ifdef FOMENT_WINDOWS
    if (sub->ProcessHandle != INVALID_HANDLE_VALUE)
        return(StringCToSymbol("running"));

    return(MakeIntegerFromUInt64(sub->Status));
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    if (sub->Done == 0)
        return(StringCToSymbol("running"));

    if (WIFEXITED(sub->Status))
        return(MakeFixnum(WEXITSTATUS(sub->Status)));

    FAssert(WIFSIGNALED(sub->Status));

    return(MakeFixnum(WTERMSIG(sub->Status)));
#endif // FOMENT_UNIX
}

Define("subprocess-kill", SubprocessKillPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("subprocess-kill", argc);
    SubprocessArgCheck("subprocess-kill", argv[0]);

    FSubprocess * sub = AsSubprocess(argv[0]);

#ifdef FOMENT_WINDOWS
    if (sub->ProcessHandle != INVALID_HANDLE_VALUE && argv[1] != FalseObject)
        TerminateProcess(sub->ProcessHandle, 0);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    if (sub->Done == 0)
        kill(sub->ProcessID, argv[1] == FalseObject ? SIGINT : SIGTERM);
#endif // FOMENT_UNIX

    return(NoValueObject);
}

Define("subprocess-pid", SubprocessPIDPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("subprocess-pid", argc);
    SubprocessArgCheck("subprocess-pid", argv[0]);

    return(MakeFixnum(AsSubprocess(argv[0])->ProcessID));
}

static FObject Primitives[] =
{
    SubprocessPrimitive,
    SubprocessPPrimitive,
    SubprocessWaitPrimitive,
    SubprocessStatusPrimitive,
    SubprocessKillPrimitive,
    SubprocessPIDPrimitive
};

void SetupProcess()
{
    StdoutSymbol = InternSymbol(StdoutSymbol);

    FAssert(StdoutSymbol == StringCToSymbol("stdout"));

    for (ulong_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(Bedrock, BedrockLibrary, Primitives[idx]);
}
