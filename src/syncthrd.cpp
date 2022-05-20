/*

Foment

*/

#ifdef FOMENT_WINDOWS
#include <windows.h>
#define exit(n) _exit(n)
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#endif // FOMENT_UNIX

#include <time.h>
#include <string.h>
#include "foment.hpp"
#include "execute.hpp"
#include "syncthrd.hpp"

// ---- Threads ----

FObject MakeThread(OSThreadHandle h, FObject thnk, FObject prms)
{
    FThread * thrd = (FThread *) MakeObject(ThreadTag, sizeof(FThread), 5, "run-thread");
    thrd->Result = NoValueObject;
    thrd->Thunk = thnk;
    thrd->Parameters = prms;
    thrd->Handle = h;
    thrd->Name = FalseObject;
    thrd->Specific = FalseObject;

    InitializeExclusive(&(thrd->Exclusive));
    InitializeCondition(&(thrd->Condition));
    thrd->State = THREAD_STATE_NEW;

    return(thrd);
}

void WriteThread(FWriteContext * wctx, FObject obj)
{
    FAssert(ThreadP(obj));

    wctx->WriteStringC("#<thread: ");

    FCh s[16];
    long_t sl = FixnumAsString((long_t) AsThread(obj)->Handle, s, 16);
    wctx->WriteString(s, sl);

    EnterExclusive(&(AsThread(obj)->Exclusive));
    FObject name = AsThread(obj)->Name;
    LeaveExclusive(&(AsThread(obj)->Exclusive));
    if (name != FalseObject)
    {
        wctx->WriteCh(' ');
        wctx->Write(name);
    }
    wctx->WriteCh('>');
}

// ---- Exclusives ----

#ifdef FOMENT_UNIX
void InitializeExclusive(OSExclusive * ose)
{
    pthread_mutexattr_t mta;

    pthread_mutexattr_init(&mta);
    pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(ose, &mta);
}
#endif // FOMENT_UNIX

FObject MakeExclusive()
{
    FExclusive * e = (FExclusive *) MakeObject(ExclusiveTag, sizeof(FExclusive), 0,
            "make-exclusive");
    InitializeExclusive(&e->Exclusive);
    InstallGuardian(e, CleanupTConc);

    return(e);
}

void WriteExclusive(FWriteContext * wctx, FObject obj)
{
    FAssert(ExclusiveP(obj));

    wctx->WriteStringC("#<exclusive: ");

    FCh s[16];
    long_t sl = FixnumAsString((long_t) &AsExclusive(obj)->Exclusive, s, 16);
    wctx->WriteString(s, sl);
    wctx->WriteCh('>');
}

// ---- Conditions ----

static FObject MakeCondition()
{
    FCondition * c = (FCondition *) MakeObject(ConditionTag, sizeof(FCondition), 0,
            "make-condition");
    InitializeCondition(&c->Condition);

    return(c);
}

void WriteCondition(FWriteContext * wctx, FObject obj)
{
    FAssert(ConditionP(obj));

    wctx->WriteStringC("#<condition: ");

    FCh s[16];
    long_t sl = FixnumAsString((long_t) &AsCondition(obj)->Condition, s, 16);
    wctx->WriteString(s, sl);
    wctx->WriteCh('>');
}

Define("current-thread", CurrentThreadPrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("current-thread", argc);

    return(GetThreadState()->Thread);
}

Define("thread?", ThreadPPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("thread?", argc);

    return(ThreadP(argv[0]) ? TrueObject : FalseObject);
}

static FObject CurrentParameters()
{
    FThreadState * ts = GetThreadState();
    FObject v = MakeVector(ts->ParametersLength, 0, NoValueObject);

    for (ulong_t idx = 0; idx < ts->ParametersLength; idx++)
            AsVector(v)->Vector[idx] = ts->Parameters[idx];

    return(v);
}

