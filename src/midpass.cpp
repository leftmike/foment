/*

Foment

*/

#include "foment.hpp"
#include "compile.hpp"

static void UPassExpression(FObject expr, int ef);
static void UPassSequence(FObject seq);
static void UPassCaseLambda(FCaseLambda * cl, int ef);

static void UPassBindingList(FObject flst)
{
    while (PairP(flst))
    {
        FObject bd = First(flst);

        FAssert(BindingP(bd));

        AsBinding(bd)->UseCount = MakeFixnum(0);
        AsBinding(bd)->SetCount = MakeFixnum(0);
        AsBinding(bd)->Escapes = FalseObject;

        flst = Rest(flst);
    }

    FAssert(flst == EmptyListObject);
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

static void UPassSpecialSyntax(FObject expr, int ef)
{
    FObject ss = First(expr);

    FAssert(ss == QuoteSyntax || ss == IfSyntax || ss == SetBangSyntax
            || ss == LetValuesSyntax || ss == LetrecValuesSyntax || ss == LetrecStarValuesSyntax
            || ss == OrSyntax || ss == BeginSyntax || ss == SetBangValuesSyntax);

    if (ss == IfSyntax)
    {
        // (if <test> <consequent> <alternate>)
        // (if <test> <consequent>)

        FAssert(PairP(Rest(expr)));
        FAssert(PairP(Rest(Rest(expr))));

        UPassExpression(First(Rest(expr)), 1);
        UPassExpression(First(Rest(Rest(expr))), 1);

        if (PairP(Rest(Rest(Rest(expr)))))
        {
            FAssert(Rest(Rest(Rest(Rest(expr)))) == EmptyListObject);

            UPassExpression(First(Rest(Rest(Rest(expr)))), 1);
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

        FObject bd = AsReference(First(Rest(expr)))->Binding;
        if (BindingP(bd))
            AsBinding(bd)->SetCount = MakeFixnum(AsFixnum(AsBinding(bd)->SetCount) + 1);

        UPassExpression(First(Rest(Rest(expr))), 1);
    }
    else if (ss == SetBangValuesSyntax)
    {
        // (set!-values (<variable> ...) <expression>)

        FAssert(PairP(Rest(expr)));
        FAssert(PairP(Rest(Rest(expr))));
        FAssert(Rest(Rest(Rest(expr))) == EmptyListObject);

        FObject lst = First(Rest(expr));
        while (lst != EmptyListObject)
        {
            FAssert(PairP(lst));
            FAssert(ReferenceP(First(lst)));

            FObject bd = AsReference(First(lst))->Binding;
            if (BindingP(bd))
                AsBinding(bd)->SetCount = MakeFixnum(AsFixnum(AsBinding(bd)->SetCount) + 1);

            lst = Rest(lst);
        }

        UPassExpression(First(Rest(Rest(expr))), 1);
    }
    else if (ss == LetValuesSyntax || ss == LetrecValuesSyntax || ss == LetrecStarValuesSyntax)
    {
        // (let-values ((<formals> <init>) ...) <body>)
        // (letrec-values ((<formals> <init>) ...) <body>)
        // (letrec*-values ((<formals> <init>) ...) <body>)

        FAssert(PairP(Rest(expr)));

        FObject lb = First(Rest(expr));
        while (PairP(lb))
        {
            FObject vi = First(lb);

            FAssert(PairP(vi));
            FAssert(PairP(Rest(vi)));
            FAssert(Rest(Rest(vi)) == EmptyListObject);

            UPassBindingList(First(vi));
            lb = Rest(lb);
        }

        FAssert(lb == EmptyListObject);

        lb = First(Rest(expr));
        while (PairP(lb))
        {
            FObject vi = First(lb);
            UPassExpression(First(Rest(vi)), 1);
            lb = Rest(lb);
        }

        UPassSequence(Rest(Rest(expr)));

        lb = First(Rest(expr));
        while (PairP(lb))
        {
            FObject vi = First(lb);

            if (PairP(First(vi)) && Rest(First(vi)) == EmptyListObject
                    && AsBinding(First(First(vi)))->RestArg == FalseObject)
            {
                FBinding * bd = AsBinding(First(First(vi)));
                FObject init = First(Rest(vi));
                if (LambdaP(init))
                {
                    AsLambda(init)->Escapes = bd->Escapes;
                    AsLambda(init)->Name = bd->Identifier;
                }
                else if (CaseLambdaP(init))
                {
                    AsCaseLambda(init)->Name = bd->Identifier;
                    AsCaseLambda(init)->Escapes = bd->Escapes;

                    FObject cases = AsCaseLambda(init)->Cases;
                    while (PairP(cases))
                    {
                        FAssert(LambdaP(First(cases)));

                        AsLambda(First(cases))->Name = bd->Identifier;
                        AsLambda(First(cases))->Escapes = bd->Escapes;

                        cases = Rest(cases);
                    }

                    FAssert(cases == EmptyListObject);
                }

                if (AsFixnum(bd->SetCount) == 0 && ConstantP(init))
                    bd->Constant = init;
            }

            lb = Rest(lb);
        }
    }
    else if (ss == OrSyntax || ss == BeginSyntax)
    {
        // (or <test> ...)
        // (begin <expression> ...)

        UPassSequence(Rest(expr));
    }
    else
    {
        // (quote <datum>)

        FAssert(ss == QuoteSyntax);
    }
}

static void UPassProcedureCall(FObject expr)
{
    FAssert(PairP(expr));
    UPassExpression(First(expr), 0);

    expr = Rest(expr);
    while (expr != EmptyListObject)
    {
        FAssert(PairP(expr));
        UPassExpression(First(expr), 1);
        expr = Rest(expr);
    }
}

static void UPassExpression(FObject expr, int ef)
{
    if (ReferenceP(expr))
    {
        FObject bd = AsReference(expr)->Binding;

        if (BindingP(bd))
        {
            if (ef != 0)
                AsBinding(bd)->Escapes = TrueObject;

            AsBinding(bd)->UseCount = MakeFixnum(AsFixnum(AsBinding(bd)->UseCount) + 1);
        }
    }
    else if (LambdaP(expr))
        UPassLambda(AsLambda(expr), ef);
    else if (CaseLambdaP(expr))
        UPassCaseLambda(AsCaseLambda(expr), ef);
    else if (PairP(expr))
    {
        if (SpecialSyntaxP(First(expr)))
            UPassSpecialSyntax(expr, ef);
        else
            UPassProcedureCall(expr);
    }
    else
    {
        FAssert(IdentifierP(expr) == 0);
        FAssert(SymbolP(expr) == 0);
        FAssert(BindingP(expr) == 0);
        FAssert(SpecialSyntaxP(expr) == 0);
    }
}

static void UPassSequence(FObject seq)
{
    while (PairP(seq))
    {
        UPassExpression(First(seq), 1);
        seq = Rest(seq);
    }

    FAssert(seq == EmptyListObject);
}

static void UPassCaseLambda(FCaseLambda * cl, int ef)
{
    cl->Escapes = (ef != 0 ? TrueObject : FalseObject);

    FObject cases = cl->Cases;

    while (PairP(cases))
    {
        FAssert(LambdaP(First(cases)));

        UPassLambda(AsLambda(First(cases)), ef);
        cases = Rest(cases);
    }

    FAssert(cases == EmptyListObject);
}

void UPassLambda(FLambda * lam, int ef)
{
    if (lam->CompilerPass != UsePassSymbol)
    {
        lam->CompilerPass = UsePassSymbol;
        lam->Escapes = (ef != 0 ? TrueObject : FalseObject);

        UPassBindingList(lam->Bindings);
        UPassSequence(lam->Body);

        lam->MayInline = FalseObject;
        lam->ArgCount = MakeFixnum(ListLength(lam->Bindings));
    }
}

static FObject CPassExpression(FObject expr);
static FObject CPassSequence(FObject seq);
static void CPassCaseLambda(FCaseLambda * cl);

static FObject CPassLetBindings(FObject lb)
{
    if (lb == EmptyListObject)
        return(EmptyListObject);

    FAssert(PairP(lb));

    FObject vi = First(lb);

    FAssert(PairP(vi));
    FAssert(PairP(Rest(vi)));
    FAssert(Rest(Rest(vi)) == EmptyListObject);

    if (PairP(First(vi)))
    {
        FAssert(BindingP(First(First(vi))));

        if (AsBinding(First(First(vi)))->Constant != NoValueObject)
            return(CPassLetBindings(Rest(lb)));
    }

    return(MakePair(List(First(vi), CPassExpression(First(Rest(vi)))),
            CPassLetBindings(Rest(lb))));
}

static FObject CPassSpecialSyntax(FObject expr)
{
    FObject ss = First(expr);

    FAssert(ss == QuoteSyntax || ss == IfSyntax || ss == SetBangSyntax
            || ss == LetValuesSyntax || ss == LetrecValuesSyntax || ss == LetrecStarValuesSyntax
            || ss == OrSyntax || ss == BeginSyntax || ss == SetBangValuesSyntax);

    if (ss == IfSyntax)
    {
        // (if <test> <consequent> <alternate>)
        // (if <test> <consequent>)

        FAssert(PairP(Rest(expr)));
        FAssert(PairP(Rest(Rest(expr))));

        if (PairP(Rest(Rest(Rest(expr)))))
        {
            FAssert(Rest(Rest(Rest(Rest(expr)))) == EmptyListObject);

            return(List(First(expr),
                    CPassExpression(First(Rest(expr))),
                    CPassExpression(First(Rest(Rest(expr)))),
                    CPassExpression(First(Rest(Rest(Rest(expr)))))));
        }
        else
        {
            FAssert(Rest(Rest(Rest(expr))) == EmptyListObject);

            return(List(First(expr),
                    CPassExpression(First(Rest(expr))),
                    CPassExpression(First(Rest(Rest(expr))))));
        }
    }
    else if (ss == SetBangSyntax || ss == SetBangValuesSyntax)
    {
        // (set! <variable> <expression>)
        // (set!-values (<variable> ...) <expression>)

        FAssert(PairP(Rest(expr)));
        FAssert(PairP(Rest(Rest(expr))));
        FAssert(Rest(Rest(Rest(expr))) == EmptyListObject);

        return(List(First(expr), First(Rest(expr)),
                CPassExpression(First(Rest(Rest(expr))))));
    }
    else if (ss == LetValuesSyntax || ss == LetrecValuesSyntax || ss == LetrecStarValuesSyntax)
    {
        // (let-values ((<formals> <init>) ...) <body>)
        // (letrec-values ((<formals> <init>) ...) <body>)
        // (letrec*-values ((<formals> <init>) ...) <body>)

        FAssert(PairP(Rest(expr)));

        return(MakePair(First(expr), MakePair(CPassLetBindings(First(Rest(expr))),
                CPassSequence(Rest(Rest(expr))))));
    }
    else if (ss == OrSyntax || ss == BeginSyntax)
    {
        // (or <test> ...)
        // (begin <expression> ...)

        return(MakePair(First(expr), CPassSequence(Rest(expr))));
    }
    else
    {
        // (quote <datum>)

        FAssert(ss == QuoteSyntax);

        return(expr);
    }
}

static FObject CPassExpression(FObject expr)
{
    if (ReferenceP(expr))
    {
        FObject bd = AsReference(expr)->Binding;

        if (BindingP(bd))
        {
            if (AsBinding(bd)->Constant != NoValueObject)
                return(CPassExpression(AsBinding(bd)->Constant));
        }
        else
        {
            FAssert(EnvironmentP(bd));

            FObject gl = EnvironmentBind(bd, AsIdentifier(AsReference(expr)->Identifier)->Symbol);

            FAssert(GlobalP(gl));

            if (AsGlobal(gl)->Interactive == FalseObject && AsGlobal(gl)->State == GlobalImported)
                return(Unbox(AsGlobal(gl)->Box));
        }

        return(expr);
    }
    else if (LambdaP(expr))
    {
        CPassLambda(AsLambda(expr));
        return(expr);
    }
    else if (CaseLambdaP(expr))
    {
        CPassCaseLambda(AsCaseLambda(expr));
        return(expr);
    }
    else if (PairP(expr))
    {
        if (SpecialSyntaxP(First(expr)))
            return(CPassSpecialSyntax(expr));
        else
            return(CPassSequence(expr));
    }
    else
    {
        FAssert(IdentifierP(expr) == 0);
        FAssert(SymbolP(expr) == 0);
        FAssert(BindingP(expr) == 0);
        FAssert(SpecialSyntaxP(expr) == 0);

        return(expr);
    }
}

static FObject CPassSequence(FObject seq)
{
    if (seq == EmptyListObject)
        return(EmptyListObject);

    FAssert(PairP(seq));

    return(MakePair(CPassExpression(First(seq)), CPassSequence(Rest(seq))));
}

static void CPassCaseLambda(FCaseLambda * cl)
{
    FObject cases = cl->Cases;

    while (PairP(cases))
    {
        FAssert(LambdaP(First(cases)));

        CPassLambda(AsLambda(First(cases)));
        cases = Rest(cases);
    }

    FAssert(cases == EmptyListObject);
}

void CPassLambda(FLambda * lam)
{
    if (lam->CompilerPass != ConstantPassSymbol)
    {
        lam->CompilerPass = ConstantPassSymbol;
        lam->Body = CPassSequence(lam->Body);
    }
}

static void APassExpression(FLambda * lam, FObject expr);
static void APassSequence(FLambda * lam, FObject seq);
static void APassCaseLambda(FLambda * lam, FCaseLambda * cl);

static void APassSpecialSyntax(FLambda * lam, FObject expr)
{
    FObject ss = First(expr);

    FAssert(ss == QuoteSyntax || ss == IfSyntax || ss == SetBangSyntax
            || ss == LetValuesSyntax || ss == LetrecValuesSyntax || ss == LetrecStarValuesSyntax
            || ss == OrSyntax || ss == BeginSyntax || ss == SetBangValuesSyntax);

    if (ss == IfSyntax)
    {
        // (if <test> <consequent> <alternate>)
        // (if <test> <consequent>)

        FAssert(PairP(Rest(expr)));
        FAssert(PairP(Rest(Rest(expr))));

        APassSequence(lam, Rest(expr));
    }
    else if (ss == SetBangSyntax || ss == SetBangValuesSyntax)
    {
        // (set! <variable> <expression>)
        // (set!-values (<variable> ...) <expression>)

        FAssert(PairP(Rest(expr)));
        FAssert(PairP(Rest(Rest(expr))));
        FAssert(Rest(Rest(Rest(expr))) == EmptyListObject);

        APassExpression(lam, First(Rest(Rest(expr))));
    }
    else if (ss == LetValuesSyntax || ss == LetrecValuesSyntax || ss == LetrecStarValuesSyntax)
    {
        // (let-values ((<formals> <init>) ...) <body>)
        // (letrec-values ((<formals> <init>) ...) <body>)
        // (letrec*-values ((<formals> <init>) ...) <body>)

        FAssert(PairP(Rest(expr)));

        FObject lb = First(Rest(expr));
        while (PairP(lb))
        {
            FObject vi = First(lb);

            FAssert(PairP(vi));
            FAssert(PairP(Rest(vi)));
            FAssert(Rest(Rest(vi)) == EmptyListObject);

            FObject flst = First(vi);
            while (PairP(flst))
            {
                FObject bd = First(flst);

                FAssert(BindingP(bd));

                AsBinding(bd)->Level = lam->Level;
                AsBinding(bd)->Slot = lam->SlotCount;
                lam->SlotCount = MakeFixnum(AsFixnum(lam->SlotCount) + 1);

                flst = Rest(flst);
            }

            FAssert(flst == EmptyListObject);

            lb = Rest(lb);
        }

        FAssert(lb == EmptyListObject);

        lb = First(Rest(expr));
        while (PairP(lb))
        {
            FObject vi = First(lb);
            APassExpression(lam, First(Rest(vi)));
            lb = Rest(lb);
        }

        APassSequence(lam, Rest(Rest(expr)));
    }
    else if (ss == OrSyntax || ss == BeginSyntax)
    {
        // (or <test> ...)
        // (begin <expression> ...)

        APassSequence(lam, Rest(expr));
    }
    else
    {
        // (quote <datum>)

        FAssert(ss == QuoteSyntax);
    }
}

static void APassExpression(FLambda * lam, FObject expr)
{
    if (LambdaP(expr))
        APassLambda(lam, AsLambda(expr));
    else if (CaseLambdaP(expr))
        APassCaseLambda(lam, AsCaseLambda(expr));
    else if (PairP(expr))
    {
        if (SpecialSyntaxP(First(expr)))
            APassSpecialSyntax(lam, expr);
        else
            APassSequence(lam, expr);
    }
    else
    {
        FAssert(IdentifierP(expr) == 0);
        FAssert(SymbolP(expr) == 0);
        FAssert(BindingP(expr) == 0);
        FAssert(SpecialSyntaxP(expr) == 0);
    }
}

static void APassSequence(FLambda * lam, FObject seq)
{
    while (PairP(seq))
    {
        APassExpression(lam, First(seq));
        seq = Rest(seq);
    }

    FAssert(seq == EmptyListObject);
}

static void APassCaseLambda(FLambda * lam, FCaseLambda * cl)
{
    FObject cases = cl->Cases;

    while (PairP(cases))
    {
        FAssert(LambdaP(First(cases)));

        APassLambda(lam, AsLambda(First(cases)));
        cases = Rest(cases);
    }

    FAssert(cases == EmptyListObject);
}

void APassLambda(FLambda * enc, FLambda * lam)
{
    if (lam->CompilerPass != AnalysisPassSymbol)
    {
        lam->CompilerPass = AnalysisPassSymbol;

        if (enc != 0)
            enc->UseStack = FalseObject;

        lam->SlotCount = MakeFixnum(1); // Slot 0: reserved for enclosing frame.

        FObject flst = lam->Bindings;
        while (PairP(flst))
        {
            FObject bd = First(flst);

            FAssert(BindingP(bd));

            AsBinding(bd)->Level = lam->Level;
            AsBinding(bd)->Slot =
                    MakeFixnum(AsFixnum(lam->ArgCount) - AsFixnum(lam->SlotCount) + 1);
            lam->SlotCount = MakeFixnum(AsFixnum(lam->SlotCount) + 1);

            if (AsBinding(bd)->RestArg == TrueObject)
                lam->RestArg = TrueObject;

            flst = Rest(flst);
        }

        FAssert(flst == EmptyListObject);

        APassSequence(lam, lam->Body);
    }
}
