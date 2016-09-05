//=== OiIREmitter.cpp - ---------------------------------------- -*- C++ -*-==//
//
// This class helps building LLVM I.R. that represents a piece of statically
// translated OpenISA code.
//
//===----------------------------------------------------------------------===//

#include "../lib/Target/Mips/MipsInstrInfo.h"
#include "OiIREmitter.h"
#include "StringRefMemoryObject.h"
#include "SBTUtils.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Object/ELF.h"
#include <algorithm>
#include <system_error>
#include <cstdlib>
using namespace llvm;

#define NDEBUG

namespace llvm {

cl::opt<bool> AbiLocals(
    "abi-locals",
    cl::desc("Reduce the pool of registers synced in region transitions"));

cl::opt<bool>
    NoLocals("nolocals",
             cl::desc("Do not use locals, always use global variables"));

cl::opt<bool>
    OneRegion("oneregion",
              cl::desc("Consider the whole program to be one big function"));

cl::opt<bool> OptimizeStack("optstack",
                            cl::desc("Optimize stack vars by considering some "
                                     "accesses as locals [experimental]"));

cl::opt<bool> AggrOptimizeStack(
    "aoptstack", cl::desc("Aggressively optimize stack vars by considering all "
                          "accesses as locals [experimental]"));

cl::opt<bool> NoShadow(
    "noshadow",
    cl::desc("Avoid adding shadowimage offset to every memory access"));
}

bool OiIREmitter::FindSectionOffset(StringRef Name, uint64_t &SectionAddr) {
  std::error_code ec;
  // Find through all sections relocations against the .text section
  for (auto &i : Obj->sections()) {
    if (error(ec))
      return false;
    StringRef CurName;
    if (error(i.getName(CurName)))
      return false;
    if (CurName != Name)
      continue;

    SectionAddr = i.getAddress();

    // Relocatable file
    if (SectionAddr == 0)
      SectionAddr = GetELFOffset(i);

    return true;
  }
  return false;
}

// Traverses the BB backwards looking for the instruction that writes
// to "Reg".
static bool FindDefiningInstr(const BasicBlock *BB,
                              BasicBlock::const_reverse_iterator &Iter,
                              const Value *LoadInstr,
                              const Value *&DefInstr) {
  auto *Load = dyn_cast<LoadInst>(LoadInstr);
  if (Load == nullptr)
    return false;
  const Value *Reg = Load->getPointerOperand();

  ++Iter;
  for (auto I = Iter, E = BB->rend(); I != E; ++I) {
    if (auto Store = dyn_cast<StoreInst>(&*I)) {
      if (Store->getPointerOperand() == Reg) {
        DefInstr = Store->getValueOperand();
        Iter = I;
        return true;
      }
    }
  }
  return false;
}

static bool CompareFragments(const Value* LHS, const Value *RHS) {
  if (LHS == RHS)
    return true;

  //Check for stack access
  auto L1 = dyn_cast<BitCastInst>(LHS);
  auto R1 = dyn_cast<BitCastInst>(RHS);
  if (!L1 || !R1)
    return false;

  auto L2 = dyn_cast<GetElementPtrInst>(L1->getOperand(0));
  auto R2 = dyn_cast<GetElementPtrInst>(R1->getOperand(0));
  if (!L2 || !R2) {
    return false;
  }

  assert(L2->getNumIndices() == 2 && R2->getNumIndices() == 2);
  auto OpIter1 = L2->idx_begin();
  auto OpIter2 = R2->idx_begin();
  auto L3 = (++OpIter1)->get();
  auto R3 = (++OpIter2)->get();

  Value *L4 = nullptr, *R4 = nullptr;
  ConstantInt *C1, *C2;
  if (!PatternMatch::match(L3, PatternMatch::m_Add(PatternMatch::m_Value(L4),
                                                   PatternMatch::m_ConstantInt(C1)))) {
    return false;
  }
  if (!PatternMatch::match(R3, PatternMatch::m_Add(PatternMatch::m_Value(R4),
                                                   PatternMatch::m_ConstantInt(C2)))){
    return false;
  }
  auto *L5 = dyn_cast<LoadInst>(L4);
  auto *R5 = dyn_cast<LoadInst>(R4);
  if (L4 && R4 && C1->getLimitedValue() == C2->getLimitedValue() &&
      L5->getPointerOperand() == R5->getPointerOperand())
    return true;
  return false;
}

static bool FindReachingDefAux(const BasicBlock *BB, const Value *Reg,
                               const Value *&DefInstr, int Depth) {
  if (Depth > 9)
    return false;

  for (auto I = BB->rbegin(), E = BB->rend(); I != E; ++I) {
    if (auto Store = dyn_cast<StoreInst>(&*I)) {
      if (CompareFragments(Store->getPointerOperand(), Reg)) {
        if (auto L = dyn_cast<LoadInst>(Store->getValueOperand())) {
          Reg = L->getPointerOperand();
          continue;
        }
        DefInstr = Store->getValueOperand();
        Store->dump();
        return true;
      }
    }
  }

  for (const_pred_iterator PI = pred_begin(BB), E = pred_end(BB); PI != E;
       ++PI) {
    if (FindReachingDefAux(*PI, Reg, DefInstr, Depth + 1))
      return true;
  }

  return false;
}

// Check for a reaching definition in the loaded value of LoadInstr and
// return the first one found via DFS.
// Only look in 5 BBs of distance.
static bool FindReachingDef(const BasicBlock *BB,
                            BasicBlock::const_reverse_iterator Iter,
                            const Value *LoadInstr,
                            const Value *&DefInstr) {
  auto *Load = dyn_cast<LoadInst>(LoadInstr);
  if (Load == nullptr)
    return false;
  const Value *Reg = Load->getPointerOperand();
  ++Iter;
  for (auto I = Iter, E = BB->rend(); I != E; ++I) {
    if (auto Store = dyn_cast<StoreInst>(&*I)) {
      if (CompareFragments(Store->getPointerOperand(), Reg)) {
        if (auto L = dyn_cast<LoadInst>(Store->getValueOperand())) {
          Reg = L->getPointerOperand();
          continue;
        }
        DefInstr = Store->getValueOperand();
        Store->dump();
        return true;
      }
    }
  }

  for (const_pred_iterator PI = pred_begin(BB), E = pred_end(BB); PI != E;
       ++PI) {
    if (FindReachingDefAux(*PI, Reg, DefInstr, 0))
      return true;
  }
  return false;
}

