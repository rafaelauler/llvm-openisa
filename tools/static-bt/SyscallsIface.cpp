//=== SyscallsIface.cpp - -------------------------------------- -*- C++ -*-==//
//
// Generate code to call host syscalls or libc functions during static
// binary translation.
//
//===----------------------------------------------------------------------===//
#include "SyscallsIface.h"
#include "../lib/Target/Mips/MipsInstrInfo.h"
#include "SBTUtils.h"

using namespace llvm;

bool SyscallsIface::HandleLibcAtoi(Value *&V, Value **First) {
  SmallVector<Type *, 8> args(1, Type::getInt32Ty(getGlobalContext()));
  FunctionType *ft = FunctionType::get(Type::getInt32Ty(getGlobalContext()),
                                       args, /*isvararg*/ false);
  Value *fun = TheModule->getOrInsertFunction("atoi", ft);
  SmallVector<Value *, 8> params;
  Value *f = Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A0)]);
  if (First)
    *First = GetFirstInstruction(*First, f);
  Value *addrbuf = IREmitter.AccessShadowMemory(f, false);
  params.push_back(
      Builder.CreatePtrToInt(addrbuf, Type::getInt32Ty(getGlobalContext())));
  V = Builder.CreateStore(Builder.CreateCall(fun, params),
                          IREmitter.Regs[ConvToDirective(Mips::V0)]);
  ReadMap[ConvToDirective(Mips::A0)] = true;
  WriteMap[ConvToDirective(Mips::V0)] = true;
  return true;
}

bool SyscallsIface::HandleLibcMalloc(Value *&V, Value **First) {
  SmallVector<Type *, 8> args(1, Type::getInt32Ty(getGlobalContext()));
  FunctionType *ft = FunctionType::get(Type::getInt32Ty(getGlobalContext()),
                                       args, /*isvararg*/ false);
  Value *fun = TheModule->getOrInsertFunction("malloc", ft);
  SmallVector<Value *, 8> params;
  Value *f = Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A0)]);
  if (First)
    *First = GetFirstInstruction(*First, f);
  params.push_back(f);
  Value *mal = Builder.CreateCall(fun, params);
  if (NoShadow) {
    V = Builder.CreateStore(mal, IREmitter.Regs[ConvToDirective(Mips::V0)]);
  } else {
    Value *ptr = Builder.CreatePtrToInt(IREmitter.ShadowImageValue,
                                        Type::getInt32Ty(getGlobalContext()));
    Value *fixed = Builder.CreateSub(mal, ptr);
    V = Builder.CreateStore(fixed, IREmitter.Regs[ConvToDirective(Mips::V0)]);
  }
  ReadMap[ConvToDirective(Mips::A0)] = true;
  WriteMap[ConvToDirective(Mips::V0)] = true;
  return true;
}

bool SyscallsIface::HandleLibcCalloc(Value *&V, Value **First) {
  SmallVector<Type *, 8> args(2, Type::getInt32Ty(getGlobalContext()));
  FunctionType *ft = FunctionType::get(Type::getInt32Ty(getGlobalContext()),
                                       args, /*isvararg*/ false);
  Value *fun = TheModule->getOrInsertFunction("calloc", ft);
  Value *f = Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A0)]);
  if (First)
    *First = GetFirstInstruction(*First, f);
  SmallVector<Value *, 8> params;
  params.push_back(f);
  params.push_back(
      Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A1)]));
  Value *mal = Builder.CreateCall(fun, params);
  if (NoShadow) {
    V = Builder.CreateStore(mal, IREmitter.Regs[ConvToDirective(Mips::V0)]);
  } else {
    Value *ptr = Builder.CreatePtrToInt(IREmitter.ShadowImageValue,
                                        Type::getInt32Ty(getGlobalContext()));
    Value *fixed = Builder.CreateSub(mal, ptr);
    V = Builder.CreateStore(fixed, IREmitter.Regs[ConvToDirective(Mips::V0)]);
  }
  ReadMap[ConvToDirective(Mips::A0)] = true;
  ReadMap[ConvToDirective(Mips::A1)] = true;
  WriteMap[ConvToDirective(Mips::V0)] = true;
  return true;
}

bool SyscallsIface::HandleLibcFree(Value *&V, Value **First) {
  SmallVector<Type *, 8> args(1, Type::getInt32Ty(getGlobalContext()));
  FunctionType *ft = FunctionType::get(Type::getVoidTy(getGlobalContext()),
                                       args, /*isvararg*/ false);
  Value *fun = TheModule->getOrInsertFunction("free", ft);
  SmallVector<Value *, 8> params;
  Value *f = Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A0)]);
  if (First)
    *First = GetFirstInstruction(*First, f);
  Value *addrbuf = IREmitter.AccessShadowMemory(f, false);
  params.push_back(
      Builder.CreatePtrToInt(addrbuf, Type::getInt32Ty(getGlobalContext())));
  V = Builder.CreateCall(fun, params);
  ReadMap[ConvToDirective(Mips::A0)] = true;
  return true;
}

bool SyscallsIface::HandleLibcExit(Value *&V, Value **First) {
  SmallVector<Type *, 8> args(1, Type::getInt32Ty(getGlobalContext()));
  FunctionType *ft = FunctionType::get(Type::getVoidTy(getGlobalContext()),
                                       args, /*isvararg*/ false);
  Value *fun = TheModule->getOrInsertFunction("exit", ft);
  SmallVector<Value *, 8> params;
  Value *f = Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A0)]);
  if (First)
    *First = GetFirstInstruction(*First, f);
  params.push_back(f);
  V = Builder.CreateCall(fun, params);
  ReadMap[ConvToDirective(Mips::A0)] = true;
  return true;
}

