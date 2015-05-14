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

namespace llvm {

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

bool OiIREmitter::ProcessIndirectJumps() {
  //  uint64_t FinalAddr = 0xFFFFFFFFUL;
  std::error_code ec;
  std::vector<Constant *> IndirectJumpTable;
  uint64_t TextOffset;
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
      if (Name != ".text")
        continue;

      uint64_t offset;
      if (error(ri.getOffset(offset)))
        break;
      offset += PatchedSecAddr;
      outs() << "REL at " << format("%8" PRIx64, offset) << " Found ";
      outs() << "Contents:" << format("%8" PRIx64,
                                      (*(int *)(&ShadowImage[offset])));
      uint64_t TargetAddr = *(int *)(&ShadowImage[offset]);
      TargetAddr += TextOffset;
      outs() << " TargetAddr = " << format("%8" PRIx64, TargetAddr) << "\n";
      BasicBlock *BB = nullptr;
      if (!HandleBackEdge(TargetAddr, BB))
        llvm_unreachable("Failed to handle backedge");
      IndirectJumpTable.push_back(BlockAddress::get(BB));
      IndirectDestinations.push_back(BB);
      IndirectDestinationsAddrs.push_back(TargetAddr);
      // Patch ShadowImage with fixed address
      *(int *)(&ShadowImage[offset]) = TargetAddr;
    }
  }
  uint64_t TableSize = IndirectJumpTable.size();

  if (TableSize == 0) {
    IndirectJumpTableValue = 0;
  } else {
    std::sort(IndirectDestinationsAddrs.begin(), IndirectDestinationsAddrs.end());
    IndirectDestinationsAddrs.erase(
        std::unique(IndirectDestinationsAddrs.begin(),
                    IndirectDestinationsAddrs.end()),
        IndirectDestinationsAddrs.end());
    IndirectJumpsHash =
        SelectHashFunctionFor<uint32_t>(IndirectDestinationsAddrs);
    printf("Selected hash function = (%d * (k - %d) + %d) %% %d %% %d \n",
           IndirectJumpsHash.A, IndirectJumpsHash.C, IndirectJumpsHash.B,
           IndirectJumpsHash.P, IndirectJumpsHash.M);
    IndirectJumpTableValue = CreateHashTableFor<uint32_t>(
        IndirectDestinationsAddrs, IndirectJumpsHash);

    for (unsigned I = 0; I < IndirectJumps.size(); ++I) {
      Instruction *Ins = IndirectJumps[I].first;
      uint64_t Addr = IndirectJumps[I].second;
      Value *first = IndirectJumpsIndexes[I];
      Builder.SetInsertPoint(Ins);
      Value *Target = AccessHashTable(first, &first, IndirectJumpsHash,
                                      IndirectJumpTableValue);
      IndirectBrInst *v =
          Builder.CreateIndirectBr(Target, IndirectDestinations.size());
      for (int I = 0, E = IndirectDestinations.size(); I != E; ++I) {
        BasicBlock *targetBB = IndirectDestinations[I];
        if (targetBB->getParent()->getName() !=
            Ins->getParent()->getParent()->getName())
          continue;
        v->addDestination(IndirectDestinations[I]);
      }
      Ins->eraseFromParent();
      first = GetFirstInstruction(first, v);
      InsMap[Addr] = dyn_cast<Instruction>(first);
    }
  }

  if (IndirectCalls.size() > 0) {
    IndirectCallsHash = SelectHashFunctionFor<uint64_t>(FunctionAddrs);
    printf("Selected hash function = (%d * (k - %d) + %d) %% %d %% %d \n",
           IndirectCallsHash.A, IndirectCallsHash.C, IndirectCallsHash.B,
           IndirectCallsHash.P, IndirectCallsHash.M);
    for (auto Addr : FunctionAddrs) {
      std::string Idx = Twine("bb").concat(Twine::utohexstr(Addr)).str();
      assert (BBMap[Idx] != 0 && "Missing basic block for function");
      // Populate FunctionBBs to be used when creating indirect calls later
      FunctionBBs.push_back(BBMap[Idx]);
    }
    IndirectCallTableValue =
        CreateHashTableFor<uint64_t>(FunctionAddrs, IndirectCallsHash);
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
    Value *Target = AccessHashTable(first, &first, IndirectCallsHash,
                                    IndirectCallTableValue);
    Type *ft = PointerType::getUnqual(
        FunctionType::get(Type::getVoidTy(getGlobalContext()),
                          /*isvararg*/ false));
    Builder.CreateCall(Builder.CreatePointerCast(Target, ft));
    Ins->eraseFromParent();
    InsMap[Addr] = dyn_cast<Instruction>(first);
  }

  return true;
}

