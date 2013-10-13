/*

Foment

Goals:
-- R7RS including syntax-rules
-- simple implementation

To Do:
-- CompileProgram
-- RunProgram

-- use tests from chibi

-- import and define-library should not be part of (scheme base)

-- IO and GC
-- boxes, vectors, procedures, records, and pairs need to be read and written using scheme code
-- or use FAlive
-- use EnterWait and LeaveWait
-- use Win32 file APIs and not stdio

-- from Gambit:
Serial Numbers
Serial numbers are used by the printer to identify objects which can’t be read
Convenient for debugging
> (let ((n 2)) (lambda (x) (* x n)))
#<procedure #2>
> (pp #2)
(lambda (x) (* x n))
> (map #2 ’(1 2 3 4 5))
(2 4 6 8 10)

Future:
-- some mature segments are compacted during a full collection; ie. mark-compact
-- inline primitives in GPassExpression
-- debugging information
-- number tags should require only a single test by sharing part of a tag
-- composable continuations
-- strings and srfi-13

Bugs:
-- current-second returns an exact integer
-- list->string needs to type check list
-- list->vector needs to type check list
-- ConvertToSystem does not protect an object from GC in some cases if IO allows GC
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
-- 6.4
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

#ifdef FOMENT_DEBUG
void FAssertFailed(char * fn, int ln, char * expr);
#define FAssert(expr)\
    if (! (expr)) FAssertFailed(__FILE__, __LINE__, #expr)
#else // FOMENT_DEBUG
#define FAssert(expr)
#endif // FOMENT_DEBUG

void FMustBeFailed(char * fn, int ln, char * expr);
#define FMustBe(expr)\
    if (! (expr)) FMustBeFailed(__FILE__, __LINE__, #expr)

typedef void * FObject;
typedef unsigned int FCh;
typedef wchar_t SCh;
typedef int FFixnum;
typedef unsigned int FImmediate;

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

FObject MakeObject(unsigned int sz, unsigned int tag);
FObject MakeMatureObject(unsigned int len, char * who);
FObject MakePinnedObject(unsigned int len, char * who);

#define RESERVED_BITS 6
#define RESERVED_TAGMASK 0x1F
#define RESERVED_MARK_BIT 0x20
#define MAXIMUM_OBJECT_LENGTH 0x3FFFFFF

#define ByteLength(obj) ((*((unsigned int *) (obj))) >> RESERVED_BITS)
#define ObjectLength(obj) ((*((unsigned int *) (obj))) >> RESERVED_BITS)
#define MakeLength(len, tag) (((len) << RESERVED_BITS) | (tag))
#define MakeMatureLength(len, tag) (((len) << RESERVED_BITS) | (tag) | RESERVED_MARK_BIT)
#define MakePinnedLength(len, tag) MakeMatureLength((len), (tag))

inline FIndirectTag IndirectTag(FObject obj)
{
    return((FIndirectTag) (IndirectP(obj) ? *((unsigned int *) (obj)) & RESERVED_TAGMASK : 0));
}

void PushRoot(FObject * rt);
void PopRoot();
void ClearRoots();

extern int GCRequired;

void EnterWait();
void LeaveWait();

void Collect();
#define CheckForGC() if (GCRequired) Collect()

void ModifyVector(FObject obj, unsigned int idx, FObject val);

/*
//    AsBox(argv[0])->Value = argv[1];
    Modify(FBox, argv[0], Value, argv[1]);
*/
#define Modify(type, obj, slot, val)\
    ModifyObject(obj, (int) &(((type *) 0)->slot), val)

// Do not directly call ModifyObject; use Modify instead.
void ModifyObject(FObject obj, int off, FObject val);

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

#define ValuesCountP(obj) ImmediateP(obj, ValuesCountTag)
#define MakeValuesCount(cnt) MakeImmediate(cnt, ValuesCountTag)
#define AsValuesCount(obj) ((int) (AsValue(obj)))

