/*

Foment

*/

#include <stdio.h>
#include "foment.hpp"
#include "compile.hpp"
#include "execute.hpp"

// ---- Generate Pass ----

/*
On entry to a call, this is what the AStack looks like:

          <-- AStackPtr
enclosing frame
    argn
     :
    arg1
    arg0

The enclosing frame will only be there for an internal call.
*/

typedef enum
{
    SingleValueFlag,
    MultipleValuesFlag,
    DiscardValuesFlag,
    TailCallFlag
} FContFlag;

static FObject GPassReturnValue(FLambda * lam, FObject cdl, FContFlag cf)
{
    FAssert(cf != DiscardValuesFlag);

    if (cf == TailCallFlag)
    {
        if (lam->UseStack == TrueObject)
            cdl = MakePair(MakeInstruction(PopCStackOpcode, AsFixnum(lam->SlotCount)), cdl);

        cdl = MakePair(MakeInstruction(ReturnOpcode, 0), cdl);
    }

    return(cdl);
}

static FObject GPassExpression(FLambda * lam, FObject cdl, FObject expr, FContFlag cf);
static FObject GPassSequence(FLambda * lam, FObject cdl, FObject seq, FContFlag cf);
static FObject GPassMakeCall(FLambda * lam, FObject cdl, FObject op, int_t argc, FObject expr,
    FContFlag cf);

static FObject GPassLetFormal(FLambda * lam, FObject cdl, FObject bd)
{
    if (AsFixnum(AsBinding(bd)->SetCount) > 0)
        cdl = MakePair(MakeInstruction(MakeBoxOpcode, 0), cdl);

    if (lam->UseStack == TrueObject)
        cdl = MakePair(MakeInstruction(SetCStackOpcode,
                AsFixnum(lam->SlotCount) - AsFixnum(AsBinding(bd)->Slot)), cdl);
    else
        cdl = MakePair(MakeInstruction(SetFrameOpcode, AsFixnum(AsBinding(bd)->Slot)), cdl);

    return(cdl);
}

static FObject GPassLetFormals(FLambda * lam, FObject cdl, FObject flst, int_t cnt)
{
    FAssert(PairP(flst));

    if (Rest(flst) == EmptyListObject)
    {
        FAssert(BindingP(First(flst)));

        if (AsBinding(First(flst))->RestArg == FalseObject)
        {
            FAssert(cnt != 1);

            cdl = MakePair(MakeInstruction(CheckValuesOpcode, cnt), cdl);
        }
        else
        {
            FAssert(cnt > 0);

            cdl = MakePair(MakeInstruction(RestValuesOpcode, cnt - 1), cdl);
        }
    }
    else
        cdl = GPassLetFormals(lam, cdl, Rest(flst), cnt + 1);

    cdl = GPassLetFormal(lam, cdl, First(flst));
    return(cdl);
}

static FObject GPassLetBindings(FLambda * lam, FObject cdl, FObject lb)
{
    // ((<formals> <init>) ...)

    while (PairP(lb))
    {
        FObject vi = First(lb);

        if (First(vi) == EmptyListObject)
        {
            cdl = GPassExpression(lam, cdl, First(Rest(vi)), MultipleValuesFlag);
            cdl = MakePair(MakeInstruction(CheckValuesOpcode, 0), cdl);
        }
        else if (Rest(First(vi)) != EmptyListObject
                || AsBinding(First(First(vi)))->RestArg == TrueObject)
        {
            cdl = GPassExpression(lam, cdl, First(Rest(vi)), MultipleValuesFlag);
            cdl = GPassLetFormals(lam, cdl, First(vi), 1);
        }
        else if (AsBinding(First(First(vi)))->Constant == NoValueObject)
        {
            FAssert(Rest(First(vi)) == EmptyListObject);

            cdl = GPassExpression(lam, cdl, First(Rest(vi)), SingleValueFlag);
            cdl = GPassLetFormal(lam, cdl, First(First(vi)));
        }

        lb = Rest(lb);
    }

    return(cdl);
}