static void FomentThread(FObject obj)
{
    FThreadState ts;

    FAssert(ThreadP(obj));

    EnterExclusive(&(AsThread(obj)->Exclusive));
    while (AsThread(obj)->State != THREAD_STATE_RUNNING)
        ConditionWait(&(AsThread(obj)->Condition), &(AsThread(obj)->Exclusive));
    LeaveExclusive(&(AsThread(obj)->Exclusive));

    try
    {
        if (EnterThread(&ts, obj, AsThread(obj)->Parameters) == 0)
            Raise(StartThreadOutOfMemory);

        FAssert(ts.Thread == obj);
        FAssert(ThreadP(ts.Thread));

        AsThread(obj)->Parameters = NoValueObject;

        if (ProcedureP(AsThread(obj)->Thunk))
            AsThread(obj)->Result = ExecuteProc(AsThread(obj)->Thunk);
        else
        {
            FAssert(PrimitiveP(AsThread(obj)->Thunk));

            AsThread(obj)->Result =
                    AsPrimitive(AsThread(obj)->Thunk)->PrimitiveFn(0, 0);
        }
    }
    catch (FObject exc)
    {
        if (ExceptionP(exc) == 0)
            WriteStringC(StandardOutput, "exception: ");
        Write(StandardOutput, exc, 0);
        WriteCh(StandardOutput, '\n');

        AsThread(obj)->Result = exc;
    }

    EnterExclusive(&(AsThread(obj)->Exclusive));
    AsThread(obj)->State = THREAD_STATE_DONE;
    WakeCondition(&(AsThread(obj)->Condition));
    LeaveExclusive(&(AsThread(obj)->Exclusive));

    LeaveThread(&ts);
}

#ifdef FOMENT_WINDOWS
static DWORD WINAPI StartThread(FObject obj)
{
    FomentThread(obj);
    return(0);
}
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
static void * StartThread(FObject obj)
{
    FAssert(ThreadP(obj));
    AsThread(obj)->Handle = pthread_self();

    FomentThread(obj);
    return(0);
}
#endif // FOMENT_UNIX

Define("%run-thread", RunThreadPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("run-thread", argc);
    ProcedureArgCheck("run-thread", argv[0]);
    BooleanArgCheck("run-thread", argv[1]);

    FObject thrd = MakeThread(0, argv[0], CurrentParameters());

    if (argv[1] == FalseObject)
        AsThread(thrd)->State = THREAD_STATE_NEW;
    else
        AsThread(thrd)->State = THREAD_STATE_RUNNING;

#ifdef FOMENT_WINDOWS
    HANDLE h = CreateThread(0, 0, StartThread, thrd, CREATE_SUSPENDED, 0);
    if (h == 0)
    {
        unsigned int ec = GetLastError();
        RaiseExceptionC(Assertion, "run-thread", "CreateThread failed", List(MakeFixnum(ec)));
    }

    EnterExclusive(&ThreadsExclusive);
    TotalThreads += 1;
    LeaveExclusive(&ThreadsExclusive);

    AsThread(thrd)->Handle = h;
    ResumeThread(h);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    EnterExclusive(&ThreadsExclusive);
    TotalThreads += 1;
    LeaveExclusive(&ThreadsExclusive);

    pthread_t pt;
    int ret = pthread_create(&pt, 0, StartThread, thrd);
    if (ret != 0)
    {
        EnterExclusive(&ThreadsExclusive);
        TotalThreads -= 1;
        LeaveExclusive(&ThreadsExclusive);

        RaiseExceptionC(Assertion, "run-thread", "pthread_create failed", List(MakeFixnum(ret)));
    }
#endif // FOMENT_UNIX

    return(thrd);
}

void ThreadExit(FObject obj)
{
    FThreadState * ts = GetThreadState();

    AsThread(ts->Thread)->Result = obj;

    if (LeaveThread(ts) == 0)
    {
        FlushStandardPorts();

        exit(0);
    }
    else
    {
#ifdef FOMENT_WINDOWS
        ExitThread(0);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
        pthread_exit(0);
#endif // FOMENT_UNIX
    }
}

Define("%exit-thread", ExitThreadPrimitive)(long_t argc, FObject argv[])
{
    FMustBe(argc == 1);

    ThreadExit(argv[0]);
    return(NoValueObject);
}

Define("%exit", ExitPrimitive)(long_t argc, FObject argv[])
{
    ZeroOrOneArgsCheck("exit", argc);

    FlushStandardPorts();
    if (argc == 0 || argv[0] == TrueObject)
        exit(0);
    if (FixnumP(argv[0]))
        exit((int) AsFixnum(argv[0]));

    exit(-1);
    return(NoValueObject);
}

Define("thread-name", ThreadNamePrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("thread-name", argc);
    ThreadArgCheck("thread-name", argv[0]);

    EnterExclusive(&(AsThread(argv[0])->Exclusive));
    FObject name = AsThread(argv[0])->Name;
    LeaveExclusive(&(AsThread(argv[0])->Exclusive));

    return(name);
}