// ---- Special Syntax ----

#define SpecialSyntaxP(obj) ImmediateP((obj), SpecialSyntaxTag)
FObject SpecialSyntaxToSymbol(FObject obj);
FObject SpecialSyntaxMsgC(FObject obj, char * msg);

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

#define ImportSyntax MakeImmediate(26, SpecialSyntaxTag)
#define DefineLibrarySyntax MakeImmediate(27, SpecialSyntaxTag)

#define ElseSyntax MakeImmediate(28, SpecialSyntaxTag)
#define ArrowSyntax MakeImmediate(29, SpecialSyntaxTag)
#define UnquoteSyntax MakeImmediate(30, SpecialSyntaxTag)
#define UnquoteSplicingSyntax MakeImmediate(31, SpecialSyntaxTag)

#define OnlySyntax MakeImmediate(32, SpecialSyntaxTag)
#define ExceptSyntax MakeImmediate(33, SpecialSyntaxTag)
#define PrefixSyntax MakeImmediate(34, SpecialSyntaxTag)
#define RenameSyntax MakeImmediate(35, SpecialSyntaxTag)
#define ExportSyntax MakeImmediate(36, SpecialSyntaxTag)
#define IncludeLibraryDeclarationsSyntax MakeImmediate(37, SpecialSyntaxTag)

/*
#define GuardSyntax MakeImmediate(, SpecialSyntaxTag)
#define DefineRecordTypeSyntax MakeImmediate(, SpecialSyntaxTag)
*/

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
int ListLength(FObject obj);
FObject ReverseListModify(FObject list);

FObject List(FObject obj);
FObject List(FObject obj1, FObject obj2);
FObject List(FObject obj1, FObject obj2, FObject obj3);
FObject List(FObject obj1, FObject obj2, FObject obj3, FObject obj4);

FObject Memq(FObject obj, FObject lst);
FObject Assq(FObject obj, FObject alst);
FObject Assoc(FObject obj, FObject alst);

FObject MakeTConc();
int TConcEmptyP(FObject tconc);
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
    unsigned int Reserved;
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
    unsigned int Length;
    FCh String[1];
} FString;

FObject MakeString(FCh * s, unsigned int sl);
FObject MakeStringCh(unsigned int sl, FCh ch);
FObject MakeStringC(char * s);
FObject MakeStringS(SCh * ss);
FObject MakeStringS(SCh * ss, unsigned int ssl);

inline unsigned int StringLength(FObject obj)
{
    unsigned int bl = ByteLength(obj);
    FAssert(bl % sizeof(FCh) == 0);

    return(bl / sizeof(FCh));
}

void StringToC(FObject s, char * b, int bl);
int StringToNumber(FCh * s, int sl, FFixnum * np, FFixnum b);
int NumberAsString(FFixnum n, FCh * s, FFixnum b);
FObject FoldcaseString(FObject s);
unsigned int ByteLengthHash(char * b, int bl);
unsigned int StringLengthHash(FCh * s, int sl);
unsigned int StringHash(FObject obj);
int StringEqualP(FObject obj1, FObject obj2);
int StringLengthEqualP(FCh * s, int sl, FObject obj);
int StringCEqualP(char * s1, FCh * s2, int sl2);
unsigned int BytevectorHash(FObject obj);

#define ConvertToSystem(obj, ss)\
    SCh __ssbuf[256];\
    ss = ConvertToStringS(obj, __ssbuf, sizeof(__ssbuf) / sizeof(SCh))

SCh * ConvertToStringS(FObject s, SCh * b, unsigned int bl);

// ---- Vectors ----

#define VectorP(obj) (IndirectTag(obj) == VectorTag)
#define AsVector(obj) ((FVector *) (obj))

typedef struct
{
    unsigned int Length;
    FObject Vector[1];
} FVector;

