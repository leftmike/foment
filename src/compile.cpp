/*

Foment

*/

#include "foment.hpp"
#include "compile.hpp"

FObject UnderscoreSymbol;
FObject TagSymbol;

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

static FObject InteractionEnv;

// ---- SyntacticEnv ----

FObject SyntacticEnvRecordType;
static char * SyntacticEnvFieldsC[] = {"global-bindings", "local-bindings"};

FObject MakeSyntacticEnv(FObject obj)
{
    FAssert(sizeof(FSyntacticEnv) == sizeof(SyntacticEnvFieldsC) + sizeof(FRecord));
    FAssert(EnvironmentP(obj) || SyntacticEnvP(obj));

    FSyntacticEnv * se = (FSyntacticEnv *) MakeRecord(SyntacticEnvRecordType);
    if (EnvironmentP(obj))
    {
        se->GlobalBindings = obj;
        se->LocalBindings = EmptyListObject;
    }
    else
    {
        se->GlobalBindings = AsSyntacticEnv(obj)->GlobalBindings;
        se->LocalBindings = AsSyntacticEnv(obj)->LocalBindings;
    }

    return(AsObject(se));
}

// ---- Binding ----

FObject BindingRecordType;
static char * BindingFieldsC[] = {"identifier", "syntax", "syntactic-env", "rest-arg", "use-count",
    "set-count", "escapes", "level", "slot", "constant"};

FObject MakeBinding(FObject se, FObject id, FObject ra)
{
    FAssert(sizeof(FBinding) == sizeof(BindingFieldsC) + sizeof(FRecord));
    FAssert(SyntacticEnvP(se));
    FAssert(IdentifierP(id));
    FAssert(ra == TrueObject || ra == FalseObject);

    FBinding * b = (FBinding *) MakeRecord(BindingRecordType);
    b->Identifier = id;
    b->Syntax = NoValueObject;
    b->SyntacticEnv = se;
    b->RestArg = ra;

    b->UseCount = MakeFixnum(0);
    b->SetCount = MakeFixnum(0);
    b->Escapes = FalseObject;
    b->Level = MakeFixnum(0);
    b->Slot = MakeFixnum(-1);
    b->Constant = NoValueObject;

    return(AsObject(b));
}

// ---- Identifier ----

static int IdentifierMagic = 0;

FObject IdentifierRecordType;
static char * IdentifierFieldsC[] = {"symbol", "line-number", "magic", "syntactic-env"};

FObject MakeIdentifier(FObject sym, int ln)
{
    FAssert(sizeof(FIdentifier) == sizeof(IdentifierFieldsC) + sizeof(FRecord));
    FAssert(SymbolP(sym));

    FIdentifier * i = (FIdentifier *) MakeRecord(IdentifierRecordType);
    i->Symbol = sym;
    i->LineNumber = MakeFixnum(ln);

    IdentifierMagic += 1;
    i->Magic = MakeFixnum(IdentifierMagic);

    i->SyntacticEnv = NoValueObject;

    return(AsObject(i));
}

FObject WrapIdentifier(FObject id, FObject se)
{
    FAssert(IdentifierP(id));

    if (SyntacticEnvP(AsIdentifier(id)->SyntacticEnv))
        return(id);

    FAssert(sizeof(FIdentifier) == sizeof(IdentifierFieldsC) + sizeof(FRecord));
    FAssert(SyntacticEnvP(se));

    FIdentifier * i = (FIdentifier *) MakeRecord(IdentifierRecordType);
    i->Symbol = AsIdentifier(id)->Symbol;
    i->LineNumber = AsIdentifier(id)->LineNumber;
    i->Magic = AsIdentifier(id)->Magic;
    i->SyntacticEnv = se;

    return(AsObject(i));
}

// ---- Lambda ----

FObject LambdaRecordType;
static char * LambdaFieldsC[] = {"name", "bindings", "body", "rest-arg", "arg-count",
    "escapes", "use-stack", "level", "slot-count", "middle-pass", "procedure", "body-index",
    "may-inline"};

FObject MakeLambda(FObject nam, FObject bs, FObject body)
{
    FAssert(sizeof(FLambda) == sizeof(LambdaFieldsC) + sizeof(FRecord));

    FLambda * l = (FLambda *) MakeRecord(LambdaRecordType);
    l->Name = nam;
    l->Bindings = bs;
    l->Body = body;

    l->RestArg = FalseObject;
    l->ArgCount = MakeFixnum(0);

    l->Escapes = FalseObject;
    l->UseStack = TrueObject;
    l->Level = MakeFixnum(0);
    l->SlotCount = MakeFixnum(-1);
    l->MiddlePass = NoValueObject;
    l->MayInline = MakeFixnum(0); // Number of procedure calls or FalseObject.

    l->Procedure = NoValueObject;
    l->BodyIndex = NoValueObject;

    return(AsObject(l));
}

