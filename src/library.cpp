/*

Foment

*/

#include "foment.hpp"
#include "compile.hpp"

// ---- Environments ----

static char * EnvironmentFieldsC[] = {"name", "hashtable", "interactive"};

FObject MakeEnvironment(FObject nam, FObject ctv)
{
    FAssert(sizeof(FEnvironment) == sizeof(EnvironmentFieldsC) + sizeof(FRecord));
    FAssert(BooleanP(ctv));

    FEnvironment * env = (FEnvironment *) MakeRecord(R.EnvironmentRecordType);
    env->Hashtable = MakeEqHashtable(67);
    env->Name = nam;
    env->Interactive = ctv;

    return(env);
}

static FObject MakeGlobal(FObject nam, FObject mod, FObject ctv);

// A global will always be returned.
FObject EnvironmentBind(FObject env, FObject sym)
{
    FAssert(EnvironmentP(env));
    FAssert(SymbolP(sym));

    FObject gl = EqHashtableRef(AsEnvironment(env)->Hashtable, sym, FalseObject);
    if (gl == FalseObject)
    {
        gl = MakeGlobal(sym, AsEnvironment(env)->Name, AsEnvironment(env)->Interactive);
        EqHashtableSet(AsEnvironment(env)->Hashtable, sym, gl);
    }

    FAssert(GlobalP(gl));
    return(gl);
}

// A global will be returned only if it has already be defined (or set).
FObject EnvironmentLookup(FObject env, FObject sym)
{
    FAssert(EnvironmentP(env));
    FAssert(SymbolP(sym));

    return(EqHashtableRef(AsEnvironment(env)->Hashtable, sym, FalseObject));
}

// If the environment is interactive, a global will be defined. Otherwise, a global will be
// defined only if it is not already defined. One will be returned if the global can't be defined.
int EnvironmentDefine(FObject env, FObject sym, FObject val)
{
    FAssert(EnvironmentP(env));
    FAssert(SymbolP(sym));

    FObject gl = EnvironmentBind(env, sym);

    FAssert(GlobalP(gl));

    if (AsEnvironment(env)->Interactive == FalseObject)
    {
        if (AsGlobal(gl)->State != GlobalUndefined)
            return(1);
    }
    else if (AsGlobal(gl)->State == GlobalImported
            || AsGlobal(gl)->State == GlobalImportedModified)
    {
        FAssert(BoxP(AsGlobal(gl)->Box));

//        AsGlobal(gl)->Box = MakeBox(NoValueObject);
        Modify(FGlobal, gl, Box, MakeBox(NoValueObject));
    }

    FAssert(BoxP(AsGlobal(gl)->Box));

//    AsBox(AsGlobal(gl)->Box)->Value = val;
    Modify(FBox, AsGlobal(gl)->Box, Value, val);
//    AsGlobal(gl)->State = GlobalDefined;
    Modify(FGlobal, gl, State, GlobalDefined);

    return(0);
}

// The value of a global is set; if it is not already defined it will be defined first.
FObject EnvironmentSet(FObject env, FObject sym, FObject val)
{
    FObject gl = EnvironmentBind(env, sym);

    FAssert(GlobalP(gl));
    FAssert(BoxP(AsGlobal(gl)->Box));

//    AsBox(AsGlobal(gl)->Box)->Value = val;
    Modify(FBox, AsGlobal(gl)->Box, Value, val);
    if (AsGlobal(gl)->State == GlobalUndefined)
    {
//        AsGlobal(gl)->State = GlobalDefined;
        Modify(FGlobal, gl, State, GlobalDefined);
    }
    else
    {
        FAssert(AsGlobal(gl)->State == GlobalDefined);

//        AsGlobal(gl)->State = GlobalModified;
        Modify(FGlobal, gl, State, GlobalModified);
    }

    return(gl);
}

FObject EnvironmentSetC(FObject env, char * sym, FObject val)
{
    return(EnvironmentSet(env, StringCToSymbol(sym), val));
}

FObject EnvironmentGet(FObject env, FObject sym)
{
    FObject gl = EqHashtableRef(AsEnvironment(env)->Hashtable, sym, FalseObject);
    if (GlobalP(gl))
    {
        FAssert(BoxP(AsGlobal(gl)->Box));

        return(Unbox(AsGlobal(gl)->Box));
    }

    return(NoValueObject);
}

