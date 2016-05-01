/*

Foment

*/

#ifdef FOMENT_WINDOWS
#include <windows.h>
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
#include <pthread.h>
#include <stdlib.h>
#endif // FOMENT_UNIX

#include <stdio.h>
#include "foment.hpp"

#ifndef FOMENT_BSD
#include <malloc.h>
#endif // FOMENT_BSD

#include "execute.hpp"
#include "syncthrd.hpp"

// ---- Procedure ----

FObject MakeProcedure(FObject nam, FObject fn, FObject ln, FObject cv, int_t ac, uint_t fl)
{
    FProcedure * p = (FProcedure *) MakeObject(ProcedureTag, sizeof(FProcedure), 4,
            "%make-procedure");
    p->Name = SyntaxToDatum(nam);
    p->Filename = fn;
    p->LineNumber = ln;
    p->Code = cv;
    p->ArgCount = ac;
    p->Flags = fl;

    return(p);
}

// ---- Dynamic ----

typedef struct
{
    FRecord Record;
    FObject Who;
    FObject CStackPtr;
    FObject AStackPtr;
    FObject Marks;
} FDynamic;

#define AsDynamic(obj) ((FDynamic *) (obj))
#define DynamicP(obj) RecordP(obj, R.DynamicRecordType)

static const char * DynamicFieldsC[] = {"who", "cstack-ptr", "astack-ptr", "marks"};

static FObject MakeDynamic(FObject who, FObject cdx, FObject adx, FObject ml)
{
    FAssert(sizeof(FDynamic) == sizeof(DynamicFieldsC) + sizeof(FRecord));
    FAssert(FixnumP(cdx));
    FAssert(FixnumP(adx));

    FDynamic * dyn = (FDynamic *) MakeRecord(R.DynamicRecordType);
    dyn->Who = who;
    dyn->CStackPtr = cdx;
    dyn->AStackPtr = adx;
    dyn->Marks = ml;

    return(dyn);
}

static FObject MakeDynamic(FObject dyn, FObject ml)
{
    FAssert(DynamicP(dyn));

    return(MakeDynamic(AsDynamic(dyn)->Who, AsDynamic(dyn)->CStackPtr, AsDynamic(dyn)->AStackPtr,
            ml));
}

// ---- Continuation ----

typedef struct
{
    FRecord Record;
    FObject CStackPtr;
    FObject CStack;
    FObject AStackPtr;
    FObject AStack;
} FContinuation;

#define AsContinuation(obj) ((FContinuation *) (obj))
#define ContinuationP(obj) RecordP(obj, R.ContinuationRecordType)

static const char * ContinuationFieldsC[] = {"cstack-ptr", "cstack", "astack-ptr", "astack"};

static FObject MakeContinuation(FObject cdx, FObject cv, FObject adx, FObject av)
{
    FAssert(sizeof(FContinuation) == sizeof(ContinuationFieldsC) + sizeof(FRecord));
    FAssert(FixnumP(cdx));
    FAssert(VectorP(cv));
    FAssert(FixnumP(adx));
    FAssert(VectorP(av));

    FContinuation * cont = (FContinuation *) MakeRecord(R.ContinuationRecordType);
    cont->CStackPtr = cdx;
    cont->CStack = cv;
    cont->AStackPtr = adx;
    cont->AStack = av;
    return(cont);
}

// ---- Instruction ----

static const char * Opcodes[] =
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
    "make-box",
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
    "capture-continuation",
    "call-continuation",
    "abort",
    "return-from",
    "mark-continuation",
    "pop-mark-stack",
};

