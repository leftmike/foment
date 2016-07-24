/*

Foment

To Do:
-- Windows: make test-all: run all tests using all three collectors
-- CheckObject: check back references from mature objects
-- partial GC
-- after partial GC, test for objects pointing to Babies
-- Use extra generation for immortal objects which are precompiled libraries

HashTree and Comparator:
-- get rid of DefineComparator
-- replace srfi 114 with srfi 128
-- Change all Compare to OrderingP
-- EqComparator and AnyPPimitive
-- DefaultComparator
-- probably replace HashTree with HashTable
-- add sets: HashSet
-- add bags: HashBag

Future:
-- change EternalSymbol (and Define) to set Symbol->Hash at compile time
-- allow larger objects by using BlockSize > 1 in FObjHdr
-- maybe add ObjectHash like MIT Scheme and get rid of trackers
-- pull options from FOMENT_OPTIONS environment variable
-- features, command-line, full-command-line, interactive options,
    environment-variables, etc passed to scheme as a single assoc list
-- number.cpp: make NumberP, BinaryNumberOp, and UnaryNumberOp faster
-- Windows: $(APPDATA)\Foment\Libraries
-- Unix: $(HOME)/.local/foment/lib
-- on unix, if gmp is available, use it instead of mini-gmp
-- replace mini-gmp
-- increase maximum/minimum fixnum on 64bit
-- some mature segments are compacted during a full collection; ie. mark-compact
-- inline primitives in GPassExpression
-- debugging information
-- composable continuations
-- strings and srfi-13
-- from Gambit:
    Serial numbers are used by the printer to identify objects which can’t be read
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
-- serialize symbol table
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
typedef uint32_t FImmediate;
typedef int32_t FFixnum;
typedef int32_t int_t;
typedef uint32_t uint_t;

#define INT_FMT "%d"
#define UINT_FMT "%u"
#endif // FOMENT_32BIT

#ifdef FOMENT_64BIT
typedef uint64_t FImmediate;
typedef int64_t FFixnum;
typedef int64_t int_t;
typedef uint64_t uint_t;

#ifdef FOMENT_OSX
#define INT_FMT "%lld"
#define UINT_FMT "%llu"
#else // FOMENT_OSX
#define INT_FMT "%ld"
#define UINT_FMT "%lu"
#endif // FOMENT_OSX
#endif // FOMENT_64BIT

#ifdef FOMENT_DEBUG
void FAssertFailed(const char * fn, int_t ln, const char * expr);
#define FAssert(expr)\
    if (! (expr)) FAssertFailed( __FILE__, __LINE__, #expr)
#else // FOMENT_DEBUG
#define FAssert(expr)
#endif // FOMENT_DEBUG

void FMustBeFailed(const char * fn, int_t ln, const char * expr);
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
    HashTreeTag,
    EphemeronTag,
    BuiltinTypeTag,
    BuiltinTag,
    FreeTag, // Only on Adult Generation
    BadDogTag // Invalid Tag
} FIndirectTag;

#define ObjectP(obj) ((((FImmediate) (obj)) & 0x7) == 0x0)

#define ImmediateTag(obj) (((FImmediate) (obj)) & 0x7)
#define ImmediateP(obj, it) (ImmediateTag((obj)) == it)
#define MakeImmediate(val, it)\
    ((FObject *) ((((FImmediate) (val)) << 3) | (it & 0x7)))
#define AsValue(obj) (((FImmediate) (obj)) >> 3)

#define FixnumP(obj) ImmediateP(obj, FixnumTag)
#define MakeFixnum(n)\
    ((FObject *) ((((FFixnum) (n)) << 3) | (FixnumTag & 0x7)))
#define AsFixnum(obj) (((FFixnum) (obj)) >> 3)

#define MAXIMUM_FIXNUM ((((FFixnum) 1) << (sizeof(int32_t) * 8 - 4)) - 1)
#define MINIMUM_FIXNUM (- MAXIMUM_FIXNUM)

// ---- Memory Management ----

typedef enum
{
    NoCollector,
    MarkSweepCollector,
    GenerationalCollector
} FCollectorType;

extern FCollectorType CollectorType;

typedef struct
{
    uint_t TopUsed;
    uint_t BottomUsed;
    uint_t MaximumSize;
    void * Base;
} FMemRegion;

void * InitializeMemRegion(FMemRegion * mrgn, uint_t max);
void DeleteMemRegion(FMemRegion * mrgn);
int_t GrowMemRegionUp(FMemRegion * mrgn, uint_t sz);
int_t GrowMemRegionDown(FMemRegion * mrgn, uint_t sz);

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
    uint_t BlockSize() {return(((uint_t) 1) << (BlockSizeAndCount >> OBJHDR_SIZE_SHIFT));}
    uint_t BlockCount() {return(BlockSizeAndCount & OBJHDR_COUNT_MASK);}
public:
    uint_t ObjectSize();
    uint_t TotalSize();
    uint_t SlotCount();
    uint_t ByteLength();
    uint32_t Tag() {return(FlagsAndTag & OBJHDR_TAG_MASK);}
    FObject * Slots() {return((FObject *) (this + 1));}
    uint32_t Generation() {return(FlagsAndTag & OBJHDR_GEN_MASK);}
} FObjHdr;

// Allocated size of the object in bytes, not including the ObjHdr and ObjFtr.
inline uint_t FObjHdr::ObjectSize()
{
    FAssert(BlockSize() * BlockCount() > 0);

    return(BlockSize() * BlockCount() * sizeof(struct _FObjHdr));
}

// Number of FObjects which must be at the beginning of the object.
inline uint_t FObjHdr::SlotCount()
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
inline uint_t FObjHdr::ByteLength()
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

inline uint_t ByteLength(FObject obj)
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
    {sizeof(type) / sizeof(FObjHdr), (sizeof(type) / sizeof(FObjHdr)) - sc, \
            tag | OBJHDR_GEN_ETERNAL | OBJHDR_HAS_SLOTS}

#define EternalObjFtr \
    {{OBJFTR_FEET, OBJFTR_FEET}}

FObject MakeObject(uint_t tag, uint_t sz, uint_t sc, const char * who, int_t pf = 0);

inline FIndirectTag IndirectTag(FObject obj)
{
    return((FIndirectTag) (ObjectP(obj) ? AsObjHdr(obj)->Tag() : 0));
}

extern volatile int_t GCRequired;

void EnterWait();
void LeaveWait();

void CheckHeap(const char * fn, int ln);
void ReadyForGC();
#define CheckForGC() if (GCRequired) ReadyForGC()

inline int_t MatureP(FObject obj)
{
    return(ObjectP(obj) && (AsObjHdr(obj)->FlagsAndTag & OBJHDR_GEN_MASK) == OBJHDR_GEN_ADULTS);
}

void ModifyVector(FObject obj, uint_t idx, FObject val);

/*
//    AsProcedure(proc)->Name = nam;
    Modify(FProcedure, proc, Name, nam);
*/
#define Modify(type, obj, slot, val)\
    ModifyObject(obj, (uint_t) &(((type *) 0)->slot), val)

