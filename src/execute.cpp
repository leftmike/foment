/*

Foment

-- Racket: prompts, aborting to a prompt, capturing a continuation up to a prompt, and
composing the current continuation with a captured continuation.
(call-with-continuation-prompt <proc> <prompt-tag> <handler> <arg> ...)
(abort-current-continuation <prompt-tag> <val> ...)
(call-with-current-continuation <proc> [<prompt-tag>])
(call-with-composable-continuation <proc> [<prompt-tag>])

(continuation-capture <proc> <mark>) -- call <proc> with the current-continuation upto <mark>
captured as an object

(compose-continuation <cont> <thunk>) -- call <thunk> in continuation <cont>

-- get rid of AVLTree

-- parameter-wind: push value on stack
-- parameter-unwind: pop stack

-- when a continuation is restored, need to run a handler which runs the befores and the parameters

-- call/cc: need to save and restore MarkStack and IndexStack

-- dynamic-wind
-- exceptions

Set of all parameter bindings at a given time is called the dynamic environment.
The system implicitly maintains a current exception handler in the dynamic enviroment.

-- guard
(guard (<variable> <cond-clause> ...) <body>)
<body> has (<variable> <cond-clause> ...) as exception handler
(<variable> ...) has a continuation and dynamic environment of guard expression
if no <cond-clause> matches then raise-continuable is invoked on object with
current exception handler being that of guard and the dynamic environment of the original
call to raise or raise-continuable

-- dynamic-wind
(dynamic-wind <before> <thunk> <after>)
whenever control enters dynamic extent of <thunk> then <before> is called first
whenever control leaves dynamic extent of <thunk> then <after> is called after
<before> and <after> are called in the same dynamic environment as the call to dynamic-wind

-- with-exception-handler
(with-exception-handler <handler> <thunk>)
<handler> is the current exception handler for the extent of <thunk>

-- raise
invoke current exception handler with same dynamic environment as raise, except use the
previous current exception handler as the current exception handler; if the handler returns,
a secondary exception is raised

-- raise-continuable
like raise, but if the exception handler returns, then it is reinstalled as the current
exception handler and the values that it returns get returned by the call to raise-continuable

*/

#include <windows.h>
#include <stdio.h>
#include <malloc.h>
#include "foment.hpp"
#include "execute.hpp"
#include "syncthrd.hpp"

#define MAXIMUM_CONTINUATION_ARGS 16

// ---- Procedure ----