void WriteInstruction(FObject port, FObject obj, int_t df)
{
    FAssert(InstructionP(obj));

    FCh s[16];
    int_t sl = FixnumAsString(InstructionArg(obj), s, 10);

    WriteStringC(port, "#<");

    if (InstructionOpcode(obj) < 0 || InstructionOpcode(obj) >= sizeof(Opcodes) / sizeof(char *))
        WriteStringC(port, "unknown");
    else
        WriteStringC(port, Opcodes[InstructionOpcode(obj)]);

    WriteStringC(port, ": ");
    WriteString(port, s, sl);
    WriteCh(port, '>');
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

static FObject FindMark(FObject key, FObject dflt)
{
    FThreadState * ts = GetThreadState();
    FObject ds = ts->DynamicStack;

    while (PairP(ds))
    {
        FAssert(DynamicP(First(ds)));

        FObject ret = Assq(key, AsDynamic(First(ds))->Marks);
        if (ret != FalseObject)
        {
            FAssert(PairP(ret));
            FAssert(EqP(First(ret), key));

            return(Rest(ret));
        }

        ds = Rest(ds);
    }

    FAssert(ds == EmptyListObject);

    return(dflt);
}

static int_t PrepareHandler(FThreadState * ts, FObject hdlr, FObject key, FObject obj)
{
    if (ProcedureP(hdlr))
    {
        FObject lst = FindMark(key, EmptyListObject);
        if (PairP(lst))
        {
            ts->ArgCount = 2;

            ts->AStack[ts->AStackPtr] = obj;
            ts->AStackPtr += 1;
            ts->AStack[ts->AStackPtr] = lst;
            ts->AStackPtr += 1;

            ts->Proc = hdlr;
            ts->IP = 0;
            ts->Frame = NoValueObject;

            return(1);
        }
        else
        {
            FAssert(lst == EmptyListObject);
        }
    }

    return(0);
}

static FObject Execute(FThreadState * ts)
{
    FObject op;

    for (;;)
    {
        FAssert(ts->DontWait == 0);

        if (ts->NotifyFlag)
        {
            ts->NotifyFlag = 0;

            PrepareHandler(ts, R.NotifyHandler, R.NotifyHandlerSymbol, ts->NotifyObject);
        }

        CheckForGC();

        FAssert(VectorP(AsProcedure(ts->Proc)->Code));
        FAssert(ts->IP >= 0);
        FAssert(ts->IP < (int_t) VectorLength(AsProcedure(ts->Proc)->Code));

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

                    int_t ac = ts->ArgCount;
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
                int_t ac = InstructionArg(obj);
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
                FAssert((uint_t) InstructionArg(obj) < VectorLength(ts->Frame));

                ts->AStack[ts->AStackPtr] = AsVector(ts->Frame)->Vector[InstructionArg(obj)];
                ts->AStackPtr += 1;
                break;

            case SetFrameOpcode:
                FAssert(VectorP(ts->Frame));
                FAssert((uint_t) InstructionArg(obj) < VectorLength(ts->Frame));
                FAssert(ts->AStackPtr > 0);

                ts->AStackPtr -= 1;
//                AsVector(ts->Frame)->Vector[InstructionArg(obj)] = ts->AStack[ts->AStackPtr];
                ModifyVector(ts->Frame, InstructionArg(obj), ts->AStack[ts->AStackPtr]);

                break;

            case GetVectorOpcode:
                FAssert(ts->AStackPtr > 0);
                FAssert(VectorP(ts->AStack[ts->AStackPtr - 1]));
                FAssert((uint_t) InstructionArg(obj)
                        < VectorLength(ts->AStack[ts->AStackPtr - 1]));

                ts->AStack[ts->AStackPtr - 1] = AsVector(ts->AStack[ts->AStackPtr - 1])->Vector[
                        InstructionArg(obj)];
                break;

            case SetVectorOpcode:
                FAssert(ts->AStackPtr > 1);
                FAssert(VectorP(ts->AStack[ts->AStackPtr - 1]));
                FAssert((uint_t) InstructionArg(obj)
                        < VectorLength(ts->AStack[ts->AStackPtr - 1]));

//                AsVector(ts->AStack[ts->AStackPtr - 1])->Vector[InstructionArg(obj)] =
//                        ts->AStack[ts->AStackPtr - 2];
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

//                    AsGlobal(ts->AStack[ts->AStackPtr - 1])->Box = MakeBox(
//                            ts->AStack[ts->AStackPtr - 2]);
                    Modify(FGlobal, ts->AStack[ts->AStackPtr - 1], Box,
                            MakeBox(ts->AStack[ts->AStackPtr - 2]));
//                    AsGlobal(ts->AStack[ts->AStackPtr - 1])->State = GlobalDefined;
                    Modify(FGlobal, ts->AStack[ts->AStackPtr - 1], State, GlobalDefined);
                }

                SetBox(AsGlobal(ts->AStack[ts->AStackPtr - 1])->Box,
                        ts->AStack[ts->AStackPtr - 2]);
                ts->AStackPtr -= 2;
                break;

            case MakeBoxOpcode:
                FAssert(ts->AStackPtr > 0);
                ts->AStack[ts->AStackPtr - 1] = MakeBox(ts->AStack[ts->AStackPtr - 1]);
                break;

            case GetBoxOpcode:
                FAssert(ts->AStackPtr > 0);
                FAssert(BoxP(ts->AStack[ts->AStackPtr - 1]));

                ts->AStack[ts->AStackPtr - 1] = Unbox(ts->AStack[ts->AStackPtr - 1]);
                break;

            case SetBoxOpcode:
                FAssert(ts->AStackPtr > 1);
                FAssert(BoxP(ts->AStack[ts->AStackPtr - 1]));

                SetBox(ts->AStack[ts->AStackPtr - 1], ts->AStack[ts->AStackPtr - 2]);
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
                    if (ts->AStackPtr + 128 > ts->Stack.BottomUsed / sizeof(FObject)
                        || ts->CStackPtr + 128 > ts->Stack.TopUsed / sizeof(FObject))
                    {
                        if (GrowMemRegionUp(&ts->Stack,
                                (ts->AStackPtr + 128) * sizeof(FObject)) == 0)
                            RaiseExceptionC(R.Assertion, "foment", "out of memory",
                                    EmptyListObject);
                        if (GrowMemRegionDown(&ts->Stack,
                                (ts->CStackPtr + 128) * sizeof(FObject)) == 0)
                            RaiseExceptionC(R.Assertion, "foment", "out of memory",
                                    EmptyListObject);
                    }

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
                FObject proc = MakeProcedure(NoValueObject, NoValueObject, NoValueObject,
                        MakeVector(3, v, NoValueObject), 0, PROCEDURE_FLAG_CLOSURE);
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

                int_t vc;

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
                                   "caller not expecting multiple values",
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
                   RaiseExceptionC(R.Assertion, "apply", "expected at least two arguments",
                           EmptyListObject);

                FObject prc = ts->AStack[ts->AStackPtr - ts->ArgCount];
                FObject lst = ts->AStack[ts->AStackPtr - 1];

                int_t adx = ts->ArgCount;
                while (adx > 2)
                {
                    ts->AStack[ts->AStackPtr - adx] = ts->AStack[ts->AStackPtr - adx + 1];
                    adx -= 1;
                }

                ts->ArgCount -= 2;
                ts->AStackPtr -= 2;

                int_t ll = ListLength("apply", lst);
                if (ts->CStackPtr + 128 > ts->Stack.TopUsed / sizeof(FObject)
                        || ts->AStackPtr + ll + 128 > ts->Stack.BottomUsed / sizeof(FObject))
                {
                    if (GrowMemRegionUp(&ts->Stack,
                            (ts->AStackPtr + ll + 128) * sizeof(FObject)) == 0)
                        RaiseExceptionC(R.Assertion, "foment", "out of memory", EmptyListObject);
                    if (GrowMemRegionDown(&ts->Stack,
                            (ts->CStackPtr + 128) * sizeof(FObject)) == 0)
                        RaiseExceptionC(R.Assertion, "foment", "out of memory", EmptyListObject);
                }

                FObject ptr = lst;
                while (PairP(ptr))
                {
                    ts->AStack[ts->AStackPtr] = First(ptr);
                    ts->AStackPtr += 1;
                    ts->ArgCount += 1;
                    ptr = Rest(ptr);

                    FAssert(ts->AStackPtr <= ts->Stack.BottomUsed / sizeof(FObject));
                }

                if (ptr != EmptyListObject)
                   RaiseExceptionC(R.Assertion, "apply", "expected a proper list", List(lst));

                ts->AStack[ts->AStackPtr] = prc;
                ts->AStackPtr += 1;
                break;
            }

            case CaseLambdaOpcode:
            {
                int_t cc = InstructionArg(obj);
                int_t idx = 0;

                while (cc > 0)
                {
                    FAssert(VectorP(AsProcedure(ts->Proc)->Code));
                    FAssert(ts->IP + idx >= 0);
                    FAssert(ts->IP + idx < (int_t) VectorLength(AsProcedure(ts->Proc)->Code));

                    FObject prc = AsVector(AsProcedure(ts->Proc)->Code)->Vector[ts->IP + idx];

                    FAssert(ProcedureP(prc));

                    if (((AsProcedure(prc)->Flags & PROCEDURE_FLAG_RESTARG)
                            && ts->ArgCount + 1 >= AsProcedure(prc)->ArgCount)
                            || AsProcedure(prc)->ArgCount == ts->ArgCount)
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
                    RaiseExceptionC(R.Assertion, "case-lambda", "no matching case",
                            List(MakeFixnum(ts->ArgCount)));
                break;
            }

            case CaptureContinuationOpcode:
            {
                FAssert(ts->AStackPtr > 0);
                FMustBe(ts->ArgCount == 1);

                op = ts->AStack[ts->AStackPtr - 1];

                FMustBe(ProcedureP(op));

                ts->AStack[ts->AStackPtr - 1] = MakeContinuation(MakeFixnum(ts->CStackPtr),
                        MakeVector(ts->CStackPtr, ts->CStack - ts->CStackPtr + 1,
                        NoValueObject), MakeFixnum(ts->AStackPtr - 1),
                        MakeVector(ts->AStackPtr - 1, ts->AStack, NoValueObject));

                goto TailCall;
            }

            case CallContinuationOpcode:
            {
                FAssert(ts->AStackPtr > 1);
                FMustBe(ts->ArgCount == 2);

                FObject cont = ts->AStack[ts->AStackPtr - 2];
                op = ts->AStack[ts->AStackPtr - 1];

                FMustBe(ContinuationP(cont));
                FMustBe(ProcedureP(op) || PrimitiveP(op));

                FAssert(FixnumP(AsContinuation(cont)->AStackPtr));
                ts->AStackPtr = AsFixnum(AsContinuation(cont)->AStackPtr);

                FAssert(VectorP(AsContinuation(cont)->AStack));
                for (int_t adx = 0; adx < ts->AStackPtr; adx++)
                    ts->AStack[adx] = AsVector(AsContinuation(cont)->AStack)->Vector[adx];

                FAssert(FixnumP(AsContinuation(cont)->CStackPtr));
                ts->CStackPtr = AsFixnum(AsContinuation(cont)->CStackPtr);

                FAssert(VectorP(AsContinuation(cont)->CStack));
                FObject * cs = ts->CStack - ts->CStackPtr + 1;
                for (int_t cdx = 0; cdx < ts->CStackPtr; cdx++)
                    cs[cdx] = AsVector(AsContinuation(cont)->CStack)->Vector[cdx];
                ts->ArgCount = 0;
                goto TailCall;
            }

            case AbortOpcode:
            {
                FMustBe(ts->ArgCount == 2);

                ts->AStackPtr -= 1;
                FObject thnk = ts->AStack[ts->AStackPtr];
                ts->AStackPtr -= 1;
                FObject dyn = ts->AStack[ts->AStackPtr];

                FMustBe(ProcedureP(thnk));
                FMustBe(DynamicP(dyn));

                FAssert(FixnumP(AsDynamic(dyn)->CStackPtr));
                FAssert(FixnumP(AsDynamic(dyn)->AStackPtr));

                ts->CStackPtr = AsFixnum(AsDynamic(dyn)->CStackPtr);
                ts->AStackPtr = AsFixnum(AsDynamic(dyn)->AStackPtr);
                ts->ArgCount = 0;
                ts->Proc = thnk;
                ts->IP = 0;
                ts->Frame = NoValueObject;
                break;
            }

            case ReturnFromOpcode:
                FAssert(ts->AStackPtr == 1);
                FAssert(ts->CStackPtr == 0);

                return(ts->AStack[0]);

            case MarkContinuationOpcode:
            {
                FAssert(ts->ArgCount == 4);
                FAssert(ts->AStackPtr >= 4);

                FObject thnk = ts->AStack[ts->AStackPtr - 1];
                FObject val = ts->AStack[ts->AStackPtr - 2];
                FObject key = ts->AStack[ts->AStackPtr - 3];
                FObject who = ts->AStack[ts->AStackPtr - 4];
                ts->AStackPtr -= 4;

                int_t idx = ts->CStackPtr - 3; // WantValuesObject, Proc, IP

                if (PairP(ts->DynamicStack))
                {
                    FAssert(DynamicP(First(ts->DynamicStack)));

                    FObject dyn = First(ts->DynamicStack);

                    FAssert(FixnumP(AsDynamic(dyn)->CStackPtr));

                    if (AsDynamic(dyn)->Who == who && AsFixnum(AsDynamic(dyn)->CStackPtr) == idx)
                    {
                        ts->DynamicStack = MakePair(MakeDynamic(dyn,
                                MarkListSet(AsDynamic(dyn)->Marks, key, val)),
                                Rest(ts->DynamicStack));

                        ts->ArgCount = 0;
                        op = thnk;
                        goto TailCall;
                    }
                }

                ts->DynamicStack = MakePair(MakeDynamic(who, MakeFixnum(ts->CStackPtr),
                        MakeFixnum(ts->AStackPtr), MarkListSet(EmptyListObject, key, val)),
                        ts->DynamicStack);

                ts->ArgCount = 0;
                ts->AStack[ts->AStackPtr] = thnk;
                ts->AStackPtr += 1;
                break;
            }

            case PopDynamicStackOpcode:
                FAssert(PairP(ts->DynamicStack));

                ts->DynamicStack = Rest(ts->DynamicStack);
                break;

            default:
                FAssert(0);
                break;
            }
        }
    }
}

