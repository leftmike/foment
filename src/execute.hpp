/*

Foment

*/

#ifndef __EXECUTE_HPP__
#define __EXECUTE_HPP__

extern FObject WrongNumberOfArguments;
extern FObject NotCallable;
extern FObject UnexpectedNumberOfValues;

// ---- Instruction ----

#define MakeInstruction(op, arg)\
    MakeImmediate(((((FFixnum) (arg)) << 8) | (op & 0xFF)), InstructionTag)
#define InstructionOpcode(obj) ((FOpcode) (AsValue(obj) & 0xFF))
#define InstructionArg(obj) ((FFixnum) (AsValue(obj) >> 8))

/*
Each instruction consists of <opcode> <arg>. The <arg> is a FFixnum.

The virtual machine state consists of an AStack (and an AStackPtr); a CStack and (a CStackPtr);
a Frame, an ArgCount, an IP, and a Proc. The Frame is optionally a vector on the heap.
The IP (instruction pointer) is an index into the Proc's (procedure) code vector.
*/

typedef enum
{
    // if (ArgCount != <arg>)
    //     RaiseException
    CheckCountOpcode = 0,

    // if (ArgCount < <arg>)
    //     RaiseException
    // else if (ArgCount == <arg>) {
    //     AStack[AStackPtr] = EmptyListObject; AStackPtr += 1;
    // } else {
    //     lst = EmptyListObject;
    //
    //     ac = ArgCount;
    //     while (ac > <arg>) {
    //         AStackPtr -= 1;
    //         lst = MakePair(AStack[AStackPtr], lst);
    //         ac -= 1;
    //     }
    //
    //     AStack[AStackPtr] = lst;
    //     AStackPtr += 1;
    RestArgOpcode,

    // lst = EmptyListObject
    // while (<arg> > 0) {
    //     AStackPtr -= 1; lst = MakePair(AStack[AStackPtr], lst); <arg> -= 1;}
    // AStack[AStackPtr] = lst; AStackPtr += 1;
    MakeListOpcode,

    // while (<arg> > 0) {
    //     AStackPtr -= 1; CStack[CStackPtr] = AStack[AStackPtr]; CStackPtr += 1; <arg> -= 1;}
    PushCStackOpcode,

    // while (<arg> > 0) {
    //     CStack[CStackPtr] = NoValueObject; CStackPtr += 1; <arg> -= 1;}
    PushNoValueOpcode,

    // CStack[CStackPtr] = WantValuesObject; CStackPtr += 1;
    PushWantValuesOpcode,

    // CStackPtr -= <arg>
    PopCStackOpcode,

    // CStack[CStackPtr] = Frame; CStackPtr += 1
    SaveFrameOpcode,

    // CStackPtr -= 1; Frame = CStack[CStackPtr]
    RestoreFrameOpcode,

    // Frame = MakeVector(<arg>)
    MakeFrameOpcode,

    // AStack[AStackPtr] = Frame; AStackPtr += 1
    PushFrameOpcode,

    // AStack[AStackPtr] = CStack[CStackPtr - <arg>]; AStackPtr += 1
    GetCStackOpcode,

    // AStackPtr -= 1; CStack[CStackPtr - <arg>] = AStack[AStackPtr]
    SetCStackOpcode,

    // AStack[AStackPtr] = Frame[<arg>]; AStackPtr += 1
    GetFrameOpcode,

    // AStackPtr -= 1; Frame[<arg>] = AStack[AStackPtr]
    SetFrameOpcode,

    // AStack[AStackPtr - 1] = AsVector(AStack[AStackPtr - 1])[<arg>]
    GetVectorOpcode,

    // AsVector(AStack[AStackPtr - 1])[<arg>] = AStack[AStackPtr - 2]; AStackPtr -= 2
    SetVectorOpcode,

    // AStack[AStackPtr - 1] = AsGlobal(AStack[AStackPtr - 1])->Value
    GetGlobalOpcode,

    // AsGlobal(AStack[AStackPtr - 1])->Value = AStack[AStackPtr - 2]; AStackPtr -= 2
    SetGlobalOpcode,

    // AStack[AStackPtr - 1] = Unbox(AStack[AStackPtr - 1])
    GetBoxOpcode,

    // AsBox(AStack[AStackPtr - 1])->Value = AStack[AStackPtr - 2]; AStackPtr -= 2
    SetBoxOpcode,

    // AStackPtr -= 1
    // NOTE: multiple values interacts with this opcode; you probably want PopAStackOpcode instead.
    DiscardResultOpcode,

    // AStackPtr -= <arg>
    PopAStackOpcode,

    // AStack[AStackPtr] = AStack[AStackPtr - 1]; AStackPtr += 1
    DuplicateOpcode,

    // CStackPtr -= 1; CStack[CStackPtr] = IP
    // CStackPtr -= 1; CStack[CStackPtr] = Proc
    ReturnOpcode,

    // CStack[CStackPtr] = Proc; CStackPtr += 1;
    // CStack[CStackPtr] = IP; CStackPtr += 1;
    // AStackPtr -= 1; Proc = AStack[AStackPtr];
    // IP = 0;
    CallOpcode,
    CallProcOpcode,
    CallPrimOpcode,

    // AStackPtr -= 1; Proc = AStack[AStackPtr];
    // IP = 0;
    TailCallOpcode,
    TailCallProcOpcode,
    TailCallPrimOpcode,

    // ArgCount = <arg>;
    SetArgCountOpcode,

    // AStackPtr -= 1;
    // AStack[AStackPtr - 1] = MakeProcedure(
    //        MakeVector(AStack[AStackPtr - 1], AStack[AStackPtr], TailCallOpcode))
    MakeClosureOpcode,

    // AStackPtr -= 1;
    // if (AStack[AStackPtr] == FalseObject) IP += <arg>
    IfFalseOpcode,

    // AStackPtr -= 1;
    // if (AStack[AStackPtr] == AStack[AStackPtr - 1]) IP += <arg>
    IfEqvPOpcode,

    // IP += <arg>
    GotoRelativeOpcode,

    // IP = <arg>
    GotoAbsoluteOpcode,

    // if (ValuesCountP(AStack[AStackPtr - 1])) {
    //     AStackPtr -= 1;
    //     if (ValuesCount(AStack[AStackPtr]) != <arg>)
    //         RaiseException
    // } else
    //     RaiseException
    CheckValuesOpcode,

    // if (ValuesCountP(AStack[AStackPtr - 1])) {
    //     AStackPtr -= 1;
    //     vc = ValuesCount(AStack[AStackPtr]);
    // } else
    //     vc = 1;
    //
    // if (vc < <arg>)
    //     RaiseException
    // else if (vc == <arg>) {
    //     AStack[AStackPtr] = EmptyListObject; AStackPtr += 1;
    // } else {
    //     lst = EmptyListObject;
    //
    //     while (vc > <arg>) {
    //         AStackPtr -= 1;
    //         lst = MakePair(AStack[AStackPtr], lst);
    //         vc -= 1;
    //     }
    //
    //     AStack[AStackPtr] = lst;
    //     AStackPtr += 1;
    RestValuesOpcode,

    // if (ArgCount != 1) {
    //     if (WantValuesObjectP(CStack[CStackPtr - 3])) {
    //         AStack[AStackPtr] = MakeValuesCount(ArgCount);
    //         AStackPtr += 1;
    //     } else {
    //         if (AsProcedure(CStack[CStackPtr - 2])->Vector[CStack[CStackPtr - 1]]
    //                   != DiscardResultOpcode)
    //            RaiseException();
    //         if (ArgCount == 0) {
    //             AStack[AStackPtr] = NoValueObject;
    //             AStackPtr += 1;
    //         } else
    //             AStackPtr -= (ArgCount - 1);
    //     }
    // }
    ValuesOpcode,

    //
    
    ApplyOpcode,

    //
    
    CaseLambdaOpcode
} FOpcode;

#endif // __EXECUTE_HPP__
