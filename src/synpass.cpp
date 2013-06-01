/*

Foment

*/

#include "foment.hpp"
#include "compile.hpp"

// ---- Syntax Pass ----

static FObject EnterScope(FObject bd)
{
    FAssert(BindingP(bd));
    FAssert(SyntacticEnvP(AsBinding(bd)->SyntacticEnv));

    AsSyntacticEnv(AsBinding(bd)->SyntacticEnv)->LocalBindings = MakePair(bd,
            AsSyntacticEnv(AsBinding(bd)->SyntacticEnv)->LocalBindings);

    return(bd);
}

static void EnterScopeList(FObject bs)
{
    while (bs != EmptyListObject)
    {
        FAssert(PairP(bs));
        FAssert(BindingP(First(bs)));

        EnterScope(First(bs));
        bs = Rest(bs);
    }
}

static void LeaveScope(FObject bd)
{
    FAssert(BindingP(bd));
    FAssert(SyntacticEnvP(AsBinding(bd)->SyntacticEnv));
    FAssert(PairP(AsSyntacticEnv(AsBinding(bd)->SyntacticEnv)->LocalBindings));
    FAssert(First(AsSyntacticEnv(AsBinding(bd)->SyntacticEnv)->LocalBindings) == bd);

    AsSyntacticEnv(AsBinding(bd)->SyntacticEnv)->LocalBindings =
            Rest(AsSyntacticEnv(AsBinding(bd)->SyntacticEnv)->LocalBindings);
}

FObject ResolveIdentifier(FObject se, FObject id)
{
    FAssert(IdentifierP(id));
    FAssert(SyntacticEnvP(se));

    FObject lb = AsSyntacticEnv(se)->LocalBindings;

    while (lb != EmptyListObject)
    {
        FAssert(PairP(lb));
        FAssert(BindingP(First(lb)));

        if (AsIdentifier(AsBinding(First(lb))->Identifier)->Symbol == AsIdentifier(id)->Symbol)
            return(First(lb));

        lb = Rest(lb);
    }

    return(AsSyntacticEnv(se)->GlobalBindings);
}

FObject SyntaxToDatum(FObject obj)
{
    if (IdentifierP(obj))
        return(AsIdentifier(obj)->Symbol);

    if (BindingP(obj))
    {
        FAssert(IdentifierP(AsBinding(obj)->Identifier));

        return(AsIdentifier(AsBinding(obj)->Identifier)->Symbol);
    }

    if (ReferenceP(obj))
    {
        FAssert(IdentifierP(AsReference(obj)->Identifier));

        return(AsIdentifier(AsReference(obj)->Identifier)->Symbol);
    }

    if (LambdaP(obj))
        return(MakePair(LambdaSyntax, MakePair(SyntaxToDatum(AsLambda(obj)->Bindings),
                SyntaxToDatum(AsLambda(obj)->Body))));

    if (PairP(obj))
        return(MakePair(SyntaxToDatum(First(obj)), SyntaxToDatum(Rest(obj))));

    if (VectorP(obj))
    {
        FObject vec = MakeVector(VectorLen(obj), 0, FalseObject);
        for (int idx = 0; idx < VectorLen(vec); idx++)
            AsVector(vec)->Vector[idx] = SyntaxToDatum(AsVector(obj)->Vector[idx]);

        return(vec);
    }

    return(obj);
}

FObject DatumToSyntax(FObject obj)
{
    if (SymbolP(obj))
        return(MakeIdentifier(obj, -1));

    if (PairP(obj))
        return(MakePair(DatumToSyntax(First(obj)), DatumToSyntax(Rest(obj))));

    if (VectorP(obj))
    {
        FObject vec = MakeVector(VectorLen(obj), 0, FalseObject);
        for (int idx = 0; idx < VectorLen(vec); idx++)
            AsVector(vec)->Vector[idx] = DatumToSyntax(AsVector(obj)->Vector[idx]);

        return(vec);
    }

    return(obj);
}

static int SyntaxP(FObject obj)
{
    return(SpecialSyntaxP(obj) || SyntaxRulesP(obj));
}

static int SyntaxBindingP(FObject be, FObject var)
{
    if (BindingP(be))
    {
        if (SyntaxP(AsBinding(be)->Syntax))
            return(1);
    }
    else
    {
        FAssert(EnvironmentP(be));

        if (SyntaxP(EnvironmentGet(be, AsIdentifier(var)->Symbol)))
            return(1);
    }

    return(0);
}

static FObject SPassExpression(FObject se, FObject expr);
static FObject SPassBody(FObject se, FObject ss, FObject body);
static FObject SPassSequenceLast(FObject se, FObject ss, FObject form, FObject seq, FObject last);
static FObject SPassSequence(FObject se, FObject ss, FObject form, FObject seq);

static FObject AddFormal(FObject se, FObject ss, FObject bs, FObject id, FObject form, FObject ra)
{
    if (IdentifierP(id) == 0)
        RaiseException(Syntax, SpecialSyntaxToSymbol(ss),
                SpecialSyntaxMsgC(ss, "expected a symbol or a list of symbols for formals"),
                List(form, id));

    if (SyntacticEnvP(AsIdentifier(id)->SyntacticEnv))
        se = AsIdentifier(id)->SyntacticEnv;

    if (bs == EmptyListObject)
        return(MakePair(MakeBinding(se, id, ra), EmptyListObject));

    FObject lst = bs;
    for (;;)
    {
        FAssert(PairP(lst));
        FAssert(BindingP(First(lst)));

        if (AsIdentifier(AsBinding(First(lst))->Identifier)->Symbol == AsIdentifier(id)->Symbol
                && AsBinding(First(lst))->SyntacticEnv == se)
            RaiseException(Syntax, SpecialSyntaxToSymbol(ss),
                    SpecialSyntaxMsgC(ss, "duplicate identifier in formals"),
                    List(First(lst), form));

        if (Rest(lst) == EmptyListObject)
            break;

        lst = Rest(lst);
    }

    FAssert(PairP(lst));
    AsPair(lst)->Rest = MakePair(MakeBinding(se, id, ra), EmptyListObject);

    return(bs);
}

static FObject SPassFormals(FObject se, FObject ss, FObject bs, FObject formals, FObject form)
{
    while (PairP(formals))
    {
        bs = AddFormal(se, ss, bs, First(formals), form, FalseObject);
        formals = Rest(formals);
    }

    if (formals != EmptyListObject)
        bs = AddFormal(se, ss, bs, formals, form, TrueObject);

    return(bs);
}

static void LeaveScopeList(FObject bs)
{
    if (bs != EmptyListObject)
    {
        FAssert(PairP(bs));
        FAssert(BindingP(First(bs)));

        LeaveScopeList(Rest(bs));
        LeaveScope(First(bs));
    }
}

static void LeaveLetScope(FObject lb)
{
    if (lb != EmptyListObject)
    {
        FAssert(PairP(lb));
        FAssert(PairP(First(lb)));

        LeaveLetScope(Rest(lb));

        if (BindingP(First(First(lb))))
            LeaveScope(First(First(lb)));
        else
            LeaveScopeList(First(First(lb)));
    }
}