FObject ExecuteThunk(FObject op)
{
    FThreadState * ts = GetThreadState();

    ts->AStackPtr = 1;
    ts->AStack[0] = op;
    ts->ArgCount = 1;
    ts->CStackPtr = 0;
    ts->Proc = R.ExecuteThunk;
    FAssert(ProcedureP(ts->Proc));
    FAssert(VectorP(AsProcedure(ts->Proc)->Code));

    ts->IP = 0;
    ts->Frame = NoValueObject;
    ts->DynamicStack = EmptyListObject;

    for (;;)
    {
        try
        {
            return(Execute(ts));
        }
        catch (FObject obj)
        {
            if (PrepareHandler(ts, R.RaiseHandler, R.ExceptionHandlerSymbol, obj) == 0)
                throw obj;
        }
        catch (FNotifyThrow nt)
        {
            ((FNotifyThrow) nt);

            ts->NotifyFlag = 0;

            if (PrepareHandler(ts, R.NotifyHandler, R.NotifyHandlerSymbol, ts->NotifyObject) == 0)
                ThreadExit(ts->NotifyObject);
        }
    }
}

Define("procedure?", ProcedurePPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("procedure?", argc);

    return((ProcedureP(argv[0]) || PrimitiveP(argv[0])) ? TrueObject : FalseObject);
}

