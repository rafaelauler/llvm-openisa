//===-- MipsSEISelDAGToDAG.cpp - A Dag to Dag Inst Selector for MipsSE ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Subclass of MipsDAGToDAGISel specialized for mips32/64.
//
//===----------------------------------------------------------------------===//

#include "MipsSEISelDAGToDAG.h"
#include "MCTargetDesc/MipsBaseInfo.h"
#include "Mips.h"
#include "MipsMachineFunction.h"
#include "MipsRegisterInfo.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
using namespace llvm;

#define DEBUG_TYPE "mips-isel"

bool MipsSEDAGToDAGISel::runOnMachineFunction(MachineFunction &MF) {
  Subtarget = &TM.getSubtarget<MipsSubtarget>();
  if (Subtarget->inMips16Mode())
    return false;
  return MipsDAGToDAGISel::runOnMachineFunction(MF);
}

void MipsSEDAGToDAGISel::addDSPCtrlRegOperands(bool IsDef, MachineInstr &MI,
                                               MachineFunction &MF) {
  MachineInstrBuilder MIB(MF, &MI);
  unsigned Mask = MI.getOperand(1).getImm();
  unsigned Flag = IsDef ? RegState::ImplicitDefine : RegState::Implicit;

  if (Mask & 1)
    MIB.addReg(Mips::DSPPos, Flag);

  if (Mask & 2)
    MIB.addReg(Mips::DSPSCount, Flag);

  if (Mask & 4)
    MIB.addReg(Mips::DSPCarry, Flag);

  if (Mask & 8)
    MIB.addReg(Mips::DSPOutFlag, Flag);

  if (Mask & 16)
    MIB.addReg(Mips::DSPCCond, Flag);

  if (Mask & 32)
    MIB.addReg(Mips::DSPEFI, Flag);
}

unsigned MipsSEDAGToDAGISel::getMSACtrlReg(const SDValue RegIdx) const {
  switch (cast<ConstantSDNode>(RegIdx)->getZExtValue()) {
  default:
    llvm_unreachable("Could not map int to register");
  case 0: return Mips::MSAIR;
  case 1: return Mips::MSACSR;
  case 2: return Mips::MSAAccess;
  case 3: return Mips::MSASave;
  case 4: return Mips::MSAModify;
  case 5: return Mips::MSARequest;
  case 6: return Mips::MSAMap;
  case 7: return Mips::MSAUnmap;
  }
}

bool MipsSEDAGToDAGISel::replaceUsesWithZeroReg(MachineRegisterInfo *MRI,
                                                const MachineInstr& MI) {
  unsigned DstReg = 0, ZeroReg = 0;

  // Check if MI is "addiu $dst, $zero, 0" or "daddiu $dst, $zero, 0".
  if ((MI.getOpcode() == Mips::ADDiu) &&
      (MI.getOperand(1).getReg() == Mips::ZERO) &&
      (MI.getOperand(2).getImm() == 0)) {
    DstReg = MI.getOperand(0).getReg();
    ZeroReg = Mips::ZERO;
  }

  if (!DstReg)
    return false;

  // Replace uses with ZeroReg.
  for (MachineRegisterInfo::use_iterator U = MRI->use_begin(DstReg),
       E = MRI->use_end(); U != E;) {
    MachineOperand &MO = *U;
    unsigned OpNo = U.getOperandNo();
    MachineInstr *MI = MO.getParent();
    ++U;

    // Do not replace if it is a phi's operand or is tied to def operand.
    if (MI->isPHI() || MI->isRegTiedToDefOperand(OpNo) || MI->isPseudo())
      continue;

    MO.setReg(ZeroReg);
  }

  return true;
}

