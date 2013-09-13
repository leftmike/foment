/*

Foment

*/

#include "foment.hpp"
#include "compile.hpp"

// ---- Common Middle Pass ----

typedef void (*FLambdaFormalFn)(FLambda * lam, FBinding * bd);
typedef void (*FLambdaEnclosingFn)(FLambda * enc, FLambda * lam, int cf);
typedef void (*FLambdaBeforeFn)(FLambda * enc, FLambda * lam, int cf);
typedef void (*FLambdaAfterFn)(FLambda * lam, int cf);
typedef void (*FCaseLambdaFn)(FLambda * enc, FCaseLambda * cl, int cf);
typedef void (*FReferenceFn)(FLambda * lam, FObject pair, FReference * ref, int cf);
typedef void (*FSetReferenceFn)(FLambda * lam, FReference * ref);
typedef void (*FLetFormalFn)(FLambda * lam, FBinding * bd);
typedef int (*FLetFormalsInitFn)(FLambda * lam, FObject flst, FObject init);
typedef void (*FSpecialSyntaxFn)(FLambda * lam, FObject expr);
typedef void (*FProcedureCallFn)(FLambda * lam, FObject expr);

typedef struct
{
    FObject MiddlePass;
    FLambdaFormalFn LambdaFormalFn;
    FLambdaEnclosingFn LambdaEnclosingFn;
    FLambdaBeforeFn LambdaBeforeFn;
    FLambdaAfterFn LambdaAfterFn;
    FCaseLambdaFn CaseLambdaFn;
    FReferenceFn ReferenceFn;
    FSetReferenceFn SetReferenceFn;
    FLetFormalFn LetFormalFn;
    FLetFormalsInitFn LetFormalsInitFn;
    FSpecialSyntaxFn SpecialSyntaxFn;
    FProcedureCallFn ProcedureCallFn;
} FMiddlePass;

static void MPassSequence(FMiddlePass * mp, FLambda * lam, FObject seq);
static void MPassExpression(FMiddlePass * mp, FLambda * lam, FObject pair, int cf);
static void MPassLambda(FMiddlePass * mp, FLambda * enc, FLambda * lam, int cf);

static void MPassCase(FMiddlePass * mp, FLambda * lam, FObject clst)
{
    // (case <key> <clause> ...)

    while (PairP(clst))
    {
        FObject cls = First(clst);
        FAssert(PairP(cls));
        FAssert(PairP(Rest(cls)));

        if (First(cls) == ElseSyntax)
        {
            if (First(Rest(cls)) == ArrowSyntax)
            {
                // (else => <expression>)

                FAssert(PairP(Rest(Rest(cls))));
                FAssert(Rest(Rest(Rest(cls))) == EmptyListObject);

                MPassExpression(mp, lam, Rest(Rest(cls)), 1);
            }
            else
            {
                // (else <expression> ...)

                MPassSequence(mp, lam, Rest(cls));
            }
        }
        else if (First(Rest(cls)) == ArrowSyntax)
        {
            // ((<datum> ...) => <expression>)

            MPassExpression(mp, lam, Rest(Rest(cls)), 1);
        }
        else
        {
            // ((<datum> ...) <expression> ...)

            MPassSequence(mp, lam, Rest(cls));
        }

        clst = Rest(clst);
    }

    FAssert(clst == EmptyListObject);
}

static void MPassLetFormals(FMiddlePass * mp, FLambda * lam, FObject lb)
{
    while (PairP(lb))
    {
        FObject vi = First(lb);

        FAssert(PairP(vi));
        FAssert(PairP(First(vi)));
        FAssert(PairP(Rest(vi)));
        FAssert(Rest(Rest(vi)) == EmptyListObject);

        FObject flst = First(vi);
        while (PairP(flst))
        {
            FObject bd = First(flst);
            FAssert(BindingP(bd));

            mp->LetFormalFn(lam, AsBinding(bd));
            flst = Rest(flst);
        }

        FAssert(flst == EmptyListObject);

        MPassExpression(mp, lam, Rest(vi), 0);
        lb = Rest(lb);
    }

    FAssert(lb == EmptyListObject);
}