static FObject GPassLetrecBindings(FLambda * lam, FObject cdl, FObject lrb)
{
    // ((<formals> <init>) ...)

    FObject lb = lrb;

    while (PairP(lb))
    {
        FObject vi = First(lb);

        FAssert(Rest(First(vi)) == EmptyListObject);
        FAssert(AsBinding(First(First(vi)))->RestArg == FalseObject);

        if (AsBinding(First(First(vi)))->Constant == NoValueObject)
        {
            cdl = GPassExpression(lam, cdl, First(Rest(vi)), SingleValueFlag);
//            cdl = GPassLetFormal(lam, cdl, First(First(vi)));
        }

        lb = Rest(lb);
    }

    lb = lrb;
    while (PairP(lb))
    {
        FObject vi = First(lb);

        if (AsBinding(First(First(vi)))->Constant == NoValueObject)
            cdl = GPassLetFormal(lam, cdl, First(First(vi)));

        lb = Rest(lb);
    }

    return(cdl);
}

static void SetJump(FObject tgt, FObject src, FOpcode op)
{
    int_t cnt = 0;
    while (tgt != src)
    {
        FAssert(PairP(tgt));

        cnt += 1;
        tgt = Rest(tgt);
    }

    FAssert(PairP(src));
//    AsPair(src)->First = MakeInstruction(op, cnt);
    SetFirst(src, MakeInstruction(op, cnt));
}

static FObject GPassOr(FLambda * lam, FObject cdl, FObject expr, FContFlag cf)
{
    FAssert(PairP(expr));

    if (Rest(expr) == EmptyListObject)
        return(GPassExpression(lam, cdl, First(expr), cf));

    cdl = GPassExpression(lam, cdl, First(expr), SingleValueFlag);
    if (cf != DiscardValuesFlag)
        cdl = MakePair(MakeInstruction(DuplicateOpcode, 0), cdl);
    cdl = MakePair(NoValueObject, cdl);
    FObject iff = cdl;

    if (cf == TailCallFlag)
        cdl = GPassReturnValue(lam, cdl, cf);
    else
        cdl = MakePair(NoValueObject, cdl);
    FObject jmp = cdl;

    SetJump(cdl, iff, IfFalseOpcode);
    if (cf != DiscardValuesFlag)
        cdl = MakePair(MakeInstruction(PopAStackOpcode, 1), cdl);
    cdl = GPassOr(lam, cdl, Rest(expr), cf);

    if (cf != TailCallFlag)
        SetJump(cdl, jmp, GotoRelativeOpcode);

    return(cdl);
}

static FObject GPassSetBang(FLambda * lam, FObject cdl, FReference * ref)
{
    if (BindingP(ref->Binding))
    {
        FBinding * bd = AsBinding(ref->Binding);

        FAssert(bd->Constant == NoValueObject);

        if (AsFixnum(bd->Level) == AsFixnum(lam->Level))
        {
            FAssert(AsFixnum(bd->Slot) > 0);

            if (lam->UseStack == TrueObject)
                cdl = MakePair(MakeInstruction(GetCStackOpcode,
                        AsFixnum(lam->SlotCount) - AsFixnum(bd->Slot)), cdl);
            else
                cdl = MakePair(MakeInstruction(GetFrameOpcode, AsFixnum(bd->Slot)), cdl);
        }
        else
        {
            FAssert(AsFixnum(bd->Slot) > 0);
            FAssert(AsFixnum(bd->Level) < AsFixnum(lam->Level));

            int_t cnt = AsFixnum(lam->Level) - AsFixnum(bd->Level);

            if (lam->UseStack == TrueObject)
                cdl = MakePair(MakeInstruction(GetCStackOpcode, AsFixnum(lam->SlotCount)),
                        cdl);
            else
                cdl = MakePair(MakeInstruction(GetFrameOpcode, 0), cdl);

            while (cnt > 1)
            {
                cdl = MakePair(MakeInstruction(GetVectorOpcode, 0), cdl);
                cnt -= 1;
            }

            cdl = MakePair(MakeInstruction(GetVectorOpcode, AsFixnum(bd->Slot)), cdl);
        }

        if (AsFixnum(AsBinding(bd)->SetCount) > 0)
            cdl = MakePair(MakeInstruction(SetBoxOpcode, 0), cdl);
    }
    else
    {
        FAssert(EnvironmentP(ref->Binding));

        FObject gl = EnvironmentBind(ref->Binding,
                AsIdentifier(ref->Identifier)->Symbol);

        FAssert(GlobalP(gl));

        if (AsGlobal(gl)->Interactive == TrueObject)
        {
            cdl = MakePair(gl, cdl);
            cdl = MakePair(MakeInstruction(SetGlobalOpcode, 0), cdl);
        }
        else
        {
            cdl = MakePair(AsGlobal(gl)->Box, cdl);
            cdl = MakePair(MakeInstruction(SetBoxOpcode, 0), cdl);
        }
    }

    return(cdl);
}

