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
#endif // FOMENT_UNIX

#include <stdio.h>
#include <time.h>
#include <string.h>
#include "foment.hpp"
#include "execute.hpp"
#include "syncthrd.hpp"

// ---- Threads ----

FThread * MakeThread(OSThreadHandle h, FObject thnk)
{
    FThread * thrd = (FThread *) MakeObject(ThreadTag, sizeof(FThread), 4, "run-thread");
    thrd->Result = NoValueObject;
    thrd->Thunk = thnk;
    thrd->Handle = h;
    thrd->Name = FalseObject;
    thrd->Specific = FalseObject;

    InitializeExclusive(&(thrd->Exclusive));
    InitializeCondition(&(thrd->Condition));
    thrd->State = THREAD_STATE_NEW;
    thrd->Exit = THREAD_EXIT_NORMAL;

    return(thrd);
}

void DeleteThread(FObject thrd)
{
    FAssert(ThreadP(thrd));

    DeleteExclusive(&(AsThread(thrd)->Exclusive));
    DeleteCondition(&(AsThread(thrd)->Condition));
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

static void SetThreadDone(FThread * thrd, ulong_t exit)
{
    EnterExclusive(&(thrd->Exclusive));
    thrd->State = THREAD_STATE_DONE;
    thrd->Exit = exit;
    WakeCondition(&(thrd->Condition));
    LeaveExclusive(&(thrd->Exclusive));
}

typedef struct
{
    FThread * Thread;
    FObject Parameters;
} FStartThread;

static void FomentThread(FStartThread * st)
{
    FThread * thrd = st->Thread;

    FAssert(thrd->State == THREAD_STATE_NEW);

    FThreadState ts;
    if (EnterThread(&ts, thrd, st->Parameters) == 0)
    {
        thrd->Result = StartThreadOutOfMemory;
        SetThreadDone(thrd, THREAD_EXIT_UNCAUGHT);
        return;
    }

    FAssert(ts.Thread == thrd);
    FAssert(ThreadP(ts.Thread));

    EnterExclusive(&(thrd->Exclusive));
    thrd->State = THREAD_STATE_READY;
    WakeCondition(&(thrd->Condition));

    while (thrd->State != THREAD_STATE_RUNNING)
        ConditionWait(&(thrd->Condition), &(thrd->Exclusive));
    LeaveExclusive(&(thrd->Exclusive));

    ulong_t exit = THREAD_EXIT_NORMAL;
    try
    {
        if (ProcedureP(thrd->Thunk))
            thrd->Result = ExecuteProc(thrd->Thunk);
        else
        {
            FAssert(PrimitiveP(thrd->Thunk));

            thrd->Result = AsPrimitive(thrd->Thunk)->PrimitiveFn(0, 0);
        }
    }
    catch (FObject exc)
    {
        /*
        if (ExceptionP(exc) == 0)
            WriteStringC(StandardOutput, "exception: ");
        Write(StandardOutput, exc, 0);
        WriteCh(StandardOutput, '\n');
        */

        thrd->Result = exc;
        exit = THREAD_EXIT_UNCAUGHT;
    }

    SetThreadDone(thrd, exit);

    LeaveThread(&ts);
}

#ifdef FOMENT_WINDOWS
static DWORD WINAPI StartThread(LPVOID ptr)
{
    FomentThread((FStartThread *) ptr);
    return(0);
}
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
static void * StartThread(void * ptr)
{
    FStartThread * st = (FStartThread *) ptr;
    st->Thread->Handle = pthread_self();

    FomentThread(st);
    return(0);
}
#endif // FOMENT_UNIX

Define("%run-thread", RunThreadPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("run-thread", argc);
    ProcedureArgCheck("run-thread", argv[0]);
    BooleanArgCheck("run-thread", argv[1]);

    FThread * thrd = MakeThread(0, argv[0]);
    FStartThread st = {thrd, CurrentParameters()};

#ifdef FOMENT_WINDOWS
    HANDLE h = CreateThread(0, 0, StartThread, &st, CREATE_SUSPENDED, 0);
    if (h == 0)
    {
        unsigned int ec = GetLastError();
        RaiseExceptionC(Assertion, "run-thread", "CreateThread failed", List(MakeFixnum(ec)));
    }

    thrd->Handle = h;
    ResumeThread(h);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    pthread_t pt;
    int ret = pthread_create(&pt, 0, StartThread, &st);
    if (ret != 0)
        RaiseExceptionC(Assertion, "run-thread", "pthread_create failed", List(MakeFixnum(ret)));
#endif // FOMENT_UNIX

    EnterExclusive(&(thrd->Exclusive));
    while (thrd->State == THREAD_STATE_NEW)
        ConditionWait(&(thrd->Condition), &(thrd->Exclusive));

    if (thrd->State == THREAD_STATE_DONE)
    {
        LeaveExclusive(&(thrd->Exclusive));
        Raise(thrd->Result);

        FAssert(0); // Never reached.
    }

    if (argv[1] == TrueObject && thrd->State == THREAD_STATE_READY)
    {
        thrd->State = THREAD_STATE_RUNNING;
        WakeCondition(&(thrd->Condition));
    }
    LeaveExclusive(&(thrd->Exclusive));

    return(thrd);
}

void ThreadExit(FObject obj, ulong_t threadExit)
{
    FThreadState * ts = GetThreadState();

    AsThread(ts->Thread)->Result = obj;
    SetThreadDone(AsThread(ts->Thread), threadExit);

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
    FMustBe(argc == 2);
    FMustBe(FixnumP(argv[1]) && AsFixnum(argv[1]) >= THREAD_EXIT_NORMAL &&
            AsFixnum(argv[1]) <= THREAD_EXIT_UNCAUGHT);

    ThreadExit(argv[0], (ulong_t) AsFixnum(argv[1]));
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

    FAssert(AsThread(argv[0])->State != THREAD_STATE_NEW);

    if (AsThread(argv[0])->State == THREAD_STATE_READY)
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

#ifdef FOMENT_WINDOWS
static DWORD TimeDelta(FObject obj)
{
    FAssert(TimeP(obj));

    LARGE_INTEGER li;
    li.LowPart = AsTime(obj)->filetime.dwLowDateTime;
    li.HighPart = AsTime(obj)->filetime.dwHighDateTime;

    SYSTEMTIME st;
    GetLocalTime(&st);
    FILETIME ft;
    SystemTimeToFileTime(&st, &ft);
    LARGE_INTEGER cli;
    cli.LowPart = ft.dwLowDateTime;
    cli.HighPart = ft.dwHighDateTime;

    if (cli.QuadPart > li.QuadPart)
        return(0);

    // Convert from 100 nano seconds (10^ - 7) to milliseconds (10^ - 3).
    return((DWORD) ((li.QuadPart - cli.QuadPart) / 10000));
}
#endif // FOMENT_WINDOWS

Define("thread-sleep!", ThreadSleepPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("thread-sleep!", argc);

    if (NonNegativeExactIntegerP(argv[0], 0))
    {
#ifdef FOMENT_WINDOWS
        DWORD n = (DWORD) AsFixnum(argv[0]);
        EnterWait();
        SleepEx(n * 1000, TRUE);
        LeaveWait();
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
        struct timespec ts;
        ts.tv_sec = AsFixnum(argv[0]);
        ts.tv_nsec = 0;
        EnterWait();
        nanosleep(&ts, 0);
        LeaveWait();
#endif // FOMENT_UNIX
    }
    else if (TimeP(argv[0]))
    {
#ifdef FOMENT_WINDOWS
        DWORD n = TimeDelta(argv[0]);
        if (n > 0)
        {
            EnterWait();
            SleepEx(n, TRUE);
            LeaveWait();
        }
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
        EnterWait();
        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &(AsTime(argv[0])->timespec), 0);
        LeaveWait();
#endif // FOMENT_UNIX
    }
    else
        RaiseExceptionC(Assertion, "thread-sleep!",
                "expected an exact non-negative integer or time", List(argv[0]));

    return(NoValueObject);
}

static void NotifyTerminate(FObject thrd);
Define("%thread-terminate!", ThreadTerminatePrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("thread-terminate!", argc);
    ThreadArgCheck("thread-terminate!", argv[0]);

    FAssert(GetThreadState()->Thread != argv[0]);

    NotifyTerminate(argv[0]);

    FThread * thrd = AsThread(argv[0]);
    EnterExclusive(&(thrd->Exclusive));
    while (thrd->State != THREAD_STATE_DONE)
        ConditionWait(&(thrd->Condition), &(thrd->Exclusive));
    LeaveExclusive(&(thrd->Exclusive));

    return(NoValueObject);
}

Define("thread-join!", ThreadJoinPrimitive)(long_t argc, FObject argv[])
{
    OneToThreeArgsCheck("thread-join!", argc);
    ThreadArgCheck("thread-join!", argv[0]);

    // XXX: handle optional timeout

    FThread * thrd = AsThread(argv[0]);
    EnterExclusive(&(thrd->Exclusive));
    while (thrd->State != THREAD_STATE_DONE)
        ConditionWait(&(thrd->Condition), &(thrd->Exclusive));
    LeaveExclusive(&(thrd->Exclusive));

    if (thrd->Exit == THREAD_EXIT_TERMINATED)
        RaiseExceptionC(Assertion, "thread-join!", StringCToSymbol("terminated-thread-exception"),
                "join thread was terminated", List(argv[0]));
    else if (thrd->Exit == THREAD_EXIT_UNCAUGHT)
        RaiseExceptionC(Assertion, "thread-join!", StringCToSymbol("uncaught-exception"),
                "join thread had uncaught exception", List(thrd->Result, argv[0]));
    return(thrd->Result);
}

Define("sleep", SleepPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("sleep", argc);
    NonNegativeArgCheck("sleep", argv[0], 0);

#ifdef FOMENT_WINDOWS
    DWORD n = (DWORD) AsFixnum(argv[0]);
    EnterWait();
    SleepEx(n, TRUE);
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
    SYSTEMTIME st;
    GetLocalTime(&st);
    SystemTimeToFileTime(&st, &(time->filetime));
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    if (clock_gettime(CLOCK_REALTIME, &(time->timespec)) != 0)
        RaiseExceptionC(Assertion, "current-time", "clock_gettime failed",
                List(MakeFixnum(errno)));
#endif // FOMENT_UNIX

    return(time);
}

static FObject MakeTimeSeconds(int64_t n)
{
    FTime * time = (FTime *) MakeObject(TimeTag, sizeof(FTime), 0, "make-time");

#ifdef FOMENT_WINDOWS
    LARGE_INTEGER li;
    // Convert from seconds to 100 nano seconds (10^ - 7).
    li.QuadPart = n * 10000000;
    time->filetime.dwLowDateTime = li.LowPart;
    time->filetime.dwHighDateTime = li.HighPart;
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    time->timespec.tv_sec = n;
    time->timespec.tv_nsec = 0;
#endif // FOMENT_UNIX

    return(time);
}

void WriteTime(FWriteContext * wctx, FObject obj)
{
    FAssert(TimeP(obj));

    wctx->WriteStringC("#<time: ");

#ifdef FOMENT_WINDOWS
    SYSTEMTIME st;
    FileTimeToSystemTime(&(AsTime(obj)->filetime), &st);
    char buf[128];
    sprintf_s(buf, sizeof(buf), "%d-%d-%d %02d:%02d:%02d", st.wMonth, st.wDay, st.wYear,
              st.wHour, st.wMinute, st.wSecond);
    wctx->WriteStringC(buf);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    struct tm tm;
    char buf[128];
    time_t tt = AsTime(obj)->timespec.tv_sec;
    localtime_r(&tt, &tm);
    sprintf_s(buf, sizeof(buf), "%d-%d-%d %02d:%02d:%02d", tm.tm_mon + 1, tm.tm_mday,
              tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);
    wctx->WriteStringC(buf);
#endif // FOMENT_UNIX

    wctx->WriteCh('>');
}

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
    LARGE_INTEGER li;
    li.LowPart = AsTime(argv[0])->filetime.dwLowDateTime;
    li.HighPart = AsTime(argv[0])->filetime.dwHighDateTime;
    // Convert from 100 nano seconds (10^ - 7) to seconds.
    return(MakeIntegerFromInt64(li.QuadPart / 10000000));
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    return(MakeIntegerFromInt64(AsTime(argv[0])->timespec.tv_sec));
#endif // FOMENT_UNIX
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
static void NotifyTerminate(FObject thrd)
{
    EnterExclusive(&ThreadsExclusive);

    FThreadState * ts = Threads;
    while (ts != 0)
    {
        if (ts->Thread == thrd)
        {
            ts->NotifyFlag = 1;
            ts->NotifyObject = FalseObject;
            ts->Terminate = 1;
            InterruptThread(ts->Thread);
            break;
        }

        ts = ts->Next;
    }

    LeaveExclusive(&ThreadsExclusive);
}

static void NotifyBroadcast(FObject obj)
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
    ThreadSleepPrimitive,
    ThreadTerminatePrimitive,
    ThreadJoinPrimitive,
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
