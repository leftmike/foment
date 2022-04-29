/*

Foment

*/

#ifndef __COMPILE_HPP__
#define __COMPILE_HPP__

// ---- SyntacticEnv ----

#define AsSyntacticEnv(obj) ((FSyntacticEnv *) (obj))
#define SyntacticEnvP(obj) (ObjectTag(obj) == SyntacticEnvTag)

typedef struct
{
    FObject GlobalBindings;
    FObject LocalBindings;
} FSyntacticEnv;

FObject MakeSyntacticEnv(FObject obj);

// ---- Binding ----

#define AsBinding(obj) ((FBinding *) (obj))
#define BindingP(obj) (ObjectTag(obj) == BindingTag)

typedef struct
{
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

FObject MakeBinding(FObject se, FObject id, FObject ra);

// ---- Reference ----

#define AsReference(obj) ((FReference *) (obj))
#define ReferenceP(obj) (ObjectTag(obj) == ReferenceTag)

typedef struct
{
    FObject Binding;
    FObject Identifier;
} FReference;

FObject MakeReference(FObject be, FObject id);

// ---- Lambda ----

#define AsLambda(obj) ((FLambda *) (obj))
#define LambdaP(obj) (ObjectTag(obj) == LambdaTag)

typedef struct
{
    FObject Name;
    FObject Bindings;
    FObject Body;

    FObject RestArg;
    FObject ArgCount;

    FObject Escapes;
    FObject UseStack; // Use a stack frame; otherwise, use a heap frame.
    FObject Level;
    FObject SlotCount;
    FObject CompilerPass;

    FObject Procedure;
    FObject BodyIndex;

    FObject Filename;
    FObject LineNumber;
} FLambda;

FObject MakeLambda(FObject enc, FObject nam, FObject bs, FObject body);

// ---- CaseLambda ----

#define AsCaseLambda(obj) ((FCaseLambda *) (obj))
#define CaseLambdaP(obj) (ObjectTag(obj) == CaseLambdaTag)

typedef struct
{
    FObject Cases;
    FObject Name;
    FObject Escapes;
} FCaseLambda;

FObject MakeCaseLambda(FObject cases);

// ----------------

FObject ResolveIdentifier(FObject se, FObject id);

FObject CompileLambda(FObject env, FObject name, FObject formals, FObject body);

FObject CompileSyntaxRules(FObject se, FObject obj);
FObject ExpandSyntaxRules(FObject se, FObject sr, FObject expr);

long_t MatchReference(FObject ref, FObject se, FObject expr);
FObject ExpandExpression(FObject enc, FObject se, FObject expr);
FObject CondExpand(FObject se, FObject expr, FObject clst);
FObject ReadInclude(FObject op, FObject lst, long_t cif);
FObject SPassLambda(FObject enc, FObject se, FObject name, FObject formals, FObject body);
void UPassLambda(FLambda * lam, int ef);
void CPassLambda(FLambda * lam);
void APassLambda(FLambda * enc, FLambda * lam);
FObject GPassLambda(FLambda * lam);

// ---- Roots ----

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
extern FObject EllipsisReference;
extern FObject UnderscoreReference;

// ---- Eternal Objects ----

extern FObject TagSymbol;
extern FObject UsePassSymbol;
extern FObject ConstantPassSymbol;
extern FObject AnalysisPassSymbol;

// ----------------

FObject FindOrLoadLibrary(FObject nam);
FObject LibraryName(FObject lst);
void CompileLibrary(FObject expr);
FObject CompileEval(FObject obj, FObject env);

#endif // __COMPILE_HPP__
