//===-- MipsSEInstrInfo.cpp - Mips32/64 Instruction Information -----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the Mips32/64 implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#include "MipsSEInstrInfo.h"
#include "InstPrinter/MipsInstPrinter.h"
#include "MipsMachineFunction.h"
#include "MipsTargetMachine.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/TargetRegistry.h"

using namespace llvm;

MipsSEInstrInfo::MipsSEInstrInfo(const MipsSubtarget &STI)
    : MipsInstrInfo(STI, STI.getRelocationModel() == Reloc::PIC_ ? Mips::B
                                                                 : Mips::J),
      RI(STI) {}

const MipsRegisterInfo &MipsSEInstrInfo::getRegisterInfo() const {
  return RI;
}

/// isLoadFromStackSlot - If the specified machine instruction is a direct
/// load from a stack slot, return the virtual or physical register number of
/// the destination along with the FrameIndex of the loaded stack slot.  If
/// not, return 0.  This predicate must return 0 if the instruction has
/// any side effects other than loading from the stack slot.
unsigned MipsSEInstrInfo::isLoadFromStackSlot(const MachineInstr *MI,
                                              int &FrameIndex) const {
  unsigned Opc = MI->getOpcode();

  if ((Opc == Mips::LW) || (Opc == Mips::LWC1) || (Opc == Mips::LDC1)) {
    if ((MI->getOperand(1).isFI()) &&  // is a stack slot
        (MI->getOperand(2).isImm()) && // the imm is zero
        (isZeroImm(MI->getOperand(2)))) {
      FrameIndex = MI->getOperand(1).getIndex();
      return MI->getOperand(0).getReg();
    }
  }

  return 0;
}

/// isStoreToStackSlot - If the specified machine instruction is a direct
/// store to a stack slot, return the virtual or physical register number of
/// the source reg along with the FrameIndex of the loaded stack slot.  If
/// not, return 0.  This predicate must return 0 if the instruction has
/// any side effects other than storing to the stack slot.
unsigned MipsSEInstrInfo::isStoreToStackSlot(const MachineInstr *MI,
                                             int &FrameIndex) const {
  unsigned Opc = MI->getOpcode();

  if ((Opc == Mips::SW) || (Opc == Mips::SWC1) || (Opc == Mips::SDC1)) {
    if ((MI->getOperand(1).isFI()) &&  // is a stack slot
        (MI->getOperand(2).isImm()) && // the imm is zero
        (isZeroImm(MI->getOperand(2)))) {
      FrameIndex = MI->getOperand(1).getIndex();
      return MI->getOperand(0).getReg();
    }
  }
  return 0;
}

void MipsSEInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator I, DebugLoc DL,
                                  unsigned DestReg, unsigned SrcReg,
                                  bool KillSrc) const {
  unsigned Opc = 0, ZeroReg = 0;

  if (Mips::GPR32RegClass.contains(DestReg)) { // Copy to CPU Reg.
    if (Mips::GPR32RegClass.contains(SrcReg)) {
      Opc = Mips::ADDu, ZeroReg = Mips::ZERO;
    } else if (Mips::FGR32RegClass.contains(SrcReg)) {
      Opc = Mips::MFC1;
    }
  } else if (Mips::GPR32RegClass.contains(SrcReg)) { // Copy from CPU Reg.
    if (Mips::FGR32RegClass.contains(DestReg)) {
      Opc = Mips::MTC1;
    }
  } else if (Mips::FGR32RegClass.contains(DestReg, SrcReg)) {
    Opc = Mips::FMOV_S;
  } else if (Mips::AFGR64RegClass.contains(DestReg, SrcReg)) {
    Opc = Mips::FMOV_D32;
  }

  assert(Opc && "Cannot copy registers");

  MachineInstrBuilder MIB = BuildMI(MBB, I, DL, get(Opc));

  if (DestReg)
    MIB.addReg(DestReg, RegState::Define);

  if (SrcReg)
    MIB.addReg(SrcReg, getKillRegState(KillSrc));

  if (ZeroReg)
    MIB.addReg(ZeroReg);
}