FObject MakeProcedure(FObject nam, FObject cv, int ac, unsigned int fl)
{
    FProcedure * p = (FProcedure *) MakeObject(sizeof(FProcedure), ProcedureTag);
    p->Reserved = MakeLength(ac, ProcedureTag) | fl;
    p->Name = SyntaxToDatum(nam);
    p->Code = cv;

    return(p);
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
    "case-lambda",
    "call-with-cc",
    "call-continuation",
    "return-from",
    "mark-continuation",
    "pop-mark-stack"
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

static FObject MarkListRef(FObject ml, FObject key, FObject def)
{
    while (PairP(ml))
    {
        FAssert(PairP(First(ml)));

        if (EqP(First(First(ml)), key))
            return(Rest(First(ml)));

        ml = Rest(ml);
    }

    FAssert(ml == EmptyListObject);

    return(def);
}

static FObject MarkListUpdate(FObject ml, FObject key, FObject val)
{
    FAssert(PairP(ml));
    FAssert(PairP(First(ml)));

    if (EqP(First(First(ml)), key))
        return(MakePair(MakePair(key, val), Rest(ml)));

    return(MakePair(First(ml), MarkListUpdate(Rest(ml), key, val)));
}

static FObject MarkListSet(FObject ml, FObject key, FObject val)
{
    FAssert(val != NotFoundObject);

    if (MarkListRef(ml, key, NotFoundObject) == NotFoundObject)
        return(MakePair(MakePair(key, val), ml));

    return(MarkListUpdate(ml, key, val));
}

static void MarkListWalkVisit(FObject ml, FWalkVisitFn wfn, FObject ctx)
{
    while (PairP(ml))
    {
        FAssert(PairP(First(ml)));

        wfn(First(First(ml)), Rest(First(ml)), ctx);
        ml = Rest(ml);
    }

    FAssert(ml == EmptyListObject);
}

FObject ExecuteThunk(FObject op)
{
    FThreadState * ts = GetThreadState();

    ts->AStackPtr = 1;
    ts->AStack[0] = op;
    ts->CStackPtr = 0;
    ts->Proc = R.ExecuteThunk;
    FAssert(ProcedureP(ts->Proc));
    FAssert(VectorP(AsProcedure(ts->Proc)->Code));

    ts->IP = 0;
    ts->Frame = NoValueObject;
    ts->ArgCount = 0;

    try
    {
        for (;;)
        {
            CheckForGC();

            FAssert(VectorP(AsProcedure(ts->Proc)->Code));
            FAssert(ts->IP >= 0);
            FAssert(ts->IP < (int) VectorLength(AsProcedure(ts->Proc)->Code));

            FObject obj = AsVector(AsProcedure(ts->Proc)->Code)->Vector[ts->IP];
            ts->IP += 1;

            if (InstructionP(obj) == 0)
            {
                ts->AStack[ts->AStackPtr] = obj;
                ts->AStackPtr += 1;
//WritePretty(R.StandardOutput, obj, 0);
//printf("\n");
            }
            else
            {
//printf("%s.%d %d %d\n", Opcodes[InstructionOpcode(obj)], InstructionArg(obj), ts->CStackPtr, ts->AStackPtr);
                switch (InstructionOpcode(obj))
                {
                case CheckCountOpcode:
                    if (ts->ArgCount != InstructionArg(obj))
                        RaiseException(R.Assertion, AsProcedure(ts->Proc)->Name,
                                R.WrongNumberOfArguments, EmptyListObject);
                    break;

                case RestArgOpcode:
                    FAssert(InstructionArg(obj) >= 0);

                    if (ts->ArgCount < InstructionArg(obj))
                        RaiseException(R.Assertion, AsProcedure(ts->Proc)->Name,
                                R.WrongNumberOfArguments, EmptyListObject);
                    else if (ts->ArgCount == InstructionArg(obj))
                    {
                        ts->AStack[ts->AStackPtr] = EmptyListObject;
                        ts->AStackPtr += 1;
                    }
                    else
                    {
                        FObject lst = EmptyListObject;

                        int ac = ts->ArgCount;
                        while (ac > InstructionArg(obj))
                        {
                            ts->AStackPtr -= 1;
                            lst = MakePair(ts->AStack[ts->AStackPtr], lst);
                            ac -= 1;
                        }

                        ts->AStack[ts->AStackPtr] = lst;
                        ts->AStackPtr += 1;
                    }
                    break;

                case MakeListOpcode:
                {
                    FAssert(InstructionArg(obj) > 0);

                    FObject lst = EmptyListObject;
                    int ac = InstructionArg(obj);
                    while (ac > 0)
                    {
                        ts->AStackPtr -= 1;
                        lst = MakePair(ts->AStack[ts->AStackPtr], lst);
                        ac -= 1;
                    }

                    ts->AStack[ts->AStackPtr] = lst;
                    ts->AStackPtr += 1;
                    break;
                }

                case PushCStackOpcode:
                {
                    FFixnum arg = InstructionArg(obj);

                    while (arg > 0)
                    {
                        FAssert(ts->AStackPtr > 0);

                        ts->AStackPtr -= 1;
                        ts->CStack[- ts->CStackPtr] = ts->AStack[ts->AStackPtr];
                        ts->CStackPtr += 1;
                        arg -= 1;
                    }
                    break;
                }

                case PushNoValueOpcode:
                {
                    FFixnum arg = InstructionArg(obj);

                    while (arg > 0)
                    {
                        ts->CStack[- ts->CStackPtr] = NoValueObject;
                        ts->CStackPtr += 1;
                        arg -= 1;
                    }

                    break;
                }

                case PushWantValuesOpcode:
                    ts->CStack[- ts->CStackPtr] = WantValuesObject;
                    ts->CStackPtr += 1;
                    break;

                case PopCStackOpcode:
                    FAssert(ts->CStackPtr >= InstructionArg(obj));

                    ts->CStackPtr -= InstructionArg(obj);
                    break;

                case SaveFrameOpcode:
                    ts->CStack[- ts->CStackPtr] = ts->Frame;
                    ts->CStackPtr += 1;
                    break;

                case RestoreFrameOpcode:
                    FAssert(ts->CStackPtr > 0);

                    ts->CStackPtr -= 1;
                    ts->Frame = ts->CStack[- ts->CStackPtr];
                    break;

                case MakeFrameOpcode:
                    ts->Frame = MakeVector(InstructionArg(obj), 0, NoValueObject);
                    break;

                case PushFrameOpcode:
                    ts->AStack[ts->AStackPtr] = ts->Frame;
                    ts->AStackPtr += 1;
                    break;

                case GetCStackOpcode:
                    FAssert(InstructionArg(obj) <= ts->CStackPtr);
                    FAssert(InstructionArg(obj) > 0);

                    ts->AStack[ts->AStackPtr] = ts->CStack[- (ts->CStackPtr - InstructionArg(obj))];
                    ts->AStackPtr += 1;
                    break;

                case SetCStackOpcode:
                    FAssert(InstructionArg(obj) <= ts->CStackPtr);
                    FAssert(InstructionArg(obj) > 0);
                    FAssert(ts->AStackPtr > 0);

                    ts->AStackPtr -= 1;
                    ts->CStack[- (ts->CStackPtr - InstructionArg(obj))] = ts->AStack[ts->AStackPtr];
                    break;

                case GetFrameOpcode:
                    FAssert(VectorP(ts->Frame));
                    FAssert(InstructionArg(obj) < VectorLength(ts->Frame));

                    ts->AStack[ts->AStackPtr] = AsVector(ts->Frame)->Vector[InstructionArg(obj)];
                    ts->AStackPtr += 1;
                    break;

                case SetFrameOpcode:
                    FAssert(VectorP(ts->Frame));
                    FAssert(InstructionArg(obj) < VectorLength(ts->Frame));
                    FAssert(ts->AStackPtr > 0);

                    ts->AStackPtr -= 1;
//                    AsVector(ts->Frame)->Vector[InstructionArg(obj)] = ts->AStack[ts->AStackPtr];
                    ModifyVector(ts->Frame, InstructionArg(obj), ts->AStack[ts->AStackPtr]);
                    break;

                case GetVectorOpcode:
                    FAssert(ts->AStackPtr > 0);
                    FAssert(VectorP(ts->AStack[ts->AStackPtr - 1]));
                    FAssert(InstructionArg(obj) < VectorLength(ts->AStack[ts->AStackPtr - 1]));

                    ts->AStack[ts->AStackPtr - 1] = AsVector(ts->AStack[ts->AStackPtr - 1])->Vector[
                            InstructionArg(obj)];
                    break;

                case SetVectorOpcode:
                    FAssert(ts->AStackPtr > 1);
                    FAssert(VectorP(ts->AStack[ts->AStackPtr - 1]));
                    FAssert(InstructionArg(obj) < VectorLength(ts->AStack[ts->AStackPtr - 1]));

//                    AsVector(ts->AStack[ts->AStackPtr - 1])->Vector[InstructionArg(obj)] =
//                            ts->AStack[ts->AStackPtr - 2];
                    ModifyVector(ts->AStack[ts->AStackPtr - 1], InstructionArg(obj),
                            ts->AStack[ts->AStackPtr - 2]);
                    ts->AStackPtr -= 2;
                    break;

                case GetGlobalOpcode:
                    FAssert(ts->AStackPtr > 0);
                    FAssert(GlobalP(ts->AStack[ts->AStackPtr - 1]));
                    FAssert(BoxP(AsGlobal(ts->AStack[ts->AStackPtr - 1])->Box));

                    if (AsGlobal(ts->AStack[ts->AStackPtr - 1])->State == GlobalUndefined)
                    {
                        FAssert(AsGlobal(ts->AStack[ts->AStackPtr - 1])->Interactive == TrueObject);

                        RaiseException(R.Assertion, AsProcedure(ts->Proc)->Name, R.UndefinedMessage,
                                List(ts->AStack[ts->AStackPtr - 1]));
                    }

                    ts->AStack[ts->AStackPtr - 1] = Unbox(
                            AsGlobal(ts->AStack[ts->AStackPtr - 1])->Box);
                    break;

                case SetGlobalOpcode:
                    FAssert(ts->AStackPtr > 1);
                    FAssert(GlobalP(ts->AStack[ts->AStackPtr - 1]));
                    FAssert(BoxP(AsGlobal(ts->AStack[ts->AStackPtr - 1])->Box));

                    if (AsGlobal(ts->AStack[ts->AStackPtr - 1])->State == GlobalImported
                            || AsGlobal(ts->AStack[ts->AStackPtr - 1])->State == GlobalImportedModified)
                    {
                        FAssert(AsGlobal(ts->AStack[ts->AStackPtr - 1])->Interactive == TrueObject);

//                        AsGlobal(ts->AStack[ts->AStackPtr - 1])->Box = MakeBox(
//                                ts->AStack[ts->AStackPtr - 2]);
                        Modify(FGlobal, ts->AStack[ts->AStackPtr - 1], Box,
                                MakeBox(ts->AStack[ts->AStackPtr - 2]));
//                        AsGlobal(ts->AStack[ts->AStackPtr - 1])->State = GlobalDefined;
                        Modify(FGlobal, ts->AStack[ts->AStackPtr - 1], State, GlobalDefined);
                    }

//                    AsBox(AsGlobal(ts->AStack[ts->AStackPtr - 1])->Box)->Value =
                            ts->AStack[ts->AStackPtr - 2];
                    Modify(FBox, AsGlobal(ts->AStack[ts->AStackPtr - 1])->Box, Value,
                            ts->AStack[ts->AStackPtr - 2]);
                    ts->AStackPtr -= 2;
                    break;

                case GetBoxOpcode:
                    FAssert(ts->AStackPtr > 0);
                    FAssert(BoxP(ts->AStack[ts->AStackPtr - 1]));

                    ts->AStack[ts->AStackPtr - 1] = Unbox(ts->AStack[ts->AStackPtr - 1]);
                    break;

                case SetBoxOpcode:
                    FAssert(ts->AStackPtr > 1);
                    FAssert(BoxP(ts->AStack[ts->AStackPtr - 1]));

//                    AsBox(ts->AStack[ts->AStackPtr - 1])->Value = ts->AStack[ts->AStackPtr - 2];
                    Modify(FBox, ts->AStack[ts->AStackPtr - 1], Value, ts->AStack[ts->AStackPtr - 2]);
                    ts->AStackPtr -= 2;
                    break;

                case DiscardResultOpcode:
                    FAssert(ts->AStackPtr >= 1);

                    ts->AStackPtr -= 1;
                    break;

                case PopAStackOpcode:
                    FAssert(ts->AStackPtr >= InstructionArg(obj));

                    ts->AStackPtr -= InstructionArg(obj);
                    break;

                case DuplicateOpcode:
                    FAssert(ts->AStackPtr >= 1);

                    ts->AStack[ts->AStackPtr] = ts->AStack[ts->AStackPtr - 1];
                    ts->AStackPtr += 1;
                    break;

                case ReturnOpcode:
                    FAssert(ts->CStackPtr >= 2);
                    FAssert(FixnumP(ts->CStack[- (ts->CStackPtr - 1)]));
                    FAssert(ProcedureP(ts->CStack[- (ts->CStackPtr - 2)]));

                    ts->CStackPtr -= 1;
                    ts->IP = AsFixnum(ts->CStack[- ts->CStackPtr]);
                    ts->CStackPtr -= 1;
                    ts->Proc = ts->CStack[- ts->CStackPtr];

                    FAssert(VectorP(AsProcedure(ts->Proc)->Code));

                    ts->Frame = NoValueObject;
                    break;

                case CallOpcode:
                    FAssert(ts->AStackPtr > 0);

                    ts->AStackPtr -= 1;
                    op = ts->AStack[ts->AStackPtr];
                    if (ProcedureP(op))
                    {
CallProcedure:
                        ts->CStack[- ts->CStackPtr] = ts->Proc;
                        ts->CStackPtr += 1;
                        ts->CStack[- ts->CStackPtr] = MakeFixnum(ts->IP);
                        ts->CStackPtr += 1;

                        ts->Proc = op;
                        FAssert(VectorP(AsProcedure(ts->Proc)->Code));

                        ts->IP = 0;
                        ts->Frame = NoValueObject;
                    }
                    else if (PrimitiveP(op))
                    {
CallPrimitive:
                        FAssert(ts->AStackPtr >= ts->ArgCount);

                        FObject ret = AsPrimitive(op)->PrimitiveFn(ts->ArgCount,
                                ts->AStack + ts->AStackPtr - ts->ArgCount);
                        ts->AStackPtr -= ts->ArgCount;
                        ts->AStack[ts->AStackPtr] = ret;
                        ts->AStackPtr += 1;
                    }
                    else
                        RaiseException(R.Assertion, AsProcedure(ts->Proc)->Name, R.NotCallable,
                                List(op));
                    break;

                case CallProcOpcode:
                    FAssert(ts->AStackPtr > 0);

                    ts->AStackPtr -= 1;
                    op = ts->AStack[ts->AStackPtr];

                    FAssert(ProcedureP(op));
                    goto CallProcedure;

                case CallPrimOpcode:
                    FAssert(ts->AStackPtr > 0);

                    ts->AStackPtr -= 1;
                    op = ts->AStack[ts->AStackPtr];

                    FAssert(PrimitiveP(op));
                    goto CallPrimitive;

                case TailCallOpcode:
                    FAssert(ts->AStackPtr > 0);

                    ts->AStackPtr -= 1;
                    op = ts->AStack[ts->AStackPtr];
TailCall:
                    if (ProcedureP(op))
                    {
TailCallProcedure:
                        ts->Proc = op;
                        FAssert(VectorP(AsProcedure(ts->Proc)->Code));

                        ts->IP = 0;
                        ts->Frame = NoValueObject;
                    }
                    else if (PrimitiveP(op))
                    {
TailCallPrimitive:
                        FAssert(ts->AStackPtr >= ts->ArgCount);

                        FObject ret = AsPrimitive(op)->PrimitiveFn(ts->ArgCount,
                                ts->AStack + ts->AStackPtr - ts->ArgCount);
                        ts->AStackPtr -= ts->ArgCount;
                        ts->AStack[ts->AStackPtr] = ret;
                        ts->AStackPtr += 1;

                        FAssert(ts->CStackPtr >= 2);

                        ts->CStackPtr -= 1;
                        ts->IP = AsFixnum(ts->CStack[- ts->CStackPtr]);
                        ts->CStackPtr -= 1;
                        ts->Proc = ts->CStack[- ts->CStackPtr];

                        FAssert(ProcedureP(ts->Proc));
                        FAssert(VectorP(AsProcedure(ts->Proc)->Code));

                        ts->Frame = NoValueObject;
                    }
                    else
                        RaiseException(R.Assertion, AsProcedure(ts->Proc)->Name, R.NotCallable,
                                List(op));
                    break;

                case TailCallProcOpcode:
                    FAssert(ts->AStackPtr > 0);

                    ts->AStackPtr -= 1;
                    op = ts->AStack[ts->AStackPtr];

                    FAssert(ProcedureP(op));

                    goto TailCallProcedure;

                case TailCallPrimOpcode:
                    FAssert(ts->AStackPtr > 0);

                    ts->AStackPtr -= 1;
                    op = ts->AStack[ts->AStackPtr];

                    FAssert(PrimitiveP(op));

                    goto TailCallPrimitive;

                case SetArgCountOpcode:
                    ts->ArgCount = InstructionArg(obj);
                    break;

                case MakeClosureOpcode:
                {
                    FAssert(ts->AStackPtr > 0);

                    FObject v[3];

                    ts->AStackPtr -= 1;
                    v[0] = ts->AStack[ts->AStackPtr - 1];
                    v[1] = ts->AStack[ts->AStackPtr];
                    v[2] = MakeInstruction(TailCallProcOpcode, 0);
                    FObject proc = MakeProcedure(NoValueObject, MakeVector(3, v, NoValueObject), 0,
                            PROCEDURE_FLAG_CLOSURE);
                    ts->AStack[ts->AStackPtr - 1] = proc;
                    break;
                }

                case IfFalseOpcode:
                    FAssert(ts->AStackPtr > 0);
                    FAssert(ts->IP + InstructionArg(obj) >= 0);

                    ts->AStackPtr -= 1;
                    if (ts->AStack[ts->AStackPtr] == FalseObject)
                        ts->IP += InstructionArg(obj);
                    break;

                case IfEqvPOpcode:
                    FAssert(ts->AStackPtr > 1);
                    FAssert(ts->IP + InstructionArg(obj) >= 0);

                    ts->AStackPtr -= 1;
                    if (ts->AStack[ts->AStackPtr] == ts->AStack[ts->AStackPtr - 1])
                        ts->IP += InstructionArg(obj);
                    break;

                case GotoRelativeOpcode:
                    FAssert(ts->IP + InstructionArg(obj) >= 0);

                    ts->IP += InstructionArg(obj);
                    break;

                case GotoAbsoluteOpcode:
                    FAssert(InstructionArg(obj) >= 0);

                    ts->IP = InstructionArg(obj);
                    break;

                case CheckValuesOpcode:
                    FAssert(ts->AStackPtr > 0);
                    FAssert(InstructionArg(obj) != 1);

                    if (ValuesCountP(ts->AStack[ts->AStackPtr - 1]))
                    {
                        ts->AStackPtr -= 1;
                        if (AsValuesCount(ts->AStack[ts->AStackPtr]) != InstructionArg(obj))
                            RaiseException(R.Assertion, AsProcedure(ts->Proc)->Name,
                                    R.UnexpectedNumberOfValues, EmptyListObject);
                    }
                    else
                        RaiseException(R.Assertion, AsProcedure(ts->Proc)->Name,
                                R.UnexpectedNumberOfValues, EmptyListObject);
                    break;

                case RestValuesOpcode:
                {
                    FAssert(ts->AStackPtr > 0);
                    FAssert(InstructionArg(obj) >= 0);

                    int vc;

                    if (ValuesCountP(ts->AStack[ts->AStackPtr - 1]))
                    {
                        ts->AStackPtr -= 1;
                        vc = AsValuesCount(ts->AStack[ts->AStackPtr]);
                    }
                    else
                        vc = 1;

                    if (vc < InstructionArg(obj))
                        RaiseException(R.Assertion, AsProcedure(ts->Proc)->Name,
                                R.UnexpectedNumberOfValues, EmptyListObject);
                    else if (vc == InstructionArg(obj))
                    {
                        ts->AStack[ts->AStackPtr] = EmptyListObject;
                        ts->AStackPtr += 1;
                    }
                    else
                    {
                        FObject lst = EmptyListObject;

                        while (vc > InstructionArg(obj))
                        {
                            ts->AStackPtr -= 1;
                            lst = MakePair(ts->AStack[ts->AStackPtr], lst);
                            vc -= 1;
                        }

                        ts->AStack[ts->AStackPtr] = lst;
                        ts->AStackPtr += 1;
                    }

                    break;
                }

                case ValuesOpcode:
                    if (ts->ArgCount != 1)
                    {
                        if (ts->CStackPtr >= 3
                                && WantValuesObjectP(ts->CStack[- (ts->CStackPtr - 3)]))
                        {
                            ts->AStack[ts->AStackPtr] = MakeValuesCount(ts->ArgCount);
                            ts->AStackPtr += 1;
                        }
                        else
                        {
                            FAssert(ts->CStackPtr >= 2);
                            FAssert(FixnumP(ts->CStack[- (ts->CStackPtr - 1)]));
                            FAssert(ProcedureP(ts->CStack[- (ts->CStackPtr - 2)]));
                            FAssert(VectorP(AsProcedure(ts->CStack[- (ts->CStackPtr - 2)])->Code));

                            FObject cd = AsVector(AsProcedure(
                                    ts->CStack[- (ts->CStackPtr - 2)])->Code)->Vector[
                                    AsFixnum(ts->CStack[- (ts->CStackPtr - 1)])];
                            if (InstructionP(cd) == 0 || InstructionOpcode(cd)
                                    != DiscardResultOpcode)
                               RaiseExceptionC(R.Assertion, "values",
                                       "values: caller not expecting multiple values",
                                       List(AsProcedure(ts->CStack[- (ts->CStackPtr - 2)])->Name));

                            if (ts->ArgCount == 0)
                            {
                                ts->AStack[ts->AStackPtr] = NoValueObject;
                                ts->AStackPtr += 1;
                            }
                            else
                            {
                                FAssert(ts->AStackPtr >= ts->ArgCount);

                                ts->AStackPtr -= (ts->ArgCount - 1);
                            }
                        }
                    }
                    break;

                case ApplyOpcode:
                {
                    if (ts->ArgCount < 2)
                       RaiseExceptionC(R.Assertion, "apply",
                               "apply: expected at least two arguments", EmptyListObject);

                    FObject prc = ts->AStack[ts->AStackPtr - ts->ArgCount];
                    FObject lst = ts->AStack[ts->AStackPtr - 1];

                    int adx = ts->ArgCount;
                    while (adx > 2)
                    {
                        ts->AStack[ts->AStackPtr - adx] = ts->AStack[ts->AStackPtr - adx + 1];
                        adx -= 1;
                    }

                    ts->ArgCount -= 2;
                    ts->AStackPtr -= 2;

                    FObject ptr = lst;
                    while (PairP(ptr))
                    {
                        ts->AStack[ts->AStackPtr] = First(ptr);
                        ts->AStackPtr += 1;
                        ts->ArgCount += 1;
                        ptr = Rest(ptr);
                    }

                    if (ptr != EmptyListObject)
                       RaiseExceptionC(R.Assertion, "apply", "apply: expected a proper list",
                               List(lst));

                    ts->AStack[ts->AStackPtr] = prc;
                    ts->AStackPtr += 1;
                    break;
                }

                case CaseLambdaOpcode:
                {
                    int cc = InstructionArg(obj);
                    int idx = 0;

                    while (cc > 0)
                    {
                        FAssert(VectorP(AsProcedure(ts->Proc)->Code));
                        FAssert(ts->IP + idx >= 0);
                        FAssert(ts->IP + idx < (int) VectorLength(AsProcedure(ts->Proc)->Code));

                        FObject prc = AsVector(AsProcedure(ts->Proc)->Code)->Vector[ts->IP + idx];

                        FAssert(ProcedureP(prc));

                        if (((AsProcedure(prc)->Reserved & PROCEDURE_FLAG_RESTARG)
                                && ts->ArgCount + 1 >= ProcedureArgCount(prc))
                                || ProcedureArgCount(prc) == ts->ArgCount)
                        {
                            ts->Proc = prc;
                            FAssert(VectorP(AsProcedure(ts->Proc)->Code));

                            ts->IP = 0;
                            ts->Frame = NoValueObject;

                            break;
                        }

                        idx += 1;
                        cc -= 1;
                    }

                    if (cc == 0)
                        RaiseExceptionC(R.Assertion, "case-lambda",
                                "case-lambda: no matching case", List(MakeFixnum(ts->ArgCount)));
                    break;
                }

                case CallWithCCOpcode:
                {
                    if (ts->ArgCount != 1)
                        RaiseExceptionC(R.Assertion, "call/cc", "call/cc: expected one argument",
                                EmptyListObject);

                    if (ProcedureP(ts->AStack[ts->AStackPtr - 1]) == 0
                            && PrimitiveP(ts->AStack[ts->AStackPtr - 1]) == 0)
                        RaiseExceptionC(R.Assertion, "call/cc", "call/cc: expected a procedure",
                                List(ts->AStack[ts->AStackPtr - 1]));

                    FObject v[7];
                    v[0] = MakeInstruction(CallContinuationOpcode, 0);
                    v[1] = MakeFixnum(ts->AStackPtr - 1);
                    v[2] = MakeVector(ts->AStackPtr - 1, ts->AStack, NoValueObject);
                    v[3] = MakeFixnum(ts->CStackPtr);
                    v[4] = MakeVector(ts->CStackPtr, ts->CStack - ts->CStackPtr + 1,
                            NoValueObject);
                    v[5] = MakeInstruction(ValuesOpcode, 0);
                    v[6] = MakeInstruction(ReturnOpcode, 0);

                    op = ts->AStack[ts->AStackPtr - 1];
                    ts->AStack[ts->AStackPtr - 1] = MakeProcedure(NoValueObject,
                            MakeVector(7, v, NoValueObject), 1,
                            PROCEDURE_FLAG_CONTINUATION | PROCEDURE_FLAG_RESTARG);

                    goto TailCall;
                }

                case CallContinuationOpcode:
                {
                    FObject args[MAXIMUM_CONTINUATION_ARGS];

                    if (ts->ArgCount > MAXIMUM_CONTINUATION_ARGS)
                        RaiseExceptionC(R.Restriction, "call/cc",
                                "call/cc: too many arguments to captured continuation",
                                List(MakeFixnum(ts->ArgCount)));

                    for (int adx = 0; adx < ts->ArgCount; adx++)
                        args[adx] = ts->AStack[ts->AStackPtr - adx - 1];

                    FAssert(ts->IP == 1);
                    FAssert(VectorLength(AsProcedure(ts->Proc)->Code) == 7);

                    FObject * v = AsVector(AsProcedure(ts->Proc)->Code)->Vector;

                    FAssert(FixnumP(v[1]));
                    ts->AStackPtr = AsFixnum(v[1]);

                    FAssert(VectorP(v[2]));
                    for (int adx = 0; adx < ts->AStackPtr; adx++)
                        ts->AStack[adx] = AsVector(v[2])->Vector[adx];

                    FAssert(FixnumP(v[3]));
                    ts->CStackPtr = AsFixnum(v[3]);

                    FAssert(VectorP(v[4]));
                    FObject * cs = ts->CStack - ts->CStackPtr + 1;
                    for (int cdx = 0; cdx < ts->CStackPtr; cdx++)
                        cs[cdx] = AsVector(v[4])->Vector[cdx];

                    ts->AStackPtr += ts->ArgCount;
                    for (int adx = 0; adx < ts->ArgCount; adx++)
                        ts->AStack[ts->AStackPtr - adx - 1] = args[adx];

                    ts->IP = 5;
                    break;
                }

                case ReturnFromOpcode:
                    FAssert(ts->AStackPtr == 1);
                    FAssert(ts->CStackPtr == 0);

                    return(ts->AStack[0]);

                case MarkContinuationOpcode:
                {
                    FAssert(ts->ArgCount == 3);
                    FAssert(ts->AStackPtr >= 3);

                    FObject thnk = ts->AStack[ts->AStackPtr - 1];
                    FObject val = ts->AStack[ts->AStackPtr - 2];
                    FObject key = ts->AStack[ts->AStackPtr - 3];
                    ts->AStackPtr -= 3;

                    int idx = ts->CStackPtr - 3; // WantValuesObject, Proc, IP

                    if (PairP(ts->IndexStack))
                    {
                        FAssert(PairP(First(ts->IndexStack)));
                        FAssert(FixnumP(First(First(ts->IndexStack))));

                        if (AsFixnum(First(First(ts->IndexStack))) == idx)
                        {
                            ts->MarkStack = MakePair(MarkListSet(First(ts->MarkStack), key, val),
                                    Rest(ts->MarkStack));

                            ts->ArgCount = 0;
                            op = thnk;
                            goto TailCall;
                        }
                    }

                    ts->MarkStack = MakePair(MarkListSet(EmptyListObject, key, val),
                            ts->MarkStack);
                    ts->IndexStack = MakePair(MakePair(MakeFixnum(ts->CStackPtr),
                            MakeFixnum(ts->AStackPtr)), ts->IndexStack);

                    ts->ArgCount = 0;
                    ts->AStack[ts->AStackPtr] = thnk;
                    ts->AStackPtr += 1;
                    break;
                }

                case PopMarkStackOpcode:
                    FAssert(PairP(ts->MarkStack));
                    FAssert(PairP(ts->IndexStack));

                    ts->MarkStack = Rest(ts->MarkStack);
                    ts->IndexStack = Rest(ts->IndexStack);
                    break;
                }
            }
        }
    }
    catch (FObject obj)
    {
        throw obj;
    }
}

#define ParameterP(obj) (ProcedureP(obj) && AsProcedure(obj)->Reserved & PROCEDURE_FLAG_PARAMETER)

Define("current-continuation-marks", CurrentContinuationMarksPrimitive)(int argc, FObject argv[])
{
    if (argc != 0)
        RaiseExceptionC(R.Assertion, "current-continuation-marks",
                "current-continuation-marks: expected zero arguments", EmptyListObject);

    return(GetThreadState()->MarkStack);
}

Define("get-parameter", GetParameterPrimitive)(int argc, FObject argv[])
{
    // (get-parameter <parameter> <initial-value>)

    if (argc != 2)
        RaiseExceptionC(R.Assertion, "get-parameter", "get-parameter: expected two arguments",
                EmptyListObject);

    if (ParameterP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "get-parameter", "get-parameter: expected a parameter",
                List(argv[0]));

    FThreadState * ts = GetThreadState();

    if (HashtableP(ts->Parameters))
    {
        FObject stk = EqHashtableRef(ts->Parameters, argv[0], EmptyListObject);
        if (stk == EmptyListObject)
            return(argv[1]);

        FAssert(PairP(stk));

        return(First(stk));
    }

    return(argv[1]);
}

Define("set-parameter!", SetParameterPrimitive)(int argc, FObject argv[])
{
    // (set-parameter! <parameter> <value>)

    if (argc != 2)
        RaiseExceptionC(R.Assertion, "set-parameter!", "set-parameter!: expected two arguments",
                EmptyListObject);

    if (ParameterP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "set-parameter!", "set-parameter!: expected a parameter",
                List(argv[0]));

    FThreadState * ts = GetThreadState();

    if (HashtableP(ts->Parameters) == 0)
        ts->Parameters = MakeEqHashtable(0);

    FObject stk = EqHashtableRef(ts->Parameters, argv[0], EmptyListObject);
    if (stk != EmptyListObject)
        stk = Rest(stk);
    EqHashtableSet(ts->Parameters, argv[0], MakePair(argv[1], stk));

    return(NoValueObject);
}

Define("push-parameter", PushParameterPrimitive)(int argc, FObject argv[])
{
    // (push-parameter <parameter> <value>)

    if (argc != 2)
        RaiseExceptionC(R.Assertion, "push-parameter", "push-parameter: expected two arguments",
                EmptyListObject);

    if (ParameterP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "push-parameter", "push-parameter: expected a parameter",
                List(argv[0]));

    FThreadState * ts = GetThreadState();

    if (HashtableP(ts->Parameters) == 0)
        ts->Parameters = MakeEqHashtable(0);

    EqHashtableSet(ts->Parameters, argv[0],
            MakePair(argv[1], EqHashtableRef(ts->Parameters, argv[0], EmptyListObject)));

    return(NoValueObject);
}

Define("pop-parameter", PopParameterPrimitive)(int argc, FObject argv[])
{
    // (pop-parameter <parameter>)

    if (argc != 1)
        RaiseExceptionC(R.Assertion, "pop-parameter", "pop-parameter: expected one argument",
                EmptyListObject);

    if (ParameterP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "pop-parameter", "pop-parameter: expected a parameter",
                List(argv[0]));

    FThreadState * ts = GetThreadState();

    FAssert(HashtableP(ts->Parameters));

    FObject stk = EqHashtableRef(ts->Parameters, argv[0], EmptyListObject);

    FAssert(PairP(stk));

    EqHashtableSet(ts->Parameters, argv[0], Rest(stk));

    return(NoValueObject);
}

Define("procedure->parameter", ProcedureToParameterPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "procedure->parameter",
                "procedure->parameter: expected one argument", EmptyListObject);

    if (ProcedureP(argv[0]) == 0)
         RaiseExceptionC(R.Assertion, "procedure->parameter",
                 "procedure->parameter: expected a procedure", List(argv[0]));

    AsProcedure(argv[0])->Reserved |= PROCEDURE_FLAG_PARAMETER;

    return(NoValueObject);
}

