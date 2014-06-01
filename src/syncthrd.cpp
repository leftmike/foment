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
#endif // FOMENT_UNIX

#include <time.h>
#include "foment.hpp"
#include "execute.hpp"
#include "syncthrd.hpp"

// ---- Threads ----

FObject MakeThread(OSThreadHandle h, FObject thnk, FObject prms, FObject idxprms)
{
    FThread * thrd = (FThread *) MakeObject(sizeof(FThread), ThreadTag);
    thrd->Reserved = MakeLength(0, ThreadTag);
    thrd->Result = NoValueObject;
    thrd->Handle = h;
    thrd->Thunk = thnk;
    thrd->Parameters = prms;
    thrd->IndexParameters = idxprms;

    return(thrd);
}

void WriteThread(FObject port, FObject obj, int_t df)
{
    FAssert(ThreadP(obj));

    WriteStringC(port, "#<thread: ");

    FCh s[16];
    int_t sl = FixnumAsString((FFixnum) AsThread(obj)->Handle, s, 16);
    WriteString(port, s, sl);
    WriteCh(port, '>');
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

static FObject MakeExclusive()
{
    FExclusive * e = (FExclusive *) MakePinnedObject(sizeof(FExclusive), "make-exclusive");
    e->Reserved = MakePinnedLength(0, ExclusiveTag);

    InitializeExclusive(&e->Exclusive);

    InstallGuardian(e, R.CleanupTConc);
    return(e);
}

void WriteExclusive(FObject port, FObject obj, int_t df)
{
    FAssert(ExclusiveP(obj));

    WriteStringC(port, "#<exclusive: ");

    FCh s[16];
    int_t sl = FixnumAsString((FFixnum) &AsExclusive(obj)->Exclusive, s, 16);
    WriteString(port, s, sl);
    WriteCh(port, '>');
}

// ---- Conditions ----

static FObject MakeCondition()
{
    FCondition * c = (FCondition *) MakePinnedObject(sizeof(FCondition), "make-condition");
    c->Reserved = MakePinnedLength(0, ConditionTag);

    InitializeCondition(&c->Condition);
    return(c);
}

void WriteCondition(FObject port, FObject obj, int_t df)
{
    FAssert(ConditionP(obj));

    WriteStringC(port, "#<condition: ");

    FCh s[16];
    int_t sl = FixnumAsString((FFixnum) &AsCondition(obj)->Condition, s, 16);
    WriteString(port, s, sl);
    WriteCh(port, '>');
}

Define("current-thread", CurrentThreadPrimitive)(int_t argc, FObject argv[])
{
    ZeroArgsCheck("current-thread", argc);

    return(GetThreadState()->Thread);
}

Define("thread?", ThreadPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("thread?", argc);

    return(ThreadP(argv[0]) ? TrueObject : FalseObject);
}

static FObject CurrentIndexParameters()
{
    FObject v = MakeVector(INDEX_PARAMETERS, 0, NoValueObject);

    for (int_t idx = 0; idx < INDEX_PARAMETERS; idx++)
    {
        FAssert(PairP(GetThreadState()->IndexParameters[idx]));

//        AsVector(v)->Vector[idx] = MakePair(First(GetThreadState()->IndexParameters[idx]),
//                EmptyListObject);
        ModifyVector(v, idx, MakePair(First(GetThreadState()->IndexParameters[idx]),
                EmptyListObject));
    }

    return(v);
}

static void FomentThread(FObject obj)
{
    FThreadState ts;

    FAssert(ThreadP(obj));

    EnterThread(&ts, obj, AsThread(obj)->Parameters, AsThread(obj)->IndexParameters);

    FAssert(ts.Thread == obj);
    FAssert(ThreadP(ts.Thread));

    AsThread(ts.Thread)->Parameters = NoValueObject;
    AsThread(ts.Thread)->IndexParameters = NoValueObject;

    try
    {
        if (ProcedureP(AsThread(ts.Thread)->Thunk))
        {
//            AsThread(ts.Thread)->Result = ExecuteThunk(AsThread(ts.Thread)->Thunk);
            Modify(FThread, ts.Thread, Result, ExecuteThunk(AsThread(ts.Thread)->Thunk));
        }
        else
        {
            FAssert(PrimitiveP(AsThread(ts.Thread)->Thunk));

//            AsThread(ts.Thread)->Result =
//                    AsPrimitive(AsThread(ts.Thread)->Thunk)->PrimitiveFn(0, 0);
            Modify(FThread, ts.Thread, Result,
                    AsPrimitive(AsThread(ts.Thread)->Thunk)->PrimitiveFn(0, 0));
        }
    }
    catch (FObject exc)
    {
        Write(R.StandardOutput, exc, 0);

//        AsThread(ts.Thread)->Result = exc;
        Modify(FThread, ts.Thread, Result, exc);
    }

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

Define("run-thread", RunThreadPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("run-thread", argc);
    ProcedureArgCheck("run-thread", argv[0]);

    FObject thrd = MakeThread(0, argv[0], CurrentParameters(), CurrentIndexParameters());

#ifdef FOMENT_WINDOWS
    HANDLE h = CreateThread(0, 0, StartThread, thrd, CREATE_SUSPENDED, 0);
    if (h == 0)
    {
        unsigned int ec = GetLastError();
        RaiseExceptionC(R.Assertion, "run-thread", "CreateThread failed", List(MakeFixnum(ec)));
    }

    EnterExclusive(&GCExclusive);
    TotalThreads += 1;
    LeaveExclusive(&GCExclusive);

    AsThread(thrd)->Handle = h;
    ResumeThread(h);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    EnterExclusive(&GCExclusive);
    TotalThreads += 1;
    LeaveExclusive(&GCExclusive);

    pthread_t pt;
    int ret = pthread_create(&pt, 0, StartThread, thrd);
    if (ret != 0)
    {
        EnterExclusive(&GCExclusive);
        TotalThreads -= 1;
        LeaveExclusive(&GCExclusive);

        RaiseExceptionC(R.Assertion, "run-thread", "pthread_create failed", List(MakeFixnum(ret)));
    }
#endif // FOMENT_UNIX

    return(thrd);
}

void ThreadExit(FObject obj)
{
    FThreadState * ts = GetThreadState();

//    AsThread(ts->Thread)->Result = obj;
    Modify(FThread, ts->Thread, Result, obj);

    if (LeaveThread(ts) == 0)
    {
        ExitFoment();

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

Define("%exit-thread", ExitThreadPrimitive)(int_t argc, FObject argv[])
{
    FMustBe(argc == 1);

    ThreadExit(argv[0]);
    return(NoValueObject);
}

Define("%exit", ExitPrimitive)(int_t argc, FObject argv[])
{
    ZeroOrOneArgsCheck("exit", argc);

    ExitFoment();

    if (argc == 0 || argv[0] == TrueObject)
        exit(0);

    if (FixnumP(argv[0]))
        exit((int) AsFixnum(argv[0]));

    exit(-1);

    return(NoValueObject);
}

Define("sleep", SleepPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("sleep", argc);
    NonNegativeArgCheck("sleep", argv[0], 0);

#ifdef FOMENT_WINDOWS
    EnterWait();
    Sleep((DWORD) AsFixnum(argv[0]));
    LeaveWait();
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    EnterWait();
    useconds_t us = AsFixnum(argv[0]);
    usleep(us * 1000);
    LeaveWait();
#endif // FOMENT_UNIX

    return(NoValueObject);
}

Define("exclusive?", ExclusivePPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("exclusive?", argc);

    return(ExclusiveP(argv[0]) ? TrueObject : FalseObject);
}

Define("make-exclusive", MakeExclusivePrimitive)(int_t argc, FObject argv[])
{
    ZeroArgsCheck("make-exclusive", argc);

    return(MakeExclusive());
}

Define("enter-exclusive", EnterExclusivePrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("enter-exclusive", argc);
    ExclusiveArgCheck("enter-exclusive", argv[0]);

    EnterWait();
    EnterExclusive(&AsExclusive(argv[0])->Exclusive);
    LeaveWait();
    return(NoValueObject);
}

Define("leave-exclusive", LeaveExclusivePrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("leave-exclusive", argc);
    ExclusiveArgCheck("leave-exclusive", argv[0]);

    LeaveExclusive(&AsExclusive(argv[0])->Exclusive);
    return(NoValueObject);
}

Define("try-exclusive", TryExclusivePrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("try-exclusive", argc);
    ExclusiveArgCheck("try-exclusive", argv[0]);

    return(TryExclusive(&AsExclusive(argv[0])->Exclusive) ? TrueObject : FalseObject);
}

Define("condition?", ConditionPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("condition?", argc);

    return(ConditionP(argv[0]) ? TrueObject : FalseObject);
}

Define("make-condition", MakeConditionPrimitive)(int_t argc, FObject argv[])
{
    ZeroArgsCheck("make-condition", argc);

    return(MakeCondition());
}

Define("condition-wait", ConditionWaitPrimitive)(int_t argc, FObject argv[])
{
    TwoArgsCheck("condition-wait", argc);
    ConditionArgCheck("condition-wait", argv[0]);
    ExclusiveArgCheck("condition-wait", argv[1]);

    EnterWait();
    ConditionWait(&AsCondition(argv[0])->Condition, &AsExclusive(argv[1])->Exclusive);
    LeaveWait();
    return(NoValueObject);
}

Define("condition-wake", ConditionWakePrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("condition-wake", argc);
    ConditionArgCheck("condition-wake", argv[0]);

    WakeCondition(&AsCondition(argv[0])->Condition);
    return(NoValueObject);
}

Define("condition-wake-all", ConditionWakeAllPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("condition-wake-all", argc);
    ConditionArgCheck("condition-wake-all", argv[0]);

    WakeAllCondition(&AsCondition(argv[0])->Condition);
    return(NoValueObject);
}

static void InterruptThread(FObject thrd);
void NotifyThread(FObject thrd, FObject obj)
{
    EnterExclusive(&GCExclusive);

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

    LeaveExclusive(&GCExclusive);
}

void NotifyBroadcast(FObject obj)
{
    EnterExclusive(&GCExclusive);

    FThreadState * ts = Threads;
    while (ts != 0)
    {
        ts->NotifyFlag = 1;
        ts->NotifyObject = obj;
        InterruptThread(ts->Thread);

        ts = ts->Next;
    }

    LeaveExclusive(&GCExclusive);
}

#define NOTIFY_EXIT 0
#define NOTIFY_IGNORE 1
#define NOTIFY_BROADCAST 2

static int_t SigIntNotify;
static int_t SigIntCount;
static time_t SigIntTime;

Define("set-ctrl-c-notify!", SetCtrlCNotifyPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("set-ctrl-c-notify!", argc);

    if (argv[0] == StringCToSymbol("exit"))
        SigIntNotify = NOTIFY_EXIT;
    else if (argv[0] == StringCToSymbol("ignore"))
        SigIntNotify = NOTIFY_IGNORE;
    else if (argv[0] == StringCToSymbol("broadcast"))
        SigIntNotify = NOTIFY_BROADCAST;
    else
        RaiseExceptionC(R.Assertion, "set-ctrl-c-notify!", "expected exit, ignore, or broadcast",
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

        if (now - SigIntTime < 2)
        {
            SigIntCount += 1;

            if (SigIntCount > 2)
                exit(-1);
        }
        else
        {
            SigIntCount = 1;
            SigIntTime = now;
        }

        NotifyBroadcast(R.SigIntSymbol);
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

static FPrimitive * Primitives[] =
{
    &CurrentThreadPrimitive,
    &ThreadPPrimitive,
    &RunThreadPrimitive,
    &ExitThreadPrimitive,
    &ExitPrimitive,
    &SleepPrimitive,
    &ExclusivePPrimitive,
    &MakeExclusivePrimitive,
    &EnterExclusivePrimitive,
    &LeaveExclusivePrimitive,
    &TryExclusivePrimitive,
    &ConditionPPrimitive,
    &MakeConditionPrimitive,
    &ConditionWaitPrimitive,
    &ConditionWakePrimitive,
    &ConditionWakeAllPrimitive,
    &SetCtrlCNotifyPrimitive
};

void SetupThreads()
{
    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);

    SigIntNotify = NOTIFY_EXIT;
    SigIntCount = 0;
    SigIntTime = time(0);

    SetupSignals();
}