static FObject GPassSetBangValues(FLambda * lam, FObject cdl, FObject lst)
{
    if (lst == EmptyListObject)
        return(cdl);

    FAssert(PairP(lst));
    FAssert(ReferenceP(First(lst)));

    cdl = GPassSetBangValues(lam, cdl, Rest(lst));
    cdl = GPassSetBang(lam, cdl, AsReference(First(lst)));

    return(cdl);
}

static FObject GPassSpecialSyntax(FLambda * lam, FObject cdl, FObject expr, FContFlag cf)
{
    FObject ss = First(expr);

    if (ss == IfSyntax)
    {
        // (if <test> <consequent> <alternate>)
        // (if <test> <consequent>)

        cdl = GPassExpression(lam, cdl, First(Rest(expr)), SingleValueFlag);
        cdl = MakePair(NoValueObject, cdl);
        FObject iff = cdl;

        cdl = GPassExpression(lam, cdl, First(Rest(Rest(expr))), cf);

        if (PairP(Rest(Rest(Rest(expr)))))
        {
            if (cf != TailCallFlag)
            {
                cdl = MakePair(NoValueObject, cdl);
                FObject jmp = cdl;

                SetJump(cdl, iff, IfFalseOpcode);
                cdl = GPassExpression(lam, cdl, First(Rest(Rest(Rest(expr)))), cf);
                SetJump(cdl, jmp, GotoRelativeOpcode);
            }
            else
            {
                SetJump(cdl, iff, IfFalseOpcode);
                cdl = GPassExpression(lam, cdl, First(Rest(Rest(Rest(expr)))), cf);
            }
        }
        else if (cf != DiscardValuesFlag)
        {
            if (cf != TailCallFlag)
            {
                cdl = MakePair(NoValueObject, cdl);
                FObject jmp = cdl;

                SetJump(cdl, iff, IfFalseOpcode);
                cdl = MakePair(NoValueObject, cdl);
                SetJump(cdl, jmp, GotoRelativeOpcode);
            }
            else
            {
                SetJump(cdl, iff, IfFalseOpcode);
                cdl = MakePair(NoValueObject, cdl);
            }

            cdl = GPassReturnValue(lam, cdl, cf);
        }
        else
            SetJump(cdl, iff, IfFalseOpcode);

        return(cdl);
    }
    else if (ss == SetBangSyntax)
    {
        // (set! <variable> <expression>)

        cdl = GPassExpression(lam, cdl, First(Rest(Rest(expr))), SingleValueFlag);
        cdl = GPassSetBang(lam, cdl, AsReference(First(Rest(expr))));

        if (cf == DiscardValuesFlag)
            return(cdl);

        cdl = MakePair(NoValueObject, cdl);
        return(GPassReturnValue(lam, cdl, cf));
    }
    else if (ss == SetBangValuesSyntax)
    {
        // (set!-values (<variable> ...) <expression>)

        cdl = GPassExpression(lam, cdl, First(Rest(Rest(expr))), MultipleValuesFlag);
        cdl = MakePair(MakeInstruction(CheckValuesOpcode, ListLength(First(Rest(expr)))), cdl);
        cdl = GPassSetBangValues(lam, cdl, First(Rest(expr)));

        if (cf == DiscardValuesFlag)
            return(cdl);

        cdl = MakePair(NoValueObject, cdl);
        return(GPassReturnValue(lam, cdl, cf));
    }
    else if (ss == LetValuesSyntax)
    {
        // (let-values ((<formals> <init>) ...) <body>)

        cdl = GPassLetBindings(lam, cdl, First(Rest(expr)));
        return(GPassSequence(lam, cdl, Rest(Rest(expr)), cf));
    }
    else if (ss == LetrecStarValuesSyntax)
    {
        // (letrec*-values ((<formals> <init>) ...) <body>)

        cdl = GPassLetBindings(lam, cdl, First(Rest(expr)));
        return(GPassSequence(lam, cdl, Rest(Rest(expr)), cf));
    }
    else if (ss == LetrecValuesSyntax)
    {
        // (letrec-values ((<formals> <init>) ...) <body>)

        cdl = GPassLetrecBindings(lam, cdl, First(Rest(expr)));
        return(GPassSequence(lam, cdl, Rest(Rest(expr)), cf));
    }
    else if (ss == OrSyntax)
    {
        // (or <test> ...)

        cdl = GPassOr(lam, cdl, Rest(expr), cf);
    }
    else if (ss == BeginSyntax)
    {
        // (begin <expression> ...)

        return(GPassSequence(lam, cdl, Rest(expr), cf));
    }
    else
    {
        // (quote <datum>)

        FAssert(ss == QuoteSyntax);

        if (cf == DiscardValuesFlag)
            return(cdl);

        return(GPassReturnValue(lam, MakePair(First(Rest(expr)), cdl), cf));
    }

    return(cdl);
}