void MipsSEDAGToDAGISel::initGlobalBaseReg(MachineFunction &MF) {
  MipsFunctionInfo *MipsFI = MF.getInfo<MipsFunctionInfo>();

  if (!MipsFI->globalBaseRegSet())
    return;

  MachineBasicBlock &MBB = MF.front();
  MachineBasicBlock::iterator I = MBB.begin();
  MachineRegisterInfo &RegInfo = MF.getRegInfo();
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  DebugLoc DL = I != MBB.end() ? I->getDebugLoc() : DebugLoc();
  unsigned V0, GlobalBaseReg = MipsFI->getGlobalBaseReg();
  const TargetRegisterClass *RC;

  RC = (Subtarget->isABI_N64()) ? &Mips::GPR64RegClass : &Mips::GPR32RegClass;

  V0 = RegInfo.createVirtualRegister(RC);

  if (MF.getTarget().getRelocationModel() == Reloc::Static) {
    // Set global register to __gnu_local_gp.
    //
    // lui   $v0, %hi(__gnu_local_gp)
    // addiu $globalbasereg, $v0, %lo(__gnu_local_gp)
//    BuildMI(MBB, I, DL, TII.get(Mips::LUi), V0)
//      .addExternalSymbol("__gnu_local_gp", MipsII::MO_ABS_HI);
    BuildMI(MBB, I, DL, TII.get(Mips::LOAD_IMM_PSEUDO), GlobalBaseReg)
      .addExternalSymbol("__gnu_local_gp", MipsII::MO_ABS_LO);
    return;
  }

  MF.getRegInfo().addLiveIn(Mips::T9);
  MBB.addLiveIn(Mips::T9);

  if (Subtarget->isABI_N32()) {
    // lui $v0, %hi(%neg(%gp_rel(fname)))
    // addu $v1, $v0, $t9
    // addiu $globalbasereg, $v1, %lo(%neg(%gp_rel(fname)))
    const GlobalValue *FName = MF.getFunction();
//    BuildMI(MBB, I, DL, TII.get(Mips::LUi), V0)
//      .addGlobalAddress(FName, 0, MipsII::MO_GPOFF_HI);
    BuildMI(MBB, I, DL, TII.get(Mips::LOAD_IMM_PSEUDO), V0)
      .addGlobalAddress(FName, 0, MipsII::MO_GPOFF_LO);
    BuildMI(MBB, I, DL, TII.get(Mips::ADDu), GlobalBaseReg).addReg(V0).addReg(Mips::T9);
    return;
  }

  assert(Subtarget->isABI_O32());

  // For O32 ABI, the following instruction sequence is emitted to initialize
  // the global base register:
  //
  //  0. lui   $2, %hi(_gp_disp)
  //  1. addiu $2, $2, %lo(_gp_disp)
  //  2. addu  $globalbasereg, $2, $t9
  //
  // We emit only the last instruction here.
  //
  // GNU linker requires that the first two instructions appear at the beginning
  // of a function and no instructions be inserted before or between them.
  // The two instructions are emitted during lowering to MC layer in order to
  // avoid any reordering.
  //
  // Register $2 (Mips::V0) is added to the list of live-in registers to ensure
  // the value instruction 1 (addiu) defines is valid when instruction 2 (addu)
  // reads it.
  MF.getRegInfo().addLiveIn(Mips::V0);
  MBB.addLiveIn(Mips::V0);
  BuildMI(MBB, I, DL, TII.get(Mips::ADDu), GlobalBaseReg)
    .addReg(Mips::V0).addReg(Mips::T9);
}

void MipsSEDAGToDAGISel::processFunctionAfterISel(MachineFunction &MF) {
  initGlobalBaseReg(MF);

  MachineRegisterInfo *MRI = &MF.getRegInfo();

  for (MachineFunction::iterator MFI = MF.begin(), MFE = MF.end(); MFI != MFE;
       ++MFI)
    for (MachineBasicBlock::iterator I = MFI->begin(); I != MFI->end(); ++I) {
      replaceUsesWithZeroReg(MRI, *I);
    }
}