bool SyscallsIface::HandleGenericInt(Value *&V, StringRef Name, int numargs,
                                     int numret, ArgType *ArgTypes,
                                     Value **First) {
  SmallVector<Type *, 8> args(numargs, Type::getInt32Ty(getGlobalContext()));
  FunctionType *ft;
  if (numret == 0)
    ft = FunctionType::get(Type::getVoidTy(getGlobalContext()), args,
                           /*isvararg*/ false);
  else if (numret == 1)
    ft = FunctionType::get(Type::getInt32Ty(getGlobalContext()), args,
                           /*isvararg*/ false);
  else
    llvm_unreachable("Unhandled return size.");
  Value *fun = TheModule->getOrInsertFunction(Name, ft);
  SmallVector<Value *, 8> params;
  assert(numargs <= 4 && "Cannot handle more than 4 arguments");
  if (numargs > 0) {
    for (int I = 0, E = numargs; I != E; ++I) {

      Value *f =
          Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A0) + I]);
      if (I == 0 && First)
        *First = GetFirstInstruction(*First, f);
      switch (ArgTypes[I]) {
      case AT_Ptr: {
        Value *zero = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0);
        Value *cmp = Builder.CreateICmpEQ(f, zero);
        Value *ptr = IREmitter.AccessShadowMemory(f, false);
        Value * final = Builder.CreateSelect(
            cmp, zero,
            Builder.CreatePtrToInt(ptr, Type::getInt32Ty(getGlobalContext())));
        params.push_back(final);
        break;
      }
      case AT_Int32: {
        params.push_back(f);
        break;
      }
      default:
        llvm_unreachable("Unhandled arg type for HandleGenericInt");
      }
      ReadMap[ConvToDirective(Mips::A0) + I] = true;
    }
    V = Builder.CreateCall(fun, params);
  } else {
    V = Builder.CreateCall(fun, params);
    if (First)
      *First = GetFirstInstruction(*First, V);
  }
  if (numret > 0) {
    switch (ArgTypes[numargs]) {
    case AT_Ptr: {
      if (NoShadow) {
        V = Builder.CreateStore(V, IREmitter.Regs[ConvToDirective(Mips::V0)]);
      } else {
        Value *zero = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0);
        Value *cmp = Builder.CreateICmpEQ(V, zero);
        Value *ptr = Builder.CreatePtrToInt(
            IREmitter.ShadowImageValue, Type::getInt32Ty(getGlobalContext()));
        Value *fixed = Builder.CreateSub(V, ptr);
        Value *final = Builder.CreateSelect(cmp, zero, fixed);
        V = Builder.CreateStore(final,
                                IREmitter.Regs[ConvToDirective(Mips::V0)]);
      }
      break;
    }
    case AT_Int32: {
      Builder.CreateStore(V, IREmitter.Regs[ConvToDirective(Mips::V0)]);
      break;
    }
    default:
      llvm_unreachable("Unhandled return type for HandleGenericInt");
    }
    WriteMap[ConvToDirective(Mips::V0)] = true;
  }

  return true;
}

Function *SyscallsIface::createTranslateCTypeFunction() {
  SmallVector<Type *, 1> args;
  args.push_back(Type::getInt32Ty(getGlobalContext()));
  FunctionType *FT =
      FunctionType::get(Type::getInt32Ty(getGlobalContext()), args, false);
  Constant *C =
      TheModule->getOrInsertFunction("__xlated_ctype_toupper_loc", FT);
  auto *F = dyn_cast<Function>(C);
  assert(F && "getOrInsertFunction must return a function type here");
  if (F->size() > 0)
    return F;

  // Need to create the function
  BasicBlock *BB = BasicBlock::Create(getGlobalContext(), "", F);
  IRBuilder<> PBuilder(getGlobalContext());
  PBuilder.SetInsertPoint(BB);

  GlobalVariable *GV = new GlobalVariable(
      *TheModule, Type::getInt1Ty(getGlobalContext()), false,
      GlobalValue::PrivateLinkage,
      ConstantInt::get(Type::getInt1Ty(getGlobalContext()), 0),
      "__ctype_xlated");

  Value *LoadGV = PBuilder.CreateLoad(GV);
  Value *one = ConstantInt::get(Type::getInt1Ty(getGlobalContext()), 1U);
  Value *cmp = PBuilder.CreateICmpEQ(LoadGV, one);

  BasicBlock *BBTrue = BasicBlock::Create(getGlobalContext(), "", F);
  BasicBlock *BBFalse = BasicBlock::Create(getGlobalContext(), "", F);

  assert(F->arg_size() == 1 && "Wrong function arguments");
  Value *InputVal = F->arg_begin();

  PBuilder.CreateCondBr(cmp, BBTrue, BBFalse);
  PBuilder.SetInsertPoint(BBTrue);
  PBuilder.CreateRet(InputVal);

  PBuilder.SetInsertPoint(BBFalse);
  // Initialize IV
  Value *IV = PBuilder.CreateAlloca(Type::getInt32Ty(getGlobalContext()));
  Value *Zero = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0);
  //  Value *One = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 1);
  Value *Four = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 4);
  PBuilder.CreateStore(Zero, IV);

  // Create Loop Header
  BasicBlock *BBLoopHeader = BasicBlock::Create(getGlobalContext(), "", F);
  BasicBlock *BBLoopExit = BasicBlock::Create(getGlobalContext(), "", F);
  PBuilder.CreateBr(BBLoopHeader);
  PBuilder.SetInsertPoint(BBLoopHeader);
  Value *IVLoad = PBuilder.CreateLoad(IV);
  //  Value *Plus255 = ConstantInt::get(Type::getInt32Ty(getGlobalContext()),
  //  255);
  BasicBlock *BBLoopBody = BasicBlock::Create(getGlobalContext(), "", F);
  Value *Cmp2 = PBuilder.CreateICmpSGT(IVLoad, Zero);
  PBuilder.CreateCondBr(Cmp2, BBLoopExit, BBLoopBody);
  // Create Loop Body
  PBuilder.SetInsertPoint(BBLoopBody);
  Value *Shadow = PBuilder.CreatePtrToInt(IREmitter.ShadowImageValue,
                                          Type::getInt32Ty(getGlobalContext()));
  Value *Ptr = PBuilder.CreateIntToPtr(
      PBuilder.CreateAdd(PBuilder.CreateLoad(IV), InputVal),
      Type::getInt32PtrTy(getGlobalContext()));
  PBuilder.CreateStore(PBuilder.CreateSub(PBuilder.CreateLoad(Ptr), Shadow),
                       Ptr);
  PBuilder.CreateStore(PBuilder.CreateAdd(PBuilder.CreateLoad(IV), Four), IV);
  PBuilder.CreateBr(BBLoopHeader);

  // Create Loop Exit
  PBuilder.SetInsertPoint(BBLoopExit);
  PBuilder.CreateStore(one, GV);
  PBuilder.CreateRet(InputVal);

  return F;
}

