/*

Foment

To Do:
-- CheckObject: check back references from mature objects
-- partial GC
-- after partial GC, test for objects pointing to Babies
-- Use extra generation for immortal objects which are precompiled libraries
-- split base.scm into separate files

-- document process-times[-reset!], object-counts[-reset!], stack-used[-reset!]
-- add time and time-apply same as racket

HashTable:
-- test hash tables

Red Edition:
-- SRFI 41 (scheme stream)
-- SRFI 101 (scheme list random-access)
-- SRFI 113 (scheme set)
-- SRFI 114 (scheme set char)
-- SRFI 116 (scheme list immutable)
-- SRFI 117 (scheme list queue)
-- SRFI 121 (scheme generator)
-- SRFI 125 (scheme hash-table)
-- SRFI 128 (scheme lazy-seq)
-- SRFI 132 (scheme sorting)
-- SRFI 133 (scheme vector)
-- SRFI 134 (scheme deque immutable)
-- SRFI 135 (scheme textual)

Future:
-- change EternalSymbol (and Define) to set Symbol->Hash at compile time
-- allow larger objects by using BlockSize > 1 in FObjHdr
-- pull options from FOMENT_OPTIONS environment variable
-- features, command-line, full-command-line, interactive options,
    environment-variables, etc passed to scheme as a single assoc list
-- number.cpp: make NumberP, BinaryNumberOp, and UnaryNumberOp faster
-- Windows: $(APPDATA)\Foment\Libraries
-- Unix: $(HOME)/.local/foment/lib
-- replace mini-gmp
-- increase maximum/minimum fixnum on 64bit
-- inline primitives in GPassExpression
-- debugging information
-- composable continuations
-- from Gambit:
    Serial numbers are used by the printer to identify objects which can't be read
    Convenient for debugging
    > (let ((n 2)) (lambda (x) (* x n)))
    #<procedure #2>
    > (pp #2)
    (lambda (x) (* x n))
    > (map #2 ’(1 2 3 4 5))
    (2 4 6 8 10)

Compiler:
-- get rid of -no-inline-procedures and -no-inline-imports
-- genpass: handle multiple values with LetrecValues correctly
-- genpass: LetrecStarValues needs to be compiled correctly
-- export letrec-values and letrec*-values from (foment base)
-- inlpass: inline lambdas
-- pass all non-local references into each lambda: frames can go away
-- get rid of Level on FLambda
-- treat case-lambda like lambda and have an entry point to compile and to each pass
-- make special syntax into an object so that can track location information

Bugs:
-- the following program should work
(import (scheme base) (scheme write))
(define-syntax deffoo
    (syntax-rules ()
        ((deffoo name (vals ...)) (deffoo name () (vals ...)))
        ((deffoo name (field ...) (val vals ...)) (deffoo name (field ... (tmp . val)) (vals ...)))
        ((deffoo name ((field . val) ...) ())
            (begin
                (define field 'val) ...
                (define name (list field ...))))))

(define tmp 'abc)
(deffoo foo (a b c d))

(write foo)
(newline)
-- make test segfault on unix32
-- letrec: http://trac.sacrideo.us/wg/wiki/LetrecStar
-- serialize loading libraries
-- r5rs_pitfall.scm: yin-yang does not terminate; see test2.scm and
   https://groups.google.com/forum/?fromgroups=#!topic/comp.lang.scheme/Fysq_Wplxsw
-- exhaustively test unicode: char-alphabetic?, char-upcase, etc
*/

#ifndef __FOMENT_HPP__
#define __FOMENT_HPP__

#define FOMENT_VERSION "0.4"

#include <stdint.h>

typedef double double64_t;

#ifdef FOMENT_WINDOWS
typedef wchar_t FCh16;
typedef FCh16 FChS;

#define FALIGN __declspec(align(8))

#ifdef _M_AMD64
#define FOMENT_64BIT
#define FOMENT_MEMORYMODEL "llp64"
#else // _M_AMD64
#define FOMENT_32BIT
#define FOMENT_MEMORYMODEL "ilp32"
#endif // _M_AMD64
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
#include <sys/param.h>
typedef uint16_t FCh16;
typedef char FChS;

#define FALIGN __attribute__ ((aligned(8)))

#ifdef __LP64__
#define FOMENT_64BIT
#define FOMENT_MEMORYMODEL "lp64"
#else // __LP64__
#define FOMENT_32BIT
#define FOMENT_MEMORYMODEL "ilp32"
#endif // __LP64__

#ifdef __APPLE__
#define FOMENT_OSX
#elif BSD
#define FOMENT_BSD
#endif // BSD
#endif // FOMENT_UNIX

typedef void * FObject;
typedef uint32_t FCh;

#ifdef FOMENT_32BIT
typedef int32_t long_t;
typedef uint32_t ulong_t;

#define LONG_FMT "%d"
#define ULONG_FMT "%u"
#endif // FOMENT_32BIT

#ifdef FOMENT_64BIT
typedef int64_t long_t;
typedef uint64_t ulong_t;

#if defined(FOMENT_OSX) || defined(FOMENT_WINDOWS)
#define LONG_FMT "%lld"
#define ULONG_FMT "%llu"
#else
#define LONG_FMT "%ld"
#define ULONG_FMT "%lu"
#endif
#endif // FOMENT_64BIT