SDNode *MipsSEDAGToDAGISel::selectAddESubE(unsigned MOp, SDValue InFlag,
                                           SDValue CmpLHS, SDLoc DL,
                                           SDNode *Node) const {
  unsigned Opc = InFlag.getOpcode(); (void)Opc;

  assert(((Opc == ISD::ADDC || Opc == ISD::ADDE) ||
          (Opc == ISD::SUBC || Opc == ISD::SUBE)) &&
         "(ADD|SUB)E flag operand must come from (ADD|SUB)C/E insn");

  SDValue Ops[] = { CmpLHS, InFlag.getOperand(1) };
  SDValue LHS = Node->getOperand(0), RHS = Node->getOperand(1);
  EVT VT = LHS.getValueType();

  SDNode *Carry = CurDAG->getMachineNode(Mips::SLTu, DL, VT, Ops);
  SDNode *AddCarry = CurDAG->getMachineNode(Mips::ADDu, DL, VT,
                                            SDValue(Carry, 0), RHS);
  return CurDAG->SelectNodeTo(Node, MOp, VT, MVT::Glue, LHS,
                              SDValue(AddCarry, 0));
}

/// Match frameindex
bool MipsSEDAGToDAGISel::selectAddrFrameIndex(SDValue Addr, SDValue &Base,
                                              SDValue &Offset) const {
  if (FrameIndexSDNode *FIN = dyn_cast<FrameIndexSDNode>(Addr)) {
    EVT ValTy = Addr.getValueType();

    Base   = CurDAG->getTargetFrameIndex(FIN->getIndex(), ValTy);
    Offset = CurDAG->getTargetConstant(0, ValTy);
    return true;
  }
  return false;
}

/// Match frameindex+offset and frameindex|offset
bool MipsSEDAGToDAGISel::selectAddrFrameIndexOffset(SDValue Addr, SDValue &Base,
                                                    SDValue &Offset,
                                                    unsigned OffsetBits) const {
  if (CurDAG->isBaseWithConstantOffset(Addr)) {
    ConstantSDNode *CN = dyn_cast<ConstantSDNode>(Addr.getOperand(1));
    if (isIntN(OffsetBits, CN->getSExtValue())) {
      EVT ValTy = Addr.getValueType();

      // If the first operand is a FI, get the TargetFI Node
      if (FrameIndexSDNode *FIN = dyn_cast<FrameIndexSDNode>
                                  (Addr.getOperand(0)))
        Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), ValTy);
      else
        Base = Addr.getOperand(0);

      Offset = CurDAG->getTargetConstant(CN->getZExtValue(), ValTy);
      return true;
    }
  }
  return false;
}

/// ComplexPattern used on MipsInstrInfo
/// Used on Mips Load/Store instructions
bool MipsSEDAGToDAGISel::selectAddrRegImm(SDValue Addr, SDValue &Base,
                                          SDValue &Offset) const {
  // if Address is FI, get the TargetFrameIndex.
  if (selectAddrFrameIndex(Addr, Base, Offset))
    return true;

  // on PIC code Load GA
  if (Addr.getOpcode() == MipsISD::Wrapper) {
    Base   = Addr.getOperand(0);
    Offset = Addr.getOperand(1);
    return true;
  }

  if (TM.getRelocationModel() != Reloc::PIC_) {
    if ((Addr.getOpcode() == ISD::TargetExternalSymbol ||
        Addr.getOpcode() == ISD::TargetGlobalAddress))
      return false;
  }

  // Addresses of the form FI+const or FI|const
  if (selectAddrFrameIndexOffset(Addr, Base, Offset, 14))
    return true;

  // Operand is a result from an ADD.
  if (Addr.getOpcode() == ISD::ADD) {
    // When loading from constant pools, load the lower address part in
    // the instruction itself. Example, instead of:
    //  lui $2, %hi($CPI1_0)
    //  addiu $2, $2, %lo($CPI1_0)
    //  lwc1 $f0, 0($2)
    // Generate:
    //  lui $2, %hi($CPI1_0)
    //  lwc1 $f0, %lo($CPI1_0)($2)
    if (//Addr.getOperand(1).getOpcode() == MipsISD::Lo ||
        Addr.getOperand(1).getOpcode() == MipsISD::GPRel) {
      SDValue Opnd0 = Addr.getOperand(1).getOperand(0);
      if (isa<ConstantPoolSDNode>(Opnd0) || isa<GlobalAddressSDNode>(Opnd0) ||
          isa<JumpTableSDNode>(Opnd0)) {
        Base = Addr.getOperand(0);
        Offset = Opnd0;
        return true;
      }
    }
  }

  return false;
}

