/*

Foment

*/

#ifdef FOMENT_WINDOWS
#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <io.h>
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#endif // FOMENT_UNIX

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "foment.hpp"
#if defined(FOMENT_BSD) || defined(FOMENT_OSX)
#include <stdlib.h>
#include <netinet/in.h>
#else
#include <malloc.h>
#endif
#include "syncthrd.hpp"
#include "io.hpp"
#include "unicode.hpp"

#ifdef FOMENT_UNIX
typedef long_t SOCKET;
#define closesocket(s) close(s)
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
#define addrinfoW addrinfo
#endif // FOMENT_UNIX

#define NOT_PEEKED ((ulong_t) (-1))
#define CR 13
#define LF 10

EternalSymbol(FileErrorSymbol, "file-error");
EternalSymbol(PositionErrorSymbol, "position-error");
EternalSymbol(DecodingErrorSymbol, "decoding-error");
EternalSymbol(EncodingErrorSymbol, "encoding-error");
EternalSymbol(CurrentSymbol, "current");
EternalSymbol(EndSymbol, "end");

// ---- Roots ----

FObject StandardInput = NoValueObject;
FObject StandardOutput = NoValueObject;
FObject StandardError = NoValueObject;

// ---- Binary Ports ----

static FFileHandle InvalidFileHandle(FObject port)
{
    return(INVALID_FILE_HANDLE);
}

static FFileHandle GetObjectFileHandle(FObject port)
{
    FAssert(BinaryPortP(port) || TextualPortP(port));

    return(GetFileHandle(AsGenericPort(port)->Object));
}

FObject MakeBinaryPort(FObject nam, FObject obj, void * ctx, FCloseInputFn cifn,
    FCloseOutputFn cofn, FFlushOutputFn fofn, FReadBytesFn rbfn, FByteReadyPFn brpfn,
    FWriteBytesFn wbfn, FGetPositionFn gpfn, FSetPositionFn spfn,
    FGetFileHandleFn gfhfn, ulong_t flgs)
{
    FAssert((cifn == 0 && rbfn == 0 && brpfn == 0) || (cifn != 0 && rbfn != 0 && brpfn != 0));
    FAssert((cofn == 0 && wbfn == 0 && fofn == 0) || (cofn != 0 && wbfn != 0 && fofn != 0));
    FAssert(cifn != 0 || cofn != 0);
    FAssert((gpfn == 0 && spfn == 0) || (gpfn != 0 && spfn != 0));

    FBinaryPort * port = (FBinaryPort *) MakeObject(BinaryPortTag, sizeof(FBinaryPort), 2,
            "%make-binary-port");
    port->Generic.Flags = (cifn != 0 ? (PORT_FLAG_INPUT | PORT_FLAG_INPUT_OPEN) : 0)
            | (cofn != 0 ? (PORT_FLAG_OUTPUT | PORT_FLAG_OUTPUT_OPEN) : 0)
            | (gpfn != 0 ? PORT_FLAG_POSITIONING : 0)
            | flgs;
    port->Generic.Name = nam;
    port->Generic.Object = obj;
    port->Generic.Context = ctx;
    port->Generic.CloseInputFn = cifn;
    port->Generic.CloseOutputFn = cofn;
    port->Generic.FlushOutputFn = fofn;
    port->Generic.GetPositionFn = gpfn;
    port->Generic.SetPositionFn = spfn;
    port->Generic.GetFileHandleFn = gfhfn == 0 ? InvalidFileHandle : gfhfn;
    port->ReadBytesFn = rbfn;
    port->ByteReadyPFn = brpfn;
    port->WriteBytesFn = wbfn;
    port->PeekedByte = NOT_PEEKED;
    port->Offset = 0;

    InstallGuardian(port, CleanupTConc);

    return(port);
}

ulong_t ReadBytes(FObject port, FByte * b, ulong_t bl)
{
    FAssert(BinaryPortP(port) && InputPortOpenP(port));
    FAssert(bl > 0);

    if (AsBinaryPort(port)->PeekedByte != NOT_PEEKED)
    {
        FAssert(AsBinaryPort(port)->PeekedByte >= 0 && AsBinaryPort(port)->PeekedByte <= 0xFF);

        *b = (FByte) AsBinaryPort(port)->PeekedByte;
        AsBinaryPort(port)->PeekedByte = NOT_PEEKED;

        if (bl == 1)
            return(1);

        return(AsBinaryPort(port)->ReadBytesFn(port, b + 1, bl - 1) + 1);
    }

    return(AsBinaryPort(port)->ReadBytesFn(port, b, bl));
}

long_t PeekByte(FObject port, FByte * b)
{
    FAssert(BinaryPortP(port) && InputPortOpenP(port));

    if (AsBinaryPort(port)->PeekedByte != NOT_PEEKED)
    {
        FAssert(AsBinaryPort(port)->PeekedByte >= 0 && AsBinaryPort(port)->PeekedByte <= 0xFF);

        *b = (FByte) AsBinaryPort(port)->PeekedByte;
        return(1);
    }

    FAlive ap(&port);

    if (PositioningPortP(port))
    {
        int64_t pos = GetPosition(port);

        if (AsBinaryPort(port)->ReadBytesFn(port, b, 1) == 0)
            return(0);

        SetPosition(port, pos, FromBegin);
    }
    else
    {
        if (AsBinaryPort(port)->ReadBytesFn(port, b, 1) == 0)
            return(0);

        AsBinaryPort(port)->PeekedByte = *b;
    }

    return(1);
}

long_t ByteReadyP(FObject port)
{
    FAssert(BinaryPortP(port) && InputPortOpenP(port));

    if (AsBinaryPort(port)->PeekedByte != NOT_PEEKED)
        return(1);

    return(AsBinaryPort(port)->ByteReadyPFn(port));
}

ulong_t GetOffset(FObject port)
{
    FAssert(BinaryPortP(port) && InputPortP(port));

    return(AsBinaryPort(port)->Offset);
}

void WriteBytes(FObject port, void * b, ulong_t bl)
{
    FAssert(BinaryPortP(port) && OutputPortOpenP(port));
    FAssert(bl >= 0);

    AsBinaryPort(port)->WriteBytesFn(port, b, bl);
}

typedef struct
{
    unsigned char * Buffer;
    ulong_t Available;
    ulong_t Used;
    ulong_t Maximum;
} FBufferedContext;

#define AsBufferedContext(port) ((FBufferedContext *) AsGenericPort(port)->Context)

static void FreeBufferedContext(FObject port)
{
    FBufferedContext * bc = AsBufferedContext(port);

    if (bc->Buffer != 0)
        free(bc->Buffer);
    free(bc);
}

static void BufferedCloseInput(FObject port)
{
    FAssert(BinaryPortP(port));

    if (OutputPortOpenP(port) == 0)
        FreeBufferedContext(port);

    CloseInput(AsGenericPort(port)->Object);
}

static void BufferedFlushOutput(FObject port)
{
    FAssert(BinaryPortP(port));

    FBufferedContext * bc = AsBufferedContext(port);

    if (bc->Used > 0)
    {
        WriteBytes(AsGenericPort(port)->Object, bc->Buffer, bc->Used);
        bc->Used = 0;

        FlushOutput(AsGenericPort(port)->Object);
    }
}

static void BufferedCloseOutput(FObject port)
{
    FAssert(BinaryPortP(port));

    BufferedFlushOutput(port);

    if (InputPortOpenP(port) == 0)
        FreeBufferedContext(port);

    CloseOutput(AsGenericPort(port)->Object);
}

static ulong_t BufferedReadBytes(FObject port, void * b, ulong_t bl)
{
    FAssert(BinaryPortP(port) && InputPortOpenP(port));
    FAssert(AsGenericPort(port)->Context != 0);

    FAlive ap(&port);
    FBufferedContext * bc = AsBufferedContext(port);
    ulong_t br = 0;
    unsigned char * ptr = (unsigned char *) b;

    while (br < bl)
    {
        if (bc->Used == bc->Available)
        {
            if (bl - br >= bc->Maximum)
            {
                br += ReadBytes(AsGenericPort(port)->Object, ptr + br, bl - br);
                return(br);
            }

            FAssert(bc->Buffer != 0);

            bc->Used = 0;
            bc->Available = ReadBytes(AsGenericPort(port)->Object, bc->Buffer, bc->Maximum);
            if (bc->Available == 0)
                return(br);
        }

        FAssert(bc->Used < bc->Available);

        ulong_t sz = bc->Available - bc->Used;
        if (sz <= (bl - br))
        {
            memcpy(ptr + br, bc->Buffer + bc->Used, sz);
            bc->Used += sz;
            br += sz;
        }
        else
        {
            memcpy(ptr + br, bc->Buffer + bc->Used, bl - br);
            bc->Used += (bl - br);
            br = bl;
        }
    }

    return(br);
}

static void * LookaheadBytes(FObject port, ulong_t * bl)
{
    FAssert(BinaryPortP(port) && InputPortOpenP(port));

    if (AsGenericPort(port)->Flags & PORT_FLAG_BUFFERED)
    {
        FAssert(AsGenericPort(port)->Context != 0);

        FBufferedContext * bc = AsBufferedContext(port);

        FAssert(bc->Used == bc->Available);
        FAssert(bc->Buffer != 0);

        bc->Used = 0;
        bc->Available = ReadBytes(AsGenericPort(port)->Object, bc->Buffer, bc->Maximum);
        if (bc->Available == 0)
            return(0);

        *bl = bc->Available;
        return(bc->Buffer);
    }

    return(0);
}

static long_t BufferedByteReadyP(FObject port)
{
    FAssert(BinaryPortP(port) && InputPortOpenP(port));

    FBufferedContext * bc = AsBufferedContext(port);

    return(bc->Used < bc->Available || ByteReadyP(AsGenericPort(port)->Object));
}

static void BufferedWriteBytes(FObject port, void * b, ulong_t bl)
{
    FAssert(BinaryPortP(port) && OutputPortOpenP(port));
    FAssert(AsGenericPort(port)->Context != 0);

    FBufferedContext * bc = AsBufferedContext(port);

    FAssert(bc->Used < bc->Maximum);

    if (bc->Used + bl < bc->Maximum)
    {
        memcpy(bc->Buffer + bc->Used, b, bl);
        bc->Used += bl;
    }
    else
    {
        ulong_t sz = bc->Maximum - bc->Used;

        memcpy(bc->Buffer + bc->Used, b, sz);
        WriteBytes(AsGenericPort(port)->Object, bc->Buffer, bc->Maximum);
        bc->Used = 0;

        if (sz < bl)
            WriteBytes(AsGenericPort(port)->Object, ((char *) b) + sz, bl - sz);
    }
}

static int64_t BufferedGetPosition(FObject port)
{
    FAssert(BinaryPortP(port) && PortOpenP(port) && PositioningPortP(port));
    FAssert(AsGenericPort(port)->Context != 0);

    FBufferedContext * bc = AsBufferedContext(port);

    if (InputPortOpenP(port))
    {
        int64_t pos = GetPosition(AsGenericPort(port)->Object);

        FAssert(pos - bc->Available + bc->Used >= 0);

        return(pos - bc->Available + bc->Used);
    }
    else if (OutputPortOpenP(port))
        return(GetPosition(AsGenericPort(port)->Object) + bc->Used);

    return(0);
}

static long_t BufferedSetPosition(FObject port, int64_t pos, FPositionFrom frm)
{
    FAssert(BinaryPortP(port) && PortOpenP(port) && PositioningPortP(port));
    FAssert(AsGenericPort(port)->Context != 0);

    if (InputPortOpenP(port))
    {
        FBufferedContext * bc = AsBufferedContext(port);

        if (frm == FromCurrent)
            pos = pos - bc->Available + bc->Used;

        bc->Available = 0;
        bc->Used = 0;

        return(SetPosition(AsGenericPort(port)->Object, pos, frm));
    }
    else if (OutputPortOpenP(port))
    {
        BufferedFlushOutput(port);

        return(SetPosition(AsGenericPort(port)->Object, pos, frm));
    }

    return(0);
}

static FObject MakeBufferedPort(FObject port)
{
    FAssert(BinaryPortP(port));
    FAssert((InputPortOpenP(port) && OutputPortOpenP(port) == 0) ||
            (InputPortOpenP(port) == 0 && OutputPortOpenP(port)));

    if (AsGenericPort(port)->Flags & PORT_FLAG_BUFFERED)
        return(port);

    FBufferedContext * bc = (FBufferedContext *) malloc(sizeof(FBufferedContext));
    if (bc == 0)
        return(NoValueObject);

    bc->Buffer = 0;
    bc->Available = 0;
    bc->Used = 0;
    bc->Maximum = 1024;

    bc->Buffer = (unsigned char *) malloc(bc->Maximum);
    if (bc->Buffer == 0)
    {
        free(bc);
        return(NoValueObject);
    }

    port = HandOffPort(port);
    return(MakeBinaryPort(AsGenericPort(port)->Name, port, bc,
            InputPortOpenP(port) ? BufferedCloseInput : 0,
            OutputPortOpenP(port) ? BufferedCloseOutput : 0,
            OutputPortOpenP(port) ? BufferedFlushOutput : 0,
            InputPortOpenP(port) ? BufferedReadBytes : 0,
            InputPortOpenP(port) ? BufferedByteReadyP : 0,
            OutputPortOpenP(port) ? BufferedWriteBytes : 0,
            PositioningPortP(port) ? BufferedGetPosition : 0,
            PositioningPortP(port) ? BufferedSetPosition : 0,
            GetObjectFileHandle,
            PORT_FLAG_BUFFERED));
}

// ---- CustomPort ----

typedef struct
{
    FObject ReadFn;
    FObject WriteFn;
    FObject GetPositionFn;
    FObject SetPositionFn;
    FObject CloseFn;
    FObject FlushFn;
    FObject Buffer;
} FCustomPort;

#define AsCustomPort(port) ((FCustomPort *) AsGenericPort(port)->Object)

static ulong_t CustomReadBytes(FObject port, void * b, ulong_t bl)
{
    FAssert(BinaryPortP(port) && InputPortOpenP(port));

    FAlive ap(&port);
    FCustomPort * custom = AsCustomPort(port);
    ulong_t vl = BytevectorLength(custom->Buffer);
    ulong_t rl = 0;

    while (bl > 0)
    {
        FObject argv[3];

        argv[0] = custom->Buffer;
        argv[1] = MakeFixnum(0);
        argv[2] = MakeFixnum(bl < vl ? bl : vl);

        // (read! bytevector start count) => number of bytes read
        FObject obj = ExecuteProc(custom->ReadFn, 3, argv);
        if (FixnumP(obj) == 0 || AsFixnum(obj) < 0)
            RaiseExceptionC(Assertion, "make-custom-binary-input-port",
                    "expected non-negative integer from calls to read!",
                    List(custom->ReadFn, obj));

        if (AsFixnum(obj) == 0)
            break;

        memcpy(((char *) b) + rl, AsBytevector(custom->Buffer)->Vector, AsFixnum(obj));
        rl += AsFixnum(obj);
        bl -= rl;
    }

    return(rl);
}

static long_t CustomByteReadyP(FObject port)
{
    FAssert(BinaryPortP(port) && InputPortOpenP(port));

    return(1);
}

static void CustomWriteBytes(FObject port, void * b, ulong_t bl)
{
    FAssert(BinaryPortP(port) && OutputPortOpenP(port));

    FCustomPort * custom = AsCustomPort(port);
    ulong_t vl = BytevectorLength(custom->Buffer);

    while (bl > 0)
    {
        ulong_t wl = bl < vl ? bl : vl;
        memcpy(AsBytevector(custom->Buffer)->Vector, b, wl);

        FObject argv[3];

        argv[0] = custom->Buffer;
        argv[1] = MakeFixnum(0);
        argv[2] = MakeFixnum(wl);

        // (write! bytevector start count) => number of bytes written
        FObject obj = ExecuteProc(custom->WriteFn, 3, argv);
        if (FixnumP(obj) == 0 || AsFixnum(obj) <= 0)
            RaiseExceptionC(Assertion, "make-custom-binary-output-port",
                    "expected positive integer from calls to write!", List(custom->WriteFn, obj));

        wl = AsFixnum(obj);

        FAssert(wl <= bl);

        bl -= wl;
        b = ((char *) b) + wl;
    }
}

static int64_t CustomGetPosition(FObject port)
{
    FAssert(BinaryPortP(port) || TextualPortP(port));
    FAssert(PortOpenP(port) && PositioningPortP(port));

    FCustomPort * custom = AsCustomPort(port);

    // (get-position)
    FObject obj = ExecuteProc(custom->GetPositionFn, 0, 0);
    if (FixnumP(obj) == 0 || AsFixnum(obj) < 0)
        RaiseExceptionC(Assertion, "make-custom-binary-port",
                "expected non-negative integer from calls to get-position",
                List(custom->GetPositionFn, obj));

    return(AsFixnum(obj));
}

static long_t CustomSetPosition(FObject port, int64_t pos, FPositionFrom frm)
{
    FAssert(BinaryPortP(port) || TextualPortP(port));
    FAssert(PortOpenP(port) && PositioningPortP(port));

    FCustomPort * custom = AsCustomPort(port);

    if (frm != FromBegin)
        RaiseExceptionC(Assertion, "set-port-position!", "expected begin", EmptyListObject);

    FObject argv[1];

    argv[0] = MakeFixnum(pos);

    // (set-position! pos)
    ExecuteProc(custom->SetPositionFn, 1, argv);
    return(1);
}

static void CustomCloseInput(FObject port)
{
    FAssert(BinaryPortP(port) || TextualPortP(port));

    FCustomPort * custom = AsCustomPort(port);

    // (close)
    if (OutputPortOpenP(port) == 0 && custom->CloseFn != FalseObject)
        ExecuteProc(custom->CloseFn, 0, 0);
}

static void CustomCloseOutput(FObject port)
{
    FAssert(BinaryPortP(port) || TextualPortP(port));

    FCustomPort * custom = AsCustomPort(port);

    // (close)
    if (InputPortOpenP(port) == 0 && custom->CloseFn != FalseObject)
        ExecuteProc(custom->CloseFn, 0, 0);
}

