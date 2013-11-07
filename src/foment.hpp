/*

Foment

Goals:
-- R7RS including syntax-rules
-- simple implementation

To Do:
-- ctrl-c handling

-- use tests from chibi
-- make tests work with (chibi test)

-- fix read to work with circular data structures

-- IO and GC
-- boxes, vectors, procedures, records, and pairs need to be read and written using scheme code
-- or use FAlive
-- use EnterWait and LeaveWait
-- use Win32 file APIs and not stdio

Future:
-- some mature segments are compacted during a full collection; ie. mark-compact
-- inline primitives in GPassExpression
-- debugging information
-- number tags should require only a single test by sharing part of a tag
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

Bugs:
-- serialize loading libraries
-- serialize symbol table
-- char-ready? always returns #t
-- current-second returns an exact integer
-- write-bytevector, write-string, read-bytevector, and read-bytevector! assume GC does
    not happen during IO
-- call/cc: unwind only as far as the common tail
-- gc.cpp: AllocateSection failing is not handled by all callers
-- ExecuteThunk does not check for CStack or AStack overflow
-- update FeaturesC as more code gets written
-- remove Hash from FSymbol (part of Reserved)
-- r5rs_pitfall.scm: yin-yang does not terminate
-- exhaustively test unicode: char-alphabetic?, char-upcase, etc

Testing:
-- 6.1 equivalence predicates complete
-- 6.2
-- 6.3 booleans complete
-- 6.4 pairs and lists complete
-- 6.5 symbols complete
-- 6.6 characters complete
-- 6.7 strings complete
-- 6.8 vectors complete
-- 6.9 bytevectors complete
-- 6.10 control features complete
-- 6.11 exceptions complete
-- 6.12 environments and evaluation complete
-- 6.13.1 ports complete
-- 6.13.2 input complete
-- 6.13.3 output complete
-- 6.14 system interface complete

Missing:
-- environment
-- scheme-report-environment
-- null-environment
-- load
-- exit

*/

#ifndef __FOMENT_HPP__
#define __FOMENT_HPP__

#define FOMENT_VERSION "0.2"

#include <stdint.h>

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
typedef int32_t FFixnum;
typedef uint32_t FImmediate;
typedef int32_t int_t;
typedef uint32_t uint_t;
#endif // FOMENT_32BIT

#ifdef FOMENT_64BIT
typedef int64_t FFixnum;
typedef uint64_t FImmediate;
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

    PairTag = 0x01,          // 0bxxxxxx01
    FlonumTag = 0x02,        // 0bxxxxxx10
    FixnumTag = 0x07,        // 0bxxx00111
    CharacterTag = 0x0B,     // 0bxxx01011
    MiscellaneousTag = 0x0F, // 0bxxx01111
    SpecialSyntaxTag = 0x13, // 0bxxx10011
    InstructionTag = 0x17,   // 0bxxx10111
    ValuesCountTag = 0x1B,   // 0bxxx11011
    UnusedTag = 0x1F,        // 0bxxx11111

    // Used by garbage collector.

    GCTagTag = 0x0B
} FDirectTag;

typedef enum
{
    // Indirect Types

    BoxTag = 3,
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

    // Invalid Tag

    BadDogTag,

    // Used by garbage collector.

    GCFreeTag = 0x1F
} FIndirectTag;

#define IndirectP(obj) ((((FImmediate) (obj)) & 0x3) == 0x0)
#define ObjectP(obj) ((((FImmediate) (obj)) & 0x3) != 0x3)

#define ImmediateTag(obj) (((FImmediate) (obj)) & 0x1F)
#define ImmediateP(obj, it) (ImmediateTag((obj)) == it)
#define MakeImmediate(val, it)\
    ((FObject *) ((((FImmediate) (val)) << 5) | (it & 0x1F)))
#define AsValue(obj) (((FImmediate) (obj)) >> 5)

// ---- Memory Management ----

FObject MakeObject(uint_t sz, uint_t tag);
FObject MakeMatureObject(uint_t len, const char * who);
FObject MakePinnedObject(uint_t len, const char * who);

