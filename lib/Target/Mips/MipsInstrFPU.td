//===-- MipsInstrFPU.td - Mips FPU Instruction Information -*- tablegen -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file describes the Mips FPU instruction set.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Floating Point Instructions
// ------------------------
// * 64bit fp:
//    - 32 64-bit registers (default mode)
//    - 16 even 32-bit registers (32-bit compatible mode) for
//      single and double access.
// * 32bit fp:
//    - 16 even 32-bit registers - single and double (aliased)
//    - 32 32-bit registers (within single-only mode)
//===----------------------------------------------------------------------===//

// Floating Point Compare and Branch
def SDT_MipsFPBrcond : SDTypeProfile<0, 2, [SDTCisInt<0>,
                                            SDTCisVT<1, OtherVT>]>;
def SDT_MipsFPCmp : SDTypeProfile<0, 3, [SDTCisSameAs<0, 1>, SDTCisFP<1>,
                                         SDTCisVT<2, i32>]>;
def SDT_MipsCMovFP : SDTypeProfile<1, 3, [SDTCisSameAs<0, 1>, SDTCisVT<2, i32>,
                                          SDTCisSameAs<1, 3>]>;
def SDT_MipsTruncIntFP : SDTypeProfile<1, 1, [SDTCisFP<0>, SDTCisFP<1>]>;
def SDT_MipsBuildPairF64 : SDTypeProfile<1, 2, [SDTCisVT<0, f64>,
                                                SDTCisVT<1, i32>,
                                                SDTCisSameAs<1, 2>]>;
def SDT_MipsExtractElementF64 : SDTypeProfile<1, 2, [SDTCisVT<0, i32>,
                                                     SDTCisVT<1, f64>,
                                                     SDTCisVT<2, i32>]>;

def MipsFPCmp : SDNode<"MipsISD::FPCmp", SDT_MipsFPCmp, [SDNPOutGlue]>;
def MipsCMovFP_T : SDNode<"MipsISD::CMovFP_T", SDT_MipsCMovFP, [SDNPInGlue]>;
def MipsCMovFP_F : SDNode<"MipsISD::CMovFP_F", SDT_MipsCMovFP, [SDNPInGlue]>;
def MipsFPBrcond : SDNode<"MipsISD::FPBrcond", SDT_MipsFPBrcond,
                          [SDNPHasChain, SDNPOptInGlue]>;
def MipsTruncIntFP : SDNode<"MipsISD::TruncIntFP", SDT_MipsTruncIntFP>;
def MipsBuildPairF64 : SDNode<"MipsISD::BuildPairF64", SDT_MipsBuildPairF64>;
def MipsExtractElementF64 : SDNode<"MipsISD::ExtractElementF64",
                                   SDT_MipsExtractElementF64>;

// Operand for printing out a condition code.
let PrintMethod = "printFCCOperand", DecoderMethod = "DecodeCondCode" in
  def condcode : Operand<i32>;

//===----------------------------------------------------------------------===//
// Feature predicates.
//===----------------------------------------------------------------------===//

def IsFP64bit        : Predicate<"Subtarget->isFP64bit()">,
                       AssemblerPredicate<"FeatureFP64Bit">;
def NotFP64bit       : Predicate<"!Subtarget->isFP64bit()">,
                       AssemblerPredicate<"!FeatureFP64Bit">;
def IsSingleFloat    : Predicate<"Subtarget->isSingleFloat()">,
                       AssemblerPredicate<"FeatureSingleFloat">;
def IsNotSingleFloat : Predicate<"!Subtarget->isSingleFloat()">,
                       AssemblerPredicate<"!FeatureSingleFloat">;

//===----------------------------------------------------------------------===//
// Mips FGR size adjectives.
// They are mutually exclusive.
//===----------------------------------------------------------------------===//

class FGR_32 { list<Predicate> FGRPredicates = [NotFP64bit]; }
class FGR_64 { list<Predicate> FGRPredicates = [IsFP64bit]; }

//===----------------------------------------------------------------------===//

// FP immediate patterns.
def fpimm0 : PatLeaf<(fpimm), [{
  return N->isExactlyValue(+0.0);
}]>;

def fpimm0neg : PatLeaf<(fpimm), [{
  return N->isExactlyValue(-0.0);
}]>;