Function *SyscallsIface::createTranslateToLowerFunction() {
  SmallVector<Type *, 1> args;
  args.push_back(Type::getInt32Ty(getGlobalContext()));
  FunctionType *FT =
      FunctionType::get(Type::getInt32Ty(getGlobalContext()), args, false);
  Constant *C =
      TheModule->getOrInsertFunction("__xlated_ctype_tolower_loc", FT);
  auto *F = dyn_cast<Function>(C);
  assert(F && "getOrInsertFunction must return a function type here");
  if (F->size() > 0)
    return F;

  // Need to create the function
  BasicBlock *BB = BasicBlock::Create(getGlobalContext(), "", F);
  IRBuilder<> PBuilder(getGlobalContext());
  PBuilder.SetInsertPoint(BB);

  GlobalVariable *GV = new GlobalVariable(
      *TheModule, Type::getInt1Ty(getGlobalContext()), false,
      GlobalValue::PrivateLinkage,
      ConstantInt::get(Type::getInt1Ty(getGlobalContext()), 0),
      "__ctype_tolower_xlated");

  Value *LoadGV = PBuilder.CreateLoad(GV);
  Value *one = ConstantInt::get(Type::getInt1Ty(getGlobalContext()), 1U);
  Value *cmp = PBuilder.CreateICmpEQ(LoadGV, one);

  BasicBlock *BBTrue = BasicBlock::Create(getGlobalContext(), "", F);
  BasicBlock *BBFalse = BasicBlock::Create(getGlobalContext(), "", F);

  assert(F->arg_size() == 1 && "Wrong function arguments");
  Value *InputVal = F->arg_begin();

  PBuilder.CreateCondBr(cmp, BBTrue, BBFalse);
  PBuilder.SetInsertPoint(BBTrue);
  PBuilder.CreateRet(InputVal);

  PBuilder.SetInsertPoint(BBFalse);
  // Initialize IV
  Value *IV = PBuilder.CreateAlloca(Type::getInt32Ty(getGlobalContext()));
  Value *Zero = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0);
  //  Value *One = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 1);
  Value *Four = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 4);
  PBuilder.CreateStore(Zero, IV);

  // Create Loop Header
  BasicBlock *BBLoopHeader = BasicBlock::Create(getGlobalContext(), "", F);
  BasicBlock *BBLoopExit = BasicBlock::Create(getGlobalContext(), "", F);
  PBuilder.CreateBr(BBLoopHeader);
  PBuilder.SetInsertPoint(BBLoopHeader);
  Value *IVLoad = PBuilder.CreateLoad(IV);
  //  Value *Plus255 = ConstantInt::get(Type::getInt32Ty(getGlobalContext()),
  //  255);
  BasicBlock *BBLoopBody = BasicBlock::Create(getGlobalContext(), "", F);
  Value *Cmp2 = PBuilder.CreateICmpSGT(IVLoad, Zero);
  PBuilder.CreateCondBr(Cmp2, BBLoopExit, BBLoopBody);
  // Create Loop Body
  PBuilder.SetInsertPoint(BBLoopBody);
  Value *Shadow = PBuilder.CreatePtrToInt(IREmitter.ShadowImageValue,
                                          Type::getInt32Ty(getGlobalContext()));
  Value *Ptr = PBuilder.CreateIntToPtr(
      PBuilder.CreateAdd(PBuilder.CreateLoad(IV), InputVal),
      Type::getInt32PtrTy(getGlobalContext()));
  PBuilder.CreateStore(PBuilder.CreateSub(PBuilder.CreateLoad(Ptr), Shadow),
                       Ptr);
  PBuilder.CreateStore(PBuilder.CreateAdd(PBuilder.CreateLoad(IV), Four), IV);
  PBuilder.CreateBr(BBLoopHeader);

  // Create Loop Exit
  PBuilder.SetInsertPoint(BBLoopExit);
  PBuilder.CreateStore(one, GV);
  PBuilder.CreateRet(InputVal);

  return F;
}