#define RESERVED_BITS 6
#define RESERVED_TAGMASK 0x1F
#define RESERVED_MARK_BIT 0x20

#ifdef FOMENT_32BIT
#define MAXIMUM_OBJECT_LENGTH 0x3FFFFFF
#endif // FOMENT_32BIT

#ifdef FOMENT_64BIT
#define MAXIMUM_OBJECT_LENGTH 0x3FFFFFFFFFFFFFF
#endif // FOMENT_64BIT

#define ByteLength(obj) ((*((uint_t *) (obj))) >> RESERVED_BITS)
#define ObjectLength(obj) ((*((uint_t *) (obj))) >> RESERVED_BITS)
#define MakeLength(len, tag) (((len) << RESERVED_BITS) | (tag))
#define MakeMatureLength(len, tag) (((len) << RESERVED_BITS) | (tag) | RESERVED_MARK_BIT)
#define MakePinnedLength(len, tag) MakeMatureLength((len), (tag))

inline FIndirectTag IndirectTag(FObject obj)
{
    return((FIndirectTag) (IndirectP(obj) ? *((uint_t *) (obj)) & RESERVED_TAGMASK : 0));
}

void PushRoot(FObject * rt);
void PopRoot();
void ClearRoots();

extern int_t GCRequired;

void EnterWait();
void LeaveWait();

void Collect();
#define CheckForGC() if (GCRequired) Collect()

void ModifyVector(FObject obj, uint_t idx, FObject val);

/*
//    AsBox(argv[0])->Value = argv[1];
    Modify(FBox, argv[0], Value, argv[1]);
*/
#define Modify(type, obj, slot, val)\
    ModifyObject(obj, (uint_t) &(((type *) 0)->slot), val)

// Do not directly call ModifyObject; use Modify instead.
void ModifyObject(FObject obj, uint_t off, FObject val);

void InstallGuardian(FObject obj, FObject tconc);
void InstallTracker(FObject obj, FObject ret, FObject tconc);

//
// ---- Immediate Types ----
//

#define FixnumP(obj) ImmediateP((obj), FixnumTag)
#define MakeFixnum(n)\
    ((FObject *) ((((FFixnum) (n)) << 5) | (FixnumTag & 0x1F)))
#define AsFixnum(obj) (((FFixnum) (obj)) >> 5)

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
FObject SpecialSyntaxToSymbol(FObject obj);
FObject SpecialSyntaxMsgC(FObject obj, const char * msg);

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
#define LetSyntaxSyntax MakeImmediate(10, SpecialSyntaxTag)
#define LetrecSyntaxSyntax MakeImmediate(11, SpecialSyntaxTag)
#define CaseSyntax MakeImmediate(12, SpecialSyntaxTag)
#define OrSyntax MakeImmediate(13, SpecialSyntaxTag)
#define BeginSyntax MakeImmediate(14, SpecialSyntaxTag)
#define DoSyntax MakeImmediate(15, SpecialSyntaxTag)
#define SyntaxRulesSyntax MakeImmediate(16, SpecialSyntaxTag)
#define SyntaxErrorSyntax MakeImmediate(17, SpecialSyntaxTag)
#define IncludeSyntax MakeImmediate(18, SpecialSyntaxTag)
#define IncludeCISyntax MakeImmediate(19, SpecialSyntaxTag)
#define CondExpandSyntax MakeImmediate(20, SpecialSyntaxTag)
#define CaseLambdaSyntax MakeImmediate(21, SpecialSyntaxTag)
#define QuasiquoteSyntax MakeImmediate(22, SpecialSyntaxTag)

#define DefineSyntax MakeImmediate(23, SpecialSyntaxTag)
#define DefineValuesSyntax MakeImmediate(24, SpecialSyntaxTag)
#define DefineSyntaxSyntax MakeImmediate(25, SpecialSyntaxTag)

#define ElseSyntax MakeImmediate(26, SpecialSyntaxTag)
#define ArrowSyntax MakeImmediate(27, SpecialSyntaxTag)
#define UnquoteSyntax MakeImmediate(28, SpecialSyntaxTag)
#define UnquoteSplicingSyntax MakeImmediate(29, SpecialSyntaxTag)
#define EllipsisSyntax MakeImmediate(30, SpecialSyntaxTag)
#define UnderscoreSyntax MakeImmediate(31, SpecialSyntaxTag)