// Do not directly call ModifyObject; use Modify instead.
void ModifyObject(FObject obj, uint_t off, FObject val);

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

extern uint_t MaximumStackSize;
extern uint_t MaximumBabiesSize;
extern uint_t MaximumKidsSize;
extern uint_t MaximumAdultsSize;
extern uint_t MaximumGenerationalBaby;
extern uint_t TriggerObjects;
extern uint_t TriggerBytes;
extern uint_t PartialPerFull;

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
#define AsValuesCount(obj) ((FFixnum) (AsValue(obj)))

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
int_t ListLength(FObject lst);
int_t ListLength(const char * nam, FObject lst);
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
int_t TConcEmptyP(FObject tconc);
void TConcAdd(FObject tconc, FObject obj);
FObject TConcRemove(FObject tconc);

// ---- Boxes ----

#define BoxP(obj) (IndirectTag(obj) == BoxTag)
#define AsBox(obj) ((FBox *) (obj))

typedef struct
{
    FObject Value;
    uint_t Index;
} FBox;

FObject MakeBox(FObject val);
FObject MakeBox(FObject val, uint_t idx);
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

FObject MakeString(FCh * s, uint_t sl);
FObject MakeStringCh(uint_t sl, FCh ch);
FObject MakeStringC(const char * s);
FObject MakeStringS(FChS * ss);
FObject MakeStringS(FChS * ss, uint_t ssl);

