/*

Foment

*/

#ifndef __IO_HPP__
#define __IO_HPP__

// ---- Binary Ports ----

typedef uint_t (*FReadBytesFn)(FObject port, void * b, uint_t bl);
typedef int_t (*FByteReadyPFn)(FObject port);
typedef void (*FWriteBytesFn)(FObject port, void * b, uint_t bl);

typedef struct
{
    FGenericPort Generic;
    FReadBytesFn ReadBytesFn;
    FByteReadyPFn ByteReadyPFn;
    FWriteBytesFn WriteBytesFn;
    uint_t PeekedByte;
    uint_t Offset;
} FBinaryPort;

#define AsBinaryPort(obj) ((FBinaryPort *) obj)

FObject MakeBinaryPort(FObject nam, FObject obj, void * ictx, void * octx, FCloseInputFn cifn,
    FCloseOutputFn cofn, FFlushOutputFn fofn, FReadBytesFn rbfn, FByteReadyPFn brpfn,
    FWriteBytesFn wbfn);

// ---- Textual Ports ----

typedef uint_t (*FReadChFn)(FObject port, FCh * ch);
typedef int_t (*FCharReadyPFn)(FObject port);
typedef void (*FWriteStringFn)(FObject port, FCh * s, uint_t sl);

typedef struct
{
    FGenericPort Generic;
    FReadChFn ReadChFn;
    FCharReadyPFn CharReadyPFn;
    FWriteStringFn WriteStringFn;
    uint_t PeekedChar;
    uint_t Line;
    uint_t Column;
} FTextualPort;

#define AsTextualPort(obj) ((FTextualPort *) obj)

FObject MakeTextualPort(FObject nam, FObject obj, void * ictx, void * octx, FCloseInputFn cifn,
    FCloseOutputFn cofn, FFlushOutputFn fofn, FReadChFn rcfn, FCharReadyPFn crpfn,
    FWriteStringFn wsfn);

inline FObject CurrentInputPort()
{
    FAssert(PairP(IndexParameter(0)));

    FObject port = First(IndexParameter(0));

    FAssert(InputPortP(port) && InputPortOpenP(port));

    return(port);
}

inline FObject CurrentOutputPort()
{
    FAssert(PairP(IndexParameter(1)));

    FObject port = First(IndexParameter(1));

    FAssert(OutputPortP(port) && OutputPortOpenP(port));

    return(port);
}

// ----------------

void SetupWrite();
void SetupRead();
int_t IdentifierSubsequentP(FCh ch);

#endif // __IO_HPP__