static int EnvironmentImportGlobal(FObject env, FObject gl)
{
    FAssert(EnvironmentP(env));
    FAssert(GlobalP(gl));

    FObject ogl = EnvironmentLookup(env, AsGlobal(gl)->Name);
    if (GlobalP(ogl))
    {
        if (AsEnvironment(env)->Interactive == FalseObject)
            return(1);

//        AsGlobal(ogl)->Box = AsGlobal(gl)->Box;
        Modify(FGlobal, ogl, Box, AsGlobal(gl)->Box);
//        AsGlobal(ogl)->State = AsGlobal(gl)->State;
        Modify(FGlobal, ogl, State, AsGlobal(gl)->State);
    }
    else
       EqHashtableSet(AsEnvironment(env)->Hashtable, AsGlobal(gl)->Name, gl);

   return(0);
}

static FObject ImportGlobal(FObject env, FObject nam, FObject gl);
static FObject FindLibrary(FObject nam);
void EnvironmentImportLibrary(FObject env, FObject nam)
{
    FAssert(EnvironmentP(env));
    FAssert(AsEnvironment(env)->Interactive == TrueObject);

    FObject lib = FindLibrary(nam);
    FAssert(LibraryP(lib));

    FObject elst = AsLibrary(lib)->Exports;

    while (PairP(elst))
    {
        FAssert(PairP(First(elst)));
        FAssert(SymbolP(First(First(elst))));
        FAssert(GlobalP(Rest(First(elst))));

        int ret = EnvironmentImportGlobal(env,
                ImportGlobal(env, First(First(elst)), Rest(First(elst))));
        FAssert(ret == 0);

        elst = Rest(elst);
    }

    FAssert(elst == EmptyListObject);
}

// ---- Globals ----

static char * GlobalFieldsC[] = {"box", "name", "module", "state", "interactive"};

static FObject MakeGlobal(FObject nam, FObject mod, FObject ctv)
{
    FAssert(sizeof(FGlobal) == sizeof(GlobalFieldsC) + sizeof(FRecord));
    FAssert(SymbolP(nam));

    FGlobal * gl = (FGlobal *) MakeRecord(R.GlobalRecordType);
    gl->Box = MakeBox(NoValueObject);
    gl->Name = nam;
    gl->Module = mod;
    gl->State = GlobalUndefined;
    gl->Interactive = ctv;

    return(gl);
}

static FObject ImportGlobal(FObject env, FObject nam, FObject gl)
{
    FAssert(sizeof(FGlobal) == sizeof(GlobalFieldsC) + sizeof(FRecord));
    FAssert(EnvironmentP(env));
    FAssert(SymbolP(nam));
    FAssert(GlobalP(gl));

    FGlobal * ngl = (FGlobal *) MakeRecord(R.GlobalRecordType);
    ngl->Box = AsGlobal(gl)->Box;
    ngl->Name = nam;
    ngl->Module =  AsEnvironment(env)->Interactive == TrueObject ? env : AsGlobal(gl)->Module;
    if (AsGlobal(gl)->State == GlobalDefined || AsGlobal(gl)->State == GlobalImported)
        ngl->State = GlobalImported;
    else
    {
        FAssert(AsGlobal(gl)->State == GlobalModified
                || AsGlobal(gl)->State == GlobalImportedModified);

        ngl->State = GlobalImportedModified;
    }

    return(ngl);
}

// ---- Libraries ----

static char * LibraryFieldsC[] = {"name", "exports"};

static FObject MakeLibrary(FObject nam, FObject exports, FObject proc)
{
    FAssert(sizeof(FLibrary) == sizeof(LibraryFieldsC) + sizeof(FRecord));

    FLibrary * lib = (FLibrary *) MakeRecord(R.LibraryRecordType);
    lib->Name = nam;
    lib->Exports = exports;

    R.LoadedLibraries = MakePair(lib, R.LoadedLibraries);

    if (ProcedureP(proc))
        R.LibraryStartupList = MakePair(List(proc), R.LibraryStartupList);

    return(lib);
}

FObject MakeLibrary(FObject nam)
{
    return(MakeLibrary(nam, EmptyListObject, NoValueObject));
}

