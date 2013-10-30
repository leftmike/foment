/*

Foment

*/

#include "foment.hpp"
#include "compile.hpp"

// ---- SyntacticEnv ----

static const char * SyntacticEnvFieldsC[] = {"global-bindings", "local-bindings"};

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

static const char * BindingFieldsC[] = {"identifier", "syntax", "syntactic-env", "rest-arg",
    "use-count", "set-count", "escapes", "level", "slot", "constant"};

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

static int_t IdentifierMagic = 0;

static const char * IdentifierFieldsC[] = {"symbol", "line-number", "magic", "syntactic-env",
    "wrapped"};

FObject MakeIdentifier(FObject sym, int_t ln)
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

static const char * LambdaFieldsC[] = {"name", "bindings", "body", "rest-arg", "arg-count",
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

static const char * CaseLambdaFieldsC[] = {"cases", "name", "escapes"};

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

static const char * InlineVariableFieldsC[] = {"index"};

FObject MakeInlineVariable(int_t idx)
{
    FAssert(sizeof(FInlineVariable) == sizeof(InlineVariableFieldsC) + sizeof(FRecord));

    FAssert(idx >= 0);

    FInlineVariable * iv = (FInlineVariable *) MakeRecord(R.InlineVariableRecordType);
    iv->Index = MakeFixnum(idx);

    return(iv);
}

// ---- Reference ----

static const char * ReferenceFieldsC[] = {"binding", "identifier"};

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
        R.InteractionEnv = MakeEnvironment(StringCToSymbol("interaction"), TrueObject);
        EnvironmentImportLibrary(R.InteractionEnv,
                List(StringCToSymbol("foment"), StringCToSymbol("base")));
    }

    return(R.InteractionEnv);
}

// ----------------

Define("%compile-eval", CompileEvalPrimitive)(int_t argc, FObject argv[])
{
    FMustBe(argc == 2);
    EnvironmentArgCheck("eval", argv[1]);

    return(CompileEval(argv[0], argv[1]));
}

Define("interaction-environment", InteractionEnvironmentPrimitive)(int_t argc, FObject argv[])
{
    if (argc == 0)
    {
        ZeroArgsCheck("interaction-environment", argc);

        return(GetInteractionEnv());
    }

    FObject env = MakeEnvironment(StringCToSymbol("interaction"), TrueObject);

    for (int_t adx = 0; adx < argc; adx++)
    {
        ListArgCheck("environment", argv[adx]);

        EnvironmentImportSet(env, argv[adx], argv[adx]);
    }

    return(env);
}

Define("environment", EnvironmentPrimitive)(int_t argc, FObject argv[])
{
    FObject env = MakeEnvironment(EmptyListObject, FalseObject);

    for (int_t adx = 0; adx < argc; adx++)
    {
        ListArgCheck("environment", argv[adx]);

        EnvironmentImportSet(env, argv[adx], argv[adx]);
    }

    EnvironmentImmutable(env);
    return(env);
}

Define("syntax", SyntaxPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("syntax", argc);

    return(ExpandExpression(MakeSyntacticEnv(GetInteractionEnv()), argv[0]));
}

Define("unsyntax", UnsyntaxPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("unsyntax", argc);

    return(SyntaxToDatum(argv[0]));
}

static FPrimitive * Primitives[] =
{
    &CompileEvalPrimitive,
    &InteractionEnvironmentPrimitive,
    &EnvironmentPrimitive,
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

    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
