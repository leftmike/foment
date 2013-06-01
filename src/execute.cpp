/*

Foment

*/

#include <stdio.h>
#include "foment.hpp"
#include "execute.hpp"

FObject WrongNumberOfArguments;
FObject NotCallable;
FObject UnexpectedNumberOfValues;
FObject UndefinedMessage;

static FObject DynamicEnvironment;

// ---- Procedure ----

FObject MakeProcedure(FObject nam, FObject cv, int ac, FObject ra)
{
    FProcedure * p = (FProcedure *) MakeObject(ProcedureTag, sizeof(FProcedure));
    p->Name = SyntaxToDatum(nam);
    p->Code = cv;
    p->RestArg = ra;
    p->ArgCount = ac;
    p->Flags = 0;

    return(AsObject(p));
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
    "tail-call",
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

static int AStackSize = 1024 * 4;
static FObject AStack[1024 * 4];
static int AStackPtr;

static int CStackSize = 1024 * 16;
static FObject CStack[1024 * 16];
static int CStackPtr;

FObject Execute(FObject op, int argc, FObject argv[])
{
    AStackPtr = 0;

    int adx;
    for (adx = 0; adx < argc; adx++)
    {
        AStack[AStackPtr] = argv[adx];
        AStackPtr += 1;
    }

    CStackPtr = 0;
    FObject Proc = op;
    FAssert(ProcedureP(Proc));
    FAssert(VectorP(AsProcedure(Proc)->Code));

    FObject * cv = AsVector(AsProcedure(Proc)->Code)->Vector;
    int IP = 0;
    FObject Frame = NoValueObject;
    int ArgCount = argc;

    for (;;)
    {
        FAssert(VectorP(AsProcedure(Proc)->Code));
        FAssert(AsVector(AsProcedure(Proc)->Code)->Vector == cv);
        FAssert(IP >= 0);
        FAssert(IP < AsVector(AsProcedure(Proc)->Code)->Length);

        FObject obj = cv[IP];
        IP += 1;

        if (InstructionP(obj) == 0)
        {
            AStack[AStackPtr] = obj;
            AStackPtr += 1;
//Write(StandardOutput, obj, 0);
//printf("\n");
        }
        else
        {
//printf("%s.%d %d %d\n", Opcodes[InstructionOpcode(obj)], InstructionArg(obj), CStackPtr, AStackPtr);
            switch (InstructionOpcode(obj))
            {
            case CheckCountOpcode:
                if (ArgCount != InstructionArg(obj))
                    RaiseException(Assertion, AsProcedure(Proc)->Name, WrongNumberOfArguments,
                            EmptyListObject);
                break;

            case RestArgOpcode:
                FAssert(InstructionArg(obj) >= 0);

                if (ArgCount < InstructionArg(obj))
                    RaiseException(Assertion, AsProcedure(Proc)->Name, WrongNumberOfArguments,
                            EmptyListObject);
                else if (ArgCount == InstructionArg(obj))
                {
                    AStack[AStackPtr] = EmptyListObject;
                    AStackPtr += 1;
                }
                else
                {
                    FObject lst = EmptyListObject;

                    int ac = ArgCount;
                    while (ac > InstructionArg(obj))
                    {
                        AStackPtr -= 1;
                        lst = MakePair(AStack[AStackPtr], lst);
                        ac -= 1;
                    }

                    AStack[AStackPtr] = lst;
                    AStackPtr += 1;
                }
                break;

            case MakeListOpcode:
            {
                FAssert(InstructionArg(obj) > 0);

                FObject lst = EmptyListObject;
                int ac = InstructionArg(obj);
                while (ac > 0)
                {
                    AStackPtr -= 1;
                    lst = MakePair(AStack[AStackPtr], lst);
                    ac -= 1;
                }

                AStack[AStackPtr] = lst;
                AStackPtr += 1;
                break;
            }

            case PushCStackOpcode:
            {
                FFixnum arg = InstructionArg(obj);

                while (arg > 0)
                {
                    FAssert(AStackPtr > 0);

                    AStackPtr -= 1;
                    CStack[CStackPtr] = AStack[AStackPtr];
                    CStackPtr += 1;
                    arg -= 1;
                }
                break;
            }

            case PushNoValueOpcode:
            {
                FFixnum arg = InstructionArg(obj);

                while (arg > 0)
                {
                    CStack[CStackPtr] = NoValueObject;
                    CStackPtr += 1;
                    arg -= 1;
                }

                break;
            }

            case PushWantValuesOpcode:
                CStack[CStackPtr] = WantValuesObject;
                CStackPtr += 1;
                break;

            case PopCStackOpcode:
                FAssert(CStackPtr >= InstructionArg(obj));

                CStackPtr -= InstructionArg(obj);
                break;

            case SaveFrameOpcode:
                CStack[CStackPtr] = Frame;
                CStackPtr += 1;
                break;

            case RestoreFrameOpcode:
                FAssert(CStackPtr > 0);

                CStackPtr -= 1;
                Frame = CStack[CStackPtr];
                break;

            case MakeFrameOpcode:
                Frame = MakeVector(InstructionArg(obj), 0, NoValueObject);
                break;

            case PushFrameOpcode:
                AStack[AStackPtr] = Frame;
                AStackPtr += 1;
                break;

            case GetCStackOpcode:
                FAssert(InstructionArg(obj) <= CStackPtr);
                FAssert(InstructionArg(obj) > 0);

                AStack[AStackPtr] = CStack[CStackPtr - InstructionArg(obj)];
                AStackPtr += 1;
                break;

            case SetCStackOpcode:
                FAssert(InstructionArg(obj) <= CStackPtr);
                FAssert(InstructionArg(obj) > 0);
                FAssert(AStackPtr > 0);

                AStackPtr -= 1;
                CStack[CStackPtr - InstructionArg(obj)] = AStack[AStackPtr];
                break;

            case GetFrameOpcode:
                FAssert(VectorP(Frame));
                FAssert(InstructionArg(obj) < AsVector(Frame)->Length);

                AStack[AStackPtr] = AsVector(Frame)->Vector[InstructionArg(obj)];
                AStackPtr += 1;
                break;

            case SetFrameOpcode:
                FAssert(VectorP(Frame));
                FAssert(InstructionArg(obj) < AsVector(Frame)->Length);
                FAssert(AStackPtr > 0);

                AStackPtr -= 1;
                AsVector(Frame)->Vector[InstructionArg(obj)] = AStack[AStackPtr];
                break;

            case GetVectorOpcode:
                FAssert(AStackPtr > 0);
                FAssert(VectorP(AStack[AStackPtr - 1]));
                FAssert(InstructionArg(obj) < AsVector(AStack[AStackPtr - 1])->Length);

                AStack[AStackPtr - 1] = AsVector(AStack[AStackPtr - 1])->Vector[
                        InstructionArg(obj)];
                break;

            case SetVectorOpcode:
                FAssert(AStackPtr > 1);
                FAssert(VectorP(AStack[AStackPtr - 1]));
                FAssert(InstructionArg(obj) < AsVector(AStack[AStackPtr - 1])->Length);

                AsVector(AStack[AStackPtr - 1])->Vector[InstructionArg(obj)] =
                        AStack[AStackPtr - 2];
                AStackPtr -= 2;
                break;

            case GetGlobalOpcode:
                FAssert(AStackPtr > 0);
                FAssert(GlobalP(AStack[AStackPtr - 1]));
                FAssert(BoxP(AsGlobal(AStack[AStackPtr - 1])->Box));

                if (AsGlobal(AStack[AStackPtr - 1])->State == GlobalUndefined)
                {
                    FAssert(AsGlobal(AStack[AStackPtr - 1])->Interactive == TrueObject);

                    RaiseException(Assertion, AsProcedure(Proc)->Name, UndefinedMessage,
                            List(AStack[AStackPtr - 1]));
                }

                AStack[AStackPtr - 1] = Unbox(AsGlobal(AStack[AStackPtr - 1])->Box);
                break;

            case SetGlobalOpcode:
                FAssert(AStackPtr > 1);
                FAssert(GlobalP(AStack[AStackPtr - 1]));
                FAssert(BoxP(AsGlobal(AStack[AStackPtr - 1])->Box));

                if (AsGlobal(AStack[AStackPtr - 1])->State == GlobalImported
                        || AsGlobal(AStack[AStackPtr - 1])->State == GlobalImportedModified)
                {
                    FAssert(AsGlobal(AStack[AStackPtr - 1])->Interactive == TrueObject);

                    AsGlobal(AStack[AStackPtr - 1])->Box = MakeBox(AStack[AStackPtr - 2]);
                    AsGlobal(AStack[AStackPtr - 1])->State = GlobalDefined;
                }

                AsBox(AsGlobal(AStack[AStackPtr - 1])->Box)->Value = AStack[AStackPtr - 2];
                AStackPtr -= 2;
                break;

            case GetBoxOpcode:
                FAssert(AStackPtr > 0);
                FAssert(BoxP(AStack[AStackPtr - 1]));

                AStack[AStackPtr - 1] = Unbox(AStack[AStackPtr - 1]);
                break;

            case SetBoxOpcode:
                FAssert(AStackPtr > 1);
                FAssert(BoxP(AStack[AStackPtr - 1]));

                AsBox(AStack[AStackPtr - 1])->Value = AStack[AStackPtr - 2];
                AStackPtr -= 2;
                break;

            case DiscardResultOpcode:
                FAssert(AStackPtr >= 1);

                AStackPtr -= 1;
                break;

            case PopAStackOpcode:
                FAssert(AStackPtr >= InstructionArg(obj));

                AStackPtr -= InstructionArg(obj);
                break;

            case DuplicateOpcode:
                FAssert(AStackPtr >= 1);

                AStack[AStackPtr] = AStack[AStackPtr - 1];
                AStackPtr += 1;
                break;

            case ReturnOpcode:
                if (CStackPtr == 0)
                {
                    FAssert(AStackPtr == 1);
                    return(AStack[0]);
                }

                FAssert(CStackPtr >= 2);
                FAssert(FixnumP(CStack[CStackPtr - 1]));
                FAssert(ProcedureP(CStack[CStackPtr - 2]));

                CStackPtr -= 1;
                IP = AsFixnum(CStack[CStackPtr]);
                CStackPtr -= 1;
                Proc = CStack[CStackPtr];

                FAssert(VectorP(AsProcedure(Proc)->Code));

                cv = AsVector(AsProcedure(Proc)->Code)->Vector;
                Frame = NoValueObject;
                break;

            case CallOpcode:
            {
                FAssert(AStackPtr > 0);

                AStackPtr -= 1;
                FObject op = AStack[AStackPtr];
                if (ProcedureP(op))
                {
                    CStack[CStackPtr] = Proc;
                    CStackPtr += 1;
                    CStack[CStackPtr] = MakeFixnum(IP);
                    CStackPtr += 1;

                    Proc = op;
                    FAssert(VectorP(AsProcedure(Proc)->Code));

                    cv = AsVector(AsProcedure(Proc)->Code)->Vector;
                    IP = 0;
                    Frame = NoValueObject;
                }
                else if (PrimitiveP(op))
                {
                    FAssert(AStackPtr >= ArgCount);

                    AStackPtr -= ArgCount;
                    AStack[AStackPtr] = AsPrimitive(op)->PrimitiveFn(ArgCount, AStack + AStackPtr);
                    AStackPtr += 1;
                }
                else
                    RaiseException(Assertion, AsProcedure(Proc)->Name, NotCallable, List(op));
                break;
            }

            case TailCallOpcode:
            {
                FAssert(AStackPtr > 0);

                AStackPtr -= 1;
                FObject op = AStack[AStackPtr];
                if (ProcedureP(op))
                {
                    Proc = op;
                    FAssert(VectorP(AsProcedure(Proc)->Code));

                    cv = AsVector(AsProcedure(Proc)->Code)->Vector;
                    IP = 0;
                    Frame = NoValueObject;
                }
                else if (PrimitiveP(op))
                {
                    FAssert(AStackPtr >= ArgCount);

                    AStackPtr -= ArgCount;
                    AStack[AStackPtr] = AsPrimitive(op)->PrimitiveFn(ArgCount, AStack + AStackPtr);
                    AStackPtr += 1;

                    if (CStackPtr == 0)
                    {
                        FAssert(AStackPtr == 1);
                        return(AStack[0]);
                    }

                    FAssert(CStackPtr >= 2);

                    CStackPtr -= 1;
                    IP = AsFixnum(CStack[CStackPtr]);
                    CStackPtr -= 1;
                    Proc = CStack[CStackPtr];

                    FAssert(ProcedureP(Proc));
                    FAssert(VectorP(AsProcedure(Proc)->Code));

                    cv = AsVector(AsProcedure(Proc)->Code)->Vector;
                    Frame = NoValueObject;
                }
                else
                    RaiseException(Assertion, AsProcedure(Proc)->Name, NotCallable, List(op));
                break;
            }

            case SetArgCountOpcode:
                ArgCount = InstructionArg(obj);
                break;

            case MakeClosureOpcode:
            {
                FAssert(AStackPtr > 0);

                FObject v[3];

                AStackPtr -= 1;
                v[0] = AStack[AStackPtr - 1];
                v[1] = AStack[AStackPtr];
                v[2] = MakeInstruction(TailCallOpcode, 0);
                FObject proc = MakeProcedure(NoValueObject, MakeVector(3, v, NoValueObject), 0,
                        FalseObject);
                AsProcedure(proc)->Flags |= PROCEDURE_FLAG_CLOSURE;
                AStack[AStackPtr - 1] = proc;
                break;
            }

            case IfFalseOpcode:
                FAssert(AStackPtr > 0);
                FAssert(IP + InstructionArg(obj) >= 0);

                AStackPtr -= 1;
                if (AStack[AStackPtr] == FalseObject)
                    IP += InstructionArg(obj);
                break;

            case IfEqvPOpcode:
                FAssert(AStackPtr > 1);
                FAssert(IP + InstructionArg(obj) >= 0);

                AStackPtr -= 1;
                if (AStack[AStackPtr] == AStack[AStackPtr - 1])
                    IP += InstructionArg(obj);
                break;

            case GotoRelativeOpcode:
                FAssert(IP + InstructionArg(obj) >= 0);

                IP += InstructionArg(obj);
                break;

            case GotoAbsoluteOpcode:
                FAssert(InstructionArg(obj) >= 0);

                IP = InstructionArg(obj);
                break;

            case CheckValuesOpcode:
                FAssert(AStackPtr > 0);
                FAssert(InstructionArg(obj) != 1);

                if (ValuesCountP(AStack[AStackPtr - 1]))
                {
                    AStackPtr -= 1;
                    if (AsValuesCount(AStack[AStackPtr]) != InstructionArg(obj))
                        RaiseException(Assertion, AsProcedure(Proc)->Name,
                                UnexpectedNumberOfValues, EmptyListObject);
                }
                else
                    RaiseException(Assertion, AsProcedure(Proc)->Name, UnexpectedNumberOfValues,
                            EmptyListObject);
                break;

            case RestValuesOpcode:
            {
                FAssert(AStackPtr > 0);
                FAssert(InstructionArg(obj) >= 0);

                int vc;

                if (ValuesCountP(AStack[AStackPtr - 1]))
                {
                    AStackPtr -= 1;
                    vc = AsValuesCount(AStack[AStackPtr]);
                }
                else
                    vc = 1;

                if (vc < InstructionArg(obj))
                    RaiseException(Assertion, AsProcedure(Proc)->Name, UnexpectedNumberOfValues,
                            EmptyListObject);
                else if (vc == InstructionArg(obj))
                {
                    AStack[AStackPtr] = EmptyListObject;
                    AStackPtr += 1;
                }
                else
                {
                    FObject lst = EmptyListObject;

                    while (vc > InstructionArg(obj))
                    {
                        AStackPtr -= 1;
                        lst = MakePair(AStack[AStackPtr], lst);
                        vc -= 1;
                    }

                    AStack[AStackPtr] = lst;
                    AStackPtr += 1;
                }

                break;
            }

            case ValuesOpcode:
                if (ArgCount != 1)
                {
                    if (CStackPtr >= 3 && WantValuesObjectP(CStack[CStackPtr - 3]))
                    {
                        AStack[AStackPtr] = MakeValuesCount(ArgCount);
                        AStackPtr += 1;
                    }
                    else
                    {
                        FAssert(CStackPtr >= 2);
                        FAssert(FixnumP(CStack[CStackPtr - 1]));
                        FAssert(ProcedureP(CStack[CStackPtr - 2]));
                        FAssert(VectorP(AsProcedure(CStack[CStackPtr - 2])->Code));

                        FObject cd = AsVector(AsProcedure(CStack[CStackPtr - 2])->Code)->Vector[
                                AsFixnum(CStack[CStackPtr - 1])];
                        if (InstructionP(cd) == 0 || InstructionOpcode(cd) != DiscardResultOpcode)
                           RaiseExceptionC(Assertion, "values",
                                   "values: caller not expecting multiple values",
                                   List(AsProcedure(CStack[CStackPtr - 2])->Name));

                        if (ArgCount == 0)
                        {
                            AStack[AStackPtr] = NoValueObject;
                            AStackPtr += 1;
                        }
                        else
                        {
                            FAssert(AStackPtr >= ArgCount);

                            AStackPtr -= (ArgCount - 1);
                        }
                    }
                }
                break;

            case ApplyOpcode:
            {
                if (ArgCount < 2)
                   RaiseExceptionC(Assertion, "apply", "apply: expected at least two arguments",
                           EmptyListObject);

                FObject prc = AStack[AStackPtr - ArgCount];
                FObject lst = AStack[AStackPtr - 1];

                int adx = ArgCount;
                while (adx > 2)
                {
                    AStack[AStackPtr - adx] = AStack[AStackPtr - adx + 1];
                    adx -= 1;
                }

                ArgCount -= 2;
                AStackPtr -= 2;

                FObject ptr = lst;
                while (PairP(ptr))
                {
                    AStack[AStackPtr] = First(ptr);
                    AStackPtr += 1;
                    ArgCount += 1;
                    ptr = Rest(ptr);
                }

                if (ptr != EmptyListObject)
                   RaiseExceptionC(Assertion, "apply", "apply: expected a proper list", List(lst));

                AStack[AStackPtr] = prc;
                AStackPtr += 1;
                break;
            }

            case CaseLambdaOpcode:
                int cc = InstructionArg(obj);
                int idx = 0;

                while (cc > 0)
                {
                    FAssert(VectorP(AsProcedure(Proc)->Code));
                    FAssert(AsVector(AsProcedure(Proc)->Code)->Vector == cv);
                    FAssert(IP + idx >= 0);
                    FAssert(IP + idx < AsVector(AsProcedure(Proc)->Code)->Length);

                    FObject prc = cv[IP + idx];

                    FAssert(ProcedureP(prc));

                    if ((AsProcedure(prc)->RestArg == TrueObject
                            && ArgCount + 1 >= AsProcedure(prc)->ArgCount)
                            || AsProcedure(prc)->ArgCount == ArgCount)
                    {
                        Proc = prc;
                        FAssert(VectorP(AsProcedure(Proc)->Code));

                        cv = AsVector(AsProcedure(Proc)->Code)->Vector;
                        IP = 0;
                        Frame = NoValueObject;

                        break;
                    }

                    idx += 1;
                    cc -= 1;
                }

                if (cc == 0)
                    RaiseExceptionC(Assertion, "case-lambda", "case-lambda: no matching case",
                            List(MakeFixnum(ArgCount)));
                break;

            }
        }
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
            AsPair(First(lst))->Rest = argv[2];
            return(NoValueObject);
        }

        lst = Rest(lst);
    }

    FAssert(lst == EmptyListObject);

    AsPair(argv[1])->Rest = argv[2];
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
    NotCallable = MakeStringC("not callable");
    UnexpectedNumberOfValues = MakeStringC("unexpected number of values");
    UndefinedMessage = MakeStringC("variable is undefined");

    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(Bedrock, BedrockLibrary, Primitives[idx]);

    DynamicEnvironment = EmptyListObject;

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