static void LibraryExportByName(FObject lib, FObject gl, FObject nam)
{
    FAssert(LibraryP(lib));
    FAssert(GlobalP(gl));
    FAssert(SymbolP(nam));
    FAssert(GlobalP(Assq(nam, AsLibrary(lib)->Exports)) == 0);

//    AsLibrary(lib)->Exports = MakePair(MakePair(nam, gl), AsLibrary(lib)->Exports);
    Modify(FLibrary, lib, Exports, MakePair(MakePair(nam, gl), AsLibrary(lib)->Exports));
}

void LibraryExport(FObject lib, FObject gl)
{
    FAssert(GlobalP(gl));

    LibraryExportByName(lib, gl, AsGlobal(gl)->Name);
}

static FObject FindLibrary(FObject nam)
{
    FObject ll = R.LoadedLibraries;

    while (PairP(ll))
    {
        FAssert(LibraryP(First(ll)));

        if (EqualP(AsLibrary(First(ll))->Name, nam))
            return(First(ll));

        ll = Rest(ll);
    }

    FAssert(ll == EmptyListObject);

    return(NoValueObject);
}

// ----------------

// (<name1> <name2> ... <namen>) --> <dir>/<name1>-<name2>-...<namen>.scm
static FObject LibraryNameFlat(FObject dir, FObject nam)
{
    FObject out = MakeStringOutputPort();
    WriteSimple(out, dir, 1);
    WriteCh(out, PathCh);

    while (PairP(nam))
    {
        FAssert(SymbolP(First(nam)));

        WriteSimple(out, First(nam), 1);

        nam = Rest(nam);
        if (nam != EmptyListObject)
            WriteCh(out, '-');
    }

    FAssert(nam == EmptyListObject);

    WriteStringC(out, ".scm");

    return(GetOutputString(out));
}

// (<name1> <name2> ... <namen>) --> <dir>\<name1>\<name2>\...\<namen>.scm
static FObject LibraryNameDeep(FObject dir, FObject nam)
{
    FObject out = MakeStringOutputPort();
    WriteSimple(out, dir, 1);

    while (PairP(nam))
    {
        FAssert(SymbolP(First(nam)));

        WriteCh(out, PathCh);
        WriteSimple(out, First(nam), 1);

        nam = Rest(nam);
    }

    FAssert(nam == EmptyListObject);

    WriteStringC(out, ".scm");

    return(GetOutputString(out));
}

static FObject LoadLibrary(FObject nam)
{
    FObject lp = R.LibraryPath;

    while (PairP(lp))
    {
        FAssert(StringP(First(lp)));

        FObject port = OpenInputFile(LibraryNameFlat(First(lp), nam));
        if (TextualPortP(port) == 0)
            port = OpenInputFile(LibraryNameDeep(First(lp), nam));

        if (TextualPortP(port))
        {
            for (;;)
            {
                FObject obj = Read(port, 1, 0);

                if (obj == EndOfFileObject)
                    break;

                if (PairP(obj) == 0 || IdentifierP(First(obj)) == 0 || DefineLibrarySyntax !=
                        EnvironmentGet(R.Bedrock, AsIdentifier(First(obj))->Symbol))
                    RaiseExceptionC(R.Syntax, "define-library", "expected a library", List(obj));

                CompileLibrary(obj);
            }
        }

        FObject lib = FindLibrary(nam);
        if (LibraryP(lib))
            return(lib);

        lp = Rest(lp);
    }

    FAssert(lp == EmptyListObject);

    return(NoValueObject);
}

FObject FindOrLoadLibrary(FObject nam)
{
    FObject lib = FindLibrary(nam);

    if (LibraryP(lib))
        return(lib);

    return(LoadLibrary(nam));
}

// ----------------

FObject LibraryName(FObject lst)
{
    FObject nlst = EmptyListObject;

    while (PairP(lst))
    {
        if (IdentifierP(First(lst)) == 0 && FixnumP(First(lst)) == 0)
            return(NoValueObject);

        nlst = MakePair(IdentifierP(First(lst)) ? AsIdentifier(First(lst))->Symbol : First(lst),
                nlst);
        lst = Rest(lst);
    }

    if (lst != EmptyListObject)
        return(NoValueObject);

    return(ReverseListModify(nlst));
}

static int CheckForIdentifier(FObject nam, FObject ids)
{
    FAssert(SymbolP(nam));

    while (PairP(ids))
    {
        FAssert(IdentifierP(First(ids)));

        if (AsIdentifier(First(ids))->Symbol == nam)
            return(1);

        ids = Rest(ids);
    }

    FAssert(ids == EmptyListObject);

    return(0);
}