inline uint_t StringLength(FObject obj)
{
    FAssert(StringP(obj));
    FAssert(ByteLength(obj) % sizeof(FCh) == 0);
    FAssert(ByteLength(obj) >= sizeof(FCh));

    return((ByteLength(obj) / sizeof(FCh)) - 1);
}

void StringToC(FObject s, char * b, int_t bl);
FObject FoldcaseString(FObject s);
uint32_t StringLengthHash(FCh * s, uint_t sl);
uint32_t StringHash(FObject obj);
uint32_t StringCiHash(FObject obj);
uint32_t CStringHash(const char * s);
int_t StringLengthEqualP(FCh * s, int_t sl, FObject obj);
int_t StringCEqualP(const char * s1, FCh * s2, int_t sl2);
int_t StringCompare(FObject obj1, FObject obj2);
int_t StringCiCompare(FObject obj1, FObject obj2);

// ---- Vectors ----

#define VectorP(obj) (IndirectTag(obj) == VectorTag)
#define AsVector(obj) ((FVector *) (obj))

typedef struct
{
    FObject Vector[1];
} FVector;

FObject MakeVector(uint_t vl, FObject * v, FObject obj);
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

FObject MakeBytevector(uint_t vl);
FObject U8ListToBytevector(FObject obj);
uint_t BytevectorHash(FObject obj);
int_t BytevectorCompare(FObject obj1, FObject obj2);

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
    uint_t Flags;
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

inline int_t InputPortP(FObject obj)
{
    return((BinaryPortP(obj) || TextualPortP(obj))
            && (AsGenericPort(obj)->Flags & PORT_FLAG_INPUT));
}

inline int_t OutputPortP(FObject obj)
{
    return((BinaryPortP(obj) || TextualPortP(obj))
            && (AsGenericPort(obj)->Flags & PORT_FLAG_OUTPUT));
}

inline int_t InputPortOpenP(FObject obj)
{
    FAssert(BinaryPortP(obj) || TextualPortP(obj));

    return(AsGenericPort(obj)->Flags & PORT_FLAG_INPUT_OPEN);
}

inline int_t OutputPortOpenP(FObject obj)
{
    FAssert(BinaryPortP(obj) || TextualPortP(obj));

    return(AsGenericPort(obj)->Flags & PORT_FLAG_OUTPUT_OPEN);
}

inline int_t PortOpenP(FObject obj)
{
    FAssert(BinaryPortP(obj) || TextualPortP(obj));

    return((AsGenericPort(obj)->Flags & PORT_FLAG_INPUT_OPEN) ||
            (AsGenericPort(obj)->Flags & PORT_FLAG_OUTPUT_OPEN));
}

inline int_t StringOutputPortP(FObject obj)
{
    return(TextualPortP(obj) && (AsGenericPort(obj)->Flags & PORT_FLAG_STRING_OUTPUT));
}

inline int_t BytevectorOutputPortP(FObject obj)
{
    return(BinaryPortP(obj) && (AsGenericPort(obj)->Flags & PORT_FLAG_BYTEVECTOR_OUTPUT));
}

inline int_t FoldcasePortP(FObject port)
{
    FAssert(TextualPortP(port) && InputPortP(port));

    return(AsGenericPort(port)->Flags & PORT_FLAG_FOLDCASE);
}

inline int_t WantIdentifiersPortP(FObject port)
{
    FAssert(TextualPortP(port) && InputPortP(port));

    return(AsGenericPort(port)->Flags & PORT_FLAG_WANT_IDENTIFIERS);
}

inline int_t ConsolePortP(FObject port)
{
    return(TextualPortP(port) && (AsGenericPort(port)->Flags & PORT_FLAG_CONSOLE));
}

inline int_t SocketPortP(FObject port)
{
    return(BinaryPortP(port) && (AsGenericPort(port)->Flags & PORT_FLAG_SOCKET));
}

inline int_t PositioningPortP(FObject obj)
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

uint_t ReadBytes(FObject port, FByte * b, uint_t bl);
int_t PeekByte(FObject port, FByte * b);
int_t ByteReadyP(FObject port);
uint_t GetOffset(FObject port);

void WriteBytes(FObject port, void * b, uint_t bl);

// Textual ports

FObject OpenInputFile(FObject fn);
FObject OpenOutputFile(FObject fn);
FObject MakeStringInputPort(FObject str);
FObject MakeStringCInputPort(const char * s);
FObject MakeStringOutputPort();
FObject GetOutputString(FObject port);