static void CustomFlushOutput(FObject port)
{
    FAssert(BinaryPortP(port) || TextualPortP(port));

    FCustomPort * custom = AsCustomPort(port);

    // (flush)
    if (custom->FlushFn != FalseObject)
        ExecuteProc(custom->FlushFn, 0, 0);
}

static ulong_t CustomReadCh(FObject port, FCh * ch)
{
    FAssert(TextualPortP(port) && InputPortOpenP(port));

    FCustomPort * custom = AsCustomPort(port);
    FObject argv[3];

    argv[0] = custom->Buffer;
    argv[1] = MakeFixnum(0);
    argv[2] = MakeFixnum(1);

    // (read! string-or-char-vector start count)
    FObject obj = ExecuteProc(custom->ReadFn, 3, argv);
    if (FixnumP(obj) == 0 || AsFixnum(obj) < 0)
        RaiseExceptionC(Assertion, "make-custom-textual-input-port",
                "expected non-negative integer from calls to read!", List(custom->ReadFn, obj));

    if (AsFixnum(obj) == 0)
        return(0);

    *ch = AsString(custom->Buffer)->String[0];
    return(1);
}

static long_t CustomCharReadyP(FObject port)
{
    FAssert(TextualPortP(port) && InputPortOpenP(port));

    return(1);
}

static void CustomWriteString(FObject port, FCh * s, ulong_t sl)
{
    FAssert(TextualPortP(port) && OutputPortOpenP(port));

    FCustomPort * custom = AsCustomPort(port);
    ulong_t bl = StringLength(custom->Buffer);

    while (sl > 0)
    {
        ulong_t wl = sl < bl ? sl : bl;
        memcpy(AsString(custom->Buffer)->String, s, wl * sizeof(FCh));

        FObject argv[3];

        argv[0] = custom->Buffer;
        argv[1] = MakeFixnum(0);
        argv[2] = MakeFixnum(wl);

        // (write! string-or-char-vector start count) => number of characters written
        FObject obj = ExecuteProc(custom->WriteFn, 3, argv);
        if (FixnumP(obj) == 0 || AsFixnum(obj) <= 0)
            RaiseExceptionC(Assertion, "make-custom-textual-output-port",
                    "expected positive integer from calls to write!", List(custom->WriteFn, obj));

        wl = AsFixnum(obj);

        FAssert(wl <= sl);

        sl -= wl;
        s += wl;
    }
}

static FObject MakeCustomBinaryPort(FObject nam, FObject rfn, FObject wfn, FObject gpfn,
    FObject spfn, FObject cfn, FObject ffn)
{
    FAssert(ProcedureP(rfn) || PrimitiveP(rfn) || ProcedureP(wfn) || PrimitiveP(wfn));
    FAssert(rfn == FalseObject || ProcedureP(rfn) || PrimitiveP(rfn));
    FAssert(wfn == FalseObject || ProcedureP(wfn) || PrimitiveP(wfn));
    FAssert(gpfn == FalseObject || ProcedureP(gpfn) || PrimitiveP(gpfn));
    FAssert(spfn == FalseObject || ProcedureP(spfn) || PrimitiveP(spfn));
    FAssert(cfn == FalseObject || ProcedureP(cfn) || PrimitiveP(cfn));
    FAssert(ffn == FalseObject || ProcedureP(ffn) || PrimitiveP(ffn));

    FCustomPort * custom = (FCustomPort *) MakeObject(CustomPortTag, sizeof(FCustomPort), 7,
            "%make-custom-binary-port");
    custom->ReadFn = rfn;
    custom->WriteFn = wfn;
    custom->GetPositionFn = gpfn;
    custom->SetPositionFn = spfn;
    custom->CloseFn = cfn;
    custom->FlushFn = ffn;
#ifdef FOMENT_DEBUG
    custom->Buffer = MakeBytevector(4);
#else // FOMENT_DEBUG
    custom->Buffer = MakeBytevector(512);
#endif // FOMENT_DEBUG

    return(MakeBinaryPort(nam, custom, 0,
            rfn != FalseObject ? CustomCloseInput : 0,
            wfn != FalseObject ? CustomCloseOutput : 0,
            ffn != FalseObject ? CustomFlushOutput : 0,
            rfn != FalseObject ? CustomReadBytes : 0,
            rfn != FalseObject ? CustomByteReadyP : 0,
            wfn != FalseObject ? CustomWriteBytes : 0,
            gpfn != FalseObject ? CustomGetPosition : 0,
            spfn != FalseObject ? CustomSetPosition : 0,
            0, 0));
}

static FObject MakeCustomTextualPort(FObject nam, FObject rfn, FObject wfn, FObject gpfn,
    FObject spfn, FObject cfn, FObject ffn)
{
    FAssert(ProcedureP(rfn) || PrimitiveP(rfn) || ProcedureP(wfn) || PrimitiveP(wfn));
    FAssert(rfn == FalseObject || ProcedureP(rfn) || PrimitiveP(rfn));
    FAssert(wfn == FalseObject || ProcedureP(wfn) || PrimitiveP(wfn));
    FAssert(gpfn == FalseObject || ProcedureP(gpfn) || PrimitiveP(gpfn));
    FAssert(spfn == FalseObject || ProcedureP(spfn) || PrimitiveP(spfn));
    FAssert(cfn == FalseObject || ProcedureP(cfn) || PrimitiveP(cfn));
    FAssert(ffn == FalseObject || ProcedureP(ffn) || PrimitiveP(ffn));

    FCustomPort * custom = (FCustomPort *) MakeObject(CustomPortTag, sizeof(FCustomPort), 7,
            "%make-custom-textual-port");
    custom->ReadFn = rfn;
    custom->WriteFn = wfn;
    custom->GetPositionFn = gpfn;
    custom->SetPositionFn = spfn;
    custom->CloseFn = cfn;
    custom->FlushFn = ffn;
#ifdef FOMENT_DEBUG
    custom->Buffer = MakeStringCh(4, 0);
#else // FOMENT_DEBUG
    custom->Buffer = MakeStringCh(128, 0);
#endif // FOMENT_DEBUG

    return(MakeTextualPort(nam, custom, 0,
            rfn != FalseObject ? CustomCloseInput : 0,
            wfn != FalseObject ? CustomCloseOutput : 0,
            ffn != FalseObject ? CustomFlushOutput : 0,
            rfn != FalseObject ? CustomReadCh : 0,
            rfn != FalseObject ? CustomCharReadyP : 0,
            wfn != FalseObject ? CustomWriteString : 0,
            gpfn != FalseObject ? CustomGetPosition : 0,
            spfn != FalseObject ? CustomSetPosition : 0,
            0, 0));
}

static void SocketCloseInput(FObject port)
{
    FAssert(BinaryPortP(port));

    if (OutputPortOpenP(port) == 0)
        closesocket((SOCKET) AsGenericPort(port)->Context);
}

static void SocketCloseOutput(FObject port)
{
    FAssert(BinaryPortP(port));

    if (InputPortOpenP(port) == 0)
        closesocket((SOCKET) AsGenericPort(port)->Context);
}

static void SocketFlushOutput(FObject port)
{
    FAssert(BinaryPortP(port) && OutputPortOpenP(port));

    // Nothing.
}

static int SocketReceive(SOCKET s, char * b, int bl, int flgs)
{
//    EnterWait();
    int br = recv(s, b, bl, flgs);
//    LeaveWait();

    if (br == SOCKET_ERROR)
        return(0);

    FAssert(br >= 0);

    return(br);
}

static ulong_t SocketReadBytes(FObject port, void * b, ulong_t bl)
{
    FAssert(BinaryPortP(port) && InputPortOpenP(port));

    return(SocketReceive((SOCKET) AsGenericPort(port)->Context, (char *) b, (int) bl, 0));
}

static long_t SocketByteReadyP(FObject port)
{
    FAssert(BinaryPortP(port) && InputPortOpenP(port));

    char b;

    return(recv((SOCKET) AsGenericPort(port)->Context, &b, 1, MSG_PEEK) == 0 ? 0 : 1);
}

static void SocketSend(SOCKET s, char * b, int bl, int flgs)
{
    while (bl > 0)
    {
        int bs = send(s, b, bl, flgs);
        if (bs == SOCKET_ERROR)
            break;

        FAssert(bs > 0 && bs <= bl);

        bl -= bs;
        b += bs;
    }
}

static void SocketWriteBytes(FObject port, void * b, ulong_t bl)
{
    FAssert(BinaryPortP(port) && OutputPortOpenP(port));

    SocketSend((SOCKET) AsGenericPort(port)->Context, (char *) b, (int) bl, 0);
}

static FObject MakeSocketPort(SOCKET s)
{
    return(MakeBinaryPort(NoValueObject, NoValueObject, (void *) s, SocketCloseInput,
            SocketCloseOutput, SocketFlushOutput, SocketReadBytes, SocketByteReadyP,
            SocketWriteBytes, 0, 0, 0, PORT_FLAG_SOCKET));
}

#ifdef FOMENT_WINDOWS
static void HandleCloseInput(FObject port)
{
    FAssert(BinaryPortP(port));

    if (OutputPortOpenP(port) == 0)
        CloseHandle((HANDLE) AsGenericPort(port)->Context);
}

static void HandleCloseOutput(FObject port)
{
    FAssert(BinaryPortP(port));

    if (InputPortOpenP(port) == 0)
        CloseHandle((HANDLE) AsGenericPort(port)->Context);
}

static void HandleFlushOutput(FObject port)
{
    FAssert(BinaryPortP(port) && OutputPortOpenP(port));

    FlushFileBuffers((HANDLE) AsGenericPort(port)->Context);
}

static ulong_t HandleReadBytes(FObject port, void * b, ulong_t bl)
{
    FAssert(BinaryPortP(port) && InputPortOpenP(port));

    DWORD nr;
    HANDLE h = (HANDLE) AsGenericPort(port)->Context;

//    EnterWait();
    BOOL ret = ReadFile(h, b, (DWORD) bl, &nr, 0);
//    LeaveWait();

    return(ret == 0 ? 0 : nr);
}

static long_t FileByteReadyP(FObject port)
{
    FAssert(BinaryPortP(port) && InputPortOpenP(port));

    return(1);
}

static long_t PipeByteReadyP(FObject port)
{
    FAssert(BinaryPortP(port) && InputPortOpenP(port));

    DWORD tba;

    return(PeekNamedPipe((HANDLE) AsGenericPort(port)->Context, 0, 0, 0, &tba, 0) == 0
            || tba > 0);
}

static void HandleWriteBytes(FObject port, void * b, ulong_t bl)
{
    FAssert(BinaryPortP(port) && OutputPortOpenP(port));

    DWORD nw;

    WriteFile((HANDLE) AsGenericPort(port)->Context, b, (DWORD) bl, &nw, 0);
}

static int64_t HandleGetPosition(FObject port)
{
    FAssert(BinaryPortP(port) && PortOpenP(port) && PositioningPortP(port));

    LARGE_INTEGER off;
    LARGE_INTEGER pos;

    off.QuadPart = 0;
    if (SetFilePointerEx((HANDLE) AsGenericPort(port)->Context, off, &pos, FILE_CURRENT) == 0)
        return(0);

    return(pos.QuadPart);
}

static long_t HandleSetPosition(FObject port, int64_t pos, FPositionFrom frm)
{
    FAssert(BinaryPortP(port) && PortOpenP(port) && PositioningPortP(port));
    FAssert(frm == FromBegin || frm == FromCurrent || frm == FromEnd);

    LARGE_INTEGER off;

    off.QuadPart = pos;
    return(SetFilePointerEx((HANDLE) AsGenericPort(port)->Context, off, 0,
            frm == FromBegin ? FILE_BEGIN : (frm == FromCurrent ? FILE_CURRENT : FILE_END)));
}

static FFileHandle HandleGetFileHandle(FObject port)
{
    FAssert(BinaryPortP(port) || TextualPortP(port));

    return(AsGenericPort(port)->Context);
}

static FObject MakeHandleInputPort(FObject nam, HANDLE h)
{
    if (GetFileType(h) == FILE_TYPE_PIPE)
        return(MakeBinaryPort(nam, NoValueObject, h, HandleCloseInput, 0, 0, HandleReadBytes,
                PipeByteReadyP, 0, 0, 0, HandleGetFileHandle, 0));

    return(MakeBinaryPort(nam, NoValueObject, h, HandleCloseInput, 0, 0, HandleReadBytes,
            FileByteReadyP, 0, HandleGetPosition, HandleSetPosition, HandleGetFileHandle, 0));
}

static FObject MakeHandleOutputPort(FObject nam, HANDLE h)
{
    if (GetFileType(h) == FILE_TYPE_PIPE)
        return(MakeBinaryPort(nam, NoValueObject, h, 0, HandleCloseOutput, HandleFlushOutput, 0, 0,
                HandleWriteBytes, 0, 0, HandleGetFileHandle, 0));

    return(MakeBinaryPort(nam, NoValueObject, h, 0, HandleCloseOutput, HandleFlushOutput, 0, 0,
            HandleWriteBytes, HandleGetPosition, HandleSetPosition, HandleGetFileHandle, 0));
}
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
static void FileDescCloseInput(FObject port)
{
    FAssert(BinaryPortP(port));

    if (OutputPortOpenP(port) == 0)
        close((long_t) AsGenericPort(port)->Context);
}

static void FileDescCloseOutput(FObject port)
{
    FAssert(BinaryPortP(port));

    if (InputPortOpenP(port) == 0)
        close((long_t) AsGenericPort(port)->Context);
}

static void FileDescFlushOutput(FObject port)
{
    FAssert(BinaryPortP(port) && OutputPortOpenP(port));

    fsync((long_t) AsGenericPort(port)->Context);
}

static ulong_t FileDescReadBytes(FObject port, void * b, ulong_t bl)
{
    FAssert(BinaryPortP(port) && InputPortOpenP(port));

    long_t fd = (long_t) AsGenericPort(port)->Context;

//    EnterWait();
    long_t br = read(fd, b, bl);
//    LeaveWait();

    return(br <= 0 ? 0 : br);
}

static long_t FileDescByteReadyP(FObject port)
{
    FAssert(BinaryPortP(port) && InputPortOpenP(port));

    fd_set fds;
    timeval tv;

    FD_ZERO(&fds);
    FD_SET(0, &fds);

    tv.tv_sec = 0;
    tv.tv_usec = 0;

    return(select(1, &fds, 0, 0, &tv) > 0);
}

static void FileDescWriteBytes(FObject port, void * b, ulong_t bl)
{
    FAssert(BinaryPortP(port) && OutputPortOpenP(port));

    write((long_t) AsGenericPort(port)->Context, b, bl);
}

static int64_t FileDescGetPosition(FObject port)
{
    FAssert(BinaryPortP(port) && PortOpenP(port) && PositioningPortP(port));

    return(lseek((long_t) AsGenericPort(port)->Context, 0, SEEK_CUR));
}

static FFileHandle FileDescGetFileHandle(FObject port)
{
    FAssert(BinaryPortP(port) || TextualPortP(port));

    return((FFileHandle) (AsGenericPort(port)->Context));
}

static long_t FileDescSetPosition(FObject port, int64_t pos, FPositionFrom frm)
{
    FAssert(BinaryPortP(port) && PortOpenP(port) && PositioningPortP(port));
    FAssert(frm == FromBegin || frm == FromCurrent || frm == FromEnd);

    return(lseek((long_t) AsGenericPort(port)->Context, pos,
            frm == FromBegin ? SEEK_SET : (frm == FromCurrent ? SEEK_CUR : SEEK_END)) >= 0);
}

static FObject MakeFileDescInputPort(FObject nam, long_t fd)
{
    return(MakeBinaryPort(nam, NoValueObject, (void *) fd, FileDescCloseInput, 0, 0,
            FileDescReadBytes, FileDescByteReadyP, 0, FileDescGetPosition, FileDescSetPosition,
            FileDescGetFileHandle, 0));
}

static FObject MakeFileDescOutputPort(FObject nam, long_t fd)
{
    return(MakeBinaryPort(nam, NoValueObject, (void *) fd, 0, FileDescCloseOutput,
            FileDescFlushOutput, 0, 0, FileDescWriteBytes, FileDescGetPosition,
            FileDescSetPosition, FileDescGetFileHandle, 0));
}
#endif // FOMENT_UNIX

static void BytevectorInputCloseInput(FObject port)
{
    FAssert(BinaryPortP(port));

    AsGenericPort(port)->Object = NoValueObject;
}

static ulong_t BytevectorInputReadBytes(FObject port, void * b, ulong_t bl)
{
    FAssert(BinaryPortP(port) && InputPortOpenP(port));

    FObject bv = AsGenericPort(port)->Object;
    ulong_t bdx = (ulong_t) AsGenericPort(port)->Context;

    FAssert(BytevectorP(bv));
    FAssert(bdx <= BytevectorLength(bv));

    if (bdx == BytevectorLength(bv))
        return(0);

    if (bdx + bl > BytevectorLength(bv))
        bl = BytevectorLength(bv) - bdx;

    memcpy(b, AsBytevector(bv)->Vector + bdx, bl);
    AsGenericPort(port)->Context = (void *) (bdx + bl);

    return(bl);
}

static long_t BytevectorInputByteReadyP(FObject port)
{
    FAssert(BinaryPortP(port) && InputPortOpenP(port));

    return(1);
}

static FObject MakeBytevectorInputPort(FObject bv)
{
    FAssert(BytevectorP(bv));

    return(MakeBinaryPort(NoValueObject, bv, 0, BytevectorInputCloseInput, 0, 0,
            BytevectorInputReadBytes, BytevectorInputByteReadyP, 0, 0, 0, 0, 0));
}

static void BytevectorOutputCloseOutput(FObject port)
{
    // Nothing.
}

static void BytevectorOutputFlushOutput(FObject port)
{
    // Nothing.
}

