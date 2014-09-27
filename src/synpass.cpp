/*

Foment

*/

#include <string.h>
#include "foment.hpp"
#include "compile.hpp"

// ---- Syntax Pass ----

static FObject EnterScope(FObject bd)
{
    FAssert(BindingP(bd));
    FAssert(SyntacticEnvP(AsBinding(bd)->SyntacticEnv));

//    AsSyntacticEnv(AsBinding(bd)->SyntacticEnv)->LocalBindings = MakePair(bd,
//            AsSyntacticEnv(AsBinding(bd)->SyntacticEnv)->LocalBindings);
    Modify(FSyntacticEnv, AsBinding(bd)->SyntacticEnv, LocalBindings, MakePair(bd,
            AsSyntacticEnv(AsBinding(bd)->SyntacticEnv)->LocalBindings));

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

//    AsSyntacticEnv(AsBinding(bd)->SyntacticEnv)->LocalBindings =
//            Rest(AsSyntacticEnv(AsBinding(bd)->SyntacticEnv)->LocalBindings);
    Modify(FSyntacticEnv, AsBinding(bd)->SyntacticEnv, LocalBindings,
            Rest(AsSyntacticEnv(AsBinding(bd)->SyntacticEnv)->LocalBindings));
}

static int_t IdentifierEqualP(FObject id1, FObject id2)
{
    FAssert(IdentifierP(id1));
    FAssert(IdentifierP(id2));

    if (AsIdentifier(id1)->Symbol != AsIdentifier(id2)->Symbol)
        return(0);

    for (;;)
    {
        FAssert(AsIdentifier(id1)->Symbol == AsIdentifier(id2)->Symbol);

        if (AsIdentifier(id1)->SyntacticEnv != AsIdentifier(id2)->SyntacticEnv)
            return(0);

        if (IdentifierP(AsIdentifier(id1)->Wrapped) == 0)
            break;

        FAssert(IdentifierP(AsIdentifier(id1)->Wrapped));
        FAssert(IdentifierP(AsIdentifier(id2)->Wrapped));

        id1 = AsIdentifier(id1)->Wrapped;
        id2 = AsIdentifier(id2)->Wrapped;
    }

    return(IdentifierP(AsIdentifier(id2)->Wrapped) == 0);
}

FObject ResolveIdentifier(FObject se, FObject id)
{
    FAssert(IdentifierP(id));

    for (;;)
    {
        FAssert(SyntacticEnvP(se));

        FObject lb = AsSyntacticEnv(se)->LocalBindings;

        while (lb != EmptyListObject)
        {
            FAssert(PairP(lb));
            FAssert(BindingP(First(lb)));

            if (IdentifierEqualP(AsBinding(First(lb))->Identifier, id))
                return(First(lb));

            lb = Rest(lb);
        }

        if (IdentifierP(AsIdentifier(id)->Wrapped) == 0)
            break;

        se = AsIdentifier(id)->SyntacticEnv;
        id = AsIdentifier(id)->Wrapped;
    }

    FAssert(SyntacticEnvP(se));

    return(AsSyntacticEnv(se)->GlobalBindings);
}

static FObject SyntaxToDatum(FObject obj, FObject ht)
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
    {
        FObject val = EqHashtableRef(ht, obj, FalseObject);
        if (PairP(val))
            return(val);

        val = MakePair(LambdaSyntax, NoValueObject);
        EqHashtableSet(ht, obj, val);
        SetRest(val, MakePair(SyntaxToDatum(AsLambda(obj)->Bindings, ht),
                SyntaxToDatum(AsLambda(obj)->Body, ht)));

        return(val);
    }

    if (PairP(obj))
    {
        FObject val = EqHashtableRef(ht, obj, FalseObject);
        if (PairP(val))
            return(val);

        val = MakePair(NoValueObject, NoValueObject);
        EqHashtableSet(ht, obj, val);
        SetFirst(val, SyntaxToDatum(First(obj), ht));
        SetRest(val, SyntaxToDatum(Rest(obj), ht));

        return(val);
    }

    if (VectorP(obj))
    {
        FObject vec = EqHashtableRef(ht, obj, FalseObject);
        if (VectorP(vec))
            return(vec);

        vec = MakeVector(VectorLength(obj), 0, FalseObject);
        EqHashtableSet(ht, obj, vec);

        for (uint_t idx = 0; idx < VectorLength(vec); idx++)
        {
//            AsVector(vec)->Vector[idx] = SyntaxToDatum(AsVector(obj)->Vector[idx], ht);
            ModifyVector(vec, idx, SyntaxToDatum(AsVector(obj)->Vector[idx], ht));
        }

        return(vec);
    }

    return(obj);
}

FObject SyntaxToDatum(FObject obj)
{
    return(SyntaxToDatum(obj, MakeEqHashtable(31)));
}

static int_t SyntaxP(FObject obj)
{
    return(SpecialSyntaxP(obj) || SyntaxRulesP(obj));
}

static int_t SyntaxBindingP(FObject be, FObject var)
{
    if (BindingP(be))
    {
        if (SyntaxP(AsBinding(be)->Syntax))
            return(1);
    }
    else
    {
        FAssert(EnvironmentP(be));

        if (SyntaxP(EnvironmentGet(be, var)))
            return(1);
    }

    return(0);
}

static FObject SPassExpression(FObject enc, FObject se, FObject expr);
static FObject SPassBody(FObject enc, FObject se, FObject ss, FObject body);
static FObject SPassSequenceLast(FObject enc, FObject se, FObject ss, FObject form, FObject seq,
    FObject last);
static FObject SPassSequence(FObject enc, FObject se, FObject ss, FObject form, FObject seq);

static FObject AddFormal(FObject se, FObject ss, FObject bs, FObject id, FObject form, FObject ra)
{
    if (SymbolP(id))
        id = MakeIdentifier(id);

    if (IdentifierP(id) == 0)
        RaiseExceptionC(R.Syntax, SpecialSyntaxToName(ss),
                "expected a symbol or a list of symbols for formals", List(id, form));

    if (bs == EmptyListObject)
        return(MakePair(MakeBinding(se, id, ra), EmptyListObject));

    FObject lst = bs;
    for (;;)
    {
        FAssert(PairP(lst));
        FAssert(BindingP(First(lst)));

//        if (AsIdentifier(AsBinding(First(lst))->Identifier)->Symbol == AsIdentifier(id)->Symbol
//                && AsBinding(First(lst))->SyntacticEnv == se)
        if (IdentifierEqualP(AsBinding(First(lst))->Identifier, id))
            RaiseExceptionC(R.Syntax, SpecialSyntaxToName(ss), "duplicate identifier in formals",
                    List(First(lst), form));

        if (Rest(lst) == EmptyListObject)
            break;

        lst = Rest(lst);
    }

    FAssert(PairP(lst));
//    AsPair(lst)->Rest = MakePair(MakeBinding(se, id, ra), EmptyListObject);
    SetRest(lst, MakePair(MakeBinding(se, id, ra), EmptyListObject));

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
    if (SymbolP(id))
        id = MakeIdentifier(id);

    if (IdentifierP(id) == 0)
        RaiseExceptionC(R.Syntax, SpecialSyntaxToName(ss),
                "expected <variable> for each binding", List(id, form));

    if (bs == EmptyListObject)
        return(MakePair(MakeBinding(se, id, FalseObject), EmptyListObject));

    FObject lst = bs;
    for (;;)
    {
        FAssert(PairP(lst));
        FAssert(BindingP(First(lst)));

//        if (AsIdentifier(AsBinding(First(lst))->Identifier)->Symbol == AsIdentifier(id)->Symbol
//                && AsBinding(First(lst))->SyntacticEnv == se)
        if (IdentifierEqualP(AsBinding(First(lst))->Identifier, id))
            RaiseExceptionC(R.Syntax, SpecialSyntaxToName(ss), "duplicate identifier in bindings",
                    List(First(lst), form));

        if (Rest(lst) == EmptyListObject)
            break;

        lst = Rest(lst);
    }

    FAssert(PairP(lst));
//    AsPair(lst)->Rest = MakePair(MakeBinding(se, id, FalseObject), EmptyListObject);
    SetRest(lst, MakePair(MakeBinding(se, id, FalseObject), EmptyListObject));

    return(bs);
}

