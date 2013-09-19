/*

Foment

*/

#include <windows.h>
#include "foment.hpp"
#include "execute.hpp"
#include "syncthrd.hpp"

// ---- Threads ----

FObject MakeThread(OSThreadHandle h, FObject thnk, FObject prms)
{
    FThread * thrd = (FThread *) MakeObject(sizeof(FThread), ThreadTag);
    thrd->Reserved = MakeLength(0, ThreadTag);
    thrd->Result = NoValueObject;
    thrd->Handle = h;
    thrd->Thunk = thnk;
    thrd->Parameters = prms;

    return(thrd);
}

void WriteThread(FObject port, FObject obj, int df)
{
    FAssert(ThreadP(obj));

    PutStringC(port, "#<thread: ");

    FCh s[16];
    int sl = NumberAsString((FFixnum) AsThread(obj)->Handle, s, 16);
    PutString(port, s, sl);
    PutCh(port, '>');
}

// ---- Exclusives ----

static FObject MakeExclusive()
{
    FExclusive * e = (FExclusive *) MakePinnedObject(sizeof(FExclusive), "make-exclusive");
    e->Reserved = MakePinnedLength(0, ExclusiveTag);

    InitializeExclusive(&e->Exclusive);

    InstallGuardian(e, R.ExclusivesTConc);
    return(e);
}

void WriteExclusive(FObject port, FObject obj, int df)
{
    FAssert(ExclusiveP(obj));

    PutStringC(port, "#<exclusive: ");

    FCh s[16];
    int sl = NumberAsString((FFixnum) &AsExclusive(obj)->Exclusive, s, 16);
    PutString(port, s, sl);
    PutCh(port, '>');
}

// ---- Conditions ----

static FObject MakeCondition()
{
    FCondition * c = (FCondition *) MakePinnedObject(sizeof(FCondition), "make-condition");
    c->Reserved = MakePinnedLength(0, ConditionTag);

    InitializeCondition(&c->Condition);
    return(c);
}

void WriteCondition(FObject port, FObject obj, int df)
{
    FAssert(ConditionP(obj));

    PutStringC(port, "#<condition: ");

    FCh s[16];
    int sl = NumberAsString((FFixnum) &AsCondition(obj)->Condition, s, 16);
    PutString(port, s, sl);
    PutCh(port, '>');
}

Define("current-thread", CurrentThreadPrimitive)(int argc, FObject argv[])
{
    if (argc != 0)
        RaiseExceptionC(R.Assertion, "current-thread", "expected zero arguments", EmptyListObject);

    return(GetThreadState()->Thread);
}

Define("thread?", ThreadPPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "thread?", "expected one argument", EmptyListObject);

    return(ThreadP(argv[0]) ? TrueObject : FalseObject);
}

#ifdef FOMENT_WIN32
static DWORD WINAPI FomentThread(FObject obj)
{
    FThreadState ts;

    FAssert(ThreadP(obj));

    EnterThread(&ts, obj, AsThread(obj)->Parameters);

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
        AsThread(obj)->Result = exc;
    }

    LeaveThread(&ts);
    return(0);
}
#endif // FOMENT_WIN32

Define("run-thread", RunThreadPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "run-thread", "expected one argument", EmptyListObject);

    if (ProcedureP(argv[0]) == 0 && PrimitiveP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "run-thread", "expected a procedure or a primitive",
                List(argv[0]));

    FObject thrd = MakeThread(0, argv[0], CurrentParameters());

#ifdef FOMENT_WIN32
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
#endif // FOMENT_WIN32

    return(thrd);
}

Define("sleep", SleepPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "sleep", "expected one argument", EmptyListObject);

    if (FixnumP(argv[0]) == 0 || AsFixnum(argv[0]) < 0)
        RaiseExceptionC(R.Assertion, "sleep", "expected a non-negative integer", List(argv[0]));

    EnterWait();
    Sleep(AsFixnum(argv[0]));
    LeaveWait();
    return(NoValueObject);
}

Define("exclusive?", ExclusivePPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "exclusive?", "expected one argument", EmptyListObject);

    return(ExclusiveP(argv[0]) ? TrueObject : FalseObject);
}

Define("make-exclusive", MakeExclusivePrimitive)(int argc, FObject argv[])
{
    if (argc != 0)
        RaiseExceptionC(R.Assertion, "make-exclusive", "expected zero arguments", EmptyListObject);

    return(MakeExclusive());
}

Define("enter-exclusive", EnterExclusivePrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "enter-exclusive", "expected one argument", EmptyListObject);

    if (ExclusiveP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "enter-exclusive", "expected an exclusive", List(argv[0]));

    EnterWait();
    EnterExclusive(&AsExclusive(argv[0])->Exclusive);
    LeaveWait();
    return(NoValueObject);
}

Define("leave-exclusive", LeaveExclusivePrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "leave-exclusive", "expected one argument", EmptyListObject);

    if (ExclusiveP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "leave-exclusive", "expected an exclusive", List(argv[0]));

    LeaveExclusive(&AsExclusive(argv[0])->Exclusive);
    return(NoValueObject);
}

Define("try-exclusive", TryExclusivePrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "try-exclusive", "expected one argument", EmptyListObject);

    if (ExclusiveP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "try-exclusive", "expected an exclusive", List(argv[0]));

    return(TryExclusive(&AsExclusive(argv[0])->Exclusive) ? TrueObject : FalseObject);
}

Define("condition?", ConditionPPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "condition?", "expected one argument", EmptyListObject);

    return(ConditionP(argv[0]) ? TrueObject : FalseObject);
}

Define("make-condition", MakeConditionPrimitive)(int argc, FObject argv[])
{
    if (argc != 0)
        RaiseExceptionC(R.Assertion, "make-condition", "expected zero arguments", EmptyListObject);

    return(MakeCondition());
}

Define("condition-wait", ConditionWaitPrimitive)(int argc, FObject argv[])
{
    if (argc != 2)
        RaiseExceptionC(R.Assertion, "condition-wait", "expected two arguments", EmptyListObject);

    if (ConditionP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "condition-wait", "expected a condition", List(argv[0]));

    if (ExclusiveP(argv[1]) == 0)
        RaiseExceptionC(R.Assertion, "condition-wait", "expected an exclusive", List(argv[1]));

    EnterWait();
    ConditionWait(&AsCondition(argv[0])->Condition, &AsExclusive(argv[1])->Exclusive);
    LeaveWait();
    return(NoValueObject);
}

Define("condition-wake", ConditionWakePrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "condition-wake", "expected one argument", EmptyListObject);

    if (ConditionP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "condition-wake", "expected a condition", List(argv[0]));

    WakeCondition(&AsCondition(argv[0])->Condition);
    return(NoValueObject);
}

Define("condition-wake-all", ConditionWakeAllPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "condition-wake-all", "expected one argument",
                EmptyListObject);

    if (ConditionP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "condition-wake-all", "expected a condition", List(argv[0]));

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

    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}

/*
Unix Support:

http://en.wikipedia.org/wiki/Critical_section
http://thompsonng.blogspot.com/2011/06/critical-section-windows-vs-linux-in-c.html
*/