static FObject AddBinding(FObject se, FObject ss, FObject bs, FObject id, FObject form)
{
    if (IdentifierP(id) == 0)
        RaiseException(Syntax, SpecialSyntaxToSymbol(ss),
                SpecialSyntaxMsgC(ss, "expected <variable> for each binding"), List(form, id));

    if (SyntacticEnvP(AsIdentifier(id)->SyntacticEnv))
        se = AsIdentifier(id)->SyntacticEnv;

    if (bs == EmptyListObject)
        return(MakePair(MakeBinding(se, id, FalseObject), EmptyListObject));

    FObject lst = bs;
    for (;;)
    {
        FAssert(PairP(lst));
        FAssert(BindingP(First(lst)));

        if (AsIdentifier(AsBinding(First(lst))->Identifier)->Symbol == AsIdentifier(id)->Symbol
                && AsBinding(First(lst))->SyntacticEnv == se)
            RaiseException(Syntax, SpecialSyntaxToSymbol(ss),
                    SpecialSyntaxMsgC(ss, "duplicate identifier in bindings"),
                    List(First(lst), form));

        if (Rest(lst) == EmptyListObject)
            break;

        lst = Rest(lst);
    }

    FAssert(PairP(lst));
    AsPair(lst)->Rest = MakePair(MakeBinding(se, id, FalseObject), EmptyListObject);

    return(bs);
}

static FObject SPassLetVar(FObject se, FObject ss, FObject lb, FObject bs, FObject vi, int vf)
{
    if (vf != 0)
    {
        // (<formals> <init>)

        if (PairP(vi) == 0 || PairP(Rest(vi)) == 0 || Rest(Rest(vi)) != EmptyListObject)
            RaiseException(Syntax, SpecialSyntaxToSymbol(ss),
                    SpecialSyntaxMsgC(ss, "expected (<formals> <init>) for each binding"),
                    List(lb, vi));

        return(SPassFormals(se, ss, bs, First(vi), lb));
    }

    // (<variable> <init>)
    // (<keyword> <transformer>)

    if (PairP(vi) == 0 || PairP(Rest(vi)) == 0 || Rest(Rest(vi)) != EmptyListObject)
        RaiseException(Syntax, SpecialSyntaxToSymbol(ss),
                SpecialSyntaxMsgC(ss, "expected (<variable> <init>) for each binding"),
                List(lb, vi));

    return(AddBinding(se, ss, bs, First(vi), lb));
}

static FObject SPassLetInit(FObject se, FObject ss, FObject lb, FObject nlb, FObject bd,
    FObject vi, int vf)
{
    FObject expr = SPassExpression(se, First(Rest(vi)));
    if (ss == LetSyntaxSyntax || ss == LetrecSyntaxSyntax)
    {
        // (<keyword> <transformer>)

        FAssert(BindingP(bd));

        if (SyntaxP(expr) == 0)
            RaiseException(Syntax, SpecialSyntaxToSymbol(ss),
                    SpecialSyntaxMsgC(ss, "expected a transformer"), List(lb, First(Rest(vi))));

        AsBinding(bd)->Syntax = expr;

        return(MakePair(MakePair(bd, EmptyListObject), nlb));
    }
    else
    {
        // (<variable> <init>)
        // (<formals> <init>)

        return(MakePair(MakePair(vf == 0 ? MakePair(bd, EmptyListObject) : bd,
                MakePair(expr, EmptyListObject)), nlb));
    }
}

static int FormalsCount(FObject formals)
{
    int fc = 0;

    while (PairP(formals))
    {
        formals = Rest(formals);
        fc += 1;
    }

    if (formals != EmptyListObject)
        fc += 1;

    return(fc);
}

static FObject GatherLetValuesFormals(FObject bs, FObject lb)
{
    FObject nbs = EmptyListObject;

    while (PairP(lb))
    {
        FAssert(PairP(First(lb)));

        int fc = FormalsCount(First(First(lb)));
        FObject vb = EmptyListObject;

        while (fc > 0)
        {
            FAssert(PairP(bs));

            vb = MakePair(First(bs), vb);
            bs = Rest(bs);

            fc -= 1;
        }

        nbs = MakePair(ReverseListModify(vb), nbs);
        lb = Rest(lb);
    }

    return(ReverseListModify(nbs));
}

static FObject SPassLetBindings(FObject se, FObject ss, FObject lb, int rf, int vf)
{
    // ((<variable> <init>) ...)
    // ((<formals> <init>) ...)
    // ((<keyword> <transformer>) ...)

    FObject bs = EmptyListObject;
    FObject tlb;

    for (tlb = lb; PairP(tlb); tlb = Rest(tlb))
        bs = SPassLetVar(se, ss, lb, bs, First(tlb), vf);

    if (tlb != EmptyListObject)
        RaiseException(Syntax, SpecialSyntaxToSymbol(ss),
                SpecialSyntaxMsgC(ss, "expected a list of bindings"), List(lb, tlb));

    if (rf != 0)
        EnterScopeList(bs);

    FObject nlb = EmptyListObject;
    FObject tbs = bs;

    if (vf != 0)
        tbs = GatherLetValuesFormals(tbs, lb);

    for (tlb = lb; PairP(tlb); tlb = Rest(tlb))
    {
        nlb = SPassLetInit(se, ss, lb, nlb, First(tbs), First(tlb), vf);
        tbs = Rest(tbs);
    }

    FAssert(tbs == EmptyListObject);

    if (rf == 0)
        EnterScopeList(bs);

    return(ReverseListModify(nlb));
}

static FObject AddLetStarBinding(FObject se, FObject id, FObject form)
{
    if (IdentifierP(id) == 0)
        RaiseExceptionC(Syntax, "let*", "let*: expected (<variable> <init>) for each binding",
                List(form, id));

    if (SyntacticEnvP(AsIdentifier(id)->SyntacticEnv))
        se = AsIdentifier(id)->SyntacticEnv;

    return(MakeBinding(se, id, FalseObject));
}

static FObject SPassLetStarVarInit(FObject se, FObject ss, FObject lb, FObject nlb, FObject vi,
    int vf)
{
    // (<variable> <init>)
    // (<formals> <init>)

    if (PairP(vi) == 0 || PairP(Rest(vi)) == 0 || Rest(Rest(vi)) != EmptyListObject)
        RaiseException(Syntax, SpecialSyntaxToSymbol(ss),
                SpecialSyntaxMsgC(ss, "expected (<variable> <init>) for each binding"),
                List(lb, vi));

    if (vf == 0)
    {
        FObject bd = AddLetStarBinding(se, First(vi), lb);
        nlb = MakePair(MakePair(MakePair(bd, EmptyListObject),
                MakePair(SPassExpression(se, First(Rest(vi))), EmptyListObject)), nlb);

        EnterScope(bd);
    }
    else
    {
        FObject bs = SPassFormals(se, ss, EmptyListObject, First(vi), First(vi));
        nlb = MakePair(MakePair(bs, MakePair(SPassExpression(se, First(Rest(vi))),
                EmptyListObject)), nlb);

        EnterScopeList(bs);
    }

    return(nlb);
}

static FObject SPassLetStarBindings(FObject se, FObject ss, FObject lb, int vf)
{
    // ((<variable> <init>) ...)
    // ((<formals> <init>) ...)

    FObject tlb;
    FObject nlb = EmptyListObject;

    for (tlb = lb; PairP(tlb); tlb = Rest(tlb))
        nlb = SPassLetStarVarInit(se, ss, lb, nlb, First(tlb), vf);

    if (tlb != EmptyListObject)
        RaiseException(Syntax, SpecialSyntaxToSymbol(ss),
                SpecialSyntaxMsgC(ss, "expected a list of bindings"), List(lb, tlb));

   return(ReverseListModify(nlb));
}

