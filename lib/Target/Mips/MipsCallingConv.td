//===-- MipsCallingConv.td - Calling Conventions for Mips --*- tablegen -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// This describes the calling conventions for Mips architecture.
//===----------------------------------------------------------------------===//

/// CCIfSubtarget - Match if the current subtarget has a feature F.
class CCIfSubtarget<string F, CCAction A, string Invert = "">
    : CCIf<!strconcat(Invert,
                      "static_cast<const MipsSubtarget&>"
			"(State.getMachineFunction().getSubtarget()).",
                      F),
           A>;

// The inverse of CCIfSubtarget
class CCIfSubtargetNot<string F, CCAction A> : CCIfSubtarget<F, A, "!">;

/// Match if the original argument (before lowering) was a float.
/// For example, this is true for i32's that were lowered from soft-float.
class CCIfOrigArgWasNotFloat<CCAction A>
    : CCIf<"!static_cast<MipsCCState *>(&State)->WasOriginalArgFloat(ValNo)",
           A>;

/// Match if the original argument (before lowering) was a 128-bit float (i.e.
/// long double).
class CCIfOrigArgWasF128<CCAction A>
    : CCIf<"static_cast<MipsCCState *>(&State)->WasOriginalArgF128(ValNo)", A>;

/// Match if this specific argument is a vararg.
/// This is slightly different fro CCIfIsVarArg which matches if any argument is
/// a vararg.
class CCIfArgIsVarArg<CCAction A>
    : CCIf<"!static_cast<MipsCCState *>(&State)->IsCallOperandFixed(ValNo)", A>;


/// Match if the special calling conv is the specified value.
class CCIfSpecialCallingConv<string CC, CCAction A>
    : CCIf<"static_cast<MipsCCState *>(&State)->getSpecialCallingConv() == "
               "MipsCCState::" # CC, A>;


//===----------------------------------------------------------------------===//
// Mips O32 Calling Convention
//===----------------------------------------------------------------------===//

def CC_MipsO32 : CallingConv<[
  // Promote i8/i16 arguments to i32.
  CCIfType<[i1, i8, i16], CCPromoteToType<i32>>,

  // Integer values get stored in stack slots that are 4 bytes in
  // size and 4-byte aligned.
  CCIfType<[i32, f32], CCAssignToStack<4, 4>>,

  // Integer values get stored in stack slots that are 8 bytes in
  // size and 8-byte aligned.
  CCIfType<[f64], CCAssignToStack<8, 8>>
]>;

// Only the return rules are defined here for O32. The rules for argument
// passing are defined in MipsISelLowering.cpp.
def RetCC_MipsO32 : CallingConv<[
  // i32 are returned in registers V0, V1, A0, A1
  CCIfType<[i32], CCAssignToReg<[V0, V1, A0, A1]>>,

  // f32 are returned in registers F0, F2
  CCIfType<[f32], CCAssignToReg<[F0, F2]>>,

  CCIfType<[f64], CCIfSubtargetNot<"isFP64bit()", CCAssignToReg<[D0, D1]>>>
]>;

def CC_MipsO32_FP32 : CustomCallingConv;

def CC_MipsO32_FP : CallingConv<[
  CCIfSubtargetNot<"isFP64bit()", CCDelegateTo<CC_MipsO32_FP32>>
]>;

//===----------------------------------------------------------------------===//
// Mips EABI Calling Convention
//===----------------------------------------------------------------------===//

def CC_MipsEABI : CallingConv<[
  // Promote i8/i16 arguments to i32.
  CCIfType<[i8, i16], CCPromoteToType<i32>>,

  // Integer arguments are passed in integer registers.
  CCIfType<[i32], CCAssignToReg<[A0, A1, A2, A3, T0, T1, T2, T3]>>,

  // Single fp arguments are passed in pairs within 32-bit mode
  CCIfType<[f32], CCIfSubtarget<"isSingleFloat()",
                  CCAssignToReg<[F12, F13, F14, F15, F16, F17, F18, F19]>>>,

  CCIfType<[f32], CCIfSubtargetNot<"isSingleFloat()",
                  CCAssignToReg<[F12, F14, F16, F18]>>>,

  // The first 4 double fp arguments are passed in single fp registers.
  CCIfType<[f64], CCIfSubtargetNot<"isSingleFloat()",
                  CCAssignToReg<[D6, D7, D8, D9]>>>,

  // Integer values get stored in stack slots that are 4 bytes in
  // size and 4-byte aligned.
  CCIfType<[i32, f32], CCAssignToStack<4, 4>>,

  // Integer values get stored in stack slots that are 8 bytes in
  // size and 8-byte aligned.
  CCIfType<[f64], CCIfSubtargetNot<"isSingleFloat()", CCAssignToStack<8, 8>>>
]>;

def RetCC_MipsEABI : CallingConv<[
  // i32 are returned in registers V0, V1
  CCIfType<[i32], CCAssignToReg<[V0, V1]>>,

  // f32 are returned in registers F0, F1
  CCIfType<[f32], CCAssignToReg<[F0, F1]>>,

  // f64 are returned in register D0
  CCIfType<[f64], CCIfSubtargetNot<"isSingleFloat()", CCAssignToReg<[D0]>>>
]>;

//===----------------------------------------------------------------------===//
// Mips FastCC Calling Convention
//===----------------------------------------------------------------------===//
def CC_MipsO32_FastCC : CallingConv<[

  // Stack parameter slots for f64 are 64-bit doublewords and 8-byte aligned.
  CCIfType<[f64], CCAssignToStack<8, 8>>
]>;