// Mips condition codes. They must correspond to condcode in MipsInstrInfo.h.
// They must be kept in synch.
def MIPS_FCOND_F    : PatLeaf<(imm), [{
  return N->getZExtValue() == 0 || N->getZExtValue() == 16;
}]>;
def MIPS_FCOND_UN   : PatLeaf<(imm), [{
  return N->getZExtValue() == 1 || N->getZExtValue() == 17;
}]>;
def MIPS_FCOND_OEQ  : PatLeaf<(imm), [{
  return N->getZExtValue() == 2 || N->getZExtValue() == 18;
}]>;
def MIPS_FCOND_UEQ  : PatLeaf<(imm), [{
  return N->getZExtValue() == 3 || N->getZExtValue() == 19;
}]>;
def MIPS_FCOND_OLT  : PatLeaf<(imm), [{
  return N->getZExtValue() == 4 || N->getZExtValue() == 20;
}]>;
def MIPS_FCOND_ULT  : PatLeaf<(imm), [{
  return N->getZExtValue() == 5 || N->getZExtValue() == 21;
}]>;
def MIPS_FCOND_OLE  : PatLeaf<(imm), [{
  return N->getZExtValue() == 6 || N->getZExtValue() == 22;
}]>;
def MIPS_FCOND_ULE  : PatLeaf<(imm), [{
  return N->getZExtValue() == 7 || N->getZExtValue() == 23;
}]>;
def MIPS_FCOND_SF   : PatLeaf<(imm), [{
  return N->getZExtValue() == 8 || N->getZExtValue() == 24;
}]>;
def MIPS_FCOND_NGLE : PatLeaf<(imm), [{
  return N->getZExtValue() == 9 || N->getZExtValue() == 25;
}]>;
def MIPS_FCOND_SEQ  : PatLeaf<(imm), [{
  return N->getZExtValue() == 10 || N->getZExtValue() == 26;
}]>;
def MIPS_FCOND_NGL  : PatLeaf<(imm), [{
  return N->getZExtValue() == 11 || N->getZExtValue() == 27;
}]>;
def MIPS_FCOND_LT   : PatLeaf<(imm), [{
  return N->getZExtValue() == 12 || N->getZExtValue() == 28;
}]>;
def MIPS_FCOND_NGE  : PatLeaf<(imm), [{
  return N->getZExtValue() == 13 || N->getZExtValue() == 29;
}]>;
def MIPS_FCOND_LE   : PatLeaf<(imm), [{
  return N->getZExtValue() == 14 || N->getZExtValue() == 30;
}]>;
def MIPS_FCOND_NGT  : PatLeaf<(imm), [{
  return N->getZExtValue() == 15 || N->getZExtValue() == 31;
}]>;

//===----------------------------------------------------------------------===//
// Instruction Class Templates
//
// A set of multiclasses is used to address the register usage.
//
// S32 - single precision in 16 32bit even fp registers
//       single precision in 32 32bit fp registers in SingleOnly mode
// S64 - single precision in 32 64bit fp registers (In64BitMode)
// D32 - double precision in 16 32bit even fp registers
// D64 - double precision in 32 64bit fp registers (In64BitMode)
//
// Only S32 and D32 are supported right now.
//===----------------------------------------------------------------------===//

class ADDS_FT<string opstr, RegisterOperand RC, InstrItinClass Itin, bit IsComm,
              SDPatternOperator OpNode= null_frag> :
  InstSE<(outs RC:$fd), (ins RC:$fs, RC:$ft),
         !strconcat(opstr, "\t$fd, $fs, $ft"),
         [(set RC:$fd, (OpNode RC:$fs, RC:$ft))], Itin, FrmPL18, opstr> {
  let isCommutable = IsComm;
}

multiclass ADDS_M<string opstr, InstrItinClass Itin, bit IsComm,
                  SDPatternOperator OpNode = null_frag> {
  def _D32 : MMRel, ADDS_FT<opstr, AFGR64Opnd, Itin, IsComm, OpNode>,
             AdditionalRequires<[NotFP64bit]>;
}

class ABSS_FT<string opstr, RegisterOperand DstRC, RegisterOperand SrcRC,
              InstrItinClass Itin, SDPatternOperator OpNode= null_frag> :
  InstSE<(outs DstRC:$fd), (ins SrcRC:$fs), !strconcat(opstr, "\t$fd, $fs"),
         [(set DstRC:$fd, (OpNode SrcRC:$fs))], Itin, FrmPL12, opstr>,
  NeverHasSideEffects;

