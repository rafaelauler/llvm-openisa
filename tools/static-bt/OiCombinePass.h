#ifndef MC2IRSTREAMER_H_
#define MC2IRSTREAMER_H_

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"

namespace llvm{

 struct OiCombinePass : public FunctionPass {
   static char ID;
   OiCombinePass() : FunctionPass(ID) {}
   
   virtual bool runOnFunction(Function &F);
 };

}

#endif