// ---- Instruction ----

#define InstructionP(obj) ImmediateP((obj), InstructionTag)

//
// ---- Object Types ----
//

// ---- Pairs ----

#define PairP(obj) ((((FImmediate) (obj)) & 0x3) == PairTag)
#define AsPair(obj) ((FPair *) (((char *) (obj)) - PairTag))
#define PairObject(pair) ((FObject) (((char *) (pair)) + PairTag))

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

FObject Memq(FObject obj, FObject lst);
FObject Assq(FObject obj, FObject alst);
FObject Assoc(FObject obj, FObject alst);

FObject MakeTConc();
int_t TConcEmptyP(FObject tconc);
void TConcAdd(FObject tconc, FObject obj);
FObject TConcRemove(FObject tconc);

// ---- Flonums ----

#define FlonumP(obj) ((((FImmediate) (obj)) & 0x3) == FlonumTag)
#define AsFlonum(obj) ((FFlonum *) (((char *) (obj)) - FlonumTag))
#define FlonumObject(fl) ((FObject) (((char *) (fl)) + FlonumTag))

typedef struct
{
    double Flonum;
} FFlonum;

// ---- Boxes ----

#define BoxP(obj) (IndirectTag(obj) == BoxTag)
#define AsBox(obj) ((FBox *) (obj))

typedef struct
{
    uint_t Reserved;
    FObject Value;
} FBox;

FObject MakeBox(FObject val);
inline FObject Unbox(FObject bx)
{
    FAssert(BoxP(bx));

    return(AsBox(bx)->Value);
}

// ---- Strings ----

#define StringP(obj) (IndirectTag(obj) == StringTag)
#define AsString(obj) ((FString *) (obj))

typedef struct
{
    uint_t Length;
    FCh String[1];
} FString;

FObject MakeString(FCh * s, uint_t sl);
FObject MakeStringCh(uint_t sl, FCh ch);
FObject MakeStringC(const char * s);
FObject MakeStringS(FChS * ss);
FObject MakeStringS(FChS * ss, uint_t ssl);

inline uint_t StringLength(FObject obj)
{
    uint_t bl = ByteLength(obj);
    FAssert(bl % sizeof(FCh) == 0);

    return(bl / sizeof(FCh));
}

void StringToC(FObject s, char * b, int_t bl);
int_t StringToNumber(FCh * s, int_t sl, FFixnum * np, FFixnum b);
int_t NumberAsString(FFixnum n, FCh * s, FFixnum b);
FObject FoldcaseString(FObject s);
uint_t ByteLengthHash(char * b, uint_t bl);
uint_t StringLengthHash(FCh * s, uint_t sl);
uint_t StringHash(FObject obj);
int_t StringEqualP(FObject obj1, FObject obj2);
int_t StringLengthEqualP(FCh * s, int_t sl, FObject obj);
int_t StringCEqualP(const char * s1, FCh * s2, int_t sl2);
uint_t BytevectorHash(FObject obj);

// ---- Vectors ----

#define VectorP(obj) (IndirectTag(obj) == VectorTag)
#define AsVector(obj) ((FVector *) (obj))

typedef struct
{
    uint_t Length;
    FObject Vector[1];
} FVector;

FObject MakeVector(uint_t vl, FObject * v, FObject obj);
FObject ListToVector(FObject obj);
FObject VectorToList(FObject vec);

#define VectorLength(obj) ObjectLength(obj)

// ---- Bytevectors ----

#define BytevectorP(obj) (IndirectTag(obj) == BytevectorTag)
#define AsBytevector(obj) ((FBytevector *) (obj))

typedef unsigned char FByte;
typedef struct
{
    uint_t Length;
    FByte Vector[1];
} FBytevector;

FObject MakeBytevector(uint_t vl);
FObject U8ListToBytevector(FObject obj);

#define BytevectorLength(obj) ByteLength(obj)

// ---- Ports ----

