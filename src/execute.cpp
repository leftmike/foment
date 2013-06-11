/*

Foment

*/

//#include <stdio.h>
#include <malloc.h>
#include "foment.hpp"
#include "execute.hpp"

FObject WrongNumberOfArguments;
FObject NotCallable;
FObject UnexpectedNumberOfValues;
FObject UndefinedMessage;

static FObject DynamicEnvironment;

// ---- Procedure ----

static FObject MakeProcedure(FObject nam, FObject cv, int ac, FObject ra, unsigned int fl)
{
    FProcedure * p = (FProcedure *) MakeObject(ProcedureTag, sizeof(FProcedure));
    p->Name = SyntaxToDatum(nam);
    p->Code = cv;
    p->RestArg = ra;
    p->ArgCount = ac;
    p->Flags = fl;

    FObject obj = AsObject(p);
    FAssert(ObjectLength(obj) == sizeof(FProcedure));
    return(obj);
}

FObject MakeProcedure(FObject nam, FObject cv, int ac, FObject ra)
{
    return(MakeProcedure(nam, cv, ac, ra, 0));
}

// ---- Instruction ----

static char * Opcodes[] =
{
    "check-count",
    "rest-arg",
    "make-list",
    "push-cstack",
    "push-no-value",
    "push-want-values",
    "pop-cstack",
    "save-frame",
    "restore-frame",
    "make-frame",
    "push-frame",
    "get-cstack",
    "set-cstack",
    "get-frame",
    "set-frame",
    "get-vector",
    "set-vector",
    "get-global",
    "set-global",
    "get-box",
    "set-box",
    "discard-result",
    "pop-astack",
    "duplicate",
    "return",
    "call",
    "call-proc",
    "call-prim",
    "tail-call",
    "tail-call-proc",
    "tail-call-prim",
    "set-arg-count",
    "make-closure",
    "if-false",
    "if-eqv?",
    "goto-relative",
    "goto-absolute",
    "check-values",
    "rest-values",
    "values",
    "apply",
    "case-lambda"
};

void WriteInstruction(FObject port, FObject obj, int df)
{
    FAssert(InstructionP(obj));

    FCh s[16];
    int sl = NumberAsString(InstructionArg(obj), s, 10);

    PutStringC(port, "#<");

    if (InstructionOpcode(obj) < 0 || InstructionOpcode(obj) >= sizeof(Opcodes) / sizeof(char *))
        PutStringC(port, "unknown");
    else
        PutStringC(port, Opcodes[InstructionOpcode(obj)]);

    PutStringC(port, ": ");
    PutString(port, s, sl);
    PutCh(port, '>');
}

// --------

