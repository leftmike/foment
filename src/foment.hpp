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

-- inline primitives in GPassExpression

-- define-record-type
-- guard
-- parameterize

-- debugging information

-- strings and srfi-13

-- number tags should require only a single test by sharing part of a tag

-- garbage collection

Bugs:

-- Execute does not check for CStack or AStack overflow
-- parameters are broken for threads
-- string input ports have not been tested at all
-- OpenInputFile and OpenOutFile don't convert the filename to C very carefully
-- update FeaturesC as more code gets written

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
    // Immediate Types

    FixnumTag = 0x01,        // 0bxxxx0001
    CharacterTag = 0x02,     // 0bxxxx0010
    MiscellaneousTag = 0x03, // 0bxxxx0011
    IllegalTag = 0x04,       // 0bxxxx0100
    SpecialSyntaxTag = 0x05, // 0bxxxx0101
    InstructionTag = 0x06,   // 0bxxxx0110
    ValuesCountTag = 0x07,   // 0bxxxx0111
    IllegalTag2 = 0x08       // 0bxxxx1000
} FImmediateTag;

typedef enum
{
    // Object Types

    PairTag = 1,
    BoxTag,
    StringTag,
    VectorTag,
    BytevectorTag,
    PortTag,
    ProcedureTag,
    SymbolTag,
    RecordTypeTag,
    RecordTag,
    PrimitiveTag,

    // Invalid Tag

    BadDogTag
} FObjectTag;

#define ObjectP(obj) ((((FImmediate) (obj)) & 0x3) == 0x0)

#define ImmediateTag(obj) (((FImmediate) (obj)) & 0xF)
#define ImmediateP(obj, it) (ImmediateTag((obj)) == it)
#define MakeImmediate(val, it)\
    ((FObject *) ((((FImmediate) (val)) << 4) | (it & 0xF)))
#define AsValue(obj) (((FImmediate) (obj)) >> 4)

// ---- Memory Management ----

FObject MakeObject(FObjectTag tag, unsigned int sz);

typedef struct
{
    unsigned short Hash;
    unsigned char Tag;
    unsigned char GCFlags;
#ifdef FOMENT_GCCHK
    unsigned int CheckSum;
#endif // FOMENT_GCCHK
} FObjectHeader;

#define AsObjectHeader(obj) (((FObjectHeader *) (obj)) - 1)
inline FObjectTag ObjectTag(FObject obj)
{
    return((FObjectTag) (ObjectP(obj) ? AsObjectHeader(obj)->Tag : 0));
}

void Root(FObject * rt);
void DropRoot();

extern int GCRequired;
void GarbageCollect();
#define AllowGC() if (GCRequired) GarbageCollect()

#ifdef FOMENT_GCCHK
void CheckSumObject(FObject obj);
FObject AsObject(FObject obj);
#else // FOMENT_GCCHK
#define AsObject(ptr) ((FObject) (ptr))
#define CheckSumObject(obj)
#endif // FOMENT_GCCHK

void ModifyVector(FObject obj, int idx, FObject val);

/*
//    AsPair(argv[0])->Rest = argv[1];
    Modify(FPair, argv[0], Rest, argv[1]);
*/
#define Modify(type, obj, slot, val)\
    ModifyObject(obj, (int) &(((type *) 0)->slot), val)

// Do not directly call ModifyObject; use Modify instead.
void ModifyObject(FObject obj, int off, FObject val);

#ifdef FOMENT_DEBUG
int ObjectLength(FObject obj);
int AlignLength(int len);
#endif // FOMENT_DEBUG

//
// ---- Immediate Types ----
//

#define FixnumP(obj) ImmediateP((obj), FixnumTag)
#define MakeFixnum(n)\
    ((FObject *) ((((FFixnum) (n)) << 4) | (FixnumTag & 0xF)))
#define AsFixnum(obj) (((FFixnum) (obj)) >> 4)

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

#define PairP(obj) (ObjectTag(obj) == PairTag)
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

FObject MakePair(FObject first, FObject rest);
int ListLength(FObject obj);
FObject ReverseListModify(FObject list);

FObject List(FObject obj);
FObject List(FObject obj1, FObject obj2);
FObject List(FObject obj1, FObject obj2, FObject obj3);
FObject List(FObject obj1, FObject obj2, FObject obj3, FObject obj4);

