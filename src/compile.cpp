/*

Foment

*/

#include "foment.hpp"
#include "compile.hpp"

// ---- SyntacticEnv ----

static char * SyntacticEnvFieldsC[] = {"global-bindings", "local-bindings"};

FObject MakeSyntacticEnv(FObject obj)
{
    FAssert(sizeof(FSyntacticEnv) == sizeof(SyntacticEnvFieldsC) + sizeof(FRecord));
    FAssert(EnvironmentP(obj) || SyntacticEnvP(obj));

    FSyntacticEnv * se = (FSyntacticEnv *) MakeRecord(R.SyntacticEnvRecordType);
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

    return(se);
}

// ---- Binding ----

static char * BindingFieldsC[] = {"identifier", "syntax", "syntactic-env", "rest-arg", "use-count",
    "set-count", "escapes", "level", "slot", "constant"};

FObject MakeBinding(FObject se, FObject id, FObject ra)
{
    FAssert(sizeof(FBinding) == sizeof(BindingFieldsC) + sizeof(FRecord));
    FAssert(SyntacticEnvP(se));
    FAssert(IdentifierP(id));
    FAssert(ra == TrueObject || ra == FalseObject);

    FBinding * b = (FBinding *) MakeRecord(R.BindingRecordType);
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

    return(b);
}

// ---- Identifier ----

static int IdentifierMagic = 0;

static char * IdentifierFieldsC[] = {"symbol", "line-number", "magic", "syntactic-env", "wrapped"};

FObject MakeIdentifier(FObject sym, int ln)
{
    FAssert(sizeof(FIdentifier) == sizeof(IdentifierFieldsC) + sizeof(FRecord));
    FAssert(SymbolP(sym));

    FIdentifier * i = (FIdentifier *) MakeRecord(R.IdentifierRecordType);
    i->Symbol = sym;
    i->LineNumber = MakeFixnum(ln);

    IdentifierMagic += 1;
    i->Magic = MakeFixnum(IdentifierMagic);

    i->SyntacticEnv = NoValueObject;
    i->Wrapped = NoValueObject;

    return(i);
}

FObject WrapIdentifier(FObject id, FObject se)
{
    FAssert(IdentifierP(id));

    FAssert(sizeof(FIdentifier) == sizeof(IdentifierFieldsC) + sizeof(FRecord));
    FAssert(SyntacticEnvP(se));

    FIdentifier * i = (FIdentifier *) MakeRecord(R.IdentifierRecordType);
    i->Symbol = AsIdentifier(id)->Symbol;
    i->LineNumber = AsIdentifier(id)->LineNumber;
    i->Magic = AsIdentifier(id)->Magic;
    i->SyntacticEnv = se;
    i->Wrapped = id;

    return(i);
}

// ---- Lambda ----

static char * LambdaFieldsC[] = {"name", "bindings", "body", "rest-arg", "arg-count",
    "escapes", "use-stack", "level", "slot-count", "middle-pass", "procedure", "body-index",
    "may-inline"};

FObject MakeLambda(FObject nam, FObject bs, FObject body)
{
    FAssert(sizeof(FLambda) == sizeof(LambdaFieldsC) + sizeof(FRecord));

    FLambda * l = (FLambda *) MakeRecord(R.LambdaRecordType);
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

    return(l);
}

// ---- CaseLambda ----

static char * CaseLambdaFieldsC[] = {"cases", "name", "escapes"};

FObject MakeCaseLambda(FObject cases)
{
    FAssert(sizeof(FCaseLambda) == sizeof(CaseLambdaFieldsC) + sizeof(FRecord));

    FCaseLambda * cl = (FCaseLambda *) MakeRecord(R.CaseLambdaRecordType);
    cl->Cases = cases;
    cl->Name = NoValueObject;
    cl->Escapes = FalseObject;

    return(cl);
}

// ---- InlineVariable ----

static char * InlineVariableFieldsC[] = {"index"};

FObject MakeInlineVariable(int idx)
{
    FAssert(sizeof(FInlineVariable) == sizeof(InlineVariableFieldsC) + sizeof(FRecord));

    FAssert(idx >= 0);

    FInlineVariable * iv = (FInlineVariable *) MakeRecord(R.InlineVariableRecordType);
    iv->Index = MakeFixnum(idx);

    return(iv);
}

// ---- Reference ----

static char * ReferenceFieldsC[] = {"binding", "identifier"};

FObject MakeReference(FObject be, FObject id)
{
    FAssert(sizeof(FReference) == sizeof(ReferenceFieldsC) + sizeof(FRecord));
    FAssert(BindingP(be) || EnvironmentP(be));
    FAssert(IdentifierP(id));

    FReference * r = (FReference *) MakeRecord(R.ReferenceRecordType);
    r->Binding = be;
    r->Identifier = id;

    return(r);
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
    if (EnvironmentP(R.InteractionEnv) == 0)
    {
        R.InteractionEnv = MakeEnvironment(List(StringCToSymbol("interaction")), TrueObject);
        EnvironmentImportLibrary(R.InteractionEnv,
                List(StringCToSymbol("scheme"), StringCToSymbol("base")));
    }

    return(R.InteractionEnv);
}

// ----------------

Define("compile-eval", CompileEvalPrimitive)(int argc, FObject argv[])
{
    if (argc != 2)
        RaiseExceptionC(R.Assertion, "compile-eval", "expected two arguments", EmptyListObject);

    if (EnvironmentP(argv[1]) == 0)
        RaiseExceptionC(R.Assertion, "compile-eval", "expected an environment", List(argv[1]));

    return(CompileEval(DatumToSyntax(argv[0]), argv[1]));
}