Define("%map-car", MapCarPrimitive)(int_t argc, FObject argv[])
{
    // (%map-car lists)

    FMustBe(argc == 1);

    FObject ret = EmptyListObject;
    FObject lst = argv[0];

    while (PairP(lst))
    {
        if (PairP(First(lst)) == 0)
            return(EmptyListObject);

        ret = MakePair(First(First(lst)), ret);
        lst = Rest(lst);
    }

    return(ReverseListModify(ret));
}

Define("%map-cdr", MapCdrPrimitive)(int_t argc, FObject argv[])
{
    // (%map-cdr lists)

    FMustBe(argc == 1);

    FObject ret = EmptyListObject;
    FObject lst = argv[0];

    while (PairP(lst))
    {
        if (PairP(First(lst)) == 0)
            return(EmptyListObject);

        ret = MakePair(Rest(First(lst)), ret);
        lst = Rest(lst);
    }

    return(ReverseListModify(ret));
}

Define("%map-strings", MapStringsPrimitive)(int_t argc, FObject argv[])
{
    // (%map-strings index strings)

    FMustBe(argc == 2);
    FMustBe(FixnumP(argv[0]));
    FMustBe(AsFixnum(argv[0]) >= 0);

    int_t idx = AsFixnum(argv[0]);
    FObject ret = EmptyListObject;
    FObject lst = argv[1];

    while (PairP(lst))
    {
        StringArgCheck("string-map", First(lst));

        if (idx == (int_t) StringLength(First(lst)))
            return(EmptyListObject);

        ret = MakePair(MakeCharacter(AsString(First(lst))->String[idx]), ret);
        lst = Rest(lst);
    }

    return(ReverseListModify(ret));
}