void MipsSEInstrInfo::
storeRegToStack(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                unsigned SrcReg, bool isKill, int FI,
                const TargetRegisterClass *RC, const TargetRegisterInfo *TRI,
                int64_t Offset) const {
  DebugLoc DL;
  if (I != MBB.end()) DL = I->getDebugLoc();
  MachineMemOperand *MMO = GetMemOperand(MBB, FI, MachineMemOperand::MOStore);

  unsigned Opc = 0;

  if (Mips::GPR32RegClass.hasSubClassEq(RC))
    Opc = Mips::SW;
  else if (Mips::FGR32RegClass.hasSubClassEq(RC))
    Opc = Mips::SWC1;
  else if (Mips::AFGR64RegClass.hasSubClassEq(RC))
    Opc = Mips::SDC1;

  assert(Opc && "Register class not handled!");
  BuildMI(MBB, I, DL, get(Opc)).addReg(SrcReg, getKillRegState(isKill))
    .addFrameIndex(FI).addImm(Offset).addMemOperand(MMO);
}

void MipsSEInstrInfo::
loadRegFromStack(MachineBasicBlock &MBB, MachineBasicBlock::iterator I,
                 unsigned DestReg, int FI, const TargetRegisterClass *RC,
                 const TargetRegisterInfo *TRI, int64_t Offset) const {
  DebugLoc DL;
  if (I != MBB.end()) DL = I->getDebugLoc();
  MachineMemOperand *MMO = GetMemOperand(MBB, FI, MachineMemOperand::MOLoad);
  unsigned Opc = 0;

  if (Mips::GPR32RegClass.hasSubClassEq(RC))
    Opc = Mips::LW;
  else if (Mips::FGR32RegClass.hasSubClassEq(RC))
    Opc = Mips::LWC1;
  else if (Mips::AFGR64RegClass.hasSubClassEq(RC))
    Opc = Mips::LDC1;

  assert(Opc && "Register class not handled!");
  BuildMI(MBB, I, DL, get(Opc), DestReg).addFrameIndex(FI).addImm(Offset)
    .addMemOperand(MMO);
}

bool MipsSEInstrInfo::expandPostRAPseudo(MachineBasicBlock::iterator MI) const {
  MachineBasicBlock &MBB = *MI->getParent();
  //  unsigned Opc;

  switch(MI->getDesc().getOpcode()) {
  default:
    return false;
  case Mips::RetRA:
    expandRetRA(MBB, MI);
    break;
  case Mips::BuildPairF64:
    expandBuildPairF64(MBB, MI, false);
    break;
  case Mips::ExtractElementF64:
    expandExtractElementF64(MBB, MI, false);
    break;
  case Mips::MIPSeh_return32:
  case Mips::MIPSeh_return64:
    expandEhReturn(MBB, MI);
    break;
  }

  MBB.erase(MI);
  return true;
}

/// getOppositeBranchOpc - Return the inverse of the specified
/// opcode, e.g. turning BEQ to BNE.
unsigned MipsSEInstrInfo::getOppositeBranchOpc(unsigned Opc) const {
  switch (Opc) {
  default:           llvm_unreachable("Illegal opcode!");
  case Mips::BEQ:    return Mips::BNE;
  case Mips::BNE:    return Mips::BEQ;
  case Mips::BGTZ:   return Mips::BLEZ;
  case Mips::BGEZ:   return Mips::BLTZ;
  case Mips::BLTZ:   return Mips::BGEZ;
  case Mips::BLEZ:   return Mips::BGTZ;
  case Mips::BC1T:   return Mips::BC1F;
  case Mips::BC1F:   return Mips::BC1T;
  }
}

