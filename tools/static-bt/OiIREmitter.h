//=== OiIREmitter.h - ------------------------------------------ -*- C++ -*-==//
//
// This class helps building LLVM I.R. that represents a piece of statically
// translated OpenISA code.
//
//===----------------------------------------------------------------------===//
#ifndef OIIREMITTER_H
#define OIIREMITTER_H

#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/ADT/IndexedMap.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include <set>
#include <unordered_set>
#include <vector>

namespace llvm {

extern cl::opt<bool> NoLocals;
extern cl::opt<bool> OneRegion;
extern cl::opt<bool> OptimizeStack;
extern cl::opt<bool> AggrOptimizeStack;
extern cl::opt<bool> NoShadow;

namespace object {
class ObjectFile;
}

using namespace object;

class OiIREmitter {
public:
  typedef DenseMap<uint32_t, Value *> SpilledRegsTy;
  typedef std::map<uint32_t, std::vector<uint32_t>> FunctionCallMapTy;
  typedef DenseMap<uint32_t, uint32_t> FunctionRetMapTy;

  class IndirectJumpEntry {
  public:
    IndirectJumpEntry(Instruction *Ins, uint64_t Addr, Value *Idx, uint64_t JT,
                      uint32_t Cnt)
        : Ins(Ins), InsAddress(Addr), Index(Idx), JTAddress(JT), JTCount(Cnt) {}

    Instruction *Ins;
    uint64_t InsAddress;
    Value *Index;
    uint64_t JTAddress;
    uint32_t JTCount;
  };

  OiIREmitter(const ObjectFile *obj, uint64_t Stacksz, StringRef CodeTarget)
      : Obj(obj), TheModule(new Module("outputtest", getGlobalContext())),
        CodeTarget(CodeTarget), Builder(getGlobalContext()),
        Regs(SmallVector<Value *, 259>(259)),
        GlobalRegs(SmallVector<Value *, 259>(259)),
        DblRegs(SmallVector<Value *, 64>(64)),
        DblGlobalRegs(SmallVector<Value *, 64>(64)), SpilledRegs(),
        FirstFunction(true), EntryPointBB(nullptr), CurAddr(0),
        CurSection(nullptr), BBMap(), InsMap(), ReadMap(), WriteMap(),
        DblReadMap(), DblWriteMap(), FunctionCallMap(), FunctionRetMap(),
        CurFunAddr(0), MainFunAddr(0), CurBlockAddr(0), StackSize(Stacksz),
        ReturnAddressesTableValue(nullptr), IndirectDestinations(),
        IndirectDestinationsAddrs(), IndirectJumps(), IndirectCalls(),
        ComdatSymbols() {
    BuildShadowImage();
    BuildRegisterFile();
    if (CodeTarget == "arm") {
      TheModule->setTargetTriple("armv4t--linux-eabi");
      TheModule->setDataLayout(
          "e-m:e-p:32:32-i64:64-v128:64:128-a:0:32-n32-S64");
    } else { // i386 data layout
      TheModule->setTargetTriple("i386-unknown-linux-gnu");
      TheModule->setDataLayout(
          "e-m:e-p:32:32-f64:32:64-f80:32-n8:16:32-S128");
    }
  }
  const ObjectFile *Obj;
  std::unique_ptr<Module> TheModule;
  StringRef CodeTarget;
  IRBuilder<> Builder;
  std::vector<uint8_t> ShadowImage;
  SmallVector<Value *, 259> Regs, GlobalRegs;
  SmallVector<Value *, 64> DblRegs, DblGlobalRegs;
  SpilledRegsTy SpilledRegs;
  bool FirstFunction;
  BasicBlock *EntryPointBB;
  uint64_t CurAddr;
  const SectionRef *CurSection;
  StringMap<BasicBlock *> BBMap;
  DenseMap<int64_t, Instruction *> InsMap;
  DenseMap<int32_t, bool> ReadMap, WriteMap, DblReadMap, DblWriteMap;
  FunctionCallMapTy FunctionCallMap; // Used only in one-region mode
  FunctionRetMapTy FunctionRetMap;   // Used only in one-region mode
  uint64_t CurFunAddr;
  uint64_t MainFunAddr;
  uint64_t CurBlockAddr;
  uint64_t StackSize;
  uint64_t ShadowSize;
  Value *ShadowImageValue;
  Value *IndirectJumpTableValue;
  Value *IndirectCallTableValue;
  Value *ReturnAddressesTableValue;
  std::vector<BasicBlock *> IndirectDestinations;
  std::vector<uint32_t> IndirectDestinationsAddrs;
  std::vector<IndirectJumpEntry> IndirectJumps;
  std::vector<std::pair<Instruction *, uint64_t>> IndirectCalls;
  std::vector<Value *> IndirectCallsIndexes;
  llvm::StringMap<uint64_t> ComdatSymbols;