FObject Execute(FObject op, int argc, FObject argv[])
{
    FExecuteState es;

    es.AStackSize = 1024 * 4;
    es.AStack = (FObject *) malloc(es.AStackSize * sizeof(FObject));
    es.AStackPtr = 0;

    es.CStackSize = 1024 * 16;
    es.CStack = (FObject *) malloc(es.CStackSize * sizeof(FObject));

    int adx;
    for (adx = 0; adx < argc; adx++)
    {
        es.AStack[es.AStackPtr] = argv[adx];
        es.AStackPtr += 1;
    }

    es.CStackPtr = 0;
    es.Proc = op;
    FAssert(ProcedureP(es.Proc));
    FAssert(VectorP(AsProcedure(es.Proc)->Code));

    es.IP = 0;
    es.Frame = NoValueObject;
    es.ArgCount = argc;

    EnterExecute(&es);
    try
    {
        for (;;)
        {
            AllowGC();

            FAssert(VectorP(AsProcedure(es.Proc)->Code));
            FAssert(es.IP >= 0);
            FAssert(es.IP < AsVector(AsProcedure(es.Proc)->Code)->Length);

            FObject obj = AsVector(AsProcedure(es.Proc)->Code)->Vector[es.IP];
            es.IP += 1;

            if (InstructionP(obj) == 0)
            {
                es.AStack[es.AStackPtr] = obj;
                es.AStackPtr += 1;
//Write(StandardOutput, obj, 0);
//printf("\n");
            }
            else
            {
//printf("%s.%d %d %d\n", Opcodes[InstructionOpcode(obj)], InstructionArg(obj), es.CStackPtr, es.AStackPtr);
                switch (InstructionOpcode(obj))
                {
                case CheckCountOpcode:
                    if (es.ArgCount != InstructionArg(obj))
                        RaiseException(Assertion, AsProcedure(es.Proc)->Name,
                                WrongNumberOfArguments, EmptyListObject);
                    break;

                case RestArgOpcode:
                    FAssert(InstructionArg(obj) >= 0);

                    if (es.ArgCount < InstructionArg(obj))
                        RaiseException(Assertion, AsProcedure(es.Proc)->Name,
                                WrongNumberOfArguments, EmptyListObject);
                    else if (es.ArgCount == InstructionArg(obj))
                    {
                        es.AStack[es.AStackPtr] = EmptyListObject;
                        es.AStackPtr += 1;
                    }
                    else
                    {
                        FObject lst = EmptyListObject;

                        int ac = es.ArgCount;
                        while (ac > InstructionArg(obj))
                        {
                            es.AStackPtr -= 1;
                            lst = MakePair(es.AStack[es.AStackPtr], lst);
                            ac -= 1;
                        }

                        es.AStack[es.AStackPtr] = lst;
                        es.AStackPtr += 1;
                    }
                    break;

                case MakeListOpcode:
                {
                    FAssert(InstructionArg(obj) > 0);

                    FObject lst = EmptyListObject;
                    int ac = InstructionArg(obj);
                    while (ac > 0)
                    {
                        es.AStackPtr -= 1;
                        lst = MakePair(es.AStack[es.AStackPtr], lst);
                        ac -= 1;
                    }

                    es.AStack[es.AStackPtr] = lst;
                    es.AStackPtr += 1;
                    break;
                }

                case PushCStackOpcode:
                {
                    FFixnum arg = InstructionArg(obj);

                    while (arg > 0)
                    {
                        FAssert(es.AStackPtr > 0);

                        es.AStackPtr -= 1;
                        es.CStack[es.CStackPtr] = es.AStack[es.AStackPtr];
                        es.CStackPtr += 1;
                        arg -= 1;
                    }
                    break;
                }

                case PushNoValueOpcode:
                {
                    FFixnum arg = InstructionArg(obj);

                    while (arg > 0)
                    {
                        es.CStack[es.CStackPtr] = NoValueObject;
                        es.CStackPtr += 1;
                        arg -= 1;
                    }

                    break;
                }

                case PushWantValuesOpcode:
                    es.CStack[es.CStackPtr] = WantValuesObject;
                    es.CStackPtr += 1;
                    break;

                case PopCStackOpcode:
                    FAssert(es.CStackPtr >= InstructionArg(obj));

                    es.CStackPtr -= InstructionArg(obj);
                    break;

                case SaveFrameOpcode:
                    es.CStack[es.CStackPtr] = es.Frame;
                    es.CStackPtr += 1;
                    break;

                case RestoreFrameOpcode:
                    FAssert(es.CStackPtr > 0);

                    es.CStackPtr -= 1;
                    es.Frame = es.CStack[es.CStackPtr];
                    break;

                case MakeFrameOpcode:
                    es.Frame = MakeVector(InstructionArg(obj), 0, NoValueObject);
                    break;

                case PushFrameOpcode:
                    es.AStack[es.AStackPtr] = es.Frame;
                    es.AStackPtr += 1;
                    break;

                case GetCStackOpcode:
                    FAssert(InstructionArg(obj) <= es.CStackPtr);
                    FAssert(InstructionArg(obj) > 0);

                    es.AStack[es.AStackPtr] = es.CStack[es.CStackPtr - InstructionArg(obj)];
                    es.AStackPtr += 1;
                    break;

                case SetCStackOpcode:
                    FAssert(InstructionArg(obj) <= es.CStackPtr);
                    FAssert(InstructionArg(obj) > 0);
                    FAssert(es.AStackPtr > 0);

                    es.AStackPtr -= 1;
                    es.CStack[es.CStackPtr - InstructionArg(obj)] = es.AStack[es.AStackPtr];
                    break;

                case GetFrameOpcode:
                    FAssert(VectorP(es.Frame));
                    FAssert(InstructionArg(obj) < AsVector(es.Frame)->Length);

                    es.AStack[es.AStackPtr] = AsVector(es.Frame)->Vector[InstructionArg(obj)];
                    es.AStackPtr += 1;
                    break;

                case SetFrameOpcode:
                    FAssert(VectorP(es.Frame));
                    FAssert(InstructionArg(obj) < AsVector(es.Frame)->Length);
                    FAssert(es.AStackPtr > 0);

                    es.AStackPtr -= 1;
    //                AsVector(es.Frame)->Vector[InstructionArg(obj)] = es.AStack[es.AStackPtr];
                    ModifyVector(es.Frame, InstructionArg(obj), es.AStack[es.AStackPtr]);
                    break;

                case GetVectorOpcode:
                    FAssert(es.AStackPtr > 0);
                    FAssert(VectorP(es.AStack[es.AStackPtr - 1]));
                    FAssert(InstructionArg(obj) < AsVector(es.AStack[es.AStackPtr - 1])->Length);

                    es.AStack[es.AStackPtr - 1] = AsVector(es.AStack[es.AStackPtr - 1])->Vector[
                            InstructionArg(obj)];
                    break;

                case SetVectorOpcode:
                    FAssert(es.AStackPtr > 1);
                    FAssert(VectorP(es.AStack[es.AStackPtr - 1]));
                    FAssert(InstructionArg(obj) < AsVector(es.AStack[es.AStackPtr - 1])->Length);

//                    AsVector(es.AStack[es.AStackPtr - 1])->Vector[InstructionArg(obj)] =
//                            es.AStack[es.AStackPtr - 2];
                    ModifyVector(es.AStack[es.AStackPtr - 1], InstructionArg(obj),
                            es.AStack[es.AStackPtr - 2]);
                    es.AStackPtr -= 2;
                    break;

                case GetGlobalOpcode:
                    FAssert(es.AStackPtr > 0);
                    FAssert(GlobalP(es.AStack[es.AStackPtr - 1]));
                    FAssert(BoxP(AsGlobal(es.AStack[es.AStackPtr - 1])->Box));

                    if (AsGlobal(es.AStack[es.AStackPtr - 1])->State == GlobalUndefined)
                    {
                        FAssert(AsGlobal(es.AStack[es.AStackPtr - 1])->Interactive == TrueObject);

                        RaiseException(Assertion, AsProcedure(es.Proc)->Name, UndefinedMessage,
                                List(es.AStack[es.AStackPtr - 1]));
                    }

                    es.AStack[es.AStackPtr - 1] = Unbox(
                            AsGlobal(es.AStack[es.AStackPtr - 1])->Box);
                    break;

                case SetGlobalOpcode:
                    FAssert(es.AStackPtr > 1);
                    FAssert(GlobalP(es.AStack[es.AStackPtr - 1]));
                    FAssert(BoxP(AsGlobal(es.AStack[es.AStackPtr - 1])->Box));

                    if (AsGlobal(es.AStack[es.AStackPtr - 1])->State == GlobalImported
                            || AsGlobal(es.AStack[es.AStackPtr - 1])->State == GlobalImportedModified)
                    {
                        FAssert(AsGlobal(es.AStack[es.AStackPtr - 1])->Interactive == TrueObject);

//                        AsGlobal(es.AStack[es.AStackPtr - 1])->Box = MakeBox(
//                                es.AStack[es.AStackPtr - 2]);
                        Modify(FGlobal, es.AStack[es.AStackPtr - 1], Box,
                                MakeBox(es.AStack[es.AStackPtr - 2]));
//                        AsGlobal(es.AStack[es.AStackPtr - 1])->State = GlobalDefined;
                        Modify(FGlobal, es.AStack[es.AStackPtr - 1], State, GlobalDefined);
                    }

//                    AsBox(AsGlobal(es.AStack[es.AStackPtr - 1])->Box)->Value =
                            es.AStack[es.AStackPtr - 2];
                    Modify(FBox, AsGlobal(es.AStack[es.AStackPtr - 1])->Box, Value,
                            es.AStack[es.AStackPtr - 2]);
                    es.AStackPtr -= 2;
                    break;

                case GetBoxOpcode:
                    FAssert(es.AStackPtr > 0);
                    FAssert(BoxP(es.AStack[es.AStackPtr - 1]));

                    es.AStack[es.AStackPtr - 1] = Unbox(es.AStack[es.AStackPtr - 1]);
                    break;

                case SetBoxOpcode:
                    FAssert(es.AStackPtr > 1);
                    FAssert(BoxP(es.AStack[es.AStackPtr - 1]));

//                    AsBox(es.AStack[es.AStackPtr - 1])->Value = es.AStack[es.AStackPtr - 2];
                    Modify(FBox, es.AStack[es.AStackPtr - 1], Value, es.AStack[es.AStackPtr - 2]);
                    es.AStackPtr -= 2;
                    break;

                case DiscardResultOpcode:
                    FAssert(es.AStackPtr >= 1);

                    es.AStackPtr -= 1;
                    break;

                case PopAStackOpcode:
                    FAssert(es.AStackPtr >= InstructionArg(obj));

                    es.AStackPtr -= InstructionArg(obj);
                    break;

                case DuplicateOpcode:
                    FAssert(es.AStackPtr >= 1);

                    es.AStack[es.AStackPtr] = es.AStack[es.AStackPtr - 1];
                    es.AStackPtr += 1;
                    break;

                case ReturnOpcode:
                    if (es.CStackPtr == 0)
                    {
                        FAssert(es.AStackPtr == 1);

                        FObject ret = es.AStack[0];

                        LeaveExecute(&es);
                        free(es.AStack);
                        free(es.CStack);
                        return(ret);
                    }

                    FAssert(es.CStackPtr >= 2);
                    FAssert(FixnumP(es.CStack[es.CStackPtr - 1]));
                    FAssert(ProcedureP(es.CStack[es.CStackPtr - 2]));

                    es.CStackPtr -= 1;
                    es.IP = AsFixnum(es.CStack[es.CStackPtr]);
                    es.CStackPtr -= 1;
                    es.Proc = es.CStack[es.CStackPtr];

                    FAssert(VectorP(AsProcedure(es.Proc)->Code));

                    es.Frame = NoValueObject;
                    break;

                case CallOpcode:
                    FAssert(es.AStackPtr > 0);

                    es.AStackPtr -= 1;
                    op = es.AStack[es.AStackPtr];
                    if (ProcedureP(op))
                    {
CallProcedure:
                        es.CStack[es.CStackPtr] = es.Proc;
                        es.CStackPtr += 1;
                        es.CStack[es.CStackPtr] = MakeFixnum(es.IP);
                        es.CStackPtr += 1;

                        es.Proc = op;
                        FAssert(VectorP(AsProcedure(es.Proc)->Code));

                        es.IP = 0;
                        es.Frame = NoValueObject;
                    }
                    else if (PrimitiveP(op))
                    {
CallPrimitive:
                        FAssert(es.AStackPtr >= es.ArgCount);

                        FObject ret = AsPrimitive(op)->PrimitiveFn(es.ArgCount,
                                es.AStack + es.AStackPtr - es.ArgCount);
                        es.AStackPtr -= es.ArgCount;
                        es.AStack[es.AStackPtr] = ret;
                        es.AStackPtr += 1;
                    }
                    else
                        RaiseException(Assertion, AsProcedure(es.Proc)->Name, NotCallable,
                                List(op));
                    break;

                case CallProcOpcode:
                    FAssert(es.AStackPtr > 0);

                    es.AStackPtr -= 1;
                    op = es.AStack[es.AStackPtr];

                    FAssert(ProcedureP(op));
                    goto CallProcedure;

                case CallPrimOpcode:
                    FAssert(es.AStackPtr > 0);

                    es.AStackPtr -= 1;
                    op = es.AStack[es.AStackPtr];

                    FAssert(PrimitiveP(op));
                    goto CallPrimitive;

                case TailCallOpcode:
                    FAssert(es.AStackPtr > 0);

                    es.AStackPtr -= 1;
                    op = es.AStack[es.AStackPtr];
                    if (ProcedureP(op))
                    {
TailCallProcedure:
                        es.Proc = op;
                        FAssert(VectorP(AsProcedure(es.Proc)->Code));

                        es.IP = 0;
                        es.Frame = NoValueObject;
                    }
                    else if (PrimitiveP(op))
                    {
TailCallPrimitive:
                        FAssert(es.AStackPtr >= es.ArgCount);

                        FObject ret = AsPrimitive(op)->PrimitiveFn(es.ArgCount,
                                es.AStack + es.AStackPtr - es.ArgCount);
                        es.AStackPtr -= es.ArgCount;
                        es.AStack[es.AStackPtr] = ret;
                        es.AStackPtr += 1;

                        if (es.CStackPtr == 0)
                        {
                            FAssert(es.AStackPtr == 1);

                            FObject ret = es.AStack[0];

                            LeaveExecute(&es);
                            free(es.AStack);
                            free(es.CStack);
                            return(ret);
                        }

                        FAssert(es.CStackPtr >= 2);

                        es.CStackPtr -= 1;
                        es.IP = AsFixnum(es.CStack[es.CStackPtr]);
                        es.CStackPtr -= 1;
                        es.Proc = es.CStack[es.CStackPtr];

                        FAssert(ProcedureP(es.Proc));
                        FAssert(VectorP(AsProcedure(es.Proc)->Code));

                        es.Frame = NoValueObject;
                    }
                    else
                        RaiseException(Assertion, AsProcedure(es.Proc)->Name, NotCallable,
                                List(op));
                    break;

                case TailCallProcOpcode:
                    FAssert(es.AStackPtr > 0);

                    es.AStackPtr -= 1;
                    op = es.AStack[es.AStackPtr];

                    FAssert(ProcedureP(op));

                    goto TailCallProcedure;

                case TailCallPrimOpcode:
                    FAssert(es.AStackPtr > 0);

                    es.AStackPtr -= 1;
                    op = es.AStack[es.AStackPtr];

                    FAssert(PrimitiveP(op));

                    goto TailCallPrimitive;

                case SetArgCountOpcode:
                    es.ArgCount = InstructionArg(obj);
                    break;

                case MakeClosureOpcode:
                {
                    FAssert(es.AStackPtr > 0);

                    FObject v[3];

                    es.AStackPtr -= 1;
                    v[0] = es.AStack[es.AStackPtr - 1];
                    v[1] = es.AStack[es.AStackPtr];
                    v[2] = MakeInstruction(TailCallOpcode, 0);
                    FObject proc = MakeProcedure(NoValueObject, MakeVector(3, v, NoValueObject), 0,
                            FalseObject, PROCEDURE_FLAG_CLOSURE);
                    es.AStack[es.AStackPtr - 1] = proc;
                    break;
                }

                case IfFalseOpcode:
                    FAssert(es.AStackPtr > 0);
                    FAssert(es.IP + InstructionArg(obj) >= 0);

                    es.AStackPtr -= 1;
                    if (es.AStack[es.AStackPtr] == FalseObject)
                        es.IP += InstructionArg(obj);
                    break;

                case IfEqvPOpcode:
                    FAssert(es.AStackPtr > 1);
                    FAssert(es.IP + InstructionArg(obj) >= 0);

                    es.AStackPtr -= 1;
                    if (es.AStack[es.AStackPtr] == es.AStack[es.AStackPtr - 1])
                        es.IP += InstructionArg(obj);
                    break;

                case GotoRelativeOpcode:
                    FAssert(es.IP + InstructionArg(obj) >= 0);

                    es.IP += InstructionArg(obj);
                    break;

                case GotoAbsoluteOpcode:
                    FAssert(InstructionArg(obj) >= 0);

                    es.IP = InstructionArg(obj);
                    break;

                case CheckValuesOpcode:
                    FAssert(es.AStackPtr > 0);
                    FAssert(InstructionArg(obj) != 1);

                    if (ValuesCountP(es.AStack[es.AStackPtr - 1]))
                    {
                        es.AStackPtr -= 1;
                        if (AsValuesCount(es.AStack[es.AStackPtr]) != InstructionArg(obj))
                            RaiseException(Assertion, AsProcedure(es.Proc)->Name,
                                    UnexpectedNumberOfValues, EmptyListObject);
                    }
                    else
                        RaiseException(Assertion, AsProcedure(es.Proc)->Name,
                                UnexpectedNumberOfValues, EmptyListObject);
                    break;

                case RestValuesOpcode:
                {
                    FAssert(es.AStackPtr > 0);
                    FAssert(InstructionArg(obj) >= 0);

                    int vc;

                    if (ValuesCountP(es.AStack[es.AStackPtr - 1]))
                    {
                        es.AStackPtr -= 1;
                        vc = AsValuesCount(es.AStack[es.AStackPtr]);
                    }
                    else
                        vc = 1;

                    if (vc < InstructionArg(obj))
                        RaiseException(Assertion, AsProcedure(es.Proc)->Name,
                                UnexpectedNumberOfValues, EmptyListObject);
                    else if (vc == InstructionArg(obj))
                    {
                        es.AStack[es.AStackPtr] = EmptyListObject;
                        es.AStackPtr += 1;
                    }
                    else
                    {
                        FObject lst = EmptyListObject;

                        while (vc > InstructionArg(obj))
                        {
                            es.AStackPtr -= 1;
                            lst = MakePair(es.AStack[es.AStackPtr], lst);
                            vc -= 1;
                        }

                        es.AStack[es.AStackPtr] = lst;
                        es.AStackPtr += 1;
                    }

                    break;
                }

                case ValuesOpcode:
                    if (es.ArgCount != 1)
                    {
                        if (es.CStackPtr >= 3 && WantValuesObjectP(es.CStack[es.CStackPtr - 3]))
                        {
                            es.AStack[es.AStackPtr] = MakeValuesCount(es.ArgCount);
                            es.AStackPtr += 1;
                        }
                        else
                        {
                            FAssert(es.CStackPtr >= 2);
                            FAssert(FixnumP(es.CStack[es.CStackPtr - 1]));
                            FAssert(ProcedureP(es.CStack[es.CStackPtr - 2]));
                            FAssert(VectorP(AsProcedure(es.CStack[es.CStackPtr - 2])->Code));

                            FObject cd = AsVector(AsProcedure(
                                    es.CStack[es.CStackPtr - 2])->Code)->Vector[
                                    AsFixnum(es.CStack[es.CStackPtr - 1])];
                            if (InstructionP(cd) == 0 || InstructionOpcode(cd)
                                    != DiscardResultOpcode)
                               RaiseExceptionC(Assertion, "values",
                                       "values: caller not expecting multiple values",
                                       List(AsProcedure(es.CStack[es.CStackPtr - 2])->Name));

                            if (es.ArgCount == 0)
                            {
                                es.AStack[es.AStackPtr] = NoValueObject;
                                es.AStackPtr += 1;
                            }
                            else
                            {
                                FAssert(es.AStackPtr >= es.ArgCount);

                                es.AStackPtr -= (es.ArgCount - 1);
                            }
                        }
                    }
                    break;

                case ApplyOpcode:
                {
                    if (es.ArgCount < 2)
                       RaiseExceptionC(Assertion, "apply",
                               "apply: expected at least two arguments", EmptyListObject);

                    FObject prc = es.AStack[es.AStackPtr - es.ArgCount];
                    FObject lst = es.AStack[es.AStackPtr - 1];

                    int adx = es.ArgCount;
                    while (adx > 2)
                    {
                        es.AStack[es.AStackPtr - adx] = es.AStack[es.AStackPtr - adx + 1];
                        adx -= 1;
                    }

                    es.ArgCount -= 2;
                    es.AStackPtr -= 2;

                    FObject ptr = lst;
                    while (PairP(ptr))
                    {
                        es.AStack[es.AStackPtr] = First(ptr);
                        es.AStackPtr += 1;
                        es.ArgCount += 1;
                        ptr = Rest(ptr);
                    }

                    if (ptr != EmptyListObject)
                       RaiseExceptionC(Assertion, "apply", "apply: expected a proper list",
                               List(lst));

                    es.AStack[es.AStackPtr] = prc;
                    es.AStackPtr += 1;
                    break;
                }

                case CaseLambdaOpcode:
                    int cc = InstructionArg(obj);
                    int idx = 0;

                    while (cc > 0)
                    {
                        FAssert(VectorP(AsProcedure(es.Proc)->Code));
                        FAssert(es.IP + idx >= 0);
                        FAssert(es.IP + idx < AsVector(AsProcedure(es.Proc)->Code)->Length);

                        FObject prc = AsVector(AsProcedure(es.Proc)->Code)->Vector[es.IP + idx];

                        FAssert(ProcedureP(prc));

                        if ((AsProcedure(prc)->RestArg == TrueObject
                                && es.ArgCount + 1 >= AsProcedure(prc)->ArgCount)
                                || AsProcedure(prc)->ArgCount == es.ArgCount)
                        {
                            es.Proc = prc;
                            FAssert(VectorP(AsProcedure(es.Proc)->Code));

                            es.IP = 0;
                            es.Frame = NoValueObject;

                            break;
                        }

                        idx += 1;
                        cc -= 1;
                    }

                    if (cc == 0)
                        RaiseExceptionC(Assertion, "case-lambda", "case-lambda: no matching case",
                                List(MakeFixnum(es.ArgCount)));
                    break;

                }
            }
        }
    }
    catch (FObject obj)
    {
        LeaveExecute(&es);
        throw obj;
    }
}