multiclass ABSS_M<string opstr, InstrItinClass Itin,
                  SDPatternOperator OpNode= null_frag> {
  def _D32 : MMRel, ABSS_FT<opstr, AFGR64Opnd, AFGR64Opnd, Itin, OpNode>,
             AdditionalRequires<[NotFP64bit]>;
}

multiclass ROUND_M<string opstr, InstrItinClass Itin> {
  def _D32 : MMRel, ABSS_FT<opstr, FGR32Opnd, AFGR64Opnd, Itin>,
             AdditionalRequires<[NotFP64bit]>;
}

class MFC1_FT<string opstr, RegisterOperand DstRC, RegisterOperand SrcRC,
              InstrItinClass Itin, SDPatternOperator OpNode= null_frag> :
  InstSE<(outs DstRC:$rt), (ins SrcRC:$fs), !strconcat(opstr, "\t$rt, $fs"),
         [(set DstRC:$rt, (OpNode SrcRC:$fs))], Itin, FrmPL12, opstr>;

class MTC1_FT<string opstr, RegisterOperand DstRC, RegisterOperand SrcRC,
              InstrItinClass Itin, SDPatternOperator OpNode= null_frag> :
  InstSE<(outs DstRC:$fs), (ins SrcRC:$rt), !strconcat(opstr, "\t$rt, $fs"),
         [(set DstRC:$fs, (OpNode SrcRC:$rt))], Itin, FrmPL12, opstr>;

class MTC1_64_FT<string opstr, RegisterOperand DstRC, RegisterOperand SrcRC,
                 InstrItinClass Itin> :
  InstSE<(outs DstRC:$fs), (ins DstRC:$fs_in, SrcRC:$rt),
         !strconcat(opstr, "\t$rt, $fs"), [], Itin, FrmPL12, opstr> {
  // $fs_in is part of a white lie to work around a widespread bug in the FPU
  // implementation. See expandBuildPairF64 for details.
  let Constraints = "$fs = $fs_in";
}

class LW_FT<string opstr, RegisterOperand RC, InstrItinClass Itin,
            SDPatternOperator OpNode= null_frag> :
  InstSE<(outs RC:$rt), (ins mem:$addr), !strconcat(opstr, "\t$rt, $addr"),
         [(set RC:$rt, (OpNode addrDefault:$addr))], Itin, FrmPL26i, opstr> {
  let DecoderMethod = "DecodeFMem";
  let mayLoad = 1;
}

class SW_FT<string opstr, RegisterOperand RC, InstrItinClass Itin,
            SDPatternOperator OpNode= null_frag> :
  InstSE<(outs), (ins RC:$rt, mem:$addr), !strconcat(opstr, "\t$rt, $addr"),
         [(OpNode RC:$rt, addrDefault:$addr)], Itin, FrmPL26i, opstr> {
  let DecoderMethod = "DecodeFMem";
  let mayStore = 1;
}

class MADDS_FT<string opstr, RegisterOperand RC, InstrItinClass Itin,
               SDPatternOperator OpNode = null_frag> :
  InstSE<(outs RC:$fd), (ins RC:$fr, RC:$fs, RC:$ft),
         !strconcat(opstr, "\t$fd, $fr, $fs, $ft"),
         [(set RC:$fd, (OpNode (fmul RC:$fs, RC:$ft), RC:$fr))], Itin,
         FrmPL24, opstr>;

class LWXC1_FT<string opstr, RegisterOperand DRC,
               InstrItinClass Itin, SDPatternOperator OpNode = null_frag> :
  InstSE<(outs DRC:$fd), (ins PtrRC:$base, PtrRC:$index),
         !strconcat(opstr, "\t$fd, ${index}(${base})"),
         [(set DRC:$fd, (OpNode (add iPTR:$base, iPTR:$index)))], Itin,
         FrmPL26i, opstr> {
  let AddedComplexity = 20;
}

class SWXC1_FT<string opstr, RegisterOperand DRC,
               InstrItinClass Itin, SDPatternOperator OpNode = null_frag> :
  InstSE<(outs), (ins DRC:$fd, PtrRC:$base, PtrRC:$index),
         !strconcat(opstr, "\t$fd, ${index}(${base})"),
         [(OpNode DRC:$fd, (add iPTR:$base, iPTR:$index))], Itin,
         FrmPL26i, opstr> {
  let AddedComplexity = 20;
}