Function *SyscallsIface::createTranslateBLocFunction() {
  SmallVector<Type *, 1> args;
  args.push_back(Type::getInt32Ty(getGlobalContext()));
  FunctionType *FT =
      FunctionType::get(Type::getInt32Ty(getGlobalContext()), args, false);
  Constant *C =
      TheModule->getOrInsertFunction("__xlated_ctype_b_loc", FT);
  auto *F = dyn_cast<Function>(C);
  assert(F && "getOrInsertFunction must return a function type here");
  if (F->size() > 0)
    return F;

  // Need to create the function
  BasicBlock *BB = BasicBlock::Create(getGlobalContext(), "", F);
  IRBuilder<> PBuilder(getGlobalContext());
  PBuilder.SetInsertPoint(BB);

  GlobalVariable *GV = new GlobalVariable(
      *TheModule, Type::getInt1Ty(getGlobalContext()), false,
      GlobalValue::PrivateLinkage,
      ConstantInt::get(Type::getInt1Ty(getGlobalContext()), 0),
      "__ctype_bloc_xlated");

  Value *LoadGV = PBuilder.CreateLoad(GV);
  Value *one = ConstantInt::get(Type::getInt1Ty(getGlobalContext()), 1U);
  Value *cmp = PBuilder.CreateICmpEQ(LoadGV, one);

  BasicBlock *BBTrue = BasicBlock::Create(getGlobalContext(), "", F);
  BasicBlock *BBFalse = BasicBlock::Create(getGlobalContext(), "", F);

  assert(F->arg_size() == 1 && "Wrong function arguments");
  Value *InputVal = F->arg_begin();

  PBuilder.CreateCondBr(cmp, BBTrue, BBFalse);
  PBuilder.SetInsertPoint(BBTrue);
  PBuilder.CreateRet(InputVal);

  PBuilder.SetInsertPoint(BBFalse);
  // Initialize IV
  Value *IV = PBuilder.CreateAlloca(Type::getInt32Ty(getGlobalContext()));
  Value *Zero = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0);
  //  Value *One = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 1);
  Value *Four = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 4);
  PBuilder.CreateStore(Zero, IV);

  // Create Loop Header
  BasicBlock *BBLoopHeader = BasicBlock::Create(getGlobalContext(), "", F);
  BasicBlock *BBLoopExit = BasicBlock::Create(getGlobalContext(), "", F);
  PBuilder.CreateBr(BBLoopHeader);
  PBuilder.SetInsertPoint(BBLoopHeader);
  Value *IVLoad = PBuilder.CreateLoad(IV);
  //  Value *Plus255 = ConstantInt::get(Type::getInt32Ty(getGlobalContext()),
  //  255);
  BasicBlock *BBLoopBody = BasicBlock::Create(getGlobalContext(), "", F);
  Value *Cmp2 = PBuilder.CreateICmpSGT(IVLoad, Zero);
  PBuilder.CreateCondBr(Cmp2, BBLoopExit, BBLoopBody);
  // Create Loop Body
  PBuilder.SetInsertPoint(BBLoopBody);
  Value *Shadow = PBuilder.CreatePtrToInt(IREmitter.ShadowImageValue,
                                          Type::getInt32Ty(getGlobalContext()));
  Value *Ptr = PBuilder.CreateIntToPtr(
      PBuilder.CreateAdd(PBuilder.CreateLoad(IV), InputVal),
      Type::getInt32PtrTy(getGlobalContext()));
  PBuilder.CreateStore(PBuilder.CreateSub(PBuilder.CreateLoad(Ptr), Shadow),
                       Ptr);
  PBuilder.CreateStore(PBuilder.CreateAdd(PBuilder.CreateLoad(IV), Four), IV);
  PBuilder.CreateBr(BBLoopHeader);

  // Create Loop Exit
  PBuilder.SetInsertPoint(BBLoopExit);
  PBuilder.CreateStore(one, GV);
  PBuilder.CreateRet(InputVal);

  return F;
}

bool SyscallsIface::HandleCTypeToUpperLoc(Value *&V, Value **First) {
  SmallVector<Type *, 1> args;
  FunctionType *ft =
      FunctionType::get(Type::getInt32Ty(getGlobalContext()), args,
                        /*isvararg*/ false);
  Value *fun = TheModule->getOrInsertFunction("__ctype_toupper_loc", ft);
  Value *AdaptorFunction = createTranslateCTypeFunction();

  SmallVector<Value *, 1> params;
  V = Builder.CreateCall(fun, params);
  if (First)
    *First = GetFirstInstruction(*First, V);
  if (NoShadow) {
    V = Builder.CreateStore(V, IREmitter.Regs[ConvToDirective(Mips::V0)]);
    WriteMap[ConvToDirective(Mips::V0)] = true;
    return true;
  }

  SmallVector<Value *, 1> params2;
  params2.push_back(V);
  V = Builder.CreateCall(AdaptorFunction, params2);
  Value *ptr = Builder.CreatePtrToInt(IREmitter.ShadowImageValue,
                                      Type::getInt32Ty(getGlobalContext()));
  Value *fixed = Builder.CreateSub(V, ptr);
  V = Builder.CreateStore(fixed, IREmitter.Regs[ConvToDirective(Mips::V0)]);
  WriteMap[ConvToDirective(Mips::V0)] = true;
  return true;
}