/// ComplexPattern used on MipsInstrInfo
/// Used on Mips Load/Store instructions
bool MipsSEDAGToDAGISel::selectAddrRegReg(SDValue Addr, SDValue &Base,
                                          SDValue &Offset) const {
  // Operand is a result from an ADD.
  if (Addr.getOpcode() == ISD::ADD) {
    Base = Addr.getOperand(0);
    Offset = Addr.getOperand(1);
    return true;
  }

  return false;
}

bool MipsSEDAGToDAGISel::selectAddrDefault(SDValue Addr, SDValue &Base,
                                           SDValue &Offset) const {
  Base = Addr;
  Offset = CurDAG->getTargetConstant(0, Addr.getValueType());
  return true;
}

bool MipsSEDAGToDAGISel::selectIJmpTarget(SDValue Addr, SDValue &Base,
                                          SDValue &Index,
                                          SDValue &Count) const {
  if (Addr.getOpcode() != ISD::LOAD)
    return false;

  SDValue LoadAddr = Addr.getOperand(1);
  if (LoadAddr.getOpcode() != ISD::ADD)
    return false;

  SDValue BaseOp = LoadAddr.getOperand(0);
  SDValue IndexReg = LoadAddr.getOperand(1);
  if (BaseOp.getOpcode() != MipsISD::GetImm) {
    BaseOp = LoadAddr.getOperand(1);
    IndexReg = LoadAddr.getOperand(0);
  }
  if (BaseOp.getOpcode() != MipsISD::GetImm)
    return false;

  SDValue JT = BaseOp.getOperand(0);
  const SDNode *N = JT.getNode();
  if (N->getOpcode() != ISD::TargetJumpTable &&
      N->getOpcode() != ISD::JumpTable)
    return false;

  unsigned I = cast<JumpTableSDNode>(N)->getIndex();
  const MachineJumpTableInfo *JTInfo = CurDAG->getMachineFunction().getJumpTableInfo();
  auto Size = JTInfo->getJumpTables()[I].MBBs.size();
  if (Size >= 256)
    Size = 0;
  Count = CurDAG->getTargetConstant(Size, MVT::i32);
  Index = IndexReg;
  Base = JT;
  return true;
}

bool MipsSEDAGToDAGISel::selectIntAddr(SDValue Addr, SDValue &Base,
                                       SDValue &Offset) const {
  return selectAddrRegImm(Addr, Base, Offset) ||
    selectAddrDefault(Addr, Base, Offset);
}

bool MipsSEDAGToDAGISel::selectAddrRegImm10(SDValue Addr, SDValue &Base,
                                            SDValue &Offset) const {
  if (selectAddrFrameIndex(Addr, Base, Offset))
    return true;

  if (selectAddrFrameIndexOffset(Addr, Base, Offset, 10))
    return true;

  return false;
}

/// Used on microMIPS Load/Store unaligned instructions (12-bit offset)
bool MipsSEDAGToDAGISel::selectAddrRegImm12(SDValue Addr, SDValue &Base,
                                            SDValue &Offset) const {
  if (selectAddrFrameIndex(Addr, Base, Offset))
    return true;

  if (selectAddrFrameIndexOffset(Addr, Base, Offset, 12))
    return true;

  return false;
}

bool MipsSEDAGToDAGISel::selectIntAddrMM(SDValue Addr, SDValue &Base,
                                         SDValue &Offset) const {
  return selectAddrRegImm12(Addr, Base, Offset) ||
    selectAddrDefault(Addr, Base, Offset);
}

bool MipsSEDAGToDAGISel::selectIntAddrMSA(SDValue Addr, SDValue &Base,
                                          SDValue &Offset) const {
  if (selectAddrRegImm10(Addr, Base, Offset))
    return true;

  if (selectAddrDefault(Addr, Base, Offset))
    return true;

  return false;
}