Define("%map-vectors", MapVectorsPrimitive)(int_t argc, FObject argv[])
{
    // (%map-vectors index vectors)

    FMustBe(argc == 2);
    FMustBe(FixnumP(argv[0]));
    FMustBe(AsFixnum(argv[0]) >= 0);

    int_t idx = AsFixnum(argv[0]);
    FObject ret = EmptyListObject;
    FObject lst = argv[1];

    while (PairP(lst))
    {
        VectorArgCheck("vector-map", First(lst));

        if (idx == (int_t) VectorLength(First(lst)))
            return(EmptyListObject);

        ret = MakePair(AsVector(First(lst))->Vector[idx], ret);
        lst = Rest(lst);
    }

    return(ReverseListModify(ret));
}

Define("%execute-thunk", ExecuteThunkPrimitive)(int_t argc, FObject argv[])
{
    // (%execute-thunk <proc>)

    FMustBe(argc == 1);
    FMustBe(ProcedureP(argv[0]));

    R.ExecuteThunk = argv[0];
    return(NoValueObject);
}

Define("%set-raise-handler!", SetRaiseHandlerPrimitive)(int_t argc, FObject argv[])
{
    // (%set-raise-handler! <proc>)

    FMustBe(argc == 1);
    FMustBe(ProcedureP(argv[0]));

    R.RaiseHandler = argv[0];
    return(NoValueObject);
}