#define PORT_FLAG_INPUT             0x80000000
#define PORT_FLAG_INPUT_OPEN        0x40000000
#define PORT_FLAG_OUTPUT            0x20000000
#define PORT_FLAG_OUTPUT_OPEN       0x10000000
#define PORT_FLAG_STRING_OUTPUT     0x08000000
#define PORT_FLAG_BYTEVECTOR_OUTPUT 0x04000000
#define PORT_FLAG_FOLDCASE          0x02000000
#define PORT_FLAG_WANT_IDENTIFIERS  0x01000000

typedef void (*FCloseInputFn)(FObject port);
typedef void (*FCloseOutputFn)(FObject port);
typedef void (*FFlushOutputFn)(FObject port);

typedef struct
{
    uint_t Flags;
    FObject Name;
    FObject Object;
    void * InputContext;
    void * OutputContext;
    FCloseInputFn CloseInputFn;
    FCloseOutputFn CloseOutputFn;
    FFlushOutputFn FlushOutputFn;
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

// Binary and textual ports

void CloseInput(FObject port);
void CloseOutput(FObject port);
void FlushOutput(FObject port);

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
    uint_t NumFields;
    FObject Fields[1];
} FRecordType;

FObject MakeRecordType(FObject nam, uint_t nf, FObject flds[]);
FObject MakeRecordTypeC(const char * nam, uint_t nf, const char * flds[]);

#define RecordTypeName(obj) AsRecordType(obj)->Fields[0]
#define RecordTypeNumFields(obj) ObjectLength(obj)

// ---- Records ----

#define GenericRecordP(obj) (IndirectTag(obj) == RecordTag)
#define AsGenericRecord(obj) ((FGenericRecord *) (obj))

typedef struct
{
    uint_t NumFields; // RecordType is include in the number of fields.
    FObject RecordType;
} FRecord;

typedef struct
{
    uint_t NumFields;
    FObject Fields[1];
} FGenericRecord;

FObject MakeRecord(FObject rt);

inline int_t RecordP(FObject obj, FObject rt)
{
    return(GenericRecordP(obj) && AsGenericRecord(obj)->Fields[0] == rt);
}

#define RecordNumFields(obj) ObjectLength(obj)

// ---- Hashtables ----

#define HashtableP(obj) RecordP(obj, R.HashtableRecordType)
#define AsHashtable(obj) ((FHashtable *) (obj))

typedef int_t (*FEquivFn)(FObject obj1, FObject obj2);
typedef uint_t (*FHashFn)(FObject obj);
typedef FObject (*FWalkUpdateFn)(FObject key, FObject val, FObject ctx);
typedef int_t (*FWalkDeleteFn)(FObject key, FObject val, FObject ctx);
typedef void (*FWalkVisitFn)(FObject key, FObject val, FObject ctx);

typedef struct
{
    FRecord Record;
    FObject Buckets; // Must be a vector.
    FObject Size; // Number of keys.
    FObject Tracker; // TConc used by EqHashtables to track movement of keys.
} FHashtable;

FObject MakeHashtable(int_t nb);
FObject HashtableRef(FObject ht, FObject key, FObject def, FEquivFn efn, FHashFn hfn);
FObject HashtableStringRef(FObject ht, FCh * s, int_t sl, FObject def);
void HashtableSet(FObject ht, FObject key, FObject val, FEquivFn efn, FHashFn hfn);
void HashtableDelete(FObject ht, FObject key, FEquivFn efn, FHashFn hfn);
int_t HashtableContainsP(FObject ht, FObject key, FEquivFn efn, FHashFn hfn);

FObject MakeEqHashtable(int_t nb);
FObject EqHashtableRef(FObject ht, FObject key, FObject def);
void EqHashtableSet(FObject ht, FObject key, FObject val);
void EqHashtableDelete(FObject ht, FObject key);
int_t EqHashtableContainsP(FObject ht, FObject key);

uint_t HashtableSize(FObject ht);
void HashtableWalkUpdate(FObject ht, FWalkUpdateFn wfn, FObject ctx);
void HashtableWalkDelete(FObject ht, FWalkDeleteFn wfn, FObject ctx);
void HashtableWalkVisit(FObject ht, FWalkVisitFn wfn, FObject ctx);