static FObject GPassCaseLambda(FCaseLambda * cl)
{
    FObject cdl = EmptyListObject;

    cdl = MakePair(MakeInstruction(CaseLambdaOpcode, ListLength(cl->Cases)), cdl);

    FObject cases = cl->Cases;
    while (PairP(cases))
    {
        FAssert(LambdaP(First(cases)));

        cdl = MakePair(GPassLambda(AsLambda(First(cases))), cdl);
        cases = Rest(cases);
    }

    FAssert(cases == EmptyListObject);

    return(MakeProcedure(cl->Name, NoValueObject, NoValueObject,
            ListToVector(ReverseListModify(cdl)), AsFixnum(1), PROCEDURE_FLAG_RESTARG));
}

FObject GPassLambdaFrame(FLambda * lam, FObject cdl, FLambda * op)
{
    if (AsFixnum(op->Level) > AsFixnum(lam->Level))
    {
        FAssert(AsFixnum(op->Level) == AsFixnum(lam->Level) + 1);
        FAssert(lam->UseStack == FalseObject);

        cdl = MakePair(MakeInstruction(PushFrameOpcode, 0), cdl);
    }
    else
    {
        if (lam->UseStack == FalseObject)
            cdl = MakePair(MakeInstruction(GetFrameOpcode, 0), cdl);
        else
            cdl = MakePair(MakeInstruction(GetCStackOpcode, AsFixnum(lam->SlotCount)), cdl);

        int_t cnt = AsFixnum(lam->Level) - AsFixnum(op->Level);
        while (cnt > 0)
        {
            cdl = MakePair(MakeInstruction(GetVectorOpcode, 0), cdl);
            cnt -= 1;
        }
    }

    return(cdl);
}

static FObject GPassProcedureCall(FLambda * lam, FObject cdl, FObject expr, FContFlag cf)
{
    FObject op = First(expr);
    int_t argc = 0;

    FObject alst = Rest(expr);
    while (PairP(alst))
    {
        cdl = GPassExpression(lam, cdl, First(alst), SingleValueFlag);
        argc += 1;
        alst = Rest(alst);
    }

    return(GPassMakeCall(lam, cdl, op, argc, expr, cf));
}

static FObject GPassSelfTailCall(FLambda * lam, FObject cdl, FObject blst)
{
    if (blst == EmptyListObject)
        return(cdl);

    FAssert(PairP(blst));

    cdl = GPassSelfTailCall(lam, cdl, Rest(blst));

    FAssert(BindingP(First(blst)));

    FBinding * bd = AsBinding(First(blst));

    FAssert(lam->UseStack == TrueObject);

    cdl = MakePair(MakeInstruction(SetCStackOpcode,
            AsFixnum(lam->SlotCount) - AsFixnum(bd->Slot)), cdl);
    return(cdl);
}