Define("thread-name-set!", ThreadNameSetPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("thread-name-set!", argc);
    ThreadArgCheck("thread-name-set!", argv[0]);

    EnterExclusive(&(AsThread(argv[0])->Exclusive));
    AsThread(argv[0])->Name = argv[1];
    LeaveExclusive(&(AsThread(argv[0])->Exclusive));

    return(NoValueObject);
}

Define("thread-specific", ThreadSpecificPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("thread-specific", argc);
    ThreadArgCheck("thread-specific", argv[0]);

    EnterExclusive(&(AsThread(argv[0])->Exclusive));
    FObject obj = AsThread(argv[0])->Specific;
    LeaveExclusive(&(AsThread(argv[0])->Exclusive));

    return(obj);
}

Define("thread-specific-set!", ThreadSpecificSetPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("thread-specific-set!", argc);
    ThreadArgCheck("thread-specific-set!", argv[0]);

    EnterExclusive(&(AsThread(argv[0])->Exclusive));
    AsThread(argv[0])->Specific = argv[1];
    LeaveExclusive(&(AsThread(argv[0])->Exclusive));

    return(NoValueObject);
}

Define("thread-start!", ThreadStartPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("thread-start!", argc);
    ThreadArgCheck("thread-start!", argv[0]);

    EnterExclusive(&(AsThread(argv[0])->Exclusive));
    if (AsThread(argv[0])->State == THREAD_STATE_NEW)
    {
        AsThread(argv[0])->State = THREAD_STATE_RUNNING;
        WakeCondition(&(AsThread(argv[0])->Condition));
        LeaveExclusive(&(AsThread(argv[0])->Exclusive));
    }
    else
    {
        LeaveExclusive(&(AsThread(argv[0])->Exclusive));
        RaiseExceptionC(Assertion, "thread-start!", "expected a new thread", List(argv[0]));
    }

    return(argv[0]);
}

Define("sleep", SleepPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("sleep", argc);
    NonNegativeArgCheck("sleep", argv[0], 0);

#ifdef FOMENT_WINDOWS
    DWORD n = (DWORD) AsFixnum(argv[0]);
    EnterWait();
    Sleep(n);
    LeaveWait();
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    useconds_t us = AsFixnum(argv[0]);
    EnterWait();
    usleep(us * 1000);
    LeaveWait();
#endif // FOMENT_UNIX

    return(NoValueObject);
}

Define("exclusive?", ExclusivePPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("exclusive?", argc);

    return(ExclusiveP(argv[0]) ? TrueObject : FalseObject);
}

Define("make-exclusive", MakeExclusivePrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("make-exclusive", argc);

    return(MakeExclusive());
}

Define("enter-exclusive", EnterExclusivePrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("enter-exclusive", argc);
    ExclusiveArgCheck("enter-exclusive", argv[0]);

    OSExclusive * ose = &AsExclusive(argv[0])->Exclusive;

    EnterWait();
    EnterExclusive(ose);
    LeaveWait();
    return(NoValueObject);
}

Define("leave-exclusive", LeaveExclusivePrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("leave-exclusive", argc);
    ExclusiveArgCheck("leave-exclusive", argv[0]);

    LeaveExclusive(&AsExclusive(argv[0])->Exclusive);
    return(NoValueObject);
}

Define("try-exclusive", TryExclusivePrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("try-exclusive", argc);
    ExclusiveArgCheck("try-exclusive", argv[0]);

    return(TryExclusive(&AsExclusive(argv[0])->Exclusive) ? TrueObject : FalseObject);
}

Define("condition?", ConditionPPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("condition?", argc);

    return(ConditionP(argv[0]) ? TrueObject : FalseObject);
}

Define("make-condition", MakeConditionPrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("make-condition", argc);

    return(MakeCondition());
}

Define("condition-wait", ConditionWaitPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("condition-wait", argc);
    ConditionArgCheck("condition-wait", argv[0]);
    ExclusiveArgCheck("condition-wait", argv[1]);

    OSCondition * osc = &AsCondition(argv[0])->Condition;
    OSExclusive * ose = &AsExclusive(argv[1])->Exclusive;

    EnterWait();
    ConditionWait(osc, ose);
    LeaveWait();
    return(NoValueObject);
}

Define("condition-wake", ConditionWakePrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("condition-wake", argc);
    ConditionArgCheck("condition-wake", argv[0]);

    WakeCondition(&AsCondition(argv[0])->Condition);
    return(NoValueObject);
}