/// Adjust SP by Amount bytes.
void MipsSEInstrInfo::adjustStackPtr(unsigned SP, int64_t Amount,
                                     MachineBasicBlock &MBB,
                                     MachineBasicBlock::iterator I) const {
  const MipsSubtarget &STI = Subtarget;
  DebugLoc DL = I != MBB.end() ? I->getDebugLoc() : DebugLoc();
  unsigned ADDu = Mips::ADDu;
  unsigned ADDiu = Mips::ADDiu;

  if (isInt<14>(Amount))// addi sp, sp, amount
    BuildMI(MBB, I, DL, get(ADDiu), SP).addReg(SP).addImm(Amount);
  else { // Expand immediate that doesn't fit in 16-bit.
    const TargetRegisterClass *RC = &Mips::GPR32RegClass;
    unsigned Reg = MBB.getParent()->getRegInfo().createVirtualRegister(RC);
    BuildMI(MBB, I, DL, get(Mips::LOAD_IMM_PSEUDO), Reg).addImm(Amount);
    BuildMI(MBB, I, DL, get(ADDu), SP).addReg(SP).addReg(Reg, RegState::Kill);
  }
}

unsigned MipsSEInstrInfo::getAnalyzableBrOpc(unsigned Opc) const {
  return (Opc == Mips::BEQ    || Opc == Mips::BNE    || Opc == Mips::BGTZ   ||
          Opc == Mips::BGEZ   || Opc == Mips::BLTZ   || Opc == Mips::BLEZ   ||
          Opc == Mips::BC1T   || Opc == Mips::BC1F   || Opc == Mips::B      ||
          Opc == Mips::J) ?
         Opc : 0;
}

void MipsSEInstrInfo::expandRetRA(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator I) const {
    BuildMI(MBB, I, I->getDebugLoc(), get(Mips::PseudoReturn)).addReg(Mips::RA);
}

std::pair<bool, bool>
MipsSEInstrInfo::compareOpndSize(unsigned Opc,
                                 const MachineFunction &MF) const {
  const MCInstrDesc &Desc = get(Opc);
  assert(Desc.NumOperands == 2 && "Unary instruction expected.");
  const MipsRegisterInfo *RI = &getRegisterInfo();
  unsigned DstRegSize = getRegClass(Desc, 0, RI, MF)->getSize();
  unsigned SrcRegSize = getRegClass(Desc, 1, RI, MF)->getSize();

  return std::make_pair(DstRegSize > SrcRegSize, DstRegSize < SrcRegSize);
}

void MipsSEInstrInfo::expandExtractElementF64(MachineBasicBlock &MBB,
                                              MachineBasicBlock::iterator I,
                                              bool FP64) const {
  unsigned DstReg = I->getOperand(0).getReg();
  unsigned SrcReg = I->getOperand(1).getReg();
  unsigned N = I->getOperand(2).getImm();
  DebugLoc dl = I->getDebugLoc();

  assert(N < 2 && "Invalid immediate");
  unsigned SubIdx = N ? Mips::sub_hi : Mips::sub_lo;

  // FPXX on MIPS-II or MIPS32r1 should have been handled with a spill/reload
  // in MipsSEFrameLowering.cpp.
  assert(!(Subtarget.isABI_FPXX() && !Subtarget.hasMips32r2()));

  // FP64A (FP64 with nooddspreg) should have been handled with a spill/reload
  // in MipsSEFrameLowering.cpp.
  assert(!(Subtarget.isFP64bit() && !Subtarget.useOddSPReg()));

  if (SubIdx == Mips::sub_hi) {
    // FIXME: Strictly speaking MFHC1 only reads the top 32-bits however, we
    //        claim to read the whole 64-bits as part of a white lie used to
    //        temporarily work around a widespread bug in the -mfp64 support.
    //        The problem is that none of the 32-bit fpu ops mention the fact
    //        that they clobber the upper 32-bits of the 64-bit FPR. Fixing that
    //        requires a major overhaul of the FPU implementation which can't
    //        be done right now due to time constraints.
    //        MFHC1 is one of two instructions that are affected since they are
    //        the only instructions that don't read the lower 32-bits.
    //        We therefore pretend that it reads the bottom 32-bits to
    //        artificially create a dependency and prevent the scheduler
    //        changing the behaviour of the code.
    BuildMI(MBB, I, dl, get(Mips::MFHC1_D32), DstReg)
        .addReg(SrcReg);
  } else
    BuildMI(MBB, I, dl, get(Mips::MFLC1_D32), DstReg).addReg(SrcReg);
}