Define("%set-notify-handler!", SetNotifyHandlerPrimitive)(int_t argc, FObject argv[])
{
    // (%set-notify-handler! <proc>)

    FMustBe(argc == 1);
    FMustBe(ProcedureP(argv[0]));

    R.NotifyHandler = argv[0];
    return(NoValueObject);
}

Define("%interactive-thunk", InteractiveThunkPrimitive)(int_t argc, FObject argv[])
{
    // (%interactive-thunk <proc>)

    FMustBe(argc == 1);
    FMustBe(ProcedureP(argv[0]));

    R.InteractiveThunk = argv[0];
    return(NoValueObject);
}

Define("%bytes-allocated", BytesAllocatedPrimitive)(int_t argc, FObject argv[])
{
    // (%bytes-allocated)

    FMustBe(argc == 0);

    uint_t ba = BytesAllocated;
    BytesAllocated = 0;

    return(MakeFixnum(ba));
}

#define ParameterP(obj) (ProcedureP(obj) && (AsProcedure(obj)->Flags & PROCEDURE_FLAG_PARAMETER))

Define("%dynamic-stack", DynamicStackPrimitive)(int_t argc, FObject argv[])
{
    // (%dynamic-stack)
    // (%dynamic-stack <stack>)

    FMustBe(argc == 0 || argc == 1);

    FThreadState * ts = GetThreadState();
    FObject ds = ts->DynamicStack;

    if (argc == 1)
    {
        FMustBe(argv[0] == EmptyListObject || PairP(argv[0]));

        ts->DynamicStack = argv[0];
    }

    return(ds);
}

