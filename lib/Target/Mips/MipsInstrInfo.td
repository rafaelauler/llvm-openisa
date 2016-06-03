//===- MipsInstrInfo.td - Target Description for Mips Target -*- tablegen -*-=//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the Mips implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// Mips profiles and nodes
//===----------------------------------------------------------------------===//

def SDT_MipsJmpLink      : SDTypeProfile<0, 2, [SDTCisVT<0, iPTR>, SDTCisVT<1,i32>]>;
def SDT_MipsTailCall     :  SDTypeProfile<0, 1, [SDTCisVT<0, iPTR>]>;
def SDT_MipsCMov         : SDTypeProfile<1, 4, [SDTCisSameAs<0, 1>,
                                                SDTCisSameAs<1, 2>,
                                                SDTCisSameAs<3, 4>,
                                                SDTCisInt<4>]>;
def SDT_MipsCallSeqStart : SDCallSeqStart<[SDTCisVT<0, i32>]>;
def SDT_MipsCallSeqEnd   : SDCallSeqEnd<[SDTCisVT<0, i32>, SDTCisVT<1, i32>]>;
def SDT_MFLOHI : SDTypeProfile<1, 1, [SDTCisInt<0>, SDTCisVT<1, untyped>]>;
def SDT_MTLOHI : SDTypeProfile<1, 2, [SDTCisVT<0, untyped>,
                                      SDTCisInt<1>, SDTCisSameAs<1, 2>]>;
def SDT_MipsMultDiv : SDTypeProfile<1, 2, [SDTCisVT<0, untyped>, SDTCisInt<1>,
                                    SDTCisSameAs<1, 2>]>;
def SDT_MipsMAddMSub : SDTypeProfile<1, 3,
                                     [SDTCisVT<0, untyped>, SDTCisSameAs<0, 3>,
                                      SDTCisVT<1, i32>, SDTCisSameAs<1, 2>]>;
def SDT_MipsDivRem16 : SDTypeProfile<0, 2, [SDTCisInt<0>, SDTCisSameAs<0, 1>]>;

def SDT_MipsThreadPointer : SDTypeProfile<1, 0, [SDTCisPtrTy<0>]>;

def SDT_Sync             : SDTypeProfile<0, 1, [SDTCisVT<0, i32>]>;

def SDT_Ext : SDTypeProfile<1, 3, [SDTCisInt<0>, SDTCisSameAs<0, 1>,
                                   SDTCisVT<2, i32>, SDTCisSameAs<2, 3>]>;
def SDT_Ins : SDTypeProfile<1, 4, [SDTCisInt<0>, SDTCisSameAs<0, 1>,
                                   SDTCisVT<2, i32>, SDTCisSameAs<2, 3>,
                                   SDTCisSameAs<0, 4>]>;

def SDTMipsLoadLR  : SDTypeProfile<1, 2,
                                   [SDTCisInt<0>, SDTCisPtrTy<1>,
                                    SDTCisSameAs<0, 2>]>;

// Call
def MipsJmpLink : SDNode<"MipsISD::JmpLink",SDT_MipsJmpLink,
                         [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue,
                          SDNPVariadic]>;