static FObject SPassLetVar(FObject se, FObject ss, FObject lb, FObject bs, FObject vi, int_t vf)
{
    if (vf != 0)
    {
        // (<formals> <init>)

        if (PairP(vi) == 0 || PairP(Rest(vi)) == 0 || Rest(Rest(vi)) != EmptyListObject)
            RaiseExceptionC(R.Syntax, SpecialSyntaxToName(ss),
                    "expected (<formals> <init>) for each binding", List(vi, lb));

        return(SPassFormals(se, ss, bs, First(vi), lb));
    }

    // (<variable> <init>)
    // (<keyword> <transformer>)

    if (PairP(vi) == 0 || PairP(Rest(vi)) == 0 || Rest(Rest(vi)) != EmptyListObject)
        RaiseExceptionC(R.Syntax, SpecialSyntaxToName(ss),
                "expected (<variable> <init>) for each binding", List(vi, lb));

    return(AddBinding(se, ss, bs, First(vi), lb));
}

static FObject SPassLetInit(FObject enc, FObject se, FObject ss, FObject lb, FObject nlb,
    FObject bd, FObject vi, int_t vf)
{
    FObject expr = SPassExpression(enc, se, First(Rest(vi)));
    if (ss == LetSyntaxSyntax || ss == LetrecSyntaxSyntax)
    {
        // (<keyword> <transformer>)

        FAssert(BindingP(bd));

        if (SyntaxP(expr) == 0)
            RaiseExceptionC(R.Syntax, SpecialSyntaxToName(ss), "expected a transformer",
                    List(First(Rest(vi)), lb));

//        AsBinding(bd)->Syntax = expr;
        Modify(FBinding, bd, Syntax, expr);

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

static int_t FormalsCount(FObject formals)
{
    int_t fc = 0;

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

        int_t fc = FormalsCount(First(First(lb)));
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

static FObject SPassLetBindings(FObject enc, FObject se, FObject ss, FObject lb, int_t rf,
    int_t vf)
{
    // ((<variable> <init>) ...)
    // ((<formals> <init>) ...)
    // ((<keyword> <transformer>) ...)

    FObject bs = EmptyListObject;
    FObject tlb;

    for (tlb = lb; PairP(tlb); tlb = Rest(tlb))
        bs = SPassLetVar(se, ss, lb, bs, First(tlb), vf);

    if (tlb != EmptyListObject)
        RaiseExceptionC(R.Syntax, SpecialSyntaxToName(ss), "expected a list of bindings",
                List(lb));

    if (rf != 0)
        EnterScopeList(bs);

    FObject nlb = EmptyListObject;
    FObject tbs = bs;

    if (vf != 0)
        tbs = GatherLetValuesFormals(tbs, lb);

    for (tlb = lb; PairP(tlb); tlb = Rest(tlb))
    {
        nlb = SPassLetInit(enc, se, ss, lb, nlb, First(tbs), First(tlb), vf);
        tbs = Rest(tbs);
    }

    FAssert(tbs == EmptyListObject);

    if (rf == 0)
        EnterScopeList(bs);

    return(ReverseListModify(nlb));
}

static FObject AddLetStarBinding(FObject se, FObject id, FObject form)
{
    if (SymbolP(id))
        id = MakeIdentifier(id);

    if (IdentifierP(id) == 0)
        RaiseExceptionC(R.Syntax, "let*", "expected (<variable> <init>) for each binding",
                List(id, form));

    return(MakeBinding(se, id, FalseObject));
}

static FObject SPassLetStarVarInit(FObject enc, FObject se, FObject ss, FObject lb, FObject nlb,
    FObject vi, int_t vf)
{
    // (<variable> <init>)
    // (<formals> <init>)

    if (PairP(vi) == 0 || PairP(Rest(vi)) == 0 || Rest(Rest(vi)) != EmptyListObject)
        RaiseExceptionC(R.Syntax, SpecialSyntaxToName(ss),
                "expected (<variable> <init>) for each binding", List(vi, lb));

    if (vf == 0)
    {
        FObject bd = AddLetStarBinding(se, First(vi), lb);
        nlb = MakePair(MakePair(MakePair(bd, EmptyListObject),
                MakePair(SPassExpression(enc, se, First(Rest(vi))), EmptyListObject)), nlb);

        EnterScope(bd);
    }
    else
    {
        FObject bs = SPassFormals(se, ss, EmptyListObject, First(vi), First(vi));
        nlb = MakePair(MakePair(bs, MakePair(SPassExpression(enc, se, First(Rest(vi))),
                EmptyListObject)), nlb);

        EnterScopeList(bs);
    }

    return(nlb);
}

static FObject SPassLetStarBindings(FObject enc, FObject se, FObject ss, FObject lb, int_t vf)
{
    // ((<variable> <init>) ...)
    // ((<formals> <init>) ...)

    FObject tlb;
    FObject nlb = EmptyListObject;

    for (tlb = lb; PairP(tlb); tlb = Rest(tlb))
        nlb = SPassLetStarVarInit(enc, se, ss, lb, nlb, First(tlb), vf);

    if (tlb != EmptyListObject)
        RaiseExceptionC(R.Syntax, SpecialSyntaxToName(ss), "expected a list of bindings",
                List(lb));

   return(ReverseListModify(nlb));
}

static FObject SPassNamedLetFormals(FObject se, FObject lb)
{
    // ((<variable> <init>) ...)

    FObject bs = EmptyListObject;
    FObject tlb;

    for (tlb = lb; PairP(tlb); tlb = Rest(tlb))
        bs = SPassLetVar(se, LetSyntax, lb, bs, First(tlb), 0);

    if (tlb != EmptyListObject)
        RaiseExceptionC(R.Syntax, "let", "expected a list of bindings", List(lb));

    return(bs);
}

static FObject SPassNamedLetInits(FObject enc, FObject se, FObject lb)
{
    FObject inits = EmptyListObject;

    while (lb != EmptyListObject)
    {
        FAssert(PairP(lb));
        FAssert(PairP(First(lb)));
        FAssert(PairP(Rest(First(lb))));

        inits = MakePair(SPassExpression(enc, se, First(Rest(First(lb)))), inits);
        lb = Rest(lb);
    }

    return(ReverseListModify(inits));
}

static FObject SPassNamedLet(FObject enc, FObject se, FObject tag, FObject expr)
{
    // (let <variable> ((<variable> <init>) ...) <body>)

    if (PairP(Rest(Rest(Rest(expr)))) == 0)
        RaiseExceptionC(R.Syntax, "let", "expected bindings followed by a body", List(expr));

    FObject tb = EnterScope(MakeBinding(se, tag, FalseObject));
    FObject bs = SPassNamedLetFormals(se, First(Rest(Rest(expr))));
    EnterScopeList(bs);

    // (let tag ((name val) ...) body1 body2 ...)
    // --> ((letrec ((tag (lambda (name ...) body1 body2 ...)))
    //         tag) val ...)

    FObject lambda = MakeLambda(enc, tag, bs, NoValueObject);
//    AsLambda(lambda)->Body = SPassBody(lambda, se, LetSyntax, Rest(Rest(Rest(expr))));
    Modify(FLambda, lambda, Body, SPassBody(lambda, se, LetSyntax, Rest(Rest(Rest(expr)))));

    LeaveScopeList(bs);
    LeaveScope(tb);

    return(List(LetrecValuesSyntax, List(List(List(tb), lambda)),
            MakePair(MakeReference(tb, tag),
            SPassNamedLetInits(enc, se, First(Rest(Rest(expr)))))));
}

static FObject SPassLetStarToLet(FObject lb, FObject body)
{
    if (lb == EmptyListObject)
        return(body);

    FAssert(PairP(lb));

    return(MakePair(MakePair(LetValuesSyntax, MakePair(MakePair(First(lb), EmptyListObject),
            SPassLetStarToLet(Rest(lb), body))), EmptyListObject));
}

static FObject SPassLetStarToLet(FObject lb, FObject body)
{
    if (lb == EmptyListObject)
        return(body);

    FAssert(PairP(lb));

    return(MakePair(MakePair(LetValuesSyntax, MakePair(MakePair(First(lb), EmptyListObject),
            SPassLetStarToLet(Rest(lb), body))), EmptyListObject));
}

static FObject SPassLet(FObject enc, FObject se, FObject ss, FObject expr, int_t rf, int_t sf,
    int_t vf)
{
    // rf : rec flag; eg. rf = 1 for letrec
    // sf : star flag; eg. sf = 1 for let*
    // vf : values flag; eg. vf = 1 for let-values

    if (PairP(Rest(expr)) == 0 || PairP(Rest(Rest(expr))) == 0)
        RaiseExceptionC(R.Syntax, SpecialSyntaxToName(ss),
                "expected bindings followed by a body", List(expr));

    FObject lb;
    if (rf == 0 && sf != 0)
        lb = SPassLetStarBindings(enc, se, ss, First(Rest(expr)), vf);
    else
        lb = SPassLetBindings(enc, se, ss, First(Rest(expr)), rf, vf);

    FObject body = SPassBody(enc, se, ss, Rest(Rest(expr)));
    FObject ret;

    if (ss == LetSyntaxSyntax || ss == LetrecSyntaxSyntax)
        ret = MakePair(BeginSyntax, SPassBody(enc, se, ss, Rest(Rest(expr))));
    else if (rf == 0 && sf != 0)
    {
        FAssert(PairP(lb));

        ret  = MakePair(LetValuesSyntax, MakePair(MakePair(First(lb), EmptyListObject),
                SPassLetStarToLet(Rest(lb), SPassBody(enc, se, ss, Rest(Rest(expr))))));
    }
    else
        ret = MakePair(ss == LetrecSyntax ? LetrecSyntax : LetValuesSyntax,
                MakePair(lb, SPassBody(enc, se, ss, Rest(Rest(expr)))));

    LeaveLetScope(lb);

    return(ret);
}

int_t MatchReference(FObject ref, FObject se, FObject expr)
{
    FAssert(ReferenceP(ref));

    if (SymbolP(expr))
    {
        if (AsIdentifier(AsReference(ref)->Identifier)->Symbol != expr)
            return(0);

        expr = MakeIdentifier(expr);
    }
    else
    {
        if (IdentifierP(expr) == 0)
            return(0);

        if (AsIdentifier(AsReference(ref)->Identifier)->Symbol != AsIdentifier(expr)->Symbol)
            return(0);
    }

    FObject be = ResolveIdentifier(se, expr);

    if (AsReference(ref)->Binding == be)
        return(1);

    if (EnvironmentP(be) == 0 || EnvironmentP(AsReference(ref)->Binding) == 0)
        return(0);

    return(EnvironmentGet(be, expr) == EnvironmentGet(AsReference(ref)->Binding,
            AsReference(ref)->Identifier));
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
            RaiseExceptionC(R.Syntax, "case", "duplicate datum", List(dtm, cse));

        if (Rest(lst) == EmptyListObject)
            break;

        lst = Rest(lst);
    }

    FAssert(PairP(lst));
//    AsPair(lst)->Rest = MakePair(dtm, EmptyListObject);
    SetRest(lst, MakePair(dtm, EmptyListObject));

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
        RaiseExceptionC(R.Syntax, "case", "expected a proper list of datum", List(cls));

    return(dtms);
}

static FObject SPassCaseClauses(FObject enc, FObject se, FObject clst, FObject cse)
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
            RaiseExceptionC(R.Syntax, "case", "expected a nonempty list for each clause",
                    List(cls, cse));

        if (Rest(clst) == EmptyListObject && MatchReference(R.ElseReference, se, First(cls)))
        {
            if (PairP(Rest(cls)) == 0)
                RaiseExceptionC(R.Syntax, "case",
                        "expected at least one expression following else", List(cls, cse));

            if (MatchReference(R.ArrowReference, se, First(Rest(cls))))
            {
                if (PairP(Rest(Rest(cls))) == 0 || Rest(Rest(Rest(cls))) != EmptyListObject)
                    RaiseExceptionC(R.Syntax, "case", "expected (else => <expression>)",
                            List(cls, cse));

                rlst = MakePair(MakePair(ElseSyntax, MakePair(ArrowSyntax,
                        MakePair(SPassExpression(enc, se, First(Rest(Rest(cls)))),
                        EmptyListObject))), rlst);
            }
            else
                rlst = MakePair(MakePair(ElseSyntax,
                        SPassSequence(enc, se, CaseSyntax, cse, Rest(cls))), rlst);
        }
        else
        {
            FObject dtml = SyntaxToDatum(First(cls));
            if (PairP(Rest(cls)) == 0 || PairP(dtml) == 0)
                RaiseExceptionC(R.Syntax, "case", "expected ((<datum> ...) <expression> ...)",
                        List(cls, cse));

            dtms = AddCaseDatumList(dtml, dtms, cse, cls);

            if (MatchReference(R.ArrowReference, se, First(Rest(cls))))
            {
                if (PairP(Rest(Rest(cls))) == 0 || Rest(Rest(Rest(cls))) != EmptyListObject)
                    RaiseExceptionC(R.Syntax, "case",
                            "expected ((<datum> ...) => <expression>)", List(cls, cse));

                rlst = MakePair(MakePair(dtml, MakePair(ArrowSyntax,
                        MakePair(SPassExpression(enc, se, First(Rest(Rest(cls)))),
                        EmptyListObject))), rlst);
            }
            else
                rlst = MakePair(MakePair(dtml, SPassSequence(enc, se, CaseSyntax, cse, Rest(cls))),
                        rlst);
        }

        clst = Rest(clst);
    }

    if (clst != EmptyListObject || rlst == EmptyListObject)
        RaiseExceptionC(R.Syntax, "case", "expected a list of clauses", List(cse));

    return(ReverseListModify(rlst));
}