bool SyscallsIface::HandleCTypeToLowerLoc(Value *&V, Value **First) {
  SmallVector<Type *, 1> args;
  FunctionType *ft =
      FunctionType::get(Type::getInt32Ty(getGlobalContext()), args,
                        /*isvararg*/ false);
  Value *fun = TheModule->getOrInsertFunction("__ctype_tolower_loc", ft);
  Value *AdaptorFunction = createTranslateToLowerFunction();

  SmallVector<Value *, 1> params;
  V = Builder.CreateCall(fun, params);
  if (First)
    *First = GetFirstInstruction(*First, V);
  if (NoShadow) {
    V = Builder.CreateStore(V, IREmitter.Regs[ConvToDirective(Mips::V0)]);
    WriteMap[ConvToDirective(Mips::V0)] = true;
    return true;
  }

  SmallVector<Value *, 1> params2;
  params2.push_back(V);
  V = Builder.CreateCall(AdaptorFunction, params2);
  Value *ptr = Builder.CreatePtrToInt(IREmitter.ShadowImageValue,
                                      Type::getInt32Ty(getGlobalContext()));
  Value *fixed = Builder.CreateSub(V, ptr);
  V = Builder.CreateStore(fixed, IREmitter.Regs[ConvToDirective(Mips::V0)]);
  WriteMap[ConvToDirective(Mips::V0)] = true;
  return true;
}

bool SyscallsIface::HandleCTypeBLoc(Value *&V, Value **First) {
  SmallVector<Type *, 1> args;
  FunctionType *ft =
      FunctionType::get(Type::getInt32Ty(getGlobalContext()), args,
                        /*isvararg*/ false);
  Value *fun = TheModule->getOrInsertFunction("__ctype_b_loc", ft);
  Value *AdaptorFunction = createTranslateBLocFunction();

  SmallVector<Value *, 1> params;
  V = Builder.CreateCall(fun, params);
  if (First)
    *First = GetFirstInstruction(*First, V);
  if (NoShadow) {
    V = Builder.CreateStore(V, IREmitter.Regs[ConvToDirective(Mips::V0)]);
    WriteMap[ConvToDirective(Mips::V0)] = true;
    return true;
  }

  SmallVector<Value *, 1> params2;
  params2.push_back(V);
  V = Builder.CreateCall(AdaptorFunction, params2);
  Value *ptr = Builder.CreatePtrToInt(IREmitter.ShadowImageValue,
                                      Type::getInt32Ty(getGlobalContext()));
  Value *fixed = Builder.CreateSub(V, ptr);
  V = Builder.CreateStore(fixed, IREmitter.Regs[ConvToDirective(Mips::V0)]);
  WriteMap[ConvToDirective(Mips::V0)] = true;
  return true;
}

bool SyscallsIface::HandleLibcPuts(Value *&V, Value **First) {
  SmallVector<Type *, 8> args(1, Type::getInt32Ty(getGlobalContext()));
  FunctionType *ft = FunctionType::get(Type::getInt32Ty(getGlobalContext()),
                                       args, /*isvararg*/ false);
  Value *fun = TheModule->getOrInsertFunction("puts", ft);
  SmallVector<Value *, 8> params;
  Value *f = Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A0)]);
  if (First)
    *First = GetFirstInstruction(*First, f);
  Value *addrbuf = IREmitter.AccessShadowMemory(f, false);
  params.push_back(
      Builder.CreatePtrToInt(addrbuf, Type::getInt32Ty(getGlobalContext())));
  V = Builder.CreateStore(Builder.CreateCall(fun, params),
                          IREmitter.Regs[ConvToDirective(Mips::V0)]);
  ReadMap[ConvToDirective(Mips::A0)] = true;
  WriteMap[ConvToDirective(Mips::V0)] = true;
  return true;
}

bool SyscallsIface::HandleLibcMemset(Value *&V, Value **First) {
  SmallVector<Type *, 8> args(3, Type::getInt32Ty(getGlobalContext()));
  FunctionType *ft = FunctionType::get(Type::getInt32Ty(getGlobalContext()),
                                       args, /*isvararg*/ false);
  Value *fun = TheModule->getOrInsertFunction("memset", ft);
  SmallVector<Value *, 8> params;
  Value *f = Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A0)]);
  if (First)
    *First = GetFirstInstruction(*First, f);
  Value *addrbuf = IREmitter.AccessShadowMemory(f, false);
  params.push_back(
      Builder.CreatePtrToInt(addrbuf, Type::getInt32Ty(getGlobalContext())));
  params.push_back(
      Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A1)]));
  params.push_back(
      Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A2)]));
  V = Builder.CreateStore(Builder.CreateCall(fun, params),
                          IREmitter.Regs[ConvToDirective(Mips::V0)]);
  ReadMap[ConvToDirective(Mips::A0)] = true;
  ReadMap[ConvToDirective(Mips::A1)] = true;
  ReadMap[ConvToDirective(Mips::A2)] = true;
  WriteMap[ConvToDirective(Mips::V0)] = true;
  return true;
}

