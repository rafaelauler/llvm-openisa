//===- lib/MC/MC2IRStreamer.cpp - Text Assembly Output --------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "OiCombinePass.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"

#define COMBINE2

using namespace llvm;
using namespace PatternMatch;

static unsigned numMatches1 = 0;
#ifdef COMBINE2
static unsigned numMatches2 = 0;
#endif

static void OiCombine(Instruction *v, IRBuilder<> &Builder) {
  Value *X, *Y;  
  if (match(v, m_Add(m_Shl(m_LShr(m_Value(X),
                                  m_ConstantInt<16>()),
                           m_ConstantInt<16>()),
                     m_And(m_Value(Y),
                           m_ConstantInt<0xFFFF>())))) {
    //    if (X == Y) {
      v->replaceAllUsesWith(Y);
      ++numMatches1;
      //    }
  }

#ifdef COMBINE2
  if (auto *GEP = dyn_cast<GetElementPtrInst>(v)) {
    if (GEP->getNumIndices() < 2)
      return;
    auto *AddInst = dyn_cast<Instruction>(GEP->getOperand(2));
    if (!AddInst)
      return;

    if (auto *Ptr = dyn_cast<Constant>(GEP->getPointerOperand())) {
      ConstantInt *CI;
      if (match(AddInst, m_Add(m_Value(X), m_ConstantInt(CI)))) {
        Builder.SetInsertPoint(AddInst);
//      AddInst->dump();
//      GEP->dump();
        Value *V1 = Builder.CreatePtrToInt(Ptr, Type::getInt32Ty(getGlobalContext()));
        AddInst->setOperand(0, V1);
        Builder.SetInsertPoint(v);
        Value *V2 = Builder.CreateAdd(AddInst, X);
        Value *AddCast = Builder.CreateIntToPtr(
            V2, Type::getInt8PtrTy(getGlobalContext()));
//     V2->dump();
       GEP->replaceAllUsesWith(AddCast);
        ++numMatches2;
      }
    }
  }
#endif
}

bool OiCombinePass::runOnFunction(Function &F) {
  IRBuilder<> Builder(getGlobalContext());
  errs() << "Hello: ";
  errs().write_escaped(F.getName()) << '\n';

  for (Function::iterator FI = F.begin(), FE = F.end(); FI != FE; ++FI) {
    for (BasicBlock::iterator BI = FI->begin(), BE = FI->end(); BI != BE; ++BI){
      OiCombine(&*BI, Builder);
    }
  }

  errs() << "Number of load large immediates recombined: " << numMatches1 << "\n";
#ifdef COMBINE2
  errs() << "Number of GEP+ShadowMemory recombined: " << numMatches2 << "\n";
  return numMatches1 + numMatches2 > 0;
#else
  return numMatches1 > 0;
#endif

}

char OiCombinePass::ID = 0;
static RegisterPass<OiCombinePass> X("oicombinepass", "OpenISA-specific Combine Pass", false, false);

#undef COMBINE2