void OiIREmitter::BuildShadowImage() {
  ShadowSize = 0;

  uint64_t TotalComdatSize = 0;
  ComdatSymbols = GetComdatSymbolsList(Obj, TotalComdatSize);

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

  // Put our comdat symbols last
  uint64_t ComdatSectionAddress = ShadowSize;
  ShadowSize += TotalComdatSize;
  // Update Comdat symbols addresses
  for (auto &sym : ComdatSymbols) {
    sym.setValue(sym.getValue() + ComdatSectionAddress);
    outs() << "COMDAT/BSS symbol \"" << sym.getKey() << "\" @"
           << format("%8" PRIx64, sym.getValue()) << "\n ";
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
    for (int I = 0; I < 64; ++I) {
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
    for (int I = 0; I < 64; ++I) {
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
  // Initialize the stack
  Value *size =
      ConstantInt::get(Type::getInt32Ty(getGlobalContext()), ShadowSize);
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
  for (int I = 1; I < 259; ++I) {
    Value *st = Builder.CreateStore(Builder.CreateLoad(GlobalRegs[I]), Regs[I]);
    if (!WroteFirst) {
      WroteFirst = true;
      if (First)
        *First = GetFirstInstruction(*First, st);
    }
  }
  for (int I = 0; I < 64; ++I) {
    Builder.CreateStore(Builder.CreateLoad(DblGlobalRegs[I]), DblRegs[I]);
  }
}

void OiIREmitter::HandleFunctionExitPoint(Value **First) {
  bool WroteFirst = false;
  if (NoLocals)
    return;
  for (int I = 1; I < 259; ++I) {
    Value *st = Builder.CreateStore(Builder.CreateLoad(Regs[I]), GlobalRegs[I]);
    if (!WroteFirst) {
      WroteFirst = true;
      if (First)
        *First = GetFirstInstruction(*First, st);
    }
  }
  for (int I = 0; I < 64; ++I) {
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
  for (int I = 0; I < 64; ++I) {
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

void OiIREmitter::BuildReturnAddressesHash() {
  std::vector<uint32_t> CallSites;
  for (auto &elmt : FunctionCallMap) {
    for (auto &addr : elmt.second) {
      bool found = false;
      for (auto &j : CallSites) {
        if (j == addr) {
          found = true;
          break;
        }
      }
      if (!found)
        CallSites.push_back(addr);
    }
  }
  HashParams Hash = SelectHashFunctionFor<uint32_t>(CallSites);
  printf("Selected hash function = (%d * (k - %d) + %d) %% %d %% %d \n",
         Hash.A, Hash.C, Hash.B, Hash.P, Hash.M);
  ReturnAddressesTableValue = CreateHashTableFor<uint32_t>(CallSites, Hash);
  ReturnAddressesHash = Hash;
}

bool OiIREmitter::BuildReturnTablesOneRegion() {
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
    Instruction *dummy = Builder.CreateUnreachable();
    Builder.SetInsertPoint(tgtins->getParent(), dummy);

    // Delete the original ret instruction
    tgtins->eraseFromParent();

    // Create a hash table
    if (CallSites.size() > 3) {
      if (ReturnAddressesTableValue == nullptr)
        BuildReturnAddressesHash();
      assert(ReturnAddressesTableValue != nullptr && "Missing hash table");
      Value *BasePtr = ReturnAddressesTableValue;
      Value *Target = AccessHashTable(ra, 0, ReturnAddressesHash, BasePtr);
      std::vector<BasicBlock *> CallSitesBBs;
      for (auto CallAddr : CallSites) {
        std::string Idx = Twine("bb").concat(Twine::utohexstr(CallAddr)).str();
        assert(BBMap[Idx] != 0 && "Invalid return target address");
        CallSitesBBs.push_back(BBMap[Idx]);
      }
      IndirectBrInst *v = Builder.CreateIndirectBr(Target, CallSitesBBs.size());
      for (auto CallBB : CallSitesBBs)
        v->addDestination(CallBB);
      dummy->eraseFromParent();
      continue;
    }

    for (std::vector<uint32_t>::iterator J = CallSites.begin(),
                                         EJ = CallSites.end();
         J != EJ; ++J) {
      Value *site = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), *J);
      Value *cmp = Builder.CreateICmpEQ(site, ra);

      std::string Idx = Twine("bb").concat(Twine::utohexstr(*J)).str();
      //  printf("\n%08X\n\n%s\n", *J,  Idx.c_str());
      assert(BBMap[Idx] != 0 && "Invalid return target address");
      Value *TrueV = BBMap[Idx];
      assert(isa<BasicBlock>(TrueV) &&
             "Values stored into BBMap must be BasicBlocks");
      BasicBlock *True = dyn_cast<BasicBlock>(TrueV);
      Function *F = True->getParent();
      BasicBlock *FallThrough = BasicBlock::Create(getGlobalContext(), "", F);

      Builder.CreateCondBr(cmp, True, FallThrough);
      Builder.SetInsertPoint(FallThrough);
    }
    Builder.CreateUnreachable();
    dummy->eraseFromParent();
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
  Value *f = nullptr;
  Value *Target =
      AccessHashTable(src, &f, IndirectCallsHash, IndirectCallTableValue);
  for (auto FnAddr : FunctionAddrs)
    FunctionCallMap[FnAddr].push_back(Addr + GetInstructionSize());
  Builder.CreateStore(ConstantInt::get(Type::getInt32Ty(getGlobalContext()),
                                       Addr + GetInstructionSize()),
                      Regs[ConvToDirective(Mips::RA)]);
  WriteMap[ConvToDirective(Mips::RA)] = true;
  IndirectBrInst *v =
      Builder.CreateIndirectBr(Target, FunctionBBs.size());
  for (int I = 0, E = FunctionBBs.size(); I != E; ++I) {
    v->addDestination(FunctionBBs[I]);
  }
  if (First)
    *First = GetFirstInstruction(*First, src, f);
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
  Value *first =
      Builder.CreateStore(ConstantInt::get(Type::getInt32Ty(getGlobalContext()),
                                           CurAddr + GetInstructionSize()),
                          Regs[ConvToDirective(Mips::RA)]);
  WriteMap[ConvToDirective(Mips::RA)] = true;
  V = Builder.CreateBr(Target);
  //  printf("\nHandleLocalCallOneregion.CurAddr: %08LX\n",
  //  CurAddr+GetInstructionSize());
  CreateBB(CurAddr + GetInstructionSize());
  if (First)
    *First = GetFirstInstruction(*First, first);
  return true;
}

bool OiIREmitter::HandleLocalCall(uint64_t Addr, Value *&V, Value **First) {
  if (OneRegion)
    return HandleLocalCallOneRegion(Addr, V, First);

  std::string Name = Twine("a").concat(Twine::utohexstr(Addr)).str();
  StringRef NameRef(Name);
  HandleFunctionExitPoint(First);
  FunctionType *ft = FunctionType::get(Type::getVoidTy(getGlobalContext()),
                                       /*isvararg*/ false);
  Value *fun = TheModule->getOrInsertFunction(NameRef, ft);
  V = Builder.CreateCall(fun);
  if (First && NoLocals)
    *First = GetFirstInstruction(*First, V);
  HandleFunctionEntryPoint();
  return true;
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

Value *OiIREmitter::AccessHashTable(Value *Idx, Value **First,
                                    const HashParams &Hash,
                                    Value *TableBasePtr) {
  SmallVector<Value *, 4> Idxs;
  Value *M = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), Hash.M);
  Value *P = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), Hash.P);
  Value *A = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), Hash.A);
  Value *B = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), Hash.B);
  Value *C = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), Hash.C);
  Value *Top = Builder.CreateSub(Idx, C);

  Value *Add = Builder.CreateAdd(
      Builder.CreatePtrToInt(TableBasePtr,
                             Type::getInt32Ty(getGlobalContext())),
      Builder.CreateShl(
          Builder.CreateURem(
              Builder.CreateURem(
                  Builder.CreateAdd(Builder.CreateMul(Top, A), B), P),
              M),
          ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 2)));

  Value *v =
      Builder.CreateIntToPtr(Builder.CreateLoad(Builder.CreateIntToPtr(
                                 Add, Type::getInt32PtrTy(getGlobalContext()))),
                             Type::getInt32PtrTy(getGlobalContext()));

  if (First)
    *First = GetFirstInstruction(*First, Top, Add);
  return v;
}