def CC_Mips_FastCC : CallingConv<[
  // Handles byval parameters.
  CCIfByVal<CCPassByVal<4, 4>>,

  // Promote i8/i16 arguments to i32.
  CCIfType<[i8, i16], CCPromoteToType<i32>>,

  // Integer arguments are passed in integer registers. All scratch registers,
  // except for AT, V0 and T9, are available to be used as argument registers.
  CCIfType<[i32], CCIfSubtargetNot<"isTargetNaCl()",
      CCAssignToReg<[A0, A1, A2, A3, T0, T1, T2, T3, T4, T5, T6, T7, T8, V1]>>>,

  // In NaCl, T6, T7 and T8 are reserved and not available as argument
  // registers for fastcc.  T6 contains the mask for sandboxing control flow
  // (indirect jumps and calls).  T7 contains the mask for sandboxing memory
  // accesses (loads and stores).  T8 contains the thread pointer.
  CCIfType<[i32], CCIfSubtarget<"isTargetNaCl()",
      CCAssignToReg<[A0, A1, A2, A3, T0, T1, T2, T3, T4, T5, V1]>>>,

  // f32 arguments are passed in single-precision floating pointer registers.
  CCIfType<[f32], CCIfSubtarget<"useOddSPReg()",
      CCAssignToReg<[F0, F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12, F13,
                     F14, F15, F16, F17, F18, F19]>>>,

  // Don't use odd numbered single-precision registers for -mno-odd-spreg.
  CCIfType<[f32], CCIfSubtarget<"noOddSPReg()",
      CCAssignToReg<[F0, F2, F4, F6, F8, F10, F12, F14, F16, F18]>>>,

  // Stack parameter slots for i32 and f32 are 32-bit words and 4-byte aligned.
  CCIfType<[i32, f32], CCAssignToStack<4, 4>>,

  CCIfSubtarget<"isABI_EABI()", CCDelegateTo<CC_MipsEABI>>,
  CCDelegateTo<CC_MipsO32_FastCC>
]>;

//===----------------------------------------------------------------------===//
// Mips Calling Convention Dispatch
//===----------------------------------------------------------------------===//

def RetCC_Mips : CallingConv<[
  CCIfSubtarget<"isABI_EABI()", CCDelegateTo<RetCC_MipsEABI>>,
  CCDelegateTo<RetCC_MipsO32>
]>;

def CC_Mips_ByVal : CallingConv<[
  CCIfSubtarget<"isABI_O32()", CCIfByVal<CCPassByVal<4, 4>>>,
  CCIfByVal<CCPassByVal<8, 8>>
]>;

def CC_Mips16RetHelper : CallingConv<[
  CCIfByVal<CCDelegateTo<CC_Mips_ByVal>>,

  // Integer arguments are passed in integer registers.
  CCIfType<[i32], CCAssignToReg<[V0, V1, A0, A1]>>
]>;

def CC_Mips_FixedArg : CallingConv<[
  // Mips16 needs special handling on some functions.
  CCIf<"State.getCallingConv() != CallingConv::Fast",
      CCIfSpecialCallingConv<"Mips16RetHelperConv",
           CCDelegateTo<CC_Mips16RetHelper>>>,

  CCIfByVal<CCDelegateTo<CC_Mips_ByVal>>,

  // f128 needs to be handled similarly to f32 and f64 on hard-float. However,
  // f128 is not legal and is lowered to i128 which is further lowered to a pair
  // of i64's.
  // This presents us with a problem for the calling convention since hard-float
  // still needs to pass them in FPU registers. We therefore resort to a
  // pre-analyze (see PreAnalyzeFormalArgsForF128()) step to pass information on
  // whether the argument was originally an f128 into the tablegen-erated code.
  //
  // f128 should only occur for the N64 ABI where long double is 128-bit. On
  // N32, long double is equivalent to double.
  CCIfType<[i64],
      CCIfSubtargetNot<"abiUsesSoftFloat()",
          CCIfOrigArgWasF128<CCBitConvertToType<f64>>>>,

  CCIfCC<"CallingConv::Fast", CCDelegateTo<CC_Mips_FastCC>>,

  // FIXME: There wasn't an EABI case in the original code and it seems unlikely
  //        that it's the same as CC_MipsN
  CCIfSubtarget<"isABI_O32()", CCDelegateTo<CC_MipsO32_FP>>
]>;

def CC_Mips_VarArg : CallingConv<[
  CCIfByVal<CCDelegateTo<CC_Mips_ByVal>>,

  // FIXME: There wasn't an EABI case in the original code and it seems unlikely
  //        that it's the same as CC_MipsN_VarArg
  CCIfSubtarget<"isABI_O32()", CCDelegateTo<CC_MipsO32_FP>>
]>;

def CC_Mips : CallingConv<[
  CCIfVarArg<CCIfArgIsVarArg<CCDelegateTo<CC_Mips_VarArg>>>,
  CCDelegateTo<CC_Mips_FixedArg>
]>;

//===----------------------------------------------------------------------===//
// Callee-saved register lists.
//===----------------------------------------------------------------------===//

def CSR_SingleFloatOnly : CalleeSavedRegs<(add (sequence "F%u", 31, 20), RA, FP,
                                               (sequence "S%u", 7, 0))>;

def CSR_O32_FPXX : CalleeSavedRegs<(add (sequence "D%u", 15, 10), RA, FP,
                                        (sequence "F%u", 31, 20),
                                        (sequence "S%u", 7, 0))>;


def CSR_O32 : CalleeSavedRegs<(add (sequence "F%u",31,20),
                                   (sequence "D%u", 15, 10), RA, FP,
                                   (sequence "S%u", 7, 0))>;

def CSR_Mips16RetHelper :
  CalleeSavedRegs<(add V0, V1, FP,
                   (sequence "A%u", 3, 0), (sequence "F%u", 31, 20),
  (sequence "S%u", 7, 0),
                   (sequence "D%u", 15, 10))>;