static void BytevectorOutputWriteBytes(FObject port, void * b, ulong_t bl)
{
    FAssert(BinaryPortP(port) && OutputPortOpenP(port));

    FObject lst = Unbox(AsGenericPort(port)->Object);

    for (ulong_t bdx = 0; bdx < bl; bdx++)
        lst = MakePair(MakeFixnum(((unsigned char *) b)[bdx]), lst);

    SetBox(AsGenericPort(port)->Object, lst);
}

static FObject GetOutputBytevector(FObject port)
{
    FAssert(BytevectorOutputPortP(port));

    FObject lst = Unbox(AsGenericPort(port)->Object);
    long_t bl = ListLength("get-output-bytevector", lst);
    FObject bv = MakeBytevector(bl);
    long_t bdx = bl;

    while (PairP(lst))
    {
        bdx -= 1;

        FAssert(bdx >= 0);
        FAssert(FixnumP(First(lst)) && AsFixnum(First(lst)) >= 0 && AsFixnum(First(lst)) <= 0xFF);

        AsBytevector(bv)->Vector[bdx] = (FByte) AsFixnum(First(lst));
        lst = Rest(lst);
    }

    FAssert(lst == EmptyListObject);

    return(bv);
}

static FObject MakeBytevectorOutputPort()
{
    return(MakeBinaryPort(NoValueObject, MakeBox(EmptyListObject), 0, 0,
            BytevectorOutputCloseOutput, BytevectorOutputFlushOutput, 0, 0,
            BytevectorOutputWriteBytes, 0, 0, 0, PORT_FLAG_BYTEVECTOR_OUTPUT));
}

// ---- Textual Ports ----

FObject MakeTextualPort(FObject nam, FObject obj, void * ctx, FCloseInputFn cifn,
    FCloseOutputFn cofn, FFlushOutputFn fofn, FReadChFn rcfn, FCharReadyPFn crpfn,
    FWriteStringFn wsfn, FGetPositionFn gpfn, FSetPositionFn spfn, FGetFileHandleFn gfhfn,
    ulong_t flgs)
{
    FAssert((cifn == 0 && rcfn == 0 && crpfn == 0) || (cifn != 0 && rcfn != 0 && crpfn != 0));
    FAssert((cofn == 0 && wsfn == 0 && fofn == 0) || (cofn != 0 && wsfn != 0 && fofn != 0));
    FAssert(cifn != 0 || cofn != 0);
    FAssert((gpfn == 0 && spfn == 0) || (gpfn != 0 && spfn != 0));

    FTextualPort * port = (FTextualPort *) MakeObject(TextualPortTag, sizeof(FTextualPort), 2,
            "%make-textual-port");
    port->Generic.Flags = (cifn != 0 ? (PORT_FLAG_INPUT | PORT_FLAG_INPUT_OPEN) : 0)
            | (cofn != 0 ? (PORT_FLAG_OUTPUT | PORT_FLAG_OUTPUT_OPEN) : 0)
            | (gpfn != 0 ? PORT_FLAG_POSITIONING : 0)
            | flgs;
    port->Generic.Name = nam;
    port->Generic.Object = obj;
    port->Generic.Context = ctx;
    port->Generic.CloseInputFn = cifn;
    port->Generic.CloseOutputFn = cofn;
    port->Generic.FlushOutputFn = fofn;
    port->Generic.GetPositionFn = gpfn;
    port->Generic.SetPositionFn = spfn;
    port->Generic.GetFileHandleFn = gfhfn == 0 ? InvalidFileHandle : gfhfn;
    port->ReadChFn = rcfn;
    port->CharReadyPFn = crpfn;
    port->WriteStringFn = wsfn;
    port->PeekedChar = NOT_PEEKED;
    port->Line = 1;
    port->Column = 0;

    InstallGuardian(port, CleanupTConc);

    return(port);
}

ulong_t ReadCh(FObject port, FCh * ch)
{
    FAssert(TextualPortP(port) && InputPortOpenP(port));

    if (AsTextualPort(port)->PeekedChar != NOT_PEEKED)
    {
        *ch = (FCh) AsTextualPort(port)->PeekedChar;
        AsTextualPort(port)->PeekedChar = NOT_PEEKED;

        return(1);
    }

    return(AsTextualPort(port)->ReadChFn(port, ch));
}

ulong_t PeekCh(FObject port, FCh * ch)
{
    FAssert(TextualPortP(port) && InputPortOpenP(port));

    if (AsTextualPort(port)->PeekedChar != NOT_PEEKED)
    {
        *ch = (FCh) AsTextualPort(port)->PeekedChar;
        return(1);
    }

    FAlive ap(&port);

    if (PositioningPortP(port))
    {
        int64_t pos = GetPosition(port);

        if (ReadCh(port, ch) == 0)
            return(0);

        SetPosition(port, pos, FromBegin);
    }
    else
    {
        if (ReadCh(port, ch) == 0)
            return(0);

        AsTextualPort(port)->PeekedChar = *ch;
    }

    return(1);
}

long_t CharReadyP(FObject port)
{
    FAssert(TextualPortP(port) && InputPortOpenP(port));

    if (AsTextualPort(port)->PeekedChar != NOT_PEEKED)
        return(1);

    return(AsTextualPort(port)->CharReadyPFn(port));
}

static FObject ListToString(FObject lst)
{
    ulong_t sl = ListLength("read-line", lst);
    FObject s = MakeString(0, sl);
    ulong_t sdx = sl;

    while (sdx > 0)
    {
        sdx -= 1;
        AsString(s)->String[sdx] = AsCharacter(First(lst));
        lst = Rest(lst);
    }

    return(s);
}

FObject ReadLine(FObject port)
{
    FAssert(TextualPortP(port) && InputPortOpenP(port));

    FAlive ap(&port);
    FObject lst = EmptyListObject;
    FAlive al(&lst);
    FCh ch;
    ulong_t ret;

    for (;;)
    {
        ret = ReadCh(port, &ch);
        if (ret == 0)
            break;
        if (ch == 0x0A || ch == 0x0D)
            break;

        lst = MakePair(MakeCharacter(ch), lst);
    }

    if (ch == 0x0D) // carriage return
    {
        while(PeekCh(port, &ch))
        {
            if (ch == 0x0A) // linefeed
            {
                ReadCh(port, &ch);
                break;
            }

            if (ch != 0x0D) // carriage return
                break;

            ReadCh(port, &ch);
        }
    }

    if (ret == 0 && lst == EmptyListObject)
        return(EndOfFileObject);

    return(ListToString(lst));
}

FObject ReadString(FObject port, ulong_t cnt)
{
    FAssert(TextualPortP(port) && InputPortOpenP(port));

    FAlive ap(&port);
    FObject lst = EmptyListObject;
    FAlive al(&lst);
    FCh ch;

    while (cnt > 0 && ReadCh(port, &ch))
    {
        cnt -= 1;
        lst = MakePair(MakeCharacter(ch), lst);
    }

    if (lst == EmptyListObject)
        return(EndOfFileObject);

    return(ListToString(lst));
}

ulong_t GetLineColumn(FObject port, ulong_t * col)
{
    FAssert(TextualPortP(port) && InputPortP(port));

    if (col)
        *col = AsTextualPort(port)->Column;
    return(AsTextualPort(port)->Line);
}

FObject GetFilename(FObject port)
{
    FAssert(BinaryPortP(port) || TextualPortP(port));

    return(AsGenericPort(port)->Name);
}

void FoldcasePort(FObject port, long_t fcf)
{
    FAssert(TextualPortP(port) && InputPortP(port));

    if (fcf)
        AsGenericPort(port)->Flags |= PORT_FLAG_FOLDCASE;
    else
        AsGenericPort(port)->Flags &= ~PORT_FLAG_FOLDCASE;
}

void WantIdentifiersPort(FObject port, long_t wif)
{
    FAssert(TextualPortP(port) && InputPortP(port));

    if (wif)
        AsGenericPort(port)->Flags |= PORT_FLAG_WANT_IDENTIFIERS;
    else
        AsGenericPort(port)->Flags &= ~PORT_FLAG_WANT_IDENTIFIERS;
}

void WriteCh(FObject port, FCh ch)
{
    FAssert(TextualPortP(port) && OutputPortOpenP(port));

    AsTextualPort(port)->WriteStringFn(port, &ch, 1);
}

void WriteString(FObject port, FCh * s, ulong_t sl)
{
    FAssert(TextualPortP(port) && OutputPortOpenP(port));

    AsTextualPort(port)->WriteStringFn(port, s, sl);
}

void WriteStringC(FObject port, const char * s)
{
    FAssert(TextualPortP(port) && OutputPortOpenP(port));

    while (*s)
    {
        FCh ch = *s;
        AsTextualPort(port)->WriteStringFn(port, &ch, 1);
        s += 1;
    }
}

FObject HandOffPort(FObject port)
{
    FAssert(BinaryPortP(port) || TextualPortP(port));

    FObject nport;

    if (BinaryPortP(port))
    {
        nport = MakeObject(BinaryPortTag, sizeof(FBinaryPort), 2, "transcoded-port");
        memcpy(nport, port, sizeof(FBinaryPort));
    }
    else
    {
        nport = MakeObject(TextualPortTag, sizeof(FTextualPort), 2, "transcoded-port");
        memcpy(nport, port, sizeof(FTextualPort));
    }

    AsGenericPort(port)->Flags &= ~PORT_FLAG_INPUT_OPEN;
    AsGenericPort(port)->Flags &= ~PORT_FLAG_OUTPUT_OPEN;
    AsGenericPort(port)->Context = 0;

    return(nport);
}

void CloseInput(FObject port)
{
    FAssert(BinaryPortP(port) || TextualPortP(port));

    if (InputPortOpenP(port))
    {
        AsGenericPort(port)->Flags &= ~PORT_FLAG_INPUT_OPEN;

        AsGenericPort(port)->CloseInputFn(port);
    }
}

void CloseOutput(FObject port)
{
    FAssert(BinaryPortP(port) || TextualPortP(port));

    if (OutputPortOpenP(port))
    {
        AsGenericPort(port)->Flags &= ~PORT_FLAG_OUTPUT_OPEN;

        AsGenericPort(port)->CloseOutputFn(port);
    }
}

void FlushOutput(FObject port)
{
    FAssert(BinaryPortP(port) || TextualPortP(port));
    FAssert(OutputPortOpenP(port));

    AsGenericPort(port)->FlushOutputFn(port);
}

int64_t GetPosition(FObject port)
{
    FAssert(PositioningPortP(port));

    return(AsGenericPort(port)->GetPositionFn(port));
}

long_t SetPosition(FObject port, int64_t pos, FPositionFrom frm)
{
    FAssert(PositioningPortP(port));

    return(AsGenericPort(port)->SetPositionFn(port, pos, frm));
}

FFileHandle GetFileHandle(FObject port)
{
    FAssert(BinaryPortP(port) || TextualPortP(port));

    return(AsGenericPort(port)->GetFileHandleFn(port));
}

// ---- Transcoded Ports ----

typedef enum
{
    StyleNone = 0,
    StyleLF = 1,
    StyleCRLF = 2
} FEOLStyle;

typedef enum
{
    ModeReplace = 0,
    ModeRaise = 1
} FErrorMode;

static ulong_t AsciiReadCh(FObject port, FErrorMode mode, FCh * ch)
{
    unsigned char b;

    if (ReadBytes(port, &b, 1) != 1)
        return(0);

    if (b > 127)
    {
        if (mode == ModeReplace)
            *ch = '?';
        else // mode == ModeRaise
            RaiseExceptionC(Assertion, "read-char", DecodingErrorSymbol,
                    "unable to decode byte to ascii", List(port, MakeFixnum(b)));
    }
    else
        *ch = b;

    return(1);
}

static void AsciiWriteString(FObject port, FErrorMode mode, FCh * s, ulong_t sl)
{
    for (ulong_t sdx = 0; sdx < sl; sdx++)
    {
        unsigned char b;

        if (s[sdx] > 0x7F)
        {
            if (mode == ModeReplace)
                b = '?';
            else // mode == ModeRaise
                RaiseExceptionC(Assertion, "write-char", EncodingErrorSymbol,
                        "unable to encode character to ascii", List(port, MakeCharacter(s[sdx])));
        }
        else
            b = (unsigned char) s[sdx];

        WriteBytes(port, &b, 1);
    }
}

static ulong_t Latin1ReadCh(FObject port, FErrorMode mode, FCh * ch)
{
    unsigned char b;

    if (ReadBytes(port, &b, 1) != 1)
        return(0);

    *ch = b;
    return(1);
}

static void Latin1WriteString(FObject port, FErrorMode mode, FCh * s, ulong_t sl)
{
    for (ulong_t sdx = 0; sdx < sl; sdx++)
    {
        unsigned char b;

        if (s[sdx] > 0xFF)
        {
            if (mode == ModeReplace)
                b = '?';
            else // mode == ModeRaise
                RaiseExceptionC(Assertion, "write-char", EncodingErrorSymbol,
                        "unable to encode character to latin1", List(port, MakeCharacter(s[sdx])));
        }
        else
            b = (unsigned char) s[sdx];

        WriteBytes(port, &b, 1);
    }
}

static ulong_t Utf8ReadCh(FObject port, FErrorMode mode, FCh * pch)
{
    unsigned char ub[6];

    if (ReadBytes(port, ub, 1) != 1)
        return(0);

    ulong_t tb = Utf8TrailingBytes[ub[0]];
    if (tb == 0)
    {
        *pch = ub[0];
        return(1);
    }

    if (ReadBytes(port, ub + 1, tb) != tb)
        return(0);

    FCh ch = ConvertUtf8ToCh(ub, tb + 1);
    if (ch > MaximumUnicodeCharacter
            || (ch >= Utf16HighSurrogateStart && ch <= Utf16LowSurrogateEnd))
    {
        if (mode == ModeReplace)
            ch = UnicodeReplacementCharacter;
        else // mode == ModeRaise
        {
            FObject lst = EmptyListObject;
            for (ulong_t bc = tb; bc > 0; bc -= 1)
                lst = MakePair(MakeFixnum(ub[tb]), lst);
            lst = MakePair(port, MakePair(MakeFixnum(ub[0]), lst));

            RaiseExceptionC(Assertion, "read-char", DecodingErrorSymbol,
                   "unable to decode byte to utf-8", lst);

        }
    }

    *pch = ch;
    return(1);
}

static void Utf8WriteString(FObject port, FErrorMode mode, FCh * s, ulong_t sl)
{
    FObject bv;

    if (mode == ModeReplace)
        bv = ConvertStringToUtf8(s, sl, 0);
    else // mode == ModeRaise
    {
        FCh ch;

        bv = ConvertStringToUtf8(s, sl, 0, &ch);
        if (BytevectorP(bv) == 0)
            RaiseExceptionC(Assertion, "write-char", EncodingErrorSymbol,
                    "unable to encode character to utf-8", List(port, MakeCharacter(ch)));
    }

    FAssert(BytevectorP(bv));

    WriteBytes(port, AsBytevector(bv)->Vector, BytevectorLength(bv));
}

static ulong_t Utf16ReadCh(FObject port, FErrorMode mode, FCh * pch)
{
    FCh16 ch16;

    for (;;)
    {
        if (ReadBytes(port, (FByte *) &ch16, 2) != 2)
            return(0);
        if (ch16 != 0xFEFF)
            break;
    }

    FCh ch = ch16;

    if (ch >= Utf16HighSurrogateStart && ch <= Utf16HighSurrogateEnd)
    {
        if (ReadBytes(port, (FByte *) &ch16, 2) != 2)
            return(0);

        if (ch16 >= Utf16LowSurrogateStart && ch16 <= Utf16LowSurrogateEnd)
            ch = ((ch - Utf16HighSurrogateStart) << Utf16HalfShift)
                    + (((FCh) ch16) - Utf16LowSurrogateStart) + Utf16HalfBase;
        else if (mode == ModeReplace)
            ch = UnicodeReplacementCharacter;
        else // mode == ModeRaise
            RaiseExceptionC(Assertion, "read-char", DecodingErrorSymbol,
                    "unable to decode byte to utf-16",
                    List(port, MakeFixnum(ch >> 8), MakeFixnum(ch & 0xFF),
                            MakeFixnum(ch16 >> 8), MakeFixnum(ch16 & 0xFF)));
    }
    else if (ch >= Utf16LowSurrogateStart && ch <= Utf16LowSurrogateEnd)
    {
        if (mode == ModeReplace)
            ch = UnicodeReplacementCharacter;
        else // mode == ModeRaise
            RaiseExceptionC(Assertion, "read-char", DecodingErrorSymbol,
                    "unable to decode byte to utf-16",
                    List(port, MakeFixnum(ch >> 8), MakeFixnum(ch & 0xFF)));
    }

    *pch = ch;
    return(1);
}

static void Utf16WriteString(FObject port, FErrorMode mode, FCh * s, ulong_t sl)
{
    FObject bv;

    if (mode == ModeReplace)
        bv = ConvertStringToUtf16(s, sl, 0, 0);
    else // mode == ModeRaise
    {
        FCh ch;

        bv = ConvertStringToUtf16(s, sl, 0, 0, &ch);
        if (BytevectorP(bv) == 0)
            RaiseExceptionC(Assertion, "write-char", EncodingErrorSymbol,
                    "unable to encode character to utf-16", List(port, MakeCharacter(ch)));
    }

    FAssert(BytevectorP(bv));

    WriteBytes(port, AsBytevector(bv)->Vector, BytevectorLength(bv));
}

typedef ulong_t (*FReadChModeFn)(FObject port, FErrorMode mode, FCh * ch);
typedef void (*FWriteStringModeFn)(FObject port, FErrorMode mode, FCh * s, ulong_t sl);

typedef struct
{
    const char * Name;
    const char * Constant;
    FReadChModeFn ReadChFn;
    FWriteStringModeFn WriteStringFn;
} FCodecType;

#define ASCII_CODEC_INDEX 0
#define LATIN1_CODEC_INDEX 1
#define UTF8_CODEC_INDEX 2
#define UTF16_CODEC_INDEX 3