Define("parameter?", ParameterPPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "parameter?", "parameter?: expected one argument",
                EmptyListObject);

    return(ParameterP(argv[0]) ? TrueObject : FalseObject);
}

static void WalkVisit(FObject key, FObject val, FObject ht)
{
    if (ParameterP(key) && EqHashtableContainsP(ht, key) == 0)
        EqHashtableSet(ht, key, val);
}

FObject CurrentParameters()
{
    FObject ht = MakeEqHashtable(0);
    FThreadState * ts = GetThreadState();
    FObject ms = ts->MarkStack;

    while (PairP(ms))
    {
        MarkListWalkVisit(First(ms), WalkVisit, ht);
        ms = Rest(ms);
    }

    FAssert(ms == EmptyListObject);

    if (HashtableP(ts->Parameters))
        HashtableWalkVisit(ts->Parameters, WalkVisit, ht);

    return(ht);
}

static FPrimitive * Primitives[] =
{
    &CurrentContinuationMarksPrimitive,
    &GetParameterPrimitive,
    &SetParameterPrimitive,
    &PushParameterPrimitive,
    &PopParameterPrimitive,
    &ProcedureToParameterPrimitive,
    &ParameterPPrimitive
};

void SetupExecute()
{
    FObject v[7];

    R.WrongNumberOfArguments = MakeStringC("wrong number of arguments");
    R.NotCallable = MakeStringC("not callable");
    R.UnexpectedNumberOfValues = MakeStringC("unexpected number of values");
    R.UndefinedMessage = MakeStringC("variable is undefined");

    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);

    v[0] = MakeInstruction(ValuesOpcode, 0);
    v[1] = MakeInstruction(ReturnOpcode, 0);
    LibraryExport(R.BedrockLibrary,
            EnvironmentSetC(R.Bedrock, "values", MakeProcedure(StringCToSymbol("values"),
            MakeVector(2, v, NoValueObject), 1, PROCEDURE_FLAG_RESTARG)));

    v[0] = MakeInstruction(ApplyOpcode, 0);
    v[1] = MakeInstruction(TailCallOpcode, 0);
    LibraryExport(R.BedrockLibrary,
            EnvironmentSetC(R.Bedrock, "apply", MakeProcedure(StringCToSymbol("apply"),
            MakeVector(2, v, NoValueObject), 2, PROCEDURE_FLAG_RESTARG)));

    v[0] = MakeInstruction(CallWithCCOpcode, 0);
    LibraryExport(R.BedrockLibrary,
            EnvironmentSetC(R.Bedrock, "call/cc", MakeProcedure(StringCToSymbol("call/cc"),
            MakeVector(1, v, NoValueObject), 1, 0)));

    v[0] = MakeInstruction(SetArgCountOpcode, 0);
    v[1] = MakeInstruction(CallOpcode, 0);
    v[2] = MakeInstruction(ReturnFromOpcode, 0);
    R.ExecuteThunk = MakeProcedure(StringCToSymbol("execute-thunk"),
            MakeVector(3, v, NoValueObject), 1, 0);

    v[0] = MakeInstruction(CheckCountOpcode, 3);
    v[1] = MakeInstruction(MarkContinuationOpcode, 0);
    v[2] = MakeInstruction(PushWantValuesOpcode, 0);
    v[3] = MakeInstruction(CallOpcode, 0);
    v[4] = MakeInstruction(PopCStackOpcode, 1);
    v[5] = MakeInstruction(PopMarkStackOpcode, 0);
    v[6] = MakeInstruction(ReturnOpcode, 0);
    LibraryExport(R.BedrockLibrary,
            EnvironmentSetC(R.Bedrock, "mark-continuation",
            MakeProcedure(StringCToSymbol("mark-continuation"), MakeVector(7, v, NoValueObject),
            3, 0)));
}