FObject SPassDo(FObject enc, FObject se, FObject expr)
{
    // (do ((var init [step]) ...) (test expr ...) cmd ...)
    // --> ((letrec ((tag (lambda (var ...)
    //         (if test (begin expr ...)
    //                (begin cmd ... (tag step ...)))))) tag) init ...)

    if (PairP(Rest(expr)) == 0 || PairP(Rest(Rest(expr))) == 0
            || PairP(First(Rest(expr))) == 0 || PairP(First(Rest(Rest(expr)))) == 0)
        RaiseExceptionC(R.Syntax, "do",
                "expected (do ((<var> <init> [<step>]) ...) (<test> <expr> ...) <cmd> ...)",
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
            RaiseExceptionC(R.Syntax, "do", "expected (<var> <init> [<step>])", List(vis, expr));

        bs = AddBinding(se, DoSyntax, bs, First(vis), vis);
        inits = MakePair(SPassExpression(enc, se, First(Rest(vis))), inits);

        visl = Rest(visl);
    }

    if (visl != EmptyListObject)
        RaiseExceptionC(R.Syntax, "do",
                "expected a proper list of ((<var> <init> [<step>]) ...)", List(expr));

    inits = ReverseListModify(inits);

    FObject tag = MakeIdentifier(R.TagSymbol);
    FObject tb = MakeBinding(se, tag, FalseObject);

    EnterScopeList(bs);
    FObject lambda = MakeLambda(enc, First(expr), bs, NoValueObject);

    FObject stps = EmptyListObject;

    // Walk ((var init [step]) ...) list, expand steps.

    visl = First(Rest(expr));
    while (PairP(visl))
    {
        FObject vis = First(visl);

        if (PairP(Rest(Rest(vis))))
            stps = MakePair(SPassExpression(lambda, se, First(Rest(Rest(vis)))), stps);
        else
            stps = MakePair(SPassExpression(lambda, se, First(vis)), stps);

        visl = Rest(visl);
    }
    FAssert(visl == EmptyListObject);

    stps = ReverseListModify(stps);

    FObject tst = First(Rest(Rest(expr))); // (test expr ...)
    FAssert(PairP(tst));

    FObject ift = Rest(tst) == EmptyListObject ? FalseObject
            : MakePair(BeginSyntax, SPassSequence(lambda, se, DoSyntax, Rest(tst), Rest(tst)));

    FObject cmds = Rest(Rest(Rest(expr))); // cmd ...
    FObject cdo = MakePair(MakeReference(tb, tag), stps); // (tag step ...)

    FObject iff = cmds == EmptyListObject ? cdo
            : MakePair(BeginSyntax, SPassSequenceLast(lambda, se, DoSyntax, cmds, cmds,
            MakePair(cdo, EmptyListObject))); // (begin cmd ... (tag step ...))

//    AsLambda(lambda)->Body = MakePair(MakePair(IfSyntax,
//            MakePair(SPassExpression(enc, se, First(tst)), MakePair(ift,
//            MakePair(iff, EmptyListObject)))), EmptyListObject);
    Modify(FLambda, lambda, Body, MakePair(MakePair(IfSyntax,
            MakePair(SPassExpression(lambda, se, First(tst)), MakePair(ift,
            MakePair(iff, EmptyListObject)))), EmptyListObject));

    LeaveScopeList(bs);

    return(MakePair(MakePair(LetrecValuesSyntax, MakePair(
            MakePair(MakePair(MakePair(tb, EmptyListObject), MakePair(lambda, EmptyListObject)),
            EmptyListObject), MakePair(MakeReference(tb, tag), EmptyListObject))), inits));
}

