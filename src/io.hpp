/*

Foment

*/

#ifndef __IO_HPP__
#define __IO_HPP__

// ---- Binary Ports ----

typedef ulong_t (*FReadBytesFn)(FObject port, void * b, ulong_t bl);
typedef long_t (*FByteReadyPFn)(FObject port);
typedef void (*FWriteBytesFn)(FObject port, void * b, ulong_t bl);

typedef struct
{
    FGenericPort Generic;
    FReadBytesFn ReadBytesFn;
    FByteReadyPFn ByteReadyPFn;
    FWriteBytesFn WriteBytesFn;
    ulong_t PeekedByte;
    ulong_t Offset;
} FBinaryPort;

#define AsBinaryPort(obj) ((FBinaryPort *) obj)

FObject MakeBinaryPort(FObject nam, FObject obj, void * ctx, FCloseInputFn cifn,
    FCloseOutputFn cofn, FFlushOutputFn fofn, FReadBytesFn rbfn, FByteReadyPFn brpfn,
    FWriteBytesFn wbfn, FGetPositionFn gpfn, FSetPositionFn spfn);

// ---- Textual Ports ----

typedef ulong_t (*FReadChFn)(FObject port, FCh * ch);
typedef long_t (*FCharReadyPFn)(FObject port);
typedef void (*FWriteStringFn)(FObject port, FCh * s, ulong_t sl);

typedef struct
{
    FGenericPort Generic;
    FReadChFn ReadChFn;
    FCharReadyPFn CharReadyPFn;
    FWriteStringFn WriteStringFn;
    ulong_t PeekedChar;
    ulong_t Line;
    ulong_t Column;
} FTextualPort;

#define AsTextualPort(obj) ((FTextualPort *) obj)

FObject MakeTextualPort(FObject nam, FObject obj, void * ctx, FCloseInputFn cifn,
    FCloseOutputFn cofn, FFlushOutputFn fofn, FReadChFn rcfn, FCharReadyPFn crpfn,
    FWriteStringFn wsfn, FGetPositionFn gpfn, FSetPositionFn spfn);

inline FObject CurrentInputPort()
{
    FAssert(PairP(IndexParameter(INDEX_PARAMETER_CURRENT_INPUT_PORT)));

    FObject port = First(IndexParameter(INDEX_PARAMETER_CURRENT_INPUT_PORT));

    FAssert(InputPortP(port) && InputPortOpenP(port));

    return(port);
}

inline FObject CurrentOutputPort()
{
    FAssert(PairP(IndexParameter(INDEX_PARAMETER_CURRENT_OUTPUT_PORT)));

    FObject port = First(IndexParameter(INDEX_PARAMETER_CURRENT_OUTPUT_PORT));

    FAssert(OutputPortP(port) && OutputPortOpenP(port));

    return(port);
}

// ----------------

void SetupWrite();
void SetupRead();
long_t IdentifierSubsequentP(FCh ch);

#endif // __IO_HPP__
