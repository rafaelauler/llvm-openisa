//===-- OiInstTranslate.cpp - Convert Oi MCInst to LLVM IR ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This class translates an Oi MCInst to LLVM IR using static binary translation
// techniques.
//
//===----------------------------------------------------------------------===//

//#define NDEBUG

#define DEBUG_TYPE "staticbt"
#include "OiInstTranslate.h"
#include "OiInstrInfo.h"
#include "StringRefMemoryObject.h"
#include "SBTUtils.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Object/ELF.h"
using namespace llvm;

static cl::opt<bool>
DebugIR("debug-ir", cl::desc("Print the generated IR for each function, prior to optimizations"));

void OiInstTranslate::StartFunction(Twine &N) {
  IREmitter.StartFunction(N);
}

void OiInstTranslate::FinishFunction() {
  if (!OneRegion) {
    IREmitter.CleanRegs();
    IREmitter.FixBBTerminators();
    if (DebugIR) 
      IREmitter.Builder.GetInsertBlock()->getParent()->dump();
  }
  //Builder.CreateRetVoid();
}

void OiInstTranslate::FinishModule() {
  if (OneRegion) {
    IREmitter.CleanRegs();
    IREmitter.FixBBTerminators();
    IREmitter.BuildReturnTablesOneRegion();
    if (DebugIR) 
      IREmitter.Builder.GetInsertBlock()->getParent()->dump();
  }
}

Module* OiInstTranslate::takeModule() {
  return IREmitter.TheModule.take();
}

bool OiInstTranslate::HandleAluSrcOperand(const MCOperand &o, Value *&V, Value **First) {
  if (o.isReg()) {
    unsigned reg = ConvToDirective(conv32(o.getReg()));
    if (reg == 0) {
      V = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0);
      return true;
    }
    V = Builder.CreateLoad(IREmitter.Regs[reg]);
    ReadMap[reg] = true;
    if (First != 0)
      *First = GetFirstInstruction(*First, V);
    return true;
  } else if (o.isImm()) {
    uint64_t myimm = o.getImm();
    uint64_t reltype = 0;
    if (RelocReader.ResolveRelocation(myimm, &reltype)) {
      if (reltype == ELF::R_MIPS_LO16) {
        Value *V0 = ConstantInt::get(Type::getInt32Ty(getGlobalContext()),
                                     myimm + o.getImm());
        Value *V1 = 0, *fixedV0 = 0;
        if (NoShadow) {
          Value *shadow = Builder.CreatePtrToInt(IREmitter.ShadowImageValue,
                                                 Type::getInt32Ty(getGlobalContext()));
          fixedV0 = Builder.CreateAdd(V0, shadow);
          V1 = fixedV0;
        } else {
          V1 = V0;
        }
        if (First != 0)
          *First = GetFirstInstruction(*First, fixedV0, V1);
        V = V1;
        return true;
      }
    }
    V = ConstantInt::get(Type::getInt32Ty(getGlobalContext()),
                         myimm);
    return true;
  } else if (o.isFPImm()) {
    V = ConstantFP::get(getGlobalContext(), APFloat(o.getFPImm()));
    return true;
  }
  llvm_unreachable("Invalid Src operand");
}

bool OiInstTranslate::HandleDoubleSrcOperand(const MCOperand &o, Value *&V, Value **First) {
  if (o.isReg()) {
    unsigned reg = ConvToDirectiveDbl(conv32(o.getReg()));
    V = Builder.CreateLoad(IREmitter.DblRegs[reg]);
    if (First != 0)
      *First = GetFirstInstruction(*First, V);
    IREmitter.DblReadMap[reg] = true;
    return true;
  } 
  llvm_unreachable("Invalid Src operand");
}

bool OiInstTranslate::HandleFloatSrcOperand(const MCOperand &o, Value *&V, Value **First) {
  if (o.isReg()) {
    unsigned reg = ConvToDirective(conv32(o.getReg()));
    Value *v = Builder.CreateLoad(IREmitter.Regs[reg]);
    // Assume little endian for doubles
    V = Builder.CreateBitCast(v, Type::getFloatTy(getGlobalContext()));
    if (First != 0)
      *First = GetFirstInstruction(*First, V);
    ReadMap[reg] = true;
    return true;
  } 
  llvm_unreachable("Invalid Src operand");
}

bool OiInstTranslate::HandleDoubleDstOperand(const MCOperand &o, Value *&V) {
  if (o.isReg()) {
    unsigned reg = ConvToDirectiveDbl(conv32(o.getReg()));
    // Assume little endian doubles
    V = IREmitter.DblRegs[reg];
    IREmitter.DblWriteMap[reg] = true;
    return true;
  }
  llvm_unreachable("Invalid dst operand");
}

bool OiInstTranslate::HandleFloatDstOperand(const MCOperand &o, Value *&V) {
  if (o.isReg()) {
    unsigned reg = ConvToDirective(conv32(o.getReg()));
    V = IREmitter.Regs[reg];
    WriteMap[reg] = true;
    return true;
  }
  llvm_unreachable("Invalid dst operand");
}

bool OiInstTranslate::HandleDoubleMemOperand(const MCOperand &o, const MCOperand &o2,
                                             Value *&Low, Value **First,
                                             bool IsLoad) {
  if (o.isReg() && o2.isImm()) {
    uint64_t myimm = o2.getImm();
    uint64_t reltype = 0;
    Value *idx, *addr, *base;
    if (RelocReader.ResolveRelocation(myimm, &reltype)) {
      if (reltype == ELF::R_MIPS_LO16) {
        Value *V0 = ConstantInt::get(Type::getInt32Ty(getGlobalContext()),
                                     myimm + o2.getImm());
        Value *V1 = 0, *fixedV0 = 0;
        if (NoShadow) {
          Value *shadow = Builder.CreatePtrToInt(IREmitter.ShadowImageValue,
                                                 Type::getInt32Ty(getGlobalContext()));
          fixedV0 = Builder.CreateAdd(V0, shadow);
          V1 = fixedV0;
        } else {
          V1 = V0;
        }
        idx = V1;
        //Assume little endian doubles
        unsigned reg = ConvToDirective(conv32(o.getReg()));
        if (reg == 0) {
          base = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0);
        } else {
          base = Builder.CreateLoad(IREmitter.Regs[reg]);
          ReadMap[reg] = true;
        }
        addr = Builder.CreateAdd(base, idx);
        if (First != 0) {
          *First = GetFirstInstruction(*First, fixedV0, V1, base, addr);
        }
      } else {
        llvm_unreachable("Don't know how to handle this relocation");
      }      
    } else {
      idx = ConstantInt::get(Type::getInt32Ty(getGlobalContext()),
                             myimm);
      //Assume little endian doubles
      unsigned reg = ConvToDirective(conv32(o.getReg()));
      if (reg == 0) {
        base = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0);
      } else {
        base = Builder.CreateLoad(IREmitter.Regs[reg]);
        ReadMap[reg] = true;
      }
      addr = Builder.CreateAdd(base, idx);
      if (First != 0)
        *First = GetFirstInstruction(*First, base, addr);
    }
    Low = IREmitter.AccessShadowMemory(addr, IsLoad, 64, false, First);
    return true;
  } 

  llvm_unreachable("Invalid Src operand");
}

bool OiInstTranslate::HandleFloatMemOperand(const MCOperand &o, const MCOperand &o2,
                                            Value *&V, Value **First, bool IsLoad) {
  if (o.isReg() && o2.isImm()) {
    uint64_t myimm = o2.getImm();
    uint64_t reltype = 0;
    Value *idx, *addr, *base;
    if (RelocReader.ResolveRelocation(myimm, &reltype)) {
      if (reltype == ELF::R_MIPS_LO16) {
        Value *V0 = ConstantInt::get(Type::getInt32Ty(getGlobalContext()),
                                     myimm + o2.getImm());
        Value *V1 = 0, *fixedV0 = 0;
        if (NoShadow) {
          Value *shadow = Builder.CreatePtrToInt(IREmitter.ShadowImageValue,
                                                 Type::getInt32Ty(getGlobalContext()));
          fixedV0 = Builder.CreateAdd(V0, shadow);
          V1 = fixedV0;
        } else {
          V1 = V0;
        }
        idx = V1;
        //Assume little endian doubles
        unsigned reg = ConvToDirective(conv32(o.getReg()));
        if (reg == 0) {
          base = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0);
        } else {
          base = Builder.CreateLoad(IREmitter.Regs[reg]);
          ReadMap[reg] = true;
        }
        addr = Builder.CreateAdd(base, idx);
        if (First != 0) {
          *First = GetFirstInstruction(*First, fixedV0, V1, base, addr);
        }
      } else {
        llvm_unreachable("Don't know how to handle this relocation");
      }      
    } else {
      idx = ConstantInt::get(Type::getInt32Ty(getGlobalContext()),
                             myimm);
      unsigned reg = ConvToDirective(conv32(o.getReg()));
      if (reg == 0) {
        base = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0);
      } else {
        base = Builder.CreateLoad(IREmitter.Regs[reg]);
        ReadMap[reg] = true;
      }
      addr = Builder.CreateAdd(base, idx);
      if (First != 0)
        *First = GetFirstInstruction(*First, base, addr);
    }
    V = IREmitter.AccessShadowMemory(addr, IsLoad, 32, true, First);
    return true;
  } 

  llvm_unreachable("Invalid Src operand");
}

bool OiInstTranslate::HandleSaveDouble(Value *In, Value *&Low, Value *&High) {
  Value *v1 = Builder.CreateBitCast(In, Type::getInt64Ty(getGlobalContext()));
  Value *v2 = Builder.CreateLShr(v1, ConstantInt::get
                                 (Type::getInt64Ty(getGlobalContext()), 32));
  // Assume little endian for doubles
  High = Builder.CreateSExtOrTrunc(v2, Type::getInt32Ty(getGlobalContext()));
  Low = Builder.CreateSExtOrTrunc(v1, Type::getInt32Ty(getGlobalContext()));
  return true;
}