static FCodecType CodecTypes[] =
{
    {"ascii", "*ascii-codec*", AsciiReadCh, AsciiWriteString},
    {"latin-1", "*latin-1-codec*", Latin1ReadCh, Latin1WriteString},
    {"utf-8", "*utf-8-codec*", Utf8ReadCh, Utf8WriteString},
    {"utf-16", "*utf-16-codec*", Utf16ReadCh, Utf16WriteString},
};

#define AsCodec(obj) ((FCodec *) (obj))
#define CodecP(obj) (ObjectTag(obj) == CodecTag)

typedef struct
{
    ulong_t Index;
} FCodec;

static const char * CodecName(FObject obj);

void WriteCodec(FWriteContext * wctx, FObject obj)
{
    wctx->WriteStringC("#<codec: ");
    wctx->WriteStringC(CodecName(obj));
    wctx->WriteStringC(">");
}

static const char * CodecName(FObject obj)
{
    FAssert(CodecP(obj));
    FAssert(AsCodec(obj)->Index >= 0 &&
            AsCodec(obj)->Index < sizeof(CodecTypes) / sizeof(FCodecType));

    return(CodecTypes[AsCodec(obj)->Index].Name);
}

static FObject MakeCodec(ulong_t idx)
{
    FAssert(idx >= 0 && idx < sizeof(CodecTypes) / sizeof(FCodecType));

    FCodec * codec = (FCodec *) MakeObject(CodecTag, sizeof(FCodec), 0, "%make-codec");
    codec->Index = idx;
    return(codec);
}

#define AsTranscoder(obj) ((FTranscoder *) (obj))
#define TranscoderP(obj) (ObjectTag(obj) == TranscoderTag)

typedef struct
{
    FObject Codec;
    FEOLStyle Style;
    FErrorMode Mode;
} FTranscoder;

void WriteTranscoder(FWriteContext * wctx, FObject obj)
{
    FAssert(CodecP(AsTranscoder(obj)->Codec));

    wctx->WriteStringC("#<transcoder: ");
    wctx->WriteStringC(CodecName(AsTranscoder(obj)->Codec));

    switch (AsTranscoder(obj)->Style)
    {
    case StyleNone:
        wctx->WriteStringC(" none");
        break;

    case StyleLF:
        wctx->WriteStringC(" lf");
        break;

    case StyleCRLF:
        wctx->WriteStringC(" crlf");
        break;

    default:
        FAssert(0);
    }

    switch (AsTranscoder(obj)->Mode)
    {
    case ModeReplace:
        wctx->WriteStringC(" replace");
        break;

    case ModeRaise:
        wctx->WriteStringC(" raise");
        break;

    default:
        FAssert(0);
    }
    wctx->WriteStringC(">");
}

static FObject MakeTranscoder(FObject codec, FEOLStyle style, FErrorMode mode)
{
    FAssert(CodecP(codec));
    FAssert(style == StyleNone || style == StyleLF || style == StyleCRLF);
    FAssert(mode == ModeReplace || mode == ModeRaise);

    FTranscoder * tc = (FTranscoder *) MakeObject(TranscoderTag, sizeof(FTranscoder), 1,
            "make-transcoder");
    tc->Codec = codec;
    tc->Style = style;
    tc->Mode = mode;
    return(tc);
}

typedef struct
{
    FEOLStyle Style;
    FErrorMode Mode;
    unsigned char PreviousCR;
    FReadChModeFn ReadChFn;
    FWriteStringModeFn WriteStringFn;
    FCh PreviousCh;
} FTranscodedContext;

#define AsTranscodedContext(port) ((FTranscodedContext *) AsGenericPort(port)->Context)

static void TranscodedCloseInput(FObject port)
{
    FAssert(BinaryPortP(AsGenericPort(port)->Object));

    if (OutputPortOpenP(port) == 0)
        free(AsTranscodedContext(port));

    CloseInput(AsGenericPort(port)->Object);
}

static void TranscodedCloseOutput(FObject port)
{
    FAssert(BinaryPortP(AsGenericPort(port)->Object));

    if (InputPortOpenP(port) == 0)
        free(AsTranscodedContext(port));

    CloseOutput(AsGenericPort(port)->Object);
}

static void TranscodedFlushOutput(FObject port)
{
    FAssert(BinaryPortP(AsGenericPort(port)->Object));

    FlushOutput(AsGenericPort(port)->Object);
}

static ulong_t TranscodedReadCh(FObject port, FCh * ch)
{
    FAssert(BinaryPortP(AsGenericPort(port)->Object));

    FTranscodedContext * tc = AsTranscodedContext(port);

    for (;;)
    {
        if (tc->ReadChFn(AsGenericPort(port)->Object, tc->Mode, ch) == 0)
            return(0);

        if (*ch != LF || tc->PreviousCR == 0)
            break;
        tc->PreviousCR = 0;
    }

    if (tc->Style == StyleNone)
        return(1);

    if (*ch == CR)
    {
        *ch = LF;
        tc->PreviousCR = 1;
    }

    if (*ch == LF)
    {
        AsTextualPort(port)->Line += 1;
        AsTextualPort(port)->Column = 0;
    }

    return(1);
}

static long_t TranscodedCharReadyP(FObject port)
{
    FAssert(BinaryPortP(AsGenericPort(port)->Object));

    return(ByteReadyP(AsGenericPort(port)->Object));
}

static void TranscodedWriteString(FObject port, FCh * s, ulong_t sl)
{
    FAssert(BinaryPortP(AsGenericPort(port)->Object));

    FTranscodedContext * tc = AsTranscodedContext(port);

    if (tc->Style == StyleNone)
        tc->WriteStringFn(AsGenericPort(port)->Object, tc->Mode, s, sl);
    else if (tc->Style == StyleLF)
    {
        for (ulong_t sdx = 0; sdx < sl; sdx += 1)
        {
            FCh ch = s[sdx];
            if (ch == LF && tc->PreviousCh == CR)
            {
                tc->PreviousCh = s[sdx];
                continue;
            }
            if (ch == CR)
                ch = LF;

            tc->WriteStringFn(AsGenericPort(port)->Object, tc->Mode, &ch, 1);
            tc->PreviousCh = s[sdx];
        }
    }
    else // tc->Style == StyleCRLF
    {
        for (ulong_t sdx = 0; sdx < sl; sdx += 1)
        {
            if (tc->PreviousCh == CR && s[sdx] != LF)
            {
                FCh ch = LF;
                tc->WriteStringFn(AsGenericPort(port)->Object, tc->Mode, &ch, 1);
            }
            else if (s[sdx] == LF && tc->PreviousCh != CR)
            {
                FCh ch = CR;
                tc->WriteStringFn(AsGenericPort(port)->Object, tc->Mode, &ch, 1);
            }

            FCh ch = s[sdx];
            tc->WriteStringFn(AsGenericPort(port)->Object, tc->Mode, &ch, 1);
            tc->PreviousCh = s[sdx];
        }
    }
}

static FObject MakeTranscodedPort(FObject port, ulong_t cdx, FEOLStyle style, FErrorMode mode)
{
    FAssert(BinaryPortP(port));
    FAssert(cdx >= 0 && cdx < sizeof(CodecTypes) / sizeof(FCodecType));

    FTranscodedContext * tc = (FTranscodedContext *) malloc(sizeof(FTranscodedContext));
    if (tc == 0)
        return(NoValueObject);

    tc->Style = style;
    tc->Mode = mode;
    tc->WriteStringFn = CodecTypes[cdx].WriteStringFn;
    tc->ReadChFn = CodecTypes[cdx].ReadChFn;
    tc->PreviousCR = 0;
    tc->PreviousCh = 0;

    port = HandOffPort(port);
    return(MakeTextualPort(AsGenericPort(port)->Name, port, tc,
            InputPortP(port) ? TranscodedCloseInput : 0,
            OutputPortP(port) ? TranscodedCloseOutput : 0,
            OutputPortP(port) ? TranscodedFlushOutput : 0,
            InputPortP(port) ? TranscodedReadCh : 0,
            InputPortP(port) ? TranscodedCharReadyP : 0,
            OutputPortP(port) ? TranscodedWriteString : 0, 0, 0, GetObjectFileHandle, 0));
}