// Matches an operand with the idiom used in indirect jumps,
// extracting the symbol storing the base of the jump table.
static bool MatchIndirectJumpTable(const Value *Operand, uint64_t &JT) {
  auto *Instr = dyn_cast<Instruction>(Operand);
  if (Instr == nullptr) {
    return false;
  }

  const BasicBlock *BB = Instr->getParent();
  BasicBlock::const_reverse_iterator It = BB->rend();
  for (auto I = BB->rbegin(), E = BB->rend(); I != E; ++I) {
    if (&*I == Instr)
      It = I;
  }
  assert(It != BB->rend() && "Instruction not found in parent BB!");

  const Value *DefInstr = nullptr;
  if (!FindDefiningInstr(BB, It, Operand, DefInstr)) {
    return false;
  }

  auto *Load = dyn_cast<LoadInst>(DefInstr);
  if (Load == nullptr) {
    return false;
  }

  Value *Op = nullptr;
  if (!PatternMatch::match(Load->getPointerOperand(),
                           PatternMatch::m_BitCast(PatternMatch::m_Value(Op)))) {
    return false;
  }

  auto *GEP = dyn_cast<GetElementPtrInst>(Op);
  if (GEP == nullptr) {
    return false;
  }

  assert(GEP->getNumIndices() == 2 &&
         "GEP instruction with unexpected number of indices");
  auto OpIter = GEP->idx_begin();
  Value *AddInstr = (++OpIter)->get();
  const Value *AddInstr2 = nullptr;
  Value *LHS = nullptr, *RHS = nullptr;

  if (!PatternMatch::match(
          AddInstr, PatternMatch::m_Add(PatternMatch::m_Value(LHS),
                                        PatternMatch::m_ConstantInt<0>())) ||
      !FindDefiningInstr(BB, It, LHS, AddInstr2) ||
      !PatternMatch::match(AddInstr2,
                           PatternMatch::m_Add(PatternMatch::m_Value(LHS),
                                               PatternMatch::m_Value(RHS)))) {
    return false;
  }

  const Value *LHSDef, *RHSDef;
  if (!FindReachingDef(BB, It, LHS, LHSDef)) {
    return false;
  }

  if (!FindReachingDef(BB, It, RHS, RHSDef)) {
    return false;
  }

  auto ConstVal = dyn_cast<ConstantInt>(LHSDef);
  if (ConstVal == nullptr) {
    if (!(ConstVal = dyn_cast<ConstantInt>(RHSDef))) {
      return false;
    }
  }
  JT = ConstVal->getLimitedValue();
  return true;
}

static uint64_t GetFuncAddr(ArrayRef<uint64_t> Funcs, uint64_t Addr) {
  auto upper = std::upper_bound(Funcs.begin(), Funcs.end(), Addr);
  if (upper == Funcs.begin()) {
    llvm_unreachable("GetFuncAddr failed");
    return 0;
  }
  --upper;
  return *upper;
}

bool OiIREmitter::ExtractJumpTargets(
    uint64_t JT, const std::unordered_set<uint64_t> &ValidPtrs,
    ArrayRef<uint64_t> Funcs, uint64_t FuncAddr,
    std::vector<BasicBlock *> &JumpTargets, uint32_t Count) {
  for (uint64_t I = 0;; ++I) {
    uint32_t Candidate = *(const uint32_t *)(&ShadowImage[JT + (I << 2)]);
    if (ValidPtrs.count(Candidate) == 0)
      break;
    BasicBlock *BB = nullptr;
    if (!HandleBackEdge(Candidate, BB))
      llvm_unreachable("Failed to handle backedge");
    if (GetFuncAddr(Funcs, Candidate) != FuncAddr)
      break;
    if (Count != 0 && I > Count)
      break;
    JumpTargets.emplace_back(BB);
  }
  if (JumpTargets.size() > 0)
    return true;
  return false;
}

// PatchSection stores addr - value pairs to patch in a second pass.
class PatchSectionBuilder {
  std::vector<uint32_t> PatchSection;
  StructType *S1;
  std::vector<Constant *> PatchPairs;
  Module *TheModule;

public:
  PatchSectionBuilder(Module *M) : TheModule(M) { preparePatchSection(); }

  void preparePatchSection() {
    std::vector<Type *> S1Types;
    S1Types.push_back(Type::getInt32Ty(getGlobalContext()));
    S1Types.push_back(Type::getInt8PtrTy(getGlobalContext()));
    S1 = StructType::create(S1Types);
  }

  void addPair(uint32_t Addr, Constant *BB) {
    std::vector<Constant *> Elmts;
    Elmts.push_back(
        ConstantInt::get(Type::getInt32Ty(getGlobalContext()), Addr));
    if (isa<Function>(BB))
      Elmts.push_back(ConstantExpr::getPointerCast(BB, Type::getInt8PtrTy(getGlobalContext())));
    else
      Elmts.push_back(BB);
    PatchPairs.push_back(ConstantStruct::get(S1, Elmts));
  }

  GlobalVariable *finish() {
    ArrayType *AT = ArrayType::get(S1, PatchPairs.size());
    Constant *VAT = ConstantArray::get(AT, PatchPairs);
    std::vector<Type *> S2Types;
    S2Types.push_back(Type::getInt32Ty(getGlobalContext()));
    S2Types.push_back(AT);
    StructType *S2 = StructType::create(S2Types);
    std::vector<Constant *> Elmts;
    Elmts.push_back(ConstantInt::get(Type::getInt32Ty(getGlobalContext()),
                                     PatchPairs.size()));
    Elmts.push_back(VAT);
    Constant *CS = ConstantStruct::get(S2, Elmts);
    return new GlobalVariable(*TheModule, S2, false,
                              GlobalValue::ExternalLinkage, CS, "PatchSection");
  }
};