bool OiInstTranslate::HandleSaveFloat(Value *In, Value *&V) {
  V = In;
  return true;
}

bool OiInstTranslate::HandleMemExpr(const MCExpr &exp, Value *&V, bool IsLoad) {
  if (const MCConstantExpr *ce = dyn_cast<const MCConstantExpr>(&exp)) {
    Value *idx = ConstantInt::get(Type::getInt32Ty(getGlobalContext()),
                                  ce->getValue());
    V = IREmitter.AccessShadowMemory(idx, IsLoad);
    return true;
  } else if (const MCSymbolRefExpr *se = dyn_cast<const MCSymbolRefExpr>(&exp)){
    V = IREmitter.TheModule->getOrInsertGlobal(se->getSymbol().getName(),
                                     Type::getInt32Ty(getGlobalContext()));
    if (se->getKind() == MCSymbolRefExpr::VK_Mips_ABS_HI) {
      Value *V0 = Builder.CreateCast(Instruction::PtrToInt, V,
                                     Type::getInt32Ty(getGlobalContext()));
      Value *V1 = Builder.CreateLShr(V0, ConstantInt::get
                                     (Type::getInt32Ty(getGlobalContext()), 16));
      Value *V2 = Builder.CreateShl(V1, ConstantInt::get
                                    (Type::getInt32Ty(getGlobalContext()), 16));
      V = V2;      
    } else if (se->getKind() == MCSymbolRefExpr::VK_Mips_ABS_LO) {
      Value *V0 = Builder.CreateCast(Instruction::PtrToInt, V,
                                     Type::getInt32Ty(getGlobalContext()));
      Value *V1 = Builder.CreateAnd(V0, ConstantInt::get
                                    (Type::getInt32Ty(getGlobalContext()), 
                                     0xFFFF));
      V = V1;
    } else if (se->getKind() != MCSymbolRefExpr::VK_None) {
      llvm_unreachable("Unhandled SymbolRef Kind");
    }
    return true;
    //    GlobalVariable(IREmitter.TheModule,
    //               Type::getInt32Ty(getGlobalContext()),
    //               false,
    //              GlobalValue::LinkageTypes::ExternalLinkage,
    //              Constant::getNullValue(Type::getInt32Ty(getGlobalContext())),
    //             se->getSymbol().getName(),
    //             0, GlobalVariable::NotThreadLocal, 0, true);
  }
  llvm_unreachable("Invalid Load Expr");
}

bool OiInstTranslate::HandleLUiOperand(const MCOperand &o, Value *&V, Value **First,
                                       bool IsLoad) {
  if (o.isImm()) {
    uint64_t addr = o.getImm();

    if (RelocReader.ResolveRelocation(addr)) {
      addr += o.getImm();
      Value *idx = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), addr);    
      if (NoShadow) {
        Value *shadow = Builder.CreatePtrToInt(IREmitter.ShadowImageValue,
                                               Type::getInt32Ty(getGlobalContext()));
        Value *fixedIdx = Builder.CreateAdd(idx, shadow);
        idx = fixedIdx;
      } 
      Value *V1 = Builder.CreateLShr(idx, ConstantInt::get
                                     (Type::getInt32Ty(getGlobalContext()), 16));
      Value *V2 = Builder.CreateShl(V1, ConstantInt::get
                                    (Type::getInt32Ty(getGlobalContext()), 16));
      if (First != 0)
        *First = GetFirstInstruction(*First, V1, idx, V2);
      V = V2;
    } else {
      Value *idx = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), addr);    
      if (NoShadow) {
        Value *shadow = Builder.CreatePtrToInt(IREmitter.ShadowImageValue,
                                               Type::getInt32Ty(getGlobalContext()));
        Value *fixedIdx = Builder.CreateAdd(idx, shadow);
        idx = fixedIdx;
      } 
      Value *V2 = Builder.CreateShl(idx, ConstantInt::get
                                    (Type::getInt32Ty(getGlobalContext()), 16));
      if (First != 0)
        *First = GetFirstInstruction(*First, idx, V2);
      V = V2;
    }
    return true;
  }
  llvm_unreachable("Invalid Src operand");
}

bool OiInstTranslate::HandleMemOperand(const MCOperand &o, const MCOperand &o2,
                                       Value *&V, Value **First, bool IsLoad,
                                       int width) {
  if (o.isReg() && o2.isImm()) {
    uint32_t r = ConvToDirective(conv32(o.getReg()));
    if (!NoLocals && AggrOptimizeStack && (r == 29 || r == 30) && width == 32)
      return HandleSpilledOperand(o, o2, V, First, IsLoad);
    uint64_t myimm = o2.getImm();
    uint64_t reltype = 0;
    Value *idx, *addr;
    if (RelocReader.ResolveRelocation(myimm, &reltype)) {
      if (reltype == ELF::R_MIPS_LO16) {
        Value *V0 = ConstantInt::get(Type::getInt32Ty(getGlobalContext()),
                                     myimm + o2.getImm());
        Value *V1 = 0;
        if (NoShadow) {
          Value *shadow = Builder.CreatePtrToInt(IREmitter.ShadowImageValue,
                                                 Type::getInt32Ty(getGlobalContext()));
          Value *fixedV0 = Builder.CreateAdd(V0, shadow);
          V0 = fixedV0;
        }
        V1 = V0;
        if (First != 0)
          *First = GetFirstInstruction(*First, V0, V1);
        idx = V1;
        unsigned reg = ConvToDirective(conv32(o.getReg()));
        Value *base;
        if (reg == 0) {
          base = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0);
        } else {
          base = Builder.CreateLoad(IREmitter.Regs[reg]);
          ReadMap[reg] = true;
        }
        if (!isa<Instruction>(*First)) {
          *First = base;
        }
        addr = Builder.CreateAdd(base, idx);
        if (!isa<Instruction>(*First)) {
          *First = addr;
        }
      } else {
        llvm_unreachable("Don't know how to handle this relocation");
      }      
    } else {
      idx = ConstantInt::get(Type::getInt32Ty(getGlobalContext()),
                             myimm);
      unsigned reg = ConvToDirective(conv32(o.getReg()));
      Value *base;
      if (reg == 0) {
        base = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0);
      } else {
        base = Builder.CreateLoad(IREmitter.Regs[reg]);
        ReadMap[reg] = true;
      }
      addr = Builder.CreateAdd(base, idx);
      if (First != 0)
        *First = GetFirstInstruction(*First, base, addr);
    }
    V = IREmitter.AccessShadowMemory(addr, IsLoad, width, false, First);
    return true;
  } 
  llvm_unreachable("Invalid Src operand");
}

bool OiInstTranslate::HandleSpilledOperand(const MCOperand &o, const MCOperand &o2,
                                           Value *&V, Value **First, bool IsLoad) {
  if (NoLocals || !(OptimizeStack || AggrOptimizeStack))
    return HandleMemOperand(o, o2, V, First, IsLoad);
  assert(o.isReg() && o2.isImm() && "Invalid spilled operand.");
  unsigned reg = ConvToDirective(conv32(o.getReg()));
  assert((reg == 29 || reg == 30) 
         && "Invalid spilled operand, reg should be SP or FP.");
  uint64_t Idx = o2.getImm();
  if (reg == 30)
    Idx += 1000000;
  uint64_t reltype = 0;
  assert(!RelocReader.ResolveRelocation(Idx, &reltype) && 
         "Invalid spilled operand");
  V = IREmitter.AccessSpillMemory(Idx, IsLoad);
  if (First)
    *First = GetFirstInstruction(*First, V);
  return true;
}

bool OiInstTranslate::HandleGetSpilledAddress(const MCOperand &o, const MCOperand &o2,
                                        const MCOperand &dst, Value *&V,
                                        Value **First) {
  if (!(OptimizeStack || AggrOptimizeStack))
    return false;
  if (!o.isReg() || !o2.isImm() || !dst.isReg())
    return false;
  unsigned r1 = ConvToDirective(conv32(o.getReg()));
  unsigned dstReg = ConvToDirective(conv32(dst.getReg()));
  unsigned imm = o2.getImm();
  if (dstReg == 29 || dstReg == 30)
    return false;
  if (r1 != 29 && r1 != 30)
    return false;
  if (r1 == 30)
    imm += 100000;
  Value* ptr = IREmitter.AccessSpillMemory(imm, false);
  Value *castptr = Builder.CreatePtrToInt(ptr,
                                          Type::getInt32Ty(getGlobalContext()));
  if (!NoShadow) {
    Value *shadow = Builder.CreatePtrToInt(IREmitter.ShadowImageValue,
                                           Type::getInt32Ty(getGlobalContext()));
    Value *fixed = Builder.CreateSub(castptr, shadow);
    V = Builder.CreateStore(fixed, IREmitter.Regs[dstReg]);
  } else {
    V = Builder.CreateStore(castptr, IREmitter.Regs[dstReg]);    
  }
  *First = GetFirstInstruction(*First, ptr, castptr, V);

  return true;
}

bool OiInstTranslate::HandleAluDstOperand(const MCOperand &o, Value *&V) {
  if (o.isReg()) {
    unsigned reg = ConvToDirective(conv32(o.getReg()));
    assert (reg != 0 && "Cannot write to register 0");
    V = IREmitter.Regs[reg];
    WriteMap[reg] = true;
    return true;
  }
  llvm_unreachable("Invalid Dst operand");
  return false;
}

