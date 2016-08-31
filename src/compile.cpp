/*

Foment

*/

#include "foment.hpp"
#include "compile.hpp"

EternalSymbol(TagSymbol, "tag");
EternalSymbol(UsePassSymbol, "use-pass");
EternalSymbol(ConstantPassSymbol, "constant-pass");
EternalSymbol(AnalysisPassSymbol, "analysis-pass");

// ---- SyntacticEnv ----

EternalBuiltinType(SyntacticEnvType, "syntactic-environment", 0);

FObject MakeSyntacticEnv(FObject obj)
{
    FAssert(EnvironmentP(obj) || SyntacticEnvP(obj));

    FSyntacticEnv * se = (FSyntacticEnv *) MakeBuiltin(SyntacticEnvType, sizeof(FSyntacticEnv), 3,
            "make-syntactic-environment");
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

EternalBuiltinType(BindingType, "binding", 0);

FObject MakeBinding(FObject se, FObject id, FObject ra)
{
    FAssert(SyntacticEnvP(se));
    FAssert(IdentifierP(id));
    FAssert(ra == TrueObject || ra == FalseObject);

    FBinding * b = (FBinding *) MakeBuiltin(BindingType, sizeof(FBinding), 11, "make-binding");
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

static void
WriteIdentifier(FWriteContext * wctx, FObject obj)
{
    obj = AsIdentifier(obj)->Symbol;

    FAssert(SymbolP(obj));

    if (StringP(AsSymbol(obj)->String))
        wctx->WriteString(AsString(AsSymbol(obj)->String)->String,
                StringLength(AsSymbol(obj)->String));
    else
    {
        FAssert(CStringP(AsSymbol(obj)->String));

        wctx->WriteStringC(AsCString(AsSymbol(obj)->String)->String);
    }
}

EternalBuiltinType(IdentifierType, "identifier", WriteIdentifier);

static int_t IdentifierMagic = 0;

FObject MakeIdentifier(FObject sym, FObject fn, int_t ln)
{
    FAssert(SymbolP(sym));

    FIdentifier * nid = (FIdentifier *) MakeBuiltin(IdentifierType, sizeof(FIdentifier), 5,
            "%make-identifier");
    nid->Symbol = sym;
    nid->Filename = fn;
    nid->SyntacticEnv = NoValueObject;
    nid->Wrapped = NoValueObject;

    nid->LineNumber = ln;
    IdentifierMagic += 1;
    nid->Magic = IdentifierMagic;

    return(nid);
}

FObject MakeIdentifier(FObject sym)
{
    return(MakeIdentifier(sym, NoValueObject, 0));
}

FObject WrapIdentifier(FObject id, FObject se)
{
    FAssert(IdentifierP(id));
    FAssert(SyntacticEnvP(se));

    FIdentifier * nid = (FIdentifier *) MakeBuiltin(IdentifierType, sizeof(FIdentifier), 5,
            "%wrap-identifier");
    nid->Symbol = AsIdentifier(id)->Symbol;
    nid->Filename = AsIdentifier(id)->Filename;
    nid->SyntacticEnv = se;
    nid->Wrapped = id;
    nid->LineNumber = AsIdentifier(id)->LineNumber;
    nid->Magic = AsIdentifier(id)->Magic;

    return(nid);
}

// ---- Lambda ----


static void
WriteLambda(FWriteContext * wctx, FObject obj)
{
    FCh s[16];
    int_t sl = FixnumAsString((FFixnum) obj, s, 16);

    wctx->WriteStringC("#<library: #x");
    wctx->WriteString(s, sl);

    wctx->WriteCh(' ');
    wctx->Write(AsLambda(obj)->Name);
    wctx->WriteCh(' ');
    wctx->Write(AsLambda(obj)->Bindings);
    if (StringP(AsLambda(obj)->Filename) && FixnumP(AsLambda(obj)->LineNumber))
    {
        wctx->WriteCh(' ');
        wctx->Display(AsLambda(obj)->Filename);
        wctx->WriteCh('[');
        wctx->Display(AsLambda(obj)->LineNumber);
        wctx->WriteCh(']');
    }
    wctx->WriteStringC(">");
}

EternalBuiltinType(LambdaType, "lambda", WriteLambda);

FObject MakeLambda(FObject enc, FObject nam, FObject bs, FObject body)
{
    FAssert(LambdaP(enc) || enc == NoValueObject);

    FLambda * l = (FLambda *) MakeBuiltin(LambdaType, sizeof(FLambda), 16, "make-lambda");
    l->Name = nam;
    l->Bindings = bs;
    l->Body = body;

    l->RestArg = FalseObject;
    l->ArgCount = MakeFixnum(0);

    l->Escapes = FalseObject;
    l->UseStack = TrueObject;
    l->Level = LambdaP(enc) ? MakeFixnum(AsFixnum(AsLambda(enc)->Level) + 1) : MakeFixnum(1);
    l->SlotCount = MakeFixnum(-1);
    l->CompilerPass = NoValueObject;
    l->MayInline = MakeFixnum(0); // Number of procedure calls or FalseObject.

    l->Procedure = NoValueObject;
    l->BodyIndex = NoValueObject;

    if (IdentifierP(nam))
    {
        l->Filename = AsIdentifier(nam)->Filename;
        l->LineNumber = MakeFixnum(AsIdentifier(nam)->LineNumber);
    }
    else
    {
        l->Filename = NoValueObject;
        l->LineNumber = NoValueObject;
    }

    return(l);
}

// ---- CaseLambda ----

EternalBuiltinType(CaseLambdaType, "case-lambda", 0);

FObject MakeCaseLambda(FObject cases)
{
    FCaseLambda * cl = (FCaseLambda *) MakeBuiltin(CaseLambdaType, sizeof(FCaseLambda), 4,
            "make-case-lambda");
    cl->Cases = cases;
    cl->Name = NoValueObject;
    cl->Escapes = FalseObject;

    return(cl);
}

// ---- InlineVariable ----

EternalBuiltinType(InlineVariableType, "inline-variable", 0);

FObject MakeInlineVariable(int_t idx)
{
    FAssert(idx >= 0);

    FInlineVariable * iv = (FInlineVariable *) MakeBuiltin(InlineVariableType,
            sizeof(FInlineVariable), 2, "make-inline-variable");
    iv->Index = MakeFixnum(idx);

    return(iv);
}

// ---- Reference ----

EternalBuiltinType(ReferenceType, "reference", 0);

FObject MakeReference(FObject be, FObject id)
{
    FAssert(BindingP(be) || EnvironmentP(be));
    FAssert(IdentifierP(id));

    FReference * r = (FReference *) MakeBuiltin(ReferenceType, sizeof(FReference), 3,
            "make-reference");
    r->Binding = be;
    r->Identifier = id;

    return(r);
}

// ----------------

FObject CompileLambda(FObject env, FObject name, FObject formals, FObject body)
{
    FObject obj = SPassLambda(NoValueObject, MakeSyntacticEnv(env), name, formals, body);
    FAssert(LambdaP(obj));

    UPassLambda(AsLambda(obj), 1);
    CPassLambda(AsLambda(obj));
    APassLambda(0, AsLambda(obj));
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

    return(ExpandExpression(NoValueObject, MakeSyntacticEnv(GetInteractionEnv()), argv[0]));
}

Define("unsyntax", UnsyntaxPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("unsyntax", argc);

    return(SyntaxToDatum(argv[0]));
}

static FObject Primitives[] =
{
    CompileEvalPrimitive,
    InteractionEnvironmentPrimitive,
    EnvironmentPrimitive,
    SyntaxPrimitive,
    UnsyntaxPrimitive
};

void SetupCompile()
{
    R.ElseReference = MakeReference(R.Bedrock, MakeIdentifier(StringCToSymbol("else")));
    R.ArrowReference = MakeReference(R.Bedrock, MakeIdentifier(StringCToSymbol("=>")));
    R.LibraryReference = MakeReference(R.Bedrock,
            MakeIdentifier(StringCToSymbol("library")));
    R.AndReference = MakeReference(R.Bedrock, MakeIdentifier(StringCToSymbol("and")));
    R.OrReference = MakeReference(R.Bedrock, MakeIdentifier(StringCToSymbol("or")));
    R.NotReference = MakeReference(R.Bedrock, MakeIdentifier(StringCToSymbol("not")));
    R.QuasiquoteReference = MakeReference(R.Bedrock,
            MakeIdentifier(StringCToSymbol("quasiquote")));
    R.UnquoteReference = MakeReference(R.Bedrock,
            MakeIdentifier(StringCToSymbol("unquote")));
    R.UnquoteSplicingReference = MakeReference(R.Bedrock,
            MakeIdentifier(StringCToSymbol("unquote-splicing")));
    R.ConsReference = MakeReference(R.Bedrock, MakeIdentifier(StringCToSymbol("cons")));
    R.AppendReference = MakeReference(R.Bedrock,
            MakeIdentifier(StringCToSymbol("append")));
    R.ListToVectorReference = MakeReference(R.Bedrock,
            MakeIdentifier(StringCToSymbol("list->vector")));
    R.EllipsisReference = MakeReference(R.Bedrock, MakeIdentifier(StringCToSymbol("...")));
    R.UnderscoreReference = MakeReference(R.Bedrock, MakeIdentifier(StringCToSymbol("_")));

    TagSymbol = InternSymbol(TagSymbol);
    UsePassSymbol = InternSymbol(UsePassSymbol);
    ConstantPassSymbol = InternSymbol(ConstantPassSymbol);
    AnalysisPassSymbol = InternSymbol(AnalysisPassSymbol);

    FAssert(TagSymbol == StringCToSymbol("tag"));
    FAssert(UsePassSymbol == StringCToSymbol("use-pass"));
    FAssert(ConstantPassSymbol == StringCToSymbol("constant-pass"));
    FAssert(AnalysisPassSymbol == StringCToSymbol("analysis-pass"));
    FAssert(R.InteractionEnv == NoValueObject);

    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