static FObject SPassCaseLambda(FObject enc, FObject se, FObject expr, FObject clst)
{
    if (clst == EmptyListObject)
        return(EmptyListObject);

    if (PairP(clst) == 0 || PairP(First(clst)) == 0 || PairP(Rest(First(clst))) == 0)
        RaiseExceptionC(R.Syntax, "case-lambda",
                "expected (case-lambda (<formals> <body>) ...)", List(clst, expr));

    FObject cls = First(clst);
    return(MakePair(SPassLambda(enc, se, First(expr), First(cls), Rest(cls)),
            SPassCaseLambda(enc, se, expr, Rest(clst))));
}

static FObject SPassReadInclude(FObject expr, FObject ss)
{
    FAssert(ss == IncludeSyntax || ss == IncludeCISyntax);

    FObject ret = ReadInclude(First(expr), Rest(expr), ss == IncludeCISyntax);
    if (PairP(ret) == 0)
        RaiseExceptionC(R.Syntax, SpecialSyntaxToName(ss),
                "expected a proper list of one or more strings", List(expr));

    return(ret);
}

static FObject SPassQuasiquote(FObject enc, FObject se, FObject expr, FObject tpl, int_t dpth)
{
    if (PairP(tpl))
    {
        if (MatchReference(R.UnquoteReference, se, First(tpl)))
        {
            FAssert(dpth > 0);
            dpth -= 1;

            if (dpth == 0)
            {
                if (PairP(Rest(tpl)) == 0 || Rest(Rest(tpl)) != EmptyListObject)
                    RaiseExceptionC(R.Syntax, "unquote", "expected (unquote <expression>)",
                            List(tpl, expr));

                return(SPassExpression(enc, se, First(Rest(tpl))));
            }
        }
        else if (MatchReference(R.QuasiquoteReference, se, First(tpl)))
            dpth += 1;
        else if (MatchReference(R.UnquoteSplicingReference, se, First(tpl)))
        {
            FAssert(dpth > 0);

            dpth -= 1;
        }
        else if (dpth == 1 && PairP(First(tpl)) && MatchReference(R.UnquoteSplicingReference, se,
                First(First(tpl))))
        {
            FObject ftpl = First(tpl);

            if (PairP(Rest(ftpl)) == 0 || Rest(Rest(ftpl)) != EmptyListObject)
                RaiseExceptionC(R.Syntax, "unquote-splicing",
                        "expected (unquote-splicing <expression>)", List(ftpl, expr));

            FObject rst = SPassQuasiquote(enc, se, expr, Rest(tpl), dpth);
            if (rst == Rest(tpl))
                rst = MakePair(QuoteSyntax, MakePair(SyntaxToDatum(rst), EmptyListObject));

            return(MakePair(R.AppendReference, MakePair(SPassExpression(enc, se,
                    First(Rest(ftpl))), MakePair(rst, EmptyListObject))));
        }

        FObject fst = SPassQuasiquote(enc, se, expr, First(tpl), dpth);
        FObject rst = SPassQuasiquote(enc, se, expr, Rest(tpl), dpth);
        if (fst == First(tpl) && rst == Rest(tpl))
            return(tpl);

        if (fst == First(tpl))
            fst = MakePair(QuoteSyntax, MakePair(SyntaxToDatum(fst), EmptyListObject));
        else if (rst == Rest(tpl))
            rst = MakePair(QuoteSyntax, MakePair(SyntaxToDatum(rst), EmptyListObject));

        return(MakePair(R.ConsReference, MakePair(fst, MakePair(rst, EmptyListObject))));
    }
    else if (VectorP(tpl))
    {
        FObject ltpl = VectorToList(tpl);
        FObject ret = SPassQuasiquote(enc, se, expr, ltpl, dpth);
        if (ret == ltpl)
            return(tpl);

        FAssert(PairP(ret));

        return(MakePair(R.ListToVectorReference, MakePair(ret, EmptyListObject)));
    }

    return(tpl);
}