// Tail call
def MipsTailCall : SDNode<"MipsISD::TailCall", SDT_MipsTailCall,
                          [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

// GetImm nodes are used to handle global addresses. Used on
// MipsISelLowering to lower stuff like GlobalAddress, ExternalSymbol
// static model.
def MipsGetImm    : SDNode<"MipsISD::GetImm", SDTIntUnaryOp>;
def MipsGPRel : SDNode<"MipsISD::GPRel", SDTIntUnaryOp>;

// TlsGd node is used to handle General Dynamic TLS
def MipsTlsGd : SDNode<"MipsISD::TlsGd", SDTIntUnaryOp>;

// TprelHi and TprelLo nodes are used to handle Local Exec TLS
def MipsTprelHi    : SDNode<"MipsISD::TprelHi", SDTIntUnaryOp>;
def MipsTprelLo    : SDNode<"MipsISD::TprelLo", SDTIntUnaryOp>;

// Thread pointer
def MipsThreadPointer: SDNode<"MipsISD::ThreadPointer", SDT_MipsThreadPointer>;

// Return
def MipsRet : SDNode<"MipsISD::Ret", SDTNone,
                     [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

// These are target-independent nodes, but have target-specific formats.
def callseq_start : SDNode<"ISD::CALLSEQ_START", SDT_MipsCallSeqStart,
                           [SDNPHasChain, SDNPSideEffect, SDNPOutGlue]>;
def callseq_end   : SDNode<"ISD::CALLSEQ_END", SDT_MipsCallSeqEnd,
                           [SDNPHasChain, SDNPSideEffect,
                            SDNPOptInGlue, SDNPOutGlue]>;

// Nodes used to extract LO/HI registers.
def MipsMFHI : SDNode<"MipsISD::MFHI", SDT_MFLOHI>;
def MipsMFLO : SDNode<"MipsISD::MFLO", SDT_MFLOHI>;

// Node used to insert 32-bit integers to LOHI register pair.
def MipsMTLOHI : SDNode<"MipsISD::MTLOHI", SDT_MTLOHI>;

// Mult nodes.
def MipsMult  : SDNode<"MipsISD::Mult", SDT_MipsMultDiv>;
def MipsMultu : SDNode<"MipsISD::Multu", SDT_MipsMultDiv>;

// MAdd*/MSub* nodes
def MipsMAdd  : SDNode<"MipsISD::MAdd", SDT_MipsMAddMSub>;
def MipsMAddu : SDNode<"MipsISD::MAddu", SDT_MipsMAddMSub>;
def MipsMSub  : SDNode<"MipsISD::MSub", SDT_MipsMAddMSub>;
def MipsMSubu : SDNode<"MipsISD::MSubu", SDT_MipsMAddMSub>;

// DivRem(u) nodes
def MipsDivRem    : SDNode<"MipsISD::DivRem", SDT_MipsMultDiv>;
def MipsDivRemU   : SDNode<"MipsISD::DivRemU", SDT_MipsMultDiv>;
def MipsDivRem16  : SDNode<"MipsISD::DivRem16", SDT_MipsDivRem16,
                           [SDNPOutGlue]>;
def MipsDivRemU16 : SDNode<"MipsISD::DivRemU16", SDT_MipsDivRem16,
                           [SDNPOutGlue]>;

// Target constant nodes that are not part of any isel patterns and remain
// unchanged can cause instructions with illegal operands to be emitted.
// Wrapper node patterns give the instruction selector a chance to replace
// target constant nodes that would otherwise remain unchanged with ADDiu
// nodes. Without these wrapper node patterns, the following conditional move
// instruction is emitted when function cmov2 in test/CodeGen/Mips/cmov.ll is
// compiled:
//  movn  %got(d)($gp), %got(c)($gp), $4
// This instruction is illegal since movn can take only register operands.

def MipsWrapper    : SDNode<"MipsISD::Wrapper", SDTIntBinOp>;

def MipsSync : SDNode<"MipsISD::Sync", SDT_Sync, [SDNPHasChain,SDNPSideEffect]>;

def MipsExt :  SDNode<"MipsISD::Ext", SDT_Ext>;
def MipsIns :  SDNode<"MipsISD::Ins", SDT_Ins>;

def MipsLWL : SDNode<"MipsISD::LWL", SDTMipsLoadLR,
                     [SDNPHasChain, SDNPMayLoad, SDNPMemOperand]>;
def MipsLWR : SDNode<"MipsISD::LWR", SDTMipsLoadLR,
                     [SDNPHasChain, SDNPMayLoad, SDNPMemOperand]>;
def MipsSWL : SDNode<"MipsISD::SWL", SDTStore,
                     [SDNPHasChain, SDNPMayStore, SDNPMemOperand]>;
def MipsSWR : SDNode<"MipsISD::SWR", SDTStore,
                     [SDNPHasChain, SDNPMayStore, SDNPMemOperand]>;
def MipsLDL : SDNode<"MipsISD::LDL", SDTMipsLoadLR,
                     [SDNPHasChain, SDNPMayLoad, SDNPMemOperand]>;
def MipsLDR : SDNode<"MipsISD::LDR", SDTMipsLoadLR,
                     [SDNPHasChain, SDNPMayLoad, SDNPMemOperand]>;
def MipsSDL : SDNode<"MipsISD::SDL", SDTStore,
                     [SDNPHasChain, SDNPMayStore, SDNPMemOperand]>;
def MipsSDR : SDNode<"MipsISD::SDR", SDTStore,
                     [SDNPHasChain, SDNPMayStore, SDNPMemOperand]>;

//===----------------------------------------------------------------------===//
// Mips Instruction Predicate Definitions.
//===----------------------------------------------------------------------===//
def HasMips2     :    Predicate<"Subtarget->hasMips2()">,
                      AssemblerPredicate<"FeatureMips2">;
def HasMips3_32  :    Predicate<"Subtarget->hasMips3_32()">,
                      AssemblerPredicate<"FeatureMips3_32">;
def HasMips3_32r2 :   Predicate<"Subtarget->hasMips3_32r2()">,
                      AssemblerPredicate<"FeatureMips3_32r2">;
def HasMips3     :    Predicate<"Subtarget->hasMips3()">,
                      AssemblerPredicate<"FeatureMips3">;
def HasMips4_32  :    Predicate<"Subtarget->hasMips4_32()">,
                      AssemblerPredicate<"FeatureMips4_32">;
def NotMips4_32  :    Predicate<"!Subtarget->hasMips4_32()">,
                      AssemblerPredicate<"FeatureMips4_32">;
def HasMips4_32r2 :   Predicate<"Subtarget->hasMips4_32r2()">,
                      AssemblerPredicate<"FeatureMips4_32r2">;
def HasMips5_32r2 :   Predicate<"Subtarget->hasMips5_32r2()">,
                      AssemblerPredicate<"FeatureMips5_32r2">;
def HasMips32    :    Predicate<"Subtarget->hasMips32()">,
                      AssemblerPredicate<"FeatureMips32">;
def HasMips32r2  :    Predicate<"Subtarget->hasMips32r2()">,
                      AssemblerPredicate<"FeatureMips32r2">;
def HasMips32r6  :    Predicate<"Subtarget->hasMips32r6()">,
                      AssemblerPredicate<"FeatureMips32r6">;
def NotMips32r6  :    Predicate<"!Subtarget->hasMips32r6()">,
                      AssemblerPredicate<"!FeatureMips32r6">;
def IsGP64bit    :    Predicate<"Subtarget->isGP64bit()">,
                      AssemblerPredicate<"FeatureGP64Bit">;
def IsGP32bit    :    Predicate<"!Subtarget->isGP64bit()">,
                      AssemblerPredicate<"!FeatureGP64Bit">;
def HasMips64    :    Predicate<"Subtarget->hasMips64()">,
                      AssemblerPredicate<"FeatureMips64">;
def HasMips64r2  :    Predicate<"Subtarget->hasMips64r2()">,
                      AssemblerPredicate<"FeatureMips64r2">;
def HasMips64r6  :    Predicate<"Subtarget->hasMips64r6()">,
                      AssemblerPredicate<"FeatureMips64r6">;
def NotMips64r6  :    Predicate<"!Subtarget->hasMips64r6()">,
                      AssemblerPredicate<"!FeatureMips64r6">;
def InMips16Mode :    Predicate<"Subtarget->inMips16Mode()">,
                      AssemblerPredicate<"FeatureMips16">;
def HasCnMips    :    Predicate<"Subtarget->hasCnMips()">,
                      AssemblerPredicate<"FeatureCnMips">;
def RelocStatic :     Predicate<"TM.getRelocationModel() == Reloc::Static">,
                      AssemblerPredicate<"FeatureMips32">;
def RelocPIC    :     Predicate<"TM.getRelocationModel() == Reloc::PIC_">,
                      AssemblerPredicate<"FeatureMips32">;
def NoNaNsFPMath :    Predicate<"TM.Options.NoNaNsFPMath">;
def HasStdEnc :       Predicate<"Subtarget->hasStandardEncoding()">,
                      AssemblerPredicate<"!FeatureMips16">;
def NotDSP :          Predicate<"!Subtarget->hasDSP()">;
def InMicroMips    :  Predicate<"Subtarget->inMicroMipsMode()">,
                      AssemblerPredicate<"FeatureMicroMips">;
def NotInMicroMips :  Predicate<"!Subtarget->inMicroMipsMode()">,
                      AssemblerPredicate<"!FeatureMicroMips">;
def IsLE           :  Predicate<"Subtarget->isLittle()">;
def IsBE           :  Predicate<"!Subtarget->isLittle()">;
def IsNotNaCl    :    Predicate<"!Subtarget->isTargetNaCl()">;

//===----------------------------------------------------------------------===//
// Mips GPR size adjectives.
// They are mutually exclusive.
//===----------------------------------------------------------------------===//

class GPR_32 { list<Predicate> GPRPredicates = [IsGP32bit]; }
class GPR_64 { list<Predicate> GPRPredicates = [IsGP64bit]; }

//===----------------------------------------------------------------------===//
// Mips ISA/ASE membership and instruction group membership adjectives.
// They are mutually exclusive.
//===----------------------------------------------------------------------===//

// FIXME: I'd prefer to use additive predicates to build the instruction sets
//        but we are short on assembler feature bits at the moment. Using a
//        subtractive predicate will hopefully keep us under the 32 predicate
//        limit long enough to develop an alternative way to handle P1||P2
//        predicates.
class ISA_MIPS1_NOT_4_32 {
  list<Predicate> InsnPredicates = [NotMips4_32];
}
class ISA_MIPS1_NOT_32R6_64R6 {
  list<Predicate> InsnPredicates = [NotMips32r6, NotMips64r6];
}
class ISA_MIPS2    { list<Predicate> InsnPredicates = [HasMips2]; }
class ISA_MIPS2_NOT_32R6_64R6 {
  list<Predicate> InsnPredicates = [HasMips2, NotMips32r6, NotMips64r6];
}
class ISA_MIPS3    { list<Predicate> InsnPredicates = [HasMips3]; }
class ISA_MIPS3_NOT_32R6_64R6 {
  list<Predicate> InsnPredicates = [HasMips3, NotMips32r6, NotMips64r6];
}
class ISA_MIPS32   { list<Predicate> InsnPredicates = [HasMips32]; }
class ISA_MIPS32_NOT_32R6_64R6 {
  list<Predicate> InsnPredicates = [HasMips32, NotMips32r6, NotMips64r6];
}
class ISA_MIPS32R2 { list<Predicate> InsnPredicates = [HasMips32r2]; }
class ISA_MIPS32R2_NOT_32R6_64R6 {
  list<Predicate> InsnPredicates = [HasMips32r2, NotMips32r6, NotMips64r6];
}
class ISA_MIPS64   { list<Predicate> InsnPredicates = [HasMips64]; }
class ISA_MIPS64_NOT_64R6 {
  list<Predicate> InsnPredicates = [HasMips64, NotMips64r6];
}
class ISA_MIPS64R2 { list<Predicate> InsnPredicates = [HasMips64r2]; }
class ISA_MIPS32R6 { list<Predicate> InsnPredicates = [HasMips32r6]; }
class ISA_MIPS64R6 { list<Predicate> InsnPredicates = [HasMips64r6]; }

// The portions of MIPS-III that were also added to MIPS32
class INSN_MIPS3_32 { list<Predicate> InsnPredicates = [HasMips3_32]; }

// The portions of MIPS-III that were also added to MIPS32 but were removed in
// MIPS32r6 and MIPS64r6.
class INSN_MIPS3_32_NOT_32R6_64R6 {
  list<Predicate> InsnPredicates = [HasMips3_32, NotMips32r6, NotMips64r6];
}

// The portions of MIPS-III that were also added to MIPS32
class INSN_MIPS3_32R2 { list<Predicate> InsnPredicates = [HasMips3_32r2]; }

// The portions of MIPS-IV that were also added to MIPS32 but were removed in
// MIPS32r6 and MIPS64r6.
class INSN_MIPS4_32_NOT_32R6_64R6 {
  list<Predicate> InsnPredicates = [HasMips4_32, NotMips32r6, NotMips64r6];
}

// The portions of MIPS-IV that were also added to MIPS32r2 but were removed in
// MIPS32r6 and MIPS64r6.
class INSN_MIPS4_32R2_NOT_32R6_64R6 {
  list<Predicate> InsnPredicates = [HasMips4_32r2, NotMips32r6, NotMips64r6];
}

// The portions of MIPS-V that were also added to MIPS32r2 but were removed in
// MIPS32r6 and MIPS64r6.
class INSN_MIPS5_32R2_NOT_32R6_64R6 {
  list<Predicate> InsnPredicates = [HasMips5_32r2, NotMips32r6, NotMips64r6];
}

//===----------------------------------------------------------------------===//

class MipsPat<dag pattern, dag result> : Pat<pattern, result>, PredicateControl {
  let EncodingPredicates = [HasStdEnc];
}

class MipsInstAlias<string Asm, dag Result, bit Emit = 0b1> :
  InstAlias<Asm, Result, Emit>, PredicateControl;

class IsCommutable {
  bit isCommutable = 1;
}

class IsBranch {
  bit isBranch = 1;
}

class IsReturn {
  bit isReturn = 1;
}

class IsCall {
  bit isCall = 1;
}

class IsTailCall {
  bit isCall = 1;
  bit isTerminator = 1;
  bit isReturn = 1;
  bit isBarrier = 1;
  bit hasExtraSrcRegAllocReq = 1;
  bit isCodeGenOnly = 1;
}

class IsAsCheapAsAMove {
  bit isAsCheapAsAMove = 1;
}

class NeverHasSideEffects {
  bit hasSideEffects = 0;
}

//===----------------------------------------------------------------------===//
// Instruction format superclass
//===----------------------------------------------------------------------===//

include "MipsInstrFormats.td"

//===----------------------------------------------------------------------===//
// Mips Operand, Complex Patterns and Transformations Definitions.
//===----------------------------------------------------------------------===//

def MipsJumpTargetAsmOperand : AsmOperandClass {
  let Name = "JumpTarget";
  let ParserMethod = "parseJumpTarget";
  let PredicateMethod = "isImm";
  let RenderMethod = "addImmOperands";
}

// Instruction operand types
def jmptarget   : Operand<OtherVT> {
  let EncoderMethod = "getJumpTargetOpValue";
  let ParserMatchClass = MipsJumpTargetAsmOperand;
}
def brtarget    : Operand<OtherVT> {
  let EncoderMethod = "getBranchTargetOpValue";
  let OperandType = "OPERAND_PCREL";
  let DecoderMethod = "DecodeBranchTarget";
  let ParserMatchClass = MipsJumpTargetAsmOperand;
}
def br16target    : Operand<OtherVT> {
  let EncoderMethod = "getBranch16TargetOpValue";
  let OperandType = "OPERAND_PCREL";
  let DecoderMethod = "DecodeBranch16Target";
  let ParserMatchClass = MipsJumpTargetAsmOperand;
}
def br14target    : Operand<OtherVT> {
  let EncoderMethod = "getBranch14TargetOpValue";
  let OperandType = "OPERAND_PCREL";
  let DecoderMethod = "DecodeBranchTarget";
  let ParserMatchClass = MipsJumpTargetAsmOperand;
}
def calltarget  : Operand<iPTR> {
  let EncoderMethod = "getJumpTargetOpValue";
  let ParserMatchClass = MipsJumpTargetAsmOperand;
  let DecoderMethod = "DecodeCallTarget";
}

def simm9 : Operand<i32>;
def simm10 : Operand<i32>;
def simm11 : Operand<i32>;

def simm16      : Operand<i32> {
  let DecoderMethod= "DecodeSimm16";
}

def simm19_lsl2 : Operand<i32> {
  let EncoderMethod = "getSimm19Lsl2Encoding";
  let DecoderMethod = "DecodeSimm19Lsl2";
  let ParserMatchClass = MipsJumpTargetAsmOperand;
}

def simm18_lsl3 : Operand<i32> {
  let EncoderMethod = "getSimm18Lsl3Encoding";
  let DecoderMethod = "DecodeSimm18Lsl3";
  let ParserMatchClass = MipsJumpTargetAsmOperand;
}

def simm14      : Operand<i32> {
  let DecoderMethod = "DecodeSimm14PL26i";
}

def simm14pl20  : Operand<i32> {
  let DecoderMethod = "DecodeSimm14PL20";
}

def simm20      : Operand<i32> {
}

def uimm20      : Operand<i32> {
}

def uimm10      : Operand<i32> {
}

def uimm14      : Operand<i32> {
}

def uimm18      : Operand<i32> {
}

def simm16_64   : Operand<i64> {
  let DecoderMethod = "DecodeSimm16";
}

// Zero
def uimmz       : Operand<i32> {
  let PrintMethod = "printUnsignedImm";
}

// Unsigned Operand
def uimm2 : Operand<i32> {
  let PrintMethod = "printUnsignedImm";
}

def uimm3 : Operand<i32> {
  let PrintMethod = "printUnsignedImm";
}

def uimm5       : Operand<i32> {
  let PrintMethod = "printUnsignedImm";
}

def uimm6 : Operand<i32> {
  let PrintMethod = "printUnsignedImm";
}

def uimm16      : Operand<i32> {
  let PrintMethod = "printUnsignedImm";
}

def pcrel16      : Operand<i32> {
}

def uimm32      : Operand<i32> {
  let PrintMethod = "printUnsignedImm";
}


def MipsMemAsmOperand : AsmOperandClass {
  let Name = "Mem";
  let ParserMethod = "parseMemOperand";
}

def MipsMemSimm11AsmOperand : AsmOperandClass {
  let Name = "MemOffsetSimm11";
  let SuperClasses = [MipsMemAsmOperand];
  let RenderMethod = "addMemOperands";
  let ParserMethod = "parseMemOperand";
  let PredicateMethod = "isMemWithSimmOffset<11>";
}

def MipsMemSimm16AsmOperand : AsmOperandClass {
  let Name = "MemOffsetSimm16";
  let SuperClasses = [MipsMemAsmOperand];
  let RenderMethod = "addMemOperands";
  let ParserMethod = "parseMemOperand";
  let PredicateMethod = "isMemWithSimmOffset<16>";
}

def MipsInvertedImmoperand : AsmOperandClass {
  let Name = "InvNum";
  let RenderMethod = "addImmOperands";
  let ParserMethod = "parseInvNum";
}

def InvertedImOperand : Operand<i32> {
  let ParserMatchClass = MipsInvertedImmoperand;
}

def InvertedImOperand64 : Operand<i64> {
  let ParserMatchClass = MipsInvertedImmoperand;
}

class mem_generic : Operand<iPTR> {
  let PrintMethod = "printMemOperand";
  let MIOperandInfo = (ops ptr_rc, simm14);
  let EncoderMethod = "getMemEncoding";
  let ParserMatchClass = MipsMemAsmOperand;
  let OperandType = "OPERAND_MEMORY";
}

// Address operand
def mem : mem_generic;

// MSA specific address operand
def mem_msa : mem_generic {
  let MIOperandInfo = (ops ptr_rc, simm10);
  let EncoderMethod = "getMSAMemEncoding";
}

def mem_simm9 : mem_generic {
  let MIOperandInfo = (ops ptr_rc, simm9);
  let EncoderMethod = "getMemEncoding";
}

def mem_simm11 : mem_generic {
  let MIOperandInfo = (ops ptr_rc, simm11);
  let EncoderMethod = "getMemEncoding";
  let ParserMatchClass = MipsMemSimm11AsmOperand;
}

def mem_simm16 : mem_generic {
  let MIOperandInfo = (ops ptr_rc, simm16);
  let EncoderMethod = "getMemEncoding";
  let ParserMatchClass = MipsMemSimm16AsmOperand;
}

def mem_ea : Operand<iPTR> {
  let PrintMethod = "printMemOperandEA";
  let MIOperandInfo = (ops ptr_rc, simm14);
  let EncoderMethod = "getMemEncoding";
  let OperandType = "OPERAND_MEMORY";
}

def PtrRC : Operand<iPTR> {
  let MIOperandInfo = (ops ptr_rc);
  let DecoderMethod = "DecodePtrRegisterClass";
  let ParserMatchClass = GPR32AsmOperand;
}

// size operand of ext instruction
def size_ext : Operand<i32> {
  let EncoderMethod = "getSizeExtEncoding";
  let DecoderMethod = "DecodeExtSize";
}

// size operand of ins instruction
def size_ins : Operand<i32> {
  let EncoderMethod = "getSizeInsEncoding";
  let DecoderMethod = "DecodeInsSize";
}

// Transformation Function - get the lower 16 bits.
def LO16 : SDNodeXForm<imm, [{
  return getImm(N, N->getZExtValue() & 0xFFFF);
}]>;
def LO14 : SDNodeXForm<imm, [{
  return getImm(N, N->getZExtValue() & 0x3FFF);
}]>;

// Transformation Function - get the higher 16 bits.
def HI16 : SDNodeXForm<imm, [{
  return getImm(N, (N->getZExtValue() >> 16) & 0xFFFF);
}]>;

// Plus 1.
def Plus1 : SDNodeXForm<imm, [{ return getImm(N, N->getSExtValue() + 1); }]>;

// Node immediate is zero (e.g. insve.d)
def immz : PatLeaf<(imm), [{ return N->getSExtValue() == 0; }]>;

// Node immediate fits as 16-bit sign extended on target immediate.
// e.g. addi, andi
def immSExt8  : PatLeaf<(imm), [{ return isInt<8>(N->getSExtValue()); }]>;

// Node immediate fits as 16-bit sign extended on target immediate.
// e.g. addi, andi
def immSExt16  : PatLeaf<(imm), [{ return isInt<16>(N->getSExtValue()); }]>;

def immSExt14  : PatLeaf<(imm), [{ return isInt<14>(N->getSExtValue()); }]>;

// Node immediate fits as 15-bit sign extended on target immediate.
// e.g. addi, andi
def immSExt15  : PatLeaf<(imm), [{ return isInt<15>(N->getSExtValue()); }]>;

// Node immediate fits as 16-bit zero extended on target immediate.
// The LO16 param means that only the lower 16 bits of the node
// immediate are caught.
// e.g. addiu, sltiu
def immZExt16  : PatLeaf<(imm), [{
  if (N->getValueType(0) == MVT::i32)
    return (uint32_t)N->getZExtValue() == (unsigned short)N->getZExtValue();
  else
    return (uint64_t)N->getZExtValue() == (unsigned short)N->getZExtValue();
}], LO16>;

// Immediate can be loaded with LUi (32-bit int with lower 16-bit cleared).
def immLow16Zero : PatLeaf<(imm), [{
  int64_t Val = N->getSExtValue();
  return isInt<32>(Val) && !(Val & 0xffff);
}]>;

// shamt field must fit in 5 bits.
def immZExt5 : ImmLeaf<i32, [{return Imm == (Imm & 0x1f);}]>;

def immZExt14 : PatLeaf<(imm), [{
  if (N->getValueType(0) == MVT::i32)
    return (uint32_t)N->getZExtValue() == (N->getZExtValue() & 0x3fff);
  else
    return (uint64_t)N->getZExtValue() == (N->getZExtValue() & 0x3fff);
}], LO14>;

// True if (N + 1) fits in 16-bit field.
def immSExt16Plus1 : PatLeaf<(imm), [{
  return isInt<17>(N->getSExtValue()) && isInt<16>(N->getSExtValue() + 1);
}]>;
def immSExt14Plus1 : PatLeaf<(imm), [{
  return isInt<15>(N->getSExtValue()) && isInt<14>(N->getSExtValue() + 1);
}]>;

// Mips Address Mode! SDNode frameindex could possibily be a match
// since load and store instructions from stack used it.
def addr :
  ComplexPattern<iPTR, 2, "selectIntAddr", [frameindex]>;

def addrRegImm :
  ComplexPattern<iPTR, 2, "selectAddrRegImm", [frameindex]>;

def addrRegReg :
  ComplexPattern<iPTR, 2, "selectAddrRegReg", [frameindex]>;

def addrDefault :
  ComplexPattern<iPTR, 2, "selectAddrDefault", [frameindex]>;

def addrimm10 : ComplexPattern<iPTR, 2, "selectIntAddrMSA", [frameindex]>;

//===----------------------------------------------------------------------===//
// Instructions specific format
//===----------------------------------------------------------------------===//

// Arithmetic and logical instructions with 3 register operands.
class ArithLogicR<string opstr, RegisterOperand RO, bit isComm = 0,
                  InstrItinClass Itin = NoItinerary,
                  SDPatternOperator OpNode = null_frag>:
  InstSE<(outs RO:$rd), (ins RO:$rs, RO:$rt),
         !strconcat(opstr, "\t$rd, $rs, $rt"),
         [(set RO:$rd, (OpNode RO:$rs, RO:$rt))], Itin, FrmPL18, opstr> {
  let isCommutable = isComm;
  let isReMaterializable = 1;
  let TwoOperandAliasConstraint = "$rd = $rs";
}

// Arithmetic and logical instructions with 2 register operands.
class ArithLogicI<string opstr, Operand Od, RegisterOperand RO,
                  InstrItinClass Itin = NoItinerary,
                  SDPatternOperator imm_type = null_frag,
                  SDPatternOperator OpNode = null_frag> :
  InstSE<(outs RO:$rt), (ins RO:$rs, Od:$imm14),
         !strconcat(opstr, "\t$rt, $rs, $imm14"),
         [(set RO:$rt, (OpNode RO:$rs, imm_type:$imm14))],
         Itin, FrmPL26i, opstr> {
  let isReMaterializable = 1;
  let TwoOperandAliasConstraint = "$rs = $rt";
}

//  Logical
class LogicNOR<string opstr, RegisterOperand RO>:
  InstSE<(outs RO:$rd), (ins RO:$rs, RO:$rt),
         !strconcat(opstr, "\t$rd, $rs, $rt"),
         [(set RO:$rd, (not (or RO:$rs, RO:$rt)))], II_NOR, FrmPL18, opstr> {
  let isCommutable = 1;
}

// Shifts
class shift_rotate_imm<string opstr, Operand ImmOpnd,
                       RegisterOperand RO, InstrItinClass itin,
                       SDPatternOperator OpNode = null_frag,
                       SDPatternOperator PF = null_frag> :
  InstSE<(outs RO:$rd), (ins RO:$rt, ImmOpnd:$shamt),
         !strconcat(opstr, "\t$rd, $rt, $shamt"),
         [(set RO:$rd, (OpNode RO:$rt, PF:$shamt))], itin, FrmPL18, opstr> {
  let TwoOperandAliasConstraint = "$rt = $rd";
}

class shift_rotate_reg<string opstr, RegisterOperand RO, InstrItinClass itin,
                       SDPatternOperator OpNode = null_frag>:
  InstSE<(outs RO:$rd), (ins RO:$rt, GPR32Opnd:$rs),
         !strconcat(opstr, "\t$rd, $rt, $rs"),
         [(set RO:$rd, (OpNode RO:$rt, GPR32Opnd:$rs))], itin, FrmPL18,
         opstr>;

// Memory Load/Store
class Load<string opstr, DAGOperand RO, SDPatternOperator OpNode = null_frag,
           InstrItinClass Itin = NoItinerary, ComplexPattern Addr = addr> :
  InstSE<(outs RO:$rt), (ins mem:$addr), !strconcat(opstr, "\t$rt, $addr"),
         [(set RO:$rt, (OpNode Addr:$addr))], Itin, FrmPL26i, opstr> {
  let DecoderMethod = "DecodeMem";
  let canFoldAsLoad = 1;
  let mayLoad = 1;
}

class Store<string opstr, DAGOperand RO, SDPatternOperator OpNode = null_frag,
            InstrItinClass Itin = NoItinerary, ComplexPattern Addr = addr> :
  InstSE<(outs), (ins RO:$rt, mem:$addr), !strconcat(opstr, "\t$rt, $addr"),
         [(OpNode RO:$rt, Addr:$addr)], Itin, FrmPL26i, opstr> {
  let DecoderMethod = "DecodeMem";
  let mayStore = 1;
}

// Load/Store Left/Right
let canFoldAsLoad = 1 in
class LoadLeftRight<string opstr, SDNode OpNode, RegisterOperand RO,
                    InstrItinClass Itin> :
  InstSE<(outs RO:$rt), (ins mem:$addr, RO:$src),
         !strconcat(opstr, "\t$rt, $addr"),
         [(set RO:$rt, (OpNode addr:$addr, RO:$src))], Itin, FrmPL26i> {
  let DecoderMethod = "DecodeMem";
  string Constraints = "$src = $rt";
}

class StoreLeftRight<string opstr, SDNode OpNode, RegisterOperand RO,
                     InstrItinClass Itin> :
  InstSE<(outs), (ins RO:$rt, mem:$addr), !strconcat(opstr, "\t$rt, $addr"),
         [(OpNode RO:$rt, addr:$addr)], Itin, FrmPL26i> {
  let DecoderMethod = "DecodeMem";
}

// Conditional Branch
class CBranch<string opstr, DAGOperand opnd, PatFrag cond_op,
              RegisterOperand RO, bit DelaySlot = 1> :
  InstSE<(outs), (ins RO:$rs, RO:$rt, opnd:$offset),
         !strconcat(opstr, "\t$rs, $rt, $offset"),
         [(brcond (i32 (cond_op RO:$rs, RO:$rt)), bb:$offset)], IIBranch,
         FrmPL26i, opstr> {
  let isBranch = 1;
  let isTerminator = 1;
  let hasDelaySlot = 0;
  let Defs = [AT];
}

class CBranchZero<string opstr, DAGOperand opnd, PatFrag cond_op,
                  RegisterOperand RO, bit DelaySlot = 1> :
  InstSE<(outs), (ins RO:$rs, opnd:$offset),
         !strconcat(opstr, "\t$rs, $offset"),
         [(brcond (i32 (cond_op RO:$rs, 0)), bb:$offset)], IIBranch,
         FrmPL20, opstr> {
  let isBranch = 1;
  let isTerminator = 1;
  let hasDelaySlot = 0;
  let Defs = [AT];
}

// SetCC
class SetCC_R<string opstr, PatFrag cond_op, RegisterOperand RO> :
  InstSE<(outs GPR32Opnd:$rd), (ins RO:$rs, RO:$rt),
         !strconcat(opstr, "\t$rd, $rs, $rt"),
         [(set GPR32Opnd:$rd, (cond_op RO:$rs, RO:$rt))],
         II_SLT_SLTU, FrmPL18, opstr>;

class SetCC_I<string opstr, PatFrag cond_op, Operand Od, PatLeaf imm_type,
              RegisterOperand RO>:
  InstSE<(outs GPR32Opnd:$rt), (ins RO:$rs, Od:$imm14),
         !strconcat(opstr, "\t$rt, $rs, $imm14"),
         [(set GPR32Opnd:$rt, (cond_op RO:$rs, imm_type:$imm14))],
         II_SLTI_SLTIU, FrmPL26i, opstr>;

// Jump
class JumpFJ<DAGOperand opnd, string opstr, SDPatternOperator operator,
             SDPatternOperator targetoperator, string bopstr> :
  InstSE<(outs), (ins opnd:$target), !strconcat(opstr, "\t$target"),
         [(operator targetoperator:$target)], IIBranch, FrmPL26j, bopstr> {
  let isTerminator=1;
  let isBarrier=1;
  let hasDelaySlot = 0;
  let DecoderMethod = "DecodeJumpTarget";
  let Defs = [AT];
}

// Unconditional branch
class UncondBranch<Instruction BEQInst> :
  PseudoSE<(outs), (ins brtarget:$offset), [(br bb:$offset)], IIBranch>,
  PseudoInstExpansion<(BEQInst ZERO, ZERO, brtarget:$offset)> {
  let isBranch = 1;
  let isTerminator = 1;
  let isBarrier = 1;
  let hasDelaySlot = 0;
  let AdditionalPredicates = [RelocPIC];
  let Defs = [AT];
}

// Base class for indirect branch and return instruction classes.
let isTerminator=1, isBarrier=1, hasDelaySlot = 0 in
class JumpFR<string opstr, RegisterOperand RO,
             SDPatternOperator operator = null_frag>:
  InstSE<(outs), (ins RO:$rt), "jumpr\t$rt", [(operator RO:$rt)], IIBranch,
         FrmPL6, opstr>;

// Indirect branch
class IndirectBranch<string opstr, RegisterOperand RO> : JumpFR<opstr, RO> {
  let isBranch = 1;
  let isIndirectBranch = 1;
}

// Jump and Link (Call)
let isCall=1, hasDelaySlot=0, Defs = [RA] in {
  class JumpLink<string opstr, DAGOperand opnd> :
    InstSE<(outs), (ins opnd:$target, uimm6:$num), !strconcat(opstr, "\t$target, $num"),
           [(MipsJmpLink imm:$target, imm:$num)], IIBranch, FrmPL26c, opstr> {
  }

  class JumpLinkReg<string opstr, RegisterOperand RO>:
    InstSE<(outs), (ins RO:$rs, uimm6:$num), !strconcat(opstr, "\t$rs, $num"),
           [], IIBranch, FrmPL12>;
}

let isCall = 1, isTerminator = 1, isReturn = 1, isBarrier = 1, hasDelaySlot = 0,
    hasExtraSrcRegAllocReq = 1, Defs = [AT] in {
  class TailCall<Instruction JumpInst> :
    PseudoSE<(outs), (ins calltarget:$target), [], IIBranch>,
    PseudoInstExpansion<(JumpInst jmptarget:$target)>;

  class TailCallReg<RegisterOperand RO, Instruction JRInst,
                    RegisterOperand ResRO = RO> :
    PseudoSE<(outs), (ins RO:$rs), [(MipsTailCall RO:$rs)], IIBranch>,
    PseudoInstExpansion<(JRInst ResRO:$rs)>;
}

class BAL_BR_Pseudo<Instruction RealInst> :
  PseudoSE<(outs), (ins brtarget:$offset), [], IIBranch>,
  PseudoInstExpansion<(RealInst ZERO, brtarget:$offset)> {
  let isBranch = 1;
  let isTerminator = 1;
  let isBarrier = 1;
  let hasDelaySlot = 0;
  let Defs = [RA];
}

// Syscall
class SYS_FT<string opstr> :
  InstSE<(outs), (ins),
         opstr, [], NoItinerary, FrmPL0, opstr>;
// Break
class BRK_FT<string opstr> :
  InstSE<(outs), (ins),
         opstr, [], NoItinerary,
         FrmPL0, opstr>;

// Sync
let hasSideEffects = 1 in
class SYNC_FT<string opstr> :
  InstSE<(outs), (ins i32imm:$stype), "sync $stype", [(MipsSync imm:$stype)],
         NoItinerary, FrmOther, opstr>;

let hasSideEffects = 1 in
class TEQ_FT<string opstr, RegisterOperand RO> :
  InstSE<(outs), (ins RO:$rs, RO:$rt),
         !strconcat(opstr, "\t$rs, $rt"), [], NoItinerary,
         FrmPL12, opstr>;

class TEQI_FT<string opstr, RegisterOperand RO> :
  InstSE<(outs), (ins RO:$rs, uimm16:$imm16),
         !strconcat(opstr, "\t$rs, $imm16"), [], NoItinerary, FrmOther, opstr>;

// Pseudo multiply/divide instruction with explicit accumulator register
// operands.
class MultDivPseudo<Instruction RealInst, RegisterClass R0, RegisterOperand R1,
                    SDPatternOperator OpNode, InstrItinClass Itin,
                    bit IsComm = 1, bit HasSideEffects = 0,
                    bit UsesCustomInserter = 0> :
  PseudoSE<(outs R0:$ac), (ins R1:$rs, R1:$rt),
           [(set R0:$ac, (OpNode R1:$rs, R1:$rt))], Itin>,
  PseudoInstExpansion<(RealInst R1:$rs, R1:$rt)> {
  let isCommutable = IsComm;
  let hasSideEffects = HasSideEffects;
  let usesCustomInserter = UsesCustomInserter;
}
// Pseudo multiply add/sub instruction with explicit accumulator register
// operands.
class MAddSubPseudo<Instruction RealInst, SDPatternOperator OpNode,
                    InstrItinClass itin>
  : PseudoSE<(outs ACC64:$ac),
             (ins GPR32Opnd:$rs, GPR32Opnd:$rt, ACC64:$acin),
             [(set ACC64:$ac,
              (OpNode GPR32Opnd:$rs, GPR32Opnd:$rt, ACC64:$acin))],
             itin>,
    PseudoInstExpansion<(RealInst GPR32Opnd:$rs, GPR32Opnd:$rt)> {
  string Constraints = "$acin = $ac";
}

class PseudoMTLOHI<RegisterClass DstRC, RegisterClass SrcRC>
  : PseudoSE<(outs DstRC:$lohi), (ins SrcRC:$lo, SrcRC:$hi),
             [(set DstRC:$lohi, (MipsMTLOHI SrcRC:$lo, SrcRC:$hi))],
             II_MTHI_MTLO>;

class EffectiveAddress<string opstr, RegisterOperand RO> :
  InstSE<(outs RO:$rt), (ins mem_ea:$addr), !strconcat(opstr, "\t$rt, $addr"),
         [(set RO:$rt, addr:$addr)], NoItinerary, FrmPL26i,
         !strconcat(opstr, "_lea")> {
  let isCodeGenOnly = 1;
  let DecoderMethod = "DecodeMem";
}

// Count Leading Ones/Zeros in Word
class CountLeading0<string opstr, RegisterOperand RO>:
  InstSE<(outs RO:$rd), (ins RO:$rt), !strconcat(opstr, "\t$rd, $rt"),
         [(set RO:$rd, (ctlz RO:$rt))], II_CLZ, FrmPL12, opstr>;


// Sign Extend in Register.
class SignExtInReg<string opstr, ValueType vt, RegisterOperand RO,
                   InstrItinClass itin> :
  InstSE<(outs RO:$rd), (ins RO:$rt), !strconcat(opstr, "\t$rd, $rt"),
         [(set RO:$rd, (sext_inreg RO:$rt, vt))], itin, FrmPL12, opstr>;

// Ext and Ins
class ExtBase<string opstr, RegisterOperand RO, Operand PosOpnd,
              SDPatternOperator Op = null_frag>:
  InstSE<(outs RO:$rt), (ins RO:$rs, PosOpnd:$pos, size_ext:$size),
         !strconcat(opstr, " $rt, $rs, $pos, $size"),
         [(set RO:$rt, (Op RO:$rs, imm:$pos, imm:$size))], NoItinerary,
         FrmPL24, opstr>;

// Atomic instructions with 2 source operands (ATOMIC_SWAP & ATOMIC_LOAD_*).
class Atomic2Ops<PatFrag Op, RegisterClass DRC> :
  PseudoSE<(outs DRC:$dst), (ins PtrRC:$ptr, DRC:$incr),
           [(set DRC:$dst, (Op iPTR:$ptr, DRC:$incr))]>;

// Atomic Compare & Swap.
class AtomicCmpSwap<PatFrag Op, RegisterClass DRC> :
  PseudoSE<(outs DRC:$dst), (ins PtrRC:$ptr, DRC:$cmp, DRC:$swap),
           [(set DRC:$dst, (Op iPTR:$ptr, DRC:$cmp, DRC:$swap))]>;

class LLBase<string opstr, RegisterOperand RO> :
  InstSE<(outs RO:$rt), (ins mem:$addr), !strconcat(opstr, "\t$rt, $addr"),
         [], NoItinerary, FrmPL26i> {
  let DecoderMethod = "DecodeMem";
  let mayLoad = 1;
}

class SCBase<string opstr, RegisterOperand RO> :
  InstSE<(outs RO:$dst), (ins RO:$rt, mem:$addr),
         !strconcat(opstr, "\t$rt, $addr"), [], NoItinerary, FrmPL26i> {
  let DecoderMethod = "DecodeMem";
  let mayStore = 1;
  let Constraints = "$rt = $dst";
}

class TrapBase<Instruction RealInst>
  : PseudoSE<(outs), (ins), [(trap)], NoItinerary>,
    PseudoInstExpansion<(RealInst)> {
  let isBarrier = 1;
  let isTerminator = 1;
  let isCodeGenOnly = 1;
}

//===----------------------------------------------------------------------===//
// Pseudo instructions
//===----------------------------------------------------------------------===//

// Return RA.
let isReturn=1, isTerminator=1, hasDelaySlot=0, isBarrier=1, hasCtrlDep=1 in
def RetRA : PseudoSE<(outs), (ins), [(MipsRet)]>;

let Defs = [SP], Uses = [SP], hasSideEffects = 1 in {
def ADJCALLSTACKDOWN : MipsPseudo<(outs), (ins i32imm:$amt),
                                  [(callseq_start timm:$amt)]>;
def ADJCALLSTACKUP   : MipsPseudo<(outs), (ins i32imm:$amt1, i32imm:$amt2),
                                  [(callseq_end timm:$amt1, timm:$amt2)]>;
}

let usesCustomInserter = 1 in {
  def ATOMIC_LOAD_ADD_I8   : Atomic2Ops<atomic_load_add_8, GPR32>;
  def ATOMIC_LOAD_ADD_I16  : Atomic2Ops<atomic_load_add_16, GPR32>;
  def ATOMIC_LOAD_ADD_I32  : Atomic2Ops<atomic_load_add_32, GPR32>;
  def ATOMIC_LOAD_SUB_I8   : Atomic2Ops<atomic_load_sub_8, GPR32>;
  def ATOMIC_LOAD_SUB_I16  : Atomic2Ops<atomic_load_sub_16, GPR32>;
  def ATOMIC_LOAD_SUB_I32  : Atomic2Ops<atomic_load_sub_32, GPR32>;
  def ATOMIC_LOAD_AND_I8   : Atomic2Ops<atomic_load_and_8, GPR32>;
  def ATOMIC_LOAD_AND_I16  : Atomic2Ops<atomic_load_and_16, GPR32>;
  def ATOMIC_LOAD_AND_I32  : Atomic2Ops<atomic_load_and_32, GPR32>;
  def ATOMIC_LOAD_OR_I8    : Atomic2Ops<atomic_load_or_8, GPR32>;
  def ATOMIC_LOAD_OR_I16   : Atomic2Ops<atomic_load_or_16, GPR32>;
  def ATOMIC_LOAD_OR_I32   : Atomic2Ops<atomic_load_or_32, GPR32>;
  def ATOMIC_LOAD_XOR_I8   : Atomic2Ops<atomic_load_xor_8, GPR32>;
  def ATOMIC_LOAD_XOR_I16  : Atomic2Ops<atomic_load_xor_16, GPR32>;
  def ATOMIC_LOAD_XOR_I32  : Atomic2Ops<atomic_load_xor_32, GPR32>;
  def ATOMIC_LOAD_NAND_I8  : Atomic2Ops<atomic_load_nand_8, GPR32>;
  def ATOMIC_LOAD_NAND_I16 : Atomic2Ops<atomic_load_nand_16, GPR32>;
  def ATOMIC_LOAD_NAND_I32 : Atomic2Ops<atomic_load_nand_32, GPR32>;

  def ATOMIC_SWAP_I8       : Atomic2Ops<atomic_swap_8, GPR32>;
  def ATOMIC_SWAP_I16      : Atomic2Ops<atomic_swap_16, GPR32>;
  def ATOMIC_SWAP_I32      : Atomic2Ops<atomic_swap_32, GPR32>;

  def ATOMIC_CMP_SWAP_I8   : AtomicCmpSwap<atomic_cmp_swap_8, GPR32>;
  def ATOMIC_CMP_SWAP_I16  : AtomicCmpSwap<atomic_cmp_swap_16, GPR32>;
  def ATOMIC_CMP_SWAP_I32  : AtomicCmpSwap<atomic_cmp_swap_32, GPR32>;
}

/// Pseudo instructions for loading and storing accumulator registers.
let isPseudo = 1, isCodeGenOnly = 1 in {
  def LOAD_ACC64  : Load<"", ACC64>;
  def STORE_ACC64 : Store<"", ACC64>;
}

// We need these two pseudo instructions to avoid offset calculation for long
// branches.  See the comment in file MipsLongBranch.cpp for detailed
// explanation.

// Expands to: lui $dst, %hi($tgt - $baltgt)
def LONG_BRANCH_LUi : PseudoSE<(outs GPR32Opnd:$dst),
  (ins brtarget:$tgt, brtarget:$baltgt), []>;

// Expands to: addiu $dst, $src, %lo($tgt - $baltgt)
def LONG_BRANCH_ADDiu : PseudoSE<(outs GPR32Opnd:$dst),
  (ins GPR32Opnd:$src, brtarget:$tgt, brtarget:$baltgt), []>;

//===----------------------------------------------------------------------===//
// Instruction definition
//===----------------------------------------------------------------------===//
//===----------------------------------------------------------------------===//
// MipsI Instructions
//===----------------------------------------------------------------------===//

/// Arithmetic Instructions (ALU Immediate)
let AdditionalPredicates = [NotInMicroMips] in {
def ADDiu : MMRel, ArithLogicI<"addi", simm14, GPR32Opnd, II_ADDIU, immSExt14,
                               add>, ADDI_FM<0xe>, IsAsCheapAsAMove;
}
def SLTi  : MMRel, SetCC_I<"slti", setlt, simm14, immSExt14, GPR32Opnd>,
            ADDI_FM<0x10>;
def SLTiu : MMRel, SetCC_I<"sltiu", setult, uimm14, immZExt14, GPR32Opnd>,
            ADDI_FM<0x11>;
let AdditionalPredicates = [NotInMicroMips] in {
def ANDi  : MMRel, ArithLogicI<"andi", uimm14, GPR32Opnd, II_ANDI, immZExt14,
                               and>, ADDI_FM<0x12>;
}
def ORi   : MMRel, ArithLogicI<"ori", uimm14, GPR32Opnd, II_ORI, immZExt14,
                               or>,
            ADDI_FM<0x13>;
def XORi  : MMRel, ArithLogicI<"xori", uimm14, GPR32Opnd, II_XORI, immZExt14,
                               xor>,
            ADDI_FM<0x14>;

def LDI   : PL20<0x1f, 0, 4, (outs GPR32Opnd:$rt), (ins uimm14:$imm14),
                 "ldi\t$rt, $imm14", [], NoItinerary>;
def LDIHI : PL18i<0x20, 0, 0, 1, (outs), (ins uimm18:$imm18),
                 "ldihi\t$imm18", [], NoItinerary>;

/// Arithmetic Instructions (3-Operand, R-Type)
def ADDu  : MMRel, ArithLogicR<"add", GPR32Opnd, 1, II_ADDU, add>,
            ADD_FM<0, 0>;
def SUBu  : MMRel, ArithLogicR<"sub", GPR32Opnd, 0, II_SUBU, sub>,
            ADD_FM<0, 0x2>;
def SLT   : MMRel, SetCC_R<"slt", setlt, GPR32Opnd>, ADD_FM<1, 0x0>;
def SLTu  : MMRel, SetCC_R<"sltu", setult, GPR32Opnd>, ADD_FM<1, 0x1>;
def AND   : MMRel, ArithLogicR<"and", GPR32Opnd, 1, II_AND, and>,
            ADD_FM<1, 0x2>;
def OR    : MMRel, ArithLogicR<"or", GPR32Opnd, 1, II_OR, or>,
            ADD_FM<1, 0x3>;
def XOR   : MMRel, ArithLogicR<"xor", GPR32Opnd, 1, II_XOR, xor>,
            ADD_FM<2, 0x0>;
def NOR   : MMRel, LogicNOR<"nor", GPR32Opnd>, ADD_FM<2, 0x1>;

/// Shift Instructions
let AdditionalPredicates = [NotInMicroMips] in {
def SLL  : MMRel, shift_rotate_imm<"shl", uimm5, GPR32Opnd, II_SLL, shl,
                                   immZExt5>, SRA_FM<2, 2>;
def SRL  : MMRel, shift_rotate_imm<"shr", uimm5, GPR32Opnd, II_SRL, srl,
                                   immZExt5>, SRA_FM<2, 3>;
}
def SRA  : MMRel, shift_rotate_imm<"asr", uimm5, GPR32Opnd, II_SRA, sra,
                                   immZExt5>, SRA_FM<3, 0>;
def SLLV : MMRel, shift_rotate_reg<"shlr", GPR32Opnd, II_SLLV, shl>,
           ADD_FM<3, 1>;
def SRLV : MMRel, shift_rotate_reg<"shrr", GPR32Opnd, II_SRLV, srl>,
           ADD_FM<3, 2>;
def SRAV : MMRel, shift_rotate_reg<"asrr", GPR32Opnd, II_SRAV, sra>,
           ADD_FM<3, 3>;

// Rotate Instructions
def ROTR  : MMRel, shift_rotate_imm<"ror", uimm5, GPR32Opnd, II_ROTR, rotr,
                                    immZExt5>,
            SRA_FM<4, 2>;
def ROTRV : MMRel, shift_rotate_reg<"rorr", GPR32Opnd, II_ROTRV, rotr>,
            ADD_FM<4, 3>;

/// Load and Store Instructions
///  aligned
def LB  : Load<"ldb", GPR32Opnd, sextloadi8, II_LB>, MMRel, LW_FM<0x02>;
def LBu : Load<"ldbu", GPR32Opnd, zextloadi8, II_LBU, addrDefault>, MMRel,
          LW_FM<0x3>;
def LH  : Load<"ldh", GPR32Opnd, sextloadi16, II_LH, addrDefault>, MMRel,
          LW_FM<0x4>;
def LHu : Load<"ldhu", GPR32Opnd, zextloadi16, II_LHU>, MMRel, LW_FM<0x05>;
def LW  : Load<"ldw", GPR32Opnd, load, II_LW, addrDefault>, MMRel,
          LW_FM<0x6>;
def SB  : Store<"stb", GPR32Opnd, truncstorei8, II_SB>, MMRel, LW_FM<0x09>;
def SH  : Store<"sth", GPR32Opnd, truncstorei16, II_SH>, MMRel, LW_FM<0x0a>;
def SW  : Store<"stw", GPR32Opnd, store, II_SW>, MMRel, LW_FM<0x0b>;

/// load/store left/right
let EncodingPredicates = []<Predicate>, // FIXME: Lack of HasStdEnc is probably a bug
    AdditionalPredicates = [NotInMicroMips] in {
def LWL : LoadLeftRight<"ldwl", MipsLWL, GPR32Opnd, II_LWL>, LW_FM<0x07>;
def LWR : LoadLeftRight<"ldwr", MipsLWR, GPR32Opnd, II_LWR>, LW_FM<0x08>;
def SWL : StoreLeftRight<"stwl", MipsSWL, GPR32Opnd, II_SWL>, LW_FM<0x0c>;
def SWR : StoreLeftRight<"stwr", MipsSWR, GPR32Opnd, II_SWR>, LW_FM<0x0d>;
}

def SYNC : MMRel, SYNC_FT<"sync">, SYNC_FM;

def TEQ : MMRel, TEQ_FT<"teq", GPR32Opnd>, TEQ_FM;

def BREAK : MMRel, BRK_FT<"break">, BRK_FM;
def SYSCALL : MMRel, SYS_FT<"syscall">, SYS_FM;

def TRAP : TrapBase<BREAK>;

let EncodingPredicates = []<Predicate>, // FIXME: Lack of HasStdEnc is probably a bug
    AdditionalPredicates = [NotInMicroMips] in {

/// Load-linked, Store-conditional
def LL : LLBase<"ll", GPR32Opnd>, LW_FM<0x17>;
def SC : SCBase<"sc", GPR32Opnd>, LW_FM<0x18>;
}

/// Jump and Branch Instructions
def J       : MMRel, JumpFJ<jmptarget, "jump", br, bb, "jump">, PL26j<0>,
              AdditionalRequires<[RelocStatic]>, IsBranch;
def JR      : MMRel, IndirectBranch<"jumpr", GPR32Opnd>, JUMPR_FM;
def BEQ     : MMRel, CBranch<"jeq", brtarget, seteq, GPR32Opnd>, BEQ_FM<0x15>;
def BNE     : MMRel, CBranch<"jne", brtarget, setne, GPR32Opnd>, BEQ_FM<0x16>;
def BGEZ    : MMRel, CBranchZero<"jgez", br14target, setge, GPR32Opnd>,
              BGEZ_FM<0x1f, 3>;
def BGTZ    : MMRel, CBranchZero<"jgtz", br14target, setgt, GPR32Opnd>,
              BGEZ_FM<0x1f, 1>;
def BLEZ    : MMRel, CBranchZero<"jlez", br14target, setle, GPR32Opnd>,
              BGEZ_FM<0x1f, 0>;
def BLTZ    : MMRel, CBranchZero<"jltz", br14target, setlt, GPR32Opnd>,
              BGEZ_FM<0x1f, 2>;
def B       : UncondBranch<BEQ>;

def JAL  : MMRel, JumpLink<"call", calltarget>, PL26c<1>;
let AdditionalPredicates = [NotInMicroMips] in {
  def JALR : JumpLinkReg<"callr", GPR32Opnd>, JALR_FM;
}

class BAL_DESC : BAL_FM<1, 0> {
  bit isBranch = 1;
  bit isTerminator = 1;
  bit isCall = 1;
  bit hasDelaySlot = 0;
  list<Register> Defs = [RA];
  dag InOperandList = (ins brtarget:$offset);
  dag OutOperandList = (outs);
}

def BAL_BR  : InstSE<(outs), (ins brtarget:$offset), "rcall\t$offset", [], NoItinerary, FrmPL16>, BAL_FM<1,0>;

def TAILCALL : TailCall<J>;
def TAILCALL_R : TailCallReg<GPR32Opnd, JR>;

// Indirect branches are matched as PseudoIndirectBranch/PseudoIndirectBranch64
// then are expanded to JR, JR64, JALR, or JALR64 depending on the ISA.
class PseudoIndirectBranchBase<RegisterOperand RO> :
    MipsPseudo<(outs), (ins RO:$rs), [(brind RO:$rs)], IIBranch> {
  let isTerminator=1;
  let isBarrier=1;
  let hasDelaySlot = 0;
  let isBranch = 1;
  let isIndirectBranch = 1;
}

def PseudoIndirectBranch : PseudoIndirectBranchBase<GPR32Opnd>;

// Return instructions are matched as a RetRA instruction, then ar expanded
// into PseudoReturn/PseudoReturn64 after register allocation. Finally,
// MipsAsmPrinter expands this into JR, JR64, JALR, or JALR64 depending on the
// ISA.
class PseudoReturnBase<RegisterOperand RO> : MipsPseudo<(outs), (ins RO:$rs),
                                                        [], IIBranch> {
  let isTerminator = 1;
  let isBarrier = 1;
  let hasDelaySlot = 0;
  let isReturn = 1;
  let isCodeGenOnly = 1;
  let hasCtrlDep = 1;
  let hasExtraSrcRegAllocReq = 1;
}

def PseudoReturn : PseudoReturnBase<GPR32Opnd>;

// Exception handling related node and instructions.
// The conversion sequence is:
// ISD::EH_RETURN -> MipsISD::EH_RETURN ->
// MIPSeh_return -> (stack change + indirect branch)
//
// MIPSeh_return takes the place of regular return instruction
// but takes two arguments (V1, V0) which are used for storing
// the offset and return address respectively.
def SDT_MipsEHRET : SDTypeProfile<0, 2, [SDTCisInt<0>, SDTCisPtrTy<1>]>;

def MIPSehret : SDNode<"MipsISD::EH_RETURN", SDT_MipsEHRET,
                      [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

let Uses = [V0, V1], isTerminator = 1, isReturn = 1, isBarrier = 1 in {
  def MIPSeh_return32 : MipsPseudo<(outs), (ins GPR32:$spoff, GPR32:$dst),
                                [(MIPSehret GPR32:$spoff, GPR32:$dst)]>;
  def MIPSeh_return64 : MipsPseudo<(outs), (ins GPR64:$spoff,
                                                GPR64:$dst),
                                [(MIPSehret GPR64:$spoff, GPR64:$dst)]>;
}

/// Multiply and Divide Instructions.
let isCommutable = 1 in {

  def MUL_OI  : InstSE<(outs GPR32Opnd:$rv, GPR32Opnd:$rd),
    (ins GPR32Opnd:$rs, GPR32Opnd:$rt), "mul\t$rv, $rd, $rs, $rt",
    [], II_MULT, FrmPL24, "mul">, MULOI_FM<0x1d, 0>;

  def MULU_OI  : InstSE<(outs GPR32Opnd:$rv, GPR32Opnd:$rd),
    (ins GPR32Opnd:$rs, GPR32Opnd:$rt), "mulu\t$rv, $rd, $rs, $rt",
    [], II_MULT, FrmPL24, "mulu">, MULOI_FM<0x25, 0>;

  def MUL_PSEUDO : PseudoSE<(outs GPR32Opnd:$rd),
    (ins GPR32Opnd:$rs, GPR32Opnd:$rt),
    [(set GPR32Opnd:$rd, (mul GPR32Opnd:$rs, GPR32Opnd:$rt))], II_MULT>,
    PseudoInstExpansion<(MUL_OI ZERO, GPR32Opnd:$rd, GPR32Opnd:$rs, GPR32Opnd:$rt)>;

  def MULHS_PSEUDO  : PseudoSE<(outs GPR32Opnd:$rd),
    (ins GPR32Opnd:$rs, GPR32Opnd:$rt),
    [(set GPR32Opnd:$rd, (mulhs GPR32Opnd:$rs, GPR32Opnd:$rt))],
    II_MULT>,
    PseudoInstExpansion<(MUL_OI GPR32Opnd:$rd, ZERO, GPR32Opnd:$rs, GPR32Opnd:$rt)>;

  def MULHU_PSEUDO  : PseudoSE<(outs GPR32Opnd:$rd),
    (ins GPR32Opnd:$rs, GPR32Opnd:$rt),
    [(set GPR32Opnd:$rd, (mulhu GPR32Opnd:$rs, GPR32Opnd:$rt))],
    II_MULT>,
    PseudoInstExpansion<(MULU_OI GPR32Opnd:$rd, ZERO, GPR32Opnd:$rs, GPR32Opnd:$rt)>;
}

def DIV_OI  : InstSE<(outs GPR32Opnd:$rv, GPR32Opnd:$rd),
  (ins GPR32Opnd:$rs, GPR32Opnd:$rt), "div\t$rv, $rd, $rs, $rt",
  [], II_MULT, FrmPL24, "div">, MULOI_FM<0x1d, 1>;

def DIVU_OI  : InstSE<(outs GPR32Opnd:$rv, GPR32Opnd:$rd),
  (ins GPR32Opnd:$rs, GPR32Opnd:$rt), "divu\t$rv, $rd, $rs, $rt",
  [], II_MULT, FrmPL24, "div">, MULOI_FM<0x1d, 2>;

def DIV_PSEUDO  : PseudoSE<(outs GPR32Opnd:$rd),
  (ins GPR32Opnd:$rs, GPR32Opnd:$rt),
  [(set GPR32Opnd:$rd, (sdiv GPR32Opnd:$rs, GPR32Opnd:$rt))],
  II_MULT>,
  PseudoInstExpansion<(DIV_OI ZERO, GPR32Opnd:$rd, GPR32Opnd:$rs, GPR32Opnd:$rt)>;

def DIVU_PSEUDO  : PseudoSE<(outs GPR32Opnd:$rd),
  (ins GPR32Opnd:$rs, GPR32Opnd:$rt),
  [(set GPR32Opnd:$rd, (udiv GPR32Opnd:$rs, GPR32Opnd:$rt))],
  II_MULT>,
  PseudoInstExpansion<(DIVU_OI ZERO, GPR32Opnd:$rd, GPR32Opnd:$rs, GPR32Opnd:$rt)>;

def MOD_PSEUDO  : PseudoSE<(outs GPR32Opnd:$rd),
  (ins GPR32Opnd:$rs, GPR32Opnd:$rt),
  [(set GPR32Opnd:$rd, (srem GPR32Opnd:$rs, GPR32Opnd:$rt))],
  II_MULT>,
  PseudoInstExpansion<(DIV_OI GPR32Opnd:$rd, ZERO, GPR32Opnd:$rs, GPR32Opnd:$rt)>;

def MODU_PSEUDO  : PseudoSE<(outs GPR32Opnd:$rd),
  (ins GPR32Opnd:$rs, GPR32Opnd:$rt),
  [(set GPR32Opnd:$rd, (urem GPR32Opnd:$rs, GPR32Opnd:$rt))],
  II_MULT>,
  PseudoInstExpansion<(DIVU_OI GPR32Opnd:$rd, ZERO, GPR32Opnd:$rs, GPR32Opnd:$rt)>;

/// Sign Ext In Register Instructions.
def SEB : MMRel, SignExtInReg<"seb", i8, GPR32Opnd, II_SEB>,
          SEB_FM<0x0, 0x3>;
def SEH : MMRel, SignExtInReg<"seh", i16, GPR32Opnd, II_SEH>,
          SEB_FM<0x0, 0x4>;

/// Count Leading
def CLZ : MMRel, CountLeading0<"clz", GPR32Opnd>, SEB_FM<0, 0x2>;

/// No operation.
def NOP : PseudoSE<(outs), (ins), []>, PseudoInstExpansion<(SLL ZERO, ZERO, 0)>;

// FrameIndexes are legalized when they are operands from load/store
// instructions. The same not happens for stack address copies, so an
// add op with mem ComplexPattern is used and the stack address copy
// can be matched. It's similar to Sparc LEA_ADDRi
def LEA_ADDiu : MMRel, EffectiveAddress<"addi", GPR32Opnd>, LW_FM<9>;

def EXT : MMRel, ExtBase<"ext", GPR32Opnd, uimm5, MipsExt>, EXT_FM;

//===----------------------------------------------------------------------===//
// Instruction aliases
//===----------------------------------------------------------------------===//
def : MipsInstAlias<"addu $rs, $rt, $imm",
                    (ADDiu GPR32Opnd:$rs, GPR32Opnd:$rt, simm14:$imm), 0>;
def : MipsInstAlias<"addu $rs, $imm",
                    (ADDiu GPR32Opnd:$rs, GPR32Opnd:$rs, simm14:$imm), 0>;
def : MipsInstAlias<"and $rs, $rt, $imm",
                    (ANDi GPR32Opnd:$rs, GPR32Opnd:$rt, simm14:$imm), 0>;
def : MipsInstAlias<"and $rs, $imm",
                    (ANDi GPR32Opnd:$rs, GPR32Opnd:$rs, simm14:$imm), 0>;
def : MipsInstAlias<"j $rs", (JR GPR32Opnd:$rs), 0>;
def : MipsInstAlias<"not $rt, $rs",
                    (NOR GPR32Opnd:$rt, GPR32Opnd:$rs, ZERO), 0>;
def : MipsInstAlias<"negu $rt",
                    (SUBu GPR32Opnd:$rt, ZERO, GPR32Opnd:$rt), 0>;
def : MipsInstAlias<"negu $rt, $rs",
                    (SUBu GPR32Opnd:$rt, ZERO, GPR32Opnd:$rs), 1>;
def : MipsInstAlias<"slt $rs, $rt, $imm",
                    (SLTi GPR32Opnd:$rs, GPR32Opnd:$rt, simm14:$imm), 0>;
def : MipsInstAlias<"sltu $rt, $rs, $imm",
                    (SLTiu GPR32Opnd:$rt, GPR32Opnd:$rs, uimm14:$imm), 0>;
def : MipsInstAlias<"xor $rs, $rt, $imm",
                    (XORi GPR32Opnd:$rs, GPR32Opnd:$rt, uimm14:$imm), 0>;
def : MipsInstAlias<"or $rs, $rt, $imm",
                    (ORi GPR32Opnd:$rs, GPR32Opnd:$rt, uimm14:$imm), 0>;
def : MipsInstAlias<"or $rs, $imm",
                    (ORi GPR32Opnd:$rs, GPR32Opnd:$rs, uimm14:$imm), 0>;
def : MipsInstAlias<"nop", (SLL ZERO, ZERO, 0), 1>;
def : MipsInstAlias<"b $offset", (BEQ ZERO, ZERO, brtarget:$offset), 0>;
def : MipsInstAlias<"jnez $rs,$offset",
                    (BNE GPR32Opnd:$rs, ZERO, brtarget:$offset), 0>;
def : MipsInstAlias<"jeqz $rs,$offset",
                    (BEQ GPR32Opnd:$rs, ZERO, brtarget:$offset), 0>;
def : MipsInstAlias<"syscall", (SYSCALL), 1>;

def : MipsInstAlias<"break", (BREAK), 1>;

def : MipsInstAlias<"teq $rs, $rt",
                    (TEQ GPR32Opnd:$rs, GPR32Opnd:$rt), 1>;

def  : MipsInstAlias<"sll $rd, $rt, $rs",
                     (SLLV GPR32Opnd:$rd, GPR32Opnd:$rt, GPR32Opnd:$rs), 0>;
def : MipsInstAlias<"subu, $rd, $rs, $imm",
                    (ADDiu GPR32Opnd:$rd, GPR32Opnd:$rs,
                           InvertedImOperand:$imm), 0>;
def : MipsInstAlias<"subu $rs, $imm", (ADDiu GPR32Opnd:$rs, GPR32Opnd:$rs,
                                             InvertedImOperand:$imm), 0>;
def : MipsInstAlias<"sra $rd, $rt, $rs",
                    (SRAV GPR32Opnd:$rd, GPR32Opnd:$rt, GPR32Opnd:$rs), 0>;
def : MipsInstAlias<"srl $rd, $rt, $rs",
                    (SRLV GPR32Opnd:$rd, GPR32Opnd:$rt, GPR32Opnd:$rs), 0>;
def : MipsInstAlias<"mul $rd, $rt, $rs",
                    (MUL_OI ZERO, GPR32Opnd:$rd, GPR32Opnd:$rt, GPR32Opnd:$rs), 1>;
def : MipsInstAlias<"mulhs $rd, $rt, $rs",
                    (MUL_OI GPR32Opnd:$rd, ZERO, GPR32Opnd:$rt, GPR32Opnd:$rs), 1>;
def : MipsInstAlias<"mulhu $rd, $rt, $rs",
                    (MULU_OI GPR32Opnd:$rd, ZERO, GPR32Opnd:$rt, GPR32Opnd:$rs), 1>;
def : MipsInstAlias<"div $rd, $rt, $rs",
                    (DIV_OI ZERO, GPR32Opnd:$rd, GPR32Opnd:$rt, GPR32Opnd:$rs), 1>;
def : MipsInstAlias<"divu $rd, $rt, $rs",
                    (DIVU_OI ZERO, GPR32Opnd:$rd, GPR32Opnd:$rt, GPR32Opnd:$rs), 1>;
def : MipsInstAlias<"mod $rd, $rt, $rs",
                    (DIV_OI GPR32Opnd:$rd, ZERO, GPR32Opnd:$rt, GPR32Opnd:$rs), 1>;
def : MipsInstAlias<"modu $rd, $rt, $rs",
                    (DIVU_OI GPR32Opnd:$rd, ZERO, GPR32Opnd:$rt, GPR32Opnd:$rs), 1>;
def : MipsInstAlias<"sync",
                    (SYNC 0), 1>;

def LOAD_IMM_PSEUDO : PseudoSE<(outs GPR32Opnd:$dst),
  (ins uimm32:$fullword), []>;

//===----------------------------------------------------------------------===//
// Assembler Pseudo Instructions
//===----------------------------------------------------------------------===//

class LoadImm32< string instr_asm, Operand Od, RegisterOperand RO> :
  MipsAsmPseudoInst<(outs RO:$rt), (ins Od:$imm32),
                     !strconcat(instr_asm, "\t$rt, $imm32")> ;
def LoadImm32Reg : LoadImm32<"li", uimm5, GPR32Opnd>;

class LoadAddress<string instr_asm, Operand MemOpnd, RegisterOperand RO> :
  MipsAsmPseudoInst<(outs RO:$rt), (ins MemOpnd:$addr),
                     !strconcat(instr_asm, "\t$rt, $addr")> ;
def LoadAddr32Reg : LoadAddress<"la", mem, GPR32Opnd>;

class LoadAddressImm<string instr_asm, Operand Od, RegisterOperand RO> :
  MipsAsmPseudoInst<(outs RO:$rt), (ins Od:$imm32),
                     !strconcat(instr_asm, "\t$rt, $imm32")> ;
def LoadAddr32Imm : LoadAddressImm<"la", uimm5, GPR32Opnd>;

//===----------------------------------------------------------------------===//
//  Arbitrary patterns that map to one or more instructions
//===----------------------------------------------------------------------===//

// Load/store pattern templates.
class LoadRegImmPat<Instruction LoadInst, ValueType ValTy, PatFrag Node> :
  MipsPat<(ValTy (Node addrRegImm:$a)), (LoadInst addrRegImm:$a)>;

class StoreRegImmPat<Instruction StoreInst, ValueType ValTy> :
  MipsPat<(store ValTy:$v, addrRegImm:$a), (StoreInst ValTy:$v, addrRegImm:$a)>;

// Small immediates
let AdditionalPredicates = [NotInMicroMips] in {
def : MipsPat<(i32 immSExt14:$in),
              (ADDiu ZERO, imm:$in)>;
def : MipsPat<(i32 immZExt14:$in),
              (ORi ZERO, imm:$in)>;
}

// Arbitrary immediates
def : MipsPat<(i32 imm:$imm),
          (LOAD_IMM_PSEUDO imm:$imm)>;

// Carry MipsPatterns
def : MipsPat<(subc GPR32:$lhs, GPR32:$rhs),
              (SUBu GPR32:$lhs, GPR32:$rhs)>;
let AdditionalPredicates = [NotDSP] in {
  def : MipsPat<(addc GPR32:$lhs, GPR32:$rhs),
                (ADDu GPR32:$lhs, GPR32:$rhs)>;
  def : MipsPat<(addc  GPR32:$src, immSExt14:$imm),
                (ADDiu GPR32:$src, imm:$imm)>;
}

// SYNC
def : MipsPat<(MipsSync (i32 immz)),
              (SYNC 0)>;

// Call
def : MipsPat<(MipsJmpLink (i32 tglobaladdr:$dst), imm:$num),
              (JAL tglobaladdr:$dst, imm:$num)>;
def : MipsPat<(MipsJmpLink (i32 texternalsym:$dst), imm:$num),
              (JAL texternalsym:$dst, imm:$num)>;
def : MipsPat<(MipsJmpLink GPR32:$dst, imm:$num),
              (JALR GPR32:$dst, imm:$num)>;

// Tail call
def : MipsPat<(MipsTailCall (iPTR tglobaladdr:$dst)),
              (TAILCALL tglobaladdr:$dst)>;
def : MipsPat<(MipsTailCall (iPTR texternalsym:$dst)),
              (TAILCALL texternalsym:$dst)>;
// hi/lo relocs
def : MipsPat<(MipsGetImm tglobaladdr:$in), (LOAD_IMM_PSEUDO tglobaladdr:$in)>;
def : MipsPat<(MipsGetImm tblockaddress:$in), (LOAD_IMM_PSEUDO tblockaddress:$in)>;
def : MipsPat<(MipsGetImm tjumptable:$in), (LOAD_IMM_PSEUDO tjumptable:$in)>;
def : MipsPat<(MipsGetImm tconstpool:$in), (LOAD_IMM_PSEUDO tconstpool:$in)>;
def : MipsPat<(MipsGetImm tglobaltlsaddr:$in), (LOAD_IMM_PSEUDO tglobaltlsaddr:$in)>;
def : MipsPat<(MipsGetImm texternalsym:$in), (LOAD_IMM_PSEUDO texternalsym:$in)>;

// gp_rel relocs
def : MipsPat<(add GPR32:$gp, (MipsGPRel tglobaladdr:$in)),
              (ADDiu GPR32:$gp, tglobaladdr:$in)>;
def : MipsPat<(add GPR32:$gp, (MipsGPRel tconstpool:$in)),
              (ADDiu GPR32:$gp, tconstpool:$in)>;

// wrapper_pic
class WrapperPat<SDNode node, Instruction ADDiuOp, RegisterClass RC>:
      MipsPat<(MipsWrapper RC:$gp, node:$in),
              (ADDiuOp RC:$gp, node:$in)>;

def : WrapperPat<tglobaladdr, ADDiu, GPR32>;
def : WrapperPat<tconstpool, ADDiu, GPR32>;
def : WrapperPat<texternalsym, ADDiu, GPR32>;
def : WrapperPat<tblockaddress, ADDiu, GPR32>;
def : WrapperPat<tjumptable, ADDiu, GPR32>;
def : WrapperPat<tglobaltlsaddr, ADDiu, GPR32>;

// Mips does not have "not", so we expand our way
def : MipsPat<(not GPR32:$in),
              (NOR GPR32Opnd:$in, ZERO)>;

// extended loads
def : MipsPat<(i32 (extloadi1  addr:$src)), (LBu addr:$src)>;
def : MipsPat<(i32 (extloadi8  addr:$src)), (LBu addr:$src)>;
def : MipsPat<(i32 (extloadi16 addr:$src)), (LHu addr:$src)>;

// peepholes
def : MipsPat<(store (i32 0), addr:$dst), (SW ZERO, addr:$dst)>;

// brcond patterns
multiclass BrcondPats<RegisterClass RC, Instruction BEQOp, Instruction BNEOp,
                      Instruction SLTOp, Instruction SLTuOp, Instruction SLTiOp,
                      Instruction SLTiuOp, Register ZEROReg> {
def : MipsPat<(brcond (i32 (setne RC:$lhs, 0)), bb:$dst),
              (BNEOp RC:$lhs, ZEROReg, bb:$dst)>;
def : MipsPat<(brcond (i32 (seteq RC:$lhs, 0)), bb:$dst),
              (BEQOp RC:$lhs, ZEROReg, bb:$dst)>;

def : MipsPat<(brcond (i32 (setge RC:$lhs, RC:$rhs)), bb:$dst),
              (BEQ (SLTOp RC:$lhs, RC:$rhs), ZERO, bb:$dst)>;
def : MipsPat<(brcond (i32 (setuge RC:$lhs, RC:$rhs)), bb:$dst),
              (BEQ (SLTuOp RC:$lhs, RC:$rhs), ZERO, bb:$dst)>;
def : MipsPat<(brcond (i32 (setge RC:$lhs, immSExt14:$rhs)), bb:$dst),
              (BEQ (SLTiOp RC:$lhs, immSExt14:$rhs), ZERO, bb:$dst)>;
def : MipsPat<(brcond (i32 (setuge RC:$lhs, immZExt14:$rhs)), bb:$dst),
              (BEQ (SLTiuOp RC:$lhs, immZExt14:$rhs), ZERO, bb:$dst)>;
def : MipsPat<(brcond (i32 (setgt RC:$lhs, immSExt14Plus1:$rhs)), bb:$dst),
              (BEQ (SLTiOp RC:$lhs, (Plus1 imm:$rhs)), ZERO, bb:$dst)>;
def : MipsPat<(brcond (i32 (setugt RC:$lhs, immSExt14Plus1:$rhs)), bb:$dst),
              (BEQ (SLTiuOp RC:$lhs, (Plus1 imm:$rhs)), ZERO, bb:$dst)>;

def : MipsPat<(brcond (i32 (setle RC:$lhs, RC:$rhs)), bb:$dst),
              (BEQ (SLTOp RC:$rhs, RC:$lhs), ZERO, bb:$dst)>;
def : MipsPat<(brcond (i32 (setule RC:$lhs, RC:$rhs)), bb:$dst),
              (BEQ (SLTuOp RC:$rhs, RC:$lhs), ZERO, bb:$dst)>;

def : MipsPat<(brcond RC:$cond, bb:$dst),
              (BNEOp RC:$cond, ZEROReg, bb:$dst)>;
}

defm : BrcondPats<GPR32, BEQ, BNE, SLT, SLTu, SLTi, SLTiu, ZERO>;

def : MipsPat<(brcond (i32 (setlt i32:$lhs, 1)), bb:$dst),
              (BLEZ i32:$lhs, bb:$dst)>;
def : MipsPat<(brcond (i32 (setgt i32:$lhs, -1)), bb:$dst),
              (BGEZ i32:$lhs, bb:$dst)>;

// setcc patterns
multiclass SeteqPats<RegisterClass RC, Instruction SLTiuOp, Instruction XOROp,
                     Instruction SLTuOp, Register ZEROReg> {
  def : MipsPat<(seteq RC:$lhs, 0),
                (SLTiuOp RC:$lhs, 1)>;
  def : MipsPat<(setne RC:$lhs, 0),
                (SLTuOp ZEROReg, RC:$lhs)>;
  def : MipsPat<(seteq RC:$lhs, RC:$rhs),
                (SLTiuOp (XOROp RC:$lhs, RC:$rhs), 1)>;
  def : MipsPat<(setne RC:$lhs, RC:$rhs),
                (SLTuOp ZEROReg, (XOROp RC:$lhs, RC:$rhs))>;
}

multiclass SetlePats<RegisterClass RC, Instruction SLTOp, Instruction SLTuOp> {
  def : MipsPat<(setle RC:$lhs, RC:$rhs),
                (XORi (SLTOp RC:$rhs, RC:$lhs), 1)>;
  def : MipsPat<(setule RC:$lhs, RC:$rhs),
                (XORi (SLTuOp RC:$rhs, RC:$lhs), 1)>;
}

multiclass SetgtPats<RegisterClass RC, Instruction SLTOp, Instruction SLTuOp> {
  def : MipsPat<(setgt RC:$lhs, RC:$rhs),
                (SLTOp RC:$rhs, RC:$lhs)>;
  def : MipsPat<(setugt RC:$lhs, RC:$rhs),
                (SLTuOp RC:$rhs, RC:$lhs)>;
}

multiclass SetgePats<RegisterClass RC, Instruction SLTOp, Instruction SLTuOp> {
  def : MipsPat<(setge RC:$lhs, RC:$rhs),
                (XORi (SLTOp RC:$lhs, RC:$rhs), 1)>;
  def : MipsPat<(setuge RC:$lhs, RC:$rhs),
                (XORi (SLTuOp RC:$lhs, RC:$rhs), 1)>;
}

multiclass SetgeImmPats<RegisterClass RC, Instruction SLTiOp,
                        Instruction SLTiuOp> {
  def : MipsPat<(setge RC:$lhs, immSExt14:$rhs),
                (XORi (SLTiOp RC:$lhs, immSExt14:$rhs), 1)>;
  def : MipsPat<(setuge RC:$lhs, immZExt14:$rhs),
                (XORi (SLTiuOp RC:$lhs, immZExt14:$rhs), 1)>;
}

defm : SeteqPats<GPR32, SLTiu, XOR, SLTu, ZERO>;
defm : SetlePats<GPR32, SLT, SLTu>;
defm : SetgtPats<GPR32, SLT, SLTu>;
defm : SetgePats<GPR32, SLT, SLTu>;
defm : SetgeImmPats<GPR32, SLTi, SLTiu>;

// bswap pattern
//def : MipsPat<(bswap GPR32:$rt), (ROTR (WSBH GPR32:$rt), 16)>;

// Load halfword/word patterns.
let AddedComplexity = 40 in {
  def : LoadRegImmPat<LBu, i32, zextloadi8>;
  def : LoadRegImmPat<LH, i32, sextloadi16>;
  def : LoadRegImmPat<LW, i32, load>;
}

//===----------------------------------------------------------------------===//
// Floating Point Support
//===----------------------------------------------------------------------===//

include "MipsInstrFPU.td"
include "MipsCondMov.td"