static FObject GatherNamedLetFormals(FObject lb)
{
    FObject formals = EmptyListObject;

    while (lb != EmptyListObject)
    {
        FAssert(PairP(lb));
        FAssert(PairP(First(lb)));
        FAssert(PairP(First(First(lb))));

        formals = MakePair(First(First(First(lb))), formals);
        lb = Rest(lb);
    }

    return(ReverseListModify(formals));
}

static FObject GatherNamedLetInits(FObject lb)
{
    FObject inits = EmptyListObject;

    while (lb != EmptyListObject)
    {
        FAssert(PairP(lb));
        FAssert(PairP(First(lb)));
        FAssert(PairP(Rest(First(lb))));

        inits = MakePair(First(Rest(First(lb))), inits);
        lb = Rest(lb);
    }

    return(ReverseListModify(inits));
}

static FObject SPassNamedLet(FObject se, FObject tag, FObject expr)
{
    // (let <variable> ((<variable> <init>) ...) <body>)

    if (PairP(Rest(Rest(Rest(expr)))) == 0)
        RaiseExceptionC(Syntax, "let", "let: expected bindings followed by a body", List(expr));

    FObject lb = SPassLetBindings(se, LetSyntax, First(Rest(Rest(expr))), 0, 0);
    FObject tb = MakeBinding(se, tag, FalseObject);
    EnterScope(tb);

    // (let tag ((name val) ...) body1 body2 ...)
    // --> ((letrec*-values (((tag) (lambda (name ...) body1 body2 ...)))
    //         tag) val ...)

    FObject lam = MakeLambda(NoValueObject, GatherNamedLetFormals(lb),
            SPassBody(se, LetSyntax, Rest(Rest(Rest(expr)))));
    FObject ret = MakePair(MakePair(LetrecStarValuesSyntax, MakePair(MakePair(
            MakePair(MakePair(tb, EmptyListObject),
                MakePair(lam, EmptyListObject)), EmptyListObject),
                MakePair(MakeReference(tb, tag), EmptyListObject))), GatherNamedLetInits(lb));

    LeaveScope(tb);
    LeaveLetScope(lb);

    return(ret);
}

static FObject SPassLet(FObject se, FObject ss, FObject expr, int rf, int sf, int vf)
{
    // rf : rec flag; eg. rf = 1 for letrec
    // sf : star flag; eg. sf = 1 for let*
    // vf : values flag; eg. vf = 1 for let-values

    if (PairP(Rest(expr)) == 0 || PairP(Rest(Rest(expr))) == 0)
        RaiseException(Syntax, SpecialSyntaxToSymbol(ss),
                SpecialSyntaxMsgC(ss, "expected bindings followed by a body"), List(expr));

    FObject lb;
    if (rf == 0 && sf != 0)
        lb = SPassLetStarBindings(se, ss, First(Rest(expr)), vf);
    else
        lb = SPassLetBindings(se, ss, First(Rest(expr)), rf, vf);

    FObject ret;
    if (ss == LetSyntaxSyntax || ss == LetrecSyntaxSyntax)
        ret = MakePair(BeginSyntax, SPassBody(se, ss, Rest(Rest(expr))));
    else
        ret = MakePair(rf == 0 ? LetStarValuesSyntax : LetrecStarValuesSyntax,
                MakePair(lb, SPassBody(se, ss, Rest(Rest(expr)))));

    LeaveLetScope(lb);

    return(ret);
}

int MatchReference(FObject ref, FObject se, FObject expr)
{
    FAssert(ReferenceP(ref));

    if (IdentifierP(expr) == 0)
        return(0);

    if (SyntacticEnvP(AsIdentifier(expr)->SyntacticEnv))
        se = AsIdentifier(expr)->SyntacticEnv;

    if (AsIdentifier(AsReference(ref)->Identifier)->Symbol != AsIdentifier(expr)->Symbol)
        return(0);

    FObject be = ResolveIdentifier(se, expr);

    if (AsReference(ref)->Binding == be)
        return(1);

    if (EnvironmentP(be) == 0 || EnvironmentP(AsReference(ref)->Binding) == 0)
        return(0);

    return(EnvironmentGet(be, AsIdentifier(expr)->Symbol)
            == EnvironmentGet(AsReference(ref)->Binding,
            AsIdentifier(AsReference(ref)->Identifier)->Symbol));
}

static FObject AddCaseDatum(FObject dtm, FObject dtms, FObject cse)
{
    if (dtms == EmptyListObject)
        return(MakePair(dtm, EmptyListObject));

    FObject lst = dtms;
    for (;;)
    {
        FAssert(PairP(lst));

        if (EqvP(First(lst), dtm))
            RaiseExceptionC(Syntax, "case", "case: duplicate datum", List(cse, dtm));

        if (Rest(lst) == EmptyListObject)
            break;

        lst = Rest(lst);
    }

    FAssert(PairP(lst));
    AsPair(lst)->Rest = MakePair(dtm, EmptyListObject);

    return(dtms);
}

static FObject AddCaseDatumList(FObject dtml, FObject dtms, FObject cse, FObject cls)
{
    while (PairP(dtml))
    {
        dtms = AddCaseDatum(First(dtml), dtms, cse);
        dtml = Rest(dtml);
    }

    if (dtml != EmptyListObject)
        RaiseExceptionC(Syntax, "case", "case: expected a proper list of datum", List(cse, cls));

    return(dtms);
}

static FObject SPassCaseClauses(FObject se, FObject clst, FObject cse)
{
    FObject rlst = EmptyListObject;
    FObject dtms = EmptyListObject;

    while (PairP(clst))
    {
        // ((<datum> ...) <expression> ...)
        // ((<datum> ...) => <expression>)
        // (else <expression> ...)
        // (else => <expression>)

        FObject cls = First(clst);
        if (PairP(cls) == 0)
            RaiseExceptionC(Syntax, "case", "case: expected a nonempty list for each clause",
                    List(cse, cls));

        if (Rest(clst) == EmptyListObject && MatchReference(ElseReference, se, First(cls)))
        {
            if (PairP(Rest(cls)) == 0)
                RaiseExceptionC(Syntax, "case",
                        "case: expected at least one expression following else", List(cse, cls));

            if (MatchReference(ArrowReference, se, First(Rest(cls))))
            {
                if (PairP(Rest(Rest(cls))) == 0 || Rest(Rest(Rest(cls))) != EmptyListObject)
                    RaiseExceptionC(Syntax, "case", "case: expected (else => <expression>)",
                            List(cse, cls));

                rlst = MakePair(MakePair(ElseSyntax, MakePair(ArrowSyntax,
                        MakePair(SPassExpression(se, First(Rest(Rest(cls)))),
                        EmptyListObject))), rlst);
            }
            else
                rlst = MakePair(MakePair(ElseSyntax,
                        SPassSequence(se, CaseSyntax, cse, Rest(cls))), rlst);
        }
        else
        {
            FObject dtml = SyntaxToDatum(First(cls));
            if (PairP(Rest(cls)) == 0 || PairP(dtml) == 0)
                RaiseExceptionC(Syntax, "case", "case: expected ((<datum> ...) <expression> ...)",
                        List(cse, cls));

            dtms = AddCaseDatumList(dtml, dtms, cse, cls);

            if (MatchReference(ArrowReference, se, First(Rest(cls))))
            {
                if (PairP(Rest(Rest(cls))) == 0 || Rest(Rest(Rest(cls))) != EmptyListObject)
                    RaiseExceptionC(Syntax, "case",
                            "case: expected ((<datum> ...) => <expression>)", List(cse, cls));

                rlst = MakePair(MakePair(dtml, MakePair(ArrowSyntax,
                        MakePair(SPassExpression(se, First(Rest(Rest(cls)))),
                        EmptyListObject))), rlst);
            }
            else
                rlst = MakePair(MakePair(dtml, SPassSequence(se, CaseSyntax, cse, Rest(cls))),
                        rlst);
        }

        clst = Rest(clst);
    }

    if (clst != EmptyListObject || rlst == EmptyListObject)
        RaiseExceptionC(Syntax, "case", "case: expected a list of clauses", List(cse, clst));

    return(ReverseListModify(rlst));
}