class BC1F_FT<string opstr, DAGOperand opnd, InstrItinClass Itin,
              SDPatternOperator Op = null_frag, bit DelaySlot = 1> :
  InstSE<(outs), (ins opnd:$offset),
         !strconcat(opstr, "\t$offset"),
         [(MipsFPBrcond Op, bb:$offset)], Itin,
         FrmPL16, opstr> {
  let isBranch = 1;
  let isTerminator = 1;
  let hasDelaySlot = 0;
  let Uses = [FCC0];
  let Defs = [AT];
}

class CEQS_FT<string typestr, RegisterClass RC, InstrItinClass Itin,
              SDPatternOperator OpNode = null_frag>  :
  InstSE<(outs), (ins RC:$fs, RC:$ft, condcode:$cond),
         !strconcat("c.$cond.", typestr, "\t$fs, $ft"), //[(OpNode RC:$fs, RC:$ft, imm:$cond)],
         [], Itin, FrmPL12,
         !strconcat("c.$cond.", typestr)> {
  let Defs = [FCC0];
  let isCodeGenOnly = 1;
}

class C_COND_FT<string CondStr, string Typestr, RegisterOperand RC,
                PatLeaf cond_type, InstrItinClass itin>  :
   InstSE<(outs), (ins RC:$fs, RC:$ft),
          !strconcat("c.", CondStr, ".", Typestr, "\t$fs, $ft"),
          [(MipsFPCmp RC:$fs, RC:$ft, cond_type)], itin,
          FrmPL12> {
  let Defs = [FCC0];
}

//#def C_F_S : C_COND_FT<"f", TypeStr, RC, itin>, C_COND_FM<fmt, 0>;
def C_UN_S : C_COND_FT<"un", "s", FGR32Opnd, MIPS_FCOND_UN, II_C_CC_S>, CEQS_FM<1, 4>;
def C_UN_D32 : C_COND_FT<"un", "d", AFGR64Opnd, MIPS_FCOND_UN, II_C_CC_D>, CEQS_FM<1, 3>;
def C_EQ_S : C_COND_FT<"eq", "s", FGR32Opnd, MIPS_FCOND_OEQ, II_C_CC_S>, CEQS_FM<0, 8>;
def C_EQ_D32 : C_COND_FT<"eq", "d", AFGR64Opnd, MIPS_FCOND_OEQ, II_C_CC_D>, CEQS_FM<0, 7>;
def C_UEQ_S : C_COND_FT<"ueq", "s", FGR32Opnd, MIPS_FCOND_UEQ, II_C_CC_S>, CEQS_FM<0, 0xe>;
def C_UEQ_D32 : C_COND_FT<"ueq", "d", AFGR64Opnd, MIPS_FCOND_UEQ, II_C_CC_D>, CEQS_FM<0, 0xd>;
def C_OLT_S : C_COND_FT<"olt", "s", FGR32Opnd, MIPS_FCOND_OLT, II_C_CC_S>, CEQS_FM<0, 0xc>;
def C_OLT_D32 : C_COND_FT<"olt", "d", AFGR64Opnd, MIPS_FCOND_OLT, II_C_CC_D>, CEQS_FM<0, 0xb>;
def C_ULT_S : C_COND_FT<"ult", "s", FGR32Opnd, MIPS_FCOND_ULT, II_C_CC_S>, CEQS_FM<1, 2>;
def C_ULT_D32 : C_COND_FT<"ult", "d", AFGR64Opnd, MIPS_FCOND_ULT, II_C_CC_D>, CEQS_FM<1, 1>;
def C_OLE_S : C_COND_FT<"ole", "s", FGR32Opnd, MIPS_FCOND_OLE, II_C_CC_S>, CEQS_FM<0, 0xa>;
def C_OLE_D32 : C_COND_FT<"ole", "d", AFGR64Opnd, MIPS_FCOND_OLE, II_C_CC_D>, CEQS_FM<0, 0x9>;
def C_ULE_S : C_COND_FT<"ule", "s", FGR32Opnd, MIPS_FCOND_ULE, II_C_CC_S>, CEQS_FM<1, 0>;
def C_ULE_D32 : C_COND_FT<"ule", "d", AFGR64Opnd, MIPS_FCOND_ULE, II_C_CC_D>, CEQS_FM<0, 0xf>;