static FObject MPassLetFormalsInits(FMiddlePass * mp, FLambda * lam, FObject lb)
{
    FObject ret = lb;
    FObject plb = NoValueObject;

    while (PairP(lb))
    {
        FObject vi = First(lb);

        FAssert(PairP(vi));
        FAssert(PairP(First(vi)));
        FAssert(PairP(Rest(vi)));
        FAssert(Rest(Rest(vi)) == EmptyListObject);

        if (mp->LetFormalsInitFn(lam, First(vi), First(Rest(vi))) == 0)
        {
            if (PairP(plb))
            {
//                AsPair(plb)->Rest = Rest(lb);
                SetRest(plb, Rest(lb));
            }
            else
                ret = Rest(lb);
        }
        else
            plb = lb;

        lb = Rest(lb);
    }

    FAssert(lb == EmptyListObject);

    return(ret);
}

static void MPassLetInits(FMiddlePass * mp, FLambda * lam, FObject lb)
{
    while (PairP(lb))
    {
        FObject vi = First(lb);

        FAssert(PairP(vi));
        FAssert(PairP(First(vi)));
        FAssert(PairP(Rest(vi)));
        FAssert(Rest(Rest(vi)) == EmptyListObject);

        MPassExpression(mp, lam, Rest(vi), 0);
        lb = Rest(lb);
    }

    FAssert(lb == EmptyListObject);
}

static void MPassCaseLambda(FMiddlePass * mp, FLambda * enc, FCaseLambda * cl, int cf)
{
    if (mp->CaseLambdaFn != 0)
        mp->CaseLambdaFn(enc, cl, cf);

    FObject cases = cl->Cases;

    while (PairP(cases))
    {
        FAssert(LambdaP(First(cases)));

        MPassLambda(mp, enc, AsLambda(First(cases)), cf);
        cases = Rest(cases);
    }

    FAssert(cases == EmptyListObject);
}

static void MPassSpecialSyntax(FMiddlePass * mp, FLambda * lam, FObject expr, int cf)
{
    FObject ss = First(expr);

    FAssert(ss == QuoteSyntax || ss == IfSyntax || ss == SetBangSyntax
            || ss == LetStarValuesSyntax || ss == LetrecSyntax
            || ss == CaseSyntax || ss == OrSyntax || ss == BeginSyntax);

    if (ss == IfSyntax)
    {
        // (if <test> <consequent> <alternate>)
        // (if <test> <consequent>)

        FAssert(PairP(Rest(expr)));
        FAssert(PairP(Rest(Rest(expr))));

        MPassExpression(mp, lam, Rest(expr), 0);
        MPassExpression(mp, lam, Rest(Rest(expr)), 0);

        if (PairP(Rest(Rest(Rest(expr)))))
        {
            FAssert(Rest(Rest(Rest(Rest(expr)))) == EmptyListObject);

            MPassExpression(mp, lam, Rest(Rest(Rest(expr))), 0);
        }
        else
        {
            FAssert(Rest(Rest(Rest(expr))) == EmptyListObject);
        }
    }
    else if (ss == SetBangSyntax)
    {
        // (set! <variable> <expression>)

        FAssert(PairP(Rest(expr)));
        FAssert(PairP(Rest(Rest(expr))));
        FAssert(Rest(Rest(Rest(expr))) == EmptyListObject);
        FAssert(ReferenceP(First(Rest(expr))));

        if (mp->SetReferenceFn != 0)
            mp->SetReferenceFn(lam, AsReference(First(Rest(expr))));
        MPassExpression(mp, lam, Rest(Rest(expr)), 0);
    }
    else if (ss == LetStarValuesSyntax || ss == LetrecSyntax)
    {
        // (let*-values ((<formals> <init>) ...) <body>)
        // (letrec ((<formals> <init>) ...) <body>)

        FAssert(PairP(Rest(expr)));

        if (mp->LetFormalFn != 0)
        {
            FAssert(mp->LetFormalsInitFn == 0);

            MPassLetFormals(mp, lam, First(Rest(expr)));
        }
        else
        {
//            if (mp->LetFormalsInitFn != 0)
//                MPassLetFormalsInits(mp, lam, First(Rest(expr)), &(AsPair(Rest(expr))->First));
            if (mp->LetFormalsInitFn != 0)
            {
                FObject ret = MPassLetFormalsInits(mp, lam, First(Rest(expr)));
                if (ret != First(Rest(expr)))
                {
//                    AsPair(Rest(expr))->First = ret;
                    SetFirst(Rest(expr), ret);
                }
            }

            MPassLetInits(mp, lam, First(Rest(expr)));
        }

        MPassSequence(mp, lam, Rest(Rest(expr)));
    }
    else if (ss == CaseSyntax)
    {
        // (case <key> <clause> ...)

        FAssert(PairP(Rest(expr)));

        MPassExpression(mp, lam, Rest(expr), 0);
        MPassCase(mp, lam, Rest(Rest(expr)));
    }
    else if (ss == OrSyntax || ss == BeginSyntax)
    {
        // (or <test> ...)
        // (begin <expression> ...)

        MPassSequence(mp, lam, Rest(expr));
    }
    else
    {
        // (quote <datum>)

        FAssert(ss == QuoteSyntax);
    }
}