// Select constant vector splats.
//
// Returns true and sets Imm if:
// * MSA is enabled
// * N is a ISD::BUILD_VECTOR representing a constant splat
bool MipsSEDAGToDAGISel::selectVSplat(SDNode *N, APInt &Imm) const {
  if (!Subtarget->hasMSA())
    return false;

  BuildVectorSDNode *Node = dyn_cast<BuildVectorSDNode>(N);

  if (!Node)
    return false;

  APInt SplatValue, SplatUndef;
  unsigned SplatBitSize;
  bool HasAnyUndefs;

  if (!Node->isConstantSplat(SplatValue, SplatUndef, SplatBitSize,
                             HasAnyUndefs, 8,
                             !Subtarget->isLittle()))
    return false;

  Imm = SplatValue;

  return true;
}

// Select constant vector splats.
//
// In addition to the requirements of selectVSplat(), this function returns
// true and sets Imm if:
// * The splat value is the same width as the elements of the vector
// * The splat value fits in an integer with the specified signed-ness and
//   width.
//
// This function looks through ISD::BITCAST nodes.
// TODO: This might not be appropriate for big-endian MSA since BITCAST is
//       sometimes a shuffle in big-endian mode.
//
// It's worth noting that this function is not used as part of the selection
// of ldi.[bhwd] since it does not permit using the wrong-typed ldi.[bhwd]
// instruction to achieve the desired bit pattern. ldi.[bhwd] is selected in
// MipsSEDAGToDAGISel::selectNode.
bool MipsSEDAGToDAGISel::
selectVSplatCommon(SDValue N, SDValue &Imm, bool Signed,
                   unsigned ImmBitSize) const {
  APInt ImmValue;
  EVT EltTy = N->getValueType(0).getVectorElementType();

  if (N->getOpcode() == ISD::BITCAST)
    N = N->getOperand(0);

  if (selectVSplat (N.getNode(), ImmValue) &&
      ImmValue.getBitWidth() == EltTy.getSizeInBits()) {
    if (( Signed && ImmValue.isSignedIntN(ImmBitSize)) ||
        (!Signed && ImmValue.isIntN(ImmBitSize))) {
      Imm = CurDAG->getTargetConstant(ImmValue, EltTy);
      return true;
    }
  }

  return false;
}

// Select constant vector splats.
bool MipsSEDAGToDAGISel::
selectVSplatUimm1(SDValue N, SDValue &Imm) const {
  return selectVSplatCommon(N, Imm, false, 1);
}

bool MipsSEDAGToDAGISel::
selectVSplatUimm2(SDValue N, SDValue &Imm) const {
  return selectVSplatCommon(N, Imm, false, 2);
}

bool MipsSEDAGToDAGISel::
selectVSplatUimm3(SDValue N, SDValue &Imm) const {
  return selectVSplatCommon(N, Imm, false, 3);
}

// Select constant vector splats.
bool MipsSEDAGToDAGISel::
selectVSplatUimm4(SDValue N, SDValue &Imm) const {
  return selectVSplatCommon(N, Imm, false, 4);
}

// Select constant vector splats.
bool MipsSEDAGToDAGISel::
selectVSplatUimm5(SDValue N, SDValue &Imm) const {
  return selectVSplatCommon(N, Imm, false, 5);
}

// Select constant vector splats.
bool MipsSEDAGToDAGISel::
selectVSplatUimm6(SDValue N, SDValue &Imm) const {
  return selectVSplatCommon(N, Imm, false, 6);
}

// Select constant vector splats.
bool MipsSEDAGToDAGISel::
selectVSplatUimm8(SDValue N, SDValue &Imm) const {
  return selectVSplatCommon(N, Imm, false, 8);
}

// Select constant vector splats.
bool MipsSEDAGToDAGISel::
selectVSplatSimm5(SDValue N, SDValue &Imm) const {
  return selectVSplatCommon(N, Imm, true, 5);
}

// Select constant vector splats whose value is a power of 2.
//
// In addition to the requirements of selectVSplat(), this function returns
// true and sets Imm if:
// * The splat value is the same width as the elements of the vector
// * The splat value is a power of two.
//
// This function looks through ISD::BITCAST nodes.
// TODO: This might not be appropriate for big-endian MSA since BITCAST is
//       sometimes a shuffle in big-endian mode.
bool MipsSEDAGToDAGISel::selectVSplatUimmPow2(SDValue N, SDValue &Imm) const {
  APInt ImmValue;
  EVT EltTy = N->getValueType(0).getVectorElementType();

  if (N->getOpcode() == ISD::BITCAST)
    N = N->getOperand(0);

  if (selectVSplat (N.getNode(), ImmValue) &&
      ImmValue.getBitWidth() == EltTy.getSizeInBits()) {
    int32_t Log2 = ImmValue.exactLogBase2();

    if (Log2 != -1) {
      Imm = CurDAG->getTargetConstant(Log2, EltTy);
      return true;
    }
  }

  return false;
}