static FObject GPassMakeCall(FLambda * lam, FObject cdl, FObject op, int_t argc, FObject expr,
    FContFlag cf)
{
    if (CaseLambdaP(op))
    {
        FObject cases = AsCaseLambda(op)->Cases;

        while (PairP(cases))
        {
            FAssert(LambdaP(First(cases)));

            if ((AsLambda(First(cases))->RestArg == TrueObject
                    && argc + 1 >= AsFixnum(AsLambda(First(cases))->ArgCount))
                    || argc == AsFixnum(AsLambda(First(cases))->ArgCount))
            {
                op = First(cases);
                break;
            }

            cases = Rest(cases);
        }

        if (cases == EmptyListObject)
            RaiseExceptionC(R.Assertion, "case-lambda", "no matching case",
                    List(op, MakeFixnum(argc), expr));
    }

    if (LambdaP(op))
    {
        if (AsLambda(op)->Escapes == FalseObject)
        {
            if (AsLambda(op)->RestArg == TrueObject)
            {
                FAssert(AsFixnum(AsLambda(op)->ArgCount) > 0);

                if (argc < AsFixnum(AsLambda(op)->ArgCount) - 1)
                    RaiseException(R.Assertion, lam->Name == NoValueObject ? lam : lam->Name,
                            R.WrongNumberOfArguments, List(expr));
                else if (argc < AsFixnum(AsLambda(op)->ArgCount))
                {
                    cdl = MakePair(EmptyListObject, cdl);
                    argc += 1;
                }
                else
                {
                    cdl = MakePair(MakeInstruction(MakeListOpcode,
                            argc - (AsFixnum(AsLambda(op)->ArgCount) - 1)), cdl);
                    argc = AsFixnum(AsLambda(op)->ArgCount);
                }
            }
            else if (AsFixnum(AsLambda(op)->ArgCount) != argc)
                RaiseException(R.Assertion, lam->Name == NoValueObject ? lam : lam->Name,
                        R.WrongNumberOfArguments, List(expr));
            else if (AsLambda(op) == lam && cf == TailCallFlag && lam->UseStack == TrueObject)
            {
                FAssert(FixnumP(lam->BodyIndex));

                cdl = GPassSelfTailCall(lam, cdl, lam->Bindings);
                return(MakePair(MakeInstruction(GotoAbsoluteOpcode, AsFixnum(lam->BodyIndex)),
                        cdl));
            }
        }

        if (AsFixnum(AsLambda(op)->Level) > 1 && AsLambda(op)->Escapes == FalseObject)
            cdl = GPassLambdaFrame(lam, cdl, AsLambda(op));

        cdl = GPassExpression(lam, cdl, GPassLambda(AsLambda(op)), SingleValueFlag);
    }
/*    else if (PrimitiveP(op))
    {
        if (InlinePrimitives)
        {
            
            // inline primitives here
            
        }
    }*/
    else
        cdl = GPassExpression(lam, cdl, op, SingleValueFlag);


    cdl = MakePair(MakeInstruction(SetArgCountOpcode, argc), cdl);

    if (cf == TailCallFlag)
    {
        if (lam->UseStack == TrueObject)
            cdl = MakePair(MakeInstruction(PopCStackOpcode, AsFixnum(lam->SlotCount)), cdl);

        if (ProcedureP(op))
            cdl = MakePair(MakeInstruction(TailCallProcOpcode, 0), cdl);
        else if (PrimitiveP(op))
            cdl = MakePair(MakeInstruction(TailCallPrimOpcode, 0), cdl);
        else
            cdl = MakePair(MakeInstruction(TailCallOpcode, 0), cdl);
    }
    else
    {
        if (lam->UseStack == FalseObject)
            cdl = MakePair(MakeInstruction(SaveFrameOpcode, 0), cdl);

        if (cf == MultipleValuesFlag)
            cdl = MakePair(MakeInstruction(PushWantValuesOpcode, 0), cdl);

        if (ProcedureP(op))
            cdl = MakePair(MakeInstruction(CallProcOpcode, 0), cdl);
        else if (PrimitiveP(op))
            cdl = MakePair(MakeInstruction(CallPrimOpcode, 0), cdl);
        else
            cdl = MakePair(MakeInstruction(CallOpcode, 0), cdl);

        if (cf == MultipleValuesFlag)
            cdl = MakePair(MakeInstruction(PopCStackOpcode, 1), cdl);

        if (cf == DiscardValuesFlag)
            cdl = MakePair(MakeInstruction(DiscardResultOpcode, 1), cdl);

        if (lam->UseStack == FalseObject)
            cdl = MakePair(MakeInstruction(RestoreFrameOpcode, 0), cdl);
    }

    return(cdl);
}