FObject MakeVector(unsigned int vl, FObject * v, FObject obj);
FObject ListToVector(FObject obj);
FObject VectorToList(FObject vec);

#define VectorLength(obj) ObjectLength(obj)

// ---- Bytevectors ----

#define BytevectorP(obj) (IndirectTag(obj) == BytevectorTag)
#define AsBytevector(obj) ((FBytevector *) (obj))

typedef unsigned char FByte;
typedef struct
{
    unsigned int Length;
    FByte Vector[1];
} FBytevector;

FObject MakeBytevector(unsigned int vl);
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
    unsigned int Flags;
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

inline int InputPortP(FObject obj)
{
    return((BinaryPortP(obj) || TextualPortP(obj))
            && (AsGenericPort(obj)->Flags & PORT_FLAG_INPUT));
}

inline int OutputPortP(FObject obj)
{
    return((BinaryPortP(obj) || TextualPortP(obj))
            && (AsGenericPort(obj)->Flags & PORT_FLAG_OUTPUT));
}

inline int InputPortOpenP(FObject obj)
{
    FAssert(BinaryPortP(obj) || TextualPortP(obj));

    return(AsGenericPort(obj)->Flags & PORT_FLAG_INPUT_OPEN);
}

inline int OutputPortOpenP(FObject obj)
{
    FAssert(BinaryPortP(obj) || TextualPortP(obj));

    return(AsGenericPort(obj)->Flags & PORT_FLAG_OUTPUT_OPEN);
}

inline int StringOutputPortP(FObject obj)
{
    return(TextualPortP(obj) && (AsGenericPort(obj)->Flags & PORT_FLAG_STRING_OUTPUT));
}

inline int BytevectorOutputPortP(FObject obj)
{
    return(BinaryPortP(obj) && (AsGenericPort(obj)->Flags & PORT_FLAG_BYTEVECTOR_OUTPUT));
}

inline int FoldcasePortP(FObject port)
{
    FAssert(TextualPortP(port) && InputPortP(port));

    return(AsGenericPort(port)->Flags & PORT_FLAG_FOLDCASE);
}

inline int WantIdentifiersPortP(FObject port)
{
    FAssert(TextualPortP(port) && InputPortP(port));

    return(AsGenericPort(port)->Flags & PORT_FLAG_WANT_IDENTIFIERS);
}

// Binary and textual ports

void CloseInput(FObject port);
void CloseOutput(FObject port);
void FlushOutput(FObject port);

// Binary ports

unsigned int ReadBytes(FObject port, FByte * b, unsigned int bl);
int PeekByte(FObject port, FByte * b);
int ByteReadyP(FObject port);
unsigned int GetOffset(FObject port);

void WriteBytes(FObject port, void * b, unsigned int bl);

// Textual ports

FObject OpenInputFile(FObject fn);
FObject OpenOutputFile(FObject fn);
FObject MakeStringInputPort(FObject str);
FObject MakeStringCInputPort(char * s);
FObject MakeStringOutputPort();
FObject GetOutputString(FObject port);

unsigned int ReadCh(FObject port, FCh * ch);
unsigned int PeekCh(FObject port, FCh * ch);
int CharReadyP(FObject port);
FObject ReadLine(FObject port);
FObject ReadString(FObject port, int cnt);
unsigned int GetLineColumn(FObject port, unsigned * col);
void FoldcasePort(FObject port, int fcf);
void WantIdentifiersPort(FObject port, int wif);

FObject Read(FObject port);

void WriteCh(FObject port, FCh ch);
void WriteString(FObject port, FCh * s, int sl);
void WriteStringC(FObject port, char * s);

void Write(FObject port, FObject obj, int df);
void WriteShared(FObject port, FObject obj, int df);
void WriteSimple(FObject port, FObject obj, int df);

// ---- Record Types ----

#define RecordTypeP(obj) (IndirectTag(obj) == RecordTypeTag)
#define AsRecordType(obj) ((FRecordType *) (obj))