FObject SPassDo(FObject se, FObject expr)
{
    // (do ((var init [step]) ...) (test expr ...) cmd ...)
    // --> ((letrec*-values (((tag) (lambda (var ...)
    //         (if test (begin expr ...)
    //                (begin cmd ... (tag step ...)))))) tag) init ...)

    if (PairP(Rest(expr)) == 0 || PairP(Rest(Rest(expr))) == 0
            || PairP(First(Rest(expr))) == 0 || PairP(First(Rest(Rest(expr)))) == 0)
        RaiseExceptionC(Syntax, "do",
                "do: expected (do ((<var> <init> [<step>]) ...) (<test> <expr> ...) <cmd> ...)",
                List(expr));

    FObject bs = EmptyListObject;
    FObject inits = EmptyListObject;

    // Walk ((var init [step]) ...) list, bind variables and expands inits.

    FObject visl = First(Rest(expr));
    while (PairP(visl))
    {
        FObject vis = First(visl);

        if (PairP(vis) == 0 || PairP(Rest(vis)) == 0 || (PairP(Rest(Rest(vis)))
                && Rest(Rest(Rest(vis))) != EmptyListObject))
            RaiseExceptionC(Syntax, "do", "do: expected (<var> <init> [<step>])", List(expr, vis));

        bs = AddBinding(se, DoSyntax, bs, First(vis), vis);
        inits = MakePair(SPassExpression(se, First(Rest(vis))), inits);

        visl = Rest(visl);
    }

    if (visl != EmptyListObject)
        RaiseExceptionC(Syntax, "do",
                "do: expected a proper list of ((<var> <init> [<step>]) ...)", List(expr, visl));

    inits = ReverseListModify(inits);

    FObject tag = MakeIdentifier(TagSymbol, -1);
    FObject tb = MakeBinding(se, tag, FalseObject);

    EnterScopeList(bs);

    FObject stps = EmptyListObject;

    // Walk ((var init [step]) ...) list, expand steps.

    visl = First(Rest(expr));
    while (PairP(visl))
    {
        FObject vis = First(visl);

        if (PairP(Rest(Rest(vis))))
            stps = MakePair(SPassExpression(se, First(Rest(Rest(vis)))), stps);
        else
            stps = MakePair(SPassExpression(se, First(vis)), stps);

        visl = Rest(visl);
    }
    FAssert(visl == EmptyListObject);

    stps = ReverseListModify(stps);

    FObject tst = First(Rest(Rest(expr))); // (test expr ...)
    FAssert(PairP(tst));

    FObject ift = Rest(tst) == EmptyListObject ? FalseObject
            : MakePair(BeginSyntax, SPassSequence(se, DoSyntax, Rest(tst), Rest(tst)));

    FObject cmds = Rest(Rest(Rest(expr))); // cmd ...
    FObject cdo = MakePair(MakeReference(tb, tag), stps); // (tag step ...)

    FObject iff = cmds == EmptyListObject ? cdo
            : MakePair(BeginSyntax, SPassSequenceLast(se, DoSyntax, cmds, cmds,
            MakePair(cdo, EmptyListObject))); // (begin cmd ... (tag step ...))

    FObject lambda = MakeLambda(NoValueObject, bs, MakePair(MakePair(IfSyntax,
            MakePair(SPassExpression(se, First(tst)), MakePair(ift,
            MakePair(iff, EmptyListObject)))), EmptyListObject));

    LeaveScopeList(bs);

    return(MakePair(MakePair(LetrecStarValuesSyntax, MakePair(
            MakePair(MakePair(MakePair(tb, EmptyListObject), MakePair(lambda, EmptyListObject)),
            EmptyListObject), MakePair(MakeReference(tb, tag), EmptyListObject))), inits));
}

static FObject SPassCaseLambda(FObject se, FObject expr, FObject clst)
{
    if (clst == EmptyListObject)
        return(EmptyListObject);

    if (PairP(clst) == 0 || PairP(First(clst)) == 0 || PairP(Rest(First(clst))) == 0)
        RaiseExceptionC(Syntax, "case-lambda",
                "case-lambda: expected (case-lambda (<formals> <body>) ...)", List(expr, clst));

    FObject cls = First(clst);
    return(MakePair(SPassLambda(se, NoValueObject, First(cls), Rest(cls)),
            SPassCaseLambda(se, expr, Rest(clst))));
}

static FObject SPassReadInclude(FObject expr, FObject ss)
{
    FAssert(ss == IncludeSyntax || ss == IncludeCISyntax);

    FObject ret = ReadInclude(Rest(expr), ss == IncludeCISyntax);
    if (PairP(ret) == 0)
        RaiseException(Syntax, SpecialSyntaxToSymbol(ss),
                SpecialSyntaxMsgC(ss, "expected a proper list of one or more strings"),
                List(expr));

    return(ret);
}

static FObject SPassQuasiquote(FObject se, FObject expr, FObject tpl, int dpth)
{
    if (PairP(tpl))
    {
        if (MatchReference(UnquoteReference, se, First(tpl)))
        {
            FAssert(dpth > 0);
            dpth -= 1;

            if (dpth == 0)
            {
                if (PairP(Rest(tpl)) == 0 || Rest(Rest(tpl)) != EmptyListObject)
                    RaiseExceptionC(Syntax, "unquote", "unquote: expected (unquote <expression>)",
                            List(expr, tpl));

                return(SPassExpression(se, First(Rest(tpl))));
            }
        }
        else if (MatchReference(QuasiquoteReference, se, First(tpl)))
            dpth += 1;
        else if (MatchReference(UnquoteSplicingReference, se, First(tpl)))
        {
            FAssert(dpth > 0);

            dpth -= 1;
        }
        else if (dpth == 1 && PairP(First(tpl)) && MatchReference(UnquoteSplicingReference, se,
                First(First(tpl))))
        {
            FObject ftpl = First(tpl);

            if (PairP(Rest(ftpl)) == 0 || Rest(Rest(ftpl)) != EmptyListObject)
                RaiseExceptionC(Syntax, "unquote-splicing",
                        "unquote-splicing: expected (unquote-splicing <expression>)",
                        List(expr, ftpl));

            FObject rst = SPassQuasiquote(se, expr, Rest(tpl), dpth);
            if (rst == Rest(tpl))
                rst = MakePair(QuoteSyntax, MakePair(SyntaxToDatum(rst), EmptyListObject));

            return(MakePair(AppendReference, MakePair(SPassExpression(se, First(Rest(ftpl))),
                    MakePair(rst, EmptyListObject))));
        }

        FObject fst = SPassQuasiquote(se, expr, First(tpl), dpth);
        FObject rst = SPassQuasiquote(se, expr, Rest(tpl), dpth);
        if (fst == First(tpl) && rst == Rest(tpl))
            return(tpl);

        if (fst == First(tpl))
            fst = MakePair(QuoteSyntax, MakePair(SyntaxToDatum(fst), EmptyListObject));
        else if (rst == Rest(tpl))
            rst = MakePair(QuoteSyntax, MakePair(SyntaxToDatum(rst), EmptyListObject));

        return(MakePair(ConsReference, MakePair(fst, MakePair(rst, EmptyListObject))));
    }
    else if (VectorP(tpl))
    {
        FObject ltpl = VectorToList(tpl);
        FObject ret = SPassQuasiquote(se, expr, ltpl, dpth);
        if (ret == ltpl)
            return(tpl);

        FAssert(PairP(ret));

        return(MakePair(ListToVectorReference, MakePair(ret, EmptyListObject)));
    }

    return(tpl);
}