static void MPassProcedureCall(FMiddlePass * mp, FLambda * lam, FObject expr)
{
    FAssert(PairP(expr));
    MPassExpression(mp, lam, expr, 1);

    expr = Rest(expr);
    while (expr != EmptyListObject)
    {
        FAssert(PairP(expr));
        MPassExpression(mp, lam, expr, 0);
        expr = Rest(expr);
    }
}

static void MPassExpression(FMiddlePass * mp, FLambda * lam, FObject pair, int cf)
{
Again:

    FAssert(PairP(pair));
    FObject expr = First(pair);

    if (ReferenceP(expr))
    {
        if (mp->ReferenceFn != 0)
        {
            mp->ReferenceFn(lam, pair, AsReference(expr), cf);

            if (First(pair) != expr)
                goto Again;
        }
    }
    else if (LambdaP(expr))
        MPassLambda(mp, lam, AsLambda(expr), cf);
    else if (CaseLambdaP(expr))
        MPassCaseLambda(mp, lam, AsCaseLambda(expr), cf);
    else if (PairP(expr))
    {
        if (SpecialSyntaxP(First(expr)))
        {
            MPassSpecialSyntax(mp, lam, expr, cf);

            if (mp->SpecialSyntaxFn != 0)
                mp->SpecialSyntaxFn(lam, expr);
        }
        else
        {
            MPassProcedureCall(mp, lam, expr);

            if (mp->ProcedureCallFn != 0)
                mp->ProcedureCallFn(lam, expr);
        }
    }
    else
    {
        FAssert(IdentifierP(expr) == 0);
        FAssert(SymbolP(expr) == 0);
        FAssert(BindingP(expr) == 0);
        FAssert(SpecialSyntaxP(expr) == 0);
    }
}

static void MPassSequence(FMiddlePass * mp, FLambda * lam, FObject seq)
{
    while (PairP(seq))
    {
        MPassExpression(mp, lam, seq, 0);
        seq = Rest(seq);
    }

    FAssert(seq == EmptyListObject);
}

static void MPassLambdaFormals(FMiddlePass * mp, FLambda * lam, FObject flst)
{
    while (PairP(flst))
    {
        FObject bd = First(flst);
        FAssert(BindingP(bd));

        mp->LambdaFormalFn(lam, AsBinding(bd));
        flst = Rest(flst);
    }

    FAssert(flst == EmptyListObject);
}

static void MPassLambda(FMiddlePass * mp, FLambda * enc, FLambda * lam, int cf)
{
    if (enc != 0 && mp->LambdaEnclosingFn != 0)
        mp->LambdaEnclosingFn(enc, lam, cf);

    if (lam->MiddlePass != mp->MiddlePass)
    {
//        lam->MiddlePass = mp->MiddlePass;
        Modify(FLambda, lam, MiddlePass, mp->MiddlePass);

        if (mp->LambdaBeforeFn != 0)
            mp->LambdaBeforeFn(enc, lam, cf);

        if (mp->LambdaFormalFn != 0)
            MPassLambdaFormals(mp, lam, lam->Bindings);

        MPassSequence(mp, lam, lam->Body);

        if (mp->LambdaAfterFn != 0)
            mp->LambdaAfterFn(lam, cf);
    }
}

static void MPassLambda(FMiddlePass * mp, FLambda * lam)
{
    MPassLambda(mp, 0, lam, 0);
}

// ---- Middle One ----
//
// - uses and sets of all variables
// - binding and lambda escape analysis

static void MOneLambdaBefore(FLambda * enc, FLambda * lam, int cf)
{
    if (cf == 0)
    {
        FAssert(lam->Escapes == FalseObject);

//        lam->Escapes = TrueObject;
        Modify(FLambda, lam, Escapes, TrueObject);
    }
}

static void MOneCaseLambda(FLambda * enc, FCaseLambda * cl, int cf)
{
    if (cf == 0)
    {
//        cl->Escapes = TrueObject;
        Modify(FCaseLambda, cl, Escapes, TrueObject);
    }
}