typedef struct
{
    unsigned int NumFields;
    FObject Fields[1];
} FRecordType;

FObject MakeRecordType(FObject nam, unsigned int nf, FObject flds[]);
FObject MakeRecordTypeC(char * nam, unsigned int nf, char * flds[]);

#define RecordTypeName(obj) AsRecordType(obj)->Fields[0]
#define RecordTypeNumFields(obj) ObjectLength(obj)

// ---- Records ----

#define GenericRecordP(obj) (IndirectTag(obj) == RecordTag)
#define AsGenericRecord(obj) ((FGenericRecord *) (obj))

typedef struct
{
    unsigned int NumFields; // RecordType is include in the number of fields.
    FObject RecordType;
} FRecord;

typedef struct
{
    unsigned int NumFields;
    FObject Fields[1];
} FGenericRecord;

FObject MakeRecord(FObject rt);

inline int RecordP(FObject obj, FObject rt)
{
    return(GenericRecordP(obj) && AsGenericRecord(obj)->Fields[0] == rt);
}

#define RecordNumFields(obj) ObjectLength(obj)

// ---- Hashtables ----

#define HashtableP(obj) RecordP(obj, R.HashtableRecordType)
#define AsHashtable(obj) ((FHashtable *) (obj))

typedef int (*FEquivFn)(FObject obj1, FObject obj2);
typedef unsigned int (*FHashFn)(FObject obj);
typedef FObject (*FWalkUpdateFn)(FObject key, FObject val, FObject ctx);
typedef int (*FWalkDeleteFn)(FObject key, FObject val, FObject ctx);
typedef void (*FWalkVisitFn)(FObject key, FObject val, FObject ctx);

typedef struct
{
    FRecord Record;
    FObject Buckets; // Must be a vector.
    FObject Size; // Number of keys.
    FObject Tracker; // TConc used by EqHashtables to track movement of keys.
} FHashtable;

FObject MakeHashtable(int nb);
FObject HashtableRef(FObject ht, FObject key, FObject def, FEquivFn efn, FHashFn hfn);
FObject HashtableStringRef(FObject ht, FCh * s, int sl, FObject def);
void HashtableSet(FObject ht, FObject key, FObject val, FEquivFn efn, FHashFn hfn);
void HashtableDelete(FObject ht, FObject key, FEquivFn efn, FHashFn hfn);
int HashtableContainsP(FObject ht, FObject key, FEquivFn efn, FHashFn hfn);

FObject MakeEqHashtable(int nb);
FObject EqHashtableRef(FObject ht, FObject key, FObject def);
void EqHashtableSet(FObject ht, FObject key, FObject val);
void EqHashtableDelete(FObject ht, FObject key);
int EqHashtableContainsP(FObject ht, FObject key);

unsigned int HashtableSize(FObject ht);
void HashtableWalkUpdate(FObject ht, FWalkUpdateFn wfn, FObject ctx);
void HashtableWalkDelete(FObject ht, FWalkDeleteFn wfn, FObject ctx);
void HashtableWalkVisit(FObject ht, FWalkVisitFn wfn, FObject ctx);

// ---- Symbols ----

#define SymbolP(obj) (IndirectTag(obj) == SymbolTag)
#define AsSymbol(obj) ((FSymbol *) (obj))

typedef struct
{
    unsigned int Reserved;
    FObject String;
} FSymbol;

FObject StringToSymbol(FObject str);
FObject StringCToSymbol(char * s);
FObject StringLengthToSymbol(FCh * s, int sl);
FObject PrefixSymbol(FObject str, FObject sym);

#define SymbolHash(obj) ByteLength(obj)

// ---- Primitives ----

#define PrimitiveP(obj) (IndirectTag(obj) == PrimitiveTag)
#define AsPrimitive(obj) ((FPrimitive *) (obj))