static FObject GPassExpression(FLambda * lam, FObject cdl, FObject expr, FContFlag cf)
{
    if (PairP(expr))
    {
        if (SpecialSyntaxP(First(expr)))
            return(GPassSpecialSyntax(lam, cdl, expr, cf));

        return(GPassProcedureCall(lam, cdl, expr, cf));
    }

    if (cf == DiscardValuesFlag)
        return(cdl);

    if (ReferenceP(expr))
    {
        if (BindingP(AsReference(expr)->Binding))
        {
            FBinding * bd = AsBinding(AsReference(expr)->Binding);
            FAssert(bd->Constant == NoValueObject);

            if (AsFixnum(bd->Level) == AsFixnum(lam->Level))
            {
                FAssert(AsFixnum(bd->Slot) > 0);

                if (lam->UseStack == TrueObject)
                    cdl = MakePair(MakeInstruction(GetCStackOpcode,
                            AsFixnum(lam->SlotCount) - AsFixnum(bd->Slot)), cdl);
                else
                    cdl = MakePair(MakeInstruction(GetFrameOpcode, AsFixnum(bd->Slot)), cdl);
            }
            else
            {
                FAssert(AsFixnum(bd->Slot) > 0);
                FAssert(AsFixnum(bd->Level) < AsFixnum(lam->Level));

                int_t cnt = AsFixnum(lam->Level) - AsFixnum(bd->Level);

                if (lam->UseStack == TrueObject)
                    cdl = MakePair(MakeInstruction(GetCStackOpcode, AsFixnum(lam->SlotCount)),
                            cdl);
                else
                    cdl = MakePair(MakeInstruction(GetFrameOpcode, 0), cdl);

                while (cnt > 1)
                {
                    cdl = MakePair(MakeInstruction(GetVectorOpcode, 0), cdl);
                    cnt -= 1;
                }

                cdl = MakePair(MakeInstruction(GetVectorOpcode, AsFixnum(bd->Slot)), cdl);
            }

            if (AsFixnum(AsBinding(bd)->SetCount) > 0)
                cdl = MakePair(MakeInstruction(GetBoxOpcode, 0), cdl);
        }
        else
        {
            FAssert(EnvironmentP(AsReference(expr)->Binding));

            FObject gl = EnvironmentBind(AsReference(expr)->Binding,
                    AsIdentifier(AsReference(expr)->Identifier)->Symbol);

            FAssert(GlobalP(gl));

            if (AsGlobal(gl)->Interactive == TrueObject)
            {
                cdl = MakePair(gl, cdl);
                cdl = MakePair(MakeInstruction(GetGlobalOpcode, 0), cdl);
            }
            else
            {
                cdl = MakePair(AsGlobal(gl)->Box, cdl);
                cdl = MakePair(MakeInstruction(GetBoxOpcode, 0), cdl);
            }
        }
    }
    else if (LambdaP(expr))
    {
        FAssert(AsLambda(expr)->Escapes == TrueObject);
        FAssert(ProcedureP(AsLambda(expr)->Procedure) == 0);

        if (AsFixnum(AsLambda(expr)->Level) == 1)
            cdl = MakePair(GPassLambda(AsLambda(expr)), cdl);
        else
        {
            cdl = GPassLambdaFrame(lam, cdl, AsLambda(expr));
            cdl = MakePair(GPassLambda(AsLambda(expr)), cdl);
            cdl = MakePair(MakeInstruction(MakeClosureOpcode, 0), cdl);
        }
    }
    else if (CaseLambdaP(expr))
    {
        FAssert(AsCaseLambda(expr)->Escapes == TrueObject);
        FAssert(PairP(AsCaseLambda(expr)->Cases));
        FAssert(LambdaP(First(AsCaseLambda(expr)->Cases)));

        FLambda * alam = AsLambda(First(AsCaseLambda(expr)->Cases));
        if (AsFixnum(alam->Level) == 1)
            cdl = MakePair(GPassCaseLambda(AsCaseLambda(expr)), cdl);
        else
        {
            cdl = GPassLambdaFrame(lam, cdl, alam);
            cdl = MakePair(GPassCaseLambda(AsCaseLambda(expr)), cdl);
            cdl = MakePair(MakeInstruction(MakeClosureOpcode, 0), cdl);
        }
    }
    else if (VectorP(expr))
        cdl = MakePair(SyntaxToDatum(expr), cdl);
    else
        cdl = MakePair(expr, cdl);

    return(GPassReturnValue(lam, cdl, cf));
}

static FObject GPassSequence(FLambda * lam, FObject cdl, FObject seq, FContFlag cf)
{
    while (PairP(seq))
    {
        if (Rest(seq) == EmptyListObject)
            cdl = GPassExpression(lam, cdl, First(seq), cf);
        else
            cdl = GPassExpression(lam, cdl, First(seq), DiscardValuesFlag);

        seq = Rest(seq);
    }

    FAssert(seq == EmptyListObject);

    return(cdl);
}

