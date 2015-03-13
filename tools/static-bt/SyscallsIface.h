//=== SyscallsIface.h - ---------------------------------------- -*- C++ -*-==//
//
// Generate code to call host syscalls or libc functions during static
// binary translation.
//
//===----------------------------------------------------------------------===//
#ifndef SYSCALLSIFACE_H
#define SYSCALLSIFACE_H

#include "OiIREmitter.h"
#include "llvm/IR/Value.h"

namespace llvm {

class SyscallsIface {
public:
  enum ArgType { AT_Int32, AT_Double, AT_Ptr, AT_PtrPtr };

  SyscallsIface(OiIREmitter &ir, StringRef CodeTarget)
      : CodeTarget(CodeTarget), IREmitter(ir), TheModule(ir.TheModule),
        Builder(ir.Builder), ReadMap(ir.ReadMap), WriteMap(ir.WriteMap) {}

  bool HandleSyscallWrite(Value *&V, Value **First = 0);
  bool HandleLibcAtoi(Value *&V, Value **First = 0);
  bool HandleLibcMalloc(Value *&V, Value **First = 0);
  bool HandleLibcCalloc(Value *&V, Value **First = 0);
  bool HandleLibcFree(Value *&V, Value **First = 0);
  bool HandleLibcExit(Value *&V, Value **First = 0);
  bool HandleGenericInt(Value *&V, StringRef Name, int numargs, int numret,
                        ArgType *ArgTypes, Value **First);
  bool HandleGenericDouble(Value *&V, StringRef Name, int numargs, int numret,
                           ArgType *ArgTypes, Value **First);
  bool HandleCTypeToUpperLoc(Value *&V, Value **First);
  bool HandleLibcPuts(Value *&V, Value **First = 0);
  bool HandleLibcMemset(Value *&V, Value **First = 0);
  bool HandleLibcFprintf(Value *&V, Value **First = 0);
  bool HandleLibcPrintf(Value *&V, Value **First = 0);
  bool HandleLibcScanf(Value *&V, Value **First = 0);
  bool HandleLibcAtof(Value *&V, Value **First);

private:
  Function *createTranslateCTypeFunction();

  StringRef CodeTarget;
  OiIREmitter &IREmitter;
  std::unique_ptr<Module> &TheModule;
  IRBuilder<> &Builder;
  DenseMap<int32_t, bool> &ReadMap, &WriteMap;
};
}

#endif