bool OiInstTranslate::HandleCallTarget(const MCOperand &o, Value *&V, Value **First) {
  if (o.isImm()) {
    if (o.getImm() != 0U) {
      uint64_t targetaddr;
      if (RelocReader.ResolveRelocation(targetaddr))
        return IREmitter.HandleLocalCall(o.getImm() + targetaddr, V, First);
      return IREmitter.HandleLocalCall(o.getImm(), V, First);
    } else { // Need to handle the relocation to find the correct jump address
      relocation_iterator ri = (*IREmitter.CurSection)->end_relocations();
      StringRef val;
      if (RelocReader.CheckRelocation(ri, val)) {
        if (val == "write") 
          return Syscalls.HandleSyscallWrite(V, First);        
        if (val == "atoi")
          return Syscalls.HandleLibcAtoi(V, First);
        if (val == "malloc")
          return Syscalls.HandleLibcMalloc(V, First);
        if (val == "calloc")
          return Syscalls.HandleLibcCalloc(V, First);
        if (val == "free")
          return Syscalls.HandleLibcFree(V, First);
        if (val == "exit")
          return Syscalls.HandleLibcExit(V, First);
        if (val == "puts")
          return Syscalls.HandleLibcPuts(V, First);
        if (val == "memset")
          return Syscalls.HandleLibcMemset(V, First);
        if (val == "printf")
          return Syscalls.HandleLibcPrintf(V, First);
        if (val == "fprintf")
          return Syscalls.HandleLibcFprintf(V, First);
        if (val == "__isoc99_scanf")
          return Syscalls.HandleLibcScanf(V, First);
        if (val == "atan") {
          bool PtrTypes[] = {false, false};
          return Syscalls.HandleGenericDouble(V, "atan", 1, 1, PtrTypes, First);
        }
        if (val == "sin") {
          bool PtrTypes[] = {false, false};
          return Syscalls.HandleGenericDouble(V, "sin", 1, 1, PtrTypes, First);
        }
        if (val == "cos") {
          bool PtrTypes[] = {false, false};
          return Syscalls.HandleGenericDouble(V, "cos", 1, 1, PtrTypes, First);
        }
        if (val == "acos") {
          bool PtrTypes[] = {false, false};
          return Syscalls.HandleGenericDouble(V, "acos", 1, 1, PtrTypes, First);
        }
        if (val == "pow") {
          bool PtrTypes[] = {false, false, false};
          return Syscalls.HandleGenericDouble(V, "pow", 2, 1, PtrTypes, First);
        }
        if (val == "sqrt") {
          bool PtrTypes[] = {false, false};
          return Syscalls.HandleGenericDouble(V, "sqrt", 1, 1, PtrTypes, First);
        }
        if (val == "exp") {
          bool PtrTypes[] = {false, false};
          return Syscalls.HandleGenericDouble(V, "exp", 1, 1, PtrTypes, First);
        }
        if (val == "atof")
          return Syscalls.HandleLibcAtof(V, First);
        if (val == "rand") {
          bool PtrTypes[] = {false};
          return Syscalls.HandleGenericInt(V, "rand", 0, 1, PtrTypes, First);
        }
        if (val == "srand") {
          bool PtrTypes[] = {false, false};
          return Syscalls.HandleGenericInt(V, "srand", 1, 1, PtrTypes, First);
        }
        if (val == "clock") {
          bool PtrTypes[] = {false};
          return Syscalls.HandleGenericInt(V, "clock", 0, 1, PtrTypes, First);
        }
        if (val == "fclose") {
          bool PtrTypes[] = {false, false};
          return Syscalls.HandleGenericInt(V, "fclose", 1, 1, PtrTypes, First);
        }
        if (val == "fopen") {
          bool PtrTypes[] = {true, true, false};
          return Syscalls.HandleGenericInt(V, "fopen", 2, 1, PtrTypes, First);
        }
        if (val == "fgetc") {
          bool PtrTypes[] = {false, false};
          return Syscalls.HandleGenericInt(V, "fgetc", 1, 1, PtrTypes, First);
        }
        if (val == "fputc") {
          bool PtrTypes[] = {false, false, false};
          return Syscalls.HandleGenericInt(V, "fputc", 2, 1, PtrTypes, First);
        }
        if (val == "strcmp") {
          bool PtrTypes[] = {true, true, false};
          return Syscalls.HandleGenericInt(V, "strcmp", 2, 1, PtrTypes, First);
        }
        if (val == "strncmp") {
          bool PtrTypes[] = {true, true, false, false};
          return Syscalls.HandleGenericInt(V, "strncmp", 3, 1, PtrTypes, First);
        }
        if (val == "_IO_getc") {
          bool PtrTypes[] = {false, false};
          return Syscalls.HandleGenericInt(V, "getc", 1, 1, PtrTypes, First);
        }
        if (val == "fgets") {
          bool PtrTypes[] = {true, false, false, false};
          return Syscalls.HandleGenericInt(V, "fgets", 3, 1, PtrTypes, First);
        }
        if (val == "abs") {
          bool PtrTypes[] = {false, false};
          return Syscalls.HandleGenericInt(V, "abs", 1, 1, PtrTypes, First);
        }
        if (val == "fread") {
          bool PtrTypes[] = {true, false, false, false, false};
          return Syscalls.HandleGenericInt(V, "fread", 4, 1, PtrTypes, First);
        }
        if (val == "fwrite") {
          bool PtrTypes[] = {true, false, false, false, false};
          return Syscalls.HandleGenericInt(V, "fwrite", 4, 1, PtrTypes, First);
        }
        if (val == "memcpy") {
          bool PtrTypes[] = {true, true, false, true};
          return Syscalls.HandleGenericInt(V, "memcpy", 3, 1, PtrTypes, First);
        }
        if (val == "bcopy") {
          bool PtrTypes[] = {true, true, false, false};
          return Syscalls.HandleGenericInt(V, "bcopy", 3, 0, PtrTypes, First);
        }
        if (val == "htonl") {
          bool PtrTypes[] = {false, false};
          return Syscalls.HandleGenericInt(V, "htonl", 1, 1, PtrTypes, First);
        }
        if (val == "perror") {
          bool PtrTypes[] = {true, false};
          return Syscalls.HandleGenericInt(V, "perror", 1, 0, PtrTypes, First);

        }
        if (val == "__isoc99_sscanf" ||
            val == "sscanf") {
          bool PtrTypes[] = {true, true, true, true, false};
          return Syscalls.HandleGenericInt(V, "sscanf", 4, 1, PtrTypes, First);
        }
        if (val == "__isoc99_fscanf" ||
            val == "fscanf") {
          bool PtrTypes[] = {false, true, true, true, false};
          return Syscalls.HandleGenericInt(V, "fscanf", 4, 1, PtrTypes, First);
        }
        if (val == "fflush") {
          bool PtrTypes[] = {false, false};
          return Syscalls.HandleGenericInt(V, "fflush", 1, 1, PtrTypes, First);
        }
        if (val == "feof") {
          bool PtrTypes[] = {false, false};
          return Syscalls.HandleGenericInt(V, "feof", 1, 1, PtrTypes, First);
        }
        if (val == "fgetpos") {
          bool PtrTypes[] = {false, true, false};
          return Syscalls.HandleGenericInt(V, "fgetpos", 2, 1, PtrTypes, First);
        }
        if (val == "fsetpos") {
          bool PtrTypes[] = {false, true, false};
          return Syscalls.HandleGenericInt(V, "fsetpos", 2, 1, PtrTypes, First);
        }
        if (val == "fseek") {
          bool PtrTypes[] = {false, false, false, false};
          return Syscalls.HandleGenericInt(V, "fseek", 3, 1, PtrTypes, First);
        }
        if (val == "strchr") {
          bool PtrTypes[] = {true, false, true};
          return Syscalls.HandleGenericInt(V, "strchr", 2, 1, PtrTypes, First);
        }
        if (val == "toupper") {
          bool PtrTypes[] = {false, false};
          return Syscalls.HandleGenericInt(V, "toupper", 1, 1, PtrTypes, First);
        }
        if (val == "tolower") {
          bool PtrTypes[] = {false, false};
          return Syscalls.HandleGenericInt(V, "tolower", 1, 1, PtrTypes, First);
        }
        if (val == "putchar") {
          bool PtrTypes[] = {false, false};
          return Syscalls.HandleGenericInt(V, "putchar", 1, 1, PtrTypes, First);
        }

      }
      uint64_t targetaddr;
      if (RelocReader.ResolveRelocation(targetaddr))
        return IREmitter.HandleLocalCall(targetaddr, V, First);
      llvm_unreachable("Unrecognized function call");
    }
    llvm_unreachable("Unrecognized function call");
    return false;
  }
  return false;
}

