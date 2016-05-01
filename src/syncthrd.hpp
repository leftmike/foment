/*

Foment

*/

#ifndef __SYNCTHRD_HPP__
#define __SYNCTHRD_HPP__

#ifdef FOMENT_WINDOWS

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

inline void DeleteCondition(OSCondition * osc)
{
    // Nothing.
}

#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX

// ---- Operating System Thread ----

typedef pthread_t OSThreadHandle;

// ---- Operating System Exclusive ----

typedef pthread_mutex_t OSExclusive;

void InitializeExclusive(OSExclusive * ose);

inline void EnterExclusive(OSExclusive * ose)
{
    pthread_mutex_lock(ose);
}

inline void LeaveExclusive(OSExclusive * ose)
{
    pthread_mutex_unlock(ose);
}

inline int TryExclusive(OSExclusive * ose)
{
    return(pthread_mutex_trylock(ose) == 0);
}

inline void DeleteExclusive(OSExclusive * ose)
{
    pthread_mutex_destroy(ose);
}

// ---- Operating System Condition ----

typedef pthread_cond_t OSCondition;

inline void InitializeCondition(OSCondition * osc)
{
    pthread_cond_init(osc, 0);
}

inline void ConditionWait(OSCondition * osc, OSExclusive * ose)
{
    pthread_cond_wait(osc, ose);
}

inline void WakeCondition(OSCondition * osc)
{
    pthread_cond_signal(osc);
}

inline void WakeAllCondition(OSCondition * osc)
{
    pthread_cond_broadcast(osc);
}

inline void DeleteCondition(OSCondition * osc)
{
    pthread_cond_destroy(osc);
}

#endif // FOMENT_UNIX

// ---- Threads ----

#define AsThread(obj) ((FThread *) (obj))

typedef struct
{
    FObject Result;
    FObject Thunk;
    FObject Parameters;
    FObject IndexParameters;
    OSThreadHandle Handle;
} FThread;

FObject MakeThread(OSThreadHandle h, FObject thnk, FObject prms, FObject idxprms);
void ThreadExit(FObject obj);

// ---- Exclusives ----

#define AsExclusive(obj) ((FExclusive *) (obj))

typedef struct
{
    OSExclusive Exclusive;
} FExclusive;

// ---- Conditions ----

#define AsCondition(obj) ((FCondition *) (obj))

typedef struct
{
    OSCondition Condition;
} FCondition;

// ----------------

#ifdef FOMENT_WINDOWS
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
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
extern pthread_key_t ThreadKey;

inline FThreadState * GetThreadState()
{
    FAssert(pthread_getspecific(ThreadKey) != 0);

    return((FThreadState *) pthread_getspecific(ThreadKey));
}

inline void SetThreadState(FThreadState * ts)
{
    pthread_setspecific(ThreadKey, ts);
}
#endif // FOMENT_UNIX

extern volatile uint_t TotalThreads;
extern FThreadState * Threads;
extern OSExclusive GCExclusive;

int_t EnterThread(FThreadState * ts, FObject thrd, FObject prms, FObject idxprms);
uint_t LeaveThread(FThreadState * ts);

inline FObject IndexParameter(uint_t idx)
{
    FAssert(idx < INDEX_PARAMETERS);

    return(GetThreadState()->IndexParameters[idx]);
}

typedef struct
{
    int_t Unused;
} FNotifyThrow;

#endif // __SYNCTHRD_HPP__
