/*

Foment

*/

#ifndef __COMPILE_HPP__
#define __COMPILE_HPP__

extern FObject UnderscoreSymbol;
extern FObject TagSymbol;
extern FObject ElseReference;
extern FObject ArrowReference;
extern FObject LibraryReference;
extern FObject AndReference;
extern FObject OrReference;
extern FObject NotReference;
extern FObject QuasiquoteReference;
extern FObject UnquoteReference;
extern FObject UnquoteSplicingReference;
extern FObject ConsReference;
extern FObject AppendReference;
extern FObject ListToVectorReference;

// ---- SyntacticEnv ----

typedef struct
{
    FRecord Record;
    FObject GlobalBindings;
    FObject LocalBindings;
} FSyntacticEnv;

#define AsSyntacticEnv(obj) ((FSyntacticEnv *) (obj))
#define SyntacticEnvP(obj) RecordP(obj, SyntacticEnvRecordType)

extern FObject SyntacticEnvRecordType;

FObject MakeSyntacticEnv(FObject obj);

// ---- Binding ----

typedef struct
{
    FRecord Record;

    FObject Identifier;
    FObject Syntax;
    FObject SyntacticEnv;
    FObject RestArg;

    FObject UseCount;
    FObject SetCount;
    FObject Escapes;
    FObject Level;
    FObject Slot;
    FObject Constant;
} FBinding;

#define AsBinding(obj) ((FBinding *) (obj))
#define BindingP(obj) RecordP(obj, BindingRecordType)

extern FObject BindingRecordType;

FObject MakeBinding(FObject se, FObject id, FObject ra);

// ---- Reference ----

typedef struct
{
    FRecord Record;
    FObject Binding;
    FObject Identifier;
} FReference;

#define AsReference(obj) ((FReference *) (obj))
#define ReferenceP(obj) RecordP(obj, ReferenceRecordType)

extern FObject ReferenceRecordType;

FObject MakeReference(FObject be, FObject id);

// ---- Lambda ----

typedef struct
{
    FRecord Record;

    FObject Name;
    FObject Bindings;
    FObject Body;

    FObject RestArg;
    FObject ArgCount;

    FObject Escapes;
    FObject UseStack; // Use a stack frame; otherwise, use a heap frame.
    FObject Level;
    FObject SlotCount;
    FObject MiddlePass;
    FObject MayInline;

    FObject Procedure;
    FObject BodyIndex;
} FLambda;

#define AsLambda(obj) ((FLambda *) (obj))
#define LambdaP(obj) RecordP(obj, LambdaRecordType)

extern FObject LambdaRecordType;

FObject MakeLambda(FObject nam, FObject bs, FObject body);

// ---- CaseLambda ----

typedef struct
{
    FRecord Record;

    FObject Cases;
    FObject Name;
    FObject Escapes;
} FCaseLambda;

#define AsCaseLambda(obj) ((FCaseLambda *) (obj))
#define CaseLambdaP(obj) RecordP(obj, CaseLambdaRecordType)

extern FObject CaseLambdaRecordType;

FObject MakeCaseLambda(FObject cases);

// ---- InlineVariable ----

typedef struct
{
    FRecord Record;

    FObject Index;
} FInlineVariable;

#define AsInlineVariable(obj) ((FInlineVariable *) (obj))
#define InlineVariableP(obj) RecordP(obj, InlineVariableRecordType)

extern FObject InlineVariableRecordType;

FObject MakeInlineVariable(int idx);

// ----------------

FObject ResolveIdentifier(FObject se, FObject id);

FObject CompileLambda(FObject env, FObject name, FObject formals, FObject body);

FObject CompileSyntaxRules(FObject se, FObject obj);
FObject ExpandSyntaxRules(FObject se, FObject sr, FObject expr);

int MatchReference(FObject ref, FObject se, FObject expr);
FObject ExpandExpression(FObject se, FObject expr);
FObject CondExpand(FObject se, FObject expr, FObject clst);
FObject ReadInclude(FObject lst, int cif);
FObject SPassLambda(FObject se, FObject name, FObject formals, FObject body);
void MPassLambda(FLambda * lam);

FObject GPassLambda(FLambda * lam);

void SetupSyntaxRules();

// ----------------

FObject FindOrLoadLibrary(FObject nam);
FObject LibraryName(FObject lst);
void CompileLibrary(FObject expr);
FObject CompileEval(FObject obj, FObject env);

#endif // __COMPILE_HPP__