static void MOneReference(FLambda * lam, FObject pair, FReference * ref, int cf)
{
    FObject bd = ref->Binding;

    if (BindingP(bd))
    {
        if (cf == 0)
        {
//            AsBinding(bd)->Escapes = TrueObject;
            Modify(FBinding, bd, Escapes, TrueObject);
        }

//        AsBinding(bd)->UseCount = MakeFixnum(AsFixnum(AsBinding(bd)->UseCount) + 1);
        Modify(FBinding, bd, UseCount, MakeFixnum(AsFixnum(AsBinding(bd)->UseCount) + 1));
    }
}

static void MOneSetReference(FLambda * lam, FReference * ref)
{
    FObject bd = ref->Binding;
    if (BindingP(bd))
    {
//        AsBinding(bd)->SetCount = MakeFixnum(AsFixnum(AsBinding(bd)->SetCount) + 1);
        Modify(FBinding, bd, SetCount, MakeFixnum(AsFixnum(AsBinding(bd)->SetCount) + 1));
    }
}

static FMiddlePass MOne =
{
    MakeFixnum(1),
    0,
    0,
    MOneLambdaBefore,
    0,
    MOneCaseLambda,
    MOneReference,
    MOneSetReference,
    0,
    0,
    0,
    0
};

// ---- Middle Two ----
//
// - substitute references to constants with the constant
// - a lambda may be inlined if:
//    - it is a leaf
//    - each formal is used exactly once and not modified
//    - no formal is called
//    - no special syntax is used except quote
//    - small: makes less than three procedure calls

static void MTwoLambdaFormal(FLambda * lam, FBinding * bd)
{
    if (bd->RestArg == TrueObject || bd->UseCount != MakeFixnum(1)
            || bd->SetCount != MakeFixnum(0) || bd->Escapes == FalseObject)
    {
//        lam->MayInline = FalseObject;
        Modify(FLambda, lam, MayInline, FalseObject);
    }
}

static void MTwoLambdaEnclosing(FLambda * enc, FLambda * lam, int cf)
{
    FAssert(enc != 0);

//    enc->MayInline = FalseObject;
    Modify(FLambda, enc, MayInline, FalseObject);
}

static void MTwoLambdaBefore(FLambda * enc, FLambda * lam, int cf)
{
//    lam->ArgCount = MakeFixnum(ListLength(lam->Bindings));
    Modify(FLambda, lam, ArgCount, MakeFixnum(ListLength(lam->Bindings)));
}

static FObject CompileInlineTemplate(FObject flst, FObject expr)
{
    if (PairP(expr))
    {
        if (First(expr) == QuoteSyntax)
            return(expr);

        return(MakePair(CompileInlineTemplate(flst, First(expr)),
                CompileInlineTemplate(flst, Rest(expr))));
    }
    else if (ReferenceP(expr))
    {
        int idx = 0;
        while (PairP(flst))
        {
            FAssert(BindingP(First(flst)));

            if (First(flst) == AsReference(expr)->Binding)
                return(MakeInlineVariable(idx));

            flst = Rest(flst);
            idx += 1;
        }

        FAssert(flst == EmptyListObject);
    }

    return(expr);
}

static void MTwoLambdaAfter(FLambda * lam, int cf)
{
    if (Config.InlineProcedures == 0)
    {
//        lam->MayInline = FalseObject;
        Modify(FLambda, lam, MayInline, FalseObject);
    }

    if (lam->MayInline != FalseObject)
    {
        FAssert(FixnumP(lam->MayInline));

        if (AsFixnum(lam->MayInline) > 2)
        {
//            lam->MayInline = FalseObject;
            Modify(FLambda, lam, MayInline, FalseObject);
        }
        else
        {
            FAssert(lam->RestArg == FalseObject);

//WritePretty(StandardOutput, lam->Name, 0);
//PutCh(StandardOutput, ' ');
//WritePretty(StandardOutput, lam->MayInline, 0);
//PutCh(StandardOutput, '\n');

//            lam->MayInline = CompileInlineTemplate(lam->Bindings, lam->Body);
            Modify(FLambda, lam, MayInline, CompileInlineTemplate(lam->Bindings, lam->Body));
        }
    }
}