bool OiIREmitter::ProcessIndirectJumps() {
  //  uint64_t FinalAddr = 0xFFFFFFFFUL;
  std::error_code ec;
  // Store all code ptrs which we discover via relocations
  std::unordered_set<uint64_t> CodePtrs;
  uint64_t TextOffset;
  PatchSectionBuilder PSBuilder(&*TheModule);
  std::sort(FunctionAddrs.begin(), FunctionAddrs.end());

  if (!FindSectionOffset(".text", TextOffset))
    return false;

  // Find through all sections relocations against the .text section
  for (auto &i : Obj->sections()) {
    if (error(ec))
      break;
    uint64_t SectionAddr = i.getAddress();

    // Relocatable file
    if (SectionAddr == 0) {
      SectionAddr = GetELFOffset(i);
    }

    // Ignore PDR relocations
    StringRef name;
    if (error(i.getName(name)))
      continue;
    if (name.endswith("pdr") || !name.startswith(".rel."))
      continue;
    name = name.drop_front(4);
    uint64_t PatchedSecAddr;
    if (!FindSectionOffset(name, PatchedSecAddr))
      continue;

    for (auto &ri : i.relocations()) {
      if (error(ec))
        break;
      uint64_t Type;
      if (error(ri.getType(Type)))
        llvm_unreachable("Error getting relocation type");
      if (Type != ELF::R_MIPS_32)
        continue;
      const SymbolRef symb = *(ri.getSymbol());
      StringRef Name;
      if (error(symb.getName(Name))) {
        continue;
      }
      uint64_t offset;
      uint64_t TargetAddr = 0;
      if (error(ri.getOffset(offset)))
        break;
      // Check if this relocation maps to a symbol in .text -- skip it if not
      if (Name != ".text") {
        section_iterator seci = Obj->section_end();
        if (error(symb.getSection(seci)) || seci == Obj->section_end())
          continue;
        StringRef SecName;
        if (error(seci->getName(SecName)))
          continue;
        if (SecName != ".text")
          continue;
        if (error(symb.getAddress(TargetAddr)))
          break;
      }
      offset += PatchedSecAddr;
#ifndef NDEBUG
      outs() << "REL at " << format("%8" PRIx64, offset) << " Found ";
      outs() << "Contents:" << format("%8" PRIx64,
                                      (*(int *)(&ShadowImage[offset])));
#endif
      TargetAddr += *(uint32_t *)(&ShadowImage[offset]);
      TargetAddr += TextOffset;
      CodePtrs.insert(TargetAddr);
#ifndef NDEBUG
      outs() << " TargetAddr = " << format("%8" PRIx64, TargetAddr) << "\n";
#endif
      BasicBlock *BB = nullptr;
      if (!HandleBackEdge(TargetAddr, BB))
        llvm_unreachable("Failed to handle backedge");
      auto p = std::equal_range(FunctionAddrs.begin(), FunctionAddrs.end(),
                                TargetAddr);
      if (!OneRegion && p.first != p.second) {
        PSBuilder.addPair(offset, BB->getParent());
      } else {
        if (OneRegion && p.first != p.second) {
          IndFunctionAddrs.insert(*p.first);
        }
        PSBuilder.addPair(offset, BlockAddress::get(BB));
      }
      IndirectDestinations.push_back(BB);
      IndirectDestinationsAddrs.push_back(TargetAddr);
      // Patch ShadowImage with fixed address
      *(int *)(&ShadowImage[offset]) = TargetAddr;
    }
  }
  PSBuilder.finish();
  uint64_t TableSize = IndirectDestinations.size();
  uint32_t NumJumpsOK = 0;
  uint32_t NumJumpsWarning = 0;

  if (TableSize != 0) {
    for (const auto &IJE : IndirectJumps) {
      Instruction *Ins = IJE.Ins;
      uint64_t Addr = IJE.InsAddress;
      Value *first = IJE.Index;
      Builder.SetInsertPoint(Ins);
      uint64_t JT = IJE.JTAddress;
      uint64_t FuncAddr = GetFuncAddr(FunctionAddrs, Addr);
      if (JT != 0 || MatchIndirectJumpTable(first, JT)) {
        std::vector<BasicBlock *> JumpTargets;
        if (ExtractJumpTargets(JT, CodePtrs, FunctionAddrs, FuncAddr,
                               JumpTargets, IJE.JTCount)) {
          Value *v = nullptr;
          if (IJE.JTAddress != 0) {
            auto Sz = JumpTargets.size();
            assert(Sz > 1 && "Invalid jump table");
            SwitchInst *Swi = Builder.CreateSwitch(first, JumpTargets[Sz - 1],
                                                   Sz - 1);
            uint32_t CaseVal = 0;
            for (auto Dest : JumpTargets) {
              Swi->addCase(ConstantInt::get(
                               Type::getInt32Ty(getGlobalContext()), CaseVal),
                           Dest);
              CaseVal += 4;
              // Add all targets, except the last one, which was already added
              // as the default target
              if ((CaseVal >> 2) >= JumpTargets.size() - 1)
                break;
            }
            v = Swi;
          } else {
            IndirectBrInst *Ind = Builder.CreateIndirectBr(
                Builder.CreateIntToPtr(first,
                                       Type::getInt32PtrTy(getGlobalContext())),
                JumpTargets.size());
            std::set<BasicBlock *> JumpTargetsSet;
            JumpTargetsSet.insert(JumpTargets.begin(), JumpTargets.end());
            for (auto Dest : JumpTargetsSet) {
              Ind->addDestination(Dest);
            }
            v = Ind;
          }
          Ins->eraseFromParent();
          first = GetFirstInstruction(first, v);
          InsMap[Addr] = dyn_cast<Instruction>(first);
          ++NumJumpsOK;
          continue;
        }
      }
      if (NumJumpsWarning++ == 0)
        printf("WARNING: Failed to retrieve jump table base address for "
               "indirect jump. Assuming all targets in function.\n");
      dyn_cast<Instruction>(first)->getParent()->dump();

      IndirectBrInst *v = Builder.CreateIndirectBr(
          Builder.CreateIntToPtr(first,
                                  Type::getInt32PtrTy(getGlobalContext())),
          IndirectDestinations.size());
      for (int I = 0, E = IndirectDestinations.size(); I != E; ++I) {
        BasicBlock *targetBB = IndirectDestinations[I];
        uint64_t bbAddr = IndirectDestinationsAddrs[I];
        if (GetFuncAddr(FunctionAddrs, bbAddr) != FuncAddr)
          continue;
        v->addDestination(IndirectDestinations[I]);
        if (&targetBB->getParent()->getEntryBlock() == targetBB) {
          BasicBlock *NewEntry =
              BasicBlock::Create(getGlobalContext(), "newentry",
                                 targetBB->getParent(), targetBB);
          Builder.SetInsertPoint(NewEntry);
          Builder.CreateBr(targetBB);
        }
      }
      Ins->eraseFromParent();
      first = GetFirstInstruction(first, v);
      InsMap[Addr] = dyn_cast<Instruction>(first);
    }
  }

  printf("INFO: Processed %d indirect jumps: %d warnings.\n",
         NumJumpsOK + NumJumpsWarning, NumJumpsWarning);
  printf("INFO: Program uses %d indirect calls.\n", (int)IndirectCalls.size());
  printf("INFO: Program has %d indirect calls targets.\n",
         (int)IndFunctionAddrs.size());

  if (OneRegion && IndirectCalls.size() > 0) {
    assert(IndFunctionAddrs.size() > 0 &&
           "Indirect calls present, but no targets found");
    for (auto Addr : IndFunctionAddrs) {
      std::string Idx = Twine("bb").concat(Twine::utohexstr(Addr)).str();
      assert (BBMap[Idx] != 0 && "Missing basic block for function");
      // Populate FunctionBBs to be used when creating indirect calls later
      FunctionBBs.push_back(BBMap[Idx]);
    }
  }

  for (unsigned I = 0; I < IndirectCalls.size(); ++I) {
    auto C = IndirectCalls[I];
    Instruction *Ins = C.first;
    uint64_t Addr = C.second;
    Value *first = IndirectCallsIndexes[I];
    Builder.SetInsertPoint(Ins);
    if (OneRegion) {
      HandleIndirectCallOneRegion(Addr, first, &first);
      Ins->eraseFromParent();
      InsMap[Addr] = dyn_cast<Instruction>(first);
      continue;
    }
    Type *ft = PointerType::getUnqual(
        FunctionType::get(Type::getVoidTy(getGlobalContext()),
                          /*isvararg*/ false));
    Builder.CreateCall(Builder.CreatePointerCast(
        Builder.CreateIntToPtr(first, Type::getInt32PtrTy(getGlobalContext())),
        ft));
    Ins->eraseFromParent();
    InsMap[Addr] = dyn_cast<Instruction>(first);
  }

  return true;
}

