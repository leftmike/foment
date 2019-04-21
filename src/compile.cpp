/*

Foment

*/

#include "foment.hpp"
#include "compile.hpp"

EternalSymbol(TagSymbol, "tag");
EternalSymbol(UsePassSymbol, "use-pass");
EternalSymbol(ConstantPassSymbol, "constant-pass");
EternalSymbol(AnalysisPassSymbol, "analysis-pass");

// ---- Roots ----

FObject ElseReference = NoValueObject;
FObject ArrowReference = NoValueObject;
FObject LibraryReference = NoValueObject;
FObject AndReference = NoValueObject;
FObject OrReference = NoValueObject;
FObject NotReference = NoValueObject;
FObject QuasiquoteReference = NoValueObject;
FObject UnquoteReference = NoValueObject;
FObject UnquoteSplicingReference = NoValueObject;
FObject ConsReference = NoValueObject;
FObject AppendReference = NoValueObject;
FObject ListToVectorReference = NoValueObject;
FObject EllipsisReference = NoValueObject;
FObject UnderscoreReference = NoValueObject;

static FObject InteractionEnv = NoValueObject;

// ---- SyntacticEnv ----

EternalBuiltinType(SyntacticEnvType, "syntactic-environment", 0);

FObject MakeSyntacticEnv(FObject obj)
{
    FAssert(EnvironmentP(obj) || SyntacticEnvP(obj));

    FSyntacticEnv * se = (FSyntacticEnv *) MakeBuiltin(SyntacticEnvType, 3,
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

    FBinding * b = (FBinding *) MakeBuiltin(BindingType, 11, "make-binding");
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

void WriteIdentifier(FWriteContext * wctx, FObject obj)
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

static long_t IdentifierMagic = 0;

FObject MakeIdentifier(FObject sym, FObject fn, long_t ln)
{
    FAssert(SymbolP(sym));

    FIdentifier * nid = (FIdentifier *) MakeObject(IdentifierTag, sizeof(FIdentifier), 4,
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

    FIdentifier * nid = (FIdentifier *) MakeObject(IdentifierTag, sizeof(FIdentifier), 4,
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
    long_t sl = FixnumAsString((long_t) obj, s, 16);

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

    FLambda * l = (FLambda *) MakeBuiltin(LambdaType, 16, "make-lambda");
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
    FCaseLambda * cl = (FCaseLambda *) MakeBuiltin(CaseLambdaType, 4, "make-case-lambda");
    cl->Cases = cases;
    cl->Name = NoValueObject;
    cl->Escapes = FalseObject;

    return(cl);
}

// ---- InlineVariable ----

EternalBuiltinType(InlineVariableType, "inline-variable", 0);

FObject MakeInlineVariable(long_t idx)
{
    FAssert(idx >= 0);

    FInlineVariable * iv = (FInlineVariable *) MakeBuiltin(InlineVariableType, 2,
            "make-inline-variable");
    iv->Index = MakeFixnum(idx);

    return(iv);
}

// ---- Reference ----

EternalBuiltinType(ReferenceType, "reference", 0);

FObject MakeReference(FObject be, FObject id)
{
    FAssert(BindingP(be) || EnvironmentP(be));
    FAssert(IdentifierP(id));

    FReference * r = (FReference *) MakeBuiltin(ReferenceType, 3, "make-reference");
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
    if (EnvironmentP(InteractionEnv) == 0)
    {
        InteractionEnv = MakeEnvironment(StringCToSymbol("interaction"), TrueObject);
        EnvironmentImportLibrary(InteractionEnv,
                List(StringCToSymbol("foment"), StringCToSymbol("base")));
    }

    return(InteractionEnv);
}

// ----------------

Define("%compile-eval", CompileEvalPrimitive)(long_t argc, FObject argv[])
{
    FMustBe(argc == 2);
    EnvironmentArgCheck("eval", argv[1]);

    return(CompileEval(argv[0], argv[1]));
}

Define("interaction-environment", InteractionEnvironmentPrimitive)(long_t argc, FObject argv[])
{
    if (argc == 0)
    {
        ZeroArgsCheck("interaction-environment", argc);

        return(GetInteractionEnv());
    }

    FObject env = MakeEnvironment(StringCToSymbol("interaction"), TrueObject);

    for (long_t adx = 0; adx < argc; adx++)
    {
        ListArgCheck("environment", argv[adx]);

        EnvironmentImportSet(env, argv[adx], argv[adx]);
    }

    return(env);
}

Define("environment", EnvironmentPrimitive)(long_t argc, FObject argv[])
{
    FObject env = MakeEnvironment(EmptyListObject, FalseObject);

    for (long_t adx = 0; adx < argc; adx++)
    {
        ListArgCheck("environment", argv[adx]);

        EnvironmentImportSet(env, argv[adx], argv[adx]);
    }

    EnvironmentImmutable(env);
    return(env);
}

Define("syntax", SyntaxPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("syntax", argc);

    return(ExpandExpression(NoValueObject, MakeSyntacticEnv(GetInteractionEnv()), argv[0]));
}

Define("unsyntax", UnsyntaxPrimitive)(long_t argc, FObject argv[])
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
    RegisterRoot(&ElseReference, "else-reference");
    RegisterRoot(&ArrowReference, "arrow-reference");
    RegisterRoot(&LibraryReference, "library-reference");
    RegisterRoot(&AndReference, "and-reference");
    RegisterRoot(&OrReference, "or-reference");
    RegisterRoot(&NotReference, "not-reference");
    RegisterRoot(&QuasiquoteReference, "quasiquote-reference");
    RegisterRoot(&UnquoteReference, "unquote-reference");
    RegisterRoot(&UnquoteSplicingReference, "unquote-splicing-reference");
    RegisterRoot(&ConsReference, "cons-reference");
    RegisterRoot(&AppendReference, "append-reference");
    RegisterRoot(&ListToVectorReference, "list-to-vector-reference");
    RegisterRoot(&EllipsisReference, "ellipsis-reference");
    RegisterRoot(&UnderscoreReference, "underscore-reference");
    RegisterRoot(&InteractionEnv, "interaction-env");

    ElseReference = MakeReference(Bedrock, MakeIdentifier(StringCToSymbol("else")));
    ArrowReference = MakeReference(Bedrock, MakeIdentifier(StringCToSymbol("=>")));
    LibraryReference = MakeReference(Bedrock, MakeIdentifier(StringCToSymbol("library")));
    AndReference = MakeReference(Bedrock, MakeIdentifier(StringCToSymbol("and")));
    OrReference = MakeReference(Bedrock, MakeIdentifier(StringCToSymbol("or")));
    NotReference = MakeReference(Bedrock, MakeIdentifier(StringCToSymbol("not")));
    QuasiquoteReference = MakeReference(Bedrock, MakeIdentifier(StringCToSymbol("quasiquote")));
    UnquoteReference = MakeReference(Bedrock, MakeIdentifier(StringCToSymbol("unquote")));
    UnquoteSplicingReference = MakeReference(Bedrock,
            MakeIdentifier(StringCToSymbol("unquote-splicing")));
    ConsReference = MakeReference(Bedrock, MakeIdentifier(StringCToSymbol("cons")));
    AppendReference = MakeReference(Bedrock, MakeIdentifier(StringCToSymbol("append")));
    ListToVectorReference = MakeReference(Bedrock, MakeIdentifier(StringCToSymbol("list->vector")));
    EllipsisReference = MakeReference(Bedrock, MakeIdentifier(StringCToSymbol("...")));
    UnderscoreReference = MakeReference(Bedrock, MakeIdentifier(StringCToSymbol("_")));

    TagSymbol = InternSymbol(TagSymbol);
    UsePassSymbol = InternSymbol(UsePassSymbol);
    ConstantPassSymbol = InternSymbol(ConstantPassSymbol);
    AnalysisPassSymbol = InternSymbol(AnalysisPassSymbol);

    FAssert(TagSymbol == StringCToSymbol("tag"));
    FAssert(UsePassSymbol == StringCToSymbol("use-pass"));
    FAssert(ConstantPassSymbol == StringCToSymbol("constant-pass"));
    FAssert(AnalysisPassSymbol == StringCToSymbol("analysis-pass"));
    FAssert(InteractionEnv == NoValueObject);

    for (ulong_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(Bedrock, BedrockLibrary, Primitives[idx]);
}
