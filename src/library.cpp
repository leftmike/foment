/*

Foment

*/

#include "foment.hpp"
#include "compile.hpp"

EternalSymbol(DefineLibrarySymbol, "define-library");
EternalSymbol(ImportSymbol, "import");
EternalSymbol(IncludeLibraryDeclarationsSymbol, "include-library-declarations");
EternalSymbol(CondExpandSymbol, "cond-expand");
EternalSymbol(ExportSymbol, "export");
EternalSymbol(IncludeSymbol, "include");
EternalSymbol(IncludeCISymbol, "include-ci");
EternalSymbol(OnlySymbol, "only");
EternalSymbol(ExceptSymbol, "except");
EternalSymbol(PrefixSymbol, "prefix");
EternalSymbol(RenameSymbol, "rename");
EternalSymbol(AkaSymbol, "aka");

// ---- Roots ----

static FObject LibraryStartupList = EmptyListObject;

// ---- Environments ----

void WriteEnvironment(FWriteContext * wctx, FObject obj)
{
    FCh s[16];
    long_t sl = FixnumAsString((long_t) obj, s, 16);

    wctx->WriteStringC("#<environment: #x");
    wctx->WriteString(s, sl);

    wctx->WriteCh(' ');
    wctx->Write(AsEnvironment(obj)->Name);
    wctx->WriteStringC(">");
}

FObject MakeEnvironment(FObject nam, FObject ctv)
{
    FAssert(BooleanP(ctv));

    FEnvironment * env = (FEnvironment *) MakeObject(EnvironmentTag, sizeof(FEnvironment), 4,
            "make-environment");
    env->HashTable = MakeSymbolHashTable(512, HASH_TABLE_THREAD_SAFE);
    env->Name = nam;
    env->Interactive = ctv;
    env->Immutable = FalseObject;
    return(env);
}

void EnvironmentImmutable(FObject env)
{
    FAssert(EnvironmentP(env));

    AsEnvironment(env)->Immutable = TrueObject;
}

static FObject MakeGlobal(FObject nam, FObject mod, FObject ctv);

// A global will always be returned.
FObject EnvironmentBind(FObject env, FObject sym)
{
    FAssert(EnvironmentP(env));
    FAssert(SymbolP(sym));

    FObject gl = HashTableRef(AsEnvironment(env)->HashTable, sym, FalseObject);
    if (gl == FalseObject)
    {
        gl = MakeGlobal(sym, AsEnvironment(env)->Name, AsEnvironment(env)->Interactive);
        HashTableSet(AsEnvironment(env)->HashTable, sym, gl);
    }

    FAssert(GlobalP(gl));
    return(gl);
}

// A global will be returned only if it has already be defined (or set).
FObject EnvironmentLookup(FObject env, FObject sym)
{
    FAssert(EnvironmentP(env));
    FAssert(SymbolP(sym));

    return(HashTableRef(AsEnvironment(env)->HashTable, sym, FalseObject));
}

// If the environment is interactive, a global will be defined. Otherwise, a global will be
// defined only if it is not already defined. One will be returned if the global can't be defined.
long_t EnvironmentDefine(FObject env, FObject symid, FObject val)
{
    FAssert(EnvironmentP(env));
    FAssert(SymbolP(symid) || IdentifierP(symid));

    if (IdentifierP(symid))
        symid = AsIdentifier(symid)->Symbol;

    FObject gl = EnvironmentBind(env, symid);

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

        AsGlobal(gl)->Box = MakeBox(NoValueObject);
    }

    FAssert(BoxP(AsGlobal(gl)->Box));

    SetBox(AsGlobal(gl)->Box, val);
    AsGlobal(gl)->State = GlobalDefined;

    return(0);
}

// The value of a global is set; if it is not already defined it will be defined first.
FObject EnvironmentSet(FObject env, FObject sym, FObject val)
{
    FObject gl = EnvironmentBind(env, sym);

    FAssert(GlobalP(gl));
    FAssert(BoxP(AsGlobal(gl)->Box));

    SetBox(AsGlobal(gl)->Box, val);
    if (AsGlobal(gl)->State == GlobalUndefined)
        AsGlobal(gl)->State = GlobalDefined;
    else
    {
        FAssert(AsGlobal(gl)->State == GlobalDefined);

        AsGlobal(gl)->State = GlobalModified;
    }

    return(gl);
}

FObject EnvironmentSetC(FObject env, const char * sym, FObject val)
{
    return(EnvironmentSet(env, StringCToSymbol(sym), val));
}

FObject EnvironmentGet(FObject env, FObject symid)
{
    if (IdentifierP(symid))
        symid = AsIdentifier(symid)->Symbol;

    FAssert(SymbolP(symid));

    FObject gl = HashTableRef(AsEnvironment(env)->HashTable, symid, FalseObject);
    if (GlobalP(gl))
    {
        FAssert(BoxP(AsGlobal(gl)->Box));

        return(Unbox(AsGlobal(gl)->Box));
    }

    return(NoValueObject);
}