void OiIREmitter::BuildShadowImage() {
  ShadowSize = 0;

  uint64_t TotalCommonsSize = 0;
  CommonSymbols = GetCommonSymbolsList(Obj, TotalCommonsSize);

  std::error_code ec;
  for (auto &i : Obj->sections()) {
    if (error(ec))
      break;

    uint64_t SectionAddr = i.getAddress();
    if (SectionAddr == 0)
      SectionAddr = GetELFOffset(i);
    uint64_t SectSize = i.getSize();
    if (SectSize + SectionAddr > ShadowSize)
      ShadowSize = SectSize + SectionAddr;
  }

  // Put our common symbols last
  uint64_t CommonSectionAddress = ShadowSize;
  ShadowSize += TotalCommonsSize;
  // Update Common symbols addresses
  for (auto &sym : CommonSymbols) {
    sym.setValue(sym.getValue() + CommonSectionAddress);
#ifndef NDEBUG
    outs() << "COMMON/BSS symbol \"" << sym.getKey() << "\" @"
           << format("%8" PRIx64, sym.getValue()) << "\n ";
#endif
  }

  // Allocate some space for the stack
  // ShadowSize += 10 << 20;
  ShadowSize += StackSize;
  ShadowImage.clear();
  ShadowImage.resize(ShadowSize);

  for (auto &i : Obj->sections()) {
    uint64_t SectionAddr = i.getAddress();
    uint64_t SectSize = i.getSize();
    StringRef SecName;
    if (error(i.getName(SecName)))
      continue;
    // Map only text and data sections
    if ((!i.isText() && !i.isData()) || i.isBSS())
      continue;

    uint64_t Offset = 0;
    if (SectionAddr == 0)
      Offset = GetELFOffset(i);

    StringRef Bytes;
    if (error(i.getContents(Bytes)))
      continue;;
    StringRefMemoryObject memoryObject(Bytes);
    memoryObject.readBytes(&ShadowImage[0] + SectionAddr + Offset, SectionAddr,
                           SectSize);
  }

  Constant *c = ConstantDataArray::get(
      getGlobalContext(),
      ArrayRef<uint8_t>(
          reinterpret_cast<const unsigned char *>(&ShadowImage[0]),
          ShadowSize));

  GlobalVariable *gv =
      new GlobalVariable(*TheModule, c->getType(), false,
                         GlobalValue::ExternalLinkage, c, "ShadowMemory");
  ShadowImageValue = gv;
}

void OiIREmitter::UpdateShadowImage() {
  Constant *c = ConstantDataArray::get(
      getGlobalContext(),
      ArrayRef<uint8_t>(
          reinterpret_cast<const unsigned char *>(&ShadowImage[0]),
          ShadowSize));

  dyn_cast<GlobalVariable>(ShadowImageValue)->setInitializer(c);
}

void OiIREmitter::BuildRegisterFile() {
  Type *ty = Type::getInt32Ty(getGlobalContext());
  Type *dblTy = Type::getDoubleTy(getGlobalContext());
  Type *fltTy = Type::getFloatTy(getGlobalContext());
  // 128 base regs  0-127
  // 128 float regs 128-255
  // LO 256
  // HI 257
  // FPCondCode 258
  for (int I = 1; I < 259; ++I) {
    std::string RegName = Twine("reg").concat(Twine(I)).str();
    Constant *ci;
    Type *myty;
    if (I < 128 || I > 255) {
      ci = ConstantInt::get(ty, 0U);
      myty = ty;
    } else {
      ci = ConstantFP::get(fltTy, 0.0f);
      myty = fltTy;
    }
    GlobalVariable *gv = new GlobalVariable(
        *TheModule, myty, false, GlobalValue::ExternalLinkage, ci, RegName);
    GlobalRegs[I] = gv;
  }
  for (int I = 0; I < 64; ++I) {
    Constant *ci = ConstantFP::get(dblTy, 0.0);
    GlobalVariable *gv = new GlobalVariable(
        *TheModule, dblTy, false, GlobalValue::ExternalLinkage, ci, "dblreg");
    DblGlobalRegs[I] = gv;
  }
}

void OiIREmitter::BuildLocalRegisterFile() {
  Type *ty = Type::getInt32Ty(getGlobalContext());
  Type *dblTy = Type::getDoubleTy(getGlobalContext());
  Type *fltTy = Type::getFloatTy(getGlobalContext());
  // 128 base regs  0-127
  // 128 float regs 128-255
  // LO 256
  // HI 257
  // FPCondCode 258
  if (NoLocals) {
    for (int I = 1; I < 259; ++I) {
      Regs[I] = GlobalRegs[I];
    }
    for (int I = 0; I < 32; ++I) {
      DblRegs[I] = DblGlobalRegs[I];
    }
  } else {
    for (int I = 1; I < 259; ++I) {
      std::string RegName = Twine("lreg").concat(Twine(I)).str();
      Type *myty;
      if (I < 128 || I > 255) {
        myty = ty;
      } else {
        myty = fltTy;
      }
      AllocaInst *inst = Builder.CreateAlloca(myty, 0, RegName);
      Regs[I] = inst;
      Builder.CreateStore(Builder.CreateLoad(GlobalRegs[I]), inst);
      WriteMap[I] = false;
      ReadMap[I] = false;
    }
    for (int I = 0; I < 32; ++I) {
      AllocaInst *inst = Builder.CreateAlloca(dblTy, 0, "ldblreg");
      DblRegs[I] = inst;
      Builder.CreateStore(Builder.CreateLoad(DblGlobalRegs[I]), inst);
      DblWriteMap[I] = false;
      DblReadMap[I] = false;
    }
  }
}