bool OiInstTranslate::HandleFCmpOperand(const MCOperand &o, Value *o0, Value *o1, Value *&V) {
  if (o.isImm()) {
    uint64_t cond = o.getImm();
    Value *cmp = 0;
    switch (cond) {
    case 0: // OI_FCOND_F  false
      cmp = ConstantInt::get(Type::getInt1Ty(getGlobalContext()), 0);
      break;
    case 1: // OI_FCOND_UN unordered - true if either nans
      cmp = Builder.CreateFCmpUNO(o0, o1);
      break;
    case 2: // OI_FCOND_OEQ equal
      cmp = Builder.CreateFCmpOEQ(o0, o1);
      break;
    case 3: // OI_FCOND_UEQ unordered or equal
      cmp = Builder.CreateFCmpUEQ(o0, o1);
      break;
    case 4: // OI_FCOND_OLT
      cmp = Builder.CreateFCmpOLT(o0, o1);
      break;
    case 5: // OI_FCOND_ULT
      cmp = Builder.CreateFCmpULT(o0, o1);
      break;
    case 6: // OI_FCOND_OLE
      cmp = Builder.CreateFCmpOLE(o0, o1);
      break;
    case 7: // OI_FCOND_ULE
      cmp = Builder.CreateFCmpULE(o0, o1);
      break;
    case 8: // OI_FCOND_SF
      // Exception not implemented
      llvm_unreachable("Unimplemented FCmp Operand");
      cmp = ConstantInt::get(Type::getInt1Ty(getGlobalContext()), 0);
      break;
    case 9: // OI_FCOND_NGLE - compare not greater or less than equal double 
            // (w/ except.)
      cmp = Builder.CreateFCmpOLE(o0, o1);
      llvm_unreachable("Unimplemented FCmp Operand");
      break;
    case 10: // OI_FCOND_SEQ
      cmp = Builder.CreateFCmpOEQ(o0, o1);
      llvm_unreachable("Unimplemented FCmp Operand");
      break;
    case 11: // OI_FCOND_NGL
      cmp = Builder.CreateFCmpULE(o0, o1);
      llvm_unreachable("Unimplemented FCmp Operand");
      break;
    case 12: // OI_FCOND_LT
      cmp = Builder.CreateFCmpULE(o0, o1);
      llvm_unreachable("Unimplemented FCmp Operand");
      break;
    case 13: // OI_FCOND_NGE
      cmp = Builder.CreateFCmpULE(o0, o1);
      llvm_unreachable("Unimplemented FCmp Operand");
      break;
    case 14: // OI_FCOND_LE
      cmp = Builder.CreateFCmpULE(o0, o1);
      llvm_unreachable("Unimplemented FCmp Operand");
      break;
    case 15: // OI_FCOND_NGT
      cmp = Builder.CreateFCmpULE(o0, o1);
      llvm_unreachable("Unimplemented FCmp Operand");
      break;
    }    
    V = cmp;
    return true;
  }
  llvm_unreachable("Unrecognized FCmp Operand");
  return false;
}

bool OiInstTranslate::HandleBranchTarget(const MCOperand &o, BasicBlock *&Target,
                                         bool IsRelative) {
  if (o.isImm()) {
    if (o.getImm() != 0U) {
      uint64_t tgtaddr;
      if (IsRelative)
        tgtaddr = (IREmitter.CurAddr + o.getImm()) & 0xFFFFFFFFULL;
      else
        tgtaddr = o.getImm();
      uint64_t rel = 0;
      if (RelocReader.ResolveRelocation(rel)) {
        tgtaddr += rel;
      }
      assert (tgtaddr != IREmitter.CurAddr);
      if (tgtaddr < IREmitter.CurAddr)
        return IREmitter.HandleBackEdge(tgtaddr, Target);
      Target = IREmitter.CreateBB(tgtaddr);
      return true;
    } else { // Need to handle the relocation to find the correct jump address
      uint64_t targetaddr;
      if (RelocReader.ResolveRelocation(targetaddr)) {
        Target = IREmitter.CreateBB(targetaddr);
        return true;
      }
    }
  }
  llvm_unreachable("Unrecognized branch target");
}