// ---- CaseLambda ----

FObject CaseLambdaRecordType;
static char * CaseLambdaFieldsC[] = {"cases", "name", "escapes"};

FObject MakeCaseLambda(FObject cases)
{
    FAssert(sizeof(FCaseLambda) == sizeof(CaseLambdaFieldsC) + sizeof(FRecord));

    FCaseLambda * cl = (FCaseLambda *) MakeRecord(CaseLambdaRecordType);
    cl->Cases = cases;
    cl->Name = NoValueObject;
    cl->Escapes = FalseObject;

    return(AsObject(cl));
}

// ---- InlineVariable ----

FObject InlineVariableRecordType;
static char * InlineVariableFieldsC[] = {"index"};

FObject MakeInlineVariable(int idx)
{
    FAssert(sizeof(FInlineVariable) == sizeof(InlineVariableFieldsC) + sizeof(FRecord));

    FAssert(idx >= 0);

    FInlineVariable * iv = (FInlineVariable *) MakeRecord(InlineVariableRecordType);
    iv->Index = MakeFixnum(idx);

    return(AsObject(iv));
}

// ---- Reference ----

FObject ReferenceRecordType;
static char * ReferenceFieldsC[] = {"binding", "identifier"};

FObject MakeReference(FObject be, FObject id)
{
    FAssert(sizeof(FReference) == sizeof(ReferenceFieldsC) + sizeof(FRecord));
    FAssert(BindingP(be) || EnvironmentP(be));
    FAssert(IdentifierP(id));

    FReference * r = (FReference *) MakeRecord(ReferenceRecordType);
    r->Binding = be;
    r->Identifier = id;

    return(AsObject(r));
}

// ----------------

FObject CompileLambda(FObject env, FObject name, FObject formals, FObject body)
{
    FObject obj = SPassLambda(MakeSyntacticEnv(env), name, formals, body);
    FAssert(LambdaP(obj));

    MPassLambda(AsLambda(obj));
    return(GPassLambda(AsLambda(obj)));
}

FObject GetInteractionEnv()
{
    if (EnvironmentP(InteractionEnv) == 0)
    {
        InteractionEnv = MakeEnvironment(List(StringCToSymbol("interaction")), TrueObject);
        EnvironmentImportLibrary(InteractionEnv,
                List(StringCToSymbol("scheme"), StringCToSymbol("base")));
    }

    return(InteractionEnv);
}

// ----------------

Define("compile-eval", CompileEvalPrimitive)(int argc, FObject argv[])
{
    if (argc != 2)
        RaiseExceptionC(Assertion, "compile-eval", "compile-eval: expected two arguments",
                EmptyListObject);

    if (EnvironmentP(argv[1]) == 0)
        RaiseExceptionC(Assertion, "compile-eval", "compile-eval: expected an environment",
                List(argv[1]));

    return(CompileEval(DatumToSyntax(argv[0]), argv[1]));
}

Define("interaction-environment", InteractionEnvironmentPrimitive)(int argc, FObject argv[])
{
    if (argc != 0)
        RaiseExceptionC(Assertion, "interaction-environment",
                "interaction-environment: expected no arguments", EmptyListObject);

    return(GetInteractionEnv());
}

Define("compile", CompilePrimitive)(int argc, FObject argv[])
{
    if (argc < 2 || argc > 3)
        RaiseExceptionC(Assertion, "compile", "compile: expected two or three arguments",
                EmptyListObject);

    if (argc == 2)
        return(CompileLambda(Bedrock, NoValueObject, argv[0], argv[1]));
    return(CompileLambda(Bedrock, argv[0], argv[1], argv[2]));
}

Define("syntax-pass", SyntaxPassPrimitive)(int argc, FObject argv[])
{
    if (argc < 2 || argc > 3)
        RaiseExceptionC(Assertion, "syntax-pass", "syntax-pass: expected two or three arguments",
                EmptyListObject);

    FObject se = MakeSyntacticEnv(Bedrock);

    if (argc == 2)
        return(SPassLambda(se, NoValueObject, DatumToSyntax(argv[0]), DatumToSyntax(argv[1])));
    return(SPassLambda(se, DatumToSyntax(argv[0]), DatumToSyntax(argv[1]),
            DatumToSyntax(argv[2])));
}

Define("middle-pass", MiddlePassPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(Assertion, "middle-pass", "middle-pass: expected one argument",
                EmptyListObject);

    if (LambdaP(argv[0]) == 0)
        RaiseExceptionC(Assertion, "middle-pass", "middle-pass: expected a lambda",
                List(argv[0]));

    MPassLambda(AsLambda(argv[0]));
    return(argv[0]);
}