template <typename T>
OiIREmitter::HashParams OiIREmitter::SelectHashFunctionFor(ArrayRef<T> Addrs) {
  HashParams Res;
  const int NumFuncs = Addrs.size();
  // We want to hash the addresses in functionaddrs with a function:
  //   hash(k) = ((a(k - c) + b) mod p) mod m
  // where HashA = a
  //       HashB = b
  //       HashC = c
  //       HashP = p
  //       HashM = m

  // First we select a prime number that is larger than the largest key
  // in our set.
  unsigned MaxKey = 0;
  unsigned MinKey = 0xFFFFFFFF;
  for (auto Addr : Addrs) {
    if ((unsigned)Addr > MaxKey)
      MaxKey = (unsigned)Addr;
    if ((unsigned)Addr < MinKey)
      MinKey = (unsigned)Addr;
  }
  // Select b
  Res.C = MinKey;
  MaxKey = MaxKey - MinKey;
  // Pick a prime number
  unsigned P;
  for (P = MaxKey; P < 0xFFFFFFFF; ++P) {
    bool Prime = true;
    for (unsigned J = 2; J < (P >> 1); ++J) {
      if (P % J == 0) {
        Prime = false;
        break;
      }
    }
    if (Prime)
      break;
  }
  if (P == 0)
    P = 3;

  assert(P < 0xFFFFFFFF && "Couldn't find a prime number");
  // Now try to pick values for a, b, m that provides a perfect hash
  Res.P = P;
  Res.M = NumFuncs * NumFuncs;
  assert (Res.M < 10000000 && "Hash is too large for this application");
  bool *Map = new bool[Res.M];
  bool Conflict = true;
  unsigned Trials = 0;
  while (Conflict) {
    assert(Trials++ < 100 && "Couldn't find a good hash function in 100 trials");
    Conflict = false;
    for (int I = 0, E = Res.M; I != E; ++I)
      Map[I] = false;
    Res.A = rand();
    Res.B = rand();
    for (auto Addr : Addrs) {
      unsigned k = (unsigned) Addr;
      unsigned h = (Res.A * (k - Res.C) + Res.B) % Res.P % Res.M;
      if (Map[h]) {
        Conflict = true;
        break;
      }
      Map[h] = true;
    }
  }
  delete [] Map;
  return Res;
}