#ifdef FOMENT_DEBUG
void FAssertFailed(const char * fn, long_t ln, const char * expr);
#define FAssert(expr)\
    if (! (expr)) FAssertFailed( __FILE__, __LINE__, #expr)
#else // FOMENT_DEBUG
#define FAssert(expr)
#endif // FOMENT_DEBUG

void FMustBeFailed(const char * fn, long_t ln, const char * expr);
#define FMustBe(expr)\
    if (! (expr)) FMustBeFailed(__FILE__, __LINE__, #expr)

typedef enum
{
    // Direct Types

    // ObjectTag = 0x0,
    FixnumTag = 0x01,
    CharacterTag = 0x02,
    MiscellaneousTag = 0x03,
    SpecialSyntaxTag = 0x04,
    InstructionTag = 0x05,
    ValuesCountTag = 0x06,
    BooleanTag = 0x07
} FDirectTag;

typedef enum
{
    // Indirect Types

    BignumTag = 0x01, // fix order of tags in gc.cpp
    RatioTag,
    ComplexTag,
    FlonumTag,
    BoxTag,
    PairTag,
    StringTag,
    CStringTag,
    VectorTag,
    BytevectorTag,
    BinaryPortTag,
    TextualPortTag,
    ProcedureTag,
    SymbolTag,
    RecordTypeTag,
    RecordTag,
    PrimitiveTag,
    ThreadTag,
    ExclusiveTag,
    ConditionTag,
    HashNodeTag,
    EphemeronTag,
    BuiltinTypeTag,
    BuiltinTag,
    FreeTag, // Only on Adult Generation
    BadDogTag // Invalid Tag
} FIndirectTag;

#define ObjectP(obj) ((((ulong_t) (obj)) & 0x7) == 0x0)

#define ImmediateTag(obj) (((ulong_t) (obj)) & 0x7)
#define ImmediateP(obj, it) (ImmediateTag((obj)) == it)
#define MakeImmediate(val, it)\
    ((FObject *) ((((ulong_t) (val)) << 3) | (it & 0x7)))
#define AsValue(obj) (((ulong_t) (obj)) >> 3)

#define FixnumP(obj) ImmediateP(obj, FixnumTag)
#define MakeFixnum(n)\
    ((FObject *) ((((ulong_t) (n)) << 3) | (FixnumTag & 0x7)))
#define AsFixnum(obj) (((long_t) (obj)) >> 3)

#define MAXIMUM_FIXNUM ((((long_t) 1) << (sizeof(int32_t) * 8 - 4)) - 1)
#define MINIMUM_FIXNUM (- MAXIMUM_FIXNUM)
#define FIXNUM_BITS (sizeof(long_t) * 8)

#define NormalizeHash(hsh) ((hsh) & MAXIMUM_FIXNUM)

// ---- Memory Management ----

#define OBJECT_ALIGNMENT 8

typedef enum
{
    NoCollector,
    MarkSweepCollector,
    GenerationalCollector
} FCollectorType;

extern FCollectorType CollectorType;

typedef struct
{
    ulong_t TopUsed;
    ulong_t BottomUsed;
    ulong_t MaximumSize;
    void * Base;
} FMemRegion;

void * InitializeMemRegion(FMemRegion * mrgn, ulong_t max);
void DeleteMemRegion(FMemRegion * mrgn);
long_t GrowMemRegionUp(FMemRegion * mrgn, ulong_t sz);
long_t GrowMemRegionDown(FMemRegion * mrgn, ulong_t sz);

#define OBJHDR_SIZE_SHIFT 28
#define OBJHDR_COUNT_MASK 0x0FFFFFFF

#define OBJHDR_GEN_MASK           0xC000
#define OBJHDR_GEN_BABIES         0x0000
#define OBJHDR_GEN_KIDS           0x4000
#define OBJHDR_GEN_ADULTS         0x8000
#define OBJHDR_GEN_ETERNAL        0xC000
#define OBJHDR_MARK_FORWARD       0x2000
#define OBJHDR_CHECK_MARK         0x1000
#define OBJHDR_HAS_SLOTS          0x0800
#define OBJHDR_EPHEMERON_KEY_MARK 0x0400

#define OBJHDR_TAG_MASK           0x001F

typedef struct _FObjHdr
{
    uint32_t BlockSizeAndCount;
    uint16_t ExtraCount;
    uint16_t FlagsAndTag;

private:
    ulong_t BlockSize() {return(((ulong_t) 1) << (BlockSizeAndCount >> OBJHDR_SIZE_SHIFT));}
    ulong_t BlockCount() {return(BlockSizeAndCount & OBJHDR_COUNT_MASK);}
public:
    ulong_t ObjectSize();
    ulong_t TotalSize();
    ulong_t SlotCount();
    ulong_t ByteLength();
    uint32_t Tag() {return(FlagsAndTag & OBJHDR_TAG_MASK);}
    FObject * Slots() {return((FObject *) (this + 1));}
    uint32_t Generation() {return(FlagsAndTag & OBJHDR_GEN_MASK);}
} FObjHdr;

// Allocated size of the object in bytes, not including the ObjHdr and ObjFtr.
inline ulong_t FObjHdr::ObjectSize()
{
    FAssert(BlockSize() * BlockCount() > 0);

    return(BlockSize() * BlockCount() * sizeof(struct _FObjHdr));
}

// Number of FObjects which must be at the beginning of the object.
inline ulong_t FObjHdr::SlotCount()
{
    if (FlagsAndTag & OBJHDR_HAS_SLOTS)
    {
        FAssert(ObjectSize() / sizeof(FObject) >= ExtraCount);

        return(ObjectSize() / sizeof(FObject) - ExtraCount);
    }

    return(0);
}

// Number of bytes requested when the object was allocated; makes sense only for objects
// without slots.
inline ulong_t FObjHdr::ByteLength()
{
    FAssert((FlagsAndTag & OBJHDR_HAS_SLOTS) == 0);
    FAssert(ObjectSize() >= ExtraCount);

    return(ObjectSize() - ExtraCount);
}

inline FObjHdr * AsObjHdr(FObject obj)
{
    FAssert(ObjectP(obj));

    return(((FObjHdr *) obj) - 1);
}

inline ulong_t ByteLength(FObject obj)
{
    return(AsObjHdr(obj)->ByteLength());
}

typedef struct
{
    uint32_t Feet[2];
} FObjFtr;

#define OBJFTR_FEET 0xDEADFEE7

#define EternalObjHdr(type, tag) \
    {sizeof(type) / sizeof(FObjHdr), 0, tag | OBJHDR_GEN_ETERNAL}

#define EternalObjHdrSlots(type, sc, tag) \
    {sizeof(type) / sizeof(FObjHdr), (sizeof(type) / sizeof(FObject)) - sc, \
            tag | OBJHDR_GEN_ETERNAL | OBJHDR_HAS_SLOTS}

#define EternalObjFtr \
    {{OBJFTR_FEET, OBJFTR_FEET}}

FObject MakeObject(ulong_t tag, ulong_t sz, ulong_t sc, const char * who, long_t pf = 0);

inline FIndirectTag IndirectTag(FObject obj)
{
    return((FIndirectTag) (ObjectP(obj) ? AsObjHdr(obj)->Tag() : 0));
}

extern volatile long_t GCRequired;

void EnterWait();
void LeaveWait();

void CheckHeap(const char * fn, int ln);
void ReadyForGC();
#define CheckForGC() if (GCRequired) ReadyForGC()

inline long_t MatureP(FObject obj)
{
    return(ObjectP(obj) && (AsObjHdr(obj)->FlagsAndTag & OBJHDR_GEN_MASK) == OBJHDR_GEN_ADULTS);
}

void ModifyVector(FObject obj, ulong_t idx, FObject val);

/*
//    AsProcedure(proc)->Name = nam;
    Modify(FProcedure, proc, Name, nam);
*/
#define Modify(type, obj, slot, val)\
    ModifyObject(obj, (ulong_t) &(((type *) 0)->slot), val)

// Do not directly call ModifyObject; use Modify instead.
void ModifyObject(FObject obj, ulong_t off, FObject val);

void InstallGuardian(FObject obj, FObject tconc);
void InstallTracker(FObject obj, FObject ret, FObject tconc);

class FAlive
{
public:

    FAlive(FObject * ptr);
    ~FAlive();

    FAlive * Next;
    FObject * Pointer;
};

extern ulong_t MaximumStackSize;
extern ulong_t MaximumBabiesSize;
extern ulong_t MaximumKidsSize;
extern ulong_t MaximumAdultsSize;
extern ulong_t MaximumGenerationalBaby;
extern ulong_t TriggerObjects;
extern ulong_t TriggerBytes;
extern ulong_t PartialPerFull;

//
// ---- Immediate Types ----
//

#define CharacterP(obj) ImmediateP(obj, CharacterTag)
#define MakeCharacter(ch) MakeImmediate(ch, CharacterTag)
#define AsCharacter(obj) ((FCh) (AsValue(obj)))

#define EmptyListObject MakeImmediate(0, MiscellaneousTag)
#define EmptyListObjectP(obj) ((obj) == EmptyListObject)

#define EndOfFileObject MakeImmediate(1, MiscellaneousTag)
#define EndOfFileObjectP(obj) ((obj) == EndOfFileObject)

#define NoValueObject MakeImmediate(2, MiscellaneousTag)
#define NoValueObjectP(obj) ((obj) == NoValueObject)

#define WantValuesObject MakeImmediate(3, MiscellaneousTag)
#define WantValuesObjectP(obj) ((obj) == WantValuesObject)

#define NotFoundObject MakeImmediate(4, MiscellaneousTag)
#define NotFoundObjectP(obj) ((obj) == NotFoundObject)

#define MatchAnyObject MakeImmediate(5, MiscellaneousTag)
#define MatchAnyObjectP(obj) ((obj) == MatchAnyObject)

#define ValuesCountP(obj) ImmediateP(obj, ValuesCountTag)
#define MakeValuesCount(cnt) MakeImmediate(cnt, ValuesCountTag)
#define AsValuesCount(obj) ((long_t) (AsValue(obj)))

#define FalseObject MakeImmediate(0, BooleanTag)
#define TrueObject MakeImmediate(1, BooleanTag)
#define BooleanP(obj) ImmediateP(obj, BooleanTag)

// ---- Special Syntax ----

#define SpecialSyntaxP(obj) ImmediateP((obj), SpecialSyntaxTag)
const char * SpecialSyntaxToName(FObject obj);

#define QuoteSyntax MakeImmediate(0, SpecialSyntaxTag)
#define LambdaSyntax MakeImmediate(1, SpecialSyntaxTag)
#define IfSyntax MakeImmediate(2, SpecialSyntaxTag)
#define SetBangSyntax MakeImmediate(3, SpecialSyntaxTag)
#define LetSyntax MakeImmediate(4, SpecialSyntaxTag)
#define LetStarSyntax MakeImmediate(5, SpecialSyntaxTag)
#define LetrecSyntax MakeImmediate(6, SpecialSyntaxTag)
#define LetrecStarSyntax MakeImmediate(7, SpecialSyntaxTag)
#define LetValuesSyntax MakeImmediate(8, SpecialSyntaxTag)
#define LetStarValuesSyntax MakeImmediate(9, SpecialSyntaxTag)
#define LetrecValuesSyntax MakeImmediate(10, SpecialSyntaxTag)
#define LetrecStarValuesSyntax MakeImmediate(11, SpecialSyntaxTag)
#define LetSyntaxSyntax MakeImmediate(12, SpecialSyntaxTag)
#define LetrecSyntaxSyntax MakeImmediate(13, SpecialSyntaxTag)
#define OrSyntax MakeImmediate(14, SpecialSyntaxTag)
#define BeginSyntax MakeImmediate(15, SpecialSyntaxTag)
#define DoSyntax MakeImmediate(16, SpecialSyntaxTag)
#define SyntaxRulesSyntax MakeImmediate(17, SpecialSyntaxTag)
#define SyntaxErrorSyntax MakeImmediate(18, SpecialSyntaxTag)
#define IncludeSyntax MakeImmediate(19, SpecialSyntaxTag)
#define IncludeCISyntax MakeImmediate(20, SpecialSyntaxTag)
#define CondExpandSyntax MakeImmediate(21, SpecialSyntaxTag)
#define CaseLambdaSyntax MakeImmediate(22, SpecialSyntaxTag)
#define QuasiquoteSyntax MakeImmediate(23, SpecialSyntaxTag)

#define DefineSyntax MakeImmediate(24, SpecialSyntaxTag)
#define DefineValuesSyntax MakeImmediate(25, SpecialSyntaxTag)
#define DefineSyntaxSyntax MakeImmediate(26, SpecialSyntaxTag)

#define ElseSyntax MakeImmediate(27, SpecialSyntaxTag)
#define ArrowSyntax MakeImmediate(28, SpecialSyntaxTag)
#define UnquoteSyntax MakeImmediate(29, SpecialSyntaxTag)
#define UnquoteSplicingSyntax MakeImmediate(30, SpecialSyntaxTag)
#define EllipsisSyntax MakeImmediate(31, SpecialSyntaxTag)
#define UnderscoreSyntax MakeImmediate(32, SpecialSyntaxTag)
#define SetBangValuesSyntax MakeImmediate(33, SpecialSyntaxTag)

// ---- Instruction ----

#define InstructionP(obj) ImmediateP((obj), InstructionTag)

//
// ---- Object Types ----
//

// ---- Pairs ----

#define PairP(obj) (IndirectTag(obj) == PairTag)
#define AsPair(obj) ((FPair *) (obj))

typedef struct
{
    FObject First;
    FObject Rest;
} FPair;

inline FObject First(FObject obj)
{
    FAssert(PairP(obj));
    return(AsPair(obj)->First);
}

inline FObject Rest(FObject obj)
{
    FAssert(PairP(obj));
    return(AsPair(obj)->Rest);
}

void SetFirst(FObject obj, FObject val);
void SetRest(FObject obj, FObject val);

FObject MakePair(FObject first, FObject rest);
long_t ListLength(FObject lst);
long_t ListLength(const char * nam, FObject lst);
FObject ReverseListModify(FObject list);

FObject List(FObject obj);
FObject List(FObject obj1, FObject obj2);
FObject List(FObject obj1, FObject obj2, FObject obj3);
FObject List(FObject obj1, FObject obj2, FObject obj3, FObject obj4);
FObject List(FObject obj1, FObject obj2, FObject obj3, FObject obj4, FObject obj5);
FObject List(FObject obj1, FObject obj2, FObject obj3, FObject obj4, FObject obj5,
    FObject obj6);
FObject List(FObject obj1, FObject obj2, FObject obj3, FObject obj4, FObject obj5,
    FObject obj6, FObject obj7);

FObject Memq(FObject obj, FObject lst);
FObject Assq(FObject obj, FObject alst);
FObject Assoc(FObject obj, FObject alst);

FObject MakeTConc();
long_t TConcEmptyP(FObject tconc);
void TConcAdd(FObject tconc, FObject obj);
FObject TConcRemove(FObject tconc);

// ---- Boxes ----

#define BoxP(obj) (IndirectTag(obj) == BoxTag)
#define AsBox(obj) ((FBox *) (obj))

typedef struct
{
    FObject Value;
    ulong_t Index;
} FBox;

FObject MakeBox(FObject val);
FObject MakeBox(FObject val, ulong_t idx);
inline FObject Unbox(FObject bx)
{
    FAssert(BoxP(bx));

    return(AsBox(bx)->Value);
}

void SetBox(FObject bx, FObject val);
#define BoxIndex(bx) (AsBox(bx)->Index)

// ---- Strings ----

#define StringP(obj) (IndirectTag(obj) == StringTag)
#define AsString(obj) ((FString *) (obj))

typedef struct
{
    FCh String[1];
} FString;

FObject MakeString(FCh * s, ulong_t sl);
FObject MakeStringCh(ulong_t sl, FCh ch);
FObject MakeStringC(const char * s);
FObject MakeStringS(FChS * ss);
FObject MakeStringS(FChS * ss, ulong_t ssl);

inline ulong_t StringLength(FObject obj)
{
    FAssert(StringP(obj));
    FAssert(ByteLength(obj) % sizeof(FCh) == 0);
    FAssert(ByteLength(obj) >= sizeof(FCh));

    return((ByteLength(obj) / sizeof(FCh)) - 1);
}

void StringToC(FObject s, char * b, long_t bl);
FObject FoldcaseString(FObject s);
uint32_t StringLengthHash(FCh * s, ulong_t sl);
uint32_t StringHash(FObject obj);
uint32_t StringCiHash(FObject obj);
uint32_t CStringHash(const char * s);
long_t StringLengthEqualP(FCh * s, long_t sl, FObject obj);
long_t StringCEqualP(const char * s1, FCh * s2, long_t sl2);
long_t StringCompare(FObject obj1, FObject obj2);

extern FObject StringPPrimitive;
extern FObject StringEqualPPrimitive;
extern FObject StringHashPrimitive;

// ---- Vectors ----

#define VectorP(obj) (IndirectTag(obj) == VectorTag)
#define AsVector(obj) ((FVector *) (obj))

typedef struct
{
    FObject Vector[1];
} FVector;

FObject MakeVector(ulong_t vl, FObject * v, FObject obj);
FObject ListToVector(FObject obj);
FObject VectorToList(FObject vec);

#define VectorLength(obj) (AsObjHdr(obj)->SlotCount())

// ---- Bytevectors ----

#define BytevectorP(obj) (IndirectTag(obj) == BytevectorTag)
#define AsBytevector(obj) ((FBytevector *) (obj))

typedef unsigned char FByte;
typedef struct
{
    FByte Vector[1];
} FBytevector;

FObject MakeBytevector(ulong_t vl);
FObject U8ListToBytevector(FObject obj);

#define BytevectorLength(obj) ByteLength(obj)

// ---- Ports ----

#define PORT_FLAG_INPUT              0x800
#define PORT_FLAG_INPUT_OPEN         0x400
#define PORT_FLAG_OUTPUT             0x200
#define PORT_FLAG_OUTPUT_OPEN        0x100
#define PORT_FLAG_STRING_OUTPUT      0x080
#define PORT_FLAG_BYTEVECTOR_OUTPUT  0x040
#define PORT_FLAG_FOLDCASE           0x020
#define PORT_FLAG_WANT_IDENTIFIERS   0x010
#define PORT_FLAG_CONSOLE            0x008
#define PORT_FLAG_SOCKET             0x004
#define PORT_FLAG_BUFFERED           0x002
#define PORT_FLAG_POSITIONING        0x001

typedef enum
{
    FromBegin,
    FromCurrent,
    FromEnd
} FPositionFrom;

typedef void (*FCloseInputFn)(FObject port);
typedef void (*FCloseOutputFn)(FObject port);
typedef void (*FFlushOutputFn)(FObject port);
typedef int64_t (*FGetPositionFn)(FObject port);
typedef void (*FSetPositionFn)(FObject port, int64_t pos, FPositionFrom frm);

typedef struct
{
    FObject Name;
    FObject Object;
    ulong_t Flags;
    void * Context;
    FCloseInputFn CloseInputFn;
    FCloseOutputFn CloseOutputFn;
    FFlushOutputFn FlushOutputFn;
    FGetPositionFn GetPositionFn;
    FSetPositionFn SetPositionFn;
} FGenericPort;

#define AsGenericPort(obj) ((FGenericPort *) obj)

#define TextualPortP(obj) (IndirectTag(obj) == TextualPortTag)
#define BinaryPortP(obj) (IndirectTag(obj) == BinaryPortTag)

inline long_t InputPortP(FObject obj)
{
    return((BinaryPortP(obj) || TextualPortP(obj))
            && (AsGenericPort(obj)->Flags & PORT_FLAG_INPUT));
}

inline long_t OutputPortP(FObject obj)
{
    return((BinaryPortP(obj) || TextualPortP(obj))
            && (AsGenericPort(obj)->Flags & PORT_FLAG_OUTPUT));
}

inline long_t InputPortOpenP(FObject obj)
{
    FAssert(BinaryPortP(obj) || TextualPortP(obj));

    return(AsGenericPort(obj)->Flags & PORT_FLAG_INPUT_OPEN);
}

inline long_t OutputPortOpenP(FObject obj)
{
    FAssert(BinaryPortP(obj) || TextualPortP(obj));

    return(AsGenericPort(obj)->Flags & PORT_FLAG_OUTPUT_OPEN);
}

inline long_t PortOpenP(FObject obj)
{
    FAssert(BinaryPortP(obj) || TextualPortP(obj));

    return((AsGenericPort(obj)->Flags & PORT_FLAG_INPUT_OPEN) ||
            (AsGenericPort(obj)->Flags & PORT_FLAG_OUTPUT_OPEN));
}

inline long_t StringOutputPortP(FObject obj)
{
    return(TextualPortP(obj) && (AsGenericPort(obj)->Flags & PORT_FLAG_STRING_OUTPUT));
}

inline long_t BytevectorOutputPortP(FObject obj)
{
    return(BinaryPortP(obj) && (AsGenericPort(obj)->Flags & PORT_FLAG_BYTEVECTOR_OUTPUT));
}

inline long_t FoldcasePortP(FObject port)
{
    FAssert(TextualPortP(port) && InputPortP(port));

    return(AsGenericPort(port)->Flags & PORT_FLAG_FOLDCASE);
}

inline long_t WantIdentifiersPortP(FObject port)
{
    FAssert(TextualPortP(port) && InputPortP(port));

    return(AsGenericPort(port)->Flags & PORT_FLAG_WANT_IDENTIFIERS);
}

inline long_t ConsolePortP(FObject port)
{
    return(TextualPortP(port) && (AsGenericPort(port)->Flags & PORT_FLAG_CONSOLE));
}

inline long_t SocketPortP(FObject port)
{
    return(BinaryPortP(port) && (AsGenericPort(port)->Flags & PORT_FLAG_SOCKET));
}

inline long_t PositioningPortP(FObject obj)
{
    FAssert(BinaryPortP(obj) || TextualPortP(obj));

    return(AsGenericPort(obj)->Flags & PORT_FLAG_POSITIONING);
}

// Binary and textual ports

FObject HandOffPort(FObject port);
void CloseInput(FObject port);
void CloseOutput(FObject port);
void FlushOutput(FObject port);
int64_t GetPosition(FObject port);
void SetPosition(FObject port, int64_t pos, FPositionFrom frm);

// Binary ports

ulong_t ReadBytes(FObject port, FByte * b, ulong_t bl);
long_t PeekByte(FObject port, FByte * b);
long_t ByteReadyP(FObject port);
ulong_t GetOffset(FObject port);

void WriteBytes(FObject port, void * b, ulong_t bl);

// Textual ports

FObject OpenInputFile(FObject fn);
FObject OpenOutputFile(FObject fn);
FObject MakeStringInputPort(FObject str);
FObject MakeStringCInputPort(const char * s);
FObject MakeStringOutputPort();
FObject GetOutputString(FObject port);

ulong_t ReadCh(FObject port, FCh * ch);
ulong_t PeekCh(FObject port, FCh * ch);
long_t CharReadyP(FObject port);
FObject ReadLine(FObject port);
FObject ReadString(FObject port, ulong_t cnt);
ulong_t GetLineColumn(FObject port, ulong_t * col);
FObject GetFilename(FObject port);
void FoldcasePort(FObject port, long_t fcf);
void WantIdentifiersPort(FObject port, long_t wif);

FObject Read(FObject port);

void WriteCh(FObject port, FCh ch);
void WriteString(FObject port, FCh * s, ulong_t sl);
void WriteStringC(FObject port, const char * s);

typedef enum
{
    NoWrite,
    SimpleWrite,
    CircularWrite,
    SharedWrite
} FWriteType;

struct FWriteContext
{
    FWriteContext(FObject port, long_t df);
    void Prepare(FObject obj, FWriteType wt);

    void Write(FObject obj);
    void Display(FObject obj);
    void WriteCh(FCh ch);
    void WriteString(FCh * s, ulong_t sl);
    void WriteStringC(const char * s);

private:

    FObject Port;
    long_t DisplayFlag;
    FWriteType WriteType;
    FObject HashTable;
    long_t SharedCount;
    long_t PreviousLabel;

    void FindSharedObjects(FObject obj, FWriteType wt);
    void WritePair(FObject obj);
    void WriteRecord(FObject obj);
    void WriteObject(FObject obj);
    void WriteSimple(FObject obj);
};

void Write(FObject port, FObject obj, long_t df);
void WriteShared(FObject port, FObject obj, long_t df);
void WriteSimple(FObject port, FObject obj, long_t df);

// ---- Builtin Types ----

#define BuiltinTypeP(obj) (IndirectTag(obj) == BuiltinTypeTag)
#define AsBuiltinType(obj) ((FBuiltinType *) (obj))

typedef void (*FBuiltinWriteFn)(FWriteContext * wctx, FObject obj);

typedef struct
{
    const char * Name;
    FBuiltinWriteFn Write;
} FBuiltinType;

typedef struct FALIGN
{
    FObjHdr ObjHdr;
    FBuiltinType BuiltinType;
    FObjFtr ObjFtr;
} FEternalBuiltinType;

#define EternalBuiltinType(name, string, writefn) \
    static FEternalBuiltinType name ## Object = \
    { \
        EternalObjHdr(FBuiltinType, BuiltinTypeTag), \
        {string, writefn}, \
        EternalObjFtr \
    }; \
    FObject name = &name ## Object.BuiltinType;

// ---- Builtins ----

#define BuiltinObjectP(obj) (IndirectTag(obj) == BuiltinTag)
#define AsBuiltin(obj) ((FBuiltin *) (obj))

typedef struct
{
    FObject BuiltinType; // must be FBuiltinType
} FBuiltin;

FObject MakeBuiltin(FObject bt, ulong_t sz, ulong_t sc, const char * who);

inline long_t BuiltinP(FObject obj, FObject bt)
{
    FAssert(BuiltinTypeP(bt));

    return(BuiltinObjectP(obj) && AsBuiltin(obj)->BuiltinType == bt);
}

// ---- Record Types ----

#define RecordTypeP(obj) (IndirectTag(obj) == RecordTypeTag)
#define AsRecordType(obj) ((FRecordType *) (obj))

typedef struct
{
    FObject Fields[1];
} FRecordType;

FObject MakeRecordType(FObject nam, ulong_t nf, FObject flds[]);

#define RecordTypeName(obj) AsRecordType(obj)->Fields[0]
#define RecordTypeNumFields(obj) (AsObjHdr(obj)->SlotCount())

// ---- Records ----

#define GenericRecordP(obj) (IndirectTag(obj) == RecordTag)
#define AsGenericRecord(obj) ((FGenericRecord *) (obj))

typedef struct
{
    FObject RecordType;
} FRecord;

typedef struct
{
    FObject Fields[1];
} FGenericRecord;

FObject MakeRecord(FObject rt);

inline long_t RecordP(FObject obj, FObject rt)
{
    return(GenericRecordP(obj) && AsGenericRecord(obj)->Fields[0] == rt);
}

#define RecordNumFields(obj) (AsObjHdr(obj)->SlotCount())

// ---- Comparators ----

#define AsComparator(obj) ((FComparator *) (obj))
#define ComparatorP(obj) BuiltinP(obj, ComparatorType)
extern FObject ComparatorType;

typedef struct
{
    FObject BuiltinType;
    FObject TypeTestP;
    FObject EqualityP;
    FObject OrderingP;
    FObject HashFn;
    FObject Context;
} FComparator;

long_t EqP(FObject obj1, FObject obj2);
long_t EqvP(FObject obj1, FObject obj2);
long_t EqualP(FObject obj1, FObject obj2);
uint32_t EqHash(FObject obj);

extern FObject EqPPrimitive;
extern FObject EqHashPrimitive;

// ---- Hash Tables ----

#define HASH_TABLE_NORMAL_KEYS      0x00
#define HASH_TABLE_WEAK_KEYS        0x01
#define HASH_TABLE_EPHEMERAL_KEYS   0x02
#define HASH_TABLE_KEYS_MASK        0x03

#define HASH_TABLE_NORMAL_VALUES    0x00
#define HASH_TABLE_WEAK_VALUES      0x04
#define HASH_TABLE_EPHEMERAL_VALUES 0x08
#define HASH_TABLE_VALUES_MASK      0x0C

#define HASH_TABLE_IMMUTABLE        0x10
#define HASH_TABLE_THREAD_SAFE      0x20

#define HashTableP(obj) BuiltinP(obj, HashTableType)
extern FObject HashTableType;

typedef FObject (*FFoldFn)(FObject key, FObject val, void * ctx, FObject accum);

FObject MakeEqHashTable(ulong_t cap, ulong_t flags);
FObject MakeStringHashTable(ulong_t cap, ulong_t flags);
FObject MakeSymbolHashTable(ulong_t cap, ulong_t flags);
FObject HashTableRef(FObject htbl, FObject key, FObject def);
void HashTableSet(FObject htbl, FObject key, FObject val);
void HashTableDelete(FObject htbl, FObject key);
FObject HashTableFold(FObject htbl, FFoldFn vfn, void * ctx, FObject seed);
void HashTableEphemeronBroken(FObject htbl);

// ---- Symbols ----

#define SymbolP(obj) (IndirectTag(obj) == SymbolTag)
#define AsSymbol(obj) ((FSymbol *) (obj))

typedef struct
{
    FObject String;
    uint32_t Hash;
#ifdef FOMENT_64BIT
    uint32_t Unused;
#endif // FOMENT_64BIT
} FSymbol;

#define CStringP(obj) (IndirectTag(obj) == CStringTag)
#define AsCString(obj) ((FCString *) (obj))

typedef struct
{
    const char * String;
#ifdef FOMENT_32BIT
    ulong_t Unused;
#endif // FOMENT_32BIT
} FCString;

typedef struct FALIGN
{
    FObjHdr ObjHdr;
    FCString String;
    FObjFtr ObjFtr;
} FEternalCString;

typedef struct FALIGN
{
    FObjHdr ObjHdr;
    FSymbol Symbol;
    FObjFtr ObjFtr;
} FEternalSymbol;

#define EternalSymbol(name, string) \
    static FEternalCString name ## String = \
    { \
        EternalObjHdr(FCString, CStringTag), \
        {string}, \
        EternalObjFtr \
    }; \
    static FEternalSymbol name ## Object = \
    { \
        EternalObjHdrSlots(FSymbol, 1, SymbolTag), \
        {&name ## String.String}, \
        EternalObjFtr \
    }; \
    FObject name = &name ## Object.Symbol;

FObject SymbolToString(FObject sym);
FObject StringToSymbol(FObject str);
FObject StringLengthToSymbol(FCh * s, long_t sl);
FObject InternSymbol(FObject sym);

FObject StringCToSymbol(const char * s);
FObject AddPrefixToSymbol(FObject str, FObject sym);

inline uint32_t SymbolHash(FObject obj)
{
    FAssert(SymbolP(obj));
    FAssert((StringP(AsSymbol(obj)->String) &&
                AsSymbol(obj)->Hash == StringHash(AsSymbol(obj)->String)) ||
           (CStringP(AsSymbol(obj)->String) &&
                AsSymbol(obj)->Hash == CStringHash(AsCString(AsSymbol(obj)->String)->String)));

    return(AsSymbol(obj)->Hash);
}

extern FObject SymbolPPrimitive;
extern FObject SymbolHashPrimitive;

// ---- Primitives ----

#define PrimitiveP(obj) (IndirectTag(obj) == PrimitiveTag)
#define AsPrimitive(obj) ((FPrimitive *) (obj))

typedef FObject (*FPrimitiveFn)(long_t argc, FObject argv[]);
typedef struct
{
    FObject Name;
    FPrimitiveFn PrimitiveFn;
    const char * Filename;
    long_t LineNumber;
} FPrimitive;

typedef struct FALIGN
{
    FObjHdr ObjHdr;
    FPrimitive Primitive;
    FObjFtr ObjFtr;
} FEternalPrimitive;

#define Define(name, prim) \
    EternalSymbol(prim ## Symbol, name); \
    FObject prim ## Fn(long_t argc, FObject argv[]);\
    static FEternalPrimitive prim ## Object = { \
        EternalObjHdrSlots(FPrimitive, 1, PrimitiveTag), \
        {prim ## Symbol, prim ## Fn, __FILE__, __LINE__}, \
        EternalObjFtr \
    }; \
    FObject prim = &prim ## Object.Primitive; \
    FObject prim ## Fn

void DefinePrimitive(FObject env, FObject lib, FObject prim);

// ---- Ephemerons ----

#define EphemeronP(obj) (IndirectTag(obj) == EphemeronTag)
#define AsEphemeron(obj) ((FEphemeron *) (obj))

typedef struct _FEphemeron
{
    FObject Key;
    FObject Datum;
    FObject HashTable;
    struct _FEphemeron * Next; // Used during garbage collection and for broken.
} FEphemeron;

FObject MakeEphemeron(FObject key, FObject dat, FObject htbl);
void EphemeronKeySet(FObject eph, FObject key);
void EphemeronDatumSet(FObject eph, FObject dat);

#define EPHEMERON_BROKEN ((FEphemeron *) -1)

inline long_t EphemeronBrokenP(FObject obj)
{
    FAssert(EphemeronP(obj));

    return(AsEphemeron(obj)->Next == EPHEMERON_BROKEN);
}

// ---- Roots ----

void RegisterRoot(FObject * root, const char * name);

extern FObject Bedrock;
extern FObject BedrockLibrary;
extern FObject Features;
extern FObject CommandLine;
extern FObject FullCommandLine;
extern FObject EnvironmentVariables;
extern FObject LibraryPath;
extern FObject LibraryExtensions;
extern FObject LoadedLibraries;
extern FObject SymbolHashTable;
extern FObject StandardInput;
extern FObject StandardOutput;
extern FObject StandardError;
extern FObject InteractiveThunk;
extern FObject CleanupTConc;

// ---- Eternal Objects ----

extern FObject BeginSymbol;
extern FObject QuoteSymbol;
extern FObject QuasiquoteSymbol;
extern FObject UnquoteSymbol;
extern FObject UnquoteSplicingSymbol;
extern FObject SigIntSymbol;
extern FObject WrongNumberOfArguments;
extern FObject Assertion;
extern FObject Restriction;
extern FObject Lexical;
extern FObject Syntax;
extern FObject Error;
extern FObject FileErrorSymbol;
extern FObject NoValuePrimitive;

// ---- Flonums ----

#define FlonumP(obj) (IndirectTag(obj) == FlonumTag)
#define AsFlonum(obj) (((FFlonum *) (obj))->Double)

typedef struct
{
    double64_t Double;
} FFlonum;

FObject MakeFlonum(double64_t dbl);

// ---- Bignums ----

#define BignumP(obj) (IndirectTag(obj) == BignumTag)
void DeleteBignum(FObject obj);

// ---- Ratios ----

#define RatioP(obj) (IndirectTag(obj) == RatioTag)
#define AsRatio(obj) ((FRatio *) (obj))

typedef struct
{
    FObject Numerator;
    FObject Denominator;
} FRatio;

FObject MakeRatio(FObject nmr, FObject dnm);

// ---- Complex ----

#define ComplexP(obj) (IndirectTag(obj) == ComplexTag)
#define AsComplex(obj) ((FComplex *) (obj))

typedef struct
{
    FObject Real;
    FObject Imaginary;
} FComplex;

// ---- Numbers ----

long_t GenericEqvP(FObject x1, FObject x2);

FObject StringToNumber(FCh * s, long_t sl, long_t rdx);
FObject NumberToString(FObject obj, long_t rdx);

long_t FixnumAsString(long_t n, FCh * s, long_t rdx);

inline long_t NumberP(FObject obj)
{
    if (FixnumP(obj))
        return(1);

    FIndirectTag tag = IndirectTag(obj);
    return(tag == BignumTag || tag == RatioTag || tag == FlonumTag || tag == ComplexTag);
}

inline long_t RealP(FObject obj)
{
    if (FixnumP(obj))
        return(1);

    FIndirectTag tag = IndirectTag(obj);
    return(tag == BignumTag || tag == RatioTag || tag == FlonumTag);
}

long_t IntegerP(FObject obj);
long_t RationalP(FObject obj);
long_t NonNegativeExactIntegerP(FObject obj, long_t bf);

#define POSITIVE_INFINITY (DBL_MAX * DBL_MAX)
#define NEGATIVE_INFINITY -POSITIVE_INFINITY

#ifndef NAN
#define NAN log((double) -2)
#endif // NAN

FObject ToInexact(FObject n);
FObject ToExact(FObject n);

FObject MakeInteger(int64_t n);
FObject MakeIntegerU(uint64_t n);
FObject MakeInteger(uint32_t high, uint32_t low);
uint32_t NumberHash(FObject obj);

FObject GenericAdd(FObject z1, FObject z2);
FObject GenericMultiply(FObject z1, FObject z2);

// ---- Environments ----

#define AsEnvironment(obj) ((FEnvironment *) (obj))
#define EnvironmentP(obj) BuiltinP(obj, EnvironmentType)
extern FObject EnvironmentType;

typedef struct
{
    FObject BuiltinType;
    FObject Name;
    FObject HashTable;
    FObject Interactive;
    FObject Immutable;
} FEnvironment;

FObject MakeEnvironment(FObject nam, FObject ctv);
FObject EnvironmentBind(FObject env, FObject sym);
FObject EnvironmentLookup(FObject env, FObject sym);
long_t EnvironmentDefine(FObject env, FObject symid, FObject val);
FObject EnvironmentSet(FObject env, FObject sym, FObject val);
FObject EnvironmentSetC(FObject env, const char * sym, FObject val);
FObject EnvironmentGet(FObject env, FObject symid);

void EnvironmentImportLibrary(FObject env, FObject nam);
void EnvironmentImportSet(FObject env, FObject is, FObject form);
void EnvironmentImmutable(FObject env);

// ---- Globals ----

#define AsGlobal(obj) ((FGlobal *) (obj))
#define GlobalP(obj) BuiltinP(obj, GlobalType)
extern FObject GlobalType;

typedef struct
{
    FObject BuiltinType;
    FObject Box;
    FObject Name;
    FObject Module;
    FObject State; // GlobalUndefined, GlobalDefined, GlobalModified, GlobalImported,
                   // GlobalImportedModified
    FObject Interactive;
} FGlobal;

#define GlobalUndefined MakeFixnum(0)
#define GlobalDefined MakeFixnum(1)
#define GlobalModified MakeFixnum(2)
#define GlobalImported MakeFixnum(3)
#define GlobalImportedModified MakeFixnum(4)

// ---- Libraries ----

#define AsLibrary(obj) ((FLibrary *) (obj))
#define LibraryP(obj) BuiltinP(obj, LibraryType)
extern FObject LibraryType;

typedef struct
{
    FObject BuiltinType;
    FObject Name;
    FObject Exports;
} FLibrary;

FObject MakeLibrary(FObject nam);
void LibraryExport(FObject lib, FObject gl);
FObject OpenFomentLibrary(FObject nam);

// ---- Syntax Rules ----

#define SyntaxRulesP(obj) BuiltinP(obj, SyntaxRulesType)
extern FObject SyntaxRulesType;

// ---- Identifiers ----

#define AsIdentifier(obj) ((FIdentifier *) (obj))
#define IdentifierP(obj) BuiltinP(obj, IdentifierType)
extern FObject IdentifierType;

typedef struct
{
    FObject BuiltinType;
    FObject Symbol;
    FObject Filename;
    FObject SyntacticEnv;
    FObject Wrapped;
    long_t LineNumber;
    long_t Magic;
} FIdentifier;

FObject MakeIdentifier(FObject sym, FObject fn, long_t ln);
FObject MakeIdentifier(FObject sym);
FObject WrapIdentifier(FObject id, FObject se);

// ---- Procedures ----

typedef struct
{
    FObject Name;
    FObject Filename;
    FObject LineNumber;
    FObject Code;
    uint16_t ArgCount;
    uint8_t Flags;
} FProcedure;

#define AsProcedure(obj) ((FProcedure *) (obj))
#define ProcedureP(obj) (IndirectTag(obj) == ProcedureTag)

FObject MakeProcedure(FObject nam, FObject fn, FObject ln, FObject cv, long_t ac, ulong_t fl);

#define PROCEDURE_FLAG_CLOSURE      0x8
#define PROCEDURE_FLAG_PARAMETER    0x4
#define PROCEDURE_FLAG_CONTINUATION 0x2
#define PROCEDURE_FLAG_RESTARG      0x1

// ---- Threads ----

#define ThreadP(obj) (IndirectTag(obj) == ThreadTag)

// ---- Exclusives ----

#define ExclusiveP(obj) (IndirectTag(obj) == ExclusiveTag)

FObject MakeExclusive();

// ---- Conditions ----

#define ConditionP(obj) (IndirectTag(obj) == ConditionTag)

// ---- Exceptions ----

#define AsException(obj) ((FException *) (obj))
#define ExceptionP(obj) BuiltinP(obj, ExceptionType)
extern FObject ExceptionType;

typedef struct
{
    FObject BuiltinType;
    FObject Type; // error, assertion-violation, implementation-restriction, lexical-violation,
                  // syntax-violation, undefined-violation
    FObject Who;
    FObject Kind; // file-error or read-error
    FObject Message; // should be a string
    FObject Irritants; // a list of zero or more irritants
} FException;

FObject MakeException(FObject typ, FObject who, FObject knd, FObject msg, FObject lst);

void Raise(FObject obj);

void RaiseException(FObject typ, FObject who, FObject knd, FObject msg, FObject lst);
inline void RaiseException(FObject typ, FObject who, FObject msg, FObject lst)
{
    RaiseException(typ, who, NoValueObject, msg, lst);
}

void RaiseExceptionC(FObject typ, const char * who, FObject knd, const char * msg, FObject lst);
inline void RaiseExceptionC(FObject typ, const char * who, const char * msg, FObject lst)
{
    RaiseExceptionC(typ, who, NoValueObject, msg, lst);
}

// ---- Thread State ----

typedef struct _FYoungSection
{
    struct _FYoungSection * Next;
    ulong_t Used;
    ulong_t Scan;
#ifdef FOMENT_32BIT
    ulong_t Pad;
#endif // FOMENT_32BIT
} FYoungSection;

#define INDEX_PARAMETERS 5
#define INDEX_PARAMETER_CURRENT_INPUT_PORT 0
#define INDEX_PARAMETER_CURRENT_OUTPUT_PORT 1
#define INDEX_PARAMETER_CURRENT_ERROR_PORT 2
#define INDEX_PARAMETER_HASH_BOUND 3
#define INDEX_PARAMETER_HASH_SALT 4

typedef struct _FThreadState
{
    struct _FThreadState * Next;
    struct _FThreadState * Previous;

    FObject Thread;

    FAlive * AliveList;

    ulong_t ObjectsSinceLast;
    ulong_t BytesSinceLast;

    FMemRegion Babies;
    ulong_t BabiesUsed;

    FMemRegion Stack;
    long_t AStackPtr;
    long_t AStackUsed;
    FObject * AStack;
    long_t CStackPtr;
    long_t CStackUsed;
    FObject * CStack;

    FObject Proc;
    FObject Frame;
    long_t IP;
    long_t ArgCount;

    FObject DynamicStack;
    FObject Parameters;
    FObject IndexParameters[INDEX_PARAMETERS];

    long_t NotifyFlag;
    FObject NotifyObject;

    ulong_t ExceptionCount;
} FThreadState;

// ---- Argument Checking ----

inline void ZeroArgsCheck(const char * who, long_t argc)
{
    if (argc != 0)
        RaiseExceptionC(Assertion, who, "expected no arguments", EmptyListObject);
}

inline void OneArgCheck(const char * who, long_t argc)
{
    if (argc != 1)
        RaiseExceptionC(Assertion, who, "expected one argument", EmptyListObject);
}

inline void TwoArgsCheck(const char * who, long_t argc)
{
    if (argc != 2)
        RaiseExceptionC(Assertion, who, "expected two arguments", EmptyListObject);
}

inline void ThreeArgsCheck(const char * who, long_t argc)
{
    if (argc != 3)
        RaiseExceptionC(Assertion, who, "expected three arguments", EmptyListObject);
}

inline void FourArgsCheck(const char * who, long_t argc)
{
    if (argc != 4)
        RaiseExceptionC(Assertion, who, "expected four arguments", EmptyListObject);
}

inline void FiveArgsCheck(const char * who, long_t argc)
{
    if (argc != 5)
        RaiseExceptionC(Assertion, who, "expected five arguments", EmptyListObject);
}

inline void SixArgsCheck(const char * who, long_t argc)
{
    if (argc != 6)
        RaiseExceptionC(Assertion, who, "expected six arguments", EmptyListObject);
}

inline void SevenArgsCheck(const char * who, long_t argc)
{
    if (argc != 7)
        RaiseExceptionC(Assertion, who, "expected seven arguments", EmptyListObject);
}

inline void AtLeastOneArgCheck(const char * who, long_t argc)
{
    if (argc < 1)
        RaiseExceptionC(Assertion, who, "expected at least one argument", EmptyListObject);
}

inline void AtLeastTwoArgsCheck(const char * who, long_t argc)
{
    if (argc < 2)
        RaiseExceptionC(Assertion, who, "expected at least two arguments", EmptyListObject);
}

inline void AtLeastThreeArgsCheck(const char * who, long_t argc)
{
    if (argc < 3)
        RaiseExceptionC(Assertion, who, "expected at least three arguments", EmptyListObject);
}

inline void AtLeastFourArgsCheck(const char * who, long_t argc)
{
    if (argc < 4)
        RaiseExceptionC(Assertion, who, "expected at least four arguments", EmptyListObject);
}

inline void ZeroOrOneArgsCheck(const char * who, long_t argc)
{
    if (argc > 1)
        RaiseExceptionC(Assertion, who, "expected zero or one arguments", EmptyListObject);
}

inline void ZeroToTwoArgsCheck(const char * who, long_t argc)
{
    if (argc > 2)
        RaiseExceptionC(Assertion, who, "expected zero to two arguments", EmptyListObject);
}

inline void OneOrTwoArgsCheck(const char * who, long_t argc)
{
    if (argc < 1 || argc > 2)
        RaiseExceptionC(Assertion, who, "expected one or two arguments", EmptyListObject);
}

inline void OneToThreeArgsCheck(const char * who, long_t argc)
{
    if (argc < 1 || argc > 3)
        RaiseExceptionC(Assertion, who, "expected one to three arguments", EmptyListObject);
}

inline void OneToFourArgsCheck(const char * who, long_t argc)
{
    if (argc < 1 || argc > 4)
        RaiseExceptionC(Assertion, who, "expected one to four arguments", EmptyListObject);
}

inline void TwoOrThreeArgsCheck(const char * who, long_t argc)
{
    if (argc < 2 || argc > 3)
        RaiseExceptionC(Assertion, who, "expected two or three arguments", EmptyListObject);
}

inline void TwoToFourArgsCheck(const char * who, long_t argc)
{
    if (argc < 2 || argc > 4)
        RaiseExceptionC(Assertion, who, "expected two to four arguments", EmptyListObject);
}

inline void ThreeToFiveArgsCheck(const char * who, long_t argc)
{
    if (argc < 3 || argc > 5)
        RaiseExceptionC(Assertion, who, "expected three to five arguments", EmptyListObject);
}

inline void NonNegativeArgCheck(const char * who, FObject arg, long_t bf)
{
    if (NonNegativeExactIntegerP(arg, bf) == 0)
        RaiseExceptionC(Assertion, who, "expected an exact non-negative integer", List(arg));
}

inline void IndexArgCheck(const char * who, FObject arg, long_t len)
{
    if (FixnumP(arg) == 0 || AsFixnum(arg) < 0 || AsFixnum(arg) > len)
        RaiseExceptionC(Assertion, who, "expected a valid index", List(arg));
}

inline void EndIndexArgCheck(const char * who, FObject arg, long_t strt, long_t len)
{
    if (FixnumP(arg) == 0 || AsFixnum(arg) < strt || AsFixnum(arg) > len)
        RaiseExceptionC(Assertion, who, "expected a valid index", List(arg));
}

inline void ByteArgCheck(const char * who, FObject obj)
{
    if (FixnumP(obj) == 0 || AsFixnum(obj) < 0 || AsFixnum(obj) > 0xFF)
        RaiseExceptionC(Assertion, who, "expected a byte: an exact integer between 0 and 255",
                List(obj));
}

inline void FixnumArgCheck(const char * who, FObject obj)
{
    if (FixnumP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected an exact integer", List(obj));
}

inline void IntegerArgCheck(const char * who, FObject obj)
{
    if (IntegerP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected an integer", List(obj));
}

inline void RealArgCheck(const char * who, FObject obj)
{
    if (RealP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a real number", List(obj));
}

inline void RationalArgCheck(const char * who, FObject obj)
{
    if (RationalP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a rational number", List(obj));
}

inline void NumberArgCheck(const char * who, FObject obj)
{
    if (NumberP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a number", List(obj));
}

inline void CharacterArgCheck(const char * who, FObject obj)
{
    if (CharacterP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a character", List(obj));
}

inline void BooleanArgCheck(const char * who, FObject obj)
{
    if (BooleanP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a boolean", List(obj));
}

inline void SymbolArgCheck(const char * who, FObject obj)
{
    if (SymbolP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a symbol", List(obj));
}

inline void ExceptionArgCheck(const char * who, FObject obj)
{
    if (ExceptionP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected an error-object", List(obj));
}

inline void StringArgCheck(const char * who, FObject obj)
{
    if (StringP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a string", List(obj));
}

inline void PairArgCheck(const char * who, FObject obj)
{
    if (PairP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a pair", List(obj));
}

inline void ListArgCheck(const char * who, FObject obj)
{
    ListLength(who, obj);
}

inline void BoxArgCheck(const char * who, FObject obj)
{
    if (BoxP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a box", List(obj));
}

inline void VectorArgCheck(const char * who, FObject obj)
{
    if (VectorP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a vector", List(obj));
}

inline void BytevectorArgCheck(const char * who, FObject obj)
{
    if (BytevectorP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a bytevector", List(obj));
}

inline void EnvironmentArgCheck(const char * who, FObject obj)
{
    if (EnvironmentP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected an environment", List(obj));
}

inline void PortArgCheck(const char * who, FObject obj)
{
    if (BinaryPortP(obj) == 0 && TextualPortP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a port", List(obj));
}

inline void InputPortArgCheck(const char * who, FObject obj)
{
    if ((BinaryPortP(obj) == 0 && TextualPortP(obj) == 0) || InputPortP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected an input port", List(obj));
}

inline void OutputPortArgCheck(const char * who, FObject obj)
{
    if ((BinaryPortP(obj) == 0 && TextualPortP(obj) == 0) || OutputPortP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected an output port", List(obj));
}

inline void BinaryPortArgCheck(const char * who, FObject obj)
{
    if (BinaryPortP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a binary port", List(obj));
}

inline void TextualPortArgCheck(const char * who, FObject obj)
{
    if (TextualPortP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a textual port", List(obj));
}

inline void StringOutputPortArgCheck(const char * who, FObject obj)
{
    if (StringOutputPortP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a string output port", List(obj));
}

inline void BytevectorOutputPortArgCheck(const char * who, FObject obj)
{
    if (BytevectorOutputPortP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a bytevector output port", List(obj));
}

inline void TextualInputPortArgCheck(const char * who, FObject obj)
{
    if (TextualPortP(obj) == 0 || InputPortOpenP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected an open textual input port", List(obj));
}

inline void TextualOutputPortArgCheck(const char * who, FObject obj)
{
    if (TextualPortP(obj) == 0 || OutputPortOpenP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected an open textual output port", List(obj));
}

inline void BinaryInputPortArgCheck(const char * who, FObject obj)
{
    if (BinaryPortP(obj) == 0 || InputPortOpenP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected an open binary input port", List(obj));
}

inline void BinaryOutputPortArgCheck(const char * who, FObject obj)
{
    if (BinaryPortP(obj) == 0 || OutputPortOpenP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected an open binary output port", List(obj));
}

inline void ConsoleInputPortArgCheck(const char * who, FObject obj)
{
    if (ConsolePortP(obj) == 0 || InputPortOpenP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected an open console input port", List(obj));
}

inline void SocketPortArgCheck(const char * who, FObject obj)
{
    if (SocketPortP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a socket port", List(obj));
}

inline void OpenPositioningPortArgCheck(const char * who, FObject obj)
{
    if (PortOpenP(obj) == 0 || PositioningPortP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected an open port which supports positioning",
                List(obj));
}

inline void ProcedureArgCheck(const char * who, FObject obj)
{
    if (ProcedureP(obj) == 0 && PrimitiveP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a procedure", List(obj));
}

inline void ThreadArgCheck(const char * who, FObject obj)
{
    if (ThreadP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a thread", List(obj));
}

inline void ExclusiveArgCheck(const char * who, FObject obj)
{
    if (ExclusiveP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected an exclusive", List(obj));
}

inline void ConditionArgCheck(const char * who, FObject obj)
{
    if (ConditionP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a condition", List(obj));
}

inline void TConcArgCheck(const char * who, FObject obj)
{
    if (PairP(obj) == 0 || PairP(First(obj)) == 0 || PairP(Rest(obj)) == 0)
        RaiseExceptionC(Assertion, who, "expected a tconc", List(obj));
}

inline void ComparatorArgCheck(const char * who, FObject obj)
{
    if (ComparatorP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a comparator", List(obj));
}

inline void EphemeronArgCheck(const char * who, FObject obj)
{
    if (EphemeronP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected an ephemeron", List(obj));
}

inline void SymbolStringArgCheck(const char * who, FObject obj)
{
    if (StringP(obj) == 0 && CStringP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a symbol string", List(obj));
}

// ----------------

extern ulong_t CheckHeapFlag;
extern ulong_t VerboseFlag;
extern ulong_t RandomSeed;
extern volatile ulong_t BytesAllocated;

FObject CompileProgram(FObject nam, FObject port);
FObject Eval(FObject obj, FObject env);
FObject GetInteractionEnv();

FObject SyntaxToDatum(FObject obj);

FObject ExecuteProc(FObject op);

long_t SetupFoment(FThreadState * ts);
extern ulong_t SetupComplete;
void ExitFoment();
void ErrorExitFoment();

// ---- Do Not Call Directly ----

long_t SetupCore(FThreadState * ts);
void SetupLibrary();
void SetupPairs();
void SetupCharacters();
void SetupStrings();
void SetupVectors();
void SetupHashTables();
void SetupCompare();
void SetupIO();
void SetupFileSys();
void SetupCompile();
void SetupExecute();
void SetupNumbers();
void SetupThreads();
void SetupGC();
void SetupMain();

void WriteSpecialSyntax(FWriteContext * wctx, FObject obj);
void WriteInstruction(FWriteContext * wctx, FObject obj);
void WriteThread(FWriteContext * wctx, FObject obj);
void WriteExclusive(FWriteContext * wctx, FObject obj);
void WriteCondition(FWriteContext * wctx, FObject obj);

#ifdef FOMENT_WINDOWS
#define PathCh '\\'
#define PathSep ';'
inline long_t PathChP(FCh ch)
{
    return(ch == '\\' || ch == '/');
}
#else // FOMENT_WINDOWS
#define PathCh '/'
#define PathSep ':'
inline long_t PathChP(FCh ch)
{
    return(ch == '/');
}
#endif // FOMENT_WINDOWS

#endif // __FOMENT_HPP__
