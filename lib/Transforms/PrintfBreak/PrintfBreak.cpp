//===- PrintfBreak.cpp - Example code from "Writing an LLVM Pass" ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Hello World" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#define DEBUG_TYPE "printf-break"

STATISTIC(PrintfBreakCounter, "Counts number of printfs transformed");

namespace llvm {
  FunctionPass *createPrintfBreakPass();
}

// Splits a given string with many percentages (%) to a group of strings
// that has at most four percentages (%) each.
std::vector<StringRef> SplitString(StringRef str) {
  size_t lastStart = 0;
  int count = 1;
  std::vector<StringRef> ans;
  for (size_t I = 0, E = str.size(); I != E; ++I) {
    if (str[I] == '%' && ((I + 1 == E) || str[I + 1] != '%' || (++I, false)) &&
        ++count == 3) {
      count = 2;
      ans.push_back(str.slice(lastStart, I));
      lastStart = I;
    }
  }
  ans.push_back(str.slice(lastStart, str.size()));
  return ans;
}

namespace {
  struct PrintfBreak : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
    PrintfBreak() : FunctionPass(ID) {
      initializePrintfBreakPass(*PassRegistry::getPassRegistry());
    }

    bool Combine(Instruction *v, IRBuilder<> &Builder) {
      auto *FunCall = dyn_cast<CallInst>(v);
      if (FunCall == nullptr)
        return false;

      Function *Callee = FunCall->getCalledFunction();
      if (Callee == nullptr || Callee->getName() != "printf" ||
          FunCall->getNumArgOperands() <= 4) {
        bool hasFloat = false;
        for (int I = 1, E = FunCall->getNumOperands() - 1; I < E; ++I) {
          Value *CurOp = FunCall->getArgOperand(I);
          if (CurOp->getType()->isFloatTy() ||
              CurOp->getType()->isDoubleTy())
            hasFloat = true;
        }
        if (!hasFloat)
          return false;
      }
      errs() << "We have a candidate (call to printf).\n";

      auto *GEP = dyn_cast<GetElementPtrInst>(FunCall->getArgOperand(0));
      GlobalVariable *Op1 = nullptr;
      if (GEP != nullptr) {
        Op1 = dyn_cast<GlobalVariable>(GEP->getPointerOperand());
        if (Op1 == nullptr) {
          errs() << "is not GlobalVariable...\n";
          GEP->getPointerOperand()->dump();
          return false;
        }

      } else {
        auto *GEPExpr = dyn_cast<ConstantExpr>(FunCall->getArgOperand(0));
        if (GEPExpr == nullptr ||
            GEPExpr->getOpcode() != llvm::Instruction::MemoryOps::GetElementPtr) {
          errs() << "... not a GEP for first argument.\n";
          FunCall->getArgOperand(0)->dump();
          return false;
        }
        Op1 = dyn_cast<GlobalVariable>(GEPExpr->getOperand(0));
        if (Op1 == nullptr) {
          errs() << "is not a GlobalVariable..\n";
          GEPExpr->getOperand(0)->dump();
          return false;
        }
      }

      auto *Op1Init = dyn_cast<ConstantDataSequential>(Op1->getInitializer());
      if (Op1Init == nullptr || !Op1Init->isCString()) {
        errs() << "is not a constantdatasequential or is not cstring\n";
        return false;
      }

      StringRef Op1Str = Op1Init->getAsCString();
      errs() << "Found : " << Op1Str << "\n";
      std::vector<StringRef> SubStrs = SplitString(Op1Str);

      // Build pack of operands for each sub str
      std::vector<std::vector<Value *>> Operands;
      std::vector<Value *> CurOperands;
      int counter = 1;
      for (int I = 1, E = FunCall->getNumOperands() - 1; I < E; ++I) {
        Value *cur = FunCall->getArgOperand(I);
        if (++counter == 3) {
          counter = 2;
          Operands.push_back(CurOperands);
          CurOperands.clear();
        }
        CurOperands.push_back(cur);
      }
      Operands.push_back(CurOperands);

      // Issue new printf calls
      Module *m = FunCall->getParent()->getParent()->getParent();
      Builder.SetInsertPoint(FunCall);
      if (SubStrs.size() != Operands.size()) {
        errs() << "Error: SubStrs.size() == " << SubStrs.size();
        errs() << " and Operands.size() == " << Operands.size() << "\n";
        errs() << "Dumping SubStrs:\n";
        int count = 0;
        for (auto aa : SubStrs) {
          errs() << count++ << ": \"" << aa << "\"\n";
        }
        errs() << "FunCall->getNumOperands() == " << FunCall->getNumOperands()
               << "\n";
        errs() << "Dumping Operands:\n";
        count = 0;
        for (auto aa : Operands) {
          errs() << count++ << ":\n";
          for (auto bb : aa) {
            bb->dump();
          }
        }
      }
      assert(SubStrs.size() == Operands.size());
      Instruction *LastCall = nullptr;
      for (int I = 0, E = SubStrs.size(); I != E; ++I) {
        SmallVector<Constant *, 2> Idxs;
        Idxs.push_back(ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0));
        Idxs.push_back(ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0));
        auto cur = Operands[I];
        assert (cur.size() == 1);
        // Build new global value
        Constant *c = nullptr;
        bool ConvertToPuts = false;
        size_t pos = SubStrs[I].find("%s");
        StringRef PreString, PostString;
        if (pos != StringRef::npos) {
          PreString = SubStrs[I].slice(0, pos);
          PostString = SubStrs[I].slice(pos + 2, SubStrs[I].size());
          ConvertToPuts = true;
        } else if (cur[0]->getType()->isFloatTy() ||
                   cur[0]->getType()->isDoubleTy()) {
          SmallVector<char, 20> Buf;
          c = ConstantDataArray::getString(
              getGlobalContext(),
              Twine("%d ").concat(SubStrs[I]).toStringRef(Buf));
          cur.insert(cur.begin(),
                     ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0));
        } else {
          c = ConstantDataArray::getString(getGlobalContext(), SubStrs[I]);
        }
        if (!ConvertToPuts) {
          GlobalVariable *gv = new GlobalVariable(
              *m, c->getType(), false, GlobalValue::ExternalLinkage, c);
          cur.insert(cur.begin(),
                     ConstantExpr::getGetElementPtr(
                         gv, Idxs, Type::getInt8PtrTy(getGlobalContext())));
          errs() << SubStrs[I] << "\n";
          for (auto aa : cur) {
            aa->dump();
          }
          LastCall = Builder.CreateCall(Callee, cur);
        } else {
          // Convert this to 3 puts
          if (PreString.size() > 0) {
            std::vector<Value *> PreStrOperands;
            c = ConstantDataArray::getString(getGlobalContext(), PreString);
            GlobalVariable *gv = new GlobalVariable(
                *m, c->getType(), false, GlobalValue::ExternalLinkage, c);
            PreStrOperands.insert(
                PreStrOperands.begin(),
                ConstantExpr::getGetElementPtr(
                    gv, Idxs, Type::getInt8PtrTy(getGlobalContext())));
            SmallVector<Type *, 8> args(1,
                                        Type::getInt8PtrTy(getGlobalContext()));
            FunctionType *ft = FunctionType::get(
                Type::getInt32Ty(getGlobalContext()), args, /*isvararg*/ false);
            Value *fun = m->getOrInsertFunction("puts", ft);
            errs() << "Emitting pre-string puts..\n";
            for (auto aa : PreStrOperands) {
              aa->dump();
            }
            LastCall = Builder.CreateCall(fun, PreStrOperands);
          }
          SmallVector<Type *, 8> args(1,
                                      Type::getInt8PtrTy(getGlobalContext()));
          FunctionType *ft = FunctionType::get(
              Type::getInt32Ty(getGlobalContext()), args, /*isvararg*/ false);
          Value *fun = m->getOrInsertFunction("puts", ft);
          errs() << "Emitting puts..\n";
          for (auto aa : cur) {
            aa->dump();
          }
          LastCall = Builder.CreateCall(fun, cur);
          if (PostString.size() > 0) {
            std::vector<Value *> PostStrOperands;
            c = ConstantDataArray::getString(getGlobalContext(), PostString);
            GlobalVariable *gv = new GlobalVariable(
                *m, c->getType(), false, GlobalValue::ExternalLinkage, c);
            PostStrOperands.insert(
                PostStrOperands.begin(),
                ConstantExpr::getGetElementPtr(
                    gv, Idxs, Type::getInt8PtrTy(getGlobalContext())));
            errs() << "Emitting post-string puts..\n";
            for (auto aa : PostStrOperands) {
              aa->dump();
            }
            LastCall = Builder.CreateCall(fun, PostStrOperands);
          }
        }
      }
      FunCall->replaceAllUsesWith(LastCall);
      ++PrintfBreakCounter;
      return true;
    }

    bool runOnFunction(Function &F) override {
      IRBuilder<> Builder(getGlobalContext());

      errs() << "Hello: ";
      errs().write_escaped(F.getName()) << '\n';

      std::vector<Instruction *> ToRemove;
      for (Function::iterator FI = F.begin(), FE = F.end(); FI != FE; ++FI) {
        for (BasicBlock::iterator BI = FI->begin(), BE = FI->end(); BI != BE;
             ++BI) {
          if (Combine(&*BI, Builder))
            ToRemove.push_back(&*BI);
        }
      }
      for (auto CurCall : ToRemove) {
        CurCall->dropAllReferences();
        CurCall->removeFromParent();
      }

      return ToRemove.size() > 0;
    }

    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.setPreservesCFG();
    }
  };
}

char PrintfBreak::ID = 0;

// createPrintfBreakPass - The public interface to this file.
FunctionPass *llvm::createPrintfBreakPass() {
  return new PrintfBreak();
}

INITIALIZE_PASS_BEGIN(PrintfBreak, "printf-break", "Breaks Printfs", false, false)
INITIALIZE_PASS_END(PrintfBreak, "printf-break", "Breaks Printfs", false, false)
//INITIALIZE_PASS_DEPENDENCY(AssumptionCacheTracker)
//INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
//INITIALIZE_PASS_DEPENDENCY(TargetLibraryInfoWrapperPass)

//static RegisterPass<PrintfBreak> X("printfbreak", "Break Printfs Pass");