void OiIREmitter::StartFunction(StringRef N, uint64_t Addr) {
  FunctionType *FT;
  Function *F;
  FunctionAddrs.push_back(Addr);
  if (FirstFunction) {
    if (OneRegion) {
      SmallVector<Type *, 8> args(2, Type::getInt32Ty(getGlobalContext()));
      FT = FunctionType::get(Type::getVoidTy(getGlobalContext()), args,
                             /*isvararg*/ false);
      F = Function::Create(FT, Function::ExternalLinkage, "main", &*TheModule);
      BasicBlock *BBEntry =
        BasicBlock::Create(getGlobalContext(), "entrypoint", F, &*F->begin());
      Builder.SetInsertPoint(BBEntry);
      BuildLocalRegisterFile();
      // This BB terminator will be adjusted in FixEntryPoint()
    } else {
      FT = FunctionType::get(Type::getVoidTy(getGlobalContext()), false);
      F = reinterpret_cast<Function *>(TheModule->getOrInsertFunction(N, FT));
    }
    FirstFunction = false;
    BasicBlock *BB = CreateBB(Addr, F);
    CurBlockAddr = Addr;
    Builder.SetInsertPoint(BB);
    if (!OneRegion)
      BuildLocalRegisterFile();
    CurFunAddr = Addr;
  } else {
    CurFunAddr = CurAddr + GetInstructionSize();
    SpilledRegs.clear();
    if (!OneRegion) {
      FT = FunctionType::get(Type::getVoidTy(getGlobalContext()), false);
      F = reinterpret_cast<Function *>(TheModule->getOrInsertFunction(N, FT));
      // Create a function with no parameters
      BasicBlock *BB = CreateBB(CurAddr + GetInstructionSize(), F);
      CurBlockAddr = CurAddr + GetInstructionSize();
      Builder.SetInsertPoint(BB);
      BuildLocalRegisterFile();
    } else {
      CreateBB(CurAddr + GetInstructionSize());
    }
  }
}

void OiIREmitter::StartMainFunction(uint64_t Addr) {
  FunctionAddrs.push_back(Addr);
  MainFunAddr = Addr;
  if (FirstFunction) {
    SmallVector<Type *, 8> args(2, Type::getInt32Ty(getGlobalContext()));
    FunctionType *FT = FunctionType::get(Type::getVoidTy(getGlobalContext()),
                                         args, /*isvararg*/ false);
    Function *F = Function::Create(FT, Function::ExternalLinkage, "main", &*TheModule);
    FirstFunction = false;
    BasicBlock *BB = CreateBB(Addr, F);
    CurBlockAddr = Addr;
    Builder.SetInsertPoint(BB);
    BuildLocalRegisterFile();
    InsertStartupCode(Addr);
    CurFunAddr = Addr;
  } else {
    CurFunAddr = CurAddr + GetInstructionSize();
    SpilledRegs.clear();
    BasicBlock *BB;
    if (!OneRegion) {
      SmallVector<Type *, 8> args(2, Type::getInt32Ty(getGlobalContext()));
      FunctionType *FT = FunctionType::get(Type::getVoidTy(getGlobalContext()),
                                           args, /*isvararg*/ false);
      Function *F =
          Function::Create(FT, Function::ExternalLinkage, "main", &*TheModule);
      BB = CreateBB(CurAddr + GetInstructionSize(), F);
      CurBlockAddr = CurAddr + GetInstructionSize();
      Builder.SetInsertPoint(BB);
      BuildLocalRegisterFile();
    } else {
      BB = CreateBB(Addr);
      EntryPointBB = BB;
      Builder.SetInsertPoint(BB);
    }
    InsertStartupCode(Addr);
  }
}

void OiIREmitter::InsertStartupCode(uint64_t Addr) {
  Function *F = Builder.GetInsertBlock()->getParent();
  // Initialize the stack (aligned to 32 bytes)
  Value *size = ConstantInt::get(Type::getInt32Ty(getGlobalContext()),
                                 ShadowSize & 0xFFFFFFE0);
  if (NoShadow) {
    Value *shadow = Builder.CreatePtrToInt(
        ShadowImageValue, Type::getInt32Ty(getGlobalContext()));
    Value *fixedSize = Builder.CreateAdd(size, shadow);
    Builder.CreateStore(fixedSize, Regs[ConvToDirective(Mips::SP)]);
  } else {
    Builder.CreateStore(size, Regs[ConvToDirective(Mips::SP)]);
  }
  Function::arg_iterator args = F->arg_begin();
  Value *argc = args++;
  Value *argv = args++;
  Builder.CreateStore(argc, Regs[ConvToDirective(Mips::A0)]);
  if (NoShadow) {
    Builder.CreateStore(argv, Regs[ConvToDirective(Mips::A1)]);
  } else {
    Value *ptr = Builder.CreatePtrToInt(ShadowImageValue,
                                        Type::getInt32Ty(getGlobalContext()));
    Value *fixed = Builder.CreateSub(argv, ptr);
    Builder.CreateStore(fixed, Regs[ConvToDirective(Mips::A1)]);
    Value *iv = Builder.CreateAlloca(Type::getInt32Ty(getGlobalContext()));
    Value *zero = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0U);
    Value *two = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 2U);
    Value *one = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 1U);
    Builder.CreateStore(zero, iv);
    BasicBlock *bb1 = BasicBlock::Create(getGlobalContext(), "loopbody", F);
    BasicBlock *bb2 = BasicBlock::Create(getGlobalContext(), "loopexit", F);
    Builder.CreateBr(bb1);
    Builder.SetInsertPoint(bb1);
    Value *ivload = Builder.CreateLoad(iv);
    Value *ivshr = Builder.CreateShl(ivload, two);
    Value *argvsum = Builder.CreateAdd(argv, ivshr);
    Value *argvptr = Builder.CreateIntToPtr(
        argvsum, Type::getInt32PtrTy(getGlobalContext()));
    Value *elem = Builder.CreateLoad(argvptr);
    Value *elemfixed = Builder.CreateSub(elem, ptr);
    Builder.CreateStore(elemfixed, argvptr);
    Value *ivsum = Builder.CreateAdd(ivload, one);
    Builder.CreateStore(ivsum, iv);
    Value *cmp = Builder.CreateICmpNE(ivsum, argc);
    Builder.CreateCondBr(cmp, bb1, bb2);
    Builder.SetInsertPoint(bb2);
    BBMap[Twine("bb").concat(Twine::utohexstr(Addr)).str()] = bb2;
  }

  WriteMap[ConvToDirective(Mips::A0)] = true;
  WriteMap[ConvToDirective(Mips::A1)] = true;
}