static long_t EncodingChP(char ch)
{
    return((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')
            || ch == '-');
}

static FObject MakeEncodedPort(FObject port)
{
    FAssert(BinaryPortP(port));

    if (InputPortOpenP(port) == 0)
#ifdef FOMENT_WINDOWS
        return(MakeTranscodedPort(port, LATIN1_CODEC_INDEX, StyleNone, ModeReplace));
#endif // FOMENT_WINDOWS
#ifdef FOMENT_UNIX
        return(MakeTranscodedPort(port, UTF8_CODEC_INDEX, StyleNone, ModeReplace));
#endif // FOMENT_UNIX

    FAlive ap(&port);
    ulong_t bl;
    char * b = (char *) LookaheadBytes(port, &bl);

    if (b == 0)
        return(MakeTranscodedPort(port, ASCII_CODEC_INDEX, StyleNone, ModeReplace));

    FAssert(b != 0);

    if (bl >= 2 && (unsigned char) b[0] == 0xFF && (unsigned char) b[1] == 0xFE) // Little Endian
        return(MakeTranscodedPort(port, UTF16_CODEC_INDEX, StyleNone, ModeReplace));

//    if (bl >= 2 && b[0] == 0xFE && b[1] == 0xFF) // Big Endian
//        return(MakeTranscodedPort(port, UTF16_CODEC_INDEX, StyleNone, ModeReplace));

    // Search for "coding:" but strstr will not work because b is not null terminated.

    for (ulong_t bdx = 6; bdx < bl; bdx += 7)
    {
        if (b[bdx] == ':' && b[bdx - 1] == 'g' && b[bdx - 2] == 'n' && b[bdx - 3] == 'i'
                && b[bdx - 4] == 'd' && b[bdx - 5] == 'o' && b[bdx - 6] == 'c')
        {
            bdx += 1;
            while (bdx < bl && EncodingChP(b[bdx]) == 0)
                bdx += 1;

            ulong_t edx = bdx;
            while (edx < bl && EncodingChP(b[edx]))
                edx += 1;

            if (edx > bdx)
            {
                if (edx - bdx == 5 && strncmp("utf-8", b + bdx, 5) == 0)
                    return(MakeTranscodedPort(port, UTF8_CODEC_INDEX, StyleNone, ModeReplace));
                else if (edx - bdx == 7 && strncmp("latin-1", b + bdx, 7) == 0)
                    return(MakeTranscodedPort(port, LATIN1_CODEC_INDEX, StyleNone, ModeReplace));
                else if (edx - bdx == 10 && strncmp("iso-8859-1", b + bdx, 10) == 0)
                    return(MakeTranscodedPort(port, LATIN1_CODEC_INDEX, StyleNone, ModeReplace));
                else if (edx - bdx == 5 && strncmp("ascii", b + bdx, 5) == 0)
                    return(MakeTranscodedPort(port, ASCII_CODEC_INDEX, StyleNone, ModeReplace));
            }

            break;
        }
    }

    return(MakeTranscodedPort(port, ASCII_CODEC_INDEX, StyleNone, ModeReplace));
}

static FObject OpenBinaryInputFile(FObject fn)
{
#ifdef FOMENT_WINDOWS
    FObject bv = ConvertStringToUtf16(fn);

    FAssert(BytevectorP(bv));

    HANDLE h = CreateFileW((FCh16 *) AsBytevector(bv)->Vector, GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (h == INVALID_HANDLE_VALUE)
        return(NoValueObject);

    return(MakeBufferedPort(MakeHandleInputPort(fn, h)));
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    FObject bv = ConvertStringToUtf8(fn);

    FAssert(BytevectorP(bv));

    long_t fd = open((const char *) AsBytevector(bv)->Vector, O_RDONLY);
    if (fd < 0)
        return(NoValueObject);

    return(MakeBufferedPort(MakeFileDescInputPort(fn, fd)));
#endif // FOMENT_UNIX
}

FObject OpenInputFile(FObject fn)
{
    FObject port = OpenBinaryInputFile(fn);

    if (BinaryPortP(port))
        return(MakeEncodedPort(port));

    return(port);
}

static FObject OpenBinaryOutputFile(FObject fn)
{
#ifdef FOMENT_WINDOWS
    FObject bv = ConvertStringToUtf16(fn);

    FAssert(BytevectorP(bv));

    HANDLE h = CreateFileW((FCh16 *) AsBytevector(bv)->Vector, GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (h == INVALID_HANDLE_VALUE)
        return(NoValueObject);

    return(MakeBufferedPort(MakeHandleOutputPort(fn, h)));
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    FObject bv = ConvertStringToUtf8(fn);

    FAssert(BytevectorP(bv));

    long_t fd = open((const char *) AsBytevector(bv)->Vector, O_WRONLY | O_TRUNC | O_CREAT,
            S_IRUSR | S_IWUSR);
    if (fd < 0)
        return(NoValueObject);

    return(MakeBufferedPort(MakeFileDescOutputPort(fn, fd)));
#endif // FOMENT_UNIX
}

FObject OpenOutputFile(FObject fn)
{
    FObject port = OpenBinaryOutputFile(fn);

    if (BinaryPortP(port))
        return(MakeEncodedPort(port));

    return(port);
}

FObject OpenInputPipe(FFileHandle fh)
{
#ifdef FOMENT_WINDOWS
    return(MakeTranscodedPort(
            MakeBufferedPort(MakeHandleInputPort(MakeStringC("input-pipe"), fh)),
            LATIN1_CODEC_INDEX, StyleNone, ModeReplace));
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    return(MakeTranscodedPort(
            MakeBufferedPort(MakeFileDescInputPort(MakeStringC("input-pipe"), fh)),
            UTF8_CODEC_INDEX, StyleNone, ModeReplace));
#endif // FOMENT_UNIX
}

FObject OpenOutputPipe(FFileHandle fh)
{
#ifdef FOMENT_WINDOWS
    return(MakeTranscodedPort(
            MakeBufferedPort(MakeHandleOutputPort(MakeStringC("output-pipe"), fh)),
            LATIN1_CODEC_INDEX, StyleNone, ModeReplace));
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    return(MakeTranscodedPort(MakeBufferedPort(
            MakeFileDescOutputPort(MakeStringC("output-pipe"), fh)),
            UTF8_CODEC_INDEX, StyleNone, ModeReplace));
#endif // FOMENT_UNIX
}

static void StringInputCloseInput(FObject port)
{
    FAssert(TextualPortP(port));

    AsGenericPort(port)->Object = NoValueObject;
}

static ulong_t StringInputReadCh(FObject port, FCh * ch)
{
    FAssert(TextualPortP(port) && InputPortOpenP(port));

    FObject s = AsGenericPort(port)->Object;
    ulong_t sdx = (ulong_t) AsGenericPort(port)->Context;

    FAssert(StringP(s));
    FAssert(sdx <= StringLength(s));

    if (sdx == StringLength(s))
        return(0);

    *ch = AsString(s)->String[sdx];
    AsGenericPort(port)->Context = (void *) (sdx + 1);

    return(1);
}

static long_t StringInputCharReadyP(FObject port)
{
    return(1);
}

FObject MakeStringInputPort(FObject s)
{
    FAssert(StringP(s));

    return(MakeTextualPort(NoValueObject, s, 0, StringInputCloseInput, 0, 0, StringInputReadCh,
            StringInputCharReadyP, 0, 0, 0, 0, 0));
}

static void StringOutputCloseOutput(FObject port)
{
    // Nothing.
}

static void StringOutputFlushOutput(FObject port)
{
    // Nothing.
}

static void StringOutputWriteString(FObject port, FCh * s, ulong_t sl)
{
    FAssert(TextualPortP(port) && OutputPortOpenP(port));

    FObject lst = Unbox(AsGenericPort(port)->Object);

    for (ulong_t sdx = 0; sdx < sl; sdx++)
        lst = MakePair(MakeCharacter(s[sdx]), lst);

    SetBox(AsGenericPort(port)->Object, lst);
}

FObject GetOutputString(FObject port)
{
    FAssert(StringOutputPortP(port));

    FObject lst = Unbox(AsGenericPort(port)->Object);
    long_t sl = ListLength("get-output-string", lst);
    FObject s = MakeString(0, sl);
    long_t sdx = sl;

    while (PairP(lst))
    {
        sdx -= 1;

        FAssert(sdx >= 0);
        FAssert(CharacterP(First(lst)));

        AsString(s)->String[sdx] = AsCharacter(First(lst));
        lst = Rest(lst);
    }

    FAssert(lst == EmptyListObject);

    return(s);
}

FObject MakeStringOutputPort()
{
    return(MakeTextualPort(NoValueObject, MakeBox(EmptyListObject), 0, 0, StringOutputCloseOutput,
            StringOutputFlushOutput, 0, 0, StringOutputWriteString, 0, 0, 0,
            PORT_FLAG_STRING_OUTPUT));
}

static void StringCInputCloseInput(FObject port)
{
    // Nothing.
}

static ulong_t StringCInputReadCh(FObject port, FCh * ch)
{
    FAssert(TextualPortP(port));

    char * s = (char *) AsGenericPort(port)->Context;

    if (*s == 0)
        return(0);

    *ch = *s;
    AsGenericPort(port)->Context = (void *) (s + 1);

    return(1);
}

static long_t StringCInputCharReadyP(FObject port)
{
    return(1);
}

FObject MakeStringCInputPort(const char * s)
{
    return(MakeTextualPort(NoValueObject, NoValueObject, (void *) s, StringCInputCloseInput, 0, 0,
            StringCInputReadCh, StringCInputCharReadyP, 0, 0, 0, 0, 0));
}

// ---- Console Input and Output ----

#define CONSOLE_INPUT_EDITLINE 0x00000001
#define CONSOLE_INPUT_ECHO     0x00000002

#define MAXIMUM_POSSIBLE 4096
#define MAXIMUM_HISTORY 64

#ifdef FOMENT_WINDOWS
typedef wchar_t FConCh;
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
typedef unsigned char FConCh;

static FConCh TypeaheadBuffer[128];
static int TypeaheadCount = 0;
static int TypeaheadRear = 0;
#endif // FOMENT_UNIX

typedef struct
{
    FConCh * String;
    long_t Length;
} FHistory;

typedef struct
{
#ifdef FOMENT_WINDOWS
    HANDLE InputHandle;
    HANDLE OutputHandle;
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    long_t InputFd;
    long_t OutputFd;
#endif // FOMENT_UNIX

    ulong_t Mode;

    // Raw Mode

    long_t RawUsed; // Number of available characters which have been used.
    long_t RawAvailable; // Total characters available.
    FConCh RawBuffer[4];

    // EditLine Mode

    long_t Used; // Number of available characters which have been used.
    long_t Available; // Total characters available.

    long_t Point; // Location of the point (cursor) in edit mode.
    long_t Possible; // Total characters in edit mode; when a line is entered, possible
                    // becomes available.
    long_t PreviousPossible;

    long_t EscPrefix;
    long_t EscBracketPrefix;

    int16_t StartX;
    int16_t StartY;
    int16_t Width;
    int16_t Height;

    FConCh Buffer[MAXIMUM_POSSIBLE];

    FHistory History[MAXIMUM_HISTORY];
    long_t HistoryEnd;
    long_t HistoryCurrent;
} FConsoleInput;

#define AsConsoleInput(port) ((FConsoleInput *) (AsGenericPort(port)->Context))

static void ResetConsoleInput(FConsoleInput * ci)
{
    ci->RawUsed = 0;
    ci->RawAvailable = 0;
#ifdef FOMENT_UNIX
    TypeaheadCount = 0;
#endif // FOMENT_UNIX

    ci->Used = 0;
    ci->Available = 0;
    ci->Point = 0;
    ci->Possible = 0;
    ci->PreviousPossible = 0;

    ci->EscPrefix = 0;
    ci->EscBracketPrefix = 0;
}

static FConsoleInput * MakeConsoleInput()
{
    FConsoleInput * ci = (FConsoleInput *) malloc(sizeof(FConsoleInput));
    if (ci == 0)
        return(0);

    ResetConsoleInput(ci);
    ci->HistoryEnd = 0;
    ci->HistoryCurrent = 0;

    for (long_t idx = 0; idx < MAXIMUM_HISTORY; idx++)
        ci->History[idx].String = 0;

    return(ci);
}

static void ConFreeHistory(FConsoleInput * ci)
{
    for (long_t idx = 0; idx < MAXIMUM_HISTORY; idx++)
        if (ci->History[idx].String != 0)
        {
            free(ci->History[idx].String);
            ci->History[idx].String = 0;
        }
}

static void FreeConsoleInput(FConsoleInput * ci)
{
    ConFreeHistory(ci);
    free(ci);
}

static long_t NextHistory(long_t hdx)
{
    return(hdx == MAXIMUM_HISTORY - 1 ? 0 : hdx + 1);
}

static long_t PreviousHistory(long_t hdx)
{
    return(hdx == 0 ? MAXIMUM_HISTORY - 1 : hdx - 1);
}

static void ConAddHistory(FConsoleInput * ci, FConCh * b, long_t bl)
{
    FConCh * s = (FConCh *) malloc(sizeof(FConCh) * bl);
    if (s != 0)
    {
        memcpy(s, b, bl * sizeof(FConCh));

        if (ci->History[ci->HistoryEnd].String != 0)
            free(ci->History[ci->HistoryEnd].String);

        ci->History[ci->HistoryEnd].String = s;
        ci->History[ci->HistoryEnd].Length = bl;

        ci->HistoryEnd = NextHistory(ci->HistoryEnd);
    }
}

static void ConAddHistory(FConsoleInput * ci)
{
    ConAddHistory(ci, ci->Buffer, ci->Possible);
}

static void ConHistoryMove(FConsoleInput * ci, long_t hdx)
{
    if (ci->History[hdx].String != 0)
    {
        FAssert(ci->History[hdx].Length < MAXIMUM_POSSIBLE);

        memcpy(ci->Buffer, ci->History[hdx].String, ci->History[hdx].Length * sizeof(FConCh));
        ci->Possible = ci->History[hdx].Length;
        ci->Point = ci->History[hdx].Length;

        ci->HistoryCurrent = hdx;
    }
}

static void ConHistoryPrevious(FConsoleInput * ci)
{
    FAssert(ci->HistoryCurrent >= 0 && ci->HistoryCurrent < MAXIMUM_HISTORY);

    ConHistoryMove(ci, PreviousHistory(ci->HistoryCurrent));
}

static void ConHistoryNext(FConsoleInput * ci)
{
    FAssert(ci->HistoryCurrent >= 0 && ci->HistoryCurrent < MAXIMUM_HISTORY);

    ConHistoryMove(ci, NextHistory(ci->HistoryCurrent));
}

static FObject ConHistory(FConsoleInput * ci)
{
    FObject lst = EmptyListObject;
    long_t hdx = ci->HistoryEnd;

    for (;;)
    {
        if (ci->History[hdx].String != 0)
        {
#ifdef FOMENT_WINDOWS
            FAssert(sizeof(FCh16) == sizeof(FConCh));

            lst = MakePair(ConvertUtf16ToString(ci->History[hdx].String, ci->History[hdx].Length),
                    lst);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
            lst = MakePair(ConvertUtf8ToString(ci->History[hdx].String, ci->History[hdx].Length),
                    lst);
#endif // FOMENT_UNIX
        }

        hdx = PreviousHistory(hdx);
        if (hdx == ci->HistoryEnd)
            break;
    }

    return(lst);
}

static void ConSetHistory(FConsoleInput * ci, FObject lst)
{
    ConFreeHistory(ci);
    ci->HistoryEnd = 0;

    FObject frm = lst;

    for (;;)
    {
        if (lst == EmptyListObject)
            break;

        if (PairP(lst) == 0 || StringP(First(lst)) == 0)
            RaiseExceptionC(Assertion, "set-console-input-history!",
                    "expected a list of strings", List(frm, lst));

        FObject bv;

#ifdef FOMENT_WINDOWS
        FAssert(sizeof(FCh16) == sizeof(FConCh));

        bv = ConvertStringToUtf16(AsString(First(lst))->String, StringLength(First(lst)), 0, 0);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
        bv = ConvertStringToUtf8(AsString(First(lst))->String, StringLength(First(lst)), 0);
#endif // FOMENT_UNIX

        FAssert(BytevectorP(bv));

        long_t bl = BytevectorLength(bv) / sizeof(FConCh);
        if (bl >= MAXIMUM_POSSIBLE)
            bl = MAXIMUM_POSSIBLE - 1;

        ConAddHistory(ci, (FConCh *) AsBytevector(bv)->Vector, bl);

        lst = Rest(lst);
    }

    ci->HistoryCurrent = ci->HistoryEnd;
}

static void ConSaveHistory(FConsoleInput * ci, FObject fn)
{
    FObject port = OpenOutputFile(fn);

    if (OutputPortP(port))
    {
        try
        {
            WriteSimple(port, ConHistory(ci), 0);
            CloseOutput(port);
        }
        catch (FObject obj)
        {
#ifdef FOMENT_WINDOWS
            ((FObject) obj);
#endif // FOMENT_WINDOWS
        }
    }
}

static void ConLoadHistory(FConsoleInput * ci, FObject fn)
{
    FObject port = OpenInputFile(fn);
    FAlive ap(&port);

    if (InputPortP(port))
    {
        try
        {
            ConSetHistory(ci, Read(port));
            CloseInput(port);
        }
        catch (FObject obj)
        {
#ifdef FOMENT_WINDOWS
            ((FObject) obj);
#endif // FOMENT_WINDOWS
        }
    }
}

static void ConNotifyThrow(FConsoleInput * ci)
{
    ci->RawUsed = 0;
    ci->RawAvailable = 0;
#ifdef FOMENT_UNIX
    TypeaheadCount = 0;
#endif // FOMENT_UNIX

    ci->Used = 0;
    ci->Available = 0;
    ci->Point = 0;
    ci->Possible = 0;
    ci->PreviousPossible = 0;

    ci->EscPrefix = 0;
    ci->EscBracketPrefix = 0;

    ci->HistoryCurrent = ci->HistoryEnd;

    FNotifyThrow nt = {0};

    throw nt;
}

#ifdef FOMENT_WINDOWS
static void ConCloseInput(FObject port)
{
    FAssert(InputPortP(port) && ConsolePortP(port));

    CloseHandle(AsConsoleInput(port)->InputHandle);
    CloseHandle(AsConsoleInput(port)->OutputHandle);

    FreeConsoleInput(AsConsoleInput(port));
}

static ulong_t ConRawEsc(FConsoleInput * ci, FConCh ch1, FConCh ch2)
{
    ci->RawUsed = 0;
    ci->RawAvailable = 3;
    ci->RawBuffer[0] = 27; // Esc
    ci->RawBuffer[1] = ch1;
    ci->RawBuffer[2] = ch2;

    return(1);
}

static ulong_t ConReadRaw(FConsoleInput * ci, long_t wif)
{
    FAssert(ci->RawAvailable == 0);

    INPUT_RECORD ir;

    for (;;)
    {
        for (;;)
        {
            EnterWait();
            DWORD ret = WaitForSingleObjectEx(ci->InputHandle, wif ? INFINITE : 0, TRUE);
            LeaveWait();

            if (ret == WAIT_OBJECT_0)
                break;

            if (ret == WAIT_TIMEOUT)
            {
                FAssert(wif == 0);

                return(1);
            }

            if (ret != WAIT_IO_COMPLETION)
                return(0);

            if (GetThreadState()->NotifyFlag)
                ConNotifyThrow(ci);
        }

        DWORD ne;

        if (ReadConsoleInput(ci->InputHandle, &ir, 1, &ne) == 0)
            return(0);

        if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown)
        {
//printf("[%d %d %x %d]\n", ir.Event.KeyEvent.wRepeatCount, ir.Event.KeyEvent.wVirtualKeyCode,
//        ir.Event.KeyEvent.dwControlKeyState, ir.Event.KeyEvent.uChar.UnicodeChar);

            if (ir.Event.KeyEvent.uChar.UnicodeChar != 0)
                break;

            if (ir.Event.KeyEvent.wVirtualKeyCode == 37) // Left Arrow
                return(ConRawEsc(ci, '[', 'D'));

            if (ir.Event.KeyEvent.wVirtualKeyCode == 38) // Up Arrow
                return(ConRawEsc(ci, '[', 'A'));

            if (ir.Event.KeyEvent.wVirtualKeyCode == 39) // Right Arrow
                return(ConRawEsc(ci, '[', 'C'));

            if (ir.Event.KeyEvent.wVirtualKeyCode == 40) // Down Arrow
                return(ConRawEsc(ci, '[', 'B'));
        }
    }

    FAssert(ir.Event.KeyEvent.uChar.UnicodeChar != 0);

    ci->RawUsed = 0;

    if ((ir.Event.KeyEvent.dwControlKeyState & LEFT_ALT_PRESSED)
            || (ir.Event.KeyEvent.dwControlKeyState & RIGHT_ALT_PRESSED))
    {
        ci->RawAvailable = 2;
        ci->RawBuffer[0] = 27; // Esc
        ci->RawBuffer[1] = ir.Event.KeyEvent.uChar.UnicodeChar;
    }
    else if (ir.Event.KeyEvent.uChar.UnicodeChar == 13) // Carriage Return
    {
        ci->RawAvailable = 1;
        ci->RawBuffer[0] = 10; // Linefeed
    }
    else
    {
        ci->RawAvailable = 1;
        ci->RawBuffer[0] = ir.Event.KeyEvent.uChar.UnicodeChar;
    }

    return(1);
}

static void ConWriteCh(FConsoleInput * ci, FConCh ch)
{
    DWORD nc;
    WriteConsoleW(ci->OutputHandle, &ch, 1, &nc, 0);
}

static void ConGetInfo(FConsoleInput * ci)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    GetConsoleScreenBufferInfo(ci->OutputHandle, &csbi);
    ci->StartX = csbi.dwCursorPosition.X;
    ci->StartY = csbi.dwCursorPosition.Y;
    ci->Width = csbi.dwSize.X;
    ci->Height = csbi.dwSize.Y;
}

static void ConSetCursor(FConsoleInput * ci, long_t x, long_t y)
{
    COORD cp;

    cp.X = (SHORT) x;
    cp.Y = (SHORT) y;
    SetConsoleCursorPosition(ci->OutputHandle, cp);
}

static void ConWriteBuffer(FConsoleInput * ci)
{
    COORD cp;
    DWORD nc;

    cp.X = ci->StartX;
    cp.Y = ci->StartY;
    WriteConsoleOutputCharacterW(ci->OutputHandle, ci->Buffer, (DWORD) ci->Possible, cp, &nc);
}

static void ConFillExtra(FConsoleInput * ci)
{
    COORD cp;
    DWORD nc;

    cp.X = (ci->StartX + ci->Possible) % ci->Width;
    cp.Y = (SHORT) (ci->StartY + (ci->StartX + ci->Possible) / ci->Width);
    FillConsoleOutputCharacterW(ci->OutputHandle, ' ',
            (DWORD) (ci->PreviousPossible - ci->Possible), cp, &nc);
}
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
static void Typeahead(char ch)
{
    int idx = (TypeaheadRear + TypeaheadCount) % sizeof(TypeaheadBuffer);
    TypeaheadBuffer[idx] = ch;
    if (TypeaheadCount < (int) sizeof(TypeaheadBuffer))
        TypeaheadCount += 1;
}

static void ConCloseInput(FObject port)
{
    FAssert(InputPortP(port) && ConsolePortP(port));

    close(AsConsoleInput(port)->InputFd);
    close(AsConsoleInput(port)->OutputFd);

    FreeConsoleInput(AsConsoleInput(port));
}

static void TranslateRaw(FConsoleInput * ci)
{
    for (long_t idx = 0; idx < ci->RawAvailable; idx += 1)
    {
        if (ci->RawBuffer[idx] == 13) // Carriage Return
            ci->RawBuffer[idx] = 10; // Linefeed
        else if (ci->RawBuffer[idx] == 127) // Backspace
            ci->RawBuffer[idx] = 8; // ctrl-h
    }
}

static ulong_t ConReadRaw(FConsoleInput * ci, long_t wif)
{
    FAssert(ci->RawAvailable == 0);

    long_t ret;

    if (TypeaheadCount > 0)
    {
        ret = 0;
        while (TypeaheadCount > 0 && ret < (long_t) sizeof(ci->RawBuffer))
        {
            ci->RawBuffer[ret] = TypeaheadBuffer[TypeaheadRear];
            ret += 1;
            TypeaheadCount -= 1;
            TypeaheadRear = (TypeaheadRear + 1) % sizeof(TypeaheadBuffer);
        }
    }
    else
    {
        for (;;)
            {
                fd_set fds;
                timeval tv;

                FD_ZERO(&fds);
                FD_SET(0, &fds);

                tv.tv_sec = 0;
                tv.tv_usec = 0;

                EnterWait();
                ret = select(1, &fds, 0, 0, wif ? 0 : &tv);
                LeaveWait();

                if (ret > 0)
                    break;

                if (ret == 0)
                    {
                        FAssert(wif == 0);

                        return(1);
                    }

                if (GetThreadState()->NotifyFlag)
                    ConNotifyThrow(ci);
            }

        ret = read(0, ci->RawBuffer, sizeof(ci->RawBuffer));
        if (ret < 1)
            return(1);
    }

    ci->RawUsed = 0;
    ci->RawAvailable = ret;
    TranslateRaw(ci);

    return(1);
}

static void ConWriteCh(FConsoleInput * ci, FConCh ch)
{
    write(1, &ch, sizeof(ch));
}

static void ConSetCursor(FConsoleInput * ci, long_t x, long_t y)
{
    char buf[16];

    sprintf(buf, "\x1B[%d;%dH", (int32_t) y, (int32_t) x);
    write(1, buf, strlen(buf));
}

static long_t ConGetCursor(long_t * x, long_t * y)
{
    char buf[16];

    strcpy(buf, "\x1B[6n");
    write(1, buf, strlen(buf));

    for (;;)
    {
        char ch1, ch2;

        if (read(0, &ch1, 1) != 1)
            return(0);
        if (ch1 == 27)
        {
            if (read(0, &ch2, 1) != 1)
                return(0);
            if (ch2 == '[')
                break;
            Typeahead(ch1);
            Typeahead(ch2);
        }
        else
            Typeahead(ch1);
    }

    *x = 0;
    *y = 0;

    for (;;)
    {
        char ch;

        if (read(0, &ch, 1) != 1)
            return(0);
        if (ch == ';')
            break;
        if (ch < '0' || ch > '9')
            return(0);

        *y = *y * 10 + (ch - '0');
    }

    for (;;)
    {
        char ch;

        if (read(0, &ch, 1) != 1)
            return(0);
        if (ch == 'R')
            break;
        if (ch < '0' || ch > '9')
            return(0);

        *x = *x * 10 + (ch - '0');
    }

    return(1);
}

static void ConGetInfo(FConsoleInput * ci)
{
    long_t x;
    long_t y;

#ifdef FOMENT_DEBUG
    long_t ret;
    ret = ConGetCursor(&x, &y);

    FAssert(ret != 0);
#else // FOMENT_DEBUG
    ConGetCursor(&x, &y);
#endif // FOMENT_DEBUG

    ci->StartX = x;
    ci->StartY = y;

    ConSetCursor(ci, 999, 999);

#ifdef FOMENT_DEBUG
    ret = ConGetCursor(&x, &y);

    FAssert(ret != 0);
#else // FOMENT_DEBUG
    ConGetCursor(&x, &y);
#endif // FOMENT_DEBUG

    ci->Width = x;
    ci->Height = y;

    ConSetCursor(ci, ci->StartX, ci->StartY);
}

static void ConWriteBuffer(FConsoleInput * ci)
{
    ConSetCursor(ci, ci->StartX, ci->StartY);
    write(1, ci->Buffer, ci->Possible);
}

static void ConFillExtra(FConsoleInput * ci)
{
    long_t x = (ci->StartX + ci->Possible) % ci->Width;
    long_t y = (ci->StartY + (ci->StartX + ci->Possible) / ci->Width);

    ConSetCursor(ci, x, y);

    FConCh ch = ' ';
    for (long_t idx = ci->Possible; idx < ci->PreviousPossible; idx++)
        write(1, &ch, 1);
}
#endif // FOMENT_UNIX

static ulong_t ConReadRawCh(FConsoleInput * ci, FCh * ch)
{
    if (ci->RawAvailable == 0)
    {
        if (ConReadRaw(ci, 1) == 0)
            return(0);
        if (ci->RawAvailable == 0)
            return(0);
    }

    FAssert(ci->RawAvailable > 0 && ci->RawUsed < ci->RawAvailable);

    *ch = ci->RawBuffer[ci->RawUsed];
    ci->RawUsed += 1;

    if (ci->RawUsed == ci->RawAvailable)
        ci->RawAvailable = 0;

    return(1);
}

static ulong_t ConRawChReadyP(FConsoleInput * ci)
{
    if (ci->RawUsed < ci->RawAvailable)
        return(1);

    return(ConReadRaw(ci, 0) == 0 || ci->RawUsed < ci->RawAvailable);
}

static void InsertCh(FConsoleInput * ci, FConCh ch)
{
    if (ch == 10 || ci->Possible < MAXIMUM_POSSIBLE - 1)
    {
        for (long_t idx = ci->Possible; idx > ci->Point; idx--)
            ci->Buffer[idx] = ci->Buffer[idx - 1];

        ci->Buffer[ci->Point] = ch;
        ci->Point += 1;
        ci->Possible += 1;
    }
}

static void DeleteCh(FConsoleInput * ci, long_t p)
{
    FAssert(p > 0 && p <= ci->Possible);

    ci->Possible -= 1;

    if (ci->Point >= p)
        ci->Point -= 1;

    for (long_t idx = p - 1; idx < ci->Possible; idx++)
        ci->Buffer[idx] = ci->Buffer[idx + 1];
}

static void Redraw(FConsoleInput * ci)
{
    if (ci->Mode & CONSOLE_INPUT_ECHO)
    {
        while (ci->Possible + ci->StartX + 1 >= ci->Width
                && ((ci->Possible + ci->StartX) / ci->Width) + ci->StartY >= ci->Height)
        {
            ConSetCursor(ci, ci->Width, ci->Height);
            ConWriteCh(ci, '\n');

            FAssert(ci->StartY > 0);

            ci->StartY -= 1;
        }

        ConWriteBuffer(ci);

        if (ci->Possible < ci->PreviousPossible)
            ConFillExtra(ci);

        ConSetCursor(ci, (ci->StartX + ci->Point) % ci->Width,
                ci->StartY + (ci->StartX + ci->Point) / ci->Width);

        ci->PreviousPossible = ci->Possible;
    }
}

static ulong_t ConEditLine(FConsoleInput * ci, long_t wif)
{
    FAssert(ci->Available == 0);

    if (ci->Possible == 0)
        ConGetInfo(ci);

    for (;;)
    {
        long_t rdf = 0;
        FCh ch;

        if (wif == 0 && ConRawChReadyP(ci) == 0)
            return(1);

        if (ConReadRawCh(ci, &ch) == 0)
            return(0);

        if (ci->EscPrefix)
        {
            FAssert(ci->EscBracketPrefix == 0);

            ci->EscPrefix = 0;

            if (ch == '[')
                ci->EscBracketPrefix = 1;
            else if (ch == 'b')
            {
                // backward-word

                if (ci->Point > 0)
                {
                    ci->Point -= 1;

                    while (IdentifierSubsequentP(ci->Buffer[ci->Point]) == 0 && ci->Point > 0)
                        ci->Point -= 1;

                    while (ci->Point > 0 && IdentifierSubsequentP(ci->Buffer[ci->Point - 1]))
                        ci->Point -= 1;

                    rdf = 1;
                }
            }
            else if (ch == 'd')
            {
                // kill-word

                while (ci->Point < ci->Possible
                        && IdentifierSubsequentP(ci->Buffer[ci->Point]) == 0)
                    DeleteCh(ci, ci->Point + 1);

                while (ci->Point < ci->Possible
                        && IdentifierSubsequentP(ci->Buffer[ci->Point]))
                    DeleteCh(ci, ci->Point + 1);

                rdf = 1;
            }
            else if (ch == 'f')
            {
                // forward-word

                if (ci->Point < ci->Possible)
                {
                    ci->Point += 1;

                    while (IdentifierSubsequentP(ci->Buffer[ci->Point]) == 0
                            && ci->Point < ci->Possible)
                        ci->Point += 1;

                    while (ci->Point < ci->Possible
                            && IdentifierSubsequentP(ci->Buffer[ci->Point]))
                        ci->Point += 1;

                    rdf = 1;
                }
            }
        }
        else if (ci->EscBracketPrefix)
        {
            FAssert(ci->EscPrefix == 0);

            ci->EscBracketPrefix = 0;

            if (ch == 'A') // Up Arrow
            {
                // previous-history

                ConHistoryPrevious(ci);
                rdf = 1;
            }
            else if (ch == 'B') // Down Arrow
            {
                // next-history

                ConHistoryNext(ci);
                rdf = 1;
            }
            else if (ch == 'C') // Right Arrow
            {
                // forward-char

                if (ci->Point < ci->Possible)
                {
                    ci->Point += 1;
                    rdf = 1;
                }
            }
            else if (ch == 'D') // Left Arrow
            {
                // backward-char

                if (ci->Point > 0)
                {
                    ci->Point -= 1;
                    rdf = 1;
                }
            }
        }
        else if (ch == 1) // ctrl-a
        {
            // beginning-of-line

            ci->Point = 0;
            rdf = 1;
        }
        else if (ch == 2) // ctrl-b
        {
            // backward-char

            if (ci->Point > 0)
            {
                ci->Point -= 1;
                rdf = 1;
            }
        }
        else if (ch == 4) // ctrl-d
        {
            // delete-char

#ifdef FOMENT_UNIX
            if (ci->Possible == 0)
            {
                FAssert(ci->Point == 0);

                ci->Buffer[ci->Possible] = 4;
                ci->Possible += 1;

                break;
            }
#endif // FOMENT_UNIX

            if (ci->Point < ci->Possible)
            {
                DeleteCh(ci, ci->Point + 1);
                rdf = 1;
            }
        }
        else if (ch == 5) // ctrl-e
        {
            // end-of-line

            ci->Point = ci->Possible;
            rdf = 1;
        }
        else if (ch == 6) // ctrl-f
        {
            // forward-char

            if (ci->Point < ci->Possible)
            {
                ci->Point += 1;
                rdf = 1;
            }
        }
        else if (ch == 8) // ctrl-h
        {
            // delete-backward-char

            if (ci->Point > 0)
            {
                DeleteCh(ci, ci->Point);
                rdf = 1;
            }
        }
        else if (ch == 10) // Linefeed
        {
            ci->Point = ci->Possible;
            Redraw(ci);

            if (ci->Possible > 0)
                ConAddHistory(ci);

            InsertCh(ci, 10); // Linefeed
            ConWriteCh(ci, 10);

            break;
        }
        else if (ch == 11) // ctrl-k
        {
            // kill-line

            ci->Possible = ci->Point;
            rdf = 1;
        }
        else if (ch == 12) // ctrl-l
        {
            // redisplay

            rdf = 1;

            ci->PreviousPossible = MAXIMUM_POSSIBLE;
        }
        else if (ch == 14) // ctrl-n
        {
            // next-history

            ConHistoryNext(ci);
            rdf = 1;
        }
        else if (ch == 16) // ctrl-p
        {
            // previous-history

            ConHistoryPrevious(ci);
            rdf = 1;
        }
#ifdef FOMENT_WINDOWS
        else if (ch == 26) // ctrl-z
        {
            if (ci->Possible == 0)
            {
                FAssert(ci->Point == 0);

                ci->Buffer[ci->Possible] = 26;
                ci->Possible += 1;

                break;
            }
        }
#endif // FOMENT_WINDOWS
        else if (ch == 27) // Esc
            ci->EscPrefix = 1;
        else if (ch == ')')
        {
            InsertCh(ci, ch);

            long_t pt = ci->Point;
            long_t cnt = 0;

            while (ci->Point > 0)
            {
                ci->Point -= 1;

                if (ci->Buffer[ci->Point] == '(')
                {
                    cnt -= 1;

                    if (cnt == 0)
                    {
                        Redraw(ci);

                        time_t t = time(0) + 2;

                        while (t > time(0))
                            if (ConRawChReadyP(ci))
                                break;
                    }
                }
                else if (ci->Buffer[ci->Point] == ')')
                    cnt += 1;
            }

            ci->Point = pt;

            rdf = 1;
        }
        else if (ch >= ' ')
        {
            InsertCh(ci, ch);
            rdf = 1;
        }

        if (rdf)
            Redraw(ci);
    }

    ci->RawUsed = 0;
    ci->RawAvailable = 0;

    ci->Used = 0;
    ci->Available = ci->Possible;
    ci->Point = 0;
    ci->Possible = 0;
    ci->PreviousPossible = 0;

    ci->EscPrefix = 0;
    ci->EscBracketPrefix = 0;

    ci->HistoryCurrent = ci->HistoryEnd;

    return(1);
}

static ulong_t ConReadCh(FObject port, FCh * ch)
{
    FAssert(ConsolePortP(port) && InputPortOpenP(port));

    FConsoleInput * ci = AsConsoleInput(port);

    if (ci->Mode & CONSOLE_INPUT_EDITLINE)
    {
        if (ci->Available == 0)
        {
            if (ConEditLine(ci, 1) == 0)
                return(0);
        }

        FAssert(ci->Available > 0 && ci->Used < ci->Available);

#ifdef FOMENT_WINDOWS
        if (ci->Buffer[ci->Used] == 26) // ctrl-z
            return(0);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
        if (ci->Buffer[ci->Used] == 4) // ctrl-d
            return(0);
#endif // FOMENT_UNIX

        *ch = ci->Buffer[ci->Used];
        ci->Used += 1;

        if (ci->Used == ci->Available)
            ci->Available = 0;

        return(1);
    }

    if (ConReadRawCh(ci, ch) == 0)
        return(0);

#ifdef FOMENT_WINDOWS
    if (*ch == 26) // ctrl-z
        return(0);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    if (*ch == 4) // ctrl-d
        return(0);
#endif // FOMENT_UNIX

    if (ci->Mode & CONSOLE_INPUT_ECHO)
        ConWriteCh(ci, *ch);

    return(1);
}

static long_t ConCharReadyP(FObject port)
{
    FAssert(ConsolePortP(port) && InputPortOpenP(port));

    FConsoleInput * ci = AsConsoleInput(port);

    if (ci->Mode & CONSOLE_INPUT_EDITLINE)
    {
        if (ci->Used < ci->Available)
            return(1);

        return(ConEditLine(ci, 0) == 0 || ci->Used < ci->Available);
    }

    return(ConRawChReadyP(ci));
}

#ifdef FOMENT_WINDOWS
static FFileHandle ConGetFileHandle(FObject port)
{
    FAssert(InputPortP(port) && ConsolePortP(port));

    return(AsConsoleInput(port)->InputHandle);
}

static FObject MakeConsoleInputPort(FObject nam, HANDLE hin, HANDLE hout)
{
    FConsoleInput * ci = MakeConsoleInput();
    if (ci == 0)
        return(NoValueObject);

    ci->InputHandle = hin;
    ci->OutputHandle = hout;

    FObject port = MakeTextualPort(nam, NoValueObject, ci, ConCloseInput, 0, 0, ConReadCh,
            ConCharReadyP, 0, 0, 0, ConGetFileHandle, PORT_FLAG_CONSOLE | PORT_FLAG_INTERACTIVE);
    AsConsoleInput(port)->Mode = CONSOLE_INPUT_ECHO;

    return(port);
}

static void ConCloseOutput(FObject port)
{
    FAssert(TextualPortP(port));

    CloseHandle((HANDLE) AsGenericPort(port)->Context);
}

static void ConFlushOutput(FObject port)
{
    // Nothing.
}

static void ConWriteString(FObject port, FCh * s, ulong_t sl)
{
    FAssert(TextualPortP(port) && OutputPortOpenP(port));

    FObject bv = ConvertStringToUtf16(s, sl, 0, 0);
    DWORD nc;

    FAssert(BytevectorP(bv));

    WriteConsoleW((HANDLE) AsGenericPort(port)->Context,
            (FCh16 *) AsBytevector(bv)->Vector, (DWORD) BytevectorLength(bv) / sizeof(FCh16),
            &nc, 0);
}

static FObject MakeConsoleOutputPort(FObject nam, HANDLE h)
{
    return(MakeTextualPort(nam, NoValueObject, h, 0, ConCloseOutput, ConFlushOutput, 0,
            0, ConWriteString, 0, 0, HandleGetFileHandle, PORT_FLAG_CONSOLE));
}
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
static FFileHandle ConGetFileHandle(FObject port)
{
    FAssert(InputPortP(port) && ConsolePortP(port));

    return(AsConsoleInput(port)->InputFd);
}

static FObject MakeConsoleInputPort(FObject nam, long_t ifd, long_t ofd, int fi)
{
    FConsoleInput * ci = MakeConsoleInput();
    if (ci == 0)
        return(NoValueObject);

    ci->InputFd = ifd;
    ci->OutputFd = ofd;

    FObject port = MakeTextualPort(nam, NoValueObject, ci, ConCloseInput, 0, 0, ConReadCh,
            ConCharReadyP, 0, 0, 0, ConGetFileHandle, PORT_FLAG_CONSOLE);
    if (fi)
    {
        AsGenericPort(port)->Flags |= PORT_FLAG_INTERACTIVE;
        AsConsoleInput(port)->Mode = CONSOLE_INPUT_ECHO;
    }

    return(port);
}

static void ConCloseOutput(FObject port)
{
    FAssert(TextualPortP(port));

    close((long_t) AsGenericPort(port)->Context);
}

static void ConFlushOutput(FObject port)
{
    // Nothing.
}

static void ConWriteString(FObject port, FCh * s, ulong_t sl)
{
    FAssert(TextualPortP(port) && OutputPortOpenP(port));

    FObject bv = ConvertStringToUtf8(s, sl, 0);

    FAssert(BytevectorP(bv));

    write((long_t) AsGenericPort(port)->Context, AsBytevector(bv)->Vector,
            BytevectorLength(bv));
}

static FObject MakeConsoleOutputPort(FObject nam, long_t ofd)
{
    return(MakeTextualPort(nam, NoValueObject, (void *) ofd, 0, ConCloseOutput,
            ConFlushOutput, 0, 0, ConWriteString, 0, 0, FileDescGetFileHandle, PORT_FLAG_CONSOLE));
}
#endif // FOMENT_UNIX

// ---- Input and output ----

Define("input-port?", InputPortPPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("input-port?", argc);

    return(InputPortP(argv[0]) ? TrueObject : FalseObject);
}

Define("output-port?", OutputPortPPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("output-port?", argc);

    return(OutputPortP(argv[0]) ? TrueObject : FalseObject);
}

Define("textual-port?", TextualPortPPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("textual-port?", argc);

    return(TextualPortP(argv[0]) ? TrueObject : FalseObject);
}

Define("binary-port?", BinaryPortPPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("binary-port?", argc);

    return(BinaryPortP(argv[0]) ? TrueObject : FalseObject);
}

Define("port?", PortPPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("port?", argc);

    return((TextualPortP(argv[0]) || BinaryPortP(argv[0])) ? TrueObject : FalseObject);
}

Define("input-port-open?", InputPortOpenPPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("input-port-open?", argc);
    PortArgCheck("input-port-open?", argv[0]);

    return(InputPortOpenP(argv[0]) ? TrueObject : FalseObject);
}

Define("output-port-open?", OutputPortOpenPPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("output-port-open?", argc);
    PortArgCheck("output-port-open?", argv[0]);

    return(OutputPortOpenP(argv[0]) ? TrueObject : FalseObject);
}

Define("open-binary-input-file", OpenBinaryInputFilePrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("open-binary-input-file", argc);
    StringArgCheck("open-binary-input-file", argv[0]);

    FObject port = OpenBinaryInputFile(argv[0]);

    if (BinaryPortP(port) == 0)
        RaiseExceptionC(Assertion, "open-binary-input-file", FileErrorSymbol,
                "unable to open file for input", List(argv[0]));

    return(port);
}

Define("open-binary-output-file", OpenBinaryOutputFilePrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("open-binary-output-file", argc);
    StringArgCheck("open-binary-output-file", argv[0]);

    FObject port = OpenBinaryOutputFile(argv[0]);

    if (BinaryPortP(port) == 0)
        RaiseExceptionC(Assertion, "open-binary-output-file", FileErrorSymbol,
                "unable to open file for output", List(argv[0]));

    return(port);
}

Define("close-port", ClosePortPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("close-port", argc);
    PortArgCheck("close-port", argv[0]);

    CloseInput(argv[0]);
    CloseOutput(argv[0]);

    return(NoValueObject);
}

Define("close-input-port", CloseInputPortPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("close-input-port", argc);
    InputPortArgCheck("close-input-port", argv[0]);

    CloseInput(argv[0]);

    return(NoValueObject);
}

Define("close-output-port", CloseOutputPortPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("close-output-port", argc);
    OutputPortArgCheck("close-output-port", argv[0]);

    CloseOutput(argv[0]);

    return(NoValueObject);
}

Define("open-input-string", OpenInputStringPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("open-input-string", argc);
    StringArgCheck("open-input-string", argv[0]);

    return(MakeStringInputPort(argv[0]));
}

Define("open-output-string", OpenOutputStringPrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("open-output-string", argc);

    return(MakeStringOutputPort());
}

Define("get-output-string", GetOutputStringPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("get-output-string", argc);
    StringOutputPortArgCheck("get-output-string", argv[0]);

    return(GetOutputString(argv[0]));
}

Define("open-input-bytevector", OpenInputBytevectorPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("open-input-bytevector", argc);
    BytevectorArgCheck("open-input-bytevector", argv[0]);

    return(MakeBytevectorInputPort(argv[0]));
}

Define("open-output-bytevector", OpenOutputBytevectorPrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("open-output-bytevector", argc);

    return(MakeBytevectorOutputPort());
}

Define("get-output-bytevector", GetOutputBytevectorPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("get-output-bytevector", argc);
    BytevectorOutputPortArgCheck("get-output-bytevector", argv[0]);

    return(GetOutputBytevector(argv[0]));
}

Define("%make-transcoder", MakeTranscoderPrimitive)(long_t argc, FObject argv[])
{
    FMustBe(argc == 3);

    if (CodecP(argv[0]) == 0)
        RaiseExceptionC(Assertion, "make-transcoder", "expected a codec", List(argv[0]));

    FMustBe(FixnumP(argv[1]));
    FMustBe(FixnumP(argv[2]));

    FEOLStyle style = (FEOLStyle) AsFixnum(argv[1]);
    FErrorMode mode = (FErrorMode) AsFixnum(argv[2]);

    FMustBe(style == StyleNone || style == StyleLF || style == StyleCRLF);
    FMustBe(mode == ModeReplace || mode == ModeRaise);

    return(MakeTranscoder(argv[0], style, mode));
}

Define("transcoded-port", TranscodedPortPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("transcoded-port", argc);
    BinaryPortArgCheck("transcoded-port", argv[0]);

    if (TranscoderP(argv[1]) == 0)
        RaiseExceptionC(Assertion, "transcoded-port", "expected a transcoder", List(argv[1]));

    FObject codec = AsTranscoder(argv[1])->Codec;

    FAssert(CodecP(codec));

    return(MakeTranscodedPort(argv[0], AsCodec(codec)->Index, AsTranscoder(argv[1])->Style,
            AsTranscoder(argv[1])->Mode));
}

Define("make-buffered-port", MakeBufferedPortPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("make-buffered-port", argc);
    BinaryPortArgCheck("make-buffered-port", argv[0]);

    return(MakeBufferedPort(argv[0]));
}

Define("make-encoded-port", MakeEncodedPortPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("make-encoded-port", argc);
    BinaryPortArgCheck("make-encoded-port", argv[0]);

    return(MakeEncodedPort(argv[0]));
}

Define("want-identifiers", WantIdentifiersPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("want-identifiers", argc);
    TextualInputPortArgCheck("want-identifiers", argv[0]);

    WantIdentifiersPort(argv[0], argv[1] == FalseObject ? 0 : 1);
    return(NoValueObject);
}

Define("console-port?", ConsolePortPPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("console-port?", argc);

    return(ConsolePortP(argv[0]) ? TrueObject : FalseObject);
}

Define("interactive-console-port?", InteractiveConsolePortPPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("interactive-console-port?", argc);

    return(InteractiveConsolePortP(argv[0]) ? TrueObject : FalseObject);
}

Define("set-console-input-editline!", SetConsoleInputEditlinePrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("set-console-input-editline!", argc);
    InteractiveConsoleInputPortArgCheck("set-console-input-editline!", argv[0]);
    BooleanArgCheck("set-console-input-editline!", argv[1]);

    if (argv[1] == TrueObject)
        AsConsoleInput(argv[0])->Mode |= CONSOLE_INPUT_EDITLINE;
    else
        AsConsoleInput(argv[0])->Mode &= ~CONSOLE_INPUT_EDITLINE;

    ResetConsoleInput(AsConsoleInput(argv[0]));

    return(NoValueObject);
}

Define("set-console-input-echo!", SetConsoleInputEchoPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("set-console-input-echo!", argc);
    ConsoleInputPortArgCheck("set-console-input-echo!", argv[0]);
    BooleanArgCheck("set-console-input-echo!", argv[1]);

    if (argv[1] == TrueObject)
        AsConsoleInput(argv[0])->Mode |= CONSOLE_INPUT_ECHO;
    else
        AsConsoleInput(argv[0])->Mode &= ~CONSOLE_INPUT_ECHO;

    return(NoValueObject);
}

Define("%save-history", SaveHistoryPrimitive)(long_t argc, FObject argv[])
{
    FMustBe(argc == 2);
    FMustBe(ConsolePortP(argv[0]) && InputPortOpenP(argv[0]));
    FMustBe(StringP(argv[1]));

    ConSaveHistory(AsConsoleInput(argv[0]), argv[1]);
    return(NoValueObject);
}

Define("%load-history", LoadHistoryPrimitive)(long_t argc, FObject argv[])
{
    FMustBe(argc == 2);
    FMustBe(ConsolePortP(argv[0]) && InputPortOpenP(argv[0]));
    FMustBe(StringP(argv[1]));

    ConLoadHistory(AsConsoleInput(argv[0]), argv[1]);
    return(NoValueObject);
}

Define("positioning-port?", PositioningPortPPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("positioning-port?", argc);
    PortArgCheck("positioning-port?", argv[0]);

    return(PositioningPortP(argv[0]) ? TrueObject : FalseObject);
}

Define("port-position", PortPositionPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("port-position", argc);
    OpenPositioningPortArgCheck("port-position", argv[0]);

    return(MakeIntegerFromInt64(GetPosition(argv[0])));
}

Define("set-port-position!", SetPortPositionPrimitive)(long_t argc, FObject argv[])
{
    TwoOrThreeArgsCheck("set-port-position!", argc);
    OpenPositioningPortArgCheck("set-port-position!", argv[0]);
    FixnumArgCheck("set-port-position!", argv[1]);

    FPositionFrom frm = FromBegin;
    if (argc == 3)
    {
        if (argv[2] == BeginSymbol)
            frm = FromBegin;
        else if (argv[2] == CurrentSymbol)
            frm = FromCurrent;
        else if (argv[2] == EndSymbol)
            frm = FromEnd;
        else
            RaiseExceptionC(Assertion, "set-port-position!",
                    "expected begin, current, or end", List(argv[2]));
    }

    if (SetPosition(argv[0], AsFixnum(argv[1]), frm) == 0)
        RaiseExceptionC(Assertion, "set-port-position!", PositionErrorSymbol,
                "unable to set port position", List(argv[0], argv[1], argv[2]));

    return(NoValueObject);
}

Define("%copy-port", CopyPortPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("%copy-port", argc);
    InputPortArgCheck("%copy-port", argv[0]);
    if (TextualPortP(argv[0]))
    {
        TextualInputPortArgCheck("%copy-port", argv[0]);
        TextualOutputPortArgCheck("%copy-port", argv[1]);

        for (;;)
        {
            FCh ch;

            if (ReadCh(argv[0], &ch) == 0)
                break;
            WriteCh(argv[1], ch);
        }
    }
    else
    {
        FAssert(BinaryPortP(argv[0]));

        BinaryInputPortArgCheck("%copy-port", argv[0]);
        BinaryOutputPortArgCheck("%copy-port", argv[1]);

        for (;;)
        {
            FByte buf[1024];
            ulong_t bl = ReadBytes(argv[0], buf, sizeof(buf));
            if (bl == 0)
                break;
            WriteBytes(argv[1], buf, bl);
        }
    }

    return(NoValueObject);
}

Define("make-custom-binary-input-port",
    MakeCustomBinaryInputPortPrimitive)(long_t argc, FObject argv[])
{
    FiveArgsCheck("make-custom-binary-input-port", argc);
    ProcedureArgCheck("make-custom-binary-input-port", argv[1]); // read!
    if (argv[2] != FalseObject) // get-position
        ProcedureArgCheck("make-custom-binary-input-port", argv[2]);
    if (argv[3] != FalseObject) // set-position!
        ProcedureArgCheck("make-custom-binary-input-port", argv[3]);
    if (argv[4] != FalseObject) // close
        ProcedureArgCheck("make-custom-binary-input-port", argv[4]);

    return(MakeCustomBinaryPort(argv[0], argv[1], FalseObject, argv[2], argv[3], argv[4],
            FalseObject));
}

Define("make-custom-textual-input-port",
    MakeCustomTextualInputPortPrimitive)(long_t argc, FObject argv[])
{
    FiveArgsCheck("make-custom-textual-input-port", argc);
    ProcedureArgCheck("make-custom-textual-input-port", argv[1]); // read!
    if (argv[2] != FalseObject) // get-position
        ProcedureArgCheck("make-custom-textual-input-port", argv[2]);
    if (argv[3] != FalseObject) // set-position!
        ProcedureArgCheck("make-custom-textual-input-port", argv[3]);
    if (argv[4] != FalseObject) // close
        ProcedureArgCheck("make-custom-textual-input-port", argv[4]);

    return(MakeCustomTextualPort(argv[0], argv[1], FalseObject, argv[2], argv[3], argv[4],
            FalseObject));
}

Define("make-custom-binary-output-port",
    MakeCustomBinaryOutputPortPrimitive)(long_t argc, FObject argv[])
{
    if (argc < 5 || argc > 6)
        RaiseExceptionC(Assertion, "make-custom-binary-output-port",
                "expected five or six arguments", EmptyListObject);

    ProcedureArgCheck("make-custom-binary-output-port", argv[1]); // write!
    if (argv[2] != FalseObject) // get-position
        ProcedureArgCheck("make-custom-binary-output-port", argv[2]);
    if (argv[3] != FalseObject) // set-position!
        ProcedureArgCheck("make-custom-binary-output-port", argv[3]);
    if (argv[4] != FalseObject) // close
        ProcedureArgCheck("make-custom-binary-output-port", argv[4]);
    if (argc == 6 && argv[5] != FalseObject) // flush
        ProcedureArgCheck("make-custom-binary-output-port", argv[5]);

    return(MakeCustomBinaryPort(argv[0], FalseObject, argv[1], argv[2], argv[3], argv[4],
            argc == 6 ? argv[5] : FalseObject));
}

Define("make-custom-textual-output-port",
    MakeCustomTextualOutputPortPrimitive)(long_t argc, FObject argv[])
{
    if (argc < 5 || argc > 6)
        RaiseExceptionC(Assertion, "make-custom-textual-output-port",
                "expected five or six arguments", EmptyListObject);

    ProcedureArgCheck("make-custom-textual-output-port", argv[1]); // write!
    if (argv[2] != FalseObject) // get-position
        ProcedureArgCheck("make-custom-textual-output-port", argv[2]);
    if (argv[3] != FalseObject) // set-position!
        ProcedureArgCheck("make-custom-textual-output-port", argv[3]);
    if (argv[4] != FalseObject) // close
        ProcedureArgCheck("make-custom-textual-output-port", argv[4]);
    if (argc == 6 && argv[5] != FalseObject) // flush
        ProcedureArgCheck("make-custom-textual-output-port", argv[5]);

    return(MakeCustomTextualPort(argv[0], FalseObject, argv[1], argv[2], argv[3], argv[4],
            argc == 6 ? argv[5] : FalseObject));
}

Define("make-custom-binary-input/output-port",
    MakeCustomBinaryInputOutputPortPrimitive)(long_t argc, FObject argv[])
{
    if (argc < 6 || argc > 7)
        RaiseExceptionC(Assertion, "make-custom-binary-input/output-port",
                "expected six or seven arguments", EmptyListObject);

    ProcedureArgCheck("make-custom-binary-input/output-port", argv[1]); // read!
    ProcedureArgCheck("make-custom-binary-input/output-port", argv[2]); // write!
    if (argv[3] != FalseObject) // get-position
        ProcedureArgCheck("make-custom-binary-input/output-port", argv[3]);
    if (argv[4] != FalseObject) // set-position!
        ProcedureArgCheck("make-custom-binary-input/output-port", argv[4]);
    if (argv[5] != FalseObject) // close
        ProcedureArgCheck("make-custom-binary-input/output-port", argv[5]);
    if (argc == 7 && argv[6] != FalseObject) // flush
        ProcedureArgCheck("make-custom-binary-input/output-port", argv[6]);

    return(MakeCustomBinaryPort(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
            argc == 7 ? argv[6] : FalseObject));
}

Define("make-custom-textual-input/output-port",
    MakeCustomTextualInputOutputPortPrimitive)(long_t argc, FObject argv[])
{
    if (argc < 6 || argc > 7)
        RaiseExceptionC(Assertion, "make-custom-textual-input/output-port",
                "expected six or seven arguments", EmptyListObject);

    ProcedureArgCheck("make-custom-textual-input/output-port", argv[1]); // read!
    ProcedureArgCheck("make-custom-textual-input/output-port", argv[2]); // write!
    if (argv[3] != FalseObject) // get-position
        ProcedureArgCheck("make-custom-textual-input/output-port", argv[3]);
    if (argv[4] != FalseObject) // set-position!
        ProcedureArgCheck("make-custom-textual-input/output-port", argv[4]);
    if (argv[5] != FalseObject) // close
        ProcedureArgCheck("make-custom-textual-input/output-port", argv[5]);
    if (argc == 7 && argv[6] != FalseObject) // flush
        ProcedureArgCheck("make-custom-textual-input/output-port", argv[6]);

    return(MakeCustomTextualPort(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
            argc == 7 ? argv[6] : FalseObject));
}

Define("socket-merge-flags", SocketMergeFlagsPrimitive)(long_t argc, FObject argv[])
{
    long_t ret = 0;

    for (long_t adx = 0; adx < argc; adx++)
    {
        FixnumArgCheck("socket-merge-flags", argv[adx]);

        ret |= AsFixnum(argv[adx]);
    }

    return(MakeFixnum(ret));
}

Define("socket-purge-flags", SocketPurgeFlagsPrimitive)(long_t argc, FObject argv[])
{
    AtLeastOneArgCheck("socket-purge-flags", argc);
    FixnumArgCheck("socket-purge-flags", argv[0]);

    long_t ret = AsFixnum(argv[0]);

    for (long_t adx = 1; adx < argc; adx++)
    {
        FixnumArgCheck("socket-purge-flags", argv[adx]);

        ret &= ~AsFixnum(argv[adx]);
    }

    return(MakeFixnum(ret));
}

#ifdef FOMENT_WINDOWS
static FObject LastSocketError()
{
    return(MakeFixnum(WSAGetLastError()));
}
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
static FObject LastSocketError()
{
    return(MakeStringC(strerror(errno)));
}
#endif // FOMENT_UNIX

Define("socket?", SocketPPrimitive)(long_t argc, FObject argv[])
{
    // (socket? <obj>)

    OneArgCheck("socket?", argc);

    return((BinaryPortP(argv[0]) && (AsGenericPort(argv[0])->Flags & PORT_FLAG_SOCKET))
            ? TrueObject : FalseObject);
}

Define("make-socket", MakeSocketPrimitive)(long_t argc, FObject argv[])
{
    // (make-socket <address-family> <socket-domain> <protocol>)

    ThreeArgsCheck("make-socket", argc);
    FixnumArgCheck("make-socket", argv[0]);
    FixnumArgCheck("make-socket", argv[1]);
    FixnumArgCheck("make-socket", argv[2]);

    SOCKET s = socket((int) AsFixnum(argv[0]), (int) AsFixnum(argv[1]), (int) AsFixnum(argv[2]));
    if (s == INVALID_SOCKET)
        RaiseExceptionC(Assertion, "make-socket", "creating a socket failed",
                List(argv[0], argv[1], argv[2], LastSocketError()));

    return(MakeSocketPort(s));
}

static void GetAddressInformation(const char * who, addrinfoW ** res, FObject node,
    FObject svc, FObject afam, FObject sdmn, FObject flgs, FObject prot)
{
    StringArgCheck(who, node);
    StringArgCheck(who, svc);
    FixnumArgCheck(who, afam);
    FixnumArgCheck(who, sdmn);
    FixnumArgCheck(who, flgs);
    FixnumArgCheck(who, prot);

    addrinfoW hts;

    memset(&hts, 0, sizeof(hts));
    hts.ai_family = (int) AsFixnum(afam);
    hts.ai_socktype = (int) AsFixnum(sdmn);
    hts.ai_protocol = (int) AsFixnum(prot);
    hts.ai_flags = AI_PASSIVE | (int) AsFixnum(flgs);

#ifdef FOMENT_WINDOWS
    FObject nn = ConvertStringToUtf16(node);
    FObject sn = ConvertStringToUtf16(svc);

    if (GetAddrInfoW((FCh16 *) AsBytevector(nn)->Vector, (FCh16 *) AsBytevector(sn)->Vector,
            &hts, res) != 0)
        RaiseExceptionC(Assertion, who, "GetAddrInfoW failed",
                List(node, svc, afam, sdmn, prot, LastSocketError()));
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    FObject nn = ConvertStringToUtf8(node);
    FObject sn = ConvertStringToUtf8(svc);

    if (getaddrinfo(StringLength(node) == 0 ? 0 : (char *) AsBytevector(nn)->Vector,
            StringLength(svc) == 0 ? 0 : (char *) AsBytevector(sn)->Vector,
            &hts, res) != 0)
        RaiseExceptionC(Assertion, who, "GetAddrInfoW failed",
                List(node, svc, afam, sdmn, prot, LastSocketError()));
#endif // FOMENT_UNIX
}

Define("bind-socket", BindSocketPrimitive)(long_t argc, FObject argv[])
{
    // (bind-socket <socket> <node> <service> <address-family> <socket-domain> <protocol>)

    SixArgsCheck("bind-socket", argc);
    SocketPortArgCheck("bind-socket", argv[0]);

    addrinfoW * res;
    GetAddressInformation("bind-socket", &res, argv[1], argv[2], argv[3], argv[4],
            MakeFixnum(0), argv[5]);

    if (bind((SOCKET) AsGenericPort(argv[0])->Context, res->ai_addr, (int) res->ai_addrlen) != 0)
        RaiseExceptionC(Assertion, "bind-socket", "bind failed",
                List(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                LastSocketError()));

    return(NoValueObject);
}

Define("listen-socket", ListenSocketPrimitive)(long_t argc, FObject argv[])
{
    // (listen-socket <socket> [<backlog>])

    OneOrTwoArgsCheck("listen-socket", argc);
    SocketPortArgCheck("listen-socket", argv[0]);

    int bcklg = SOMAXCONN;
    if (argc == 2)
    {
        NonNegativeArgCheck("listen-socket", argv[1], 0);

        bcklg = (int) AsFixnum(argv[1]);
    }

    if (listen((SOCKET) AsGenericPort(argv[0])->Context, bcklg) != 0)
        RaiseExceptionC(Assertion, "listen-socket", "listen failed",
                List(LastSocketError()));

    return(NoValueObject);
}

Define("accept-socket", AcceptSocketPrimitive)(long_t argc, FObject argv[])
{
    // (accept-socket <socket>)

    OneArgCheck("accept-socket", argc);
    SocketPortArgCheck("accept-socket", argv[0]);

    SOCKET ls = (SOCKET) AsGenericPort(argv[0])->Context;

    EnterWait();
    SOCKET s = accept(ls, 0, 0);
    LeaveWait();

    if (s == INVALID_SOCKET)
        RaiseExceptionC(Assertion, "accept-socket", "accept failed",
                List(LastSocketError()));

    return(MakeSocketPort(s));
}

Define("connect-socket", ConnectSocketPrimitive)(long_t argc, FObject argv[])
{
    // (connect-socket <socket> <node> <service> <address-family> <socket-domain>
    //         <flags> <protocol>)

    SevenArgsCheck("connect-socket", argc);
    SocketPortArgCheck("connect-socket", argv[0]);

    addrinfoW * res;
    GetAddressInformation("connect-socket", &res, argv[1], argv[2], argv[3], argv[4], argv[5],
            argv[6]);

    SOCKET s = (SOCKET) AsGenericPort(argv[0])->Context;

    EnterWait();
    long_t ret = connect(s, res->ai_addr, (int) res->ai_addrlen);
    LeaveWait();

    if (ret != 0)
        RaiseExceptionC(Assertion, "connect-socket", "connect failed",
                List(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5],
                LastSocketError()));

    return(NoValueObject);
}