static FObject SPassSpecialSyntax(FObject enc, FObject se, FObject ss, FObject expr)
{
    FAssert(SpecialSyntaxP(ss));

    if (ss == QuoteSyntax)
    {
        // (quote <datum>)

        if (PairP(Rest(expr)) == 0 || Rest(Rest(expr)) != EmptyListObject)
            RaiseExceptionC(R.Syntax, "quote", "expected (quote <datum>)", List(expr));

        return(MakePair(QuoteSyntax, MakePair(SyntaxToDatum(First(Rest(expr))), EmptyListObject)));
    }
    else if (ss == LambdaSyntax)
    {
        // (lambda (<variable> ...) <body>)
        // (lambda <variable> <body>)
        // (lambda (<variable> <variable> ... . <variable>) <body>)

        if (PairP(Rest(expr)) == 0 || PairP(Rest(Rest(expr))) == 0)
            RaiseExceptionC(R.Syntax, "lambda", "expected (lambda <formals> <body>)",
                    List(expr));

        return(SPassLambda(enc, se, First(expr), First(Rest(expr)), Rest(Rest(expr))));
    }
    else if (ss == IfSyntax)
    {
        // (if <test> <consequent> <alternate>)
        // (if <test> <consequent>)

        if (PairP(Rest(expr)) == 0 || PairP(Rest(Rest(expr))) == 0
                || (Rest(Rest(Rest(expr))) != EmptyListObject
                    && (PairP(Rest(Rest(Rest(expr)))) == 0
                    || Rest(Rest(Rest(Rest(expr)))) != EmptyListObject)))
            RaiseExceptionC(R.Syntax, "if", "expected (if <test> <consequent> [<alternate>])",
                    List(expr));

        return(MakePair(IfSyntax, MakePair(SPassExpression(enc, se, First(Rest(expr))),
                MakePair(SPassExpression(enc, se, First(Rest(Rest(expr)))),
                Rest(Rest(Rest(expr))) == EmptyListObject ? EmptyListObject
                : MakePair(SPassExpression(enc, se, First(Rest(Rest(Rest(expr))))),
                EmptyListObject)))));
    }
    else if (ss == SetBangSyntax)
    {
        // (set! <variable> <expression>)

        if (PairP(Rest(expr)) == 0 || PairP(Rest(Rest(expr))) == 0
                || Rest(Rest(Rest(expr))) != EmptyListObject)
            RaiseExceptionC(R.Syntax, "set!", "expected (set! <variable> <expression>)",
                    List(expr));

        FObject var = First(Rest(expr));
        if (SymbolP(var))
            var = MakeIdentifier(var);
        if (IdentifierP(var) == 0)
            RaiseExceptionC(R.Syntax, "set!", "variable expected", List(var, expr));

        FObject be = ResolveIdentifier(se, var);
        if (SyntaxBindingP(be, var))
            RaiseExceptionC(R.Syntax, "set!", "variable already bound to syntax",
                    List(var, expr));

        if (EnvironmentP(be) && AsEnvironment(be)->Interactive == FalseObject)
        {
            FObject gl = EnvironmentBind(be, AsIdentifier(var)->Symbol);

            FAssert(GlobalP(gl));

            if (AsGlobal(gl)->State == GlobalImported
                    || AsGlobal(gl)->State == GlobalImportedModified)
                RaiseExceptionC(R.Syntax, "set!",
                        "imported variables may not be modified in libraries",
                        List(var, expr));
        }

        return(MakePair(SetBangSyntax, MakePair(MakeReference(be, var),
                MakePair(SPassExpression(enc, se, First(Rest(Rest(expr)))), EmptyListObject))));
    }
    else if (ss == SetBangValuesSyntax)
    {
        // (set!-values (<variable> ...) <expression>)

        if (PairP(Rest(expr)) == 0 || PairP(Rest(Rest(expr))) == 0
                || Rest(Rest(Rest(expr))) != EmptyListObject)
            RaiseExceptionC(R.Syntax, "set!-values",
                    "expected (set!-values (<variable> ...) <expression>)", List(expr));

        FObject lst = First(Rest(expr));
        FObject blst = EmptyListObject;
        while (PairP(lst))
        {
            FObject var = First(lst);

            if (SymbolP(var))
                var = MakeIdentifier(var);
            if (IdentifierP(var) == 0)
                RaiseExceptionC(R.Syntax, "set!-values", "variable expected", List(var, expr));

            FObject be = ResolveIdentifier(se, var);
            if (SyntaxBindingP(be, var))
                RaiseExceptionC(R.Syntax, "set!-values", "variable already bound to syntax",
                        List(var, expr));

            if (EnvironmentP(be) && AsEnvironment(be)->Interactive == FalseObject)
            {
                FObject gl = EnvironmentBind(be, AsIdentifier(var)->Symbol);

                FAssert(GlobalP(gl));

                if (AsGlobal(gl)->State == GlobalImported
                        || AsGlobal(gl)->State == GlobalImportedModified)
                    RaiseExceptionC(R.Syntax, "set!-values",
                            "imported variables may not be modified in libraries",
                            List(var, expr));
            }

            blst = MakePair(MakeReference(be, var), blst);
            lst = Rest(lst);
        }

        return(MakePair(SetBangValuesSyntax, MakePair(ReverseListModify(blst),
                MakePair(SPassExpression(enc, se, First(Rest(Rest(expr)))), EmptyListObject))));
    }
    else if (ss == LetSyntax)
    {
        // (let ((<variable> <init>) ...) <body>)
        // (let <variable> ((<variable> <init>) ...) <body>)

        if (PairP(Rest(expr)) == 0)
            RaiseExceptionC(R.Syntax, "let", "expected bindings followed by a body",
                    List(expr));

        FObject tag = First(Rest(expr));
        if (SymbolP(tag))
            tag = MakeIdentifier(tag);
        if (IdentifierP(tag))
            return(SPassNamedLet(enc, se, tag, expr));

        return(SPassLet(enc, se, ss, expr, 0, 0, 0));
    }
    else if (ss == LetStarSyntax)
    {
        // (let* ((<variable> <init>) ...) <body>)

        return(SPassLet(enc, se, ss, expr, 0, 1, 0));
    }
    else if (ss == LetrecSyntax)
    {
        // (letrec ((<variable> <init>) ...) <body>)

        return(SPassLet(enc, se, ss, expr, 1, 0, 0));
    }
    else if (ss == LetrecStarSyntax)
    {
        // (letrec* ((<variable> <init>) ...) <body>)

        return(SPassLet(enc, se, ss, expr, 1, 1, 0));
    }
    else if (ss == LetValuesSyntax)
    {
        // (let-values ((<formals> <init>) ...) <body>)

        return(SPassLet(enc, se, ss, expr, 0, 0, 1));
    }
    else if (ss == LetStarValuesSyntax)
    {
        // (let*-values ((<formals> <init>) ...) <body>)

        return(SPassLet(enc, se, ss, expr, 0, 1, 1));
    }
    else if (ss == LetrecValuesSyntax)
    {
        // (letrec-values ((<formals> <init>) ...) <body>)

        return(SPassLet(enc, se, ss, expr, 1, 0, 1));
    }
    else if (ss == LetrecStarValuesSyntax)
    {
        // (letrec*-values ((<formals> <init>) ...) <body>)

        return(SPassLet(enc, se, ss, expr, 1, 1, 1));
    }
    else if (ss == LetSyntaxSyntax)
    {
        // (let-syntax ((<keyword> <transformer>) ...) <body>)

        return(SPassLet(enc, se, ss, expr, 0, 0, 0));
    }
    else if (ss == LetrecSyntaxSyntax)
    {
        // (letrec-syntax ((<keyword> <transformer>) ...) <body>)

        return(SPassLet(enc, se, ss, expr, 1, 0, 0));
    }
    else if (ss == CaseSyntax)
    {
        // (case <key> <clause> ...)

        if (PairP(Rest(expr)) == 0)
            RaiseExceptionC(R.Syntax, "case", "expected (case <test> <clause> ...)",
                    List(expr));

        return(MakePair(CaseSyntax, MakePair(SPassExpression(enc, se, First(Rest(expr))),
                SPassCaseClauses(enc, se, Rest(Rest(expr)), expr))));
    }
    else if (ss == OrSyntax)
    {
        // (or <test> ...)

        if (Rest(expr) == EmptyListObject)
            return(FalseObject);

        if (PairP(Rest(expr)) == 0)
            RaiseExceptionC(R.Syntax, "or", "expected (or <test> ...)", List(expr));

        if (Rest(Rest(expr)) == EmptyListObject)
            return(SPassExpression(enc, se, First(Rest(expr))));

        return(MakePair(OrSyntax, SPassSequence(enc, se, OrSyntax, expr, Rest(expr))));
    }
    else if (ss == BeginSyntax)
    {
        // (begin <expression> ...)

        if (PairP(Rest(expr)) == 0)
            RaiseExceptionC(R.Syntax, "begin", "expected (begin <expression> ...)",
                    List(expr));

        return(MakePair(BeginSyntax, SPassSequence(enc, se, BeginSyntax, expr, Rest(expr))));
    }
    else if (ss == DoSyntax)
    {
        // (do ((<variable> <init> [<step>]) ...) (<test> <expression> ...) <command> ...)

        return(SPassDo(enc, se, expr));
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
            RaiseExceptionC(R.Syntax, "syntax-error",
                    "expected (syntax-error <message> <args> ...)", List(expr));

        if (StringP(First(Rest(expr))) == 0)
            RaiseExceptionC(R.Syntax, "syntax-error",
                    "expected a string for the <message>", List(First(Rest(expr)), expr));

        char msg[128];
        StringToC(First(Rest(expr)), msg, sizeof(msg));

        RaiseExceptionC(R.Syntax, "syntax-error", msg, Rest(Rest(expr)));
    }
    else if (ss == IncludeSyntax || ss == IncludeCISyntax)
    {
        // (include <string> ...)
        // (include-ci <string> ...)

        return(SPassExpression(enc, se, MakePair(BeginSyntax, SPassReadInclude(expr, ss))));
    }
    else if (ss == CondExpandSyntax)
    {
        // (cond-expand <ce-clause> ...)

        FObject ce = CondExpand(se, expr, Rest(expr));
        if (ce == EmptyListObject)
            return(NoValueObject);
        return(SPassExpression(enc, se, MakePair(BeginSyntax, ce)));
    }
    else if (ss == CaseLambdaSyntax)
    {
        // (case-lambda (<formals> <body>) ...)

        if (PairP(Rest(expr)) == 0)
            RaiseExceptionC(R.Syntax, "case-lambda",
                    "expected (case-lambda (<formals> <body>) ...)", List(expr));

        return(MakeCaseLambda(SPassCaseLambda(enc, se, expr, Rest(expr))));
    }
    else if (ss == QuasiquoteSyntax)
    {
        // (quasiquote <qq-template>)

        if (PairP(Rest(expr)) == 0 || Rest(Rest(expr)) != EmptyListObject)
            RaiseExceptionC(R.Syntax, "quasiquote", "expected (quasiquote <qq-template>)",
                    List(expr));

        FObject obj = SPassQuasiquote(enc, se, expr, First(Rest(expr)), 1);
        if (obj == First(Rest(expr)))
            return(MakePair(QuoteSyntax, MakePair(SyntaxToDatum(obj), EmptyListObject)));
        return(obj);
    }
    else
        RaiseExceptionC(R.Syntax, SpecialSyntaxToName(ss), "syntax not allowed here",
                List(expr));

    return(expr);
}