Define("generate-pass", GeneratePassPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(Assertion, "generate-pass", "generate-pass: expected one argument",
                EmptyListObject);

    if (LambdaP(argv[0]) == 0)
        RaiseExceptionC(Assertion, "generate-pass", "generate-pass: expected a lambda",
                List(argv[0]));

    return(GPassLambda(AsLambda(argv[0])));
}

Define("keyword", KeywordPrimitive)(int argc, FObject argv[])
{
    FObject env = Bedrock;

    if (argc != 1)
        RaiseExceptionC(Assertion, "keyword", "keyword: expected one argument", EmptyListObject);

    if (SymbolP(argv[0]) == 0)
        RaiseExceptionC(Assertion, "keyword", "keyword: expected a symbol", List(argv[0]));

    FObject val = EnvironmentGet(env, argv[0]);
    if (SpecialSyntaxP(val) == 0)
        RaiseExceptionC(Assertion, "keyword", "keyword: symbol is not bound to special syntax",
                List(argv[0]));

    return(val);
}

Define("syntax", SyntaxPrimitive)(int argc, FObject argv[])
{
    FObject env = Bedrock;

    if (argc != 1)
        RaiseExceptionC(Assertion, "syntax", "syntax: expected one argument", EmptyListObject);

    return(ExpandExpression(MakeSyntacticEnv(Bedrock), DatumToSyntax(argv[0])));
}

Define("unsyntax", UnsyntaxPrimitive)(int argc, FObject argv[])
{
    FObject env = Bedrock;

    if (argc != 1)
        RaiseExceptionC(Assertion, "unsyntax", "unsyntax: expected one argument", EmptyListObject);

    return(SyntaxToDatum(argv[0]));
}

static FPrimitive * Primitives[] =
{
    &CompileEvalPrimitive,
    &InteractionEnvironmentPrimitive,
    &CompilePrimitive,
    &SyntaxPassPrimitive,
    &MiddlePassPrimitive,
    &GeneratePassPrimitive,
    &KeywordPrimitive,
    &SyntaxPrimitive,
    &UnsyntaxPrimitive
};

void SetupCompile()
{
    SyntacticEnvRecordType = MakeRecordTypeC("syntactic-env",
            sizeof(SyntacticEnvFieldsC) / sizeof(char *), SyntacticEnvFieldsC);
    BindingRecordType = MakeRecordTypeC("binding", sizeof(BindingFieldsC) / sizeof(char *),
            BindingFieldsC);
    ReferenceRecordType = MakeRecordTypeC("reference", sizeof(ReferenceFieldsC) / sizeof(char *),
            ReferenceFieldsC);
    LambdaRecordType = MakeRecordTypeC("lambda", sizeof(LambdaFieldsC) / sizeof(char *),
            LambdaFieldsC);
    CaseLambdaRecordType = MakeRecordTypeC("case-lambda",
            sizeof(CaseLambdaFieldsC) / sizeof(char *), CaseLambdaFieldsC);
    InlineVariableRecordType = MakeRecordTypeC("inline-variable",
            sizeof(InlineVariableFieldsC) / sizeof(char *), InlineVariableFieldsC);
    IdentifierRecordType = MakeRecordTypeC("identifier",
            sizeof(IdentifierFieldsC) / sizeof(char *), IdentifierFieldsC);

    ElseReference = MakeReference(Bedrock, MakeIdentifier(StringCToSymbol("else"), 0));
    ArrowReference = MakeReference(Bedrock, MakeIdentifier(StringCToSymbol("=>"), 0));
    LibraryReference = MakeReference(Bedrock, MakeIdentifier(StringCToSymbol("library"), 0));
    AndReference = MakeReference(Bedrock, MakeIdentifier(StringCToSymbol("and"), 0));
    OrReference = MakeReference(Bedrock, MakeIdentifier(StringCToSymbol("or"), 0));
    NotReference = MakeReference(Bedrock, MakeIdentifier(StringCToSymbol("not"), 0));
    QuasiquoteReference = MakeReference(Bedrock, MakeIdentifier(StringCToSymbol("quasiquote"), 0));
    UnquoteReference = MakeReference(Bedrock, MakeIdentifier(StringCToSymbol("unquote"), 0));
    UnquoteSplicingReference = MakeReference(Bedrock,
            MakeIdentifier(StringCToSymbol("unquote-splicing"), 0));
    ConsReference = MakeReference(Bedrock, MakeIdentifier(StringCToSymbol("cons"), 0));
    AppendReference = MakeReference(Bedrock, MakeIdentifier(StringCToSymbol("append"), 0));
    ListToVectorReference = MakeReference(Bedrock,
            MakeIdentifier(StringCToSymbol("list->vector"), 0));

    UnderscoreSymbol = StringCToSymbol("_");
    TagSymbol = StringCToSymbol("tag");

    InteractionEnv = NoValueObject;

    SetupSyntaxRules();

    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(Bedrock, BedrockLibrary, Primitives[idx]);
}