Define("shutdown-socket", ShutdownSocketPrimitive)(long_t argc, FObject argv[])
{
    // (shutdown-socket <socket> <how>)

    TwoArgsCheck("shutdown-socket", argc);
    SocketPortArgCheck("shutdown-socket", argv[0]);
    FixnumArgCheck("shutdown-socket", argv[1]);

    if (shutdown((SOCKET) AsGenericPort(argv[0])->Context, (int) AsFixnum(argv[1])) != 0)
        RaiseExceptionC(Assertion, "shutdown-socket", "shutdown failed",
                List(argv[0], argv[1], LastSocketError()));

    return(NoValueObject);
}

Define("send-socket", SendSocketPrimitive)(long_t argc, FObject argv[])
{
    // (send-socket <socket> <bv> <flags>)

    ThreeArgsCheck("send-socket", argc);
    SocketPortArgCheck("send-socket", argv[0]);
    BytevectorArgCheck("send-socket", argv[1]);
    FixnumArgCheck("send-socket", argv[2]);

    SocketSend((SOCKET) AsGenericPort(argv[0])->Context, (char *) AsBytevector(argv[1])->Vector,
            (int) BytevectorLength(argv[1]), (int) AsFixnum(argv[2]));
    return(NoValueObject);
}

Define("recv-socket", ReceiveSocketPrimitive)(long_t argc, FObject argv[])
{
    // (recv-socket <socket> <size> <flags>)

    ThreeArgsCheck("recv-socket", argc);
    SocketPortArgCheck("recv-socket", argv[0]);
    NonNegativeArgCheck("recv-socket", argv[1], 0);
    FixnumArgCheck("recv-socket", argv[2]);

    int bvl = (int) AsFixnum(argv[1]);
    char b[128];
    char * ptr;
    if (bvl <= (int) sizeof(b))
        ptr = b;
    else
    {
        ptr = (char *) malloc(bvl);
        if (ptr == 0)
            RaiseExceptionC(Restriction, "recv-socket", "insufficient memory",
                    List(argv[0]));
    }

    int br = SocketReceive((SOCKET) AsGenericPort(argv[0])->Context, ptr, bvl,
            (int) AsFixnum(argv[2]));

    FObject bv = MakeBytevector(br);
    memcpy(AsBytevector(bv)->Vector, ptr, br);

    if (ptr != b)
        free(ptr);

    return(bv);
}

