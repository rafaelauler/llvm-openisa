//=== OiMachineModel.h - Convert Oi MCInst to LLVM IR -*- C++ -*-==//
// 
// A MCInstPrinter subclass that specializes in executing OpenISA 
// MC Instructions 
//
//===------------------------------------------------------------===//

#ifndef OIMACHINEMODEL_H
#define OIMACHINEMODEL_H
#include "OiMemoryModel.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/Object/ObjectFile.h"
#include "../lib/Target/Mips/InstPrinter/MipsInstPrinter.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"

namespace llvm {

namespace object{
class ObjectFile;
}

using namespace object;

class OiMachineModel {
  const MCAsmInfo &MAI;
  const MCInstrInfo &MII;
  const MCRegisterInfo &MRI;
  MCInstPrinter &IP;
public:
  OiMemoryModel *Mem;

  OiMachineModel(const MCAsmInfo &MAI, const MCInstrInfo &MII,
                 const MCRegisterInfo &MRI, OiMemoryModel *Mem,
                 MCInstPrinter &IP) 
    : MAI(MAI), MII(MII), MRI(MRI), IP(IP), Mem(Mem)
  {
    for (int i = 0; i < 32; ++i) {
      Bank[i] = 0;
    }
    Hi = 0;
    Lo = 0;
    for (int i = 0; i < 16; ++i) {
      DblBank[i] = 0.0;
    }    
  }

  void ConfigureUserLevelStack(int argc, char **argv);
  uint64_t executeInstruction(const MCInst *MI, uint64_t CurPC);
  void StartFunction(StringRef &N);
  void FinishFunction();
  void FinishModule();
  void UpdateCurAddr(uint64_t val) {
  }
  void SetCurSection(section_iterator *i) {
  }

  uint32_t Bank[32];
  uint32_t Hi, Lo, FCC;
  double   DblBank[16];

private:

  uint32_t HandleAluSrcOperand(const MCOperand &o);
  uint32_t HandleAluDstOperand(const MCOperand &o);
  uint32_t HandleMemExpr(const MCExpr &exp, bool IsLoad);
  uint32_t HandleSpilledOperand(const MCOperand &o, const MCOperand &o2,
                            bool IsLoad);
  uint32_t HandleGetSpilledAddress(const MCOperand &o, const MCOperand &o2,
                         const MCOperand &dst);
  uint32_t* HandleMemOperand(const MCOperand &o, const MCOperand &o2);
  double HandleDoubleLoadOperand(const MCOperand &o, const MCOperand &o2);
  void HandleDoubleSaveOperand(const MCOperand &o, const MCOperand &o2,
                               double val);
  uint32_t HandleFloatMemOperand(const MCOperand &o, const MCOperand &o2,
                             bool IsLoad);
  uint32_t HandleLUiOperand(const MCOperand &o);
  uint32_t HandleCallTarget(const MCOperand &o);
  bool HandleFCmpOperand(const MCOperand &o, double o0, double o1);
  uint32_t HandleBranchTarget(const MCOperand &o, bool IsRelative,
                              uint64_t CurPC);
  uint32_t HandleSaveDouble(Value *In, Value *&Out1, Value *&Out2);
  double HandleDoubleSrcOperand(const MCOperand &o);
  uint32_t HandleDoubleDstOperand(const MCOperand &o);
  uint32_t HandleSaveFloat(Value *In, Value *&V);
  uint32_t HandleFloatSrcOperand(const MCOperand &o);
  uint32_t HandleFloatDstOperand(const MCOperand &o);


  void printOperand(const MCInst *MI, unsigned OpNo, raw_ostream &O);
  void printUnsignedImm(const MCInst *MI, int opNum, raw_ostream &O);
  void printMemOperand(const MCInst *MI, int opNum, raw_ostream &O);
  void printMemOperandEA(const MCInst *MI, int opNum, raw_ostream &O);
  void printFCCOperand(const MCInst *MI, int opNum, raw_ostream &O);
};
} // end namespace llvm

#endif