//===----------------------------------------------------------------------===//
// Floating Point Instructions
//===----------------------------------------------------------------------===//
def ROUND_W_S  : MMRel, ABSS_FT<"round.w.s", FGR32Opnd, FGR32Opnd, II_ROUND>,
                 ABSS_FM<2, 0xc>;
def TRUNC_W_S  : MMRel, ABSS_FT<"trunc.w.s", FGR32Opnd, FGR32Opnd, II_TRUNC>,
                 ABSS_FM<2, 0>;
def CEIL_W_S   : MMRel, ABSS_FT<"ceil.w.s", FGR32Opnd, FGR32Opnd, II_CEIL>,
                 ABSS_FM<2, 0xe>;
def FLOOR_W_S  : MMRel, ABSS_FT<"floor.w.s", FGR32Opnd, FGR32Opnd, II_FLOOR>,
                 ABSS_FM<3, 0>;

def ROUND_W_D32 : MMRel, ABSS_FT<"round.w.d", FGR32Opnd, AFGR64Opnd, II_ROUND>,
             AdditionalRequires<[NotFP64bit]>, ABSS_FM<2,0xb>;
def TRUNC_W_D32 : MMRel, ABSS_FT<"trunc.w.d", FGR32Opnd, AFGR64Opnd, II_TRUNC>,
             AdditionalRequires<[NotFP64bit]>, ABSS_FM<1,0xf>;
def CEIL_W_D32 : MMRel, ABSS_FT<"ceil.w.d", FGR32Opnd, AFGR64Opnd, II_CEIL>,
             AdditionalRequires<[NotFP64bit]>, ABSS_FM<2,0xd>;
def FLOOR_W_D32 : MMRel, ABSS_FT<"floor.w.d", FGR32Opnd, AFGR64Opnd, II_FLOOR>,
             AdditionalRequires<[NotFP64bit]>, ABSS_FM<2,0xf>;


def CVT_S_W : MMRel, ABSS_FT<"cvt.s.w", FGR32Opnd, FGR32Opnd, II_CVT>,
              ABSS_FM<1, 8>;

def CVT_S_D32 : MMRel, ABSS_FT<"cvt.s.d", FGR32Opnd, AFGR64Opnd, II_CVT>,
                ABSS_FM<1, 5>, FGR_32;
def CVT_D32_W : MMRel, ABSS_FT<"cvt.d.w", AFGR64Opnd, FGR32Opnd, II_CVT>,
                ABSS_FM<1, 7>, FGR_32;
def CVT_D32_S : MMRel, ABSS_FT<"cvt.d.s", AFGR64Opnd, FGR32Opnd, II_CVT>,
                ABSS_FM<1, 6>, FGR_32;

def FABS_S : MMRel, ABSS_FT<"abs.s", FGR32Opnd, FGR32Opnd, II_ABS, fabs>,
             ABSS_FM<0, 6>;
def FNEG_S : MMRel, ABSS_FT<"neg.s", FGR32Opnd, FGR32Opnd, II_NEG, fneg>,
             ABSS_FM<1, 0xe>;
defm FABS : ABSS_M<"abs.d", II_ABS, fabs>, ABSS_FM<0, 0x5>;
defm FNEG : ABSS_M<"neg.d", II_NEG, fneg>, ABSS_FM<1, 0xd>;

def FSQRT_S : MMRel, ABSS_FT<"sqrt.s", FGR32Opnd, FGR32Opnd, II_SQRT_S, fsqrt>,
              ABSS_FM<2, 2>;
defm FSQRT : ABSS_M<"sqrt.d", II_SQRT_D, fsqrt>, ABSS_FM<2, 1>;

// The odd-numbered registers are only referenced when doing loads,
// stores, and moves between floating-point and integer registers.
// When defining instructions, we reference all 32-bit registers,
// regardless of register aliasing.

/// Move Control Registers From/To CPU Registers
def MFC1 : MMRel, MFC1_FT<"mfc1", GPR32Opnd, FGR32Opnd, II_MFC1,
                          bitconvert>, MFC1_FM<1, 0x9>;
def MTC1 : MMRel, MTC1_FT<"mtc1", FGR32Opnd, GPR32Opnd, II_MTC1,
                          bitconvert>, MFC1_FM<1, 0xc>;
def MFHC1_D32 : MMRel, MFC1_FT<"mfhc1", GPR32Opnd, AFGR64Opnd, II_MFHC1>,
                MFC1_FM<2, 9>, AdditionalRequires<[NotFP64bit]>;