void MipsSEInstrInfo::expandBuildPairF64(MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator I,
                                         bool FP64) const {
  unsigned DstReg = I->getOperand(0).getReg();
  unsigned LoReg = I->getOperand(1).getReg(), HiReg = I->getOperand(2).getReg();
  DebugLoc dl = I->getDebugLoc();
  const TargetRegisterInfo &TRI = getRegisterInfo();

  // When mthc1 is available, use:
  //   mtc1 Lo, $fp
  //   mthc1 Hi, $fp
  //
  // Otherwise, for O32 FPXX ABI:
  //   spill + reload via ldc1
  // This case is handled by the frame lowering code.
  //
  // Otherwise, for FP32:
  //   mtc1 Lo, $fp
  //   mtc1 Hi, $fp + 1
  //
  // The case where dmtc1 is available doesn't need to be handled here
  // because it never creates a BuildPairF64 node.

  // FPXX on MIPS-II or MIPS32r1 should have been handled with a spill/reload
  // in MipsSEFrameLowering.cpp.
  assert(!(Subtarget.isABI_FPXX() && !Subtarget.hasMips32r2()));

  // FP64A (FP64 with nooddspreg) should have been handled with a spill/reload
  // in MipsSEFrameLowering.cpp.
  assert(!(Subtarget.isFP64bit() && !Subtarget.useOddSPReg()));

  BuildMI(MBB, I, dl, get(Mips::MTLC1_D32), DstReg)
    .addReg(DstReg)
    .addReg(LoReg);

  // FIXME: The .addReg(DstReg) is a white lie used to temporarily work
  //        around a widespread bug in the -mfp64 support.
  //        The problem is that none of the 32-bit fpu ops mention the fact
  //        that they clobber the upper 32-bits of the 64-bit FPR. Fixing that
  //        requires a major overhaul of the FPU implementation which can't
  //        be done right now due to time constraints.
  //        MTHC1 is one of two instructions that are affected since they are
  //        the only instructions that don't read the lower 32-bits.
  //        We therefore pretend that it reads the bottom 32-bits to
  //        artificially create a dependency and prevent the scheduler
  //        changing the behaviour of the code.
  BuildMI(MBB, I, dl, get(Mips::MTHC1_D32), DstReg)
      .addReg(DstReg)
      .addReg(HiReg);
}

void MipsSEInstrInfo::expandEhReturn(MachineBasicBlock &MBB,
                                     MachineBasicBlock::iterator I) const {
  // This pseudo instruction is generated as part of the lowering of
  // ISD::EH_RETURN. We convert it to a stack increment by OffsetReg, and
  // indirect jump to TargetReg
  unsigned ADDU = Mips::ADDu;//Subtarget.isABI_N64() ? Mips::DADDu : Mips::ADDu;
  unsigned SP = Subtarget.isGP64bit() ? Mips::SP_64 : Mips::SP;
  unsigned RA = Subtarget.isGP64bit() ? Mips::RA_64 : Mips::RA;
  unsigned T9 = Subtarget.isGP64bit() ? Mips::T9_64 : Mips::T9;
  unsigned ZERO = Subtarget.isGP64bit() ? Mips::ZERO_64 : Mips::ZERO;
  unsigned OffsetReg = I->getOperand(0).getReg();
  unsigned TargetReg = I->getOperand(1).getReg();

  // addu $ra, $v0, $zero
  // addu $sp, $sp, $v1
  // jr   $ra (via RetRA)
  const TargetMachine &TM = MBB.getParent()->getTarget();
  if (TM.getRelocationModel() == Reloc::PIC_)
    BuildMI(MBB, I, I->getDebugLoc(), get(ADDU), T9)
        .addReg(TargetReg)
        .addReg(ZERO);
  BuildMI(MBB, I, I->getDebugLoc(), get(ADDU), RA)
      .addReg(TargetReg)
      .addReg(ZERO);
  BuildMI(MBB, I, I->getDebugLoc(), get(ADDU), SP).addReg(SP).addReg(OffsetReg);
  expandRetRA(MBB, I);
}

const MipsInstrInfo *llvm::createMipsSEInstrInfo(const MipsSubtarget &STI) {
  return new MipsSEInstrInfo(STI);
}
