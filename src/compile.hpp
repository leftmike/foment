/*

Foment

*/

#ifndef __COMPILE_HPP__
#define __COMPILE_HPP__

// ---- SyntacticEnv ----

typedef struct
{
    FRecord Record;
    FObject GlobalBindings;
    FObject LocalBindings;
} FSyntacticEnv;

#define AsSyntacticEnv(obj) ((FSyntacticEnv *) (obj))
#define SyntacticEnvP(obj) RecordP(obj, R.SyntacticEnvRecordType)

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
#define BindingP(obj) RecordP(obj, R.BindingRecordType)

FObject MakeBinding(FObject se, FObject id, FObject ra);

// ---- Reference ----

typedef struct
{
    FRecord Record;
    FObject Binding;
    FObject Identifier;
} FReference;

#define AsReference(obj) ((FReference *) (obj))
#define ReferenceP(obj) RecordP(obj, R.ReferenceRecordType)

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
#define LambdaP(obj) RecordP(obj, R.LambdaRecordType)

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
#define CaseLambdaP(obj) RecordP(obj, R.CaseLambdaRecordType)

FObject MakeCaseLambda(FObject cases);

// ---- InlineVariable ----

typedef struct
{
    FRecord Record;

    FObject Index;
} FInlineVariable;

#define AsInlineVariable(obj) ((FInlineVariable *) (obj))
#define InlineVariableP(obj) RecordP(obj, R.InlineVariableRecordType)

FObject MakeInlineVariable(int idx);

// ---- Pattern Variable ----

typedef struct
{
    FRecord Record;
    FObject RepeatDepth;
    FObject Index;
    FObject Variable;
} FPatternVariable;

#define AsPatternVariable(obj) ((FPatternVariable *) (obj))
#define PatternVariableP(obj) RecordP(obj, R.PatternVariableRecordType)

// ---- Pattern Repeat ----

typedef struct
{
    FRecord Record;
    FObject LeaveCount;
    FObject Ellipsis;
    FObject Variables;
    FObject Pattern;
    FObject Rest;
} FPatternRepeat;

#define AsPatternRepeat(obj) ((FPatternRepeat *) (obj))
#define PatternRepeatP(obj) RecordP(obj, R.PatternRepeatRecordType)

// ---- Template Repeat ----

typedef struct
{
    FRecord Record;
    FObject Ellipsis;
    FObject RepeatCount;
    FObject Variables;
    FObject Template;
    FObject Rest;
} FTemplateRepeat;

#define AsTemplateRepeat(obj) ((FTemplateRepeat *) (obj))
#define TemplateRepeatP(obj) RecordP(obj, R.TemplateRepeatRecordType)

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