def MFLC1_D32 : MMRel, MFC1_FT<"mflc1", GPR32Opnd, AFGR64Opnd, II_MFHC1>,
                MFC1_FM<3, 1>, AdditionalRequires<[NotFP64bit]>;
def MTHC1_D32 : MMRel, MTC1_64_FT<"mthc1", AFGR64Opnd, GPR32Opnd, II_MTHC1>,
                MFC1_FM<2, 0xa>, AdditionalRequires<[NotFP64bit]>;
def MTLC1_D32 : MMRel, MTC1_64_FT<"mtlc1", AFGR64Opnd, GPR32Opnd, II_MTHC1>,
                MFC1_FM<3, 0x2>, AdditionalRequires<[NotFP64bit]>;

def FMOV_S   : MMRel, ABSS_FT<"mov.s", FGR32Opnd, FGR32Opnd, II_MOV_S>,
               ABSS_FM<1, 0xb>;
def FMOV_D32 : MMRel, ABSS_FT<"mov.d", AFGR64Opnd, AFGR64Opnd, II_MOV_D>,
               ABSS_FM<1, 0xa>, AdditionalRequires<[NotFP64bit]>;

/// Floating Point Memory Instructions
def LWC1 : MMRel, LW_FT<"lwc1", FGR32Opnd, II_LWC1, load>, LW_FM<0x1c>;
def SWC1 : MMRel, SW_FT<"swc1", FGR32Opnd, II_SWC1, store>, LW_FM<0x1b>;

def LDC1 : MMRel, LW_FT<"ldc1", AFGR64Opnd, II_LDC1, load>, LW_FM<0x1a>,
           FGR_32;
def SDC1 : MMRel, SW_FT<"sdc1", AFGR64Opnd, II_SDC1, store>, LW_FM<0x19>,
           FGR_32;

// Indexed loads and stores.
// Base register + offset register addressing mode (indicated by "x" in the
// instruction mnemonic) is disallowed under NaCl.
let AdditionalPredicates = [IsNotNaCl] in {
  def LWXC1 : MMRel, LWXC1_FT<"lwxc1", FGR32Opnd, II_LWXC1, load>, LWXC1_FM<8, 2>;
  def SWXC1 : MMRel, SWXC1_FT<"swxc1", FGR32Opnd, II_SWXC1, store>, LWXC1_FM<8, 3>;
}

let AdditionalPredicates = [NotInMicroMips, IsNotNaCl] in {
  def LDXC1 : LWXC1_FT<"ldxc1", AFGR64Opnd, II_LDXC1, load>, LWXC1_FM<8, 0>;
  def SDXC1 : SWXC1_FT<"sdxc1", AFGR64Opnd, II_SDXC1, store>, LWXC1_FM<8, 1>;

}

/// Floating-point Aritmetic
def FADD_S : MMRel, ADDS_FT<"add.s", FGR32Opnd, II_ADD_S, 1, fadd>,
             ADDS_FM<5, 1>;
defm FADD :  ADDS_M<"add.d", II_ADD_D, 1, fadd>, ADDS_FM<5, 0>;
def FDIV_S : MMRel, ADDS_FT<"div.s", FGR32Opnd, II_DIV_S, 0, fdiv>,
             ADDS_FM<5, 3>;
defm FDIV :  ADDS_M<"div.d", II_DIV_D, 0, fdiv>, ADDS_FM<5, 2>;
def FMUL_S : MMRel, ADDS_FT<"mul.s", FGR32Opnd, II_MUL_S, 1, fmul>,
             ADDS_FM<6, 1>;
defm FMUL :  ADDS_M<"mul.d", II_MUL_D, 1, fmul>, ADDS_FM<6, 0>;
def FSUB_S : MMRel, ADDS_FT<"sub.s", FGR32Opnd, II_SUB_S, 0, fsub>,
             ADDS_FM<6, 3>;
defm FSUB :  ADDS_M<"sub.d", II_SUB_D, 0, fsub>, ADDS_FM<6, 2>;

def MADD_S : MMRel, MADDS_FT<"madd.s", FGR32Opnd, II_MADD_S, fadd>,
             MADDS_FM<1>;
def MSUB_S : MMRel, MADDS_FT<"msub.s", FGR32Opnd, II_MSUB_S, fsub>,
             MADDS_FM<3>;

