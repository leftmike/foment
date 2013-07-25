/*

Foment

Goals:
-- R7RS including syntax-rules
-- simple implementation

To Do:
-- documentation
-- ports optionally return or seek to a location
-- ports describe whether they use char offset, byte offset, or line number for location
-- CompileProgram
-- RunProgram

-- define-record-type

-- guard
-- parameterize
-- call-with-current-continuation
-- dynamic-wind
-- with-exception-handler
-- raise
-- raise-continuable
-- threads

-- strings and srfi-13

-- garbage collection

Future:
-- some mature segments are compacted during a full collection; ie. mark-compact
-- inline primitives in GPassExpression
-- debugging information
-- number tags should require only a single test by sharing part of a tag

Bugs:
-- gc.cpp: AllocateSection failing is not handled by all callers
-- execute.cpp: DynamicEnvironment needs to be put someplace
-- Execute does not check for CStack or AStack overflow
-- parameters are broken for threads
-- string input ports have not been tested at all
-- OpenInputFile and OpenOutFile don't convert the filename to C very carefully
-- update FeaturesC as more code gets written
-- remove Hash from FSymbol (part of Reserved)

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

typedef void * FObject;
typedef unsigned short FCh;
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
    PortTag,
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

typedef enum
{
    GCIdle = 0,
    GCRequired,
    GCPending,
    GCRunning
} FGCState;

extern FGCState GCState;

void Collect();
#define CheckForGC() if (GCState == GCRequired) Collect()

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
#define LetrecSyntax MakeImmediate(5, SpecialSyntaxTag)
#define LetrecStarSyntax MakeImmediate(6, SpecialSyntaxTag)
#define LetStarSyntax MakeImmediate(7, SpecialSyntaxTag)
#define LetValuesSyntax MakeImmediate(8, SpecialSyntaxTag)
#define LetStarValuesSyntax MakeImmediate(9, SpecialSyntaxTag)
#define LetrecValuesSyntax MakeImmediate(10, SpecialSyntaxTag)
#define LetrecStarValuesSyntax MakeImmediate(11, SpecialSyntaxTag)
#define LetSyntaxSyntax MakeImmediate(12, SpecialSyntaxTag)
#define LetrecSyntaxSyntax MakeImmediate(13, SpecialSyntaxTag)
#define CaseSyntax MakeImmediate(14, SpecialSyntaxTag)
#define OrSyntax MakeImmediate(15, SpecialSyntaxTag)
#define BeginSyntax MakeImmediate(16, SpecialSyntaxTag)
#define DoSyntax MakeImmediate(17, SpecialSyntaxTag)
#define SyntaxRulesSyntax MakeImmediate(18, SpecialSyntaxTag)
#define SyntaxErrorSyntax MakeImmediate(19, SpecialSyntaxTag)
#define IncludeSyntax MakeImmediate(20, SpecialSyntaxTag)
#define IncludeCISyntax MakeImmediate(21, SpecialSyntaxTag)
#define CondExpandSyntax MakeImmediate(22, SpecialSyntaxTag)
#define CaseLambdaSyntax MakeImmediate(23, SpecialSyntaxTag)
#define QuasiquoteSyntax MakeImmediate(24, SpecialSyntaxTag)

#define DefineSyntax MakeImmediate(25, SpecialSyntaxTag)
#define DefineValuesSyntax MakeImmediate(26, SpecialSyntaxTag)
#define DefineSyntaxSyntax MakeImmediate(27, SpecialSyntaxTag)

#define ImportSyntax MakeImmediate(28, SpecialSyntaxTag)
#define DefineLibrarySyntax MakeImmediate(29, SpecialSyntaxTag)

#define ElseSyntax MakeImmediate(30, SpecialSyntaxTag)
#define ArrowSyntax MakeImmediate(31, SpecialSyntaxTag)
#define UnquoteSyntax MakeImmediate(32, SpecialSyntaxTag)
#define UnquoteSplicingSyntax MakeImmediate(33, SpecialSyntaxTag)

#define OnlySyntax MakeImmediate(34, SpecialSyntaxTag)
#define ExceptSyntax MakeImmediate(35, SpecialSyntaxTag)
#define PrefixSyntax MakeImmediate(36, SpecialSyntaxTag)
#define RenameSyntax MakeImmediate(37, SpecialSyntaxTag)
#define ExportSyntax MakeImmediate(38, SpecialSyntaxTag)
#define IncludeLibraryDeclarationsSyntax MakeImmediate(39, SpecialSyntaxTag)

/*
#define ParameterizeSyntax MakeImmediate(, SpecialSyntaxTag)
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

FObject Assq(FObject obj, FObject alst);

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

inline unsigned int StringLength(FObject obj)
{
    unsigned int bl = ByteLength(obj);
    FAssert(bl % sizeof(FCh) == 0);

    return(bl / sizeof(FCh));
}

void StringToC(FObject s, char * b, int bl);
int StringAsNumber(FCh * s, int sl, FFixnum * np);
int NumberAsString(FFixnum n, FCh * s, FFixnum b);
int ChAlphabeticP(FCh ch);
int ChNumericP(FCh ch);
int ChWhitespaceP(FCh ch);
FCh ChUpCase(FCh ch);
FCh ChDownCase(FCh ch);
FObject FoldCaseString(FObject s);
unsigned int ByteLengthHash(char * b, int bl);
unsigned int StringLengthHash(FCh * s, int sl);
unsigned int StringHash(FObject obj);
int StringEqualP(FObject obj1, FObject obj2);
int StringLengthEqualP(FCh * s, int sl, FObject obj);
int StringCEqualP(char * s, FObject obj);
unsigned int BytevectorHash(FObject obj);

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

FObject MakeBytevector(unsigned int vl, FByte * v);
FObject U8ListToBytevector(FObject obj);

#define BytevectorLength(obj) ByteLength(obj)

// ---- Ports ----

#define PortP(obj) (IndirectTag(obj) == PortTag)
int InputPortP(FObject obj);
int OutputPortP(FObject obj);
void ClosePort(FObject port);

FObject OpenInputFile(FObject fn, int ref);
FObject OpenOutputFile(FObject fn, int ref);

FCh GetCh(FObject port, int * eof);
FCh PeekCh(FObject port, int * eof);
FObject Read(FObject port, int rif, int fcf);

FObject MakeStringCInputPort(char * s);
FObject ReadStringC(char * s, int rif);

void PutCh(FObject port, FCh ch);
void PutString(FObject port, FCh * s, int sl);
void PutStringC(FObject port, char * s);
void Write(FObject port, FObject obj, int df);
void WriteShared(FObject port, FObject obj, int df);
void WriteSimple(FObject port, FObject obj, int df);
void WritePretty(FObject port, FObject obj, int df);

int GetLocation(FObject port);

FObject MakeStringOutputPort();
FObject GetOutputString(FObject port);

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
int EnvironmentDefine(FObject env, FObject sym, FObject val);
FObject EnvironmentSet(FObject env, FObject sym, FObject val);
FObject EnvironmentSetC(FObject env, char * sym, FObject val);
FObject EnvironmentGet(FObject env, FObject sym);

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
    FObject OnStartup;
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
} FIdentifier;

#define AsIdentifier(obj) ((FIdentifier *) (obj))
#define IdentifierP(obj) RecordP(obj, R.IdentifierRecordType)

FObject MakeIdentifier(FObject sym, int ln);
FObject WrapIdentifier(FObject id, FObject se);

// ---- Procedure ----

typedef struct
{
    unsigned int Reserved;
    FObject Name;
    FObject Code;
    FObject RestArg;
} FProcedure;

#define AsProcedure(obj) ((FProcedure *) (obj))
#define ProcedureP(obj) (IndirectTag(obj) == ProcedureTag)

FObject MakeProcedure(FObject nam, FObject cv, int ac, FObject ra);

#define MAXIMUM_ARG_COUNT 0xFFFF
#define ProcedureArgCount(obj)\
    ((int) ((AsProcedure(obj)->Reserved >> RESERVED_BITS) & MAXIMUM_ARG_COUNT))

#define PROCEDURE_FLAG_CLOSURE   0x80000000
#define PROCEDURE_FLAG_PARAMETER 0x40000000

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

typedef struct _FThreadState
{
    struct _FThreadState * Next;
    struct _FThreadState * Previous;

    FObject Thread;

    FYoungSection * ActiveZero;

    int StackSize;
    int AStackPtr;
    FObject * AStack;
    int CStackPtr;
    FObject * CStack;

    FObject Proc;
    FObject Frame;
    int IP;
    int ArgCount;
} FThreadState;

// ----------------

typedef struct
{
    FObject Bedrock;
    FObject BedrockLibrary;
    FObject EllipsisSymbol;
    FObject Features;
    FObject CommandLine;
    FObject FullCommandLine;
    FObject LibraryPath;

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

    FObject WrongNumberOfArguments;
    FObject NotCallable;
    FObject UnexpectedNumberOfValues;
    FObject UndefinedMessage;
} FRoots;

extern FRoots R;

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
FObject DatumToSyntax(FObject obj);

FObject Execute(FObject op, int argc, FObject argv[]);

FObject MakeCommandLine(int argc, char * argv[]);
void SetupFoment(FThreadState * ts, int argc, char * argv[]);

// ---- Do Not Call Directly ----

void SetupCore(FThreadState * ts);
void SetupLibrary();
void SetupPairs();
void SetupStrings();
void SetupVectors();
void SetupIO();
void SetupCompile();
void SetupExecute();
void SetupNumbers();
void SetupGC();
void SetupThreads();

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