BasicBlock *OiIREmitter::CreateBB(uint64_t Addr, Function *F) {
  if (Addr == 0)
    Addr = CurAddr;
  std::string Idx = Twine("bb").concat(Twine::utohexstr(Addr)).str();

  if (BBMap[Idx] == 0) {
    if (F == 0) {
      F = Builder.GetInsertBlock()->getParent();
    }
    BBMap[Idx] = BasicBlock::Create(getGlobalContext(), Idx, F);
  }
  return BBMap[Idx];
}

void OiIREmitter::UpdateInsertPoint() {
  std::string Idx = Twine("bb").concat(Twine::utohexstr(CurAddr)).str();

  if (BBMap[Idx] != 0) {
    if (Builder.GetInsertBlock() != BBMap[Idx]) {
      // First check if we need to add a fall-through terminator to the
      // current basic block
      BasicBlock *BB = Builder.GetInsertBlock();
      if (BB != 0) {
        if (BB->getTerminator() == 0) {
          Builder.CreateBr(BBMap[Idx]);
        }
      }
      CurBlockAddr = CurAddr;
      Builder.SetInsertPoint(BBMap[Idx]);
    }
  }
}

void OiIREmitter::HandleFunctionEntryPoint(Value **First) {
  bool WroteFirst = false;
  if (NoLocals)
    return;
  if (AbiLocals) {
    for (unsigned I = ConvToDirective(Mips::V0); I <= ConvToDirective(Mips::V1);
         ++I) {
      Value *ld = Builder.CreateLoad(GlobalRegs[I]);
      Builder.CreateStore(ld, Regs[I]);
      if (!WroteFirst) {
        WroteFirst = true;
        if (First)
          *First = GetFirstInstruction(*First, ld);
      }
    }
    Builder.CreateStore(
        Builder.CreateLoad(GlobalRegs[ConvToDirective(Mips::F0)]),
        Regs[ConvToDirective(Mips::F0)]);
    Builder.CreateStore(
        Builder.CreateLoad(DblGlobalRegs[ConvToDirectiveDbl(Mips::F0)]),
        DblRegs[ConvToDirectiveDbl(Mips::F0)]);
    return;
  }
  for (int I = 1; I < 259; ++I) {
    Value *ld = Builder.CreateLoad(GlobalRegs[I]);
    Builder.CreateStore(ld, Regs[I]);
    if (!WroteFirst) {
      WroteFirst = true;
      if (First)
        *First = GetFirstInstruction(*First, ld);
    }
  }
  for (int I = 0; I < 32; ++I) {
    Builder.CreateStore(Builder.CreateLoad(DblGlobalRegs[I]), DblRegs[I]);
  }
}

void OiIREmitter::HandleFunctionExitPoint(uint32_t Count, Value **First) {
  bool WroteFirst = false;
  if (NoLocals)
    return;
  if (AbiLocals) {
    // Only save registers that pass useful information from one function to the
    // other.
    uint32_t ArgsSaved = 0;
    uint32_t IntRegCount = Count;
    if (Count >= 2)
      IntRegCount = 4; // Some parameters may be double, which shadows int regs
    for (unsigned I = ConvToDirective(Mips::A0); I <= ConvToDirective(Mips::A3);
         ++I) {
      if (++ArgsSaved > IntRegCount)
        break;
      Value *ld = Builder.CreateLoad(Regs[I]);
      Builder.CreateStore(ld, GlobalRegs[I]);
      if (!WroteFirst) {
        WroteFirst = true;
        if (First)
          *First = GetFirstInstruction(*First, ld);
      }
    }
    Value *LdIns = Builder.CreateLoad(Regs[ConvToDirective(Mips::T0)]);
    Builder.CreateStore(LdIns, GlobalRegs[ConvToDirective(Mips::T0)]);
    if (!WroteFirst) {
      WroteFirst = true;
      if (First)
        *First = GetFirstInstruction(*First, LdIns);
    }
    Builder.CreateStore(Builder.CreateLoad(Regs[ConvToDirective(Mips::T1)]),
                        GlobalRegs[ConvToDirective(Mips::T1)]);
    ArgsSaved = 0;
    for (unsigned I = ConvToDirective(Mips::F12);
         I <= ConvToDirective(Mips::F15); ++I) {
      if (++ArgsSaved > IntRegCount)
        break;
      Builder.CreateStore(Builder.CreateLoad(Regs[I]), GlobalRegs[I]);
    }
    ArgsSaved = 0;
    for (unsigned I = ConvToDirectiveDbl(Mips::D6);
         I < ConvToDirectiveDbl(Mips::D8); I += 1) {
      if (++ArgsSaved > Count)
        break;
      Builder.CreateStore(Builder.CreateLoad(DblRegs[I]), DblGlobalRegs[I]);
    }
    for (unsigned I = ConvToDirective(Mips::V0); I <= ConvToDirective(Mips::V1);
         ++I) {
      Builder.CreateStore(Builder.CreateLoad(Regs[I]), GlobalRegs[I]);
    }
    Builder.CreateStore(Builder.CreateLoad(Regs[ConvToDirective(Mips::SP)]),
                        GlobalRegs[ConvToDirective(Mips::SP)]);
    Builder.CreateStore(Builder.CreateLoad(Regs[ConvToDirective(Mips::F0)]),
                        GlobalRegs[ConvToDirective(Mips::F0)]);
    Builder.CreateStore(
        Builder.CreateLoad(DblRegs[ConvToDirectiveDbl(Mips::F0)]),
        DblGlobalRegs[ConvToDirectiveDbl(Mips::F0)]);
    Builder.CreateStore(
        Builder.CreateLoad(DblRegs[ConvToDirectiveDbl(Mips::D1)]),
        DblGlobalRegs[ConvToDirectiveDbl(Mips::D1)]);
    return;
  }
  for (int I = 1; I < 259; ++I) {
    Value *ld = Builder.CreateLoad(Regs[I]);
    Builder.CreateStore(ld, GlobalRegs[I]);
    if (!WroteFirst) {
      WroteFirst = true;
      if (First)
        *First = GetFirstInstruction(*First, ld);
    }
  }
  for (int I = 0; I < 32; I += 1) {
    Builder.CreateStore(Builder.CreateLoad(DblRegs[I]), DblGlobalRegs[I]);
  }
}