typedef FObject (*FPrimitiveFn)(int argc, FObject argv[]);
typedef struct
{
    unsigned int Reserved;
    FPrimitiveFn PrimitiveFn;
    char * Name;
    char * Filename;
    int LineNumber;
} FPrimitive;

#define Define(name, fn)\
    static FObject fn ## Fn(int argc, FObject argv[]);\
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
} FEnvironment;

FObject MakeEnvironment(FObject nam, FObject ctv);
FObject EnvironmentBind(FObject env, FObject sym);
FObject EnvironmentLookup(FObject env, FObject sym);
int EnvironmentDefine(FObject env, FObject symid, FObject val);
FObject EnvironmentSet(FObject env, FObject sym, FObject val);
FObject EnvironmentSetC(FObject env, char * sym, FObject val);
FObject EnvironmentGet(FObject env, FObject symid);

void EnvironmentImportLibrary(FObject env, FObject nam);
void EnvironmentImport(FObject env, FObject form);

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

FObject MakeIdentifier(FObject sym, int ln);
FObject WrapIdentifier(FObject id, FObject se);

// ---- Procedures ----

typedef struct
{
    unsigned int Reserved;
    FObject Name;
    FObject Code;
} FProcedure;

#define AsProcedure(obj) ((FProcedure *) (obj))
#define ProcedureP(obj) (IndirectTag(obj) == ProcedureTag)

FObject MakeProcedure(FObject nam, FObject cv, int ac, unsigned int fl);

#define MAXIMUM_ARG_COUNT 0xFFFF
#define ProcedureArgCount(obj)\
    ((int) ((AsProcedure(obj)->Reserved >> RESERVED_BITS) & MAXIMUM_ARG_COUNT))

#define PROCEDURE_FLAG_CLOSURE      0x80000000
#define PROCEDURE_FLAG_PARAMETER    0x40000000
#define PROCEDURE_FLAG_CONTINUATION 0x20000000
#define PROCEDURE_FLAG_RESTARG      0x10000000

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
void RaiseExceptionC(FObject typ, char * who, char * msg, FObject lst);
void Raise(FObject obj);

// ---- Config ----

typedef struct
{
    int InlineProcedures; // Enable inlining of some procedures.
    int InlinePrimitives; // Enable inlining of some primitives.
    int InlineImports; // Enable inlining of imports which are constant.
    int InteractiveLikeLibrary; // Treat interactive environments like libraries.
} FConfig;

extern FConfig Config;

// ---- Thread State ----

typedef struct _FYoungSection
{
    struct _FYoungSection * Next;
    unsigned int Used;
    unsigned int Scan;
} FYoungSection;

#define INDEX_PARAMETERS 3

typedef struct _FThreadState
{
    struct _FThreadState * Next;
    struct _FThreadState * Previous;

    FObject Thread;

    FYoungSection * ActiveZero;
    unsigned int ObjectsSinceLast;

    int UsedRoots;
    FObject * Roots[12];

    int StackSize;
    int AStackPtr;
    FObject * AStack;
    int CStackPtr;
    FObject * CStack;

    FObject Proc;
    FObject Frame;
    int IP;
    int ArgCount;

    FObject DynamicStack;
    FObject Parameters;
    FObject IndexParameters[INDEX_PARAMETERS];
} FThreadState;

// ---- Roots ----

typedef struct
{
    FObject Bedrock;
    FObject BedrockLibrary;
    FObject EllipsisSymbol;
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

    FObject UnderscoreSymbol;
    FObject TagSymbol;

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
    FObject ExceptionHandlerSymbol;

    FObject DynamicRecordType;
    FObject ContinuationRecordType;

    FObject ExclusivesTConc;
} FRoots;

extern FRoots R;

// ---- Argument Checking ----

inline void ZeroArgsCheck(char * who, int argc)
{
    if (argc != 0)
        RaiseExceptionC(R.Assertion, who, "expected no arguments", EmptyListObject);
}

