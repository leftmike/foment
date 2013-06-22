/*

Foment

*/

#ifndef __PORT_HPP__
#define __PORT_HPP__

// ---- Ports ----

typedef FCh (*FGetChFn)(void * ctx, FObject obj, int * eof);
typedef FCh (*FPeekChFn)(void * ctx, FObject obj, int * eof);

typedef struct
{
    FGetChFn GetChFn;
    FPeekChFn PeekChFn;
} FInputPort;

typedef void (*FPutChFn)(void * ctx, FObject obj, FCh ch);
typedef void (*FPutStringFn)(void * ctx, FObject obj, FCh * s, int sl);
typedef void (*FPutStringCFn)(void * ctx, FObject obj, char * s);

typedef struct
{
    FPutChFn PutChFn;
    FPutStringFn PutStringFn;
    FPutStringCFn PutStringCFn;
} FOutputPort;

typedef int (*FGetLocationFn)(void * ctx, FObject obj);
typedef void (*FSetLocationFn)(void * ctx, FObject obj, int loc);

typedef struct
{
    FGetLocationFn GetLocationFn;
    FSetLocationFn SetLocationFn;
} FPortLocation;

typedef void (*FCloseFn)(void * ctx, FObject obj);

typedef struct
{
    unsigned int Reserved;
    FObject Name;
    FInputPort * Input;
    FOutputPort * Output;
    FPortLocation * Location;
    FCloseFn CloseFn;
    void * Context;
    FObject Object;
} FPort;

#define AsPort(obj) ((FPort *) obj)

FObject MakePort(FObject nam, FInputPort * inp, FOutputPort * outp, FPortLocation * loc,
    FCloseFn cfn, void * ctx, FObject obj);

inline int SharedObjectP(FObject obj)
{
    return(PairP(obj) || BoxP(obj) || VectorP(obj) || ProcedureP(obj) || GenericRecordP(obj));
}

unsigned int FindSharedObjects(FObject ht, FObject obj, unsigned int cnt, int cof);
FObject WalkUpdate(FObject key, FObject val, FObject ctx);
int WalkDelete(FObject key, FObject val, FObject ctx);

typedef void (*FWriteFn)(FObject port, FObject obj, int df, void * wfn, void * ctx);
void WriteGeneric(FObject port, FObject obj, int df, FWriteFn wfn, void * ctx);

typedef struct
{
    FObject Hashtable;
    int Label;
} FWriteSharedCtx;

#define ToWriteSharedCtx(ctx) ((FWriteSharedCtx *) (ctx))
void WriteSharedObject(FObject port, FObject obj, int df, FWriteFn wfn, void * ctx);

void SetupPrettyPrint();

#endif // __PORT_HPP__