static long_t EnvironmentImportGlobal(FObject env, FObject gl)
{
    FAssert(EnvironmentP(env));
    FAssert(GlobalP(gl));

    FObject ogl = EnvironmentLookup(env, AsGlobal(gl)->Name);
    if (GlobalP(ogl))
    {
        if (AsGlobal(ogl)->Box == AsGlobal(gl)->Box)
            return(0);

        if (AsEnvironment(env)->Interactive == FalseObject)
            return(1);

        AsGlobal(ogl)->Box = AsGlobal(gl)->Box;
        AsGlobal(ogl)->State = AsGlobal(gl)->State;
    }
    else
       HashTableSet(AsEnvironment(env)->HashTable, AsGlobal(gl)->Name, gl);

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

#ifdef FOMENT_DEBUG
        long_t ret =
#endif // FOMENT_DEBUG
        EnvironmentImportGlobal(env,
                ImportGlobal(env, First(First(elst)), Rest(First(elst))));
        FAssert(ret == 0);

        elst = Rest(elst);
    }

    FAssert(elst == EmptyListObject);
}

// ---- Globals ----

void WriteGlobal(FWriteContext * wctx, FObject obj)
{
    FCh s[16];
    long_t sl = FixnumAsString((long_t) obj, s, 16);

    wctx->WriteStringC("#<global: #x");
    wctx->WriteString(s, sl);

    wctx->WriteCh(' ');
    wctx->Write(AsGlobal(obj)->Name);
    wctx->WriteCh(' ');
    wctx->Write(AsGlobal(obj)->Module);
    wctx->WriteStringC(">");
}

static FObject MakeGlobal(FObject nam, FObject mod, FObject ctv)
{
    FAssert(SymbolP(nam));

    FGlobal * gl = (FGlobal *) MakeObject(GlobalTag, sizeof(FGlobal), 5, "make-global");
    gl->Box = MakeBox(NoValueObject);
    gl->Name = nam;
    gl->Module = mod;
    gl->State = GlobalUndefined;
    gl->Interactive = ctv;

    return(gl);
}

static FObject ImportGlobal(FObject env, FObject nam, FObject gl)
{
    FAssert(EnvironmentP(env));
    FAssert(SymbolP(nam));
    FAssert(GlobalP(gl));

    FGlobal * ngl = (FGlobal *) MakeObject(GlobalTag, sizeof(FGlobal), 5, "import-global");
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
    ngl->Interactive = NoValueObject;

    return(ngl);
}

// ---- Libraries ----

void WriteLibrary(FWriteContext * wctx, FObject obj)
{
    FCh s[16];
    long_t sl = FixnumAsString((long_t) obj, s, 16);

    wctx->WriteStringC("#<library: #x");
    wctx->WriteString(s, sl);

    wctx->WriteCh(' ');
    wctx->Write(AsLibrary(obj)->Name);
    wctx->WriteStringC(">");
}

static FObject MakeLibrary(FObject nam, FObject exports, FObject proc)
{
    FLibrary * lib = (FLibrary *) MakeObject(LibraryTag, sizeof(FLibrary), 2, "make-library");
    lib->Name = nam;
    lib->Exports = exports;

    LoadedLibraries = MakePair(lib, LoadedLibraries);

    if (ProcedureP(proc))
        LibraryStartupList = MakePair(List(proc), LibraryStartupList);

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

    AsLibrary(lib)->Exports = MakePair(MakePair(nam, gl), AsLibrary(lib)->Exports);
}

void LibraryExport(FObject lib, FObject gl)
{
    FAssert(GlobalP(gl));

    LibraryExportByName(lib, gl, AsGlobal(gl)->Name);
}