// ---- Symbols ----

#define SymbolP(obj) (IndirectTag(obj) == SymbolTag)
#define AsSymbol(obj) ((FSymbol *) (obj))

typedef struct
{
    uint_t Reserved;
    FObject String;
} FSymbol;

FObject StringToSymbol(FObject str);
FObject StringCToSymbol(const char * s);
FObject StringLengthToSymbol(FCh * s, int_t sl);
FObject PrefixSymbol(FObject str, FObject sym);

#define SymbolHash(obj) ByteLength(obj)

// ---- Primitives ----

#define PrimitiveP(obj) (IndirectTag(obj) == PrimitiveTag)
#define AsPrimitive(obj) ((FPrimitive *) (obj))

typedef FObject (*FPrimitiveFn)(int_t argc, FObject argv[]);
typedef struct
{
    uint_t Reserved;
    FPrimitiveFn PrimitiveFn;
    const char * Name;
    const char * Filename;
    int_t LineNumber;
} FPrimitive;

#define Define(name, fn)\
    static FObject fn ## Fn(int_t argc, FObject argv[]);\
    static FPrimitive fn = {PrimitiveTag, fn ## Fn, name, __FILE__, __LINE__};\
    static FObject fn ## Fn

FObject MakePrimitive(FPrimitive * prim);
void DefinePrimitive(FObject env, FObject lib, FPrimitive * prim);

// ---- Environments ----

#define EnvironmentP(obj) RecordP(obj, R.EnvironmentRecordType)
#define AsEnvironment(obj) ((FEnvironment *) (obj))

typedef struct
{
    FRecord Record;
    FObject Name;
    FObject Hashtable;
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
    FObject LineNumber;
    FObject Magic;
    FObject SyntacticEnv;
    FObject Wrapped;
} FIdentifier;

#define AsIdentifier(obj) ((FIdentifier *) (obj))
#define IdentifierP(obj) RecordP(obj, R.IdentifierRecordType)

FObject MakeIdentifier(FObject sym, int_t ln);
FObject WrapIdentifier(FObject id, FObject se);

// ---- Procedures ----

typedef struct
{
    uint_t Reserved;
    FObject Name;
    FObject Code;
} FProcedure;

#define AsProcedure(obj) ((FProcedure *) (obj))
#define ProcedureP(obj) (IndirectTag(obj) == ProcedureTag)

FObject MakeProcedure(FObject nam, FObject cv, int_t ac, uint_t fl);

#define MAXIMUM_ARG_COUNT 0xFFFF
#define ProcedureArgCount(obj)\
    ((int_t) ((AsProcedure(obj)->Reserved >> RESERVED_BITS) & MAXIMUM_ARG_COUNT))

#define PROCEDURE_FLAG_CLOSURE      0x80000000
#define PROCEDURE_FLAG_PARAMETER    0x40000000
#define PROCEDURE_FLAG_CONTINUATION 0x20000000
#define PROCEDURE_FLAG_RESTARG      0x10000000

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
    FObject Message; // should be a string
    FObject Irritants; // a list of zero or more irritants
} FException;

#define AsException(obj) ((FException *) (obj))
#define ExceptionP(obj) RecordP(obj, R.ExceptionRecordType)

FObject MakeException(FObject typ, FObject who, FObject msg, FObject lst);
void RaiseException(FObject typ, FObject who, FObject msg, FObject lst);
void RaiseExceptionC(FObject typ, const char * who, const char * msg, FObject lst);
void Raise(FObject obj);

// ---- Thread State ----

typedef struct _FYoungSection
{
    struct _FYoungSection * Next;
    uint_t Used;
    uint_t Scan;
} FYoungSection;

#define INDEX_PARAMETERS 3

typedef struct _FThreadState
{
    struct _FThreadState * Next;
    struct _FThreadState * Previous;

    FObject Thread;

    FYoungSection * ActiveZero;
    uint_t ObjectsSinceLast;

    uint_t UsedRoots;
    FObject * Roots[12];

    int_t StackSize;
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
} FThreadState;

// ---- Roots ----