Define("interaction-environment", InteractionEnvironmentPrimitive)(int argc, FObject argv[])
{
    if (argc != 0)
        RaiseExceptionC(R.Assertion, "interaction-environment", "expected no arguments",
                EmptyListObject);

    return(GetInteractionEnv());
}

Define("compile", CompilePrimitive)(int argc, FObject argv[])
{
    if (argc < 2 || argc > 3)
        RaiseExceptionC(R.Assertion, "compile", "expected two or three arguments",
                EmptyListObject);

    if (argc == 2)
        return(CompileLambda(R.Bedrock, NoValueObject, argv[0], argv[1]));
    return(CompileLambda(R.Bedrock, argv[0], argv[1], argv[2]));
}

Define("syntax-pass", SyntaxPassPrimitive)(int argc, FObject argv[])
{
    if (argc < 2 || argc > 3)
        RaiseExceptionC(R.Assertion, "syntax-pass", "expected two or three arguments",
                EmptyListObject);

    FObject se = MakeSyntacticEnv(R.Bedrock);

    if (argc == 2)
        return(SPassLambda(se, NoValueObject, DatumToSyntax(argv[0]), DatumToSyntax(argv[1])));
    return(SPassLambda(se, DatumToSyntax(argv[0]), DatumToSyntax(argv[1]),
            DatumToSyntax(argv[2])));
}

Define("middle-pass", MiddlePassPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "middle-pass", "expected one argument", EmptyListObject);

    if (LambdaP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "middle-pass", "expected a lambda", List(argv[0]));

    MPassLambda(AsLambda(argv[0]));
    return(argv[0]);
}

Define("generate-pass", GeneratePassPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "generate-pass", "expected one argument", EmptyListObject);

    if (LambdaP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "generate-pass", "expected a lambda", List(argv[0]));

    return(GPassLambda(AsLambda(argv[0])));
}

Define("keyword", KeywordPrimitive)(int argc, FObject argv[])
{
    FObject env = R.Bedrock;

    if (argc != 1)
        RaiseExceptionC(R.Assertion, "keyword", "expected one argument", EmptyListObject);

    if (SymbolP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "keyword", "expected a symbol", List(argv[0]));

    FObject val = EnvironmentGet(env, argv[0]);
    if (SpecialSyntaxP(val) == 0)
        RaiseExceptionC(R.Assertion, "keyword", "symbol is not bound to special syntax",
                List(argv[0]));

    return(val);
}

Define("syntax", SyntaxPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "syntax", "expected one argument", EmptyListObject);

    return(ExpandExpression(MakeSyntacticEnv(GetInteractionEnv()), DatumToSyntax(argv[0])));
}

Define("unsyntax", UnsyntaxPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "unsyntax", "expected one argument", EmptyListObject);

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
    R.SyntacticEnvRecordType = MakeRecordTypeC("syntactic-env",
            sizeof(SyntacticEnvFieldsC) / sizeof(char *), SyntacticEnvFieldsC);
    R.BindingRecordType = MakeRecordTypeC("binding", sizeof(BindingFieldsC) / sizeof(char *),
            BindingFieldsC);
    R.ReferenceRecordType = MakeRecordTypeC("reference",
            sizeof(ReferenceFieldsC) / sizeof(char *), ReferenceFieldsC);
    R.LambdaRecordType = MakeRecordTypeC("lambda", sizeof(LambdaFieldsC) / sizeof(char *),
            LambdaFieldsC);
    R.CaseLambdaRecordType = MakeRecordTypeC("case-lambda",
            sizeof(CaseLambdaFieldsC) / sizeof(char *), CaseLambdaFieldsC);
    R.InlineVariableRecordType = MakeRecordTypeC("inline-variable",
            sizeof(InlineVariableFieldsC) / sizeof(char *), InlineVariableFieldsC);
    R.IdentifierRecordType = MakeRecordTypeC("identifier",
            sizeof(IdentifierFieldsC) / sizeof(char *), IdentifierFieldsC);

    R.ElseReference = MakeReference(R.Bedrock, MakeIdentifier(StringCToSymbol("else"), 0));
    R.ArrowReference = MakeReference(R.Bedrock, MakeIdentifier(StringCToSymbol("=>"), 0));
    R.LibraryReference = MakeReference(R.Bedrock,
            MakeIdentifier(StringCToSymbol("library"), 0));
    R.AndReference = MakeReference(R.Bedrock, MakeIdentifier(StringCToSymbol("and"), 0));
    R.OrReference = MakeReference(R.Bedrock, MakeIdentifier(StringCToSymbol("or"), 0));
    R.NotReference = MakeReference(R.Bedrock, MakeIdentifier(StringCToSymbol("not"), 0));
    R.QuasiquoteReference = MakeReference(R.Bedrock,
            MakeIdentifier(StringCToSymbol("quasiquote"), 0));
    R.UnquoteReference = MakeReference(R.Bedrock,
            MakeIdentifier(StringCToSymbol("unquote"), 0));
    R.UnquoteSplicingReference = MakeReference(R.Bedrock,
            MakeIdentifier(StringCToSymbol("unquote-splicing"), 0));
    R.ConsReference = MakeReference(R.Bedrock, MakeIdentifier(StringCToSymbol("cons"), 0));
    R.AppendReference = MakeReference(R.Bedrock,
            MakeIdentifier(StringCToSymbol("append"), 0));
    R.ListToVectorReference = MakeReference(R.Bedrock,
            MakeIdentifier(StringCToSymbol("list->vector"), 0));

    R.UnderscoreSymbol = StringCToSymbol("_");
    R.TagSymbol = StringCToSymbol("tag");
    R.InteractionEnv = NoValueObject;

    SetupSyntaxRules();

    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