uint_t ReadCh(FObject port, FCh * ch);
uint_t PeekCh(FObject port, FCh * ch);
int_t CharReadyP(FObject port);
FObject ReadLine(FObject port);
FObject ReadString(FObject port, uint_t cnt);
uint_t GetLineColumn(FObject port, uint_t * col);
FObject GetFilename(FObject port);
void FoldcasePort(FObject port, int_t fcf);
void WantIdentifiersPort(FObject port, int_t wif);

FObject Read(FObject port);

void WriteCh(FObject port, FCh ch);
void WriteString(FObject port, FCh * s, uint_t sl);
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
    FWriteContext(FObject port, int_t df);
    void Prepare(FObject obj, FWriteType wt);

    void Write(FObject obj);
    void Display(FObject obj);
    void WriteCh(FCh ch);
    void WriteString(FCh * s, uint_t sl);
    void WriteStringC(const char * s);

private:

    FObject Port;
    int_t DisplayFlag;
    FWriteType WriteType;
    FObject HashMap;
    int_t SharedCount;
    int_t PreviousLabel;

    void FindSharedObjects(FObject obj, FWriteType wt);
    void WritePair(FObject obj);
    void WriteRecord(FObject obj);
    void WriteObject(FObject obj);
    void WriteSimple(FObject obj);
};

void Write(FObject port, FObject obj, int_t df);
void WriteShared(FObject port, FObject obj, int_t df);
void WriteSimple(FObject port, FObject obj, int_t df);

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

FObject MakeBuiltin(FObject bt, uint_t sz, uint_t sc, const char * who);

inline int_t BuiltinP(FObject obj, FObject bt)
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

FObject MakeRecordType(FObject nam, uint_t nf, FObject flds[]);

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

inline int_t RecordP(FObject obj, FObject rt)
{
    return(GenericRecordP(obj) && AsGenericRecord(obj)->Fields[0] == rt);
}

#define RecordNumFields(obj) (AsObjHdr(obj)->SlotCount())

// ---- HashTree ----

#define HashTreeP(obj) (IndirectTag(obj) == HashTreeTag)
#define AsHashTree(obj) ((FHashTree *) (obj))

typedef struct
{
    FObject Buckets[1];
} FHashTree;

FObject MakeHashTree(const char * who);

#define HashTreeLength(obj) (AsObjHdr(obj)->SlotCount())
#define HashTreeBitmap(obj) (* ((uint_t *) (AsHashTree(obj)->Buckets + HashTreeLength(obj))))

// ---- Comparator ----

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

void DefineComparator(const char * nam, FObject ttp, FObject eqp, FObject orderp, FObject hashfn);

int_t EqP(FObject obj1, FObject obj2);
int_t EqvP(FObject obj1, FObject obj2);
int_t EqualP(FObject obj1, FObject obj2);

uint_t EqHash(FObject obj);
extern FObject EqHashPrimitive;

// ---- HashContainer ----

#define HashContainerP(obj) (HashMapP(obj) || HashSetP(obj) || HashBagP(obj))
#define AsHashContainer(obj) ((FHashContainer *) (obj))

typedef struct
{
    FObject BuiltinType;
    FObject HashTree;
    FObject Comparator;
    FObject Tracker;
    FObject Size;
} FHashContainer;

// ---- HashMap ----

#define HashMapP(obj) BuiltinP(obj, HashMapType)
extern FObject HashMapType;

typedef int_t (*FEquivFn)(FObject obj1, FObject obj2);
typedef uint_t (*FHashFn)(FObject obj);
typedef void (*FVisitFn)(FObject key, FObject val, FObject ctx);

typedef FHashContainer FHashMap;

FObject MakeEqHashMap();
FObject EqHashMapRef(FObject hmap, FObject key, FObject def);
void EqHashMapSet(FObject hmap, FObject key, FObject val);
void EqHashMapDelete(FObject hmap, FObject key);
void EqHashMapVisit(FObject hmap, FVisitFn vfn, FObject ctx);

// ---- HashSet ----

#define HashSetP(obj) BuiltinP(obj, HashSetType)
extern FObject HashSetType;

typedef FHashContainer FHashSet;

FObject MakeEqHashSet();
int_t EqHashSetContainsP(FObject hset, FObject elem);
void EqHashSetAdjoin(FObject hset, FObject elem);
void EqHashSetDelete(FObject hset, FObject elem);