static FObject SPassSpecialSyntax(FObject se, FObject ss, FObject expr)
{
    FAssert(SpecialSyntaxP(ss));

    if (ss == QuoteSyntax)
    {
        // (quote <datum>)

        if (PairP(Rest(expr)) == 0 || Rest(Rest(expr)) != EmptyListObject)
            RaiseExceptionC(Syntax, "quote", "quote: expected (quote <datum>)", List(expr));

        return(MakePair(QuoteSyntax, MakePair(SyntaxToDatum(First(Rest(expr))), EmptyListObject)));
    }
    else if (ss == LambdaSyntax)
    {
        // (lambda (<variable> ...) <body>)
        // (lambda <variable> <body>)
        // (lambda (<variable> <variable> ... . <variable>) <body>)

        if (PairP(Rest(expr)) == 0 || PairP(Rest(Rest(expr))) == 0)
            RaiseExceptionC(Syntax, "lambda", "lambda: expected (lambda <formals> <body>)",
                    List(expr));

        return(SPassLambda(se, NoValueObject, First(Rest(expr)), Rest(Rest(expr))));
    }
    else if (ss == IfSyntax)
    {
        // (if <test> <consequent> <alternate>)
        // (if <test> <consequent>)

        if (PairP(Rest(expr)) == 0 || PairP(Rest(Rest(expr))) == 0
                || (Rest(Rest(Rest(expr))) != EmptyListObject
                    && (PairP(Rest(Rest(Rest(expr)))) == 0
                    || Rest(Rest(Rest(Rest(expr)))) != EmptyListObject)))
            RaiseExceptionC(Syntax, "if", "if: expected (if <test> <consequent> [<alternate>])",
                    List(expr));

        return(MakePair(IfSyntax, MakePair(SPassExpression(se, First(Rest(expr))),
                MakePair(SPassExpression(se, First(Rest(Rest(expr)))),
                Rest(Rest(Rest(expr))) == EmptyListObject ? EmptyListObject
                : MakePair(SPassExpression(se, First(Rest(Rest(Rest(expr))))),
                EmptyListObject)))));
    }
    else if (ss == SetBangSyntax)
    {
        // (set! <variable> <expression>)

        if (PairP(Rest(expr)) == 0 || PairP(Rest(Rest(expr))) == 0
                || Rest(Rest(Rest(expr))) != EmptyListObject)
            RaiseExceptionC(Syntax, "set!", "set!: expected (set! <variable> <expression>)",
                    List(expr));

        FObject var = First(Rest(expr));
        if (IdentifierP(var) == 0)
            RaiseExceptionC(Syntax, "set!", "set!: variable expected", List(expr, var));

        if (SyntacticEnvP(AsIdentifier(var)->SyntacticEnv))
            se = AsIdentifier(var)->SyntacticEnv;

        FObject be = ResolveIdentifier(se, var);
        if (SyntaxBindingP(be, var))
            RaiseExceptionC(Syntax, "set!", "set!: variable already bound to syntax",
                    List(expr, var));

        if (EnvironmentP(be) && AsEnvironment(be)->Interactive == FalseObject)
        {
            FObject gl = EnvironmentBind(be, AsIdentifier(var)->Symbol);

            FAssert(GlobalP(gl));

            if (AsGlobal(gl)->State == GlobalImported
                    || AsGlobal(gl)->State == GlobalImportedModified)
                RaiseExceptionC(Syntax, "set!",
                        "set!: imported variables may not be modified in libraries",
                        List(expr, var));
        }

        return(MakePair(SetBangSyntax, MakePair(MakeReference(be, var),
                MakePair(SPassExpression(se, First(Rest(Rest(expr)))), EmptyListObject))));
    }
    else if (ss == LetSyntax)
    {
        // (let ((<variable> <init>) ...) <body>)
        // (let <variable> ((<variable> <init>) ...) <body>)

        if (PairP(Rest(expr)) == 0)
            RaiseExceptionC(Syntax, "let", "let: expected bindings followed by a body",
                    List(expr));

        FObject tag = First(Rest(expr));
        if (IdentifierP(tag))
            return(SPassNamedLet(se, tag, expr));

        return(SPassLet(se, ss, expr, 0, 0, 0));
    }
    else if (ss == LetrecSyntax)
    {
        // (letrec ((<variable> <init>) ...) <body>)

        return(SPassLet(se, ss, expr, 1, 0, 0));
    }
    else if (ss == LetrecStarSyntax)
    {
        // (letrec* ((<variable> <init>) ...) <body>)

        return(SPassLet(se, ss, expr, 1, 1, 0));
    }
    else if (ss == LetStarSyntax)
    {
        // (let* ((<variable> <init>) ...) <body>)

        return(SPassLet(se, ss, expr, 0, 1, 0));
    }
    else if (ss == LetValuesSyntax)
    {
        // (let-values ((<formals> <init>) ...) <body>)

        return(SPassLet(se, ss, expr, 0, 0, 1));
    }
    else if (ss == LetStarValuesSyntax)
    {
        // (let*-values ((<formals> <init>) ...) <body>)

        return(SPassLet(se, ss, expr, 0, 1, 1));
    }
    else if (ss == LetrecValuesSyntax)
    {
        // (letrec-values ((<formals> <init>) ...) <body>)

        return(SPassLet(se, ss, expr, 1, 0, 1));
    }
    else if (ss == LetrecStarValuesSyntax)
    {
        // (letrec*-values ((<formals> <init>) ...) <body>)

        return(SPassLet(se, ss, expr, 1, 1, 1));
    }
    else if (ss == LetSyntaxSyntax)
    {
        // (let-syntax ((<keyword> <transformer>) ...) <body>)

        return(SPassLet(se, ss, expr, 0, 0, 0));
    }
    else if (ss == LetrecSyntaxSyntax)
    {
        // (letrec-syntax ((<keyword> <transformer>) ...) <body>)

        return(SPassLet(se, ss, expr, 1, 0, 0));
    }
    else if (ss == CaseSyntax)
    {
        // (case <key> <clause> ...)

        if (PairP(Rest(expr)) == 0)
            RaiseExceptionC(Syntax, "case", "case: expected (case <test> <clause> ...)",
                    List(expr));

        return(MakePair(CaseSyntax, MakePair(SPassExpression(se, First(Rest(expr))),
                SPassCaseClauses(se, Rest(Rest(expr)), expr))));
    }
    else if (ss == OrSyntax)
    {
        // (or <test> ...)

        if (Rest(expr) == EmptyListObject)
            return(FalseObject);

        if (PairP(Rest(expr)) == 0)
            RaiseExceptionC(Syntax, "or", "or: expected (or <test> ...)", List(expr));

        if (Rest(Rest(expr)) == EmptyListObject)
            return(SPassExpression(se, First(Rest(expr))));

        return(MakePair(OrSyntax, SPassSequence(se, OrSyntax, expr, Rest(expr))));
    }
    else if (ss == BeginSyntax)
    {
        // (begin <expression> ...)

        if (PairP(Rest(expr)) == 0)
            RaiseExceptionC(Syntax, "begin", "begin: expected (begin <expression> ...)",
                    List(expr));

        return(MakePair(BeginSyntax, SPassSequence(se, BeginSyntax, expr, Rest(expr))));
    }
    else if (ss == DoSyntax)
    {
        // (do ((<variable> <init> [<step>]) ...) (<test> <expression> ...) <command> ...)

        return(SPassDo(se, expr));
    }
    else if (ss == SyntaxRulesSyntax)
    {
        // (syntax-rules (<literal> ...) <syntax rule> ...)
        // (syntax-rules <ellipse> (<literal> ...) <syntax rule> ...)

        return(CompileSyntaxRules(MakeSyntacticEnv(se), expr));
    }
    else if (ss == SyntaxErrorSyntax)
    {
        // (syntax-error <message> <args> ...)

        if (PairP(Rest(expr)) == 0)
            RaiseExceptionC(Syntax, "syntax-error",
                    "syntax-error: expected (syntax-error <message> <args> ...)", List(expr));

        if (StringP(First(Rest(expr))) == 0)
            RaiseExceptionC(Syntax, "syntax-error",
                    "syntax-error: expected a string for the <message>",
                    List(expr, First(Rest(expr))));

        char msg[128];
        StringToC(First(Rest(expr)), msg, sizeof(msg));

        RaiseExceptionC(Syntax, "syntax-error", msg, Rest(Rest(expr)));
    }
    else if (ss == IncludeSyntax || ss == IncludeCISyntax)
    {
        // (include <string> ...)
        // (include-ci <string> ...)

        return(SPassExpression(se, MakePair(BeginSyntax, SPassReadInclude(expr, ss))));
    }
    else if (ss == CondExpandSyntax)
    {
        // (cond-expand <ce-clause> ...)

        return(SPassExpression(se, MakePair(BeginSyntax, CondExpand(se, expr, Rest(expr)))));
    }
    else if (ss == CaseLambdaSyntax)
    {
        // (case-lambda (<formals> <body>) ...)

        if (PairP(Rest(expr)) == 0)
            RaiseExceptionC(Syntax, "case-lambda",
                    "case-lambda: expected (case-lambda (<formals> <body>) ...)",
                    List(expr));

        return(MakeCaseLambda(SPassCaseLambda(se, expr, Rest(expr))));
    }
    else if (ss == QuasiquoteSyntax)
    {
        // (quasiquote <qq-template>)

        if (PairP(Rest(expr)) == 0 || Rest(Rest(expr)) != EmptyListObject)
            RaiseExceptionC(Syntax, "quasiquote",
                    "quasiquote: expected (quasiquote <qq-template>)", List(expr));

        FObject obj = SPassQuasiquote(se, expr, First(Rest(expr)), 1);
        if (obj == First(Rest(expr)))
            return(MakePair(QuoteSyntax, MakePair(SyntaxToDatum(obj), EmptyListObject)));
        return(obj);
    }
    else
        RaiseException(Syntax, SpecialSyntaxToSymbol(ss),
                SpecialSyntaxMsgC(ss, "syntax not allowed here"), List(expr));

    return(expr);
}