Define("condition-wake-all", ConditionWakeAllPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("condition-wake-all", argc);
    ConditionArgCheck("condition-wake-all", argv[0]);

    WakeAllCondition(&AsCondition(argv[0])->Condition);
    return(NoValueObject);
}

static FObject MakeCurrentTime()
{
    FTime * time = (FTime *) MakeObject(TimeTag, sizeof(FTime), 0, "make-time");
#ifdef FOMENT_WINDOWS
    // XXX
#else // FOMENT_WINDOWS
    if (clock_gettime(CLOCK_REALTIME, &(time->timespec)) != 0)
        RaiseExceptionC(Assertion, "current-time", "clock_gettime failed",
                List(MakeFixnum(errno)));
#endif // FOMENT_WINDOWS

    return(time);
}

static FObject MakeTimeSeconds(int64_t n)
{
    FTime * time = (FTime *) MakeObject(TimeTag, sizeof(FTime), 0, "make-time");
#ifdef FOMENT_WINDOWS
    // XXX
#else // FOMENT_WINDOWS
    time->timespec.tv_sec = n;
    time->timespec.tv_nsec = 0;
#endif // FOMENT_WINDOWS

    return(time);
}

void WriteTime(FWriteContext * wctx, FObject obj)
{
    FAssert(TimeP(obj));

    wctx->WriteStringC("#<time: ");

#ifdef FOMENT_WINDOWS
    // XXX
#else // FOMENT_WINDOWS
    struct tm tm;
    char buf[128];
    time_t tt = AsTime(obj)->timespec.tv_sec;
    localtime_r(&tt, &tm);
    sprintf_s(buf, sizeof(buf), "%d-%d-%d %02d:%02d:%02d", tm.tm_mon + 1, tm.tm_mday,
              tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);
    wctx->WriteStringC(buf);
#endif // FOMENT_WINDOWS

    wctx->WriteCh('>');
}

/*
https://docs.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-filetime
https://stackoverflow.com/questions/9112749/converting-ularge-integer-quadpart-in-to-millisecond
https://stackoverflow.com/questions/19709580/c-convert-filetime-to-seconds
https://stackoverflow.com/questions/46201834/how-to-convert-milli-seconds-time-to-filetime-in-c
https://stackoverflow.com/questions/24878395/convert-filetime-to-portable-time-unit
*/

Define("current-time", CurrentTimePrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("current-second", argc);

    return(MakeCurrentTime());
}

Define("time?", TimePPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("time?", argc);

    return(TimeP(argv[0]) ? TrueObject : FalseObject);
}

Define("time->seconds", TimeToSecondsPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("time->seconds", argc);
    if (TimeP(argv[0]) == 0)
        RaiseExceptionC(Assertion, "time->seconds", "expected time", List(argv[0]));

#ifdef FOMENT_WINDOWS
    // XXX
    return(MakeFixnum(0));
#else // FOMENT_WINDOWS
    return(MakeIntegerFromInt64(AsTime(argv[0])->timespec.tv_sec));
#endif // FOMENT_WINDOWS
}

Define("seconds->time", SecondsToTimePrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("seconds->time", argc);

    int64_t n;
    if (FixnumP(argv[0]))
        n = AsFixnum(argv[0]);
    else if (BignumP(argv[0]) == 0 || BignumToInt64(argv[0], &n) == 0)
        RaiseExceptionC(Assertion, "seconds->time", "expected an exact integer", List(argv[0]));

    return(MakeTimeSeconds(n));
}

static void InterruptThread(FObject thrd);
void NotifyThread(FObject thrd, FObject obj)
{
    EnterExclusive(&ThreadsExclusive);

    FThreadState * ts = Threads;
    while (ts != 0)
    {
        if (ts->Thread == thrd)
        {
            ts->NotifyFlag = 1;
            ts->NotifyObject = obj;
            InterruptThread(ts->Thread);

            break;
        }

        ts = ts->Next;
    }

    LeaveExclusive(&ThreadsExclusive);
}

void NotifyBroadcast(FObject obj)
{
    EnterExclusive(&ThreadsExclusive);

    FThreadState * ts = Threads;
    while (ts != 0)
    {
        ts->NotifyFlag = 1;
        ts->NotifyObject = obj;
        InterruptThread(ts->Thread);

        ts = ts->Next;
    }

    LeaveExclusive(&ThreadsExclusive);
}

#define NOTIFY_EXIT 0
#define NOTIFY_IGNORE 1
#define NOTIFY_BROADCAST 2

static long_t SigIntNotify;
static long_t SigIntCount;
static time_t SigIntTime;