void OiInstTranslate::printInstruction(const MCInst *MI, raw_ostream &O) {
#ifndef NDEBUG
  raw_ostream &DebugOut = outs();
#else
  raw_ostream &DebugOut = nulls();
#endif

  switch (MI->getOpcode()) {
  case Oi::ADDiu:
  case Oi::ADDi:
  case Oi::ADDu:
  case Oi::ADD:
    {
      DebugOut << "Handling ADDiu, ADDi, ADDu, ADD\n";
      Value *o0, *o1, *o2, *first = 0;
      if (HandleGetSpilledAddress(MI->getOperand(1), MI->getOperand(2),
                                  MI->getOperand(0), o0, &first)) {
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
        break;
      }
      if (HandleAluSrcOperand(MI->getOperand(1), o1, &first) &&
          HandleAluSrcOperand(MI->getOperand(2), o2, &first) &&
          HandleAluDstOperand(MI->getOperand(0), o0)) {
        Value *v = Builder.CreateAdd(o1, o2);
        Value *v2 = Builder.CreateStore(v, o0);
        first = GetFirstInstruction(first, o1, o2, v, v2);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }
      break;
    }
  case Oi::SUBu:
  case Oi::SUB:
    {
      DebugOut << "Handling SUBu, SUB\n";
      Value *o0, *o1, *o2, *first = 0;
      if (HandleAluSrcOperand(MI->getOperand(1), o1, &first) &&
          HandleAluSrcOperand(MI->getOperand(2), o2, &first) &&
          HandleAluDstOperand(MI->getOperand(0), o0)) {      
        Value *v = Builder.CreateSub(o1, o2);
        Value *v2 = Builder.CreateStore(v, o0);
        first = GetFirstInstruction(first, o1, o2, v, v2);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }
      break;
    }
  case Oi::MUL:
    {
      DebugOut << "Handling MUL\n";
      Value *o0, *o1, *o2, *first = 0;
      if (HandleAluSrcOperand(MI->getOperand(1), o1, &first) &&
          HandleAluSrcOperand(MI->getOperand(2), o2, &first) &&
          HandleAluDstOperand(MI->getOperand(0), o0)) {      
        Value *v = Builder.CreateMul(o1, o2);
        Value *v2 = Builder.CreateStore(v, o0);
        first = GetFirstInstruction(first, o1, o2, v, v2);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }
      break;
    }
  case Oi::MULT:
    {
      DebugOut << "Handling MULT\n";
      Value *o0, *o1, *o2, *first = 0;
      if (HandleAluSrcOperand(MI->getOperand(0), o0, &first) &&
          HandleAluSrcOperand(MI->getOperand(1), o1, &first)) {      
        Value *o0se = Builder.CreateSExtOrTrunc(o0, Type::getInt64Ty(getGlobalContext()));
        Value *o1se = Builder.CreateSExtOrTrunc(o1, Type::getInt64Ty(getGlobalContext()));
        Value *v = Builder.CreateMul(o0se, o1se);
        Value *V1 = Builder.CreateLShr(v, ConstantInt::get
                                       (Type::getInt64Ty(getGlobalContext()), 32));
        Value *V2 = Builder.CreateSExtOrTrunc(V1, Type::getInt32Ty(getGlobalContext()));
        Value *V3 = Builder.CreateSExtOrTrunc(v, Type::getInt32Ty(getGlobalContext()));
        Value *v4 = Builder.CreateStore(V2, IREmitter.Regs[257]);
        Value *v5 = Builder.CreateStore(V3, IREmitter.Regs[256]);
        WriteMap[257] = true;
        WriteMap[256] = true;
        first = GetFirstInstruction(first, o0, o1, o0se, o1se);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }
      break;
    }
  case Oi::SDIV:
  case Oi::UDIV:
    {
      DebugOut << "Handling DIV\n";
      Value *o0, *o1, *o2, *first = 0;
      if (HandleAluSrcOperand(MI->getOperand(0), o0, &first) &&
          HandleAluSrcOperand(MI->getOperand(1), o1, &first)) {      
        Value *vdiv;
        Value *vmod; 
        if (MI->getOpcode() == Oi::SDIV) {
          vdiv = Builder.CreateSDiv(o0, o1);
          vmod = Builder.CreateSRem(o0, o1);
        } else {
          vdiv = Builder.CreateUDiv(o0, o1);
          vmod = Builder.CreateURem(o0, o1);
        }
        Value *v4 = Builder.CreateStore(vmod, IREmitter.Regs[257]);
        Value *v5 = Builder.CreateStore(vdiv, IREmitter.Regs[256]);
        WriteMap[257] = true;
        WriteMap[256] = true;
        first = GetFirstInstruction(first, o0, o1, vdiv, vmod);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }
      break;
    }
  case Oi::MFHI:
    {
      DebugOut << "Handling MFHI\n";
      Value *o0;
      if (HandleAluDstOperand(MI->getOperand(0), o0)) {
        Value *v = Builder.CreateLoad(IREmitter.Regs[257]);
        ReadMap[257] = true;
        Value *v2 = Builder.CreateStore(v, o0);
        Value *first = GetFirstInstruction(o0, v, v2);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }
      break;
    }
  case Oi::MFLO:
    {
      DebugOut << "Handling MFLO\n";
      Value *o0;
      if (HandleAluDstOperand(MI->getOperand(0), o0)) {
        Value *v = Builder.CreateLoad(IREmitter.Regs[256]);
        ReadMap[257] = true;
        Value *v2 = Builder.CreateStore(v, o0);
        Value *first = GetFirstInstruction(o0, v, v2);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }
      break;
    }
  case Oi::LDC1:
    {
      DebugOut << "Handling LDC1\n";
      Value *dst, *src, *first = 0;
      if (HandleDoubleDstOperand(MI->getOperand(0),dst) &&
          HandleDoubleMemOperand(MI->getOperand(1), MI->getOperand(2), src, &first, true)) {
        Value *v = Builder.CreateStore(src, dst);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }
      break;
    }
  case Oi::LWC1:
    {
      DebugOut << "Handling LWC1\n";
      Value *dst, *src, *first = 0;
      if (HandleFloatDstOperand(MI->getOperand(0),dst) &&
          HandleFloatMemOperand(MI->getOperand(1), MI->getOperand(2), src, &first, true)) {
        Value *v = Builder.CreateStore(src, dst);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }
      break;
    }
  case Oi::SDC1:
    {
      DebugOut << "Handling SDC1\n";
      Value *dst, *src, *first = 0;
      if (HandleDoubleSrcOperand(MI->getOperand(0), src, &first) &&
          HandleDoubleMemOperand(MI->getOperand(1), MI->getOperand(2), dst, 0, false)) {
        Builder.CreateStore(src, dst);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }
      break;
    }
  case Oi::SWC1:
    {
      DebugOut << "Handling SWC1\n";
      Value *dst, *src, *first = 0;
      if (HandleFloatSrcOperand(MI->getOperand(0), src, &first) &&
          HandleFloatMemOperand(MI->getOperand(1), MI->getOperand(2), dst, 0, false)) {
        Value *v;
        HandleSaveFloat(src, v);
        Builder.CreateStore(v, dst);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }
      break;
    }
  // XXX: Note for FCMP and MOVT: MIPS IV defines several FCC, floating-point
  // codes. We always use the 0th bit (MIPS I mode).
  // TODO: Implement all 8 CC bits.
  case Oi::FCMP_D32:
    {
      DebugOut << "Handling FCMP_D32\n";
      uint32_t cond;
      Value *o0, *o1, *first = 0;
      if (HandleDoubleSrcOperand(MI->getOperand(0), o0, &first) &&
          HandleDoubleSrcOperand(MI->getOperand(1), o1)) {
        Value *cmp;
        if (HandleFCmpOperand(MI->getOperand(2), o0, o1, cmp)) {
          Value *one = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 1U);
          Value *zero = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0U);
          Value *select = Builder.CreateSelect(cmp, one, zero);
          WriteMap[258] = true; // Ignores other FCC fields
          Builder.CreateStore(select, IREmitter.Regs[258]);
          assert(isa<Instruction>(first) && "Need to rework map logic");
          IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
        }
      }
      break;
    }
  case Oi::FCMP_S32:
    {
      DebugOut << "Handling FCMP_S32\n";
      uint32_t cond;
      Value *o0, *o1, *first = 0;
      if (HandleFloatSrcOperand(MI->getOperand(0), o0, &first) &&
          HandleFloatSrcOperand(MI->getOperand(1), o1)) {
        Value *cmp;
        if (HandleFCmpOperand(MI->getOperand(2), o0, o1, cmp)) {
          Value *one = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 1U);
          Value *zero = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0U);
          Value *select = Builder.CreateSelect(cmp, one, zero);
          WriteMap[258] = true; // Ignores other FCC fields
          Builder.CreateStore(select, IREmitter.Regs[258]);
          assert(isa<Instruction>(first) && "Need to rework map logic");
          IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
        }
      }
      break;
    }
  case Oi::MOVT_I:
    {
      DebugOut << "Handling MOVT\n";
      Value *o0, *o1, *o2, *first = 0;
      if (HandleAluSrcOperand(MI->getOperand(1), o1, &first) &&
          HandleAluSrcOperand(MI->getOperand(2), o2, &first) && // fcc0 encoded as reg1 TODO:fix
          HandleAluDstOperand(MI->getOperand(0), o0)) {        
        Value *zero = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0U);
        Value *cmp;
        Value *fcc = Builder.CreateLoad(IREmitter.Regs[258]);
        cmp = Builder.CreateICmpNE(fcc, zero);
        Value *loaddst = Builder.CreateLoad(o0);
        Value *select = Builder.CreateSelect(cmp, o1, loaddst, "movt");
        Builder.CreateStore(select, o0);
        first = GetFirstInstruction(first, o1, fcc, cmp, loaddst);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }
      break;
    }
  case Oi::FSUB_D32:
  case Oi::FADD_D32:
    {
      DebugOut << "Handling FADD_D32 FSUB_D32\n";
      Value *o0, *o1, *o2, *first = 0;
      if (HandleDoubleSrcOperand(MI->getOperand(1), o1, &first) &&       
          HandleDoubleSrcOperand(MI->getOperand(2), o2) &&       
          HandleDoubleDstOperand(MI->getOperand(0), o0)) {      
        Value *V;
        if (MI->getOpcode() == Oi::FADD_D32)
          V = Builder.CreateFAdd(o1, o2);
        else
          V = Builder.CreateFSub(o1, o2);
        Builder.CreateStore(V, o0);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }      
      break;
    }
  case Oi::FSUB_S:
  case Oi::FADD_S:
  case Oi::FMUL_S:
  case Oi::FDIV_S:
    {
      DebugOut << "Handling FADD_S FSUB_S FMUL_S FDIV_S\n";
      Value *o0, *o1, *o2, *first = 0;
      if (HandleFloatSrcOperand(MI->getOperand(1), o1, &first) &&       
          HandleFloatSrcOperand(MI->getOperand(2), o2) &&       
          HandleFloatDstOperand(MI->getOperand(0), o0)) {      
        Value *v;
        Value *V;
        if (MI->getOpcode() == Oi::FADD_S)
          V = Builder.CreateFAdd(o1, o2);
        else if (MI->getOpcode() == Oi::FSUB_S)
          V = Builder.CreateFSub(o1, o2);
        else if (MI->getOpcode() == Oi::FMUL_S)
          V = Builder.CreateFMul(o1, o2);
        else
          V = Builder.CreateFDiv(o1, o2);
        HandleSaveFloat(V, v);
        Builder.CreateStore(v, o0);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }      
      break;
    }
  case Oi::FMOV_D32:
    {
      DebugOut << "Handling FMOV\n";
      Value *o0, *o1, *first = 0;
      if (HandleDoubleSrcOperand(MI->getOperand(1), o1, &first) &&       
          HandleDoubleDstOperand(MI->getOperand(0), o0)) {      
        Value *V = o1;
        Builder.CreateStore(V, o0);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }      
      break;
    }
  case Oi::FMUL_D32:
    {
      DebugOut << "Handling FMUL\n";
      Value *o0, *o1, *o2, *first = 0;
      if (HandleDoubleSrcOperand(MI->getOperand(1), o1, &first) &&       
          HandleDoubleSrcOperand(MI->getOperand(2), o2) &&       
          HandleDoubleDstOperand(MI->getOperand(0), o0)) {      
        Value *V = Builder.CreateFMul(o1, o2);
        Builder.CreateStore(V, o0);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }      
      break;
    }
  case Oi::FDIV_D32:
    {
      DebugOut << "Handling FDIV\n";
      Value *o0, *o0lo, *o0hi, *o1, *o2, *first = 0;
      if (HandleDoubleSrcOperand(MI->getOperand(1), o1, &first) &&       
          HandleDoubleSrcOperand(MI->getOperand(2), o2) &&       
          HandleDoubleDstOperand(MI->getOperand(0), o0)) {      
        Value *V = Builder.CreateFDiv(o1, o2);
        Builder.CreateStore(V, o0);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }      
      break;
    }
  case Oi::CVT_D32_W:
    {
      DebugOut << "Handling CVT.D.W\n";
      Value *o0, *o1;
      if (HandleDoubleSrcOperand(MI->getOperand(1), o1) &&       
          HandleDoubleDstOperand(MI->getOperand(0), o0)) {      
        Value *v0 = Builder.CreateBitCast(o1, Type::getInt64Ty(getGlobalContext()));
        Value *v0_trunc;
        if (ConvToDirective(conv32(MI->getOperand(1).getReg())) % 2) {
          Value *v0_tmp = Builder.CreateLShr(v0, ConstantInt::get
                                             (Type::getInt64Ty(getGlobalContext()), 32));
          v0_trunc = Builder.CreateTrunc(v0_tmp, Type::getInt32Ty(getGlobalContext()));
        } else {
          v0_trunc = Builder.CreateTrunc(v0, Type::getInt32Ty(getGlobalContext()));
        }        
        Value *v1 = Builder.CreateSIToFP(v0_trunc, Type::getDoubleTy(getGlobalContext()));
        Value *v3 = Builder.CreateStore(v1, o0);
        Value *first = GetFirstInstruction(o1, v0);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }      
      break;
    }
  case Oi::CVT_S_W:
    {
      DebugOut << "Handling CVT.S.W\n";
      Value *o0, *o1, *first = 0;
      if (HandleAluSrcOperand(MI->getOperand(1), o1, &first) &&       
          HandleFloatDstOperand(MI->getOperand(0), o0)) {      
        Value *V;
        Value *v1 = Builder.CreateSIToFP(o1, Type::getFloatTy(getGlobalContext()));
        HandleSaveFloat(v1, V);
        Value *v2 = Builder.CreateStore(V, o0);
        Value *first = GetFirstInstruction(first, o1, v1);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }      
      break;
    }
  case Oi::CVT_D32_S:
    {
      DebugOut << "Handling CVT.D.S\n";
      Value *o0, *o1, *first = 0;
      if (HandleFloatSrcOperand(MI->getOperand(1), o1, &first) &&       
          HandleDoubleDstOperand(MI->getOperand(0), o0)) {      
        Value *v1 = Builder.CreateFPExt(o1, Type::getDoubleTy(getGlobalContext()));
        Value *v3 = Builder.CreateStore(v1, o0);
        Value *first = GetFirstInstruction(o1, v1);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }      
      break;
    }
  case Oi::CVT_S_D32:
    {
      DebugOut << "Handling CVT.S.D\n";
      Value *o0, *o1, *first = 0, *v;
      if (HandleDoubleSrcOperand(MI->getOperand(1), o1, &first) &&       
          HandleFloatDstOperand(MI->getOperand(0), o0)) {
        Value *v1 = Builder.CreateFPTrunc(o1, Type::getFloatTy(getGlobalContext()));
        HandleSaveFloat(v1, v);
        Builder.CreateStore(v, o0);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }
      break;
    }
  case Oi::TRUNC_W_D32:
    {
      DebugOut << "Handling TRUNC.W.D\n";
      Value *o0, *o1, *first = 0;
      if (HandleDoubleSrcOperand(MI->getOperand(1), o1, &first) &&       
          HandleDoubleDstOperand(MI->getOperand(0), o0)) {      
        Value *v1 = Builder.CreateFPToSI(o1, Type::getInt32Ty(getGlobalContext()));
        Value *v2 = Builder.CreateZExt(v1, Type::getInt64Ty(getGlobalContext()));
        Value *v3 = Builder.CreateBitCast(v2, Type::getDoubleTy(getGlobalContext()));
        Value *v5 = Builder.CreateStore(v3, o0);
        first = GetFirstInstruction(first, o1, v1);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }      
      break;
    }
  case Oi::TRUNC_W_S:
    {
      DebugOut << "Handling TRUNC.W.S\n";
      Value *o0, *o1, *first = 0;
      if (HandleFloatSrcOperand(MI->getOperand(1), o1, &first) &&       
          HandleFloatDstOperand(MI->getOperand(0), o0)) {      
        Value *V;
        Value *v1 = Builder.CreateFPToSI(o1, Type::getInt32Ty(getGlobalContext()));
        Value *v3 = Builder.CreateBitCast(v1, Type::getFloatTy(getGlobalContext()));
        HandleSaveFloat(v3, V);
        Value *v4 = Builder.CreateStore(V, o0);
        first = GetFirstInstruction(first, o1, v1);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }      
      break;
    }
  case Oi::MFC1:
    {
      DebugOut << "Handling MFC1\n";
      Value *o0, *o1;
      if (HandleDoubleSrcOperand(MI->getOperand(1), o1) &&       
          HandleAluDstOperand(MI->getOperand(0), o0)) {      
        Value *hi, *lo, *V;
        HandleSaveDouble(o1, lo, hi);
        if (ConvToDirective(conv32(MI->getOperand(1).getReg())) % 2)
          V = hi;
        else
          V = lo;
        Value *v = Builder.CreateStore(V, o0);
        Value *first = GetFirstInstruction(o1, v);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }
      break;
    }
  case Oi::MTC1:
    {
      DebugOut << "Handling MTC1\n";
      Value *o0, *o1, *first = 0;
      if (HandleAluSrcOperand(MI->getOperand(1), o1, &first) &&       
          HandleDoubleDstOperand(MI->getOperand(0), o0)) {
        Value *hi, *lo;
        Value *previousVal = Builder.CreateLoad(o0);
        HandleSaveDouble(previousVal, lo, hi);
        if (ConvToDirective(conv32(MI->getOperand(0).getReg())) % 2) {
          hi = o1;
        } else {
          lo = o1;
        }        
        Value *v3 = Builder.CreateZExtOrTrunc(hi, Type::getInt64Ty(getGlobalContext()));
        Value *v4 = Builder.CreateZExtOrTrunc(lo, Type::getInt64Ty(getGlobalContext()));
        Value *v5 = Builder.CreateShl(v3, ConstantInt::get
                                      (Type::getInt64Ty(getGlobalContext()), 32));
        Value *v6 = Builder.CreateOr(v5,v4);
        Value *dblSrc = Builder.CreateBitCast(v6, Type::getDoubleTy(getGlobalContext()));
        Value *v = Builder.CreateStore(dblSrc, o0);
        first = GetFirstInstruction(first, o1, o0,previousVal);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }
      break;
    }
  case Oi::BC1T:
  case Oi::BC1F:
    {
      DebugOut << "Handling BC1F, BC1T\n";
      BasicBlock *True = 0;
      if (HandleBranchTarget(MI->getOperand(0), True)) {
        Value *cmp;
        if (MI->getOpcode() == Oi::BC1T) {
          ReadMap[258] = true;
          cmp = Builder.CreateSExtOrTrunc(Builder.CreateLoad(IREmitter.Regs[258]),
                                    Type::getInt1Ty(getGlobalContext()));
        } else {
          ReadMap[258] = true;
          cmp = Builder.CreateICmpEQ(Builder.CreateLoad(IREmitter.Regs[258]),
                             ConstantInt::get(Type::getInt32Ty
                                              (getGlobalContext()), 0U));
        }
        Value *v = Builder.CreateCondBr(cmp, True, 
                                        IREmitter.CreateBB(IREmitter.CurAddr+
                                                           GetInstructionSize()));
        assert(isa<Instruction>(cmp) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(cmp);
      }
      break;
    }
  case Oi::J:
    {
      DebugOut << "Handling J\n";
      Value *o1;
      BasicBlock *Target = 0;
      if (HandleBranchTarget(MI->getOperand(0), Target, false)) {
        Value *v = Builder.CreateBr(Target);
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(v);
        IREmitter.CreateBB(IREmitter.CurAddr+GetInstructionSize());
      }
      break;
    }
  case Oi::SRA:
    {
      DebugOut << "Handling SRA\n";
      Value *o0, *o1, *o2, *first = 0;
      if (HandleAluSrcOperand(MI->getOperand(1), o1, &first) &&
          HandleAluSrcOperand(MI->getOperand(2), o2, &first) &&
          HandleAluDstOperand(MI->getOperand(0), o0)) {      
        Value *v = Builder.CreateAShr(o1, o2);
        Value *v2 = Builder.CreateStore(v, o0);
        first = GetFirstInstruction(first, o1, o2, v, v2);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }
      break;
    }
  case Oi::SRL:
  case Oi::SRLV:
    {
      DebugOut << "Handling SRL SRLV\n";
      Value *o0, *o1, *o2, *first = 0;
      if (HandleAluSrcOperand(MI->getOperand(1), o1, &first) &&
          HandleAluSrcOperand(MI->getOperand(2), o2, &first) &&
          HandleAluDstOperand(MI->getOperand(0), o0)) {      
        Value *v;
        //XXX: SRLV is decoded with operands inverted!
        if (MI->getOpcode() == Oi::SRLV)
          v = Builder.CreateLShr(o2, o1);
        else
          v = Builder.CreateLShr(o1, o2);
        Value *v2 = Builder.CreateStore(v, o0);
        first = GetFirstInstruction(first, o1, o2, v, v2);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }
      break;
    }
  case Oi::SLL:
  case Oi::SLLV:
    {
      DebugOut << "Handling SLL SLLV";
      Value *o0, *o1, *o2, *first = 0;
      if (MI->getOperand(1).isReg() &&
          ConvToDirective(conv32(MI->getOperand(1).getReg())) == 0 &&
          MI->getOperand(2).isImm() &&
          MI->getOperand(2).getImm() == 0 &&
          MI->getOperand(0).isReg() &&
          ConvToDirective(conv32(MI->getOperand(0).getReg())) == 0) {
        //NOP
        DebugOut << "... NOP!\n";
        break;
      }
      DebugOut << "\n";
      if (HandleAluSrcOperand(MI->getOperand(1), o1, &first) &&
          HandleAluSrcOperand(MI->getOperand(2), o2, &first) &&
          HandleAluDstOperand(MI->getOperand(0), o0)) {      
        Value *v;
        //XXX: SLLV is decoded with operands inverted!
        if (MI->getOpcode() == Oi::SLLV)
          v = Builder.CreateShl(o2, o1);
        else
          v = Builder.CreateShl(o1, o2);
        Value *v2 = Builder.CreateStore(v, o0);
        first = GetFirstInstruction(first, o1, o2, v, v2);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }
      break;
    }
  case Oi::MOVN_I_I:
  case Oi::MOVZ_I_I:
    {
      DebugOut << "Handling MOVN, MOVZ\n";
      Value *o0, *o1, *o2, *first = 0;
      if (HandleAluSrcOperand(MI->getOperand(1), o1, &first) &&
          HandleAluSrcOperand(MI->getOperand(2), o2, &first) &&
          HandleAluDstOperand(MI->getOperand(0), o0)) {        
        Value *zero = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0U);
        Value *cmp;
        if (MI->getOpcode() == Oi::MOVN_I_I) {
          cmp = Builder.CreateICmpNE(o2, zero);
        } else {
          cmp = Builder.CreateICmpEQ(o2, zero);
        }
        Value *loaddst = Builder.CreateLoad(o0);
        Value *select = Builder.CreateSelect(cmp, o1, loaddst, "movz_n");
        Builder.CreateStore(select, o0);
        first = GetFirstInstruction(first, o1, o2, cmp, loaddst);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }
      break;
    }
  case Oi::ORi:
  case Oi::OR:
    {
      DebugOut << "Handling ORi, OR\n";
      Value *o0, *o1, *o2, *first = 0;
      if (HandleAluSrcOperand(MI->getOperand(1), o1, &first) &&
          HandleAluSrcOperand(MI->getOperand(2), o2, &first) &&
          HandleAluDstOperand(MI->getOperand(0), o0)) {      
        Value *v = Builder.CreateOr(o1, o2);
        Value *v2 = Builder.CreateStore(v, o0);
        first = GetFirstInstruction(first, o1, o2, v, v2);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }
      break;
    }
  case Oi::NOR:
    {
      DebugOut << "Handling NORi, NOR\n";
      Value *o0, *o1, *o2, *first = 0;
      if (HandleAluSrcOperand(MI->getOperand(1), o1, &first) &&
          HandleAluSrcOperand(MI->getOperand(2), o2, &first) &&
          HandleAluDstOperand(MI->getOperand(0), o0)) {      
        Value *v = Builder.CreateOr(o1, o2);
        Value *v2 = Builder.CreateNot(v);
        Value *v3 = Builder.CreateStore(v2, o0);
        first = GetFirstInstruction(first, o1, o2, v, v2);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }
      break;
    }
  case Oi::ANDi:
  case Oi::AND:
    {
      DebugOut << "Handling ANDi, AND\n";
      Value *o0, *o1, *o2, *first = 0;
      if (HandleAluSrcOperand(MI->getOperand(1), o1, &first) &&
          HandleAluSrcOperand(MI->getOperand(2), o2, &first) &&
          HandleAluDstOperand(MI->getOperand(0), o0)) {      
        Value *v = Builder.CreateAnd(o1, o2);
        Value *v2 = Builder.CreateStore(v, o0);
        first = GetFirstInstruction(first, o1, o2, v, v2);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }
      break;
    }
  case Oi::XORi:
  case Oi::XOR:
    {
      DebugOut << "Handling XORi, XOR\n";
      Value *o0, *o1, *o2, *first = 0;
      if (HandleAluSrcOperand(MI->getOperand(1), o1, &first) &&
          HandleAluSrcOperand(MI->getOperand(2), o2, &first) &&
          HandleAluDstOperand(MI->getOperand(0), o0)) {      
        Value *v = Builder.CreateXor(o1, o2);
        Value *v2 = Builder.CreateStore(v, o0);
        first = GetFirstInstruction(first, o1, o2, v, v2);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }
      break;
    }
  case Oi::SLTiu:
  case Oi::SLTu:
  case Oi::SLTi:
  case Oi::SLT:
    {
      DebugOut << "Handling SLT\n";
      Value *o0, *o1, *o2, *first = 0;
      if (HandleAluSrcOperand(MI->getOperand(1), o1, &first) &&
          HandleAluSrcOperand(MI->getOperand(2), o2, &first) &&
          HandleAluDstOperand(MI->getOperand(0), o0)) {      

        Function *F = Builder.GetInsertBlock()->getParent();
        BasicBlock *BB1 = BasicBlock::Create(getGlobalContext(), "", F);
        BasicBlock *BB2 = BasicBlock::Create(getGlobalContext(), "", F);
        BasicBlock *FT = IREmitter.CreateBB(IREmitter.CurAddr
                                            + GetInstructionSize());

        Value *cmp = 0;
        if (MI->getOpcode() == Oi::SLTiu ||
            MI->getOpcode() == Oi::SLTu)
          cmp = Builder.CreateICmpULT(o1, o2);
        else
          cmp = Builder.CreateICmpSLT(o1, o2);
        Builder.CreateCondBr(cmp, BB1, BB2);
        
        Value *one = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 1U);
        Value *zero = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0U);

        Builder.SetInsertPoint(BB1);
        Builder.CreateStore(one, o0);
        Builder.CreateBr(FT);
        Builder.SetInsertPoint(BB2);
        Builder.CreateStore(zero, o0);
        Builder.CreateBr(FT);
        Builder.SetInsertPoint(FT);
        IREmitter.CurBlockAddr = IREmitter.CurAddr + GetInstructionSize();

        first = GetFirstInstruction(first, cmp);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }      
      break;
    }
  case Oi::BEQ:
  case Oi::BNE:
  case Oi::BLTZ:
  case Oi::BGTZ:
  case Oi::BLEZ:
    {
      DebugOut << "Handling BEQ, BNE, BLTZ\n";
      Value *o1, *o2, *first = 0;
      BasicBlock *True = 0;
      if (HandleAluSrcOperand(MI->getOperand(0), o1, &first)) {
        Value *cmp;
        if (MI->getOpcode() == Oi::BEQ) {
          HandleAluSrcOperand(MI->getOperand(1), o2);
          HandleBranchTarget(MI->getOperand(2), True);
          cmp = Builder.CreateICmpEQ(o1, o2);
        } else if (MI->getOpcode() == Oi::BNE) {
          HandleAluSrcOperand(MI->getOperand(1), o2);
          HandleBranchTarget(MI->getOperand(2), True);
          cmp = Builder.CreateICmpNE(o1, o2);
        } else if (MI->getOpcode() == Oi::BLTZ) {
          o2 = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0U);
          HandleBranchTarget(MI->getOperand(1), True);
          cmp = Builder.CreateICmpSLT(o1, o2);
        } else if (MI->getOpcode() == Oi::BLEZ) {
          o2 = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0U);
          HandleBranchTarget(MI->getOperand(1), True);
          cmp = Builder.CreateICmpSLE(o1, o2);
        } else { /*  Oi::BGTZ  */
          o2 = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0U);
          HandleBranchTarget(MI->getOperand(1), True);
          cmp = Builder.CreateICmpSGT(o1, o2);
        }
        Value *v = Builder.CreateCondBr(cmp, True, 
                                        IREmitter.CreateBB(IREmitter.CurAddr
                                                      + GetInstructionSize()));
        first = GetFirstInstruction(first, o1, o2, cmp, v);
        assert(isa<Instruction>(first) && "Need to rework map logic");
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      }
      break;
    }
  case Oi::LUi:
  case Oi::LUi64: {
    DebugOut << "Handling LUi\n";
    Value *dst, *src, *first = 0;
    if (HandleAluDstOperand(MI->getOperand(0),dst) &&
        HandleLUiOperand(MI->getOperand(1), src, &first, true)) {
      Value *v = Builder.CreateStore(src, dst);
      if (!isa<Instruction>(first))
        first = v;
      assert(isa<Instruction>(first) && "Need to rework map logic");
      IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
    }
    break;
  }
  case Oi::LW:
  case Oi::LW64: {
    DebugOut << "Handling LW\n";
    Value *dst, *src, *first = 0;
    if (HandleAluDstOperand(MI->getOperand(0),dst) &&
        HandleMemOperand(MI->getOperand(1), MI->getOperand(2), src, &first, true)) {
      Value *v = Builder.CreateStore(src, dst);
      assert(isa<Instruction>(first) && "Need to rework map logic");
      IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
    }
    break;
  }
  case Oi::SPILLLW: {
    DebugOut << "Handling SPILLLW\n";
    Value *dst, *src, *first = 0;
    if (HandleAluDstOperand(MI->getOperand(0),dst) &&
        HandleSpilledOperand(MI->getOperand(1), MI->getOperand(2), src, &first, true)) {
      Value *v = Builder.CreateStore(src, dst);
      if (!isa<Instruction>(first))
        first = v;
      assert(isa<Instruction>(first) && "Need to rework map logic");
      IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
    }
    break;
  }
  case Oi::SPILLSW: {
    DebugOut << "Handling SPILLSW\n";
    Value *dst, *src, *first = 0;
    if (HandleAluSrcOperand(MI->getOperand(0),src) &&
        HandleSpilledOperand(MI->getOperand(1), MI->getOperand(2), dst, &first, false)) {
      Value *v = Builder.CreateStore(src, dst);
      first = GetFirstInstruction(src, first);
      assert(isa<Instruction>(first) && "Need to rework map logic");
      IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
    }
    break;
  }
  case Oi::LH:
  case Oi::LHu: {
    DebugOut << "Handling LH\n";
    Value *dst, *src, *first = 0;
    if (HandleAluDstOperand(MI->getOperand(0),dst) &&
        HandleMemOperand(MI->getOperand(1), MI->getOperand(2), src, &first, true, 16)) {
      Value *ext;
      if (MI->getOpcode() == Oi::LH) 
        ext = Builder.CreateSExt(src, Type::getInt32Ty(getGlobalContext()));
      else
        ext = Builder.CreateZExt(src, Type::getInt32Ty(getGlobalContext()));
      Value *v = Builder.CreateStore(ext, dst);
      assert(isa<Instruction>(first) && "Need to rework map logic");
      IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
    }    
    break;
  }
  case Oi::LB:
  case Oi::LBu: {
    DebugOut << "Handling LB\n";
    Value *dst, *src, *first = 0;
    if (HandleAluDstOperand(MI->getOperand(0),dst) &&
        HandleMemOperand(MI->getOperand(1), MI->getOperand(2), src, &first, true, 8)) {
      Value *ext;
      if (MI->getOpcode() == Oi::LB) 
        ext = Builder.CreateSExt(src, Type::getInt32Ty(getGlobalContext()));
      else
        ext = Builder.CreateZExt(src, Type::getInt32Ty(getGlobalContext()));
      Value *v = Builder.CreateStore(ext, dst);
      assert(isa<Instruction>(first) && "Need to rework map logic");
      IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
    }    
    break;
  }
  case Oi::SW:
  case Oi::SW64: {
    DebugOut << "Handling SW\n";
    Value *dst, *src, *first1 = 0, *first2 = 0, *first = 0;
    if (HandleAluSrcOperand(MI->getOperand(0),src, &first1) &&
        HandleMemOperand(MI->getOperand(1), MI->getOperand(2), dst, &first2, false)) {
      Value *v = Builder.CreateStore(src, dst);
      first = GetFirstInstruction(first1, src, first2, v);
      assert(isa<Instruction>(first) && "Need to rework map logic");
      IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
    }
    break;
  }
  case Oi::SB: {
    DebugOut << "Handling SB\n";
    Value *dst, *src, *first1 = 0, *first2 = 0, *first = 0;
    if (HandleAluSrcOperand(MI->getOperand(0),src, &first1) &&
        HandleMemOperand(MI->getOperand(1), MI->getOperand(2), dst, &first2, false, 8)) {
      Value *tr = Builder.CreateTrunc(src, Type::getInt8Ty(getGlobalContext()));
      Value *v = Builder.CreateStore(tr, dst);
      first = GetFirstInstruction(first1, src, tr, first2);
      assert(isa<Instruction>(first) && "Need to rework map logic");
      IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
    }
    break;
  }
  case Oi::SH: {
    DebugOut << "Handling SH\n";
    Value *dst, *src, *first1 = 0, *first2 = 0, *first = 0;
    if (HandleAluSrcOperand(MI->getOperand(0),src, &first1) &&
        HandleMemOperand(MI->getOperand(1), MI->getOperand(2), dst, &first2, false, 16)) {
      Value *tr = Builder.CreateTrunc(src, Type::getInt16Ty(getGlobalContext()));
      Value *v = Builder.CreateStore(tr, dst);
      first = GetFirstInstruction(first1, src, tr, first2);
      assert(isa<Instruction>(first) && "Need to rework map logic");
      IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
    }
    break;
  }
  case Oi::JALR64:
  case Oi::JALR: {
    assert(OneRegion && "Can't handle indirect calls without -oneregion yet.");
    Value *src, *first = 0;
    if (HandleAluSrcOperand(MI->getOperand(0), src, &first) &&
        IREmitter.HandleIndirectCallOneRegion(src, &first)) {      
      assert(isa<Instruction>(first) && "Need to rework map logic");
      IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);     
    } else {
      llvm_unreachable("Failed to handle JALR.");
    }
    break;
  }
  case Oi::JAL: {
    DebugOut << "Handling JAL\n";
    Value *call, *first = 0;
    if(HandleCallTarget(MI->getOperand(0), call, &first)) {
      assert(isa<Instruction>(first) && "Need to rework map logic");
      IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
    }
    break;
  }
  case Oi::JR64:
  case Oi::JR: {
    DebugOut << "Handling JR\n";
    Value *first = 0;
    // Do not create a checkpoint at the end of the main function. Since
    // the program is terminating, it is not neccessary.
    if (!NoLocals && !OneRegion && IREmitter.CurFunAddr != 0x34)
      IREmitter.HandleFunctionExitPoint(&first);
    if (MI->getOperand(0).getReg() == Oi::RA
        || MI->getOperand(0).getReg() == Oi::RA_64) {
      Value *v = Builder.CreateRetVoid();
      if (!first)
        first = v;
      assert(isa<Instruction>(first) && "Need to rework map logic");      
      IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      IREmitter.FunctionRetMap[IREmitter.CurAddr] = IREmitter.CurFunAddr;
    } else {
      Value *src, *first = 0;
      if (HandleAluSrcOperand(MI->getOperand(0), src, &first)) {
        Value *Target = IREmitter.AccessJumpTable(src, &first);
        IndirectBrInst *v = Builder.CreateIndirectBr(Target,
          IREmitter.IndirectDestinations.size());
        for(int I = 0, E = IREmitter.IndirectDestinations.size(); I != E; ++I) {
          v->addDestination(IREmitter.IndirectDestinations[I]);
        }        
        first = GetFirstInstruction(src, first, v);
        assert(isa<Instruction>(first) && "Need to rework map logic");      
        IREmitter.CreateBB(IREmitter.CurAddr+GetInstructionSize());
        IREmitter.InsMap[IREmitter.CurAddr] = dyn_cast<Instruction>(first);
      } else {
        llvm_unreachable("Failed to handle indirect jump.");
      }
    }
    break;
  }
  case Oi::NOP:
    DebugOut << "Handling NOP\n";
    break;
  default: 
    llvm_unreachable("Unimplemented instruction!");
  }
  return;
}