#define ParameterP(obj) (ProcedureP(obj) && AsProcedure(obj)->Flags & PROCEDURE_FLAG_PARAMETER)

Define("get-parameter", GetParameterPrimitive)(int argc, FObject argv[])
{
    if (argc != 2)
        RaiseExceptionC(Assertion, "get-parameter", "get-parameter: expected two arguments",
                EmptyListObject);

    if (ParameterP(argv[0]) == 0)
        RaiseExceptionC(Assertion, "get-parameter", "get-parameter: expected a parameter",
                List(argv[0]));

    if (PairP(argv[1]) == 0)
        RaiseExceptionC(Assertion, "get-parameter", "get-parameter: expected a pair",
                List(argv[1]));

    FObject lst = DynamicEnvironment;
    while (PairP(lst))
    {
        FAssert(PairP(First(lst)));

        if (First(First(lst)) == argv[0])
            return(Rest(First(lst)));

        lst = Rest(lst);
    }

    FAssert(lst == EmptyListObject);

    return(Rest(argv[1]));
}

Define("set-parameter!", SetParameterPrimitive)(int argc, FObject argv[])
{
    if (argc != 3)
        RaiseExceptionC(Assertion, "set-parameter!", "set-parameter!: expected three arguments",
                EmptyListObject);

    if (ParameterP(argv[0]) == 0)
        RaiseExceptionC(Assertion, "set-parameter!", "set-parameter!: expected a parameter",
                List(argv[0]));

    if (PairP(argv[1]) == 0)
        RaiseExceptionC(Assertion, "set-parameter!", "set-parameter!: expected a pair",
                List(argv[1]));

    FObject lst = DynamicEnvironment;
    while (PairP(lst))
    {
        FAssert(PairP(First(lst)));

        if (First(First(lst)) == argv[0])
        {
//            AsPair(First(lst))->Rest = argv[2];
            Modify(FPair, First(lst), Rest, argv[2]);
            return(NoValueObject);
        }

        lst = Rest(lst);
    }

    FAssert(lst == EmptyListObject);

//    AsPair(argv[1])->Rest = argv[2];
    Modify(FPair, argv[1], Rest, argv[2]);
    return(NoValueObject);
}