void OiIREmitter::FixEntryBB() {
  BasicBlock *BB = &Builder.GetInsertBlock()->getParent()->getEntryBlock();
  assert (BB != nullptr && "Cannot find entry basic block");
  // If entry BB already has no predecessors, we're done
  if (pred_begin(BB) == pred_end(BB))
    return;
  BasicBlock *NewEntry = BasicBlock::Create(
      getGlobalContext(), "newentry", Builder.GetInsertBlock()->getParent(), BB);
  Builder.SetInsertPoint(NewEntry);
  Builder.CreateBr(BB);
}

void OiIREmitter::FixBBTerminators() {
  Function *F = Builder.GetInsertBlock()->getParent();
  std::vector<BasicBlock *> ToDelete;

  for (Function::iterator I = F->begin(), E = F->end(); I != E; ++I) {
    if (!I->getTerminator()) {
      if (!I->empty()) {
        Builder.SetInsertPoint(&*I);
        Builder.CreateUnreachable();
      } else {
        // Empty basic block
        BBMap[I->getName()] = 0;
        ToDelete.push_back(&*I);
      }
    }
  }
  for (std::vector<BasicBlock *>::iterator I = ToDelete.begin(),
                                           E = ToDelete.end();
       I != E; ++I) {
    (*I)->eraseFromParent();
  }
}

void OiIREmitter::FixEntryPoint() {
  if (OneRegion && EntryPointBB != nullptr) {
    Function *F = Builder.GetInsertBlock()->getParent();
    Builder.SetInsertPoint(&*F->begin());
    Builder.CreateBr(EntryPointBB);
  }
}

// Note: CleanRegs() leaves a few remaining "Load GlobalRegXX" after the
// cleanup, but a simple SSA dead code elimination should handle them.
void OiIREmitter::CleanRegs() {
  for (int I = 1; I < 259; ++I) {
    if (!(WriteMap[I] || ReadMap[I])) {
      Instruction *inst = dyn_cast<Instruction>(Regs[I]);
      if (inst) {
        while (!inst->use_empty()) {
          Instruction *UI = inst->user_back();
          // These are assigning a value to the local copy of the reg, bu since
          // we don't use it, we can delete the assignment.
          if (isa<StoreInst>(UI)) {
            UI->eraseFromParent();
            continue;
          }
          assert(isa<LoadInst>(UI));
          // Here we should have a false usage of the value. It is loading only
          // in checkpoints (exit points) to save it back to the global copy.
          // Since we do not really use it, we should delete the load and the
          // store insruction that is using it.
          assert(UI->hasOneUse());
          Instruction *StUI = dyn_cast<Instruction>(UI->user_back());
          assert(isa<StoreInst>(StUI));
          StUI->eraseFromParent();
          UI->eraseFromParent();
        }
        inst->eraseFromParent();
      }
    }
  }
  for (int I = 0; I < 32; ++I) {
    if (!(DblWriteMap[I] || DblReadMap[I])) {
      Instruction *inst = dyn_cast<Instruction>(DblRegs[I]);
      if (inst) {
        while (!inst->use_empty()) {
          Instruction *UI = inst->user_back();
          // These are assigning a value to the local copy of the reg, bu since
          // we don't use it, we can delete the assignment.
          if (isa<StoreInst>(UI)) {
            UI->eraseFromParent();
            continue;
          }
          assert(isa<LoadInst>(UI));
          // Here we should have a false usage of the value. It is loading only
          // in checkpoints (exit points) to save it back to the global copy.
          // Since we do not really use it, we should delete the load and the
          // store insruction that is using it.
          assert(UI->hasOneUse());
          Instruction *StUI = dyn_cast<Instruction>(UI->user_back());
          assert(isa<StoreInst>(StUI));
          StUI->eraseFromParent();
          UI->eraseFromParent();
        }
        inst->eraseFromParent();
      }
    }
  }
}

bool OiIREmitter::BuildReturns() {
  for (FunctionRetMapTy::iterator I = FunctionRetMap.begin(),
                                  E = FunctionRetMap.end();
       I != E; ++I) {
    uint32_t retaddr = I->first;
    uint32_t funcaddr = I->second;

    Instruction *tgtins = InsMap[retaddr];
    assert(tgtins && "Invalid return address");

    Builder.SetInsertPoint(tgtins->getParent(), tgtins);

    std::vector<uint32_t> CallSites = GetCallSitesFor(funcaddr);
    if (CallSites.empty())
      continue;

    Value *ra =
        Builder.CreateLoad(Regs[ConvToDirective(Mips::RA)], "RetTableInput");
    ReadMap[ConvToDirective(Mips::RA)] = true;

    Value *Target =
        Builder.CreateIntToPtr(ra, Type::getInt8PtrTy(getGlobalContext()));
    std::vector<BasicBlock *> CallSitesBBs;
    for (auto CallAddr : CallSites) {
      std::string Idx = Twine("bb").concat(Twine::utohexstr(CallAddr)).str();
      assert(BBMap[Idx] != 0 && "Invalid return target address");
      CallSitesBBs.push_back(BBMap[Idx]);
    }
    IndirectBrInst *v = Builder.CreateIndirectBr(Target, CallSitesBBs.size());
    for (auto CallBB : CallSitesBBs)
      v->addDestination(CallBB);
    // Delete the original ret instruction
    tgtins->eraseFromParent();
  }
  return true;
}

std::vector<uint32_t> OiIREmitter::GetCallSitesFor(uint32_t FuncAddr) {
  return FunctionCallMap[FuncAddr];
}

bool OiIREmitter::HandleBackEdge(uint64_t Addr, BasicBlock *&Target) {
  std::string Idx = Twine("bb").concat(Twine::utohexstr(Addr)).str();

  if (BBMap[Idx] != 0) {
    Target = BBMap[Idx];
    return true;
  }

  if (Addr == CurAddr) {
    Target = CreateBB(Addr);
    UpdateInsertPoint();
    return true;
  }

  Instruction *TgtIns = InsMap[Addr];
  while (TgtIns == 0 && Addr < CurAddr) {
    Addr += GetInstructionSize();
    TgtIns = InsMap[Addr];
  }

  assert(TgtIns && "Backedge out of range");
  //  assert(TgtIns->getParent()->getParent() ==
  //             Builder.GetInsertBlock()->getParent() &&
  //         "Backedge out of range");

  Idx = Twine("bb").concat(Twine::utohexstr(Addr)).str();
  if (BBMap[Idx] != 0) {
    Target = BBMap[Idx];
    return true;
  }
  BasicBlock *BB = TgtIns->getParent();
  BasicBlock::iterator I, E;
  for (I = BB->begin(), E = BB->end(); I != E; ++I) {
    if (&*I == TgtIns)
      break;
  }
  assert(I != E);
  if (BB->getTerminator()) {
    Target = BB->splitBasicBlock(I, Idx);
    BBMap[Idx] = Target;
    return true;
  }

  // Insert dummy terminator
  assert(Builder.GetInsertBlock() == BB && CurBlockAddr < Addr);
  Instruction *dummy = dyn_cast<Instruction>(Builder.CreateRetVoid());
  assert(dummy);
  Target = BB->splitBasicBlock(I, Idx);
  BBMap[Idx] = Target;
  CurBlockAddr = Addr;
  dummy->eraseFromParent();
  Builder.SetInsertPoint(Target, Target->end());

  return true;
}