// XXX: Handling a fixed number of 4 arguments, since we cannot infer how many
// arguments the program is using with fprintf
bool SyscallsIface::HandleLibcFprintf(Value *&V, Value **First) {
  SmallVector<Type *, 8> args(2, Type::getInt32Ty(getGlobalContext()));
  FunctionType *ft = FunctionType::get(Type::getInt32Ty(getGlobalContext()),
                                       args, /*isvararg*/ true);
  Value *fun = TheModule->getOrInsertFunction("fprintf", ft);
  SmallVector<Value *, 8> params;
  Value *f = Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A0)]);
  if (First)
    *First = GetFirstInstruction(*First, f);
  params.push_back(f);
  Value *addrbuf = IREmitter.AccessShadowMemory(
      Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A1)]), false);
  params.push_back(
      Builder.CreatePtrToInt(addrbuf, Type::getInt32Ty(getGlobalContext())));
  params.push_back(
      Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A2)]));
  params.push_back(
      Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A3)]));
  V = Builder.CreateStore(Builder.CreateCall(fun, params),
                          IREmitter.Regs[ConvToDirective(Mips::V0)]);
  ReadMap[ConvToDirective(Mips::A0)] = true;
  ReadMap[ConvToDirective(Mips::A1)] = true;
  ReadMap[ConvToDirective(Mips::A2)] = true;
  ReadMap[ConvToDirective(Mips::A3)] = true;
  WriteMap[ConvToDirective(Mips::V0)] = true;
  return true;
}

// XXX: Handling a fixed number of 4 arguments, since we cannot infer how many
// arguments the program is using with printf
bool SyscallsIface::HandleLibcPrintf(Value *&V, Value **First) {
  SmallVector<Type *, 8> args(1, Type::getInt32Ty(getGlobalContext()));
  FunctionType *ft = FunctionType::get(Type::getInt32Ty(getGlobalContext()),
                                       args, /*isvararg*/ true);
  Value *fun = TheModule->getOrInsertFunction("printf", ft);
  SmallVector<Value *, 8> params;
  Value *f = Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A0)]);
  if (First)
    *First = GetFirstInstruction(*First, f);
  Value *addrbuf = IREmitter.AccessShadowMemory(f, false);
  params.push_back(
      Builder.CreatePtrToInt(addrbuf, Type::getInt32Ty(getGlobalContext())));
  params.push_back(
      Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A1)]));
  params.push_back(
      Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A2)]));
  params.push_back(
      Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A3)]));
  V = Builder.CreateStore(Builder.CreateCall(fun, params),
                          IREmitter.Regs[ConvToDirective(Mips::V0)]);
  ReadMap[ConvToDirective(Mips::A0)] = true;
  ReadMap[ConvToDirective(Mips::A1)] = true;
  ReadMap[ConvToDirective(Mips::A2)] = true;
  ReadMap[ConvToDirective(Mips::A3)] = true;
  WriteMap[ConvToDirective(Mips::V0)] = true;
  return true;
}

// XXX: Handling a fixed number of 4 arguments, since we cannot infer how many
// arguments the program is using with scanf
bool SyscallsIface::HandleLibcScanf(Value *&V, Value **First) {
  SmallVector<Type *, 8> args(1, Type::getInt32Ty(getGlobalContext()));
  FunctionType *ft = FunctionType::get(Type::getInt32Ty(getGlobalContext()),
                                       args, /*isvararg*/ true);
  Value *fun = TheModule->getOrInsertFunction("__isoc99_scanf", ft);
  SmallVector<Value *, 8> params;
  Value *f = Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A0)]);
  if (First)
    *First = GetFirstInstruction(*First, f);
  Value *addrbuf0 = IREmitter.AccessShadowMemory(f, false);
  Value *addrbuf1 = IREmitter.AccessShadowMemory(
      Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A1)]), false);
  Value *addrbuf2 = IREmitter.AccessShadowMemory(
      Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A2)]), false);
  Value *addrbuf3 = IREmitter.AccessShadowMemory(
      Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A3)]), false);
  params.push_back(
      Builder.CreatePtrToInt(addrbuf0, Type::getInt32Ty(getGlobalContext())));
  params.push_back(
      Builder.CreatePtrToInt(addrbuf1, Type::getInt32Ty(getGlobalContext())));
  params.push_back(
      Builder.CreatePtrToInt(addrbuf2, Type::getInt32Ty(getGlobalContext())));
  params.push_back(
      Builder.CreatePtrToInt(addrbuf3, Type::getInt32Ty(getGlobalContext())));
  V = Builder.CreateStore(Builder.CreateCall(fun, params),
                          IREmitter.Regs[ConvToDirective(Mips::V0)]);
  ReadMap[ConvToDirective(Mips::A0)] = true;
  ReadMap[ConvToDirective(Mips::A1)] = true;
  ReadMap[ConvToDirective(Mips::A2)] = true;
  ReadMap[ConvToDirective(Mips::A3)] = true;
  WriteMap[ConvToDirective(Mips::V0)] = true;
  return true;
}