FObject Assq(FObject obj, FObject alst);

// ---- Boxes ----

#define BoxP(obj) (ObjectTag(obj) == BoxTag)
#define AsBox(obj) ((FBox *) (obj))

typedef struct
{
    FObject Value;
} FBox;

FObject MakeBox(FObject val);
inline FObject Unbox(FObject bx)
{
    FAssert(BoxP(bx));

    return(AsBox(bx)->Value);
}

// ---- Strings ----

#define StringP(obj) (ObjectTag(obj) == StringTag)
#define AsString(obj) ((FString *) (obj))

typedef struct
{
    int Length;
    FCh String[1];
} FString;

FObject MakeString(FCh * s, int sl);
FObject MakeStringCh(int sl, FCh ch);
FObject MakeStringC(char * s);
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

#define VectorP(obj) (ObjectTag(obj) == VectorTag)
#define AsVector(obj) ((FVector *) (obj))

typedef struct
{
    int Length;
    FObject Vector[1];
} FVector;

FObject MakeVector(int vl, FObject * v, FObject obj);
FObject ListToVector(FObject obj);
FObject VectorToList(FObject vec);

#define VectorLen(vec) (AsVector(vec)->Length)

// ---- Bytevectors ----

#define BytevectorP(obj) (ObjectTag(obj) == BytevectorTag)
#define AsBytevector(obj) ((FBytevector *) (obj))

typedef unsigned char FByte;
typedef struct
{
    int Length;
    FByte Vector[1];
} FBytevector;

FObject MakeBytevector(int vl, FByte * v);
FObject U8ListToBytevector(FObject obj);

// ---- Ports ----

#define PortP(obj) (ObjectTag(obj) == PortTag)
int InputPortP(FObject obj);
int OutputPortP(FObject obj);
void ClosePort(FObject port);

extern FObject StandardInput;
extern FObject StandardOutput;

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

#define RecordTypeP(obj) (ObjectTag(obj) == RecordTypeTag)
#define AsRecordType(obj) ((FRecordType *) (obj))

typedef struct
{
    FObject Name;
    int NumFields;
    FObject Fields[1];
} FRecordType;

FObject MakeRecordType(FObject nam, int nf, FObject flds[]);
FObject MakeRecordTypeC(char * nam, int nf, char * flds[]);

// ---- Records ----

#define GenericRecordP(obj) (ObjectTag(obj) == RecordTag)
#define AsGenericRecord(obj) ((FGenericRecord *) (obj))

typedef struct
{
    FObject RecordType;
    int NumFields;
} FRecord;

typedef struct
{
    FRecord Record;
    FObject Fields[1];
} FGenericRecord;

FObject MakeRecord(FObject rt);

inline int RecordP(FObject obj, FObject rt)
{
    return(GenericRecordP(obj) && AsGenericRecord(obj)->Record.RecordType == rt);
}

// ---- Hashtables ----

#define HashtableP(obj) RecordP(obj, HashtableRecordType)
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
} FHashtable;

FObject MakeHashtable(int nb);
FObject HashtableRef(FObject ht, FObject key, FObject def, FEquivFn efn, FHashFn hfn);
FObject HashtableStringRef(FObject ht, FCh * s, int sl, FObject def);
void HashtableSet(FObject ht, FObject key, FObject val, FEquivFn efn, FHashFn hfn);
void HashtableDelete(FObject ht, FObject key, FEquivFn efn, FHashFn hfn);
int HashtableContainsP(FObject ht, FObject key, FEquivFn efn, FHashFn hfn);
unsigned int HashtableSize(FObject ht);
void HashtableWalkUpdate(FObject ht, FWalkUpdateFn wfn, FObject ctx);
void HashtableWalkDelete(FObject ht, FWalkDeleteFn wfn, FObject ctx);
void HashtableWalkVisit(FObject ht, FWalkVisitFn wfn, FObject ctx);

extern FObject HashtableRecordType;

// ---- Symbols ----

#define SymbolP(obj) (ObjectTag(obj) == SymbolTag)
#define AsSymbol(obj) ((FSymbol *) (obj))

typedef struct
{
    FObject String;
    FObject Hash;
} FSymbol;

FObject StringToSymbol(FObject str);
FObject StringCToSymbol(char * s);
FObject StringLengthToSymbol(FCh * s, int sl);
FObject PrefixSymbol(FObject str, FObject sym);