def MADD_D32 : MMRel, MADDS_FT<"madd.d", AFGR64Opnd, II_MADD_D, fadd>,
               MADDS_FM<0>, FGR_32;
def MSUB_D32 : MMRel, MADDS_FT<"msub.d", AFGR64Opnd, II_MSUB_D, fsub>,
               MADDS_FM<2>, FGR_32;

//===----------------------------------------------------------------------===//
// Floating Point Branch Codes
//===----------------------------------------------------------------------===//
// Mips branch codes. These correspond to condcode in MipsInstrInfo.h.
// They must be kept in synch.
def MIPS_BRANCH_F  : PatLeaf<(i32 0)>;
def MIPS_BRANCH_T  : PatLeaf<(i32 1)>;

def BC1F : MMRel, BC1F_FT<"bc1f", br16target, IIBranch, MIPS_BRANCH_F>,
           BC1F_FM<2>;
def BC1FL : MMRel, BC1F_FT<"bc1fl", br16target, IIBranch, MIPS_BRANCH_F, 0>,
            BC1F_FM<3>;
def BC1T : MMRel, BC1F_FT<"bc1t", br16target, IIBranch, MIPS_BRANCH_T>,
           BC1F_FM<0>;
def BC1TL : MMRel, BC1F_FT<"bc1tl", br16target, IIBranch, MIPS_BRANCH_T, 0>,
            BC1F_FM<1>;

//===----------------------------------------------------------------------===//
// Floating Point Pseudo-Instructions
//===----------------------------------------------------------------------===//

// This pseudo instr gets expanded into 2 mtc1 instrs after register
// allocation.
class BuildPairF64Base<RegisterOperand RO> :
  PseudoSE<(outs RO:$dst), (ins GPR32Opnd:$lo, GPR32Opnd:$hi),
           [(set RO:$dst, (MipsBuildPairF64 GPR32Opnd:$lo, GPR32Opnd:$hi))]>;

def BuildPairF64 : BuildPairF64Base<AFGR64Opnd>,
                   AdditionalRequires<[NotFP64bit]>;

// This pseudo instr gets expanded into 2 mfc1 instrs after register
// allocation.
// if n is 0, lower part of src is extracted.
// if n is 1, higher part of src is extracted.
class ExtractElementF64Base<RegisterOperand RO> :
  PseudoSE<(outs GPR32Opnd:$dst), (ins RO:$src, i32imm:$n),
           [(set GPR32Opnd:$dst, (MipsExtractElementF64 RO:$src, imm:$n))]>;

def ExtractElementF64 : ExtractElementF64Base<AFGR64Opnd>,
                        AdditionalRequires<[NotFP64bit]>;

//===----------------------------------------------------------------------===//
// InstAliases.
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Floating Point Patterns
//===----------------------------------------------------------------------===//
def : MipsPat<(f32 fpimm0), (MTC1 ZERO)>;
def : MipsPat<(f32 fpimm0neg), (FNEG_S (MTC1 ZERO))>;

def : MipsPat<(f32 (sint_to_fp GPR32Opnd:$src)),
              (CVT_S_W (MTC1 GPR32Opnd:$src))>;
def : MipsPat<(MipsTruncIntFP FGR32Opnd:$src),
              (TRUNC_W_S FGR32Opnd:$src)>;

def : MipsPat<(f64 (sint_to_fp GPR32Opnd:$src)),
              (CVT_D32_W (MTC1 GPR32Opnd:$src))>, FGR_32;
def : MipsPat<(MipsTruncIntFP AFGR64Opnd:$src),
              (TRUNC_W_D32 AFGR64Opnd:$src)>, FGR_32;
def : MipsPat<(f32 (fround AFGR64Opnd:$src)),
              (CVT_S_D32 AFGR64Opnd:$src)>, FGR_32;
def : MipsPat<(f64 (fextend FGR32Opnd:$src)),
              (CVT_D32_S FGR32Opnd:$src)>, FGR_32;

// Patterns for loads/stores with a reg+imm operand.
let AddedComplexity = 40 in {
  def : LoadRegImmPat<LWC1, f32, load>;
  def : StoreRegImmPat<SWC1, f32>;

  def : LoadRegImmPat<LDC1, f64, load>, FGR_32;
  def : StoreRegImmPat<SDC1, f64>, FGR_32;
}