static FObject FindLibrary(FObject nam)
{
    FObject ll = LoadedLibraries;

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

// (<name1> <name2> ... <namen>) --> <dir>/<name1>-<name2>-...<namen>.<ext>
static FObject LibraryNameFlat(FObject dir, FObject nam, FObject ext)
{
    FObject out = MakeStringOutputPort();
    WriteSimple(out, dir, 1);
    WriteCh(out, PathCh);

    while (PairP(nam))
    {
        FAssert(SymbolP(First(nam)) || IntegerP(First(nam)));

        WriteSimple(out, First(nam), 1);

        nam = Rest(nam);
        if (nam != EmptyListObject)
            WriteCh(out, '-');
    }

    FAssert(nam == EmptyListObject);

    WriteStringC(out, ".");
    WriteSimple(out, ext, 1);

    return(GetOutputString(out));
}

// (<name1> <name2> ... <namen>) --> <dir>\<name1>\<name2>\...\<namen>.<ext>
static FObject LibraryNameDeep(FObject dir, FObject nam, FObject ext)
{
    FObject out = MakeStringOutputPort();
    WriteSimple(out, dir, 1);

    while (PairP(nam))
    {
        FAssert(SymbolP(First(nam)) || IntegerP(First(nam)));

        WriteCh(out, PathCh);
        WriteSimple(out, First(nam), 1);

        nam = Rest(nam);
    }

    FAssert(nam == EmptyListObject);

    WriteStringC(out, ".");
    WriteSimple(out, ext, 1);

    return(GetOutputString(out));
}

static long_t EqualToSymbol(FObject obj, FObject sym)
{
    FAssert(SymbolP(sym));

    if (IdentifierP(obj))
        return(AsIdentifier(obj)->Symbol == sym);

    return(obj == sym);
}

static FObject TryLoadLibrary(FObject port, FObject nam, FObject libfn)
{
    if (TextualPortP(port))
    {
        WantIdentifiersPort(port, 1);

        for (;;)
        {
            FObject obj = Read(port);

            if (obj == EndOfFileObject)
                break;

            if (PairP(obj) == 0 || EqualToSymbol(First(obj), DefineLibrarySymbol) == 0)
                RaiseExceptionC(Syntax, "define-library", "expected a library",
                        List(libfn, obj));

            CompileLibrary(obj);
        }

        return(FindLibrary(nam));
    }

    return(NoValueObject);
}

static FObject LoadLibrary(FObject nam)
{
    FObject lib = TryLoadLibrary(OpenFomentLibrary(nam), nam, nam);
    if (LibraryP(lib))
        return(lib);

    FObject lp = LibraryPath;

    while (PairP(lp))
    {
        FAssert(StringP(First(lp)));

        FObject le = LibraryExtensions;

        while (PairP(le))
        {
            FAssert(StringP(First(le)));

            FObject libfn = LibraryNameFlat(First(lp), nam, First(le));
            FObject port = OpenInputFile(libfn);

            if (TextualPortP(port) == 0)
            {
                libfn = LibraryNameDeep(First(lp), nam, First(le));
                port = OpenInputFile(libfn);
            }

            FObject lib = TryLoadLibrary(port, nam, libfn);
            if (LibraryP(lib))
                return(lib);

            le = Rest(le);
        }

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
        if (IdentifierP(First(lst)) == 0 && FixnumP(First(lst)) == 0 && SymbolP(First(lst)) == 0)
            return(NoValueObject);

        nlst = MakePair(IdentifierP(First(lst)) ? AsIdentifier(First(lst))->Symbol : First(lst),
                nlst);
        lst = Rest(lst);
    }

    if (lst != EmptyListObject)
        return(NoValueObject);

    return(ReverseListModify(nlst));
}

static long_t CheckForIdentifier(FObject nam, FObject ids)
{
    FAssert(SymbolP(nam));

    while (PairP(ids))
    {
        if (SymbolP(First(ids)))
        {
            if (First(ids) == nam)
                return(1);
        }
        else
        {
            FAssert(IdentifierP(First(ids)));

            if (AsIdentifier(First(ids))->Symbol == nam)
                return(1);
        }

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
        FAssert(PairP(Rest(rnm)));

        if (SymbolP(First(rnm)))
        {
            FAssert(SymbolP(First(Rest(rnm))));

            if (nam == First(rnm))
                return(First(Rest(rnm)));
        }
        else
        {
            FAssert(IdentifierP(First(rnm)));
            FAssert(IdentifierP(First(Rest(rnm))));

            if (nam == AsIdentifier(First(rnm))->Symbol)
                return(AsIdentifier(First(Rest(rnm)))->Symbol);
        }

        rlst = Rest(rlst);
    }

    FAssert(rlst == EmptyListObject);

    return(NoValueObject);
}

static FObject DoImportSet(FObject env, FObject is, FObject form);
static FObject DoOnlyOrExcept(FObject env, FObject is, long_t cfif)
{
    if (PairP(Rest(is)) == 0)
        return(NoValueObject);

    FObject ilst = DoImportSet(env, First(Rest(is)), is);
    FObject ids = Rest(Rest(is));
    while (PairP(ids))
    {
        if (IdentifierP(First(ids)) == 0 && SymbolP(First(ids)) == 0)
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

    if (PairP(is) == 0 || (IdentifierP(First(is)) == 0 && SymbolP(First(is)) == 0))
        RaiseExceptionC(Syntax, "import", "expected a list starting with an identifier",
                List(is, form));

    if (EqualToSymbol(First(is), OnlySymbol))
    {
        FObject ilst = DoOnlyOrExcept(env, is, 1);
        if (ilst == NoValueObject)
            RaiseExceptionC(Syntax, "import",
                    "expected (only <import-set> <identifier> ...)", List(is, form));

        return(ilst);
    }
    else if (EqualToSymbol(First(is), ExceptSymbol))
    {
        FObject ilst = DoOnlyOrExcept(env, is, 0);
        if (ilst == NoValueObject)
            RaiseExceptionC(Syntax, "import",
                    "expected (except <import-set> <identifier> ...)", List(is, form));

        return(ilst);
    }
    else if (EqualToSymbol(First(is), PrefixSymbol))
    {
        if (PairP(Rest(is)) == 0 || PairP(Rest(Rest(is))) == 0
                || Rest(Rest(Rest(is))) != EmptyListObject ||
                (IdentifierP(First(Rest(Rest(is)))) == 0 && SymbolP(First(Rest(Rest(is)))) == 0))
            RaiseExceptionC(Syntax, "import",
                    "expected (prefix <import-set> <identifier>)", List(is, form));

        FObject prfx;
        if (SymbolP(First(Rest(Rest(is)))))
            prfx = SymbolToString(First(Rest(Rest(is))));
        else
            prfx = SymbolToString(AsIdentifier(First(Rest(Rest(is))))->Symbol);
        FObject ilst = DoImportSet(env, First(Rest(is)), is);
        FObject lst = ilst;
        while (PairP(lst))
        {
            FAssert(GlobalP(First(lst)));

            AsGlobal(First(lst))->Name = AddPrefixToSymbol(prfx, AsGlobal(First(lst))->Name);
            lst = Rest(lst);
        }

        FAssert(lst == EmptyListObject);

        return(ilst);
    }
    else if (EqualToSymbol(First(is), RenameSymbol))
    {
        if (PairP(Rest(is)) == 0)
            RaiseExceptionC(Syntax, "import",
                    "expected (rename <import-set> (<identifier> <identifier>) ...)",
                    List(is, form));

        FObject ilst = DoImportSet(env, First(Rest(is)), is);
        FObject rlst = Rest(Rest(is));
        while (PairP(rlst))
        {
            FObject rnm = First(rlst);
            if (PairP(rnm) == 0 || PairP(Rest(rnm)) == 0 || Rest(Rest(rnm)) != EmptyListObject
                    || (IdentifierP(First(rnm)) == 0 && SymbolP(First(rnm)) == 0)
                    || (IdentifierP(First(Rest(rnm))) == 0 && SymbolP(First(Rest(rnm))) == 0))
                RaiseExceptionC(Syntax, "import",
                        "expected (rename <import-set> (<identifier> <identifier>) ...)",
                        List(is, form));

            rlst = Rest(rlst);
        }

        if (rlst != EmptyListObject)
            RaiseExceptionC(Syntax, "import",
                    "expected (rename <import-set> (<identifier> <identifier>) ...)",
                    List(is, form));

        FObject lst = ilst;
        while (PairP(lst))
        {
            FAssert(GlobalP(First(lst)));

            FObject nm = CheckForRename(AsGlobal(First(lst))->Name, Rest(Rest(is)));
            if (SymbolP(nm))
                AsGlobal(First(lst))->Name = nm;

            lst = Rest(lst);
        }

        FAssert(lst == EmptyListObject);

        return(ilst);
    }

    FObject nam = LibraryName(is);
    if (PairP(nam) == 0)
        RaiseExceptionC(Syntax, "import",
                "library name must be a list of symbols and/or integers", List(is));

    FObject lib = FindOrLoadLibrary(nam);

    if (LibraryP(lib) == 0)
        RaiseExceptionC(Syntax, "import", "library not found", List(nam, form));

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

void EnvironmentImportSet(FObject env, FObject is, FObject form)
{
    FObject ilst = DoImportSet(env, is, form);

    while (PairP(ilst))
    {
        FAssert(GlobalP(First(ilst)));

        if (EnvironmentImportGlobal(env, First(ilst)))
            RaiseExceptionC(Syntax, "import", "expected an undefined identifier",
                    List(AsGlobal(First(ilst))->Name, form));

        ilst = Rest(ilst);
    }

    FAssert(ilst == EmptyListObject);
}

static void EnvironmentImport(FObject env, FObject form)
{
    // (import <import-set> ...)

    FAssert(PairP(form));

    FObject islst = Rest(form);
    while (PairP(islst))
    {
        EnvironmentImportSet(env, First(islst), form);
        islst = Rest(islst);
    }

    FAssert(islst == EmptyListObject);
}

// ----------------

static FObject ExpandLibraryDeclarations(FObject env, FObject lst, FObject body)
{
    while (PairP(lst))
    {
        if (PairP(First(lst)) == 0 || (IdentifierP(First(First(lst))) == 0
                && SymbolP(First(First(lst))) == 0))
            RaiseExceptionC(Syntax, "define-library",
                    "expected a library declaration", List(First(lst)));

        FObject form = First(lst);

        if (EqualToSymbol(First(form), ImportSymbol))
            EnvironmentImport(env, form);
        else if (EqualToSymbol(First(form), IncludeLibraryDeclarationsSymbol))
            body = ExpandLibraryDeclarations(env, ReadInclude(First(form), Rest(form), 0), body);
        else if (EqualToSymbol(First(form), CondExpandSymbol))
        {
            FObject ce = CondExpand(MakeSyntacticEnv(Bedrock), form, Rest(form));
            if (ce != EmptyListObject)
                body = ExpandLibraryDeclarations(env, ce , body);
        }
        else if (EqualToSymbol(First(form), ExportSymbol) == 0
                && EqualToSymbol(First(form), AkaSymbol) == 0
                && EqualToSymbol(First(form), BeginSymbol) == 0
                && EqualToSymbol(First(form), IncludeSymbol) == 0
                && EqualToSymbol(First(form), IncludeCISymbol) == 0)
            RaiseExceptionC(Syntax, "define-library",
                    "expected a library declaration", List(First(lst)));
        else
            body = MakePair(form, body);

        lst = Rest(lst);
    }

    if (lst != EmptyListObject)
        RaiseExceptionC(Syntax, "define-library",
                "expected a proper list of library declarations", List(lst));

    return(body);
}

static FObject CompileTransformer(FObject obj, FObject env)
{
    if (PairP(obj) == 0 || (IdentifierP(First(obj)) == 0 && SymbolP(First(obj)) == 0))
        return(NoValueObject);

    FObject op = EnvironmentGet(env, First(obj));
    if (SyntaxRulesP(op))
        return(CompileTransformer(ExpandSyntaxRules(MakeSyntacticEnv(env), op, Rest(obj)), env));
    if (op == SyntaxRulesSyntax)
        return(CompileSyntaxRules(MakeSyntacticEnv(env), obj));

    return(NoValueObject);
}

static FObject CompileEvalExpr(FObject obj, FObject env, FObject body);
static FObject CompileEvalBegin(FObject obj, FObject env, FObject body, FObject form, FObject ss)
{
    if (PairP(obj) == 0)
    {
        if (ss == BeginSyntax)
            return(body);

        RaiseExceptionC(Syntax, SpecialSyntaxToName(ss),
                "expected at least one expression", List(form));
    }

    while (PairP(obj))
    {
        body = CompileEvalExpr(First(obj), env, body);

        obj = Rest(obj);
    }

    if (obj != EmptyListObject)
        RaiseExceptionC(Syntax, SpecialSyntaxToName(ss),
                "expected a proper list", List(form));

    return(body);
}

static FObject ResolvedGet(FObject env, FObject symid)
{
    FAssert(IdentifierP(symid) || SymbolP(symid));

    if (IdentifierP(symid))
    {
        while (IdentifierP(AsIdentifier(symid)->Wrapped))
        {
            env = AsSyntacticEnv(AsIdentifier(symid)->SyntacticEnv)->GlobalBindings;
            symid = AsIdentifier(symid)->Wrapped;
        }
    }

    FAssert(EnvironmentP(env));

    return(EnvironmentGet(env, symid));
}

static FObject CompileEvalExpr(FObject obj, FObject env, FObject body)
{
    if (VectorP(obj))
        return(MakePair(SyntaxToDatum(obj), body));
    else if (PairP(obj) && (IdentifierP(First(obj)) || SymbolP(First(obj))))
    {
        if (EqualToSymbol(First(obj), DefineLibrarySymbol))
        {
            CompileLibrary(obj);
            return(body);
        }
        else if (EqualToSymbol(First(obj), ImportSymbol))
        {
            EnvironmentImport(env, obj);
            if (body != EmptyListObject)
                body = MakePair(List(NoValuePrimitive), body);

            return(body);
        }
//        FObject op = EnvironmentGet(env, First(obj));
        FObject op = ResolvedGet(env, First(obj));

        if (op == DefineSyntax)
        {
            // (define <variable> <expression>)
            // (define <variable>)
            // (define (<variable> <formals>) <body>)
            // (define (<variable> . <formal>) <body>)

            FAssert(EnvironmentP(env));

            if (AsEnvironment(env)->Immutable == TrueObject)
                RaiseExceptionC(Assertion, "define",
                        "environment is immutable", List(env, obj));

            if (PairP(Rest(obj)) == 0)
                RaiseExceptionC(Syntax, "define",
                        "expected a variable or list beginning with a variable", List(obj));

            if (IdentifierP(First(Rest(obj))) || SymbolP(First(Rest(obj))))
            {
                if (Rest(Rest(obj)) == EmptyListObject)
                {
                    // (define <variable>)

                    if (EnvironmentDefine(env, First(Rest(obj)), NoValueObject))
                        RaiseExceptionC(Syntax, "define",
                                "imported variables may not be redefined in libraries",
                                List(First(Rest(obj)), obj));

                    return(body);
                }

                // (define <variable> <expression>)

                if (PairP(Rest(Rest(obj))) == 0 || Rest(Rest(Rest(obj))) != EmptyListObject)
                    RaiseExceptionC(Syntax, "define",
                            "expected (define <variable> <expression>)", List(obj));

                if (EnvironmentDefine(env, First(Rest(obj)), NoValueObject))
                    RaiseExceptionC(Syntax, "define",
                            "imported variables may not be redefined in libraries",
                            List(First(Rest(obj)), obj));

                return(MakePair(List(SetBangSyntax, First(Rest(obj)),
                        First(Rest(Rest(obj)))), body));
            }
            else
            {
                // (define (<variable> <formals>) <body>)
                // (define (<variable> . <formal>) <body>)

                if (PairP(First(Rest(obj))) == 0 || (IdentifierP(First(First(Rest(obj)))) == 0
                        && SymbolP(First(First(Rest(obj)))) == 0))
                    RaiseExceptionC(Syntax, "define",
                            "expected a list beginning with a variable", List(obj));

                if (EnvironmentDefine(env, First(First(Rest(obj))),
                        CompileLambda(env, First(First(Rest(obj))), Rest(First(Rest(obj))),
                        Rest(Rest(obj)))))
                    RaiseExceptionC(Syntax, "define",
                            "imported variables may not be redefined in libraries",
                            List(First(First(Rest(obj))), obj));

                return(body);
            }
        }
        else if (op == DefineValuesSyntax)
        {
            // (define-values (<variable> ...) <expression>)

            FAssert(EnvironmentP(env));

            if (AsEnvironment(env)->Immutable == TrueObject)
                RaiseExceptionC(Assertion, "define-values",
                        "environment is immutable", List(env, obj));

            if (PairP(Rest(obj)) == 0
                    || (PairP(First(Rest(obj))) == 0 && First(Rest(obj)) != EmptyListObject)
                    || PairP(Rest(Rest(obj))) == 0 || Rest(Rest(Rest(obj))) != EmptyListObject)
                RaiseExceptionC(Syntax, "define-values",
                        "expected (define-values (<variable> ...) <expression>)", List(obj));

            FObject lst = First(Rest(obj));
            while (PairP(lst))
            {
                if (IdentifierP(First(lst)) == 0 && SymbolP(First(lst)) == 0)
                    RaiseExceptionC(Syntax, "define-values",
                            "expected (define-values (<variable> ...) <expression>)",
                            List(First(lst), obj));

                if (EnvironmentDefine(env, First(lst), NoValueObject))
                    RaiseExceptionC(Syntax, "define-values",
                            "imported variables may not be redefined in libraries",
                            List(First(lst), obj));

                lst = Rest(lst);
            }

            if (lst != EmptyListObject)
                RaiseExceptionC(Syntax, "define-values", "expected a list of variables",
                        List(obj));

            return(MakePair(List(SetBangValuesSyntax, First(Rest(obj)), First(Rest(Rest(obj)))),
                    body));
        }
        else if (op == DefineSyntaxSyntax)
        {
            // (define-syntax <keyword> <expression>)

            FAssert(EnvironmentP(env));

            if (AsEnvironment(env)->Immutable == TrueObject)
                RaiseExceptionC(Assertion, "define",
                        "environment is immutable", List(env, obj));

            if (PairP(Rest(obj)) == 0 || PairP(Rest(Rest(obj))) == 0
                    || Rest(Rest(Rest(obj))) != EmptyListObject
                    || (IdentifierP(First(Rest(obj))) == 0 && SymbolP(First(Rest(obj))) == 0))
                RaiseExceptionC(Syntax, "define-syntax",
                        "expected (define-syntax <keyword> <transformer>)",
                        List(obj));

            FObject trans = CompileTransformer(First(Rest(Rest(obj))), env);
            if (SyntaxRulesP(trans) == 0)
                RaiseExceptionC(Syntax, "define-syntax",
                        "expected a transformer", List(trans, obj));

            if (EnvironmentDefine(env, First(Rest(obj)), trans))
                RaiseExceptionC(Syntax, "define",
                        "imported variables may not be redefined in libraries",
                        List(First(Rest(obj)), obj));

            return(body);
        }
        else if (SyntaxRulesP(op))
            return(CompileEvalExpr(ExpandSyntaxRules(MakeSyntacticEnv(env), op, Rest(obj)), env,
                    body));
        else if (op == BeginSyntax)
            return(CompileEvalBegin(Rest(obj), env, body, obj, BeginSyntax));
        else if (op == IncludeSyntax || op == IncludeCISyntax)
            return(CompileEvalBegin(ReadInclude(First(obj), Rest(obj), op == IncludeCISyntax), env,
                    body, obj, op));
        else if (op == CondExpandSyntax)
        {
            FObject ce = CondExpand(MakeSyntacticEnv(env), obj, Rest(obj));
            if (ce == EmptyListObject)
                return(body);
            return(CompileEvalBegin(ce, env, body, obj, op));
        }
    }

    return(MakePair(obj, body));
}

FObject CompileEval(FObject obj, FObject env)
{
    FAssert(EnvironmentP(env));

    FObject body = CompileEvalExpr(obj, env, EmptyListObject);

    body = ReverseListModify(body);
    if (LibraryStartupList != EmptyListObject)
    {
        body = MakePair(MakePair(BeginSyntax, ReverseListModify(LibraryStartupList)), body);
        LibraryStartupList = EmptyListObject;
    }
    else if (body == EmptyListObject)
        return(NoValuePrimitive);

    return(CompileLambda(env, NoValueObject, EmptyListObject, body));
}

FObject Eval(FObject obj, FObject env)
{
    FObject proc = CompileEval(obj, env);
    if (proc == NoValuePrimitive)
        return(NoValueObject);

    FAssert(ProcedureP(proc));

    return(ExecuteProc(proc));
}

static FObject CompileLibraryCode(FObject env, FObject lst)
{
    FObject body = EmptyListObject;

    while (PairP(lst))
    {
        FAssert(PairP(First(lst)));
        FAssert(IdentifierP(First(First(lst))) || SymbolP(First(First(lst))));

        FObject form = First(lst);

        if (EqualToSymbol(First(form), BeginSymbol))
            body = CompileEvalBegin(Rest(form), env, body, form, BeginSyntax);
        else if (EqualToSymbol(First(form), IncludeSymbol))
            body = CompileEvalBegin(ReadInclude(First(form), Rest(form), 0), env, body, form,
                    IncludeSyntax);
        else if (EqualToSymbol(First(form), IncludeCISymbol))
            body = CompileEvalBegin(ReadInclude(First(form), Rest(form), 1), env, body, form,
                    IncludeCISyntax);
        else
        {
            FAssert(EqualToSymbol(First(form), ImportSymbol)
                    || EqualToSymbol(First(form), IncludeLibraryDeclarationsSymbol)
                    || EqualToSymbol(First(form), CondExpandSymbol)
                    || EqualToSymbol(First(form), ExportSymbol)
                    || EqualToSymbol(First(form), AkaSymbol));
        }

        lst = Rest(lst);
    }

    FAssert(lst == EmptyListObject);

    if (body == EmptyListObject)
        return(NoValueObject);

    return(CompileLambda(env, NoValueObject, EmptyListObject, ReverseListModify(body)));
}

static FObject CompileExports(FObject env, FObject lst)
{
    FObject elst = EmptyListObject;

    while (PairP(lst))
    {
        FAssert(PairP(First(lst)));
        FAssert(IdentifierP(First(First(lst))) || SymbolP(First(First(lst))));

        FObject form = First(lst);

        if (EqualToSymbol(First(form), ExportSymbol))
        {
            FObject especs = Rest(form);
            while (PairP(especs))
            {
                FObject spec = First(especs);
                if (IdentifierP(spec) == 0 && SymbolP(spec) == 0
                        && (PairP(spec) == 0
                        || PairP(Rest(spec)) == 0
                        || (IdentifierP(First(Rest(spec))) == 0 && SymbolP(First(Rest(spec))) == 0)
                        || PairP(Rest(Rest(spec))) == 0
                        || (IdentifierP(First(Rest(Rest(spec)))) == 0
                        && SymbolP(First(Rest(Rest(spec)))) == 0)
                        || Rest(Rest(Rest(spec))) != EmptyListObject
                        || EqualToSymbol(First(spec), RenameSymbol) == 0))
                    RaiseExceptionC(Syntax, "export",
                            "expected an identifier or (rename <id1> <id2>)",
                            List(spec, form));

                FObject lid = spec;
                FObject eid = spec;

                if (PairP(spec))
                {
                    lid = First(Rest(spec));
                    eid = First(Rest(Rest(spec)));
                }

                if (IdentifierP(lid))
                    lid = AsIdentifier(lid)->Symbol;
                if (IdentifierP(eid))
                    eid = AsIdentifier(eid)->Symbol;

                FObject gl = EnvironmentLookup(env, lid);
                if (GlobalP(gl) == 0)
                    RaiseExceptionC(Syntax, "export", "identifier is undefined",
                            List(lid, form));

                if (GlobalP(Assq(eid, elst)))
                    RaiseExceptionC(Syntax, "export", "identifier already exported",
                            List(eid, form));

                elst = MakePair(MakePair(eid, gl), elst);
                especs = Rest(especs);
            }

            if (especs != EmptyListObject)
                RaiseExceptionC(Syntax, "export", "expected a proper list of exports",
                        List(form));
        }
        else
        {
            FAssert(EqualToSymbol(First(form), ImportSymbol)
                    || EqualToSymbol(First(form), AkaSymbol)
                    || EqualToSymbol(First(form), IncludeLibraryDeclarationsSymbol)
                    || EqualToSymbol(First(form), CondExpandSymbol)
                    || EqualToSymbol(First(form), BeginSymbol)
                    || EqualToSymbol(First(form), IncludeSymbol)
                    || EqualToSymbol(First(form), IncludeCISymbol));
        }

        lst = Rest(lst);
    }

    FAssert(lst == EmptyListObject);

    return(elst);
}

static FObject CompileAkas(FObject env, FObject lst)
{
    FObject akalst = EmptyListObject;

    while (PairP(lst))
    {
        FAssert(PairP(First(lst)));
        FAssert(IdentifierP(First(First(lst))) || SymbolP(First(First(lst))));

        FObject form = First(lst);

        if (EqualToSymbol(First(form), AkaSymbol))
        {
            if (PairP(Rest(form)) == 0 || Rest(Rest(form)) != EmptyListObject)
                RaiseExceptionC(Syntax, "aka", "expected (aka <library-name>)", List(form));

            FObject ln = LibraryName(First(Rest(form)));
            if (PairP(ln) == 0)
                RaiseExceptionC(Syntax, "aka",
                        "library name must be a list of symbols and/or integers",
                        List(First(Rest(form))));

            akalst = MakePair(ln, akalst);
        }
        else
        {
            FAssert(EqualToSymbol(First(form), ImportSymbol)
                    || EqualToSymbol(First(form), ExportSymbol)
                    || EqualToSymbol(First(form), IncludeLibraryDeclarationsSymbol)
                    || EqualToSymbol(First(form), CondExpandSymbol)
                    || EqualToSymbol(First(form), BeginSymbol)
                    || EqualToSymbol(First(form), IncludeSymbol)
                    || EqualToSymbol(First(form), IncludeCISymbol));
        }

        lst = Rest(lst);
    }

    FAssert(lst == EmptyListObject);

    return(akalst);
}

static FObject Fold(FObject key, FObject val, void * ctx, FObject lst)
{
    FAssert(GlobalP(val));

    if (AsGlobal(val)->State == GlobalUndefined)
        return(MakePair(AsGlobal(val)->Name, lst));

    return(lst);
}

void CompileLibrary(FObject expr)
{
    if (PairP(Rest(expr)) == 0)
        RaiseExceptionC(Syntax, "define-library", "expected a library name",
                List(expr));

    FObject ln = LibraryName(First(Rest(expr)));
    if (PairP(ln) == 0)
        RaiseExceptionC(Syntax, "define-library",
                "library name must be a list of symbols and/or integers", List(First(Rest(expr))));

    FObject env = MakeEnvironment(ln, FalseObject);
    FObject body = ReverseListModify(ExpandLibraryDeclarations(env, Rest(Rest(expr)),
            EmptyListObject));

    FObject proc = CompileLibraryCode(env, body);
    FObject exports = CompileExports(env, body);
    FObject akalst = CompileAkas(env, body);

    FObject lst = HashTableFold(AsEnvironment(env)->HashTable, Fold, 0, EmptyListObject);
    if (lst != EmptyListObject)
        RaiseExceptionC(Syntax, "define-library", "identifier(s) used but never defined",
                List(lst, expr));

    FObject lib = MakeLibrary(ln, exports, proc);

    while (akalst != EmptyListObject)
    {
        FAssert(PairP(akalst));
        FAssert(LibraryP(lib));

        MakeLibrary(First(akalst), AsLibrary(lib)->Exports, NoValueObject);
        akalst = Rest(akalst);
    }
}

static FObject CondExpandProgram(FObject lst, FObject prog)
{
    while (lst != EmptyListObject)
    {
        FAssert(PairP(lst));

        FObject obj = First(lst);

        if (PairP(obj) && EqualToSymbol(First(obj), CondExpandSymbol))
        {
            FObject ce = CondExpand(MakeSyntacticEnv(Bedrock), obj, Rest(obj));
            if (ce != EmptyListObject)
                prog = CondExpandProgram(ce, prog);
        }
        else
            prog = MakePair(obj, prog);

        lst = Rest(lst);
    }

    return(prog);
}

extern FObject ExitPrimitive;
FObject CompileProgram(FObject nam, FObject port, int mf)
{
    FAssert(TextualPortP(port) && InputPortOpenP(port));

    WantIdentifiersPort(port, 1);

    FObject env = MakeEnvironment(nam, FalseObject);
    FObject prog = EmptyListObject;
    FObject body = EmptyListObject;

    try
    {
        for (;;)
        {
            FObject obj = Read(port);
            if (obj == EndOfFileObject)
                break;

            if (PairP(obj) && EqualToSymbol(First(obj), CondExpandSymbol))
            {
                FObject ce = CondExpand(MakeSyntacticEnv(Bedrock), obj, Rest(obj));
                if (ce != EmptyListObject)
                    prog = CondExpandProgram(ce, prog);
            }
            else
                prog = MakePair(obj, prog);
        }

        prog = ReverseListModify(prog);

        while (prog != EmptyListObject)
        {
            FAssert(PairP(prog));

            FObject obj = First(prog);
            if (PairP(obj) == 0)
                break;

            if (EqualToSymbol(First(obj), ImportSymbol))
                EnvironmentImport(env, obj);
            else if (EqualToSymbol(First(obj), DefineLibrarySymbol))
                CompileLibrary(obj);
            else
                break;

            prog = Rest(prog);
        }

        if (LibraryStartupList != EmptyListObject)
        {
            body = MakePair(MakePair(BeginSyntax, ReverseListModify(LibraryStartupList)), body);
            LibraryStartupList = EmptyListObject;
        }

        while (prog != EmptyListObject)
        {
            FAssert(PairP(prog));

            body = CompileEvalExpr(First(prog), env, body);
            prog = Rest(prog);
        }
    }
    catch (FObject obj)
    {
        if (ExceptionP(obj) == 0)
            WriteStringC(StandardOutput, "exception: ");
        Write(StandardOutput, obj, 0);
        WriteCh(StandardOutput, '\n');
    }

    if (mf)
    {
        FObject main = HashTableRef(AsEnvironment(env)->HashTable, StringCToSymbol("main"),
                NoValueObject);
        if (GlobalP(main) && AsGlobal(main)->State == GlobalDefined &&
                ProcedureP(Unbox(AsGlobal(main)->Box)))
        {
            // (%exit (main <command-line>))
            body = MakePair(List(ExitPrimitive,
                    List(StringCToSymbol("main"), List(QuoteSymbol, CommandLine))), body);
        }
    }

    FObject proc = CompileLambda(env, NoValueObject, EmptyListObject, ReverseListModify(body));

    FObject lst = HashTableFold(AsEnvironment(env)->HashTable, Fold, 0, EmptyListObject);
    if (lst != EmptyListObject)
        RaiseExceptionC(Syntax, "program", "identifier(s) used but never defined",
                List(lst, nam));

    return(proc);
}

void SetupLibrary()
{
    RegisterRoot(&LibraryStartupList, "library-startup-list");

    DefineLibrarySymbol = InternSymbol(DefineLibrarySymbol);
    ImportSymbol = InternSymbol(ImportSymbol);
    IncludeLibraryDeclarationsSymbol = InternSymbol(IncludeLibraryDeclarationsSymbol);
    CondExpandSymbol = InternSymbol(CondExpandSymbol);
    ExportSymbol = InternSymbol(ExportSymbol);
    IncludeSymbol = InternSymbol(IncludeSymbol);
    IncludeCISymbol = InternSymbol(IncludeCISymbol);
    OnlySymbol = InternSymbol(OnlySymbol);
    ExceptSymbol = InternSymbol(ExceptSymbol);
    PrefixSymbol = InternSymbol(PrefixSymbol);
    RenameSymbol = InternSymbol(RenameSymbol);
    AkaSymbol = InternSymbol(AkaSymbol);

    FAssert(DefineLibrarySymbol == StringCToSymbol("define-library"));
    FAssert(ImportSymbol == StringCToSymbol("import"));
    FAssert(IncludeLibraryDeclarationsSymbol == StringCToSymbol("include-library-declarations"));
    FAssert(CondExpandSymbol == StringCToSymbol("cond-expand"));
    FAssert(ExportSymbol == StringCToSymbol("export"));
    FAssert(IncludeSymbol == StringCToSymbol("include"));
    FAssert(IncludeCISymbol == StringCToSymbol("include-ci"));
    FAssert(OnlySymbol == StringCToSymbol("only"));
    FAssert(ExceptSymbol == StringCToSymbol("except"));
    FAssert(PrefixSymbol == StringCToSymbol("prefix"));
    FAssert(RenameSymbol == StringCToSymbol("rename"));
    FAssert(AkaSymbol == StringCToSymbol("aka"));
}