FObject GPassLambda(FLambda * lam)
{
    if (ProcedureP(lam->Procedure))
        return(lam->Procedure);

//    lam->Procedure = MakeProcedure(lam->Name, lam->Filename, lam->LineNumber, NoValueObject,
//            AsFixnum(lam->ArgCount), lam->RestArg);
    Modify(FLambda, lam, Procedure,
            MakeProcedure(lam->Name, lam->Filename, lam->LineNumber, NoValueObject,
            AsFixnum(lam->ArgCount), lam->RestArg == TrueObject ? PROCEDURE_FLAG_RESTARG : 0));

    FObject cdl = EmptyListObject;

    if (lam->UseStack == FalseObject)
        cdl = MakePair(MakeInstruction(MakeFrameOpcode, AsFixnum(lam->SlotCount)), cdl);

    if (AsFixnum(lam->Level) > 1)
    {
        if (lam->UseStack == TrueObject)
            cdl = MakePair(MakeInstruction(PushCStackOpcode, 1), cdl);
        else
            cdl = MakePair(MakeInstruction(SetFrameOpcode, 0), cdl);
    }
    else if (lam->UseStack == TrueObject)
        cdl = MakePair(MakeInstruction(PushNoValueOpcode, 1), cdl);

    if (lam->Escapes == TrueObject)
    {
        if (lam->RestArg == TrueObject)
        {
            FAssert(AsFixnum(lam->ArgCount) > 0);

            cdl = MakePair(MakeInstruction(RestArgOpcode, AsFixnum(lam->ArgCount) - 1), cdl);
        }
        else
            cdl = MakePair(MakeInstruction(CheckCountOpcode, AsFixnum(lam->ArgCount)), cdl);
    }

    if (lam->UseStack == FalseObject)
    {
        for (int_t adx = 0; adx < AsFixnum(lam->ArgCount); adx++)
            cdl = MakePair(MakeInstruction(SetFrameOpcode, adx + 1), cdl);
    }
    else
    {
        cdl = MakePair(MakeInstruction(PushCStackOpcode, AsFixnum(lam->ArgCount)), cdl);

        FAssert(AsFixnum(lam->SlotCount) >= AsFixnum(lam->ArgCount) + 1);

        if (AsFixnum(lam->SlotCount) > AsFixnum(lam->ArgCount) + 1)
            cdl = MakePair(MakeInstruction(PushNoValueOpcode,
                    AsFixnum(lam->SlotCount) - (AsFixnum(lam->ArgCount) + 1)), cdl);
    }

//    lam->BodyIndex = MakeFixnum(ListLength(cdl));
    Modify(FLambda, lam, BodyIndex, MakeFixnum(ListLength(cdl)));

    for (FObject flst = lam->Bindings; PairP(flst); flst = Rest(flst))
    {
        FAssert(BindingP(First(flst)));

        FBinding * bd = AsBinding(First(flst));

        if (AsFixnum(bd->SetCount) > 0)
        {
            if (lam->UseStack == TrueObject)
            {
                cdl = MakePair(MakeInstruction(GetCStackOpcode,
                        AsFixnum(lam->SlotCount) - AsFixnum(bd->Slot)), cdl);
                cdl = MakePair(MakeInstruction(MakeBoxOpcode, 0), cdl);
                cdl = MakePair(MakeInstruction(SetCStackOpcode,
                        AsFixnum(lam->SlotCount) - AsFixnum(bd->Slot)), cdl);
            }
            else
            {
                cdl = MakePair(MakeInstruction(GetFrameOpcode, AsFixnum(bd->Slot)), cdl);
                cdl = MakePair(MakeInstruction(MakeBoxOpcode, 0), cdl);
                cdl = MakePair(MakeInstruction(SetFrameOpcode, AsFixnum(bd->Slot)), cdl);
            }
        }
    }

    cdl = GPassSequence(lam, cdl, lam->Body, TailCallFlag);

    FAssert(ProcedureP(lam->Procedure));

//    AsProcedure(lam->Procedure)->Code = ListToVector(ReverseListModify(cdl));
    Modify(FProcedure, lam->Procedure, Code, ListToVector(ReverseListModify(cdl)));
    return(lam->Procedure);
}