Define("procedure->parameter", ProcedureToParameterPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(Assertion, "procedure->parameter",
                "procedure->parameter: expected one argument", EmptyListObject);

    if (ProcedureP(argv[0]) == 0)
         RaiseExceptionC(Assertion, "procedure->parameter",
                 "procedure->parameter: expected a procedure", List(argv[0]));

    AsProcedure(argv[0])->Flags |= PROCEDURE_FLAG_PARAMETER;
#ifdef FOMENT_GCCHK
    CheckSumObject(argv[0]);
#endif // FOMENT_GCCHK

    return(NoValueObject);
}

static FPrimitive * Primitives[] =
{
    &GetParameterPrimitive,
    &SetParameterPrimitive,
    &ProcedureToParameterPrimitive
};

void SetupExecute()
{
    FObject v[2];

    WrongNumberOfArguments = MakeStringC("wrong number of arguments");
    Root(&WrongNumberOfArguments);

    NotCallable = MakeStringC("not callable");
    Root(&NotCallable);

    UnexpectedNumberOfValues = MakeStringC("unexpected number of values");
    Root(&UnexpectedNumberOfValues);

    UndefinedMessage = MakeStringC("variable is undefined");
    Root(&UndefinedMessage);

    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(Bedrock, BedrockLibrary, Primitives[idx]);

    DynamicEnvironment = EmptyListObject;
    Root(&DynamicEnvironment);

    v[0] = MakeInstruction(ValuesOpcode, 0);
    v[1] = MakeInstruction(ReturnOpcode, 0);

    LibraryExport(BedrockLibrary,
            EnvironmentSetC(Bedrock, "values", MakeProcedure(StringCToSymbol("values"),
            MakeVector(2, v, NoValueObject), 1, TrueObject)));

    v[0] = MakeInstruction(ApplyOpcode, 0);
    v[1] = MakeInstruction(TailCallOpcode, 0);

    LibraryExport(BedrockLibrary,
            EnvironmentSetC(Bedrock, "apply", MakeProcedure(StringCToSymbol("apply"),
            MakeVector(2, v, NoValueObject), 2, TrueObject)));
}
