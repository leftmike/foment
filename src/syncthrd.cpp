/*

Foment

*/

#include <windows.h>
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
    int_t sl = NumberAsString((FFixnum) AsThread(obj)->Handle, s, 16);
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

    InstallGuardian(e, R.ExclusivesTConc);
    return(e);
}

void WriteExclusive(FObject port, FObject obj, int_t df)
{
    FAssert(ExclusiveP(obj));

    WriteStringC(port, "#<exclusive: ");

    FCh s[16];
    int_t sl = NumberAsString((FFixnum) &AsExclusive(obj)->Exclusive, s, 16);
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
    int_t sl = NumberAsString((FFixnum) &AsCondition(obj)->Condition, s, 16);
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

        AsVector(v)->Vector[idx] = MakePair(First(GetThreadState()->IndexParameters[idx]),
                EmptyListObject);
    }

    return(v);
}

#ifdef FOMENT_WINDOWS
static DWORD WINAPI FomentThread(FObject obj)
{
    FThreadState ts;

    FAssert(ThreadP(obj));

    EnterThread(&ts, obj, AsThread(obj)->Parameters, AsThread(obj)->IndexParameters);
    AsThread(obj)->Parameters = NoValueObject;
    AsThread(obj)->IndexParameters = NoValueObject;

    try
    {
        if (ProcedureP(AsThread(obj)->Thunk))
            AsThread(obj)->Result = ExecuteThunk(AsThread(obj)->Thunk);
        else
        {
            FAssert(PrimitiveP(AsThread(obj)->Thunk));

            AsThread(obj)->Result = AsPrimitive(AsThread(obj)->Thunk)->PrimitiveFn(0, 0);
        }
    }
    catch (FObject exc)
    {
        Write(R.StandardOutput, exc, 0);

        AsThread(obj)->Result = exc;
    }

    LeaveThread(&ts);
    return(0);
}
#endif // FOMENT_WINDOWS

Define("run-thread", RunThreadPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("run-thread", argc);
    ProcedureArgCheck("run-thread", argv[0]);

    FObject thrd = MakeThread(0, argv[0], CurrentParameters(), CurrentIndexParameters());

#ifdef FOMENT_WINDOWS
    HANDLE h = CreateThread(0, 0, FomentThread, thrd, CREATE_SUSPENDED, 0);
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

    return(thrd);
}

Define("sleep", SleepPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("sleep", argc);
    NonNegativeArgCheck("sleep", argv[0]);

    EnterWait();
    Sleep((DWORD) AsFixnum(argv[0]));
    LeaveWait();
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

static FPrimitive * Primitives[] =
{
    &CurrentThreadPrimitive,
    &ThreadPPrimitive,
    &RunThreadPrimitive,
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
    &ConditionWakeAllPrimitive
};

void SetupThreads()
{
    R.ExclusivesTConc = MakeTConc();

    for (int_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}

/*
Unix Support:

http://en.wikipedia.org/wiki/Critical_section
http://thompsonng.blogspot.com/2011/06/critical-section-windows-vs-linux-in-c.html
*/