bool OiIREmitter::HandleIndirectCallOneRegion(uint64_t Addr, Value *src,
                                              Value **First) {
  for (auto FnAddr : IndFunctionAddrs) {
    if (FnAddr != MainFunAddr)
      FunctionCallMap[FnAddr].push_back(Addr + GetInstructionSize());
  }
  BasicBlock *Ret = CreateBB(Addr + GetInstructionSize());
  Builder.CreateStore(
      ConstantExpr::getPtrToInt(BlockAddress::get(Ret),
                                Type::getInt32Ty(getGlobalContext())),
      Regs[ConvToDirective(Mips::RA)]);
  WriteMap[ConvToDirective(Mips::RA)] = true;
  IndirectBrInst *v = Builder.CreateIndirectBr(
      Builder.CreateIntToPtr(src, Type::getInt32PtrTy(getGlobalContext())),
      FunctionBBs.size());
  for (int I = 0, E = FunctionBBs.size(); I != E; ++I) {
    v->addDestination(FunctionBBs[I]);
  }
  if (First)
    *First = GetFirstInstruction(*First, src);
  return true;
}

bool OiIREmitter::HandleLocalCallOneRegion(uint64_t Addr, Value *&V,
                                           Value **First) {
  BasicBlock *Target;
  FunctionCallMap[Addr].push_back(CurAddr + GetInstructionSize());
  if (Addr < CurAddr)
    HandleBackEdge(Addr, Target);
  else
    Target = CreateBB(Addr);

  BasicBlock *Ret = CreateBB(CurAddr + GetInstructionSize());
  Value *first = Builder.CreateStore(
      ConstantExpr::getPointerCast(BlockAddress::get(Ret),
                                   Type::getInt32Ty(getGlobalContext())),
      Regs[ConvToDirective(Mips::RA)]);
  WriteMap[ConvToDirective(Mips::RA)] = true;
  V = Builder.CreateBr(Target);
  if (First)
    *First = GetFirstInstruction(*First, first);
  return true;
}

bool OiIREmitter::HandleLocalCall(uint64_t Addr, uint32_t Count, Value *&V,
                                  Value **First) {
  if (OneRegion)
    return HandleLocalCallOneRegion(Addr, V, First);

  std::string Name = Twine("a").concat(Twine::utohexstr(Addr)).str();
  StringRef NameRef(Name);
  HandleFunctionExitPoint(Count, First);
  FunctionType *ft = FunctionType::get(Type::getVoidTy(getGlobalContext()),
                                       /*isvararg*/ false);
  Value *fun = TheModule->getOrInsertFunction(NameRef, ft);
  V = Builder.CreateCall(fun);
  if (First && NoLocals)
    *First = GetFirstInstruction(*First, V);
  HandleFunctionEntryPoint();
  return true;
}

Value *OiIREmitter::HandleGetFunctionAddr(uint64_t Addr) {
  if (OneRegion) {
    IndFunctionAddrs.insert(Addr);
    BasicBlock *Target;
    if (Addr < CurAddr)
      HandleBackEdge(Addr, Target);
    else
      Target = CreateBB(Addr);
    return ConstantExpr::getPointerCast(BlockAddress::get(Target),
                                        Type::getInt32Ty(getGlobalContext()));
  }

  std::string Name = Twine("a").concat(Twine::utohexstr(Addr)).str();
  StringRef NameRef(Name);
  FunctionType *ft = FunctionType::get(Type::getVoidTy(getGlobalContext()),
                                       /*isvararg*/ false);
  return ConstantExpr::getPointerCast(
      TheModule->getOrInsertFunction(NameRef, ft),
      Type::getInt32Ty(getGlobalContext()));
}

Value *OiIREmitter::AccessSpillMemory(unsigned Idx, bool IsLoad) {
  Value *ptr = SpilledRegs[Idx];
  if (!ptr) {
    Function *CurFun = Builder.GetInsertBlock()->getParent();
    IRBuilder<> Builder(&CurFun->getEntryBlock(),
                        CurFun->getEntryBlock().begin());
    ptr = Builder.CreateAlloca(Type::getInt32Ty(getGlobalContext()), 0,
                               StringRef("frame") + Twine(Idx));
    SpilledRegs[Idx] = ptr;
  }
  if (IsLoad)
    return Builder.CreateLoad(ptr);
  return ptr;
}

Value *OiIREmitter::AccessShadowMemory(Value *Idx, bool IsLoad, int width,
                                       bool isFloat, Value **First) {
  Type *targetType = 0;
  switch (width) {
  case 8:
    targetType = Type::getInt8PtrTy(getGlobalContext());
    break;
  case 16:
    targetType = Type::getInt16PtrTy(getGlobalContext());
    break;
  case 32:
    if (isFloat) {
      targetType = Type::getFloatPtrTy(getGlobalContext());
    } else {
      targetType = Type::getInt32PtrTy(getGlobalContext());
    }
    break;
  case 64:
    targetType = Type::getDoublePtrTy(getGlobalContext());
    break;
  default:
    llvm_unreachable("Invalid memory access width");
  }
  Value *ptr = 0;
  if (NoShadow) {
    ptr = Builder.CreateIntToPtr(Idx, targetType);
    if (First)
      *First = GetFirstInstruction(*First, ptr);
  } else {
    SmallVector<Value *, 4> Idxs;
    Idxs.push_back(ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0U));
    Idxs.push_back(Idx);
    Value *gep = Builder.CreateGEP(ShadowImageValue, Idxs);
    ptr = Builder.CreateBitCast(gep, targetType);
    if (First)
      *First = GetFirstInstruction(*First, gep, ptr);
  }
  if (IsLoad) {
    Value *load = Builder.CreateLoad(ptr);
    if (First)
      *First = GetFirstInstruction(*First, load);
    return load;
  }
  return ptr;
}