static FObject CheckForRename(FObject nam, FObject rlst)
{
    FAssert(SymbolP(nam));

    while (PairP(rlst))
    {
        FObject rnm = First(rlst);

        FAssert(PairP(rnm));
        FAssert(IdentifierP(First(rnm)));
        FAssert(PairP(Rest(rnm)));
        FAssert(IdentifierP(First(Rest(rnm))));

        if (nam == AsIdentifier(First(rnm))->Symbol)
            return(AsIdentifier(First(Rest(rnm)))->Symbol);

        rlst = Rest(rlst);
    }

    FAssert(rlst == EmptyListObject);

    return(NoValueObject);
}

static FObject DoImportSet(FObject env, FObject is, FObject form);
static FObject DoOnlyOrExcept(FObject env, FObject is, int cfif)
{
    if (PairP(Rest(is)) == 0)
        return(NoValueObject);

    FObject ilst = DoImportSet(env, First(Rest(is)), is);
    FObject ids = Rest(Rest(is));
    while (PairP(ids))
    {
        if (IdentifierP(First(ids)) == 0)
            return(NoValueObject);

        ids = Rest(ids);
    }

    if (ids != EmptyListObject)
        return(NoValueObject);

    FObject nlst = EmptyListObject;
    while (PairP(ilst))
    {
        FAssert(GlobalP(First(ilst)));

        if (CheckForIdentifier(AsGlobal(First(ilst))->Name, Rest(Rest(is))) == cfif)
            nlst = MakePair(First(ilst), nlst);

        ilst = Rest(ilst);
    }

    FAssert(ilst == EmptyListObject);

    return(nlst);
}

static FObject DoImportSet(FObject env, FObject is, FObject form)
{
    // <library-name>
    // (only <import-set> <identifier> ...)
    // (except <import-set> <identifier> ...)
    // (prefix <import-set> <identifier>)
    // (rename <import-set> (<identifier1> <identifier2>) ...)

    if (PairP(is) == 0 || IdentifierP(First(is)) == 0)
        RaiseExceptionC(R.Syntax, "import", "expected a list starting with an identifier",
                List(form, is));

    FObject op = EnvironmentGet(R.Bedrock, AsIdentifier(First(is))->Symbol);
    if (op == OnlySyntax)
    {
        FObject ilst = DoOnlyOrExcept(env, is, 1);
        if (ilst == NoValueObject)
            RaiseExceptionC(R.Syntax, "import",
                    "expected (only <import-set> <identifier> ...)", List(form, is));

        return(ilst);
    }
    else if (op == ExceptSyntax)
    {
        FObject ilst = DoOnlyOrExcept(env, is, 0);
        if (ilst == NoValueObject)
            RaiseExceptionC(R.Syntax, "import",
                    "expected (except <import-set> <identifier> ...)", List(form, is));

        return(ilst);
    }
    else if (op == PrefixSyntax)
    {
        if (PairP(Rest(is)) == 0 || PairP(Rest(Rest(is))) == 0
                || Rest(Rest(Rest(is))) != EmptyListObject
                || IdentifierP(First(Rest(Rest(is)))) == 0)
            RaiseExceptionC(R.Syntax, "import",
                    "expected (prefix <import-set> <identifier>)", List(form, is));

        FObject prfx = AsSymbol(AsIdentifier(First(Rest(Rest(is))))->Symbol)->String;
        FObject ilst = DoImportSet(env, First(Rest(is)), is);
        FObject lst = ilst;
        while (PairP(lst))
        {
            FAssert(GlobalP(First(lst)));

//            AsGlobal(First(lst))->Name = PrefixSymbol(prfx, AsGlobal(First(lst))->Name);
            Modify(FGlobal, First(lst), Name, PrefixSymbol(prfx, AsGlobal(First(lst))->Name));
            lst = Rest(lst);
        }

        FAssert(lst == EmptyListObject);

        return(ilst);
    }
    else if (op == RenameSyntax)
    {
        if (PairP(Rest(is)) == 0)
            RaiseExceptionC(R.Syntax, "import",
                    "expected (rename <import-set> (<identifier> <identifier>) ...)",
                    List(form, is));

        FObject ilst = DoImportSet(env, First(Rest(is)), is);
        FObject rlst = Rest(Rest(is));
        while (PairP(rlst))
        {
            FObject rnm = First(rlst);
            if (PairP(rnm) == 0 || PairP(Rest(rnm)) == 0 || Rest(Rest(rnm)) != EmptyListObject
                    || IdentifierP(First(rnm)) == 0 || IdentifierP(First(Rest(rnm))) == 0)
                RaiseExceptionC(R.Syntax, "import",
                        "expected (rename <import-set> (<identifier> <identifier>) ...)",
                        List(form, is));

                rlst = Rest(rlst);
        }

        if (rlst != EmptyListObject)
            RaiseExceptionC(R.Syntax, "import",
                    "expected (rename <import-set> (<identifier> <identifier>) ...)",
                    List(form, is));

        FObject lst = ilst;
        while (PairP(lst))
        {
            FAssert(GlobalP(First(lst)));

            FObject nm = CheckForRename(AsGlobal(First(lst))->Name, Rest(Rest(is)));
            if (SymbolP(nm))
            {
//                AsGlobal(First(lst))->Name = nm;
                Modify(FGlobal, First(lst), Name, nm);
            }

            lst = Rest(lst);
        }

        FAssert(lst == EmptyListObject);

        return(ilst);
    }

    FObject nam = LibraryName(is);
    if (PairP(nam) == 0)
        RaiseExceptionC(R.Syntax, "import",
                "library name must be a list of symbols and/or integers", List(is));

    FObject lib = FindOrLoadLibrary(nam);

    if (LibraryP(lib) == 0)
        RaiseExceptionC(R.Syntax, "import", "library not found", List(form, nam));

    FObject ilst = EmptyListObject;
    FObject elst = AsLibrary(lib)->Exports;

    while (PairP(elst))
    {
        FAssert(PairP(First(elst)));
        FAssert(SymbolP(First(First(elst))));
        FAssert(GlobalP(Rest(First(elst))));

        ilst = MakePair(ImportGlobal(env, First(First(elst)), Rest(First(elst))), ilst);
        elst = Rest(elst);
    }

    FAssert(elst == EmptyListObject);

    return(ilst);
}