// Select constant vector splats whose value only has a consecutive sequence
// of left-most bits set (e.g. 0b11...1100...00).
//
// In addition to the requirements of selectVSplat(), this function returns
// true and sets Imm if:
// * The splat value is the same width as the elements of the vector
// * The splat value is a consecutive sequence of left-most bits.
//
// This function looks through ISD::BITCAST nodes.
// TODO: This might not be appropriate for big-endian MSA since BITCAST is
//       sometimes a shuffle in big-endian mode.
bool MipsSEDAGToDAGISel::selectVSplatMaskL(SDValue N, SDValue &Imm) const {
  APInt ImmValue;
  EVT EltTy = N->getValueType(0).getVectorElementType();

  if (N->getOpcode() == ISD::BITCAST)
    N = N->getOperand(0);

  if (selectVSplat(N.getNode(), ImmValue) &&
      ImmValue.getBitWidth() == EltTy.getSizeInBits()) {
    // Extract the run of set bits starting with bit zero from the bitwise
    // inverse of ImmValue, and test that the inverse of this is the same
    // as the original value.
    if (ImmValue == ~(~ImmValue & ~(~ImmValue + 1))) {

      Imm = CurDAG->getTargetConstant(ImmValue.countPopulation(), EltTy);
      return true;
    }
  }

  return false;
}

// Select constant vector splats whose value only has a consecutive sequence
// of right-most bits set (e.g. 0b00...0011...11).
//
// In addition to the requirements of selectVSplat(), this function returns
// true and sets Imm if:
// * The splat value is the same width as the elements of the vector
// * The splat value is a consecutive sequence of right-most bits.
//
// This function looks through ISD::BITCAST nodes.
// TODO: This might not be appropriate for big-endian MSA since BITCAST is
//       sometimes a shuffle in big-endian mode.
bool MipsSEDAGToDAGISel::selectVSplatMaskR(SDValue N, SDValue &Imm) const {
  APInt ImmValue;
  EVT EltTy = N->getValueType(0).getVectorElementType();

  if (N->getOpcode() == ISD::BITCAST)
    N = N->getOperand(0);

  if (selectVSplat(N.getNode(), ImmValue) &&
      ImmValue.getBitWidth() == EltTy.getSizeInBits()) {
    // Extract the run of set bits starting with bit zero, and test that the
    // result is the same as the original value
    if (ImmValue == (ImmValue & ~(ImmValue + 1))) {
      Imm = CurDAG->getTargetConstant(ImmValue.countPopulation(), EltTy);
      return true;
    }
  }

  return false;
}

bool MipsSEDAGToDAGISel::selectVSplatUimmInvPow2(SDValue N,
                                                 SDValue &Imm) const {
  APInt ImmValue;
  EVT EltTy = N->getValueType(0).getVectorElementType();

  if (N->getOpcode() == ISD::BITCAST)
    N = N->getOperand(0);

  if (selectVSplat(N.getNode(), ImmValue) &&
      ImmValue.getBitWidth() == EltTy.getSizeInBits()) {
    int32_t Log2 = (~ImmValue).exactLogBase2();

    if (Log2 != -1) {
      Imm = CurDAG->getTargetConstant(Log2, EltTy);
      return true;
    }
  }

  return false;
}