static void MTwoReference(FLambda * lam, FObject pair, FReference * ref, int cf)
{
    FObject bd = ref->Binding;

    if (BindingP(bd))
    {
        if (AsBinding(bd)->Constant != NoValueObject)
        {
            FAssert(PairP(pair));

//            AsPair(pair)->First = AsBinding(bd)->Constant;
            SetFirst(pair, AsBinding(bd)->Constant);
        }
    }
    else
    {
        FAssert(EnvironmentP(bd));

        FObject gl = EnvironmentBind(bd, AsIdentifier(ref->Identifier)->Symbol);

        FAssert(GlobalP(gl));

        if (Config.InlineImports
                && (AsGlobal(gl)->Interactive == FalseObject || Config.InteractiveLikeLibrary)
                && AsGlobal(gl)->State == GlobalImported)
        {
//            AsPair(pair)->First = Unbox(AsGlobal(gl)->Box);
            SetFirst(pair, Unbox(AsGlobal(gl)->Box));
        }
    }
}

static int ConstantP(FObject obj)
{
    if (PairP(obj) && First(obj) != QuoteSyntax)
        return(0);

    if (ReferenceP(obj))
        return(0);

    if (LambdaP(obj) && AsLambda(obj)->Escapes == TrueObject)
        return(0);

    if (CaseLambdaP(obj) && AsCaseLambda(obj)->Escapes == TrueObject)
        return(0);

    return(1);
}

static void MTwoLetInitCaseLambda(FCaseLambda * cl, FObject nam, FObject ef)
{
    FAssert(cl->Name == NoValueObject);

//    cl->Name = nam;
    Modify(FCaseLambda, cl, Name, nam);
//    cl->Escapes = ef;
    Modify(FCaseLambda, cl, Escapes, ef);

    FObject cases = cl->Cases;

    while (PairP(cases))
    {
        FAssert(LambdaP(First(cases)));
        FAssert(AsLambda(First(cases))->Name == NoValueObject);

//        AsLambda(First(cases))->Name = nam;
        Modify(FLambda, First(cases), Name, nam);
//        AsLambda(First(cases))->Escapes = ef;
        Modify(FLambda, First(cases), Escapes, ef);

        cases = Rest(cases);
    }

    FAssert(cases == EmptyListObject);
}

static int MTwoLetFormalsInit(FLambda * lam, FObject flst, FObject init)
{
    FAssert(BindingP(First(flst)));
    FBinding * bd = AsBinding(First(flst));

    if (Rest(flst) == EmptyListObject && bd->RestArg == FalseObject)
    {
        if (LambdaP(init))
        {
            FAssert(AsLambda(init)->Name == NoValueObject);

//            AsLambda(init)->Escapes = bd->Escapes;
            Modify(FLambda, init, Escapes, bd->Escapes);
//            AsLambda(init)->Name = bd->Identifier;
            Modify(FLambda, init, Name, bd->Identifier);
        }

        if (CaseLambdaP(init))
            MTwoLetInitCaseLambda(AsCaseLambda(init), bd->Identifier, bd->Escapes);

        if (AsFixnum(bd->SetCount) == 0 && ConstantP(init))
        {
//            bd->Constant = init;
            Modify(FBinding, bd, Constant, init);
            return(0);
        }
    }

    return(1);
}

static void MTwoSpecialSyntax(FLambda * lam, FObject expr)
{
    FAssert(PairP(expr));

    if (First(expr) != QuoteSyntax && First(expr) != BeginSyntax)
    {
//        lam->MayInline = FalseObject;
        Modify(FLambda, lam, MayInline, FalseObject);
    }
}

static void MTwoProcedureCall(FLambda * lam, FObject expr)
{
    if (lam->MayInline != FalseObject)
    {
        FAssert(FixnumP(lam->MayInline));

//        lam->MayInline = MakeFixnum(AsFixnum(lam->MayInline) + 1);
        Modify(FLambda, lam, MayInline, MakeFixnum(AsFixnum(lam->MayInline) + 1));
    }
}

static FMiddlePass MTwo =
{
    MakeFixnum(2),
    MTwoLambdaFormal,
    MTwoLambdaEnclosing,
    MTwoLambdaBefore,
    MTwoLambdaAfter,
    0,
    MTwoReference,
    0,
    0,
    MTwoLetFormalsInit,
    MTwoSpecialSyntax,
    MTwoProcedureCall
};

// ---- Middle Three ----
//
// - inline lambdas where possible