Define("%dynamic-marks", DynamicMarksPrimitive)(int_t argc, FObject argv[])
{
    // (%dynamic-marks <dynamic>)

    FMustBe(argc == 1);
    FMustBe(DynamicP(argv[0]));

    return(AsDynamic(argv[0])->Marks);
}

Define("%parameters", ParametersPrimitive)(int_t argc, FObject argv[])
{
    // (%parameters)

    FMustBe(argc == 0);

    return(GetThreadState()->Parameters);
}

Define("%procedure->parameter", ProcedureToParameterPrimitive)(int_t argc, FObject argv[])
{
    // (%procedure->parameter <proc>)

    FMustBe(argc == 1);
    FMustBe(ProcedureP(argv[0]));

    AsProcedure(argv[0])->Flags |= PROCEDURE_FLAG_PARAMETER;

    return(NoValueObject);
}

Define("%parameter?", ParameterPPrimitive)(int_t argc, FObject argv[])
{
    // (%parameter? <obj>)

    FMustBe(argc == 1);

    return(ParameterP(argv[0]) ? TrueObject : FalseObject);
}

Define("%index-parameter", IndexParameterPrimitive)(int_t argc, FObject argv[])
{
    // (%index-parameter <index>)
    // (%index-parameter <index> <value>)

    FMustBe(argc >= 1 && argc <= 2);
    FMustBe(FixnumP(argv[0]) && AsFixnum(argv[0]) >= 0 && AsFixnum(argv[0]) < INDEX_PARAMETERS);

    if (argc == 1)
    {
        FAssert(PairP(GetThreadState()->IndexParameters[AsFixnum(argv[0])]));

        return(GetThreadState()->IndexParameters[AsFixnum(argv[0])]);
    }

    FAssert(PairP(argv[1]));

    GetThreadState()->IndexParameters[AsFixnum(argv[0])] = argv[1];
    return(NoValueObject);
}

Define("%find-mark", FindMarkPrimitive)(int_t argc, FObject argv[])
{
    // (%find-mark <key> <default>)

    FMustBe(argc == 2);

    return(FindMark(argv[0], argv[1]));
}

static void Visit(FObject key, FObject val, FObject hm)
{
    FAssert(ParameterP(key));

    EqHashMapSet(hm, key, PairP(val) ? MakePair(First(val), EmptyListObject) : EmptyListObject);
}

FObject CurrentParameters()
{
    FObject hm = MakeEqHashMap();
    FThreadState * ts = GetThreadState();

    if (HashMapP(ts->Parameters))
        EqHashMapVisit(ts->Parameters, Visit, hm);

    return(hm);
}

static FPrimitive * Primitives[] =
{
    &ProcedurePPrimitive,
    &MapCarPrimitive,
    &MapCdrPrimitive,
    &MapStringsPrimitive,
    &MapVectorsPrimitive,
    &ExecuteThunkPrimitive,
    &SetRaiseHandlerPrimitive,
    &SetNotifyHandlerPrimitive,
    &InteractiveThunkPrimitive,
    &BytesAllocatedPrimitive,
    &DynamicStackPrimitive,
    &DynamicMarksPrimitive,
    &ParametersPrimitive,
    &ProcedureToParameterPrimitive,
    &ParameterPPrimitive,
    &IndexParameterPrimitive,
    &FindMarkPrimitive
};