static FObject SPassOperands(FObject se, FObject opds, FObject form)
{
    if (opds == EmptyListObject)
        return(EmptyListObject);

    if (PairP(opds) == 0)
        RaiseExceptionC(Syntax, "procedure-call", "procedure-call: expected list of operands",
                List(form, opds));

    return(MakePair(SPassExpression(se, First(opds)), SPassOperands(se, Rest(opds), form)));
}

static FObject SPassKeyword(FObject se, FObject expr)
{
    if (IdentifierP(expr))
    {
        if (SyntacticEnvP(AsIdentifier(expr)->SyntacticEnv))
            se = AsIdentifier(expr)->SyntacticEnv;

        return(MakeReference(ResolveIdentifier(se, expr), expr));
    }

    return(expr);
}

static FObject SPassExpression(FObject se, FObject expr)
{
    if (IdentifierP(expr))
    {
        if (SyntacticEnvP(AsIdentifier(expr)->SyntacticEnv))
            se = AsIdentifier(expr)->SyntacticEnv;

        FObject be = ResolveIdentifier(se, expr);
        if (SyntaxBindingP(be, expr))
            RaiseExceptionC(Syntax, "variable", "variable: bound to syntax", List(expr));

        return(MakeReference(be, expr));
    }
    else if (PairP(expr) == 0)
    {
        FAssert(SymbolP(expr) == 0);
        return(expr);
    }

    FObject op = SPassKeyword(se, First(expr));
    if (ReferenceP(op))
    {
        FObject val;
        if (BindingP(AsReference(op)->Binding))
            val = AsBinding(AsReference(op)->Binding)->Syntax;
        else
        {
            FAssert(EnvironmentP(AsReference(op)->Binding));

            val = EnvironmentGet(AsReference(op)->Binding,
                    AsIdentifier(AsReference(op)->Identifier)->Symbol);
        }

        if (SpecialSyntaxP(val))
            return(SPassSpecialSyntax(se, val, expr));

        if (SyntaxRulesP(val))
            return(SPassExpression(se, ExpandSyntaxRules(MakeSyntacticEnv(se), val, Rest(expr))));

        // Other macro transformers would go here.
    }

    // Procedure Call
    // (<operator> <operand> ...)

    return(MakePair(SPassExpression(se, First(expr)), SPassOperands(se, Rest(expr), expr)));
}

static FObject SPassSequenceLast(FObject se, FObject ss, FObject form, FObject seq, FObject last)
{
    if (seq == EmptyListObject)
        return(last);

    if (PairP(seq) == 0)
        RaiseException(Syntax, SpecialSyntaxToSymbol(ss),
                SpecialSyntaxMsgC(ss, "expected list of expressions"), List(form, seq));

    return(MakePair(SPassExpression(se, First(seq)),
            SPassSequenceLast(se, ss, form, Rest(seq), last)));
}

static FObject SPassSequence(FObject se, FObject ss, FObject form, FObject seq)
{
    return(SPassSequenceLast(se, ss, form, seq, EmptyListObject));
}

