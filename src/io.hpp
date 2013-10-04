/*

Foment

-- StdioByteReadyP

-- MakeUtf8Port(FObject port): for utf8 characters
-- MakeUtf16Port(FObject port): and utf16 characters

-- GetLocation
-- ports optionally return or seek to a location
-- ports describe whether they use char offset, byte offset, or line number for location

*/

#ifndef __IO_HPP__
#define __IO_HPP__

// ---- Binary Ports ----

typedef unsigned int (*FReadBytesFn)(FObject port, void * b, unsigned int bl);
typedef int (*FByteReadyPFn)(FObject port);
typedef void (*FWriteBytesFn)(FObject port, void * b, unsigned int bl);

typedef struct
{
    FGenericPort Generic;
    FReadBytesFn ReadBytesFn;
    FByteReadyPFn ByteReadyPFn;
    FWriteBytesFn WriteBytesFn;
    unsigned int PeekedByte;
} FBinaryPort;

#define AsBinaryPort(obj) ((FBinaryPort *) obj)

FObject MakeBinaryPort(FObject nam, FObject obj, void * ictx, void * octx, FCloseInputFn cifn,
    FCloseOutputFn cofn, FFlushOutputFn fofn, FReadBytesFn rbfn, FByteReadyPFn brpfn,
    FWriteBytesFn wbfn);

// ---- Textual Ports ----

typedef unsigned int (*FReadChFn)(FObject port, FCh * ch);
typedef int (*FCharReadyPFn)(FObject port);
typedef void (*FWriteStringFn)(FObject port, FCh * s, unsigned int sl);

typedef struct
{
    FGenericPort Generic;
    FReadChFn ReadChFn;
    FCharReadyPFn CharReadyPFn;
    FWriteStringFn WriteStringFn;
    FCh PeekedChar;
} FTextualPort;

#define AsTextualPort(obj) ((FTextualPort *) obj)

FObject MakeTextualPort(FObject nam, FObject obj, void * ictx, void * octx, FCloseInputFn cifn,
    FCloseOutputFn cofn, FFlushOutputFn fofn, FReadChFn rcfn, FCharReadyPFn crpfn,
    FWriteStringFn wsfn);

// Write

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

#endif // __IO_HPP__