template<typename T>
Value *OiIREmitter::CreateHashTableFor(ArrayRef<T> Addrs,
                                       const HashParams &Hash) {
  Constant **Map = new Constant *[Hash.M];

  for (unsigned I = 0; I < Hash.M; ++I)
    Map[I] = nullptr;

  for (auto Addr : Addrs) {
    std::string Idx = Twine("bb").concat(Twine::utohexstr(Addr)).str();
    assert (BBMap[Idx] != 0 && "Missing basic block for address");
    Constant *Target = nullptr;
    BasicBlock *BB = BBMap[Idx];
    if (&(*BB->getParent()->begin()) != BB)
      Target = BlockAddress::get(BBMap[Idx]);
    else {
      std::string FunIdx = Twine("a").concat(Twine::utohexstr(Addr)).str();
      FunctionType *FT =
          FunctionType::get(Type::getVoidTy(getGlobalContext()), false);
      Function *F = reinterpret_cast<Function *>(
          TheModule->getOrInsertFunction(FunIdx, FT));
      Target = ConstantExpr::getBitCast(F, Type::getInt8PtrTy(getGlobalContext()));
    }
    unsigned k = (unsigned) Addr;
    unsigned h = (Hash.A * (k - Hash.C) + Hash.B) % Hash.P % Hash.M;
    Map[h] = Target;
  }

  for (unsigned I = 0; I < Hash.M; ++I) {
    if (Map[I] == nullptr)
      Map[I] = ConstantExpr::getIntToPtr(
          ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0),
          Type::getInt8PtrTy(getGlobalContext()));
  }

  ConstantArray *c = dyn_cast<ConstantArray>(ConstantArray::get(
      ArrayType::get(Type::getInt8PtrTy(getGlobalContext()), Hash.M),
      ArrayRef<Constant *>(&Map[0], Hash.M)));

  GlobalVariable *gv = new GlobalVariable(*TheModule, c->getType(), false,
                                          GlobalValue::ExternalLinkage, c);

  delete [] Map;
  return gv;
}