// ---- HashBag ----

#define HashBagP(obj) BuiltinP(obj, HashBagType)
extern FObject HashBagType;

typedef FHashContainer FHashBag;

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

FObject SymbolToString(FObject sym);
FObject StringToSymbol(FObject str);
FObject StringCToSymbol(const char * s);
FObject StringLengthToSymbol(FCh * s, int_t sl);
int_t SymbolCompare(FObject obj1, FObject obj2);
FObject AddPrefixToSymbol(FObject str, FObject sym);

#define CStringP(obj) (IndirectTag(obj) == CStringTag)
#define AsCString(obj) ((FCString *) (obj))

typedef struct
{
    const char * String;
#ifdef FOMENT_32BIT
    uint_t Unused;
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

FObject InternSymbol(FObject sym);

inline uint_t SymbolHash(FObject obj)
{
    FAssert(SymbolP(obj));
    FAssert((StringP(AsSymbol(obj)->String) &&
                AsSymbol(obj)->Hash == StringHash(AsSymbol(obj)->String)) ||
           (CStringP(AsSymbol(obj)->String) &&
                AsSymbol(obj)->Hash == CStringHash(AsCString(AsSymbol(obj)->String)->String)));

    return(AsSymbol(obj)->Hash);
}

// ---- Primitives ----

#define PrimitiveP(obj) (IndirectTag(obj) == PrimitiveTag)
#define AsPrimitive(obj) ((FPrimitive *) (obj))

typedef FObject (*FPrimitiveFn)(int_t argc, FObject argv[]);
typedef struct
{
    FObject Name;
    FPrimitiveFn PrimitiveFn;
    const char * Filename;
    int_t LineNumber;
} FPrimitive;

typedef struct FALIGN
{
    FObjHdr ObjHdr;
    FPrimitive Primitive;
    FObjFtr ObjFtr;
} FEternalPrimitive;

#define Define(name, prim) \
    EternalSymbol(prim ## Symbol, name); \
    FObject prim ## Fn(int_t argc, FObject argv[]);\
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
    int_t Broken;
    struct _FEphemeron * Next; // Used during garbage collection.
} FEphemeron;

FObject MakeEphemeron(FObject key, FObject dat);
inline int_t EphemeronBrokenP(FObject obj)
{
    FAssert(EphemeronP(obj));

    return(AsEphemeron(obj)->Broken);
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
extern FObject SymbolHashTree;
extern FObject EqComparator;
extern FObject DefaultComparator;
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

// ---- Ratios ----

#define RatioP(obj) (IndirectTag(obj) == RatioTag)
#define AsRatio(obj) ((FRatio *) (obj))

typedef struct
{
    FObject Numerator;
    FObject Denominator;
} FRatio;

// ---- Complex ----

#define ComplexP(obj) (IndirectTag(obj) == ComplexTag)
#define AsComplex(obj) ((FComplex *) (obj))

typedef struct
{
    FObject Real;
    FObject Imaginary;
} FComplex;

// ---- Numbers ----

int_t GenericEqvP(FObject x1, FObject x2);

FObject StringToNumber(FCh * s, int_t sl, FFixnum rdx);
FObject NumberToString(FObject obj, FFixnum rdx);

int_t FixnumAsString(FFixnum n, FCh * s, FFixnum rdx);

inline int_t NumberP(FObject obj)
{
    if (FixnumP(obj))
        return(1);

    FIndirectTag tag = IndirectTag(obj);
    return(tag == BignumTag || tag == RatioTag || tag == FlonumTag || tag == ComplexTag);
}

inline int_t RealP(FObject obj)
{
    if (FixnumP(obj))
        return(1);

    FIndirectTag tag = IndirectTag(obj);
    return(tag == BignumTag || tag == RatioTag || tag == FlonumTag);
}

int_t IntegerP(FObject obj);
int_t RationalP(FObject obj);
int_t NonNegativeExactIntegerP(FObject obj, int_t bf);

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
uint_t NumberHash(FObject obj);
int_t NumberCompare(FObject obj1, FObject obj2);

// ---- Environments ----

#define AsEnvironment(obj) ((FEnvironment *) (obj))
#define EnvironmentP(obj) BuiltinP(obj, EnvironmentType)
extern FObject EnvironmentType;

typedef struct
{
    FObject BuiltinType;
    FObject Name;
    FObject HashMap;
    FObject Interactive;
    FObject Immutable;
} FEnvironment;

FObject MakeEnvironment(FObject nam, FObject ctv);
FObject EnvironmentBind(FObject env, FObject sym);
FObject EnvironmentLookup(FObject env, FObject sym);
int_t EnvironmentDefine(FObject env, FObject symid, FObject val);
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
    int_t LineNumber;
    int_t Magic;
} FIdentifier;

FObject MakeIdentifier(FObject sym, FObject fn, int_t ln);
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

FObject MakeProcedure(FObject nam, FObject fn, FObject ln, FObject cv, int_t ac, uint_t fl);

#define PROCEDURE_FLAG_CLOSURE      0x8
#define PROCEDURE_FLAG_PARAMETER    0x4
#define PROCEDURE_FLAG_CONTINUATION 0x2
#define PROCEDURE_FLAG_RESTARG      0x1

// ---- Threads ----

#define ThreadP(obj) (IndirectTag(obj) == ThreadTag)

// ---- Exclusives ----

#define ExclusiveP(obj) (IndirectTag(obj) == ExclusiveTag)

// ---- Conditions ----

#define ConditionP(obj) (IndirectTag(obj) == ConditionTag)

// ---- Exception ----

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
    uint_t Used;
    uint_t Scan;
#ifdef FOMENT_32BIT
    uint_t Pad;
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

    uint_t ObjectsSinceLast;
    uint_t BytesSinceLast;

    FMemRegion Babies;
    uint_t BabiesUsed;

    FMemRegion Stack;
    int_t AStackPtr;
    FObject * AStack;
    int_t CStackPtr;
    FObject * CStack;

    FObject Proc;
    FObject Frame;
    int_t IP;
    int_t ArgCount;

    FObject DynamicStack;
    FObject Parameters;
    FObject IndexParameters[INDEX_PARAMETERS];

    int_t NotifyFlag;
    FObject NotifyObject;
} FThreadState;

