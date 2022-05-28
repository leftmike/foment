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

ulong_t ConditionWaitTimeout(OSCondition * osc, OSExclusive * ose, FObject to);

// ---- Threads ----

#define AsThread(obj) ((FThread *) (obj))

#define THREAD_STATE_NEW 0
#define THREAD_STATE_READY 1
#define THREAD_STATE_RUNNING 2
#define THREAD_STATE_DONE 3

#define THREAD_EXIT_NORMAL 0
#define THREAD_EXIT_TERMINATED 1
#define THREAD_EXIT_UNCAUGHT 2

typedef struct
{
    FObject Result;
    FObject Thunk;
    FObject Name;
    FObject Specific;
    OSThreadHandle Handle;
    OSExclusive Exclusive;
    OSCondition Condition;
    ulong_t State;
    ulong_t Exit;
} FThread;

FThread * MakeThread(OSThreadHandle h, FObject thnk);
void DeleteThread(FObject thrd);
void ThreadExit(FObject obj, ulong_t exit);

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

// ---- Time ----

#define AsTime(obj) ((FTime *) (obj))
#define TimeP(obj) (ObjectTag(obj) == TimeTag)

typedef struct
{
#ifdef FOMENT_WINDOWS
    FILETIME filetime;
#else // FOMENT_WINDOWS
    struct timespec timespec;
#endif // FOMENT_WINDOWS
} FTime;

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

extern FThreadState * Threads;
extern OSExclusive ThreadsExclusive;

long_t EnterThread(FThreadState * ts, FObject thrd, FObject prms);
ulong_t LeaveThread(FThreadState * ts);

inline FObject Parameter(ulong_t idx)
{
    FThreadState * ts = GetThreadState();

    FAssert(idx < ts->ParametersLength);
    FAssert(BoxP(ts->Parameters[idx]));

    return(Unbox(ts->Parameters[idx]));
}

typedef struct
{
    long_t Unused;
} FNotifyThrow;

class FWithExclusive
{
public:

    FWithExclusive(FObject exc);
    FWithExclusive(OSExclusive * ose);
    ~FWithExclusive();

private:

    OSExclusive * Exclusive;
};

#endif // __SYNCTHRD_HPP__