const char *OiInstTranslate::getRegisterName(unsigned RegNo) {
  return 0;
}

bool OiInstTranslate::printAliasInstr(const MCInst *MI, raw_ostream &OS) {
  switch (MI->getOpcode()) {
  default: return false;
  }
  return true;
}


void OiInstTranslate::printRegName(raw_ostream &OS, unsigned RegNo) const {
  OS << '$' << StringRef(getRegisterName(RegNo)).lower();
}

void OiInstTranslate::printInst(const MCInst *MI, raw_ostream &O,
                                StringRef Annot) {
  switch (MI->getOpcode()) {
  default:
    break;
  case Oi::RDHWR:
  case Oi::RDHWR64:
    O << "\t.set\tpush\n";
    O << "\t.set\toi32r2\n";
  }

  // Try to print any aliases first.
  if (!printAliasInstr(MI, O))
    printInstruction(MI, O);
  printAnnotation(O, Annot);

  switch (MI->getOpcode()) {
  default:
    break;
  case Oi::RDHWR:
  case Oi::RDHWR64:
    O << "\n\t.set\tpop";
  }
}

static void printExpr(const MCExpr *Expr, raw_ostream &OS) {
  int Offset = 0;
  const MCSymbolRefExpr *SRE;

  if (const MCBinaryExpr *BE = dyn_cast<MCBinaryExpr>(Expr)) {
    SRE = dyn_cast<MCSymbolRefExpr>(BE->getLHS());
    const MCConstantExpr *CE = dyn_cast<MCConstantExpr>(BE->getRHS());
    assert(SRE && CE && "Binary expression must be sym+const.");
    Offset = CE->getValue();
  }
  else if (!(SRE = dyn_cast<MCSymbolRefExpr>(Expr)))
    assert(false && "Unexpected MCExpr type.");

  MCSymbolRefExpr::VariantKind Kind = SRE->getKind();

  switch (Kind) {
  default:                                 llvm_unreachable("Invalid kind!");
  case MCSymbolRefExpr::VK_None:           break;
  case MCSymbolRefExpr::VK_Mips_GPREL:     OS << "%gp_rel("; break;
  case MCSymbolRefExpr::VK_Mips_GOT_CALL:  OS << "%call16("; break;
  case MCSymbolRefExpr::VK_Mips_GOT16:     OS << "%got(";    break;
  case MCSymbolRefExpr::VK_Mips_GOT:       OS << "%got(";    break;
  case MCSymbolRefExpr::VK_Mips_ABS_HI:    OS << "%hi(";     break;
  case MCSymbolRefExpr::VK_Mips_ABS_LO:    OS << "%lo(";     break;
  case MCSymbolRefExpr::VK_Mips_TLSGD:     OS << "%tlsgd(";  break;
  case MCSymbolRefExpr::VK_Mips_TLSLDM:    OS << "%tlsldm(";  break;
  case MCSymbolRefExpr::VK_Mips_DTPREL_HI: OS << "%dtprel_hi(";  break;
  case MCSymbolRefExpr::VK_Mips_DTPREL_LO: OS << "%dtprel_lo(";  break;
  case MCSymbolRefExpr::VK_Mips_GOTTPREL:  OS << "%gottprel("; break;
  case MCSymbolRefExpr::VK_Mips_TPREL_HI:  OS << "%tprel_hi("; break;
  case MCSymbolRefExpr::VK_Mips_TPREL_LO:  OS << "%tprel_lo("; break;
  case MCSymbolRefExpr::VK_Mips_GPOFF_HI:  OS << "%hi(%neg(%gp_rel("; break;
  case MCSymbolRefExpr::VK_Mips_GPOFF_LO:  OS << "%lo(%neg(%gp_rel("; break;
  case MCSymbolRefExpr::VK_Mips_GOT_DISP:  OS << "%got_disp("; break;
  case MCSymbolRefExpr::VK_Mips_GOT_PAGE:  OS << "%got_page("; break;
  case MCSymbolRefExpr::VK_Mips_GOT_OFST:  OS << "%got_ofst("; break;
  case MCSymbolRefExpr::VK_Mips_HIGHER:    OS << "%higher("; break;
  case MCSymbolRefExpr::VK_Mips_HIGHEST:   OS << "%highest("; break;
  case MCSymbolRefExpr::VK_Mips_GOT_HI16:  OS << "%got_hi("; break;
  case MCSymbolRefExpr::VK_Mips_GOT_LO16:  OS << "%got_lo("; break;
  case MCSymbolRefExpr::VK_Mips_CALL_HI16: OS << "%call_hi("; break;
  case MCSymbolRefExpr::VK_Mips_CALL_LO16: OS << "%call_lo("; break;
  }

  OS << SRE->getSymbol();

  if (Offset) {
    if (Offset > 0)
      OS << '+';
    OS << Offset;
  }

  if ((Kind == MCSymbolRefExpr::VK_Mips_GPOFF_HI) ||
      (Kind == MCSymbolRefExpr::VK_Mips_GPOFF_LO))
    OS << ")))";
  else if (Kind != MCSymbolRefExpr::VK_None)
    OS << ')';
}