// ---- Argument Checking ----

inline void ZeroArgsCheck(const char * who, int_t argc)
{
    if (argc != 0)
        RaiseExceptionC(Assertion, who, "expected no arguments", EmptyListObject);
}

inline void OneArgCheck(const char * who, int_t argc)
{
    if (argc != 1)
        RaiseExceptionC(Assertion, who, "expected one argument", EmptyListObject);
}

inline void TwoArgsCheck(const char * who, int_t argc)
{
    if (argc != 2)
        RaiseExceptionC(Assertion, who, "expected two arguments", EmptyListObject);
}

inline void ThreeArgsCheck(const char * who, int_t argc)
{
    if (argc != 3)
        RaiseExceptionC(Assertion, who, "expected three arguments", EmptyListObject);
}

inline void FourArgsCheck(const char * who, int_t argc)
{
    if (argc != 4)
        RaiseExceptionC(Assertion, who, "expected four arguments", EmptyListObject);
}

inline void FiveArgsCheck(const char * who, int_t argc)
{
    if (argc != 5)
        RaiseExceptionC(Assertion, who, "expected five arguments", EmptyListObject);
}

inline void SixArgsCheck(const char * who, int_t argc)
{
    if (argc != 6)
        RaiseExceptionC(Assertion, who, "expected six arguments", EmptyListObject);
}

inline void SevenArgsCheck(const char * who, int_t argc)
{
    if (argc != 7)
        RaiseExceptionC(Assertion, who, "expected seven arguments", EmptyListObject);
}

inline void AtLeastOneArgCheck(const char * who, int_t argc)
{
    if (argc < 1)
        RaiseExceptionC(Assertion, who, "expected at least one argument", EmptyListObject);
}

inline void AtLeastTwoArgsCheck(const char * who, int_t argc)
{
    if (argc < 2)
        RaiseExceptionC(Assertion, who, "expected at least two arguments", EmptyListObject);
}

inline void AtLeastThreeArgsCheck(const char * who, int_t argc)
{
    if (argc < 3)
        RaiseExceptionC(Assertion, who, "expected at least three arguments", EmptyListObject);
}

inline void AtLeastFourArgsCheck(const char * who, int_t argc)
{
    if (argc < 4)
        RaiseExceptionC(Assertion, who, "expected at least four arguments", EmptyListObject);
}