static void DoImport(FObject env, FObject is, FObject form)
{
    // (import <import-set> ...)

    FObject ilst = DoImportSet(env, is, form);

    while (PairP(ilst))
    {
        FAssert(GlobalP(First(ilst)));

        if (EnvironmentImportGlobal(env, First(ilst)))
            RaiseExceptionC(R.Syntax, "import", "expected an undefined identifier",
                    List(form, AsGlobal(First(ilst))->Name));

        ilst = Rest(ilst);
    }

    FAssert(ilst == EmptyListObject);
}

void EnvironmentImport(FObject env, FObject form)
{
    FAssert(PairP(form));

    FObject islst = Rest(form);
    while (PairP(islst))
    {
        DoImport(env, First(islst), form);
        islst = Rest(islst);
    }

    FAssert(islst == EmptyListObject);
}

// ----------------

static FObject ExpandLibraryDeclarations(FObject env, FObject lst, FObject body)
{
    while (PairP(lst))
    {
        if (PairP(First(lst)) == 0 || IdentifierP(First(First(lst))) == 0)
            RaiseExceptionC(R.Syntax, "define-library",
                    "expected a library declaration", List(First(lst)));

        FObject form = First(lst);
        FObject op = EnvironmentGet(R.Bedrock, AsIdentifier(First(form))->Symbol);

        if (op == ImportSyntax)
            EnvironmentImport(env, form);
        else if (op == IncludeLibraryDeclarationsSyntax)
            body = ExpandLibraryDeclarations(env, ReadInclude(Rest(form), 0), body);
        else if (op == CondExpandSyntax)
            body = ExpandLibraryDeclarations(env,
                    CondExpand(MakeSyntacticEnv(R.Bedrock), form, Rest(form)), body);
        else if (op != ExportSyntax && op != BeginSyntax && op != IncludeSyntax
                && op != IncludeCISyntax)
            RaiseExceptionC(R.Syntax, "define-library",
                    "expected a library declaration", List(First(lst)));
        else
            body = MakePair(form, body);

        lst = Rest(lst);
    }

    if (lst != EmptyListObject)
        RaiseExceptionC(R.Syntax, "define-library",
                "expected a proper list of library declarations", List(lst));

    return(body);
}