void OiInstTranslate::printCPURegs(const MCInst *MI, unsigned OpNo,
                                   raw_ostream &O) {
  printRegName(O, MI->getOperand(OpNo).getReg());
}

void OiInstTranslate::printOperand(const MCInst *MI, unsigned OpNo,
                                   raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isReg()) {
    printRegName(O, Op.getReg());
    return;
  }

  if (Op.isImm()) {
    O << Op.getImm();
    return;
  }

  assert(Op.isExpr() && "unknown operand kind in printOperand");
  printExpr(Op.getExpr(), O);
}

void OiInstTranslate::printUnsignedImm(const MCInst *MI, int opNum,
                                       raw_ostream &O) {
  const MCOperand &MO = MI->getOperand(opNum);
  if (MO.isImm())
    O << (unsigned short int)MO.getImm();
  else
    printOperand(MI, opNum, O);
}

void OiInstTranslate::
printMemOperand(const MCInst *MI, int opNum, raw_ostream &O) {
  // Load/Store memory operands -- imm($reg)
  // If PIC target the target is loaded as the
  // pattern lw $25,%call16($28)
  printOperand(MI, opNum+1, O);
  O << "(";
  printOperand(MI, opNum, O);
  O << ")";
}

void OiInstTranslate::
printMemOperandEA(const MCInst *MI, int opNum, raw_ostream &O) {
  // when using stack locations for not load/store instructions
  // print the same way as all normal 3 operand instructions.
  printOperand(MI, opNum, O);
  O << ", ";
  printOperand(MI, opNum+1, O);
  return;
}

void OiInstTranslate::
printFCCOperand(const MCInst *MI, int opNum, raw_ostream &O) {
  const MCOperand& MO = MI->getOperand(opNum);
  O << OiFCCToString((Oi::CondCode)MO.getImm());
}