FObject GatherVariablesAndSyntax(FObject se, FObject dlst, FObject bs)
{
    FObject bl = EmptyListObject;

    while (PairP(dlst))
    {
        FObject expr = First(dlst);

        FAssert(PairP(expr));

        if (First(expr) == DefineSyntax)
        {
            FAssert(PairP(bs));

            bl = MakePair(MakePair(First(bs), EmptyListObject), bl);
            bs = Rest(bs);
        }
        else if (First(expr) == DefineValuesSyntax)
        {
            // (define-values (<formal> ...) <expression>)

            FAssert(PairP(Rest(expr)));

            int fc = FormalsCount(First(Rest(expr)));
            FObject vb = EmptyListObject;

            while (fc > 0)
            {
                FAssert(PairP(bs));

                vb = MakePair(First(bs), vb);
                bs = Rest(bs);

                fc -= 1;
            }

            bl = MakePair(ReverseListModify(vb), bl);
        }
        else
        {
            // (define-syntax <keyword> <transformer>)

            FAssert(First(expr) == DefineSyntaxSyntax);

            FAssert(PairP(bs));
            FAssert(BindingP(First(bs)));
            FAssert(PairP(Rest(expr)));
            FAssert(PairP(Rest(Rest(expr))));
            FAssert(Rest(Rest(Rest(expr))) == EmptyListObject);

            FObject trans = SPassExpression(se, First(Rest(Rest(expr))));
            if (SyntaxP(trans) == 0)
                RaiseExceptionC(Syntax, "define-syntax", "define-syntax: expected a transformer",
                        List(expr, First(Rest(Rest(expr)))));

            AsBinding(First(bs))->Syntax = trans;
            bs = Rest(bs);
        }

        dlst = Rest(dlst);
    }

    FAssert(dlst == EmptyListObject);
    FAssert(bs == EmptyListObject);

    return(ReverseListModify(bl));
}

FObject VariablesAndExpandInits(FObject se, FObject dlst, FObject bl)
{
    FObject lb = EmptyListObject;

    while (PairP(dlst))
    {
        FObject expr = First(dlst);

        FAssert(PairP(expr));

        if (First(expr) == DefineSyntax)
        {
            FAssert(PairP(Rest(expr)));
            FAssert(PairP(bl));

            if (PairP(First(Rest(expr))))
            {
                // (define (<variable> <formals>) <body>)
                // (define (<variable> . <formal>) <body>)

                lb = MakePair(MakePair(First(bl), MakePair(
                        SPassLambda(se, NoValueObject, Rest(First(Rest(expr))), Rest(Rest(expr))),
                        EmptyListObject)), lb);
            }
            else
            {
                // (define <variable> <expression>)

                FAssert(PairP(Rest(Rest(expr))));
                FAssert(Rest(Rest(Rest(expr))) == EmptyListObject);

                lb = MakePair(MakePair(First(bl),
                        MakePair(SPassExpression(se, First(Rest(Rest(expr)))),
                        EmptyListObject)), lb);
            }

            bl = Rest(bl);
        }
        else if (First(expr) == DefineValuesSyntax)
        {
            FAssert(PairP(Rest(expr)));
            FAssert(PairP(Rest(Rest(expr))));
            FAssert(Rest(Rest(Rest(expr))) == EmptyListObject);
            FAssert(PairP(bl));

            lb = MakePair(MakePair(First(bl),
                    MakePair(SPassExpression(se, First(Rest(Rest(expr)))), EmptyListObject)),
                    lb);

            bl = Rest(bl);
        }

        // DefineSyntaxSyntax dealt with in GatherVariablesAndSyntax

        dlst = Rest(dlst);
    }

    FAssert(dlst == EmptyListObject);
    FAssert(bl == EmptyListObject);

    return(ReverseListModify(lb));
}

static FObject SPassBodyExpression(FObject se, FObject expr)
{
    if (PairP(expr) == 0)
        return(expr);

    FObject op = SPassBodyExpression(se, First(expr));
    if (IdentifierP(op))
    {
        FObject be = ResolveIdentifier(se, op);

        FObject val;
        if (BindingP(be))
            val = AsBinding(be)->Syntax;
        else
        {
            FAssert(EnvironmentP(be));

            val = EnvironmentGet(be, AsIdentifier(op)->Symbol);
        }

        if (SpecialSyntaxP(val))
            return(MakePair(val, Rest(expr)));

        if (SyntaxRulesP(val))
            return(SPassBodyExpression(se, ExpandSyntaxRules(MakeSyntacticEnv(se), val,
                    Rest(expr))));

        // Other macro transformers would go here.
    }

    return(expr);
}

static FObject AppendBegin(FObject ss, FObject begin, FObject body, FObject form)
{
    if (begin == EmptyListObject)
        return(body);

    if (PairP(begin) == 0)
        RaiseException(Syntax, SpecialSyntaxToSymbol(ss),
                SpecialSyntaxMsgC(ss, "begin must be a proper list"), List(form, begin));

    return(MakePair(First(begin), AppendBegin(ss, Rest(begin), body, form)));
}

static FObject SPassBody(FObject se, FObject ss, FObject body)
{
    // lambda, let, let*, letrec, letrec*, let-values, let*-values, let-syntax, letrec-syntax,
    // parameterize, guard, and case-lambda all have bodies

    // begin, define, define-values, define-syntax, include, include-ci, cond-expand
    // 3 passes required

    FObject bs = EmptyListObject;
    FObject dlst = EmptyListObject;

    // Pass 1: make a list of bindings for all variables being defined and check for
    // duplicates; splice begin bodies into a single body

    while (PairP(body))
    {
        FObject expr = SPassBodyExpression(se, First(body));
        if (PairP(expr) == 0)
            break;

        if (First(expr) == BeginSyntax)
        {
            // (begin ...)

            body = AppendBegin(ss, Rest(expr), Rest(body), expr);
        }
        else if (First(expr) == IncludeSyntax || First(expr) == IncludeCISyntax)
        {
            // (include <string> ...)
            // (include-ci <string> ...)

            body = AppendBegin(ss, SPassReadInclude(expr, First(expr)), Rest(body), expr);
        }
        else if (First(expr) == CondExpandSyntax)
        {
            // (cond-expand <ce-clause> ...)

            body = AppendBegin(ss, CondExpand(se, expr, Rest(expr)), Rest(body), expr);
        }
        else if (First(expr) == DefineSyntax)
        {
            // (define <variable> <expression>)
            // (define (<variable> <formal> ...) <body)
            // (define (<variable> . <formal>) <body>)

            if (PairP(Rest(expr)) == 0)
                RaiseExceptionC(Syntax, "define",
                        "define: expected (define (<variable> ...) <body>) or (define <variable> <expr>)",
                        List(expr));

            if (PairP(First(Rest(expr))))
            {
                // (define (<variable> <formals>) <body>)
                // (define (<variable> . <formal>) <body>)

                bs = AddBinding(se, DefineSyntax, bs, First(First(Rest(expr))), expr);
            }
            else
            {
                // (define <variable> <expression>)

                if (PairP(Rest(Rest(expr))) == 0 || Rest(Rest(Rest(expr))) != EmptyListObject)
                    RaiseExceptionC(Syntax, "define",
                            "define: expected (define <variable> <expression>)", List(expr));

                bs = AddBinding(se, DefineSyntax, bs, First(Rest(expr)), expr);
            }

            dlst = MakePair(expr, dlst);

            body = Rest(body);
        }
        else if (First(expr) == DefineValuesSyntax)
        {
            // (define-values (<formal> ...) <expression>)

            if (PairP(Rest(expr)) == 0 || PairP(Rest(Rest(expr))) == 0
                    || Rest(Rest(Rest(expr))) != EmptyListObject)
                RaiseExceptionC(Syntax, "define-values",
                        "define-values: expected (define-values (<formal> ...) <expression>)",
                        List(expr));

            bs = SPassFormals(se, DefineValuesSyntax, bs, First(Rest(expr)), expr);
            dlst = MakePair(expr, dlst);

            body = Rest(body);
        }
        else if (First(expr) == DefineSyntaxSyntax)
        {
            // (define-syntax <keyword> <transformer>)

            if (PairP(Rest(expr)) == 0 || PairP(Rest(Rest(expr))) == 0
                    || Rest(Rest(Rest(expr))) != EmptyListObject)
                RaiseExceptionC(Syntax, "define-syntax",
                        "define-syntax: expected (define-syntax <keyword> <transformer>)",
                        List(expr));

            bs = AddBinding(se, DefineSyntaxSyntax, bs, First(Rest(expr)), expr);
            dlst = MakePair(expr, dlst);

            body = Rest(body);
        }
        else
            break;
    }

    if (PairP(body) == 0)
        RaiseException(Syntax, SpecialSyntaxToSymbol(ss),
                SpecialSyntaxMsgC(ss, "body must have at least one expression"), List(body));

    if (dlst == EmptyListObject)
        return(SPassSequence(se, ss, body, body));

    dlst = ReverseListModify(dlst);
    FAssert(bs != EmptyListObject);

    // Pass 2: evaluate and assign transformers to all keywords specified by define-syntax;
    // collect bindings together for the same define or define-values ready to be paired with
    // an init expression in pass 3.

    EnterScopeList(bs);
    FObject bl = GatherVariablesAndSyntax(se, dlst, bs);

    FObject ret;
    if (bl == EmptyListObject)
        ret = SPassSequence(se, ss, body, body);
    else
    {
        // Pass 3: expand inits and pair with bindings

        FObject lb = VariablesAndExpandInits(se, dlst, bl);
        ret = MakePair(MakePair(LetrecStarValuesSyntax,
                MakePair(lb, SPassSequence(se, ss, body, body))), EmptyListObject);
    }

    LeaveScopeList(bs);
    return(ret);
}

