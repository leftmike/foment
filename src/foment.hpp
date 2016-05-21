/*

Foment

To Do:
-- update documenation: MemoryManagement APIs and Internals
-- check trackers
-- CheckObject: check back references from mature objects
-- Kids
-- generational + mark and sweep
-- after GC, test for objects pointing to Babies
-- Use extra generation for immortal objects which are precompiled libraries

Future:
-- number.cpp: make NumberP, BinaryNumberOp, and UnaryNumberOp faster
-- Windows: $(APPDATA)\Foment\Libraries
-- Unix: $(HOME)/.local/foment/lib
-- don't load all builtin libraries at startup
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

HashTree and Comparator:
-- replace srfi 114 with srfi 128
-- add sets: HashSet
-- add bags: HashBag

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

#ifdef __LP64__
#define FOMENT_64BIT
#define FOMENT_MEMORYMODEL "lp64"
#else // __LP64__
#define FOMENT_32BIT
#define FOMENT_MEMORYMODEL "ilp32"
#endif // __LP64__

#ifdef BSD
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
#endif // FOMENT_32BIT

#ifdef FOMENT_64BIT
typedef uint64_t FImmediate;
typedef int64_t FFixnum;
typedef int64_t int_t;
typedef uint64_t uint_t;
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

    FixnumTag = 0x01,
    CharacterTag = 0x02,
    MiscellaneousTag = 0x03,
    SpecialSyntaxTag = 0x04,
    InstructionTag = 0x05,
    ValuesCountTag = 0x06,
    UnusedTag = 0x07
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

#define OBJHDR_SIZE_MASK 0x1FFFFFFF
#define OBJHDR_GEN_MASK 0x60000000
#define OBJHDR_MARK_FORWARD 0x80000000
#define OBJHDR_GEN_BABIES 0x00000000
#define OBJHDR_GEN_KIDS 0x20000000
#define OBJHDR_GEN_ADULTS 0x40000000
#define OBJHDR_GEN_UNUSED 0x60000000

#define OBJHDR_SLOT_COUNT_MASK 0x03FFFFFF
#define OBJHDR_TAG_SHIFT 26
#define OBJHDR_TAG_MASK 0x0000001F
#define OBJHDR_CHECK_MARK 0x80000000

typedef struct
{
    union
    {
        uint32_t FlagsAndSize;
        uint32_t Flags;
    };
    union
    {
        uint32_t TagAndSlotCount;
        uint32_t CheckMark;
    };

    uint_t Size() {return(FlagsAndSize & OBJHDR_SIZE_MASK);}
    uint_t SlotCount() {return(TagAndSlotCount & OBJHDR_SLOT_COUNT_MASK);}
    uint_t Tag() {return((TagAndSlotCount >> OBJHDR_TAG_SHIFT) & OBJHDR_TAG_MASK);}
    FObject * Slots() {return((FObject *) (this + 1));}
    uint_t Generation() {return(Flags & OBJHDR_GEN_MASK);}
} FObjHdr;

inline FObjHdr * AsObjHdr(FObject obj)
{
    FAssert(ObjectP(obj));

    return(((FObjHdr *) obj) - 1);
}

#define ByteLength(obj) (AsObjHdr(obj)->Size())

FObject MakeObject(uint_t tag, uint_t sz, uint_t sc, const char * who, int_t pf = 0);

inline FIndirectTag IndirectTag(FObject obj)
{
    return((FIndirectTag) (ObjectP(obj) ? AsObjHdr(obj)->Tag() : 0));
}

void PushRoot(FObject * rt);
void PopRoot();
void ClearRoots();

extern volatile int_t GCRequired;

void EnterWait();
void LeaveWait();

void CheckHeap(const char * fn, int ln);
void ReadyForGC();
#define CheckForGC() if (GCRequired) ReadyForGC()

inline int_t MatureP(FObject obj)
{
    return(ObjectP(obj) && (AsObjHdr(obj)->Flags & OBJHDR_GEN_MASK) == OBJHDR_GEN_ADULTS);
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

#define FalseObject MakeImmediate(1, MiscellaneousTag)
#define TrueObject MakeImmediate(2, MiscellaneousTag)
#define BooleanP(obj) ((obj) == FalseObject || (obj) == TrueObject)

#define EndOfFileObject MakeImmediate(3, MiscellaneousTag)
#define EndOfFileObjectP(obj) ((obj) == EndOfFileObject)

#define NoValueObject MakeImmediate(4, MiscellaneousTag)
#define NoValueObjectP(obj) ((obj) == NoValueObject)

#define WantValuesObject MakeImmediate(5, MiscellaneousTag)
#define WantValuesObjectP(obj) ((obj) == WantValuesObject)

#define NotFoundObject MakeImmediate(6, MiscellaneousTag)
#define NotFoundObjectP(obj) ((obj) == NotFoundObject)

#define MatchAnyObject MakeImmediate(7, MiscellaneousTag)
#define MatchAnyObjectP(obj) ((obj) == MatchAnyObject)

#define ValuesCountP(obj) ImmediateP(obj, ValuesCountTag)
#define MakeValuesCount(cnt) MakeImmediate(cnt, ValuesCountTag)
#define AsValuesCount(obj) ((FFixnum) (AsValue(obj)))

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
uint_t ByteLengthHash(char * b, uint_t bl);
uint_t StringLengthHash(FCh * s, uint_t sl);
uint_t StringHash(FObject obj);
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

void Write(FObject port, FObject obj, int_t df);
void WriteShared(FObject port, FObject obj, int_t df);
void WriteSimple(FObject port, FObject obj, int_t df);

// ---- Record Types ----

#define RecordTypeP(obj) (IndirectTag(obj) == RecordTypeTag)
#define AsRecordType(obj) ((FRecordType *) (obj))

typedef struct
{
    FObject Fields[1];
} FRecordType;

FObject MakeRecordType(FObject nam, uint_t nf, FObject flds[]);
FObject MakeRecordTypeC(const char * nam, uint_t nf, const char * flds[]);

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

// ---- Primitives ----

#define PrimitiveP(obj) (IndirectTag(obj) == PrimitiveTag)
#define AsPrimitive(obj) ((FPrimitive *) (obj))

typedef FObject (*FPrimitiveFn)(int_t argc, FObject argv[]);
typedef struct
{
    FPrimitiveFn PrimitiveFn;
    const char * Name;
    const char * Filename;
    int_t LineNumber;
} FPrimitive;

#define Define(name, fn)\
    static FObject fn ## Fn(int_t argc, FObject argv[]);\
    FPrimitive fn = {fn ## Fn, name, __FILE__, __LINE__};\
    static FObject fn ## Fn

void DefinePrimitive(FObject env, FObject lib, FPrimitive * prim);
FObject MakePrimitive(FPrimitive * prim);

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

#define ComparatorP(obj) RecordP(obj, R.ComparatorRecordType)
#define AsComparator(obj) ((FComparator *) (obj))

typedef struct
{
    FRecord Record;
    FObject TypeTestFn;
    FObject EqualityFn;
    FObject ComparisonFn;
    FObject HashFn;
    FObject HasComparison;
    FObject HasHash;
} FComparator;

FObject MakeComparator(FObject ttfn, FObject eqfn, FObject compfn, FObject hashfn);
void DefineComparator(const char * nam, FPrimitive * ttprim, FPrimitive * eqprim,
    FPrimitive * compprim, FPrimitive * hashprim);

int_t EqP(FObject obj1, FObject obj2);
int_t EqvP(FObject obj1, FObject obj2);
int_t EqualP(FObject obj1, FObject obj2);

uint_t EqHash(FObject obj);
extern FPrimitive EqHashPrimitive;

// ---- HashContainer ----

#define HashContainerP(obj) (HashMapP(obj) || HashSetP(obj) || HashBagP(obj))
#define AsHashContainer(obj) ((FHashContainer *) (obj))

typedef struct
{
    FRecord Record;
    FObject HashTree;
    FObject Comparator;
    FObject Tracker;
    FObject Size;
} FHashContainer;

// ---- HashMap ----

#define HashMapP(obj) RecordP(obj, R.HashMapRecordType)

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

#define HashSetP(obj) RecordP(obj, R.HashSetRecordType)

typedef FHashContainer FHashSet;

FObject MakeEqHashSet();
int_t EqHashSetContainsP(FObject hset, FObject elem);
void EqHashSetAdjoin(FObject hset, FObject elem);
void EqHashSetDelete(FObject hset, FObject elem);

// ---- HashBag ----

#define HashBagP(obj) RecordP(obj, R.HashBagRecordType)

typedef FHashContainer FHashBag;

// ---- Symbols ----

#define SymbolP(obj) (IndirectTag(obj) == SymbolTag)
#define AsSymbol(obj) ((FSymbol *) (obj))

typedef struct
{
    FObject String;
    uint_t Hash;
} FSymbol;

FObject StringToSymbol(FObject str);
FObject StringCToSymbol(const char * s);
FObject StringLengthToSymbol(FCh * s, int_t sl);
FObject PrefixSymbol(FObject str, FObject sym);

// ---- Roots ----

typedef struct
{
    FObject Bedrock;
    FObject BedrockLibrary;
    FObject Features;
    FObject CommandLine;
    FObject FullCommandLine;
    FObject InteractiveOptions;
    FObject LibraryPath;
    FObject LibraryExtensions;
    FObject EnvironmentVariables;

    FObject SymbolHashTree;

    FObject ComparatorRecordType;
    FObject AnyPPrimitive;
    FObject NoHashPrimitive;
    FObject NoComparePrimitive;
    FObject EqComparator;
    FObject DefaultComparator;

    FObject HashMapRecordType;
    FObject HashSetRecordType;
    FObject HashBagRecordType;
    FObject ExceptionRecordType;

    FObject Assertion;
    FObject Restriction;
    FObject Lexical;
    FObject Syntax;
    FObject Error;

    FObject LoadedLibraries;

    FObject SyntacticEnvRecordType;
    FObject BindingRecordType;
    FObject IdentifierRecordType;
    FObject LambdaRecordType;
    FObject CaseLambdaRecordType;
    FObject InlineVariableRecordType;
    FObject ReferenceRecordType;

    FObject ElseReference;
    FObject ArrowReference;
    FObject LibraryReference;
    FObject AndReference;
    FObject OrReference;
    FObject NotReference;
    FObject QuasiquoteReference;
    FObject UnquoteReference;
    FObject UnquoteSplicingReference;
    FObject ConsReference;
    FObject AppendReference;
    FObject ListToVectorReference;
    FObject EllipsisReference;
    FObject UnderscoreReference;

    FObject TagSymbol;
    FObject UsePassSymbol;
    FObject ConstantPassSymbol;
    FObject AnalysisPassSymbol;
    FObject InteractionEnv;

    FObject StandardInput;
    FObject StandardOutput;
    FObject StandardError;
    FObject QuoteSymbol;
    FObject QuasiquoteSymbol;
    FObject UnquoteSymbol;
    FObject UnquoteSplicingSymbol;
    FObject FileErrorSymbol;
    FObject CurrentSymbol;
    FObject EndSymbol;

    FObject SyntaxRulesRecordType;
    FObject PatternVariableRecordType;
    FObject PatternRepeatRecordType;
    FObject TemplateRepeatRecordType;
    FObject SyntaxRuleRecordType;

    FObject EnvironmentRecordType;
    FObject GlobalRecordType;
    FObject LibraryRecordType;
    FObject NoValuePrimitive;
    FObject LibraryStartupList;

    FObject WrongNumberOfArguments;
    FObject NotCallable;
    FObject UnexpectedNumberOfValues;
    FObject UndefinedMessage;
    FObject ExecuteThunk;
    FObject RaiseHandler;
    FObject NotifyHandler;
    FObject InteractiveThunk;
    FObject ExceptionHandlerSymbol;
    FObject NotifyHandlerSymbol;
    FObject SigIntSymbol;

    FObject DynamicRecordType;
    FObject ContinuationRecordType;

    FObject CleanupTConc;

    FObject DefineLibrarySymbol;
    FObject ImportSymbol;
    FObject IncludeLibraryDeclarationsSymbol;
    FObject CondExpandSymbol;
    FObject ExportSymbol;
    FObject BeginSymbol;
    FObject IncludeSymbol;
    FObject IncludeCISymbol;
    FObject OnlySymbol;
    FObject ExceptSymbol;
    FObject PrefixSymbol;
    FObject RenameSymbol;
    FObject AkaSymbol;

    FObject DatumReferenceRecordType;
} FRoots;

extern FRoots R;

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

#define EnvironmentP(obj) RecordP(obj, R.EnvironmentRecordType)
#define AsEnvironment(obj) ((FEnvironment *) (obj))

typedef struct
{
    FRecord Record;
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

#define GlobalP(obj) RecordP(obj, R.GlobalRecordType)
#define AsGlobal(obj) ((FGlobal *) (obj))

typedef struct
{
    FRecord Record;
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

#define LibraryP(obj) RecordP(obj, R.LibraryRecordType)
#define AsLibrary(obj) ((FLibrary *) (obj))

typedef struct
{
    FRecord Record;
    FObject Name;
    FObject Exports;
} FLibrary;

FObject MakeLibrary(FObject nam);
void LibraryExport(FObject lib, FObject gl);

// ---- Syntax Rules ----

#define SyntaxRulesP(obj) RecordP(obj, R.SyntaxRulesRecordType)

// ---- Identifiers ----

typedef struct
{
    FRecord Record;
    FObject Symbol;
    FObject Filename;
    FObject LineNumber;
    FObject Magic;
    FObject SyntacticEnv;
    FObject Wrapped;
} FIdentifier;

#define AsIdentifier(obj) ((FIdentifier *) (obj))
#define IdentifierP(obj) RecordP(obj, R.IdentifierRecordType)

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

typedef struct
{
    FRecord Record;
    FObject Type; // error, assertion-violation, implementation-restriction, lexical-violation,
                  // syntax-violation, undefined-violation
    FObject Who;
    FObject Kind; // file-error or read-error
    FObject Message; // should be a string
    FObject Irritants; // a list of zero or more irritants
} FException;

#define AsException(obj) ((FException *) (obj))
#define ExceptionP(obj) RecordP(obj, R.ExceptionRecordType)

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

#define INDEX_PARAMETERS 3

typedef struct _FThreadState
{
    struct _FThreadState * Next;
    struct _FThreadState * Previous;

    FObject Thread;

    FAlive * AliveList;

    uint_t ObjectsSinceLast;
    uint_t BytesSinceLast;

    uint_t RootsUsed;
    FObject * Roots[12];

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
        RaiseExceptionC(R.Assertion, who, "expected no arguments", EmptyListObject);
}

inline void OneArgCheck(const char * who, int_t argc)
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, who, "expected one argument", EmptyListObject);
}

inline void TwoArgsCheck(const char * who, int_t argc)
{
    if (argc != 2)
        RaiseExceptionC(R.Assertion, who, "expected two arguments", EmptyListObject);
}

inline void ThreeArgsCheck(const char * who, int_t argc)
{
    if (argc != 3)
        RaiseExceptionC(R.Assertion, who, "expected three arguments", EmptyListObject);
}

inline void FourArgsCheck(const char * who, int_t argc)
{
    if (argc != 4)
        RaiseExceptionC(R.Assertion, who, "expected four arguments", EmptyListObject);
}

inline void FiveArgsCheck(const char * who, int_t argc)
{
    if (argc != 5)
        RaiseExceptionC(R.Assertion, who, "expected five arguments", EmptyListObject);
}

inline void SixArgsCheck(const char * who, int_t argc)
{
    if (argc != 6)
        RaiseExceptionC(R.Assertion, who, "expected six arguments", EmptyListObject);
}

inline void SevenArgsCheck(const char * who, int_t argc)
{
    if (argc != 7)
        RaiseExceptionC(R.Assertion, who, "expected seven arguments", EmptyListObject);
}

inline void AtLeastOneArgCheck(const char * who, int_t argc)
{
    if (argc < 1)
        RaiseExceptionC(R.Assertion, who, "expected at least one argument", EmptyListObject);
}

inline void AtLeastTwoArgsCheck(const char * who, int_t argc)
{
    if (argc < 2)
        RaiseExceptionC(R.Assertion, who, "expected at least two arguments", EmptyListObject);
}

inline void AtLeastThreeArgsCheck(const char * who, int_t argc)
{
    if (argc < 3)
        RaiseExceptionC(R.Assertion, who, "expected at least three arguments", EmptyListObject);
}

inline void AtLeastFourArgsCheck(const char * who, int_t argc)
{
    if (argc < 4)
        RaiseExceptionC(R.Assertion, who, "expected at least four arguments", EmptyListObject);
}

inline void ZeroOrOneArgsCheck(const char * who, int_t argc)
{
    if (argc > 1)
        RaiseExceptionC(R.Assertion, who, "expected zero or one arguments", EmptyListObject);
}

inline void ZeroToTwoArgsCheck(const char * who, int_t argc)
{
    if (argc > 2)
        RaiseExceptionC(R.Assertion, who, "expected zero to two arguments", EmptyListObject);
}

inline void OneOrTwoArgsCheck(const char * who, int_t argc)
{
    if (argc < 1 || argc > 2)
        RaiseExceptionC(R.Assertion, who, "expected one or two arguments", EmptyListObject);
}

inline void OneToThreeArgsCheck(const char * who, int_t argc)
{
    if (argc < 1 || argc > 3)
        RaiseExceptionC(R.Assertion, who, "expected one to three arguments", EmptyListObject);
}

inline void OneToFourArgsCheck(const char * who, int_t argc)
{
    if (argc < 1 || argc > 4)
        RaiseExceptionC(R.Assertion, who, "expected one to four arguments", EmptyListObject);
}

inline void TwoOrThreeArgsCheck(const char * who, int_t argc)
{
    if (argc < 2 || argc > 3)
        RaiseExceptionC(R.Assertion, who, "expected two or three arguments", EmptyListObject);
}

inline void TwoToFourArgsCheck(const char * who, int_t argc)
{
    if (argc < 2 || argc > 4)
        RaiseExceptionC(R.Assertion, who, "expected two to four arguments", EmptyListObject);
}

inline void ThreeToFiveArgsCheck(const char * who, int_t argc)
{
    if (argc < 3 || argc > 5)
        RaiseExceptionC(R.Assertion, who, "expected three to five arguments", EmptyListObject);
}

inline void NonNegativeArgCheck(const char * who, FObject arg, int_t bf)
{
    if (NonNegativeExactIntegerP(arg, bf) == 0)
        RaiseExceptionC(R.Assertion, who, "expected an exact non-negative integer", List(arg));
}

inline void IndexArgCheck(const char * who, FObject arg, FFixnum len)
{
    if (FixnumP(arg) == 0 || AsFixnum(arg) < 0 || AsFixnum(arg) > len)
        RaiseExceptionC(R.Assertion, who, "expected a valid index", List(arg));
}

inline void EndIndexArgCheck(const char * who, FObject arg, FFixnum strt, FFixnum len)
{
    if (FixnumP(arg) == 0 || AsFixnum(arg) < strt || AsFixnum(arg) > len)
        RaiseExceptionC(R.Assertion, who, "expected a valid index", List(arg));
}

inline void ByteArgCheck(const char * who, FObject obj)
{
    if (FixnumP(obj) == 0 || AsFixnum(obj) < 0 || AsFixnum(obj) > 0xFF)
        RaiseExceptionC(R.Assertion, who, "expected a byte: an exact integer between 0 and 255",
                List(obj));
}

inline void FixnumArgCheck(const char * who, FObject obj)
{
    if (FixnumP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected an exact integer", List(obj));
}

inline void IntegerArgCheck(const char * who, FObject obj)
{
    if (IntegerP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected an integer", List(obj));
}

inline void RealArgCheck(const char * who, FObject obj)
{
    if (RealP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a real number", List(obj));
}

inline void RationalArgCheck(const char * who, FObject obj)
{
    if (RationalP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a rational number", List(obj));
}

inline void NumberArgCheck(const char * who, FObject obj)
{
    if (NumberP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a number", List(obj));
}

inline void CharacterArgCheck(const char * who, FObject obj)
{
    if (CharacterP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a character", List(obj));
}

inline void BooleanArgCheck(const char * who, FObject obj)
{
    if (BooleanP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a boolean", List(obj));
}

inline void SymbolArgCheck(const char * who, FObject obj)
{
    if (SymbolP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a symbol", List(obj));
}

inline void ExceptionArgCheck(const char * who, FObject obj)
{
    if (ExceptionP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected an error-object", List(obj));
}

inline void StringArgCheck(const char * who, FObject obj)
{
    if (StringP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a string", List(obj));
}

inline void PairArgCheck(const char * who, FObject obj)
{
    if (PairP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a pair", List(obj));
}

inline void ListArgCheck(const char * who, FObject obj)
{
    if (ListLength(obj) < 0)
        RaiseExceptionC(R.Assertion, who, "expected a list", List(obj));
}

inline void BoxArgCheck(const char * who, FObject obj)
{
    if (BoxP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a box", List(obj));
}

inline void VectorArgCheck(const char * who, FObject obj)
{
    if (VectorP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a vector", List(obj));
}

inline void BytevectorArgCheck(const char * who, FObject obj)
{
    if (BytevectorP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a bytevector", List(obj));
}

inline void EnvironmentArgCheck(const char * who, FObject obj)
{
    if (EnvironmentP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected an environment", List(obj));
}

inline void PortArgCheck(const char * who, FObject obj)
{
    if (BinaryPortP(obj) == 0 && TextualPortP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a port", List(obj));
}

inline void InputPortArgCheck(const char * who, FObject obj)
{
    if ((BinaryPortP(obj) == 0 && TextualPortP(obj) == 0) || InputPortP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected an input port", List(obj));
}

inline void OutputPortArgCheck(const char * who, FObject obj)
{
    if ((BinaryPortP(obj) == 0 && TextualPortP(obj) == 0) || OutputPortP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected an output port", List(obj));
}

inline void BinaryPortArgCheck(const char * who, FObject obj)
{
    if (BinaryPortP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a binary port", List(obj));
}

inline void TextualPortArgCheck(const char * who, FObject obj)
{
    if (TextualPortP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a textual port", List(obj));
}

inline void StringOutputPortArgCheck(const char * who, FObject obj)
{
    if (StringOutputPortP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a string output port", List(obj));
}

inline void BytevectorOutputPortArgCheck(const char * who, FObject obj)
{
    if (BytevectorOutputPortP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a bytevector output port", List(obj));
}

inline void TextualInputPortArgCheck(const char * who, FObject obj)
{
    if (TextualPortP(obj) == 0 || InputPortOpenP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected an open textual input port", List(obj));
}

inline void TextualOutputPortArgCheck(const char * who, FObject obj)
{
    if (TextualPortP(obj) == 0 || OutputPortOpenP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected an open textual output port", List(obj));
}

inline void BinaryInputPortArgCheck(const char * who, FObject obj)
{
    if (BinaryPortP(obj) == 0 || InputPortOpenP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected an open binary input port", List(obj));
}

inline void BinaryOutputPortArgCheck(const char * who, FObject obj)
{
    if (BinaryPortP(obj) == 0 || OutputPortOpenP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected an open binary output port", List(obj));
}

inline void ConsoleInputPortArgCheck(const char * who, FObject obj)
{
    if (ConsolePortP(obj) == 0 || InputPortOpenP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected an open console input port", List(obj));
}

inline void SocketPortArgCheck(const char * who, FObject obj)
{
    if (SocketPortP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a socket port", List(obj));
}

inline void OpenPositioningPortArgCheck(const char * who, FObject obj)
{
    if (PortOpenP(obj) == 0 || PositioningPortP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected an open port which supports positioning",
                List(obj));
}

inline void ProcedureArgCheck(const char * who, FObject obj)
{
    if (ProcedureP(obj) == 0 && PrimitiveP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a procedure", List(obj));
}

inline void ThreadArgCheck(const char * who, FObject obj)
{
    if (ThreadP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a thread", List(obj));
}

inline void ExclusiveArgCheck(const char * who, FObject obj)
{
    if (ExclusiveP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected an exclusive", List(obj));
}

inline void ConditionArgCheck(const char * who, FObject obj)
{
    if (ConditionP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a condition", List(obj));
}

inline void TConcArgCheck(const char * who, FObject obj)
{
    if (PairP(obj) == 0 || PairP(First(obj)) == 0 || PairP(Rest(obj)) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a tconc", List(obj));
}

inline void HashTreeArgCheck(const char * who, FObject obj)
{
    if (HashTreeP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a hash tree", List(obj));
}

inline void HashContainerArgCheck(const char * who, FObject obj)
{
    if (HashContainerP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a hash-map, hash-set, or hash-bag", List(obj));
}

inline void EqHashMapArgCheck(const char * who, FObject obj)
{
    if (HashMapP(obj) == 0 || PairP(AsHashContainer(obj)->Tracker) == 0)
        RaiseExceptionC(R.Assertion, who, "expected an eq-hash-map", List(obj));
}

inline void EqHashSetArgCheck(const char * who, FObject obj)
{
    if (HashSetP(obj) == 0 || PairP(AsHashContainer(obj)->Tracker) == 0)
        RaiseExceptionC(R.Assertion, who, "expected an eq-hash-set", List(obj));
}

inline void ComparatorArgCheck(const char * who, FObject obj)
{
    if (ComparatorP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a comparator", List(obj));
}

// ----------------

extern uint_t CheckHeapFlag;
extern uint_t VerboseFlag;

extern void * SectionTableBase;
extern uint_t RandomSeed;

extern volatile uint_t BytesAllocated;
extern uint_t CollectionCount;

FObject CompileProgram(FObject nam, FObject port);
FObject Eval(FObject obj, FObject env);
FObject GetInteractionEnv();

FObject SyntaxToDatum(FObject obj);

FObject ExecuteThunk(FObject op);

int_t SetupFoment(FThreadState * ts);
extern uint_t SetupComplete;
void ExitFoment();

// ---- Do Not Call Directly ----

int_t SetupCore(FThreadState * ts);
void SetupLibrary();
void SetupPairs();
void SetupCharacters();
void SetupStrings();
void SetupVectors();
void SetupHashContainers();
void SetupHashContainerPrims();
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

void WriteSpecialSyntax(FObject port, FObject obj, int_t df);
void WriteInstruction(FObject port, FObject obj, int_t df);
void WriteThread(FObject port, FObject obj, int_t df);
void WriteExclusive(FObject port, FObject obj, int_t df);
void WriteCondition(FObject port, FObject obj, int_t df);

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
