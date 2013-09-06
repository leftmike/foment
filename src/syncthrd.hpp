/*

Foment

*/

#ifndef __SYNCTHRD_HPP__
#define __SYNCTHRD_HPP__

#ifdef FOMENT_WIN32

// ---- Operating System Thread ----

typedef HANDLE OSThreadHandle;

// ---- Operating System Exclusive ----

typedef CRITICAL_SECTION OSExclusive;

inline void InitializeExclusive(OSExclusive * ose)
{
    InitializeCriticalSection(ose);
}

inline void EnterExclusive(OSExclusive * ose)
{
    EnterCriticalSection(ose);
}

inline void LeaveExclusive(OSExclusive * ose)
{
    LeaveCriticalSection(ose);
}

inline int TryExclusive(OSExclusive * ose)
{
    return(TryEnterCriticalSection(ose));
}

inline void DeleteExclusive(OSExclusive * ose)
{
    DeleteCriticalSection(ose);
}

// ---- Operating System Condition ----

typedef CONDITION_VARIABLE OSCondition;

inline void InitializeCondition(OSCondition * osc)
{
    InitializeConditionVariable(osc);
}

inline void ConditionWait(OSCondition * osc, OSExclusive * ose)
{
    SleepConditionVariableCS(osc, ose, INFINITE);
}

inline void WakeCondition(OSCondition * osc)
{
    WakeConditionVariable(osc);
}

inline void WakeAllCondition(OSCondition * osc)
{
    WakeAllConditionVariable(osc);
}

// ---- Operating System Event ----

typedef HANDLE OSEvent;

inline OSEvent CreateEvent()
{
    return(CreateEvent(0, TRUE, 0, 0));
}

inline void SignalEvent(OSEvent ose)
{
    SetEvent(ose);
}

inline void ClearEvent(OSEvent ose)
{
    ResetEvent(ose);
}

inline void WaitEvent(OSEvent ose)
{
    WaitForSingleObject(ose, INFINITE);
}

inline void DeleteEvent(OSEvent ose)
{
    CloseHandle(ose);
}

#endif // FOMENT_WIN32

// ---- Threads ----

#define ThreadP(obj) (IndirectTag(obj) == ThreadTag)
#define AsThread(obj) ((FThread *) (obj))

typedef struct _FThread
{
    unsigned int Reserved;
    OSThreadHandle Handle;
    FObject Result;
    FObject Thunk;
    FObject Parameters;
} FThread;

FObject MakeThread(OSThreadHandle h, FObject thnk, FObject prms);

// ---- Exclusives ----

#define ExclusiveP(obj) (IndirectTag(obj) == ExclusiveTag)
#define AsExclusive(obj) ((FExclusive *) (obj))

typedef struct
{
    unsigned int Reserved;
    OSExclusive Exclusive;
} FExclusive;

// ---- Conditions ----

#define ConditionP(obj) (IndirectTag(obj) == ConditionTag)
#define AsCondition(obj) ((FCondition *) (obj))

typedef struct
{
    unsigned int Reserved;
    OSCondition Condition;
} FCondition;

// ----------------

#ifdef FOMENT_WIN32
extern unsigned int TlsIndex;

inline FThreadState * GetThreadState()
{
    FAssert(TlsGetValue(TlsIndex) != 0);

    return((FThreadState *) TlsGetValue(TlsIndex));
}

inline void SetThreadState(FThreadState * ts)
{
    TlsSetValue(TlsIndex, ts);
}
#endif // FOMENT_WIN32

extern unsigned int TotalThreads;
extern unsigned int WaitThreads;
extern OSExclusive GCExclusive;

void EnterThread(FThreadState * ts, FObject thrd, FObject prms);
void LeaveThread(FThreadState * ts);

#endif // __SYNCTHRD_HPP__