static FObject SPassOperands(FObject enc, FObject se, FObject opds, FObject form)
{
    if (opds == EmptyListObject)
        return(EmptyListObject);

    if (PairP(opds) == 0)
        RaiseExceptionC(R.Syntax, "procedure-call", "expected list of operands", List(opds, form));

    return(MakePair(SPassExpression(enc, se, First(opds)),
            SPassOperands(enc, se, Rest(opds), form)));
}

static FObject SPassKeyword(FObject se, FObject expr)
{
    if (SymbolP(expr))
        expr = MakeIdentifier(expr);
    if (IdentifierP(expr))
        return(MakeReference(ResolveIdentifier(se, expr), expr));

    return(expr);
}

static FObject SPassExpression(FObject enc, FObject se, FObject expr)
{
    if (SymbolP(expr))
        expr = MakeIdentifier(expr);

    if (IdentifierP(expr))
    {
        FObject be = ResolveIdentifier(se, expr);
        if (SyntaxBindingP(be, expr))
            RaiseExceptionC(R.Syntax, "variable", "bound to syntax", List(expr));

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

            val = EnvironmentGet(AsReference(op)->Binding, AsReference(op)->Identifier);
        }

        if (SpecialSyntaxP(val))
            return(SPassSpecialSyntax(enc, se, val, expr));

        if (SyntaxRulesP(val))
            return(SPassExpression(enc, se, ExpandSyntaxRules(se, val, Rest(expr))));
//            return(SPassExpression(enc, se, ExpandSyntaxRules(MakeSyntacticEnv(se), val, Rest(expr))));

        // Other macro transformers would go here.
    }

    // Procedure Call
    // (<operator> <operand> ...)

    return(MakePair(SPassExpression(enc, se, First(expr)),
            SPassOperands(enc, se, Rest(expr), expr)));
}

