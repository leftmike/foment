/*

Foment

-- use Win32 console APIs
-- use Win32 file APIs and not stdio

-- MakeUtf8Port(FObject port): for utf8 characters
-- MakeUtf16Port(FObject port): and utf16 characters

-- GetLocation
-- ports optionally return or seek to a location
-- ports describe whether they use char offset, byte offset, or line number for location

-- need a port guardian to make sure ports are closed

-- Alive to keep track of which are live objects

-- get rid of Pretty or clean it up and call it pp

-- read
-- write

-- test all input and output procedures
-- document additional io procedures

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

void SetupWrite();
void SetupRead();

#endif // __IO_HPP__
