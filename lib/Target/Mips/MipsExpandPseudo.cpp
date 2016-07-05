
#include "Mips.h"
#include "MipsMachineFunction.h"
#include "MipsTargetMachine.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;

#define DEBUG_TYPE "mips-expand-pseudo"

STATISTIC(ExpandPseudo, "Number of pseudos expanded.");

namespace {
  typedef MachineBasicBlock::iterator Iter;
  typedef MachineBasicBlock::reverse_iterator ReverseIter;

  class MipsExpandPseudo : public MachineFunctionPass {

  public:
    static char ID;
    MipsExpandPseudo(TargetMachine &tm)
      : MachineFunctionPass(ID), TM(tm) {}

    const char *getPassName() const override {
      return "Mips Expand Pseudo";
    }

    bool runOnMachineFunction(MachineFunction &F) override;

  private:
    void expandLoadImm(MachineBasicBlock &MBB,
                       MachineBasicBlock::iterator I,
                       const MipsInstrInfo *TII) const;
    void expandIJmp(MachineBasicBlock &MBB,
                    MachineBasicBlock::iterator I,
                    const MipsInstrInfo *TII) const;

    const TargetMachine &TM;
  };

  char MipsExpandPseudo::ID = 0;
} // end of anonymous namespace

/// createMipsExpandPseudoPass
FunctionPass *llvm::createMipsExpandPseudoPass(MipsTargetMachine &tm) {
  return new MipsExpandPseudo(tm);
}

// loadimm %reg, $imm
// ->
// ldi $reg, $imm
// ldihi $imm
void MipsExpandPseudo::expandLoadImm(MachineBasicBlock &MBB,
                                     MachineBasicBlock::iterator I,
                                     const MipsInstrInfo *TII) const {
  unsigned DstReg = I->getOperand(0).getReg();

  if (I->getOperand(1).isImm()) {
    unsigned N      = I->getOperand(1).getImm();
    BuildMI(MBB, I, I->getDebugLoc(), TII->get(Mips::LDI), DstReg)
        .addImm(N & 0x3FFF);
    if (!isInt<14>(N)) {
      BuildMI(MBB, I, I->getDebugLoc(), TII->get(Mips::LDIHI))
        .addImm((N & 0xFFFFC000) >> 14);
    }
    return;
  }
  if (I->getOperand(1).isGlobal()) {
    BuildMI(MBB, I, I->getDebugLoc(), TII->get(Mips::LDI), DstReg)
        .addGlobalAddress(I->getOperand(1).getGlobal(), 0, MipsII::MO_ABS_LO);
    BuildMI(MBB, I, I->getDebugLoc(), TII->get(Mips::LDIHI))
        .addGlobalAddress(I->getOperand(1).getGlobal(), 0, MipsII::MO_ABS_HI);
    return;
  }
  if (I->getOperand(1).isCPI()) {
    BuildMI(MBB, I, I->getDebugLoc(), TII->get(Mips::LDI), DstReg)
        .addConstantPoolIndex(I->getOperand(1).getIndex(),
                              I->getOperand(1).getOffset(), MipsII::MO_ABS_LO);
    BuildMI(MBB, I, I->getDebugLoc(), TII->get(Mips::LDIHI))
        .addConstantPoolIndex(I->getOperand(1).getIndex(),
                              I->getOperand(1).getOffset(), MipsII::MO_ABS_HI);
    return;
  }
  if (I->getOperand(1).isJTI()) {
    BuildMI(MBB, I, I->getDebugLoc(), TII->get(Mips::LDI), DstReg)
        .addJumpTableIndex(I->getOperand(1).getIndex(), MipsII::MO_ABS_LO);
    BuildMI(MBB, I, I->getDebugLoc(), TII->get(Mips::LDIHI))
        .addJumpTableIndex(I->getOperand(1).getIndex(), MipsII::MO_ABS_HI);
    return;
  }
  llvm_unreachable("Unrecognized PSEUDO_LOAD_IMM operand!");
}

// ijmp_pseudo $imm, %reg, $imm
// ->
// ijmphi $imm, $reg, lo($imm)
// ijmp hi($imm)
void MipsExpandPseudo::expandIJmp(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator I,
                                  const MipsInstrInfo *TII) const {
  if (I->getOperand(0).isJTI()) {
    BuildMI(MBB, I, I->getDebugLoc(), TII->get(Mips::IJMPHI))
        .addJumpTableIndex(I->getOperand(0).getIndex(), MipsII::MO_IJMP_HI);
    BuildMI(MBB, I, I->getDebugLoc(), TII->get(Mips::IJMP))
        .addJumpTableIndex(I->getOperand(0).getIndex(), MipsII::MO_IJMP_LO)
        .addReg(I->getOperand(1).getReg())
        .addImm(I->getOperand(2).getImm());
    return;
  }
  llvm_unreachable("Unrecognized IJMP_PSEUDO operand!");
}

bool MipsExpandPseudo::runOnMachineFunction(MachineFunction &F) {
  const MipsInstrInfo *TII =
      static_cast<const MipsInstrInfo *>(TM.getSubtargetImpl()->getInstrInfo());

  const MipsSubtarget &STI = TM.getSubtarget<MipsSubtarget>();

  // Do the expansion.
  for (MachineFunction::iterator mbbi = F.begin(), mbbe = F.end();
       mbbi != mbbe; ++mbbi) {
    for (MachineBasicBlock::iterator mi = mbbi->begin(), me = mbbi->end();
         mi != me;) {
      MachineBasicBlock::iterator MI = mi;
      MachineBasicBlock *MBB = mbbi;
      // Advance iterator here because MI may be erased.
      ++mi;

      if (MI->getDesc().getOpcode() == Mips::LOAD_IMM_PSEUDO) {
        expandLoadImm(*MBB, MI, TII);
        MBB->erase(MI);
        ++ExpandPseudo;
        continue;
      }
      if (MI->getDesc().getOpcode() == Mips::IJMP_PSEUDO) {
        expandIJmp(*MBB, MI, TII);
        MBB->erase(MI);
        ++ExpandPseudo;
      }
    }
  }

  return true;
}