static FObject CompileTransformer(FObject obj, FObject env)
{
    if (PairP(obj) == 0 || IdentifierP(First(obj)) == 0)
        return(NoValueObject);

    FObject op = EnvironmentGet(env, AsIdentifier(First(obj))->Symbol);
    if (op == SyntaxRulesSyntax)
        return(CompileSyntaxRules(MakeSyntacticEnv(env), obj));

    return(NoValueObject);
}

static FObject CompileEvalExpr(FObject obj, FObject env, FObject body);
static FObject CompileEvalBegin(FObject obj, FObject env, FObject body, FObject form, FObject ss)
{
    if (PairP(obj) == 0)
        RaiseException(R.Syntax, SpecialSyntaxToSymbol(ss),
                SpecialSyntaxMsgC(ss, "expected at least one expression"), List(form, obj));

    while (PairP(obj))
    {
        body = CompileEvalExpr(First(obj), env, body);

        obj = Rest(obj);
    }

    if (obj != EmptyListObject)
        RaiseException(R.Syntax, SpecialSyntaxToSymbol(ss),
                SpecialSyntaxMsgC(ss, "expected a proper list"), List(form, obj));

    return(body);
}

static FObject ResolvedGet(FObject env, FObject id)
{
    FAssert(IdentifierP(id));

    while (IdentifierP(AsIdentifier(id)->Wrapped))
    {
        env = AsSyntacticEnv(AsIdentifier(id)->SyntacticEnv)->GlobalBindings;
        id = AsIdentifier(id)->Wrapped;
    }

    FAssert(EnvironmentP(env));

    return(EnvironmentGet(env, AsIdentifier(id)->Symbol));
}

static FObject CompileEvalExpr(FObject obj, FObject env, FObject body)
{
    if (VectorP(obj))
        return(MakePair(SyntaxToDatum(obj), body));
    else if (PairP(obj) && IdentifierP(First(obj)))
    {
//        FObject op = EnvironmentGet(env, AsIdentifier(First(obj))->Symbol);
        FObject op = ResolvedGet(env, First(obj));

        if (op == DefineSyntax)
        {
            // (define <variable> <expression>)
            // (define <variable>)
            // (define (<variable> <formals>) <body>)
            // (define (<variable> . <formal>) <body>)

            if (PairP(Rest(obj)) == 0)
                RaiseExceptionC(R.Syntax, "define",
                        "expected a variable or list beginning with a variable", List(obj));

            if (IdentifierP(First(Rest(obj))))
            {
                if (Rest(Rest(obj)) == EmptyListObject)
                {
                    // (define <variable>)

                    if (EnvironmentDefine(env, AsIdentifier(First(Rest(obj)))->Symbol,
                            NoValueObject))
                        RaiseExceptionC(R.Syntax, "define",
                                "imported variables may not be redefined in libraries",
                                List(obj, First(Rest(obj))));

                    return(body);
                }

                // (define <variable> <expression>)

                if (PairP(Rest(Rest(obj))) == 0 || Rest(Rest(Rest(obj))) != EmptyListObject)
                    RaiseExceptionC(R.Syntax, "define",
                            "expected (define <variable> <expression>)", List(obj));

                if (EnvironmentDefine(env, AsIdentifier(First(Rest(obj)))->Symbol, NoValueObject))
                    RaiseExceptionC(R.Syntax, "define",
                            "imported variables may not be redefined in libraries",
                            List(obj, First(Rest(obj))));

                return(MakePair(List(SetBangSyntax, First(Rest(obj)),
                        First(Rest(Rest(obj)))), body));
            }
            else
            {
                // (define (<variable> <formals>) <body>)
                // (define (<variable> . <formal>) <body>)

                if (PairP(First(Rest(obj))) == 0 || IdentifierP(First(First(Rest(obj)))) == 0)
                    RaiseExceptionC(R.Syntax, "define",
                            "expected a list beginning with a variable", List(obj));

                if (EnvironmentDefine(env, AsIdentifier(First(First(Rest(obj))))->Symbol,
                        CompileLambda(env, First(First(Rest(obj))), Rest(First(Rest(obj))),
                        Rest(Rest(obj)))))
                    RaiseExceptionC(R.Syntax, "define",
                            "imported variables may not be redefined in libraries",
                            List(obj, First(First(Rest(obj)))));

                return(body);
            }
        }
        else if (op == DefineSyntaxSyntax)
        {
            // (define-syntax <keyword> <expression>)

            if (PairP(Rest(obj)) == 0 || PairP(Rest(Rest(obj))) == 0
                    || Rest(Rest(Rest(obj))) != EmptyListObject
                    || IdentifierP(First(Rest(obj))) == 0)
                RaiseExceptionC(R.Syntax, "define-syntax",
                        "expected (define-syntax <keyword> <transformer>)",
                        List(obj));

            FObject trans = CompileTransformer(First(Rest(Rest(obj))), env);
            if (SyntaxRulesP(trans) == 0)
                RaiseExceptionC(R.Syntax, "define-syntax",
                        "expected a transformer", List(obj, trans));

            if (EnvironmentDefine(env, AsIdentifier(First(Rest(obj)))->Symbol, trans))
                RaiseExceptionC(R.Syntax, "define",
                        "imported variables may not be redefined in libraries",
                        List(obj, First(Rest(obj))));

            return(body);
        }
        else if (op == ImportSyntax)
        {
            EnvironmentImport(env, obj);
            if (body != EmptyListObject)
                body = MakePair(List(R.NoValuePrimitiveObject), body);

            return(body);
        }
        else if (op == DefineLibrarySyntax)
        {
            CompileLibrary(obj);
            return(body);
        }
        else if (SyntaxRulesP(op))
            return(CompileEvalExpr(ExpandSyntaxRules(MakeSyntacticEnv(env), op, Rest(obj)), env,
                    body));
        else if (op == BeginSyntax)
            return(CompileEvalBegin(Rest(obj), env, body, obj, BeginSyntax));
        else if (op == IncludeSyntax || op == IncludeCISyntax)
            return(CompileEvalBegin(ReadInclude(Rest(obj), op == IncludeCISyntax), env, body, obj,
                    op));
        else if (op == CondExpandSyntax)
            return(CompileEvalBegin(CondExpand(MakeSyntacticEnv(env), obj, Rest(obj)), env, body,
                    obj, op));
    }

    return(MakePair(obj, body));
}