std::pair<bool, SDNode*> MipsSEDAGToDAGISel::selectNode(SDNode *Node) {
  unsigned Opcode = Node->getOpcode();
  SDLoc DL(Node);

  ///
  // Instruction Selection not handled by the auto-generated
  // tablegen selection should be handled here.
  ///
  SDNode *Result;

  switch(Opcode) {
  default: break;

  case ISD::SUBE: {
    SDValue InFlag = Node->getOperand(2);
    Result = selectAddESubE(Mips::SUBu, InFlag, InFlag.getOperand(0), DL, Node);
    return std::make_pair(true, Result);
  }

  case ISD::ADDE: {
    if (Subtarget->hasDSP()) // Select DSP instructions, ADDSC and ADDWC.
      break;
    SDValue InFlag = Node->getOperand(2);
    Result = selectAddESubE(Mips::ADDu, InFlag, InFlag.getValue(0), DL, Node);
    return std::make_pair(true, Result);
  }

  case ISD::ConstantFP: {
    ConstantFPSDNode *CN = dyn_cast<ConstantFPSDNode>(Node);
    if (Node->getValueType(0) == MVT::f64 && CN->isExactlyValue(+0.0)) {

      SDValue Zero = CurDAG->getCopyFromReg(CurDAG->getEntryNode(), DL,
                                            Mips::ZERO, MVT::i32);
      Result =
          CurDAG->getMachineNode(Mips::BuildPairF64, DL, MVT::f64, Zero, Zero);

      return std::make_pair(true, Result);
    }
    break;
  }

  case ISD::Constant: {
    const ConstantSDNode *CN = dyn_cast<ConstantSDNode>(Node);
    unsigned Size = CN->getValueSizeInBits(0);
    assert(Size == 32 && "Can't handle 64-bit constants");

    if (Size == 32)
      break;

  }

  case ISD::INTRINSIC_W_CHAIN: {
    switch (cast<ConstantSDNode>(Node->getOperand(1))->getZExtValue()) {
    default:
      break;

    case Intrinsic::mips_cfcmsa: {
      SDValue ChainIn = Node->getOperand(0);
      SDValue RegIdx = Node->getOperand(2);
      SDValue Reg = CurDAG->getCopyFromReg(ChainIn, DL,
                                           getMSACtrlReg(RegIdx), MVT::i32);
      return std::make_pair(true, Reg.getNode());
    }
    }
    break;
  }

  case ISD::INTRINSIC_WO_CHAIN: {
    switch (cast<ConstantSDNode>(Node->getOperand(0))->getZExtValue()) {
    default:
      break;

    }
    break;
  }

  case ISD::INTRINSIC_VOID: {
    switch (cast<ConstantSDNode>(Node->getOperand(1))->getZExtValue()) {
    default:
      break;

    case Intrinsic::mips_ctcmsa: {
      SDValue ChainIn = Node->getOperand(0);
      SDValue RegIdx  = Node->getOperand(2);
      SDValue Value   = Node->getOperand(3);
      SDValue ChainOut = CurDAG->getCopyToReg(ChainIn, DL,
                                              getMSACtrlReg(RegIdx), Value);
      return std::make_pair(true, ChainOut.getNode());
    }
    }
    break;
  }

  case MipsISD::ThreadPointer: {
//    EVT PtrVT = getTargetLowering()->getPointerTy();
//    unsigned RdhwrOpc, DestReg;
//
//    if (PtrVT == MVT::i32) {
//      RdhwrOpc = Mips::RDHWR;
//      DestReg = Mips::V1;
//    }
//    SDNode *Rdhwr =
//      CurDAG->getMachineNode(RdhwrOpc, SDLoc(Node),
//                             Node->getValueType(0),
//                             CurDAG->getRegister(Mips::HWR29, MVT::i32));
//    SDValue Chain = CurDAG->getCopyToReg(CurDAG->getEntryNode(), DL, DestReg,
//                                         SDValue(Rdhwr, 0));
//    SDValue ResNode = CurDAG->getCopyFromReg(Chain, DL, DestReg, PtrVT);
//    ReplaceUses(SDValue(Node, 0), ResNode);
//    return std::make_pair(true, ResNode.getNode());
  }

  case ISD::BUILD_VECTOR: {
    return std::make_pair(false, nullptr);

  }

  }

  return std::make_pair(false, nullptr);
}

FunctionPass *llvm::createMipsSEISelDag(MipsTargetMachine &TM) {
  return new MipsSEDAGToDAGISel(TM);
}