static FObject SPassSequenceLast(FObject enc, FObject se, FObject ss, FObject form, FObject seq,
    FObject last)
{
    if (seq == EmptyListObject)
        return(last);

    if (PairP(seq) == 0)
        RaiseExceptionC(R.Syntax, SpecialSyntaxToName(ss), "expected list of expressions",
                List(seq, form));

    return(MakePair(SPassExpression(enc, se, First(seq)),
            SPassSequenceLast(enc, se, ss, form, Rest(seq), last)));
}

static FObject SPassSequence(FObject enc, FObject se, FObject ss, FObject form, FObject seq)
{
    return(SPassSequenceLast(enc, se, ss, form, seq, EmptyListObject));
}

static FObject GatherVariablesAndSyntax(FObject enc, FObject se, FObject dlst, FObject bs)
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

            int_t fc = FormalsCount(First(Rest(expr)));
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

            FObject trans = SPassExpression(enc, se, First(Rest(Rest(expr))));
            if (SyntaxP(trans) == 0)
                RaiseExceptionC(R.Syntax, "define-syntax", "expected a transformer",
                        List(First(Rest(Rest(expr))), expr));

//            AsBinding(First(bs))->Syntax = trans;
            Modify(FBinding, First(bs), Syntax, trans);
            bs = Rest(bs);
        }

        dlst = Rest(dlst);
    }

    FAssert(dlst == EmptyListObject);
    FAssert(bs == EmptyListObject);

    return(ReverseListModify(bl));
}