FObject CompileEval(FObject obj, FObject env)
{
    FAssert(EnvironmentP(env));

    FObject body = CompileEvalExpr(obj, env, EmptyListObject);

    body = ReverseListModify(body);
    if (R.LibraryStartupList != EmptyListObject)
    {
        body = MakePair(MakePair(BeginSyntax, ReverseListModify(R.LibraryStartupList)), body);
        R.LibraryStartupList = EmptyListObject;
    }
    else if (body == EmptyListObject)
        return(R.NoValuePrimitiveObject);

    return(CompileLambda(env, NoValueObject, EmptyListObject, body));
}

FObject Eval(FObject obj, FObject env)
{
    FObject proc = CompileEval(obj, env);
    if (proc == R.NoValuePrimitiveObject)
        return(NoValueObject);

    FAssert(ProcedureP(proc));

    return(ExecuteThunk(proc));
}

static FObject CompileLibraryCode(FObject env, FObject lst)
{
    FObject body = EmptyListObject;

    while (PairP(lst))
    {
        FAssert(PairP(First(lst)));
        FAssert(IdentifierP(First(First(lst))));

        FObject form = First(lst);
        FObject op = EnvironmentGet(R.Bedrock, AsIdentifier(First(form))->Symbol);

        if (op == BeginSyntax || op == IncludeSyntax || op == IncludeCISyntax)
            body = CompileEvalExpr(form, env, body);
        else
        {
            FAssert(op == ImportSyntax || op == IncludeLibraryDeclarationsSyntax
                    || op == CondExpandSyntax || op == ExportSyntax);
        }

        lst = Rest(lst);
    }

    FAssert(lst == EmptyListObject);

    if (body == EmptyListObject)
        return(NoValueObject);

    // Move (#<syntax: set!> <var> <expr>) to the front of the body;
    // there is one of these for each (define <var> <expr>) expression.

    FObject slst = EmptyListObject;
    FObject blst = EmptyListObject;

    while (PairP(body))
    {
        if (PairP(First(body)) && First(First(body)) == SetBangSyntax)
            slst = MakePair(First(body), slst);
        else
            blst = MakePair(First(body), blst);

        body = Rest(body);
    }

    FAssert(body == EmptyListObject);

    if (slst == EmptyListObject)
        body = blst;
    else
    {
        body = slst;

        FAssert(PairP(slst));

        while (Rest(slst) != EmptyListObject)
        {
            FAssert(PairP(slst));

            slst = Rest(slst);
        }

//        AsPair(slst)->Rest = blst;
        SetRest(slst, blst);
    }

    return(CompileLambda(env, NoValueObject, EmptyListObject, body));
}