// ---- Primitives ----

#define PrimitiveP(obj) (ObjectTag(obj) == PrimitiveTag)
#define AsPrimitive(obj) ((FPrimitive *) (obj))

typedef FObject (*FPrimitiveFn)(int argc, FObject argv[]);
typedef struct
{
    FPrimitiveFn PrimitiveFn;
    char * Name;
    char * Filename;
    int LineNumber;
} FPrimitive;

#define Define(name, fn)\
    static FObject fn ## Fn(int argc, FObject argv[]);\
    static FPrimitive fn = {fn ## Fn, name, __FILE__, __LINE__};\
    static FObject fn ## Fn

FObject MakePrimitive(FPrimitive * prim);
void DefinePrimitive(FObject env, FObject lib, FPrimitive * prim);

// ---- Environments ----

#define EnvironmentP(obj) RecordP(obj, EnvironmentRecordType)
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

extern FObject EnvironmentRecordType;

// ---- Globals ----

#define GlobalP(obj) RecordP(obj, GlobalRecordType)
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

extern FObject GlobalRecordType;

#define GlobalUndefined MakeFixnum(0)
#define GlobalDefined MakeFixnum(1)
#define GlobalModified MakeFixnum(2)
#define GlobalImported MakeFixnum(3)
#define GlobalImportedModified MakeFixnum(4)

// ---- Libraries ----

#define LibraryP(obj) RecordP(obj, LibraryRecordType)
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

extern FObject LibraryRecordType;
extern FObject LoadedLibraries;

// ---- Syntax Rules ----

#define SyntaxRulesP(obj) RecordP(obj, SyntaxRulesRecordType)
extern FObject SyntaxRulesRecordType;

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
#define IdentifierP(obj) RecordP(obj, IdentifierRecordType)

extern FObject IdentifierRecordType;

FObject MakeIdentifier(FObject sym, int ln);
FObject WrapIdentifier(FObject id, FObject se);

// ---- Procedure ----

typedef struct
{
    FObject Name;
    FObject Code;
    FObject RestArg;
    int ArgCount;
    unsigned int Flags;
} FProcedure;

#define AsProcedure(obj) ((FProcedure *) (obj))
#define ProcedureP(obj) (ObjectTag(obj) == ProcedureTag)

FObject MakeProcedure(FObject nam, FObject cv, int ac, FObject ra);

#define PROCEDURE_FLAG_CLOSURE   0x0001
#define PROCEDURE_FLAG_PARAMETER 0x0002

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
#define ExceptionP(obj) RecordP(obj, ExceptionRecordType)

extern FObject ExceptionRecordType;

FObject MakeException(FObject typ, FObject who, FObject msg, FObject lst);
void RaiseException(FObject typ, FObject who, FObject msg, FObject lst);
void RaiseExceptionC(FObject typ, char * who, char * msg, FObject lst);
void Raise(FObject obj);

extern FObject Assertion;
extern FObject Restriction;
extern FObject Lexical;
extern FObject Syntax;
extern FObject Error;

// ---- Config ----

typedef struct
{
    int InlineProcedures; // Enable inlining of some procedures.
    int InlinePrimitives; // Enable inlining of some primitives.
    int InlineImports; // Enable inlining of imports which are constant.
    int InteractiveLikeLibrary; // Treat interactive environments like libraries.
} FConfig;

extern FConfig Config;

// ----------------

extern FObject Bedrock;
extern FObject BedrockLibrary;
extern FObject EllipsisSymbol;
extern FObject Features;
extern FObject CommandLine;
extern FObject LibraryPath;

extern unsigned int BytesAllocated;

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
void SetupFoment(int argc, char * argv[]);

// ---- Do Not Call Directly ----

void SetupGC();
void SetupLibrary();
void SetupPairs();
void SetupStrings();
void SetupVectors();
void SetupIO();
void SetupCompile();
void SetupExecute();
void SetupNumbers();

void WriteSpecialSyntax(FObject port, FObject obj, int df);
void WriteInstruction(FObject port, FObject obj, int df);

#ifdef FOMENT_WIN32
#define PathCh '\\'
#else // FOMENT_WIN32
#define PathCh '/'
#endif // FOMENT_WIN32

#endif // __FOMENT_HPP__