static FObject ExpandInlineTemplate(FObject vars, FObject tpl)
{
    if (PairP(tpl))
    {
        if (First(tpl) == QuoteSyntax)
            return(tpl);

        return(MakePair(ExpandInlineTemplate(vars, First(tpl)),
                ExpandInlineTemplate(vars, Rest(tpl))));
    }
    else if (InlineVariableP(tpl))
    {
        for (int idx = 0; idx < AsFixnum(AsInlineVariable(tpl)->Index); idx++)
        {
            FAssert(PairP(vars));

            vars = Rest(vars);
        }

        return(First(vars));
    }

    return(tpl);
}

static void MThreeProcedureCall(FLambda * lam, FObject expr)
{
    FAssert(PairP(expr));

    if (lam->MayInline == FalseObject && LambdaP(First(expr))
            && AsLambda(First(expr))->MayInline != FalseObject
            && ListLength(Rest(expr)) == AsFixnum(AsLambda(First(expr))->ArgCount))
    {
//WritePretty(StandardOutput, AsLambda(First(expr))->MayInline, 0);
//PutCh(StandardOutput, '\n');
//WritePretty(StandardOutput, Rest(expr), 0);
//PutCh(StandardOutput, '\n');

        FObject exp = ExpandInlineTemplate(Rest(expr), AsLambda(First(expr))->MayInline);
//        AsPair(expr)->First = BeginSyntax;
        SetFirst(expr, BeginSyntax);
//        AsPair(expr)->Rest = exp;
        SetRest(expr, exp);
    }
}

static FMiddlePass MThree =
{
    MakeFixnum(3),
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    MThreeProcedureCall
};

// ---- Middle Four ----
//
// - assign slots to bindings
// - lambda leaf analysis
// - level for each lambda and binding: outer most lambda is level one

static void MFourLambdaFormal(FLambda * lam, FBinding * bd)
{
//    bd->Level = lam->Level;
    Modify(FBinding, bd, Level, lam->Level);

//    bd->Slot = MakeFixnum(AsFixnum(lam->ArgCount) - AsFixnum(lam->SlotCount) + 1);
    Modify(FBinding, bd, Slot, MakeFixnum(AsFixnum(lam->ArgCount) - AsFixnum(lam->SlotCount) + 1));
//    lam->SlotCount = MakeFixnum(AsFixnum(lam->SlotCount) + 1);
    Modify(FLambda, lam, SlotCount, MakeFixnum(AsFixnum(lam->SlotCount) + 1));

    if (bd->RestArg == TrueObject)
    {
//        lam->RestArg = TrueObject;
        Modify(FLambda, lam, RestArg, TrueObject);
    }
}

static void MFourLambdaEnclosing(FLambda * enc, FLambda * lam, int cf)
{
    FAssert(enc != 0);

//    enc->UseStack = FalseObject;
    Modify(FLambda, enc, UseStack, FalseObject);
}

static void MFourLambdaBefore(FLambda * enc, FLambda * lam, int cf)
{
//    lam->SlotCount = MakeFixnum(1); // Slot 0 is reserved for the enclosing frame.
    Modify(FLambda, lam, SlotCount, MakeFixnum(1)); // Slot 0 is reserved for the enclosing frame.

    if (enc == 0)
    {
//        lam->Level = MakeFixnum(1);
        Modify(FLambda, lam, Level, MakeFixnum(1));
    }
    else
    {
//        lam->Level = MakeFixnum(AsFixnum(enc->Level) + 1);
        Modify(FLambda, lam, Level, MakeFixnum(AsFixnum(enc->Level) + 1));
    }
}

static void MFourLetFormal(FLambda * lam, FBinding * bd)
{
    FAssert(bd->Constant == NoValueObject);

//    bd->Level = lam->Level;
    Modify(FBinding, bd, Level, lam->Level);

//    bd->Slot = lam->SlotCount;
    Modify(FBinding, bd, Slot, lam->SlotCount);
//    lam->SlotCount = MakeFixnum(AsFixnum(lam->SlotCount) + 1);
    Modify(FLambda, lam, SlotCount, MakeFixnum(AsFixnum(lam->SlotCount) + 1));
}

static FMiddlePass MFour =
{
    MakeFixnum(4),
    MFourLambdaFormal,
    MFourLambdaEnclosing,
    MFourLambdaBefore,
    0,
    0,
    0,
    0,
    MFourLetFormal,
    0,
    0,
    0
};

// ----------------

void MPassLambda(FLambda * lam)
{
    MPassLambda(&MOne, lam);
    MPassLambda(&MTwo, lam);
    MPassLambda(&MThree, lam);
    MPassLambda(&MFour, lam);
}