bool SyscallsIface::HandleXstat(Value *&V, Value **First) {
  SmallVector<Type *, 8> args(3, Type::getInt32Ty(getGlobalContext()));
  FunctionType *ft = FunctionType::get(Type::getInt32Ty(getGlobalContext()),
                                       args, /*isvararg*/false);
  Value *fun = TheModule->getOrInsertFunction("__xstat", ft);
  SmallVector<Value *, 8> params;
  Value *f = Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A0)]);
  if (First)
    *First = GetFirstInstruction(*First, f);
  params.push_back(f);
  params.push_back(Builder.CreatePtrToInt(
      IREmitter.AccessShadowMemory(
          Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A1)]), false),
      Type::getInt32Ty(getGlobalContext())));
  Value *StatStruct = Builder.CreatePtrToInt(
      IREmitter.AccessShadowMemory(
          Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A2)]), false),
      Type::getInt32Ty(getGlobalContext()));
  params.push_back(StatStruct);
  V = Builder.CreateStore(Builder.CreateCall(fun, params),
                          IREmitter.Regs[ConvToDirective(Mips::V0)]);
  // convert st_size from offset 44 (x86) to 48 (OpenISA-MIPS)
  // TODO: convert other fields! convert for ARM! need to check target!
  Builder.CreateStore(
      Builder.CreateLoad(Builder.CreateIntToPtr(
          Builder.CreateAdd(
              StatStruct,
              ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 44)),
          Type::getInt32PtrTy(getGlobalContext()))),
      Builder.CreateIntToPtr(
          Builder.CreateAdd(
              StatStruct,
              ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 48)),
          Type::getInt32PtrTy(getGlobalContext())));
  ReadMap[ConvToDirective(Mips::A0)] = true;
  ReadMap[ConvToDirective(Mips::A1)] = true;
  ReadMap[ConvToDirective(Mips::A2)] = true;
  WriteMap[ConvToDirective(Mips::V0)] = true;
  return true;
}

bool SyscallsIface::HandleGenericDouble(Value *&V, StringRef Name, int numargs,
                                        int numret, ArgType *ArgTypes,
                                        Value **First) {
  SmallVector<Type *, 8> args;
  for (int I = 0, E = numargs; I != E; ++I) {
    switch (ArgTypes[I]) {
    case AT_Int32:
    case AT_Ptr:
      args.push_back(Type::getInt32Ty(getGlobalContext()));
      break;
    case AT_Float:
      args.push_back(Type::getFloatTy(getGlobalContext()));
      break;
    case AT_Double:
      args.push_back(Type::getDoubleTy(getGlobalContext()));
      break;
    default:
      llvm_unreachable("Unhandled arg type for HandleGenericDouble");
    }
  }

  FunctionType *ft;
  if (numret == 0)
    ft = FunctionType::get(Type::getVoidTy(getGlobalContext()), args,
                           /*isvararg*/ false);
  else if (numret == 1) {
    switch(ArgTypes[numargs]) {
    case AT_Double:
      ft = FunctionType::get(Type::getDoubleTy(getGlobalContext()), args,
                             /*isvararg*/ false);
      break;
    case AT_Float:
      ft = FunctionType::get(Type::getFloatTy(getGlobalContext()), args,
                             /*isvararg*/ false);
      break;
    case AT_Ptr:
    case AT_Int32:
      ft = FunctionType::get(Type::getInt32Ty(getGlobalContext()), args,
                             /*isvararg*/ false);
      break;
    default:
      llvm_unreachable("Unhandled return type.");
    }
  }
  else
    llvm_unreachable("Unhandled return size.");

  AttributeSet attrs;
  if (CodeTarget == "arm") {
    attrs = attrs.addAttribute(getGlobalContext(), AttributeSet::FunctionIndex,
                               Attribute::NoUnwind)
                .addAttribute(getGlobalContext(), AttributeSet::FunctionIndex,
                              "less-precise-fpmad", "false")
                .addAttribute(getGlobalContext(), AttributeSet::FunctionIndex,
                              "no-frame-pointer-elim", "true")
                .addAttribute(getGlobalContext(), AttributeSet::FunctionIndex,
                              "no-frame-pointer-elim-non-leaf", "true")
                .addAttribute(getGlobalContext(), AttributeSet::FunctionIndex,
                              "no-infs-fp-math", "false")
                .addAttribute(getGlobalContext(), AttributeSet::FunctionIndex,
                              "no-nans-fp-math", "false")
                .addAttribute(getGlobalContext(), AttributeSet::FunctionIndex,
                              "stack-protector-buffer-size", "8")
                .addAttribute(getGlobalContext(), AttributeSet::FunctionIndex,
                              "unsafe-fp-math", "false")
                .addAttribute(getGlobalContext(), AttributeSet::FunctionIndex,
                              "use-soft-float", "false");
  }

  Value *fun = TheModule->getOrInsertFunction(Name, ft, attrs);
  SmallVector<Value *, 8> params;
  assert(numargs <= 4 && "Cannot handle more than 4 arguments");
  if (numargs > 0) {
    unsigned numInts = 0;
    unsigned numDoubles = 0;
    unsigned numFloats = 0;
    for (int I = 0, E = numargs; I != E; ++I) {
      switch (ArgTypes[I]) {
      case AT_Ptr: {
        Value *f = Builder.CreateLoad(
            IREmitter.Regs[ConvToDirective(Mips::A0) + numInts +
                           (numDoubles << 1) + numFloats]);
        if (I == 0 && First)
          *First = GetFirstInstruction(*First, f);
        Value *addrbuf = IREmitter.AccessShadowMemory(f, false);
        params.push_back(Builder.CreatePtrToInt(
            addrbuf, Type::getInt32Ty(getGlobalContext())));
        ReadMap[ConvToDirective(Mips::A0) + numInts++ + (numDoubles << 1) +
                numFloats] = true;
        break;
      }
      case AT_Int32: {
        Value *f = Builder.CreateLoad(
            IREmitter.Regs[ConvToDirective(Mips::A0) + numInts +
                           (numDoubles << 1) + numFloats]);
        if (I == 0 && First)
          *First = GetFirstInstruction(*First, f);
        params.push_back(f);
        ReadMap[ConvToDirective(Mips::A0) + numInts++ + (numDoubles << 1) +
                numFloats] = true;
        break;
      }
      case AT_Float: {
        Value *f =
            Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::F12) +
                                              (numDoubles << 1) + numFloats]);
        if (I == 0 && First)
          *First = GetFirstInstruction(*First, f);
        params.push_back(f);
        ReadMap[ConvToDirective(Mips::F12) + (numDoubles << 1) + numFloats++] =
            true;
        break;
      }
      case AT_Double: {
        Value *f = Builder.CreateLoad(
            IREmitter.DblRegs[ConvToDirectiveDbl(Mips::F12) + numDoubles +
                              ((numFloats + 1) >> 1)]);
        if (I == 0 && First)
          *First = GetFirstInstruction(*First, f);
        params.push_back(f);
        IREmitter.DblReadMap[ConvToDirectiveDbl(Mips::F12) + numDoubles++ +
                             ((numFloats + 1) >> 1)] = true;
        break;
      }
      default:
        llvm_unreachable("Unhandled arg type for HandleDoubleGeneric");
      }
    }
    V = Builder.CreateCall(fun, params);
  } else {
    V = Builder.CreateCall(fun, params);
    if (First)
      *First = GetFirstInstruction(*First, V);
  }
  if (numret > 0) {
    switch (ArgTypes[numargs]) {
    case AT_Double:
      Builder.CreateStore(V, IREmitter.DblRegs[ConvToDirectiveDbl(Mips::F0)]);
      IREmitter.DblWriteMap[ConvToDirectiveDbl(Mips::F0)] = true;
      break;
    case AT_Float:
      Builder.CreateStore(V, IREmitter.Regs[ConvToDirective(Mips::F0)]);
      IREmitter.WriteMap[ConvToDirective(Mips::F0)] = true;
      break;
    case AT_Int32:
      Builder.CreateStore(V, IREmitter.Regs[ConvToDirective(Mips::V0)]);
      IREmitter.WriteMap[ConvToDirective(Mips::V0)] = true;
      break;
    case AT_Ptr:
      if (NoShadow) {
        V = Builder.CreateStore(V, IREmitter.Regs[ConvToDirective(Mips::V0)]);
      } else {
        Value *zero = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0);
        Value *cmp = Builder.CreateICmpEQ(V, zero);
        Value *ptr = Builder.CreatePtrToInt(
            IREmitter.ShadowImageValue, Type::getInt32Ty(getGlobalContext()));
        Value *fixed = Builder.CreateSub(V, ptr);
        Value *final = Builder.CreateSelect(cmp, zero, fixed);
        V = Builder.CreateStore(final,
                                IREmitter.Regs[ConvToDirective(Mips::V0)]);
      }
      break;
      IREmitter.WriteMap[ConvToDirective(Mips::V0)] = true;
    default:
      llvm_unreachable("Unhandled return type for HandleGenericDouble");
    }

  }

  return true;
}