typedef struct
{
    FObject Bedrock;
    FObject BedrockLibrary;
    FObject Features;
    FObject CommandLine;
    FObject LibraryPath;
    FObject EnvironmentVariables;

    FObject SymbolHashtable;

    FObject HashtableRecordType;
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
    FObject InteractionEnv;

    FObject StandardInput;
    FObject StandardOutput;
    FObject StandardError;
    FObject QuoteSymbol;
    FObject QuasiquoteSymbol;
    FObject UnquoteSymbol;
    FObject UnquoteSplicingSymbol;

    FObject SyntaxRulesRecordType;
    FObject PatternVariableRecordType;
    FObject PatternRepeatRecordType;
    FObject TemplateRepeatRecordType;
    FObject SyntaxRuleRecordType;

    FObject EnvironmentRecordType;
    FObject GlobalRecordType;
    FObject LibraryRecordType;
    FObject NoValuePrimitiveObject;
    FObject LibraryStartupList;

    FObject WrongNumberOfArguments;
    FObject NotCallable;
    FObject UnexpectedNumberOfValues;
    FObject UndefinedMessage;
    FObject ExecuteThunk;
    FObject RaiseHandler;
    FObject InteractiveThunk;
    FObject ExceptionHandlerSymbol;

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

} FRoots;

extern FRoots R;

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

inline void ZeroOrOneArgsCheck(const char * who, int_t argc)
{
    if (argc > 1)
        RaiseExceptionC(R.Assertion, who, "expected zero or one arguments", EmptyListObject);
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

inline void NonNegativeArgCheck(const char * who, FObject arg)
{
    if (FixnumP(arg) == 0 || AsFixnum(arg) < 0)
        RaiseExceptionC(R.Assertion, who, "expected an exact non-negative integer", List(arg));
}

inline void IndexArgCheck(const char * who, FObject arg, FFixnum len)
{
    if (FixnumP(arg) == 0 || AsFixnum(arg) < 0 || AsFixnum(arg) >= len)
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
        RaiseExceptionC(R.Assertion, who, "expected a fixnum", List(obj));
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

inline void EqHashtableArgCheck(const char * who, FObject obj)
{
    if (HashtableP(obj) == 0 || PairP(AsHashtable(obj)->Tracker) == 0)
        RaiseExceptionC(R.Assertion, who, "expected an eq-hashtable", List(obj));
}

// ----------------

extern uint_t InlineProcedures;
extern uint_t InlineImports;

extern uint_t BytesAllocated;
extern uint_t CollectionCount;

FObject CompileProgram(FObject nam, FObject port);
FObject Eval(FObject obj, FObject env);
FObject GetInteractionEnv();

int_t EqP(FObject obj1, FObject obj2);
int_t EqvP(FObject obj1, FObject obj2);
int_t EqualP(FObject obj1, FObject obj2);

uint_t EqHash(FObject obj);
uint_t EqvHash(FObject obj);
uint_t EqualHash(FObject obj);

FObject SyntaxToDatum(FObject obj);

FObject ExecuteThunk(FObject op);

FObject MakeCommandLine(int_t argc, FChS * argv[]);
void SetupFoment(FThreadState * ts, int argc, FChS * argv[]);
extern uint_t SetupComplete;

// ---- Do Not Call Directly ----

void SetupCore(FThreadState * ts);
void SetupLibrary();
void SetupPairs();
void SetupCharacters();
void SetupStrings();
void SetupVectors();
void SetupIO();
void SetupCompile();
void SetupExecute();
void SetupNumbers();
void SetupThreads();
void SetupGC();

void WriteSpecialSyntax(FObject port, FObject obj, int_t df);
void WriteInstruction(FObject port, FObject obj, int_t df);
void WriteThread(FObject port, FObject obj, int_t df);
void WriteExclusive(FObject port, FObject obj, int_t df);
void WriteCondition(FObject port, FObject obj, int_t df);

#ifdef FOMENT_WINDOWS
#define PathCh '\\'
#define PathSep ';'
#else // FOMENT_WINDOWS
#define PathCh '/'
#define PathSep ':'
#endif // FOMENT_WINDOWS

#endif // __FOMENT_HPP__