static FObject CompileExports(FObject env, FObject lst)
{
    FObject elst = EmptyListObject;

    while (PairP(lst))
    {
        FAssert(PairP(First(lst)));
        FAssert(IdentifierP(First(First(lst))));

        FObject form = First(lst);
        FObject op = EnvironmentGet(R.Bedrock, AsIdentifier(First(form))->Symbol);

        if (op == ExportSyntax)
        {
            FObject especs = Rest(form);
            while (PairP(especs))
            {
                FObject spec = First(especs);
                if (IdentifierP(spec) == 0
                        && (PairP(spec) == 0 || IdentifierP(First(spec)) == 0
                        || PairP(Rest(spec)) == 0 || IdentifierP(First(Rest(spec))) == 0
                        || PairP(Rest(Rest(spec))) == 0
                        || IdentifierP(First(Rest(Rest(spec)))) == 0
                        || Rest(Rest(Rest(spec))) != EmptyListObject
                        || EnvironmentGet(R.Bedrock, AsIdentifier(First(spec))->Symbol)
                                != RenameSyntax))
                    RaiseExceptionC(R.Syntax, "export",
                            "expected an identifier or (rename <id1> <id2>)",
                            List(form, spec));

                FObject lid = spec;
                FObject eid = spec;

                if (PairP(spec))
                {
                    lid = First(Rest(spec));
                    eid = First(Rest(Rest(spec)));
                }

                FObject gl = EnvironmentLookup(env, AsIdentifier(lid)->Symbol);
                if (GlobalP(gl) == 0)
                    RaiseExceptionC(R.Syntax, "export", "identifier is undefined",
                            List(form, lid));

                if (GlobalP(Assq(AsIdentifier(eid)->Symbol, elst)))
                    RaiseExceptionC(R.Syntax, "export", "identifier already exported",
                            List(form, eid));

                elst = MakePair(MakePair(AsIdentifier(eid)->Symbol, gl), elst);
                especs = Rest(especs);
            }

            if (especs != EmptyListObject)
                RaiseExceptionC(R.Syntax, "export", "expected a proper list of exports",
                        List(form, especs));

        }
        else
        {
            FAssert(op == ImportSyntax || op == IncludeLibraryDeclarationsSyntax
                    || op == CondExpandSyntax || op == BeginSyntax || op == IncludeSyntax
                    || op == IncludeCISyntax);
        }

        lst = Rest(lst);
    }

    FAssert(lst == EmptyListObject);

    return(elst);
}

static void WalkVisit(FObject key, FObject val, FObject ctx)
{
    FAssert(GlobalP(val));

    if (AsGlobal(val)->State == GlobalUndefined)
        RaiseExceptionC(R.Syntax, "define-library", "identifier used but never defined",
                List(AsGlobal(val)->Name, ctx));
}

void CompileLibrary(FObject expr)
{
    if (PairP(Rest(expr)) == 0)
        RaiseExceptionC(R.Syntax, "define-library", "expected a library name",
                List(expr));

    FObject ln = LibraryName(First(Rest(expr)));
    if (PairP(ln) == 0)
        RaiseExceptionC(R.Syntax, "define-library",
                "library name must be a list of symbols and/or integers", List(First(Rest(expr))));

    FObject env = MakeEnvironment(ln, FalseObject);
    FObject body = ReverseListModify(ExpandLibraryDeclarations(env, Rest(Rest(expr)),
            EmptyListObject));

    FObject proc = CompileLibraryCode(env, body);
    FObject exports = CompileExports(env, body);

    HashtableWalkVisit(AsEnvironment(env)->Hashtable, WalkVisit, expr);
    MakeLibrary(ln, exports, proc);
}

void SetupLibrary()
{
    R.EnvironmentRecordType = MakeRecordTypeC("environment",
            sizeof(EnvironmentFieldsC) / sizeof(char *), EnvironmentFieldsC);
    R.GlobalRecordType = MakeRecordTypeC("global", sizeof(GlobalFieldsC) / sizeof(char *),
            GlobalFieldsC);
    R.LibraryRecordType = MakeRecordTypeC("library", sizeof(LibraryFieldsC) / sizeof(char *),
            LibraryFieldsC);

    R.LibraryStartupList = EmptyListObject;
}