Define("set-ctrl-c-notify!", SetCtrlCNotifyPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("set-ctrl-c-notify!", argc);

    if (argv[0] == StringCToSymbol("exit"))
        SigIntNotify = NOTIFY_EXIT;
    else if (argv[0] == StringCToSymbol("ignore"))
        SigIntNotify = NOTIFY_IGNORE;
    else if (argv[0] == StringCToSymbol("broadcast"))
        SigIntNotify = NOTIFY_BROADCAST;
    else
        RaiseExceptionC(Assertion, "set-ctrl-c-notify!", "expected exit, ignore, or broadcast",
                List(argv[0]));

    return(NoValueObject);
}

static void NotifySigInt()
{
    if (SigIntNotify == NOTIFY_EXIT)
        exit(-1);
    else if (SigIntNotify == NOTIFY_BROADCAST)
    {
        time_t now = time(0);

        if (now == SigIntTime)
        {
            SigIntCount += 1;

            if (SigIntCount > 2)
                ErrorExitFoment("aborted", "repeated control-C\n");
        }
        else
        {
            SigIntCount = 1;
            SigIntTime = now;
        }

        NotifyBroadcast(SigIntSymbol);
    }
}

#ifdef FOMENT_WINDOWS
static void CALLBACK UserAPC(ULONG_PTR ign)
{
    // Nothing.
}

static void InterruptThread(FObject thrd)
{
    FAssert(ThreadP(thrd));

    QueueUserAPC(UserAPC, AsThread(thrd)->Handle, 0);
}

static BOOL WINAPI CtrlHandler(DWORD ct)
{
    if (ct == CTRL_C_EVENT)
    {
        NotifySigInt();
        return(TRUE);
    }

    return(FALSE);
}

static void SetupSignals()
{
    SetConsoleCtrlHandler(CtrlHandler, TRUE);
}
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
static void InterruptThread(FObject thrd)
{
    pthread_kill(AsThread(thrd)->Handle, SIGUSR2);
}

static void * SignalThread(void * ign)
{
    sigset_t ss;
    sigemptyset(&ss);
    sigaddset(&ss, SIGINT);

    for (;;)
    {
        int sig;

        sigwait(&ss, &sig);

        FAssert(sig == SIGINT);

        NotifySigInt();
    }

    return(0);
}

static void HandleSigUsr2(int sig)
{
    // Nothing.
}

static void SetupSignals()
{
    sigset_t ss;
    sigemptyset(&ss);
    sigaddset(&ss, SIGINT);

    sigprocmask(SIG_BLOCK, &ss, 0);

    signal(SIGUSR2, HandleSigUsr2);

    pthread_t pt;
    pthread_create(&pt, 0, SignalThread, 0);
}
#endif // FOMENT_UNIX

FWithExclusive::FWithExclusive(FObject exc)
{
    FAssert(ExclusiveP(exc));

    Exclusive = &AsExclusive(exc)->Exclusive;
    EnterExclusive(Exclusive);
}

FWithExclusive::FWithExclusive(OSExclusive * ose)
{
    Exclusive = ose;
    EnterExclusive(Exclusive);
}

FWithExclusive::~FWithExclusive()
{
    LeaveExclusive(Exclusive);
}

static FObject Primitives[] =
{
    CurrentThreadPrimitive,
    ThreadPPrimitive,
    RunThreadPrimitive,
    ExitThreadPrimitive,
    ExitPrimitive,
    ThreadNamePrimitive,
    ThreadNameSetPrimitive,
    ThreadSpecificPrimitive,
    ThreadSpecificSetPrimitive,
    ThreadStartPrimitive,
    SleepPrimitive,
    ExclusivePPrimitive,
    MakeExclusivePrimitive,
    EnterExclusivePrimitive,
    LeaveExclusivePrimitive,
    TryExclusivePrimitive,
    ConditionPPrimitive,
    MakeConditionPrimitive,
    ConditionWaitPrimitive,
    ConditionWakePrimitive,
    ConditionWakeAllPrimitive,
    CurrentTimePrimitive,
    TimePPrimitive,
    TimeToSecondsPrimitive,
    SecondsToTimePrimitive,
    SetCtrlCNotifyPrimitive
};

void SetupThreads()
{
    for (ulong_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(Bedrock, BedrockLibrary, Primitives[idx]);

    SigIntNotify = NOTIFY_EXIT;
    SigIntCount = 0;
    SigIntTime = time(0);

    SetupSignals();
}