inline void ZeroOrOneArgsCheck(const char * who, int_t argc)
{
    if (argc > 1)
        RaiseExceptionC(Assertion, who, "expected zero or one arguments", EmptyListObject);
}

inline void ZeroToTwoArgsCheck(const char * who, int_t argc)
{
    if (argc > 2)
        RaiseExceptionC(Assertion, who, "expected zero to two arguments", EmptyListObject);
}

inline void OneOrTwoArgsCheck(const char * who, int_t argc)
{
    if (argc < 1 || argc > 2)
        RaiseExceptionC(Assertion, who, "expected one or two arguments", EmptyListObject);
}

inline void OneToThreeArgsCheck(const char * who, int_t argc)
{
    if (argc < 1 || argc > 3)
        RaiseExceptionC(Assertion, who, "expected one to three arguments", EmptyListObject);
}

inline void OneToFourArgsCheck(const char * who, int_t argc)
{
    if (argc < 1 || argc > 4)
        RaiseExceptionC(Assertion, who, "expected one to four arguments", EmptyListObject);
}

inline void TwoOrThreeArgsCheck(const char * who, int_t argc)
{
    if (argc < 2 || argc > 3)
        RaiseExceptionC(Assertion, who, "expected two or three arguments", EmptyListObject);
}

inline void TwoToFourArgsCheck(const char * who, int_t argc)
{
    if (argc < 2 || argc > 4)
        RaiseExceptionC(Assertion, who, "expected two to four arguments", EmptyListObject);
}

inline void ThreeToFiveArgsCheck(const char * who, int_t argc)
{
    if (argc < 3 || argc > 5)
        RaiseExceptionC(Assertion, who, "expected three to five arguments", EmptyListObject);
}

inline void NonNegativeArgCheck(const char * who, FObject arg, int_t bf)
{
    if (NonNegativeExactIntegerP(arg, bf) == 0)
        RaiseExceptionC(Assertion, who, "expected an exact non-negative integer", List(arg));
}

inline void IndexArgCheck(const char * who, FObject arg, FFixnum len)
{
    if (FixnumP(arg) == 0 || AsFixnum(arg) < 0 || AsFixnum(arg) > len)
        RaiseExceptionC(Assertion, who, "expected a valid index", List(arg));
}

inline void EndIndexArgCheck(const char * who, FObject arg, FFixnum strt, FFixnum len)
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

inline void HashTreeArgCheck(const char * who, FObject obj)
{
    if (HashTreeP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a hash tree", List(obj));
}

inline void HashContainerArgCheck(const char * who, FObject obj)
{
    if (HashContainerP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a hash-map, hash-set, or hash-bag", List(obj));
}

inline void EqHashMapArgCheck(const char * who, FObject obj)
{
    if (HashMapP(obj) == 0 || PairP(AsHashContainer(obj)->Tracker) == 0)
        RaiseExceptionC(Assertion, who, "expected an eq-hash-map", List(obj));
}

inline void EqHashSetArgCheck(const char * who, FObject obj)
{
    if (HashSetP(obj) == 0 || PairP(AsHashContainer(obj)->Tracker) == 0)
        RaiseExceptionC(Assertion, who, "expected an eq-hash-set", List(obj));
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

// ----------------

extern uint_t CheckHeapFlag;
extern uint_t VerboseFlag;
extern uint_t RandomSeed;
extern volatile uint_t BytesAllocated;

FObject CompileProgram(FObject nam, FObject port);
FObject Eval(FObject obj, FObject env);
FObject GetInteractionEnv();

FObject SyntaxToDatum(FObject obj);

FObject ExecuteProc(FObject op);

int_t SetupFoment(FThreadState * ts);
extern uint_t SetupComplete;
void ExitFoment();
void ErrorExitFoment();

// ---- Do Not Call Directly ----

int_t SetupCore(FThreadState * ts);
void SetupLibrary();
void SetupPairs();
void SetupCharacters();
void SetupStrings();
void SetupVectors();
void SetupHashContainers();
void SetupCompare();
void SetupComparePrims();
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
inline int_t PathChP(FCh ch)
{
    return(ch == '\\' || ch == '/');
}
#else // FOMENT_WINDOWS
#define PathCh '/'
#define PathSep ':'
inline int_t PathChP(FCh ch)
{
    return(ch == '/');
}
#endif // FOMENT_WINDOWS

#endif // __FOMENT_HPP__