inline void OneArgCheck(char * who, int argc)
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, who, "expected one argument", EmptyListObject);
}

inline void TwoArgsCheck(char * who, int argc)
{
    if (argc != 2)
        RaiseExceptionC(R.Assertion, who, "expected two arguments", EmptyListObject);
}

inline void ThreeArgsCheck(char * who, int argc)
{
    if (argc != 3)
        RaiseExceptionC(R.Assertion, who, "expected three arguments", EmptyListObject);
}

inline void AtLeastOneArgCheck(char * who, int argc)
{
    if (argc < 1)
        RaiseExceptionC(R.Assertion, who, "expected at least one argument", EmptyListObject);
}

inline void AtLeastTwoArgsCheck(char * who, int argc)
{
    if (argc < 2)
        RaiseExceptionC(R.Assertion, who, "expected at least two arguments", EmptyListObject);
}

inline void AtLeastThreeArgsCheck(char * who, int argc)
{
    if (argc < 3)
        RaiseExceptionC(R.Assertion, who, "expected at least three arguments", EmptyListObject);
}

inline void ZeroOrOneArgsCheck(char * who, int argc)
{
    if (argc > 1)
        RaiseExceptionC(R.Assertion, who, "expected zero or one arguments", EmptyListObject);
}

inline void OneOrTwoArgsCheck(char * who, int argc)
{
    if (argc < 1 || argc > 2)
        RaiseExceptionC(R.Assertion, who, "expected one or two arguments", EmptyListObject);
}

inline void OneToThreeArgsCheck(char * who, int argc)
{
    if (argc < 1 || argc > 3)
        RaiseExceptionC(R.Assertion, who, "expected one to three arguments", EmptyListObject);
}

inline void OneToFourArgsCheck(char * who, int argc)
{
    if (argc < 1 || argc > 4)
        RaiseExceptionC(R.Assertion, who, "expected one to four arguments", EmptyListObject);
}

inline void TwoToFourArgsCheck(char * who, int argc)
{
    if (argc < 2 || argc > 4)
        RaiseExceptionC(R.Assertion, who, "expected two to four arguments", EmptyListObject);
}

inline void ThreeToFiveArgsCheck(char * who, int argc)
{
    if (argc < 3 || argc > 5)
        RaiseExceptionC(R.Assertion, who, "expected three to five arguments", EmptyListObject);
}

inline void NonNegativeArgCheck(char * who, FObject arg)
{
    if (FixnumP(arg) == 0 || AsFixnum(arg) < 0)
        RaiseExceptionC(R.Assertion, who, "expected an exact non-negative integer", List(arg));
}

inline void IndexArgCheck(char * who, FObject arg, FFixnum len)
{
    if (FixnumP(arg) == 0 || AsFixnum(arg) < 0 || AsFixnum(arg) >= len)
        RaiseExceptionC(R.Assertion, who, "expected a valid index", List(arg));
}

inline void EndIndexArgCheck(char * who, FObject arg, FFixnum strt, FFixnum len)
{
    if (FixnumP(arg) == 0 || AsFixnum(arg) < strt || AsFixnum(arg) > len)
        RaiseExceptionC(R.Assertion, who, "expected a valid index", List(arg));
}

inline void ByteArgCheck(char * who, FObject obj)
{
    if (FixnumP(obj) == 0 || AsFixnum(obj) < 0 || AsFixnum(obj) > 0xFF)
        RaiseExceptionC(R.Assertion, who, "expected a byte: an exact integer between 0 and 255",
                List(obj));
}