static FObject VariablesAndExpandInits(FObject enc, FObject se, FObject dlst, FObject bl)
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
                        SPassLambda(enc, se, First(First(Rest(expr))), Rest(First(Rest(expr))),
                                Rest(Rest(expr))),
                        EmptyListObject)), lb);
            }
            else
            {
                // (define <variable> <expression>)

                FAssert(PairP(Rest(Rest(expr))));
                FAssert(Rest(Rest(Rest(expr))) == EmptyListObject);

                lb = MakePair(MakePair(First(bl),
                        MakePair(SPassExpression(enc, se, First(Rest(Rest(expr)))),
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
                    MakePair(SPassExpression(enc, se, First(Rest(Rest(expr)))), EmptyListObject)),
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

    if (SymbolP(op))
        op = MakeIdentifier(op);

    if (IdentifierP(op))
    {
        FObject be = ResolveIdentifier(se, op);

        FObject val;
        if (BindingP(be))
            val = AsBinding(be)->Syntax;
        else
        {
            FAssert(EnvironmentP(be));

            val = EnvironmentGet(be, op);
        }

        if (SpecialSyntaxP(val))
        {
            if (val == IncludeSyntax || val == IncludeCISyntax)
                return(MakePair(val, expr));
            return(MakePair(val, Rest(expr)));
        }

        if (SyntaxRulesP(val))
            return(SPassBodyExpression(se, ExpandSyntaxRules(se, val, Rest(expr))));
//            return(SPassBodyExpression(se, ExpandSyntaxRules(MakeSyntacticEnv(se), val,
//                    Rest(expr))));

        // Other macro transformers would go here.
    }

    return(expr);
}

static FObject AppendBegin(FObject ss, FObject begin, FObject body, FObject form)
{
    if (begin == EmptyListObject)
        return(body);

    if (PairP(begin) == 0)
        RaiseExceptionC(R.Syntax, SpecialSyntaxToName(ss), "begin must be a proper list",
                List(begin, form));

    return(MakePair(First(begin), AppendBegin(ss, Rest(begin), body, form)));
}

static FObject SPassBody(FObject enc, FObject se, FObject ss, FObject body)
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

        if (First(expr) == IncludeSyntax || First(expr) == IncludeCISyntax)
        {
            // (include <string> ...)
            // (include-ci <string> ...)

            body = AppendBegin(ss, SPassReadInclude(Rest(expr), First(expr)), Rest(body), expr);
        }
        else if (First(expr) == BeginSyntax)
        {
            // (begin ...)

            body = AppendBegin(ss, Rest(expr), Rest(body), expr);
        }
        else if (First(expr) == CondExpandSyntax)
        {
            // (cond-expand <ce-clause> ...)

            FObject ce = CondExpand(se, expr, Rest(expr));
            if (ce != EmptyListObject)
                body = AppendBegin(ss, ce, Rest(body), expr);
            else
                body = Rest(body);
        }
        else if (First(expr) == DefineSyntax)
        {
            // (define <variable> <expression>)
            // (define (<variable> <formal> ...) <body)
            // (define (<variable> . <formal>) <body>)

            if (PairP(Rest(expr)) == 0)
                RaiseExceptionC(R.Syntax, "define",
                    "expected (define (<variable> ...) <body>) or (define <variable> <expr>)",
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
                    RaiseExceptionC(R.Syntax, "define",
                            "expected (define <variable> <expression>)", List(expr));

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
                RaiseExceptionC(R.Syntax, "define-values",
                        "expected (define-values (<formal> ...) <expression>)", List(expr));

            bs = SPassFormals(se, DefineValuesSyntax, bs, First(Rest(expr)), expr);
            dlst = MakePair(expr, dlst);

            body = Rest(body);
        }
        else if (First(expr) == DefineSyntaxSyntax)
        {
            // (define-syntax <keyword> <transformer>)

            if (PairP(Rest(expr)) == 0 || PairP(Rest(Rest(expr))) == 0
                    || Rest(Rest(Rest(expr))) != EmptyListObject)
                RaiseExceptionC(R.Syntax, "define-syntax",
                        "expected (define-syntax <keyword> <transformer>)", List(expr));

            bs = AddBinding(se, DefineSyntaxSyntax, bs, First(Rest(expr)), expr);
            dlst = MakePair(expr, dlst);

            body = Rest(body);
        }
        else
            break;
    }

    if (PairP(body) == 0)
        RaiseExceptionC(R.Syntax, SpecialSyntaxToName(ss),
                "body must have at least one expression", List(body));

    if (dlst == EmptyListObject)
        return(SPassSequence(enc, se, ss, body, body));

    dlst = ReverseListModify(dlst);

    // Pass 2: evaluate and assign transformers to all keywords specified by define-syntax;
    // collect bindings together for the same define or define-values ready to be paired with
    // an init expression in pass 3.

    EnterScopeList(bs);
    FObject bl = GatherVariablesAndSyntax(enc, se, dlst, bs);

    FObject ret;
    if (bl == EmptyListObject)
        ret = SPassSequence(enc, se, ss, body, body);
    else
    {
        // Pass 3: expand inits and pair with bindings

        FObject lb = VariablesAndExpandInits(enc, se, dlst, bl);
        ret = MakePair(MakePair(LetValuesSyntax,
                MakePair(lb, SPassSequence(enc, se, ss, body, body))), EmptyListObject);
    }

    LeaveScopeList(bs);
    return(ret);
}

FObject SPassLambda(FObject enc, FObject se, FObject name, FObject formals, FObject body)
{
    FAssert(SyntacticEnvP(se));

    FObject bs = SPassFormals(se, LambdaSyntax, EmptyListObject, formals, formals);
    EnterScopeList(bs);
    FObject lambda = MakeLambda(enc, name, bs, NoValueObject);
//    AsLambda(lambda)->Body = SPassBody(lambda, se, LambdaSyntax, body);
    Modify(FLambda, lambda, Body, SPassBody(lambda, se, LambdaSyntax, body));
    LeaveScopeList(bs);

    return(lambda);
}

FObject ExpandExpression(FObject enc, FObject se, FObject expr)
{
    return(SPassExpression(enc, se, expr));
}

// ----------------

static int_t EvaluateFeatureRequirement(FObject se, FObject expr, FObject cls, FObject obj)
{
    if (IdentifierP(obj))
        obj = AsIdentifier(obj)->Symbol;

    if (SymbolP(obj))
    {
        FObject lst = R.Features;
        while (PairP(lst))
        {
            FAssert(SymbolP(First(lst)));

            if (First(lst) == obj)
                return(1);

            lst = Rest(lst);
        }

        FAssert(lst == EmptyListObject);

        return(0);
    }

    if (PairP(obj) == 0 || (IdentifierP(First(obj)) == 0 && SymbolP(First(obj)) == 0))
        RaiseExceptionC(R.Syntax, "cond-expand", "invalid feature requirement syntax",
                List(obj, cls, expr));

    if (MatchReference(R.LibraryReference, se, First(obj)))
    {
        if (PairP(Rest(obj)) == 0 || Rest(Rest(obj)) != EmptyListObject)
            RaiseExceptionC(R.Syntax, "cond-expand", "expected (library <library name>)",
                    List(obj, cls, expr));

        FObject nam = LibraryName(First(Rest(obj)));
        if (PairP(nam) == 0)
            RaiseExceptionC(R.Syntax, "cond-expand",
                    "library name must be a list of symbols and/or integers", List(obj));

        return(LibraryP(FindOrLoadLibrary(nam)));
    }
    else if (MatchReference(R.AndReference, se, First(obj)))
    {
        FObject lst = Rest(obj);

        while (PairP(lst))
        {
            if (EvaluateFeatureRequirement(se, expr, cls, First(lst)) == 0)
                return(0);

            lst = Rest(lst);
        }

        if (lst != EmptyListObject)
            RaiseExceptionC(R.Syntax, "cond-expand",
                    "expected a proper list of feature requirements", List(obj, cls, expr));

        return(1);
    }
    else if (MatchReference(R.OrReference, se, First(obj)))
    {
        FObject lst = Rest(obj);

        while (PairP(lst))
        {
            if (EvaluateFeatureRequirement(se, expr, cls, First(lst)))
                return(1);

            lst = Rest(lst);
        }

        if (lst != EmptyListObject)
            RaiseExceptionC(R.Syntax, "cond-expand",
                    "expected a proper list of feature requirements", List(obj, cls, expr));

        return(0);
    }
    else if (MatchReference(R.NotReference, se, First(obj)))
    {
        if (PairP(Rest(obj)) == 0 || Rest(Rest(obj)) != EmptyListObject)
            RaiseExceptionC(R.Syntax, "cond-expand", "expected (not <feature requirement>)",
                    List(obj, cls, expr));

        return(EvaluateFeatureRequirement(se, expr, cls, First(Rest(obj))) == 0);
    }
    else
        RaiseExceptionC(R.Syntax, "cond-expand", "invalid feature requirement syntax",
                List(obj, cls, expr));

    return(0);
}

FObject CondExpand(FObject se, FObject expr, FObject clst)
{
    while (PairP(clst))
    {
        FObject cls = First(clst);

        if (PairP(cls) == 0)
            RaiseExceptionC(R.Syntax, "cond-expand",
                    "expected (<feature requirement> <expression> ...) for each clause",
                    List(clst, expr));

        if ((IdentifierP(First(cls)) || SymbolP(First(cls)))
                && MatchReference(R.ElseReference, se, First(cls)))
        {
            if (Rest(clst) != EmptyListObject)
                RaiseExceptionC(R.Syntax, "cond-expand",
                        "(else <expression> ..) must be the last clause", List(cls, clst, expr));

            return(Rest(cls));
        }

        if (EvaluateFeatureRequirement(se, expr, cls, First(cls)))
            return(Rest(cls));

        clst = Rest(clst);
    }

    if (clst != EmptyListObject)
        RaiseExceptionC(R.Syntax, "cond-expand", "expected a proper list of clauses",
                List(expr));

    RaiseExceptionC(R.Syntax, "cond-expand", "no clauses match", List(expr));

    // Never reached.
    return(NoValueObject);
}

// ----------------

static FObject MungeIncludeName(FObject source, FObject target)
{
    FAssert(StringP(source));
    FAssert(StringP(target));

    FCh * src = AsString(source)->String;
    uint_t srclen = StringLength(source);
    FCh * tgt = AsString(target)->String;
    uint_t tgtlen = StringLength(target);
    uint_t idx;

    if (PathChP(tgt[0]))
        return(target);

    for (idx = srclen; idx > 0; idx--)
    {
        if (PathChP(src[idx - 1]))
            break;
    }

    if (idx == 0)
        return(target);

    FObject s = MakeStringCh(idx + tgtlen, 0);
    memcpy(AsString(s)->String, src, idx * sizeof(FCh));
    memcpy(AsString(s)->String + idx, tgt, tgtlen * sizeof(FCh));
    return(s);
}

FObject ReadInclude(FObject op, FObject lst, int_t cif)
{
    FAssert(SymbolP(op) || IdentifierP(op));

    if (PairP(lst) == 0)
        return(EmptyListObject);

    FDontWait dw;
    FObject ret = EmptyListObject;
    FObject src = IdentifierP(op) ? AsIdentifier(op)->Filename : NoValueObject;

    while (PairP(lst))
    {
        if (StringP(First(lst)) == 0)
            return(EmptyListObject);

        FObject port = OpenInputFile(MungeIncludeName(src, First(lst)));
        if (TextualPortP(port) == 0)
            RaiseExceptionC(R.Assertion, "open-input-file", "can not open file for reading",
                    List(First(lst)));

        FoldcasePort(port, cif);
        WantIdentifiersPort(port, 1);

        for (;;)
        {
            FObject obj = Read(port);
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