bool SyscallsIface::HandleLibcAtof(Value *&V, Value **First) {
  SmallVector<Type *, 8> args(1, Type::getInt32Ty(getGlobalContext()));
  FunctionType *ft = FunctionType::get(Type::getDoubleTy(getGlobalContext()),
                                       args, /*isvararg*/ true);
  Value *fun = TheModule->getOrInsertFunction("atof", ft);
  SmallVector<Value *, 8> params;

  Value *f = Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A0)]);
  if (First)
    *First = GetFirstInstruction(*First, f);
  Value *addrbuf = IREmitter.AccessShadowMemory(f, false);
  params.push_back(
      Builder.CreatePtrToInt(addrbuf, Type::getInt32Ty(getGlobalContext())));

  Value *call = Builder.CreateCall(fun, params);
  V = call;
  Builder.CreateStore(call, IREmitter.DblRegs[ConvToDirectiveDbl(Mips::F0)]);

  ReadMap[ConvToDirective(Mips::A0)] = true;
  IREmitter.DblWriteMap[ConvToDirectiveDbl(Mips::F0)] = true;
  return true;
}

bool SyscallsIface::HandleSyscallWrite(Value *&V, Value **First) {
  SmallVector<Type *, 8> args(3, Type::getInt32Ty(getGlobalContext()));
  FunctionType *ft = FunctionType::get(Type::getInt32Ty(getGlobalContext()),
                                       args, /*isvararg*/ false);
  Value *fun = TheModule->getOrInsertFunction("write", ft);
  SmallVector<Value *, 8> params;
  Value *f = Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A0)]);
  if (First)
    *First = GetFirstInstruction(*First, f);
  params.push_back(f);
  Value *addrbuf = IREmitter.AccessShadowMemory(
      Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A1)]), false);
  params.push_back(
      Builder.CreatePtrToInt(addrbuf, Type::getInt32Ty(getGlobalContext())));
  params.push_back(
      Builder.CreateLoad(IREmitter.Regs[ConvToDirective(Mips::A2)]));

  V = Builder.CreateStore(Builder.CreateCall(fun, params),
                          IREmitter.Regs[ConvToDirective(Mips::V0)]);
  ReadMap[ConvToDirective(Mips::A0)] = true;
  ReadMap[ConvToDirective(Mips::A1)] = true;
  ReadMap[ConvToDirective(Mips::A2)] = true;
  WriteMap[ConvToDirective(Mips::V0)] = true;
  return true;
}