inline void FixnumArgCheck(char * who, FObject obj)
{
    if (FixnumP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a fixnum", List(obj));
}

inline void CharacterArgCheck(char * who, FObject obj)
{
    if (CharacterP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a character", List(obj));
}

inline void BooleanArgCheck(char * who, FObject obj)
{
    if (BooleanP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a boolean", List(obj));
}

inline void SymbolArgCheck(char * who, FObject obj)
{
    if (SymbolP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a symbol", List(obj));
}

inline void ExceptionArgCheck(char * who, FObject obj)
{
    if (ExceptionP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected an error-object", List(obj));
}

inline void StringArgCheck(char * who, FObject obj)
{
    if (StringP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a string", List(obj));
}

inline void VectorArgCheck(char * who, FObject obj)
{
    if (VectorP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a vector", List(obj));
}

inline void BytevectorArgCheck(char * who, FObject obj)
{
    if (BytevectorP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a bytevector", List(obj));
}

inline void PortArgCheck(char * who, FObject obj)
{
    if (BinaryPortP(obj) == 0 && TextualPortP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a port", List(obj));
}

inline void InputPortArgCheck(char * who, FObject obj)
{
    if ((BinaryPortP(obj) == 0 && TextualPortP(obj) == 0) || InputPortP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected an input port", List(obj));
}

inline void OutputPortArgCheck(char * who, FObject obj)
{
    if ((BinaryPortP(obj) == 0 && TextualPortP(obj) == 0) || OutputPortP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected an output port", List(obj));
}

inline void BinaryPortArgCheck(char * who, FObject obj)
{
    if (BinaryPortP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a binary port", List(obj));
}

inline void TextualPortArgCheck(char * who, FObject obj)
{
    if (TextualPortP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a textual port", List(obj));
}

inline void StringOutputPortArgCheck(char * who, FObject obj)
{
    if (StringOutputPortP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a string output port", List(obj));
}

inline void BytevectorOutputPortArgCheck(char * who, FObject obj)
{
    if (BytevectorOutputPortP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected a bytevector output port", List(obj));
}

inline void TextualInputPortArgCheck(char * who, FObject obj)
{
    if (TextualPortP(obj) == 0 || InputPortOpenP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected an open textual input port", List(obj));
}

inline void TextualOutputPortArgCheck(char * who, FObject obj)
{
    if (TextualPortP(obj) == 0 || OutputPortOpenP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected an open textual output port", List(obj));
}

inline void BinaryInputPortArgCheck(char * who, FObject obj)
{
    if (BinaryPortP(obj) == 0 || InputPortOpenP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected an open binary input port", List(obj));
}

inline void BinaryOutputPortArgCheck(char * who, FObject obj)
{
    if (BinaryPortP(obj) == 0 || OutputPortOpenP(obj) == 0)
        RaiseExceptionC(R.Assertion, who, "expected an open binary output port", List(obj));
}

// ----------------

extern unsigned int BytesAllocated;
extern unsigned int CollectionCount;

FObject Eval(FObject obj, FObject env);
FObject GetInteractionEnv();

int EqP(FObject obj1, FObject obj2);
int EqvP(FObject obj1, FObject obj2);
int EqualP(FObject obj1, FObject obj2);

unsigned int EqHash(FObject obj);
unsigned int EqvHash(FObject obj);
unsigned int EqualHash(FObject obj);

FObject SyntaxToDatum(FObject obj);

FObject ExecuteThunk(FObject op);

FObject MakeCommandLine(int argc, SCh * argv[]);
void SetupFoment(FThreadState * ts, int argc, SCh * argv[]);
extern unsigned int SetupComplete;

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

void WriteSpecialSyntax(FObject port, FObject obj, int df);
void WriteInstruction(FObject port, FObject obj, int df);
void WriteThread(FObject port, FObject obj, int df);
void WriteExclusive(FObject port, FObject obj, int df);
void WriteCondition(FObject port, FObject obj, int df);

#ifdef FOMENT_WIN32
#define PathCh '\\'
#else // FOMENT_WIN32
#define PathCh '/'
#endif // FOMENT_WIN32

#endif // __FOMENT_HPP__