Define("get-ip-addresses", GetIpAddressesPrimitive)(long_t argc, FObject argv[])
{
    // (get-ip-addresses <address-family>)

    OneArgCheck("get-ip-addresses", argc);
    FixnumArgCheck("get-ip-addresses", argv[0]);

#ifdef FOMENT_WINDOWS
    ULONG sz = 1024 * 15;
    int cnt = 0;
    IP_ADAPTER_ADDRESSES * iaabuf;
    FObject lst = EmptyListObject;

    for (;;)
    {
        iaabuf = (IP_ADAPTER_ADDRESSES *) malloc(sz);
        if (iaabuf == 0)
            RaiseExceptionC(Assertion, "get-ip-addresses", "unable to allocate memory",
                    EmptyListObject);

        ULONG ret = GetAdaptersAddresses((ULONG) AsFixnum(argv[0]), 0, 0, iaabuf, &sz);
        if (ret == ERROR_BUFFER_OVERFLOW && cnt < 2)
        {
            free(iaabuf);

            FAssert(sz > 1024 * 15);
        }
        else if (ret == ERROR_SUCCESS)
            break;
        else
            RaiseExceptionC(Assertion, "get-ip-addresses", "GetAdaptersAddresses failed",
                    List(MakeFixnum(ret)));

        cnt += 1;
    }

    for (IP_ADAPTER_ADDRESSES * iaa = iaabuf; iaa != 0; iaa = iaa->Next)
    {
        if (iaa->OperStatus == IfOperStatusUp)
        {
            for (IP_ADAPTER_UNICAST_ADDRESS * iaua = iaa->FirstUnicastAddress; iaua != 0;
                    iaua = iaua->Next)
            {
                if (iaua->Address.lpSockaddr->sa_family == AF_INET)
                {
                    char buf[46];

                    lst = MakePair(MakeStringC(InetNtop(AF_INET,
                            &((struct sockaddr_in *) iaua->Address.lpSockaddr)->sin_addr, buf, 46)),
                            lst);
                }
                else if (iaua->Address.lpSockaddr->sa_family == AF_INET6)
                {
                    char buf[46];

                    lst = MakePair(MakeStringC(InetNtop(AF_INET6,
                            &((struct sockaddr_in6 *) iaua->Address.lpSockaddr)->sin6_addr, buf,
                            46)), lst);
                }
            }
        }
    }

    free(iaabuf);

    return(lst);
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    struct ifaddrs * ifab;

    if (getifaddrs(&ifab) != 0)
        RaiseExceptionC(Assertion, "get-ip-addresses", "getifaddrs failed",
                List(LastSocketError()));

    FObject lst = EmptyListObject;

    for (struct ifaddrs * ifa = ifab; ifa != 0; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr != 0 && (ifa->ifa_addr->sa_family == AF_INET
                || ifa->ifa_addr->sa_family == AF_INET6))
        {
            char buf[46];

            if (AsFixnum(argv[0]) == AF_UNSPEC || ifa->ifa_addr->sa_family == AsFixnum(argv[0]))
                lst = MakePair(MakeStringC(inet_ntop(ifa->ifa_addr->sa_family,
                        ifa->ifa_addr, buf, 46)), lst);
        }
    }

    freeifaddrs(ifab);

    return(lst);