void SetupExecute()
{
    FObject v[7];

    R.WrongNumberOfArguments = MakeStringC("wrong number of arguments");
    R.NotCallable = MakeStringC("not callable");
    R.UnexpectedNumberOfValues = MakeStringC("unexpected number of values");
    R.UndefinedMessage = MakeStringC("variable is undefined");

    R.ExceptionHandlerSymbol = StringCToSymbol("exception-handler");
    R.NotifyHandlerSymbol = StringCToSymbol("notify-handler");
    R.SigIntSymbol = StringCToSymbol("sigint");

    R.DynamicRecordType = MakeRecordTypeC("dynamic", sizeof(DynamicFieldsC) / sizeof(char *),
            DynamicFieldsC);
    R.ContinuationRecordType = MakeRecordTypeC("continuation",
            sizeof(ContinuationFieldsC) / sizeof(char *), ContinuationFieldsC);

    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);

    v[0] = MakeInstruction(ValuesOpcode, 0);
    v[1] = MakeInstruction(ReturnOpcode, 0);
    LibraryExport(R.BedrockLibrary,
            EnvironmentSetC(R.Bedrock, "values", MakeProcedure(StringCToSymbol("values"),
            MakeStringC(__FILE__), MakeFixnum(__LINE__),
            MakeVector(2, v, NoValueObject), 1, PROCEDURE_FLAG_RESTARG)));

    v[0] = MakeInstruction(ApplyOpcode, 0);
    v[1] = MakeInstruction(TailCallOpcode, 0);
    LibraryExport(R.BedrockLibrary,
            EnvironmentSetC(R.Bedrock, "apply", MakeProcedure(StringCToSymbol("apply"),
            MakeStringC(__FILE__), MakeFixnum(__LINE__),
            MakeVector(2, v, NoValueObject), 2, PROCEDURE_FLAG_RESTARG)));

    v[0] = MakeInstruction(SetArgCountOpcode, 0);
    v[1] = MakeInstruction(CallOpcode, 0);
    v[2] = MakeInstruction(ReturnFromOpcode, 0);
    R.ExecuteThunk = MakeProcedure(NoValueObject, MakeStringC(__FILE__), MakeFixnum(__LINE__),
            MakeVector(3, v, NoValueObject), 1, 0);

    // (%return <value>)

    v[0] = MakeInstruction(ReturnFromOpcode, 0);
    LibraryExport(R.BedrockLibrary,
            EnvironmentSetC(R.Bedrock, "%return",
            MakeProcedure(StringCToSymbol("%return-from"), MakeStringC(__FILE__),
            MakeFixnum(__LINE__), MakeVector(1, v, NoValueObject), 1, 0)));

    // (%capture-continuation <proc>)

    v[0] = MakeInstruction(CaptureContinuationOpcode, 0);
    LibraryExport(R.BedrockLibrary, EnvironmentSetC(R.Bedrock, "%capture-continuation",
            MakeProcedure(StringCToSymbol("%capture-continuation"),
            MakeStringC(__FILE__), MakeFixnum(__LINE__), MakeVector(1, v, NoValueObject), 1, 0)));

    // (%call-continuation <cont> <thunk>)

    v[0] = MakeInstruction(CallContinuationOpcode, 0);
    LibraryExport(R.BedrockLibrary, EnvironmentSetC(R.Bedrock, "%call-continuation",
            MakeProcedure(StringCToSymbol("%call-continuation"),
            MakeStringC(__FILE__), MakeFixnum(__LINE__), MakeVector(1, v, NoValueObject), 2, 0)));

    // (%mark-continuation <who> <key> <value> <thunk>)

    v[0] = MakeInstruction(CheckCountOpcode, 4);
    v[1] = MakeInstruction(MarkContinuationOpcode, 0);
    v[2] = MakeInstruction(PushWantValuesOpcode, 0);
    v[3] = MakeInstruction(CallOpcode, 0);
    v[4] = MakeInstruction(PopCStackOpcode, 1);
    v[5] = MakeInstruction(PopDynamicStackOpcode, 0);
    v[6] = MakeInstruction(ReturnOpcode, 0);
    LibraryExport(R.BedrockLibrary,
            EnvironmentSetC(R.Bedrock, "%mark-continuation",
            MakeProcedure(StringCToSymbol("%mark-continuation"), MakeStringC(__FILE__),
            MakeFixnum(__LINE__), MakeVector(7, v, NoValueObject), 4, 0)));

    // (%abort-dynamic <dynamic> <thunk>)

    v[0] = MakeInstruction(AbortOpcode, 0);
    LibraryExport(R.BedrockLibrary,
            EnvironmentSetC(R.Bedrock, "%abort-dynamic",
            MakeProcedure(StringCToSymbol("%abort-dynamic"), MakeStringC(__FILE__),
            MakeFixnum(__LINE__), MakeVector(1, v, NoValueObject), 2, 0)));
}