  std::vector<uint64_t> FunctionAddrs;
  std::vector<BasicBlock *> FunctionBBs;
  // Properties of the hash function used in indirect calls
  struct HashParams {
    unsigned A;
    unsigned B;
    unsigned C;
    unsigned P;
    unsigned M;
  };
  HashParams IndirectCallsHash;
  HashParams ReturnAddressesHash;
  HashParams IndirectJumpsHash;

  void AddIndirectJump(Instruction *Ins, Value *Idx, uint64_t JT = 0,
                       uint32_t Count = 0) {
    IndirectJumps.emplace_back(IndirectJumpEntry(Ins, CurAddr, Idx, JT, Count));
  }
  void AddIndirectCall(Instruction *Ins, Value *Idx) {
    IndirectCalls.push_back(std::make_pair(Ins, CurAddr));
    IndirectCallsIndexes.push_back(Idx);
  }
  bool ExtractJumpTargets(uint64_t JT,
                          const std::unordered_set<uint64_t> &ValidPtrs,
                          ArrayRef<uint64_t> Funcs, uint64_t FuncAddr,
                          std::vector<BasicBlock *> &JumpTargets,
                          uint32_t Count);
  bool ProcessIndirectJumps();
  template <typename T>
  Value *CreateHashTableFor(ArrayRef<T> Addrs, const HashParams &Hash);
  template <typename T> HashParams SelectHashFunctionFor(ArrayRef<T> Addrs);
  void BuildShadowImage();
  void UpdateShadowImage();
  void BuildRegisterFile();
  void BuildLocalRegisterFile();
  bool HandleBackEdge(uint64_t Addr, BasicBlock *&Target);
  bool HandleIndirectCallOneRegion(uint64_t Addr, Value *src,
                                   Value **First = 0);
  bool HandleLocalCallOneRegion(uint64_t Addr, Value *&V, Value **First = 0);
  std::vector<uint32_t> GetCallSitesFor(uint32_t FuncAddr);
  void BuildReturnAddressesHash();
  bool BuildReturnTablesOneRegion();
  bool HandleLocalCall(uint64_t Addr, Value *&V, Value **First = 0);
  Value *HandleGetFunctionAddr(uint64_t Addr);
  Value *AccessSpillMemory(unsigned Idx, bool IsLoad);
  Value *AccessShadowMemory(Value *Idx, bool IsLoad, int width = 32,
                            bool isFloat = false, Value **First = 0);
  Value *AccessJumpTable(Value *Idx, Value **First = 0);
  Value *AccessHashTable(Value *Idx, Value **First, const HashParams &Hash,
                         Value *TableBasePtr);
  void InsertStartupCode(uint64_t Addr);
  BasicBlock *CreateBB(uint64_t Addr = 0, Function *F = 0);
  void UpdateInsertPoint();
  void CleanRegs();
  void StartFunction(StringRef N, uint64_t Addr);
  void StartMainFunction(uint64_t Addr);
  void HandleFunctionEntryPoint(Value **First = 0);
  void HandleFunctionExitPoint(Value **First = 0);
  void FixEntryBB();
  void FixBBTerminators();
  void FixEntryPoint();
  void UpdateCurAddr(uint64_t val) {
    CurAddr = val;
    UpdateInsertPoint();
  }
  void SetCurSection(const SectionRef *I) { CurSection = I; }

private:
  bool FindSectionOffset(StringRef Name, uint64_t &SectionAddr);
};
}

#endif