FObject SPassLambda(FObject se, FObject name, FObject formals, FObject body)
{
    FAssert(SyntacticEnvP(se));

    FObject bs = SPassFormals(se, LambdaSyntax, EmptyListObject, formals, formals);
    EnterScopeList(bs);
    FObject ret = MakeLambda(name, bs, SPassBody(se, LambdaSyntax, body));
    LeaveScopeList(bs);

    return(ret);
}

FObject ExpandExpression(FObject se, FObject expr)
{
    return(SPassExpression(se, expr));
}

// ----------------

static int EvaluateFeatureRequirement(FObject se, FObject expr, FObject cls, FObject obj)
{
    if (IdentifierP(obj))
    {
        FObject lst = Features;
        while (PairP(lst))
        {
            FAssert(SymbolP(First(lst)));

            if (First(lst) == AsIdentifier(obj)->Symbol)
                return(1);

            lst = Rest(lst);
        }

        FAssert(lst == EmptyListObject);

        return(0);
    }

    if (PairP(obj) == 0 || IdentifierP(First(obj)) == 0)
        RaiseExceptionC(Syntax, "cond-expand",
                "cond-expand: invalid feature requirement syntax", List(expr, cls, obj));

    if (MatchReference(LibraryReference, se, First(obj)))
    {
        if (PairP(Rest(obj)) == 0 || Rest(Rest(obj)) != EmptyListObject)
            RaiseExceptionC(Syntax, "cond-expand",
                    "cond-expand: expected (library <library name>)", List(expr, cls, obj));

        FObject nam = LibraryName(First(Rest(obj)));
        if (PairP(nam) == 0)
            RaiseExceptionC(Syntax, "cond-expand",
                    "cond-expand: library name must be a list of symbols and/or integers",
                    List(obj));

        return(LibraryP(FindOrLoadLibrary(nam)));
    }
    else if (MatchReference(AndReference, se, First(obj)))
    {
        FObject lst = Rest(obj);

        while (PairP(lst))
        {
            if (EvaluateFeatureRequirement(se, expr, cls, First(lst)) == 0)
                return(0);

            lst = Rest(lst);
        }

        if (lst != EmptyListObject)
            RaiseExceptionC(Syntax, "cond-expand",
                    "cond-expand: expected a proper list of feature requirements",
                    List(expr, cls, obj));

        return(1);
    }
    else if (MatchReference(OrReference, se, First(obj)))
    {
        FObject lst = Rest(obj);

        while (PairP(lst))
        {
            if (EvaluateFeatureRequirement(se, expr, cls, First(lst)))
                return(1);

            lst = Rest(lst);
        }

        if (lst != EmptyListObject)
            RaiseExceptionC(Syntax, "cond-expand",
                    "cond-expand: expected a proper list of feature requirements",
                    List(expr, cls, obj));

        return(0);
    }
    else if (MatchReference(NotReference, se, First(obj)))
    {
        if (PairP(Rest(obj)) == 0 || Rest(Rest(obj)) != EmptyListObject)
            RaiseExceptionC(Syntax, "cond-expand",
                    "cond-expand: expected (not <feature requirement>)", List(expr, cls, obj));

        return(EvaluateFeatureRequirement(se, expr, cls, First(Rest(obj))) == 0);
    }
    else
        RaiseExceptionC(Syntax, "cond-expand",
                "cond-expand: invalid feature requirement syntax", List(expr, cls, obj));

    return(0);
}

FObject CondExpand(FObject se, FObject expr, FObject clst)
{
    while (PairP(clst))
    {
        FObject cls = First(clst);

        if (PairP(cls) == 0 || PairP(Rest(cls)) == 0)
            RaiseExceptionC(Syntax, "cond-expand",
                    "cond-expand: expected (<feature requirement> <expression> ..) for each clause",
                    List(expr, clst));

        if (IdentifierP(First(cls)) && MatchReference(ElseReference, se, First(cls)))
        {
            if (Rest(clst) != EmptyListObject)
                RaiseExceptionC(Syntax, "cond-expand",
                        "cond-expand: (else <expression> ..) must be the last clause",
                        List(expr, clst, cls));

            return(Rest(cls));
        }

        if (EvaluateFeatureRequirement(se, expr, cls, First(cls)))
            return(Rest(cls));

        clst = Rest(clst);
    }

    if (clst != EmptyListObject)
        RaiseExceptionC(Syntax, "cond-expand", "cond-expand: expected a proper list of clauses",
                List(expr, clst));

    RaiseExceptionC(Syntax, "cond-expand", "cond-expand: no clauses match", List(expr));

    // Never reached.
    return(NoValueObject);
}

// ----------------

FObject ReadInclude(FObject lst, int cif)
{
    if (PairP(lst) == 0)
        return(EmptyListObject);

    FObject ret = EmptyListObject;

    while (PairP(lst))
    {
        if (StringP(First(lst)) == 0)
            return(EmptyListObject);

        FObject port = OpenInputFile(First(lst), 1);
        for (;;)
        {
            FObject obj = Read(port, 1, cif);
            if (obj == EndOfFileObject)
                break;

            ret = MakePair(obj, ret);
        }

        lst = Rest(lst);
    }

    if (lst != EmptyListObject)
        return(EmptyListObject);

    return(ReverseListModify(ret));
}
