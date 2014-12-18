//===- lib/MC/MC2IRStreamer.cpp - Text Assembly Output --------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "OiCombinePass.h"
#include "llvm/Support/PatternMatch.h"
using namespace llvm;
using namespace PatternMatch;

static void OiCombine(Instruction *v) {
  Value *X, *Y;  
  if (match(v, m_Add(m_Shl(m_LShr(m_Value(X),
                                  m_ConstantInt<16>()),
                           m_ConstantInt<16>()),
                     m_And(m_Value(Y),
                           m_ConstantInt<0xFFFF>())))) {
    //    if (X == Y) {
      v->replaceAllUsesWith(Y);
      //    }
  }
}

bool OiCombinePass::runOnFunction(Function &F) {
  errs() << "Hello: ";
  errs().write_escaped(F.getName()) << '\n';

  for (Function::iterator FI = F.begin(), FE = F.end(); FI != FE; ++FI) {
    for (BasicBlock::iterator BI = FI->begin(), BE = FI->end(); BI != BE; ++BI){
      OiCombine(&*BI);
    }
  }
  return false;
}

char OiCombinePass::ID = 0;
static RegisterPass<OiCombinePass> X("oicombinepass", "OpenISA-specific Combine Pass", false, false);