#endif // FOMENT_UNIX
}

static FObject Primitives[] =
{
    InputPortPPrimitive,
    OutputPortPPrimitive,
    TextualPortPPrimitive,
    BinaryPortPPrimitive,
    PortPPrimitive,
    InputPortOpenPPrimitive,
    OutputPortOpenPPrimitive,
    OpenBinaryInputFilePrimitive,
    OpenBinaryOutputFilePrimitive,
    ClosePortPrimitive,
    CloseInputPortPrimitive,
    CloseOutputPortPrimitive,
    OpenInputStringPrimitive,
    OpenOutputStringPrimitive,
    GetOutputStringPrimitive,
    OpenInputBytevectorPrimitive,
    OpenOutputBytevectorPrimitive,
    GetOutputBytevectorPrimitive,
    MakeTranscoderPrimitive,
    TranscodedPortPrimitive,
    MakeBufferedPortPrimitive,
    MakeEncodedPortPrimitive,
    WantIdentifiersPrimitive,
    ConsolePortPPrimitive,
    InteractiveConsolePortPPrimitive,
    SetConsoleInputEditlinePrimitive,
    SetConsoleInputEchoPrimitive,
    SaveHistoryPrimitive,
    LoadHistoryPrimitive,
    PositioningPortPPrimitive,
    PortPositionPrimitive,
    SetPortPositionPrimitive,
    CopyPortPrimitive,
    MakeCustomBinaryInputPortPrimitive,
    MakeCustomTextualInputPortPrimitive,
    MakeCustomBinaryOutputPortPrimitive,
    MakeCustomTextualOutputPortPrimitive,
    MakeCustomBinaryInputOutputPortPrimitive,
    MakeCustomTextualInputOutputPortPrimitive,
    SocketMergeFlagsPrimitive,
    SocketPurgeFlagsPrimitive,
    SocketPPrimitive,
    MakeSocketPrimitive,
    BindSocketPrimitive,
    ListenSocketPrimitive,
    AcceptSocketPrimitive,
    ConnectSocketPrimitive,
    ShutdownSocketPrimitive,
    SendSocketPrimitive,
    ReceiveSocketPrimitive,
    GetIpAddressesPrimitive
};

#ifdef FOMENT_UNIX
static struct termios OriginalTios;
static int InteractiveConsole = 0;

static void SetupTios(void)
{
    struct termios tios;

    tcgetattr(0, &tios);
    tios.c_iflag = BRKINT;
    tios.c_lflag = ISIG;
    tios.c_cc[VMIN] = 1;
    tios.c_cc[VTIME] = 1;
    tcsetattr(0, TCSANOW, &tios);
}

static void RestoreTios(void)
{
    tcsetattr(0, TCSANOW, &OriginalTios);
}

void SetupConsoleAgain()
{
    if (InteractiveConsole)
        SetupTios();
}

void RestoreConsole()
{
    if (InteractiveConsole)
        RestoreTios();
}

static long_t SetupConsole()
{
    tcgetattr(0, &OriginalTios);
    atexit(RestoreTios);

    SetupTios();

    long_t x;
    long_t y;

    if (ConGetCursor(&x, &y) == 0)
        return(0);

    return(1);
}
#endif // FOMENT_UNIX

static void DefineConstant(FObject env, FObject lib, const char * nam, FObject val)
{
    LibraryExport(lib, EnvironmentSetC(env, nam, val));
}

void FlushStandardPorts()
{
    if (TextualPortP(StandardOutput) || BinaryPortP(StandardOutput))
        FlushOutput(StandardOutput);
    if (TextualPortP(StandardError) || BinaryPortP(StandardError))
        FlushOutput(StandardError);
}

void SetupIO()
{
    RegisterRoot(&StandardInput, "standard-input");
    RegisterRoot(&StandardOutput, "standard-output");
    RegisterRoot(&StandardError, "standard-error");

#ifdef FOMENT_WINDOWS
    WSADATA wd;

    WSAStartup(MAKEWORD(2, 2), &wd);

    HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE herr = GetStdHandle(STD_ERROR_HANDLE);

    if (hin != INVALID_HANDLE_VALUE && hout != INVALID_HANDLE_VALUE)
    {
        DWORD imd, omd;

        if (GetConsoleMode(hin, &imd) != 0 && GetConsoleMode(hout, &omd) != 0)
        {
            SetConsoleMode(hin, ENABLE_PROCESSED_INPUT | ENABLE_WINDOW_INPUT);

            StandardInput = MakeConsoleInputPort(MakeStringC("console-input"), hin, hout);
            StandardOutput = MakeConsoleOutputPort(MakeStringC("console-output"), hout);
        }
        else
        {
            StandardInput = MakeTranscodedPort(MakeBufferedPort(
                    MakeHandleInputPort(MakeStringC("standard-input"), hin)),
                    LATIN1_CODEC_INDEX, StyleCRLF, ModeReplace);
            StandardOutput = MakeTranscodedPort(MakeBufferedPort(
                    MakeHandleOutputPort(MakeStringC("standard-output"), hout)),
                    LATIN1_CODEC_INDEX, StyleCRLF, ModeReplace);
        }
    }

    if (herr != INVALID_HANDLE_VALUE)
    {
        DWORD emd;

        if (GetConsoleMode(herr, &emd) != 0)
            StandardError = MakeConsoleOutputPort(MakeStringC("console-output"), herr);
        else
            StandardError = MakeTranscodedPort(
                    MakeHandleOutputPort(MakeStringC("standard-error"), herr),
                    LATIN1_CODEC_INDEX, StyleCRLF, ModeReplace);

    }
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    if (isatty(0) && isatty(1))
    {
        char * term = getenv("TERM");
        InteractiveConsole = (term != 0 && strcasecmp(term, "dumb") != 0 && SetupConsole());
        StandardInput = MakeConsoleInputPort(MakeStringC("console-input"), 0, 1,
                InteractiveConsole);
        StandardOutput = MakeConsoleOutputPort(MakeStringC("console-output"), 1);
    }
    else
    {
        StandardInput = MakeTranscodedPort(MakeBufferedPort(
                MakeFileDescInputPort(MakeStringC("standard-input"), 0)),
                UTF8_CODEC_INDEX, StyleNone, ModeReplace);
        StandardOutput = MakeTranscodedPort(MakeBufferedPort(
                MakeFileDescOutputPort(MakeStringC("standard-output"), 1)),
                UTF8_CODEC_INDEX, StyleNone, ModeReplace);
    }

    StandardError = MakeTranscodedPort(
            MakeFileDescOutputPort(MakeStringC("standard-error"), 2),
            UTF8_CODEC_INDEX, StyleNone, ModeReplace);
#endif // FOMENT_UNIX

    FileErrorSymbol = InternSymbol(FileErrorSymbol);
    PositionErrorSymbol = InternSymbol(PositionErrorSymbol);
    DecodingErrorSymbol = InternSymbol(DecodingErrorSymbol);
    EncodingErrorSymbol = InternSymbol(EncodingErrorSymbol);
    CurrentSymbol = InternSymbol(CurrentSymbol);
    EndSymbol = InternSymbol(EndSymbol);

    FAssert(FileErrorSymbol == StringCToSymbol("file-error"));
    FAssert(PositionErrorSymbol == StringCToSymbol("position-error"));
    FAssert(DecodingErrorSymbol == StringCToSymbol("decoding-error"));
    FAssert(EncodingErrorSymbol == StringCToSymbol("encoding-error"));
    FAssert(CurrentSymbol == StringCToSymbol("current"));
    FAssert(EndSymbol == StringCToSymbol("end"));

    for (ulong_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(Bedrock, BedrockLibrary, Primitives[idx]);

    for (ulong_t idx = 0; idx < sizeof(CodecTypes) / sizeof(FCodecType); idx++)
        DefineConstant(Bedrock, BedrockLibrary, CodecTypes[idx].Constant, MakeCodec(idx));

    DefineConstant(Bedrock, BedrockLibrary, "*af-unspec*", MakeFixnum(AF_UNSPEC));
    DefineConstant(Bedrock, BedrockLibrary, "*af-inet*", MakeFixnum(AF_INET));
    DefineConstant(Bedrock, BedrockLibrary, "*af-inet6*", MakeFixnum(AF_INET6));

    DefineConstant(Bedrock, BedrockLibrary, "*sock-stream*", MakeFixnum(SOCK_STREAM));
    DefineConstant(Bedrock, BedrockLibrary, "*sock-dgram*", MakeFixnum(SOCK_DGRAM));
    DefineConstant(Bedrock, BedrockLibrary, "*sock-raw*", MakeFixnum(SOCK_RAW));

    DefineConstant(Bedrock, BedrockLibrary, "*ai-canonname*", MakeFixnum(AI_CANONNAME));
    DefineConstant(Bedrock, BedrockLibrary, "*ai-numerichost*", MakeFixnum(AI_NUMERICHOST));
    DefineConstant(Bedrock, BedrockLibrary, "*ai-v4mapped*", MakeFixnum(AI_V4MAPPED));
    DefineConstant(Bedrock, BedrockLibrary, "*ai-all*", MakeFixnum(AI_ALL));
    DefineConstant(Bedrock, BedrockLibrary, "*ai-addrconfig*", MakeFixnum(AI_ADDRCONFIG));

    DefineConstant(Bedrock, BedrockLibrary, "*ipproto-ip*", MakeFixnum(IPPROTO_IP));
    DefineConstant(Bedrock, BedrockLibrary, "*ipproto-tcp*", MakeFixnum(IPPROTO_TCP));
    DefineConstant(Bedrock, BedrockLibrary, "*ipproto-udp*", MakeFixnum(IPPROTO_UDP));

    DefineConstant(Bedrock, BedrockLibrary, "*msg-peek*", MakeFixnum(MSG_PEEK));
    DefineConstant(Bedrock, BedrockLibrary, "*msg-oob*", MakeFixnum(MSG_OOB));
    DefineConstant(Bedrock, BedrockLibrary, "*msg-waitall*", MakeFixnum(MSG_WAITALL));

#ifdef FOMENT_WINDOWS
    DefineConstant(Bedrock, BedrockLibrary, "*shut-rd*", MakeFixnum(SD_RECEIVE));
    DefineConstant(Bedrock, BedrockLibrary, "*shut-wr*", MakeFixnum(SD_SEND));
    DefineConstant(Bedrock, BedrockLibrary, "*shut-rdwr*", MakeFixnum(SD_BOTH));
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
    DefineConstant(Bedrock, BedrockLibrary, "*shut-rd*", MakeFixnum(SHUT_RD));
    DefineConstant(Bedrock, BedrockLibrary, "*shut-wr*", MakeFixnum(SHUT_WR));
    DefineConstant(Bedrock, BedrockLibrary, "*shut-rdwr*", MakeFixnum(SHUT_RDWR));
#endif // FOMENT_UNIX

    SetupWrite();
    SetupRead();
}
