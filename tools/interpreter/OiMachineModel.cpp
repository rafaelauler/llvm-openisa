//===-- OiMachineModel.cpp - Convert Oi MCInst to LLVM IR ------===//
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
#include "OiMachineModel.h"
#include "../lib/Target/Mips/MipsInstrInfo.h"
#include "StringRefMemoryObject.h"
#include "SyscallWrapper.h"
#include "InterpUtils.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Object/ELF.h"
using namespace llvm;

cl::opt<int32_t>
Verbosity("v", cl::desc("Specifies the verbosity level of debug information (up to 3)"
                                "(Default 0 = no debug)"),
          cl::init(0));

void OiMachineModel::StartFunction(StringRef &N) {
}

void OiMachineModel::FinishFunction() {
}

void OiMachineModel::FinishModule() {
}

void OiMachineModel::ConfigureUserLevelStack(int argc, uint8_t **argv) {
  unsigned stackpos = Mem->TOTALSIZE - 1;
  uint8_t *dstptr = &Mem->memory[stackpos];
  std::vector<unsigned> StrPtrs;

  for (int i = 0; i < argc; ++i) {
    uint8_t *srcptr;
    unsigned sz = 0;
    while (*(srcptr = &argv[i][sz++]));
    while (sz--) {
      *(dstptr--) = *(srcptr--);
    }
    StrPtrs.push_back((unsigned)(dstptr + 1 - Mem->memory));
  }
  dstptr -= ((StrPtrs.size() + 1) << 2) + ((uint64_t)(dstptr) % 4);
  unsigned *aux = reinterpret_cast<unsigned*>(dstptr);
  for (std::vector<unsigned>::iterator I = StrPtrs.begin(),
         E = StrPtrs.end(); I != E; ++I) {
    *(aux++) = *I;
  }
  *aux = 0;
  dstptr -= 4;
  aux = reinterpret_cast<unsigned*>(dstptr);
  *(aux--) = dstptr + 4 - Mem->memory;
  *aux = argc;
  Bank[29] = dstptr - 4 - Mem->memory;
}


uint32_t OiMachineModel::HandleAluSrcOperand(const MCOperand &o) {
  if (o.isReg()) {
    return Bank[ConvToDirective(conv32(o.getReg()))];  
  } else if (o.isImm()) {
    return o.getImm();    
  } else if (o.isFPImm()) {
  }
  llvm_unreachable("Invalid Src operand");
}

double OiMachineModel::HandleDoubleSrcOperand(const MCOperand &o) {
  if (o.isReg())
    return DblBank[ConvToDirectiveDbl(conv32(o.getReg()))];
  llvm_unreachable("Invalid Src operand");
}

uint32_t OiMachineModel::HandleFloatSrcOperand(const MCOperand &o) {
  llvm_unreachable("Invalid Src operand");
}

uint32_t OiMachineModel::HandleDoubleDstOperand(const MCOperand &o) {
  if (o.isReg())
    return ConvToDirectiveDbl(conv32(o.getReg()));
  llvm_unreachable("Invalid dst operand");
}

uint32_t OiMachineModel::HandleFloatDstOperand(const MCOperand &o) {
  llvm_unreachable("Invalid dst operand");
}

double OiMachineModel::HandleDoubleLoadOperand(const MCOperand &o, const MCOperand &o2) {
  if (o.isReg() && o2.isImm()) {
    uint32_t myimm = o2.getImm();
    uint32_t reg = ConvToDirective(conv32(o.getReg()));
    return *reinterpret_cast<double*>(&Mem->memory[Bank[reg] + myimm]);
  }
  llvm_unreachable("Invalid Src operand");
}

void OiMachineModel::HandleDoubleSaveOperand(const MCOperand &o, const MCOperand &o2,
                                             double val) {
  if (o.isReg() && o2.isImm()) {
    uint32_t myimm = o2.getImm();
    uint32_t reg = ConvToDirective(conv32(o.getReg()));
    *reinterpret_cast<double*>(&Mem->memory[Bank[reg] + myimm]) = val;
    return;
  }
  llvm_unreachable("Invalid double save operand");
}

uint32_t OiMachineModel::HandleFloatMemOperand(const MCOperand &o, const MCOperand &o2,
                                             bool IsLoad) {
  llvm_unreachable("Invalid Src operand");
}

uint32_t OiMachineModel::HandleSaveDouble(Value *In, Value *&Low, Value *&High) {
  return true;
}

uint32_t OiMachineModel::HandleSaveFloat(Value *In, Value *&V) {
  return true;
}

uint32_t OiMachineModel::HandleMemExpr(const MCExpr &exp,  bool IsLoad) {
  llvm_unreachable("Invalid Load Expr");
}

uint32_t OiMachineModel::HandleLUiOperand(const MCOperand &o) {
  if (o.isImm()) {
    uint32_t val = o.getImm();
    return val << 16;
  }
  llvm_unreachable("Invalid Src operand");
}

uint32_t* OiMachineModel::HandleMemOperand(const MCOperand &o, const MCOperand &o2) {
  if (o.isReg() && o2.isImm()) {
    uint32_t r = ConvToDirective(conv32(o.getReg()));
    uint32_t imm = o2.getImm();
    return reinterpret_cast<uint32_t*>(&Mem->memory[Bank[r]+imm]);
  }
  llvm_unreachable("Invalid Src operand");
}

uint32_t OiMachineModel::HandleSpilledOperand(const MCOperand &o, const MCOperand &o2,
                                            bool IsLoad) {
  return true;
}

uint32_t OiMachineModel::HandleGetSpilledAddress(const MCOperand &o, const MCOperand &o2,
                                                 const MCOperand &dst) {
                                
  return true;
}

uint32_t OiMachineModel::HandleAluDstOperand(const MCOperand &o) {
  if (o.isReg()) {
    unsigned reg = ConvToDirective(conv32(o.getReg()));
    assert (reg != 0 && "Cannot write to register 0");
    return reg;
  }
  llvm_unreachable("Invalid Dst operand");
}

uint32_t OiMachineModel::HandleCallTarget(const MCOperand &o) {
  return false;
}

static bool custom_isnan(double var)
{
  return var != var;
}

bool OiMachineModel::HandleFCmpOperand(const MCOperand &o, double o0, double o1) {
  if (o.isImm()) {
    uint64_t cond = o.getImm();
    switch (cond) {
    case 0: // OI_FCOND_F  false
      return false;
    case 1: // OI_FCOND_UN unordered - true if either nans
      return custom_isnan(o0) || custom_isnan(o1);
    case 2: // OI_FCOND_OEQ equal
      return o0 == o1;
    case 3: // OI_FCOND_UEQ unordered or equal
      return (custom_isnan(o0) || custom_isnan(o1)) || (o0 == o1);
    case 4: // OI_FCOND_OLT
      return o0 < o1;
    case 5: // OI_FCOND_ULT
      return (custom_isnan(o0) || custom_isnan(o1)) || (o0 < o1);
    case 6: // OI_FCOND_OLE
      return o0 <= o1;
    case 7: // OI_FCOND_ULE
      return (custom_isnan(o0) || custom_isnan(o1)) || (o0 <= o1);
    case 8: // OI_FCOND_SF
      // Exception not implemented
      llvm_unreachable("Unimplemented FCmp Operand");
      break;
    case 9: // OI_FCOND_NGLE - compare not greater or less than equal double 
            // (w/ except.)
      llvm_unreachable("Unimplemented FCmp Operand");
      break;
    case 10: // OI_FCOND_SEQ
      llvm_unreachable("Unimplemented FCmp Operand");
      break;
    case 11: // OI_FCOND_NGL
      llvm_unreachable("Unimplemented FCmp Operand");
      break;
    case 12: // OI_FCOND_LT
      return o0 < o1;
    case 13: // OI_FCOND_NGE
      llvm_unreachable("Unimplemented FCmp Operand");
      break;
    case 14: // OI_FCOND_LE
      return o0 <= o1;
    case 15: // OI_FCOND_NGT
      llvm_unreachable("Unimplemented FCmp Operand");
      break;
    }    
  }
  llvm_unreachable("Unrecognized FCmp Operand");
}

uint32_t OiMachineModel::HandleBranchTarget(const MCOperand &o, bool IsRelative,
                                            uint64_t CurPC) {
  if (o.isImm()) {
    if (IsRelative)
      return (CurPC + o.getImm()) & 0xFFFFFFFFULL;
    else
      return o.getImm();
  }
  llvm_unreachable("Unrecognized branch target");
}


uint64_t OiMachineModel::executeInstruction(const MCInst *MI, uint64_t CurPC) {
#ifndef NDEBUG
  raw_ostream &DebugOut = dbgs();
  if (Verbosity > 0) {
    DebugOut << "\033[0;32m";
    IP.printInst(MI, DebugOut, "");
    DebugOut << "\033[0m\n";
  }
#else
  raw_ostream &DebugOut = nulls();
#endif

  switch (MI->getOpcode()) {
  case Mips::ADDiu:
  case Mips::ADDi:
  case Mips::ADDu:
  case Mips::ADD:
    {
      if (Verbosity > 0)
        DebugOut << "\tHandling ADDiu, ADDi, ADDu, ADD\n";
      uint32_t o1 = HandleAluSrcOperand(MI->getOperand(1));
      uint32_t o2 = HandleAluSrcOperand(MI->getOperand(2));
      uint32_t o0 = HandleAluDstOperand(MI->getOperand(0));
#ifndef NDEBUG
      if (Verbosity >= 2) {
      DebugOut << "\t\033[0;36mBank[\033[1;36m" << o0 << "\033[0;36m]\033[0m <= "
               << "\033[1;35m" << o1 << "\033[0m + \033[1;35m" << o2 << "\033[0m\n"
               << "\t\033[0;36mBank[\033[1;36m" << o0 << "\033[0;36m] = "
               << "\033[1;35m" << (o1 + o2) << " \033[0;36m(0x" 
               << format("%04" PRIx32,(o1 + o2)) << "\033[0;36m)\033[0m\n";
      }
#endif
      Bank[o0] = o1 + o2;
      return CurPC + 8;
    }
  case Mips::SUBu:
  case Mips::SUB:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling SUBu, SUB\n";
      uint32_t o1 = HandleAluSrcOperand(MI->getOperand(1));
      uint32_t o2 = HandleAluSrcOperand(MI->getOperand(2));
      uint32_t o0 = HandleAluDstOperand(MI->getOperand(0));
#ifndef NDEBUG
      if (Verbosity >= 2) 
        DebugOut << " \tBank[" << o0 << "] <= " << o1 << " - " << o2 << "\n";
#endif
      Bank[o0] = o1 - o2;
      return CurPC + 8;
    }
  case Mips::MUL:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling MUL\n";
      uint32_t o1 = HandleAluSrcOperand(MI->getOperand(1));
      uint32_t o2 = HandleAluSrcOperand(MI->getOperand(2));
      uint32_t o0 = HandleAluDstOperand(MI->getOperand(0));
      Bank[o0] = o1 * o2;
      return CurPC + 8;
    }
  case Mips::MULTu:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling MULTU\n";
      uint64_t o1 = HandleAluSrcOperand(MI->getOperand(0));
      uint64_t o2 = HandleAluSrcOperand(MI->getOperand(1));
      uint64_t ans = o1 * o2;
      Hi = ans >> 32;
      Lo = ans & 0xFFFFFFFFULL;
      return CurPC + 8;
    }
  case Mips::MULT:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling MULT\n";
      int64_t o1 = HandleAluSrcOperand(MI->getOperand(0));
      int64_t o2 = HandleAluSrcOperand(MI->getOperand(1));
      uint64_t ans = (uint64_t)(o1 * o2);
      Hi = ans >> 32;
      Lo = ans & 0xFFFFFFFFULL;
      return CurPC + 8;
    }
  case Mips::SDIV:
  case Mips::UDIV:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling DIV\n";
      uint32_t o1 = HandleAluSrcOperand(MI->getOperand(0));
      uint32_t o2 = HandleAluSrcOperand(MI->getOperand(1));
      Lo = o1 / o2;
      Hi = o1 % o2;
      return CurPC + 8;
    }
  case Mips::MFHI:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling MFHI\n";
      uint32_t o0 = HandleAluDstOperand(MI->getOperand(0));
      Bank[o0] = Hi;
      return CurPC + 8;
    }
  case Mips::MFLO:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling MFLO\n";
      uint32_t o0 = HandleAluDstOperand(MI->getOperand(0));
      Bank[o0] = Lo;
      return CurPC + 8;
    }
  case Mips::LDC1:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling LDC1\n";
      uint32_t o1 = HandleDoubleDstOperand(MI->getOperand(0));
      double o2 = HandleDoubleLoadOperand(MI->getOperand(1), MI->getOperand(2));
      DblBank[o1] = o2;
      return CurPC + 8;
    }
  case Mips::LWC1:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling LWC1\n";
      llvm_unreachable("LWC1 unimplemented!");
      break;
    }
  case Mips::SDC1:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling SDC1\n";
      double o1 = HandleDoubleSrcOperand(MI->getOperand(0));
      HandleDoubleSaveOperand(MI->getOperand(1), MI->getOperand(2), 
                              o1);
      return CurPC + 8;
    }
  case Mips::SWC1:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling SWC1\n";
      llvm_unreachable("LWC1 unimplemented!");
      break;
    }
  // XXX: Note for FCMP and MOVT: MIPS IV defines several FCC, floating-point
  // codes. We always use the 0th bit (MIPS I mode).
  // TODO: Implement all 8 CC bits.
  case Mips::FCMP_D32:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling FCMP_D32\n";
      double o0 = HandleDoubleSrcOperand(MI->getOperand(0));
      double o1 = HandleDoubleSrcOperand(MI->getOperand(1));
//      if (MI->getOperand(2).getImm() != 2) {
//        llvm_unreachable("Unimplemented FCC");
//      }
      if (HandleFCmpOperand(MI->getOperand(2), o0, o1))
        FCC = 1;
      else
        FCC = 0;
      return CurPC + 8;
    }
  case Mips::FCMP_S32:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling FCMP_S32\n";
      llvm_unreachable("FCMP_S32 unimplemented!");
      break;
    }
    // TODO: implement all fcc bits (get in MI->getOperand(2))
  case Mips::MOVT_I:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling MOVT\n";
      uint32_t o1 = HandleAluSrcOperand(MI->getOperand(1));
      uint32_t o0 = HandleAluDstOperand(MI->getOperand(0));
//      if (MI->getOperand(2).getImm() != 2) {
//        llvm_unreachable("Unimplemented FCC");
//      }
      if (FCC) {
        Bank[o0] = o1;
      } 
      return CurPC + 8;
    }
  case Mips::MOVT_D32:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling MOVT\n";
      double o1 = HandleDoubleSrcOperand(MI->getOperand(1));
      uint32_t o0 = HandleDoubleDstOperand(MI->getOperand(0));
//      if (MI->getOperand(2).getImm() != 2) {
//        llvm_unreachable("Unimplemented FCC");
//      }
      if (FCC) {
        DblBank[o0] = o1;
      } 
      return CurPC + 8;
    }
  case Mips::MOVF_I:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling MOVF\n";
      uint32_t o1 = HandleAluSrcOperand(MI->getOperand(1));
      uint32_t o0 = HandleAluDstOperand(MI->getOperand(0));
//      if (MI->getOperand(2).getImm() != 2) {
//        llvm_unreachable("Unimplemented FCC");
//      }
      if (!FCC) {
        Bank[o0] = o1;
      } 
      return CurPC + 8;
    }
  case Mips::MOVF_D32:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling MOVF\n";
      double o1 = HandleDoubleSrcOperand(MI->getOperand(1));
      uint32_t o0 = HandleDoubleDstOperand(MI->getOperand(0));
      //      if (MI->getOperand(2).getImm() != 2) {
      //        llvm_unreachable("Unimplemented FCC");
      //      }
      if (!FCC) {
        DblBank[o0] = o1;
      } 
      return CurPC + 8;
    }
  case Mips::FSUB_D32:
  case Mips::FADD_D32:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling FADD_D32 FSUB_D32\n";
      double o1 = HandleDoubleSrcOperand(MI->getOperand(1));
      double o2 = HandleDoubleSrcOperand(MI->getOperand(2));
      uint32_t o0 = HandleDoubleDstOperand(MI->getOperand(0));
      if (MI->getOpcode() == Mips::FADD_D32)
        DblBank[o0] = o1 + o2;
      else
        DblBank[o0] = o1 - o2;
      return CurPC + 8;
    }
  case Mips::FSUB_S:
  case Mips::FADD_S:
  case Mips::FMUL_S:
  case Mips::FDIV_S:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling FADD_S FSUB_S FMUL_S FDIV_S\n";
      llvm_unreachable("FSUB_S.. unimplemented!");
      break;
    }
  case Mips::FMOV_D32:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling FMOV\n";
      double o1 = HandleDoubleSrcOperand(MI->getOperand(1));
      uint32_t o0 = HandleDoubleDstOperand(MI->getOperand(0));
      DblBank[o0] = o1;
      return CurPC + 8;
    }
  case Mips::FMUL_D32:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling FMUL\n";
      double o1 = HandleDoubleSrcOperand(MI->getOperand(1));
      double o2 = HandleDoubleSrcOperand(MI->getOperand(2));
      uint32_t o0 = HandleDoubleDstOperand(MI->getOperand(0));
      DblBank[o0] = o1 * o2;
      return CurPC + 8;
    }
  case Mips::FDIV_D32:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling FDIV\n";
      double o1 = HandleDoubleSrcOperand(MI->getOperand(1));
      double o2 = HandleDoubleSrcOperand(MI->getOperand(2));
      uint32_t o0 = HandleDoubleDstOperand(MI->getOperand(0));
      DblBank[o0] = o1 / o2;
      return CurPC + 8;
    }
  case Mips::CVT_D32_W:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling CVT.D.W\n";
      llvm_unreachable("CVT.D.W unimplemented!");
      break;
    }
  case Mips::CVT_S_W:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling CVT.S.W\n";
      llvm_unreachable("CVT.S.W unimplemented!");
      break;
    }
  case Mips::CVT_D32_S:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling CVT.D.S\n";
      llvm_unreachable("CVT.D.S unimplemented!");
      break;
    }
  case Mips::CVT_S_D32:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling CVT.S.D\n";
      llvm_unreachable("CVT.S.D unimplemented!");
      break;
    }
  case Mips::TRUNC_W_D32:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling TRUNC.W.D\n";
      llvm_unreachable("TRUNC.W.D unimplemented!");
      break;
    }
  case Mips::TRUNC_W_S:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling TRUNC.W.S\n";
      llvm_unreachable("TRUNC.W.S unimplemented!");
      break;
    }
  case Mips::MFC1:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling MFC1\n";
      double o1 = HandleDoubleSrcOperand(MI->getOperand(1));
      uint32_t o0 = HandleAluDstOperand(MI->getOperand(0));
      uint64_t tmp = 0;
      memcpy(&tmp,&o1,sizeof(uint64_t));
      if (ConvToDirective(conv32(MI->getOperand(1).getReg())) % 2) {
        Bank[o0] = tmp >> 32;
      } else {
        Bank[o0] = tmp & 0xFFFFFFFFULL;
      }
      return CurPC + 8;
    }
  case Mips::MTC1:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling MTC1\n";
      uint32_t o1 = HandleAluSrcOperand(MI->getOperand(1));
      uint32_t o0 = HandleDoubleDstOperand(MI->getOperand(0));
      uint64_t val = 0;
      memcpy(&val,&DblBank[o0],sizeof(uint64_t));
      if (ConvToDirective(conv32(MI->getOperand(0).getReg())) % 2) {
        val = val & 0xFFFFFFFFULL;
        val |= ((uint64_t)(o1)) << 32;
      } else {
        val = val & 0xFFFFFFFF00000000ULL;
        val |= o1;
      }      
      DblBank[o0] = val;
      return CurPC + 8;
    }
  case Mips::BC1T:
  case Mips::BC1F:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling BC1F, BC1T\n";
      if (MI->getOpcode() == Mips::BC1T && FCC)
        return HandleBranchTarget(MI->getOperand(0), true, CurPC);
      else if (MI->getOpcode() == Mips::BC1F && !FCC)
        return HandleBranchTarget(MI->getOperand(0), true, CurPC);
      return CurPC + 8;
    }
  case Mips::J:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling J\n";
      return HandleBranchTarget(MI->getOperand(0), false, CurPC);
    }
  case Mips::SRA:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling SRA\n";
      int32_t o1 = HandleAluSrcOperand(MI->getOperand(1));
      int32_t o2 = HandleAluSrcOperand(MI->getOperand(2));
      uint32_t o0 = HandleAluDstOperand(MI->getOperand(0));
      Bank[o0] = o1 / (1 << o2);
      return CurPC + 8;
    }
  case Mips::SRL:
  case Mips::SRLV:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling SRL SRLV\n";
      uint32_t o1 = HandleAluSrcOperand(MI->getOperand(1));
      uint32_t o2 = HandleAluSrcOperand(MI->getOperand(2));
      uint32_t o0 = HandleAluDstOperand(MI->getOperand(0));
      //XXX: SRLV is decoded with operands inverted!
      if (MI->getOpcode() == Mips::SRLV)
        Bank[o0] = o2 >> o1;
      else
        Bank[o0] = o1 >> o2;
      return CurPC + 8;
    }
  case Mips::SLL:
  case Mips::SLLV:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling SLL SLLV\n";
      if (MI->getOperand(1).isReg() &&
          ConvToDirective(conv32(MI->getOperand(1).getReg())) == 0 &&
          MI->getOperand(2).isImm() &&
          MI->getOperand(2).getImm() == 0 &&
          MI->getOperand(0).isReg() &&
          ConvToDirective(conv32(MI->getOperand(0).getReg())) == 0) {
        //NOP
        if (Verbosity > 0)
          DebugOut << "... NOP!\n";
        break;
      }
      uint32_t o1 = HandleAluSrcOperand(MI->getOperand(1));
      uint32_t o2 = HandleAluSrcOperand(MI->getOperand(2));
      uint32_t o0 = HandleAluDstOperand(MI->getOperand(0));      
      //XXX: SRLV is decoded with operands inverted!
      if (MI->getOpcode() == Mips::SRLV)
        Bank[o0] = o2 << o1;
      else
        Bank[o0] = o1 << o2;
      return CurPC + 8;
    }
  case Mips::MOVN_I_I:
  case Mips::MOVZ_I_I:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling MOVN, MOVZ\n";
      uint32_t o1 = HandleAluSrcOperand(MI->getOperand(1));
      uint32_t o2 = HandleAluSrcOperand(MI->getOperand(2));
      uint32_t o0 = HandleAluDstOperand(MI->getOperand(0));
      if (MI->getOpcode() == Mips::MOVN_I_I && o2)
        Bank[o0] =  o1;
      else if (MI->getOpcode() == Mips::MOVZ_I_I && !o2)
        Bank[o0] = o1;
      return CurPC + 8;
    }
  case Mips::ORi:
  case Mips::OR:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling ORi, OR\n";
      uint32_t o1 = HandleAluSrcOperand(MI->getOperand(1));
      uint32_t o2 = HandleAluSrcOperand(MI->getOperand(2));
      uint32_t o0 = HandleAluDstOperand(MI->getOperand(0));
#ifndef NDEBUG
      if (Verbosity > 2) {
        DebugOut << "\t\033[0;36mBank[\033[1;36m" << o0 << "\033[0;36m]\033[0m <= "
                 << "\033[1;35m" << o1 << "\033[0m or \033[1;35m" << o2 << "\033[0m\n"
                 << "\t\033[0;36mBank[\033[1;36m" << o0 << "\033[0;36m] = "
                 << "\033[1;35m" << (o1 | o2) << " \033[0;36m(0x" 
                 << format("%04" PRIx32,(o1 | o2)) << "\033[0;36m)\033[0m\n";
      }
#endif
      Bank[o0] = o1 | o2;
      return CurPC + 8;
    }
  case Mips::NOR:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling NORi, NOR\n";
      uint32_t o1 = HandleAluSrcOperand(MI->getOperand(1));
      uint32_t o2 = HandleAluSrcOperand(MI->getOperand(2));
      uint32_t o0 = HandleAluDstOperand(MI->getOperand(0));
      Bank[o0] = ~(o1 | o2);
      return CurPC + 8;
    }
  case Mips::ANDi:
  case Mips::AND:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling ANDi, AND\n";
      uint32_t o1 = HandleAluSrcOperand(MI->getOperand(1));
      uint32_t o2 = HandleAluSrcOperand(MI->getOperand(2));
      uint32_t o0 = HandleAluDstOperand(MI->getOperand(0));
      Bank[o0] = o1 & o2;
      return CurPC + 8;
    }
  case Mips::XORi:
  case Mips::XOR:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling XORi, XOR\n";
      uint32_t o1 = HandleAluSrcOperand(MI->getOperand(1));
      uint32_t o2 = HandleAluSrcOperand(MI->getOperand(2));
      uint32_t o0 = HandleAluDstOperand(MI->getOperand(0));
      Bank[o0] = o1 ^ o2;
      return CurPC + 8;
    }
  case Mips::SLTiu:
  case Mips::SLTu:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling SLT\n";
      uint32_t o1 = HandleAluSrcOperand(MI->getOperand(1));
      uint32_t o2 = HandleAluSrcOperand(MI->getOperand(2));
      uint32_t o0 = HandleAluDstOperand(MI->getOperand(0));      
      Bank[o0] = o1 < o2;
      return CurPC + 8;
    }
  case Mips::SLTi:
  case Mips::SLT:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling SLT\n";
      int32_t o1 = HandleAluSrcOperand(MI->getOperand(1));
      int32_t o2 = HandleAluSrcOperand(MI->getOperand(2));
      uint32_t o0 = HandleAluDstOperand(MI->getOperand(0));      
      Bank[o0] = o1 < o2;
      return CurPC + 8;
    }
  case Mips::BEQ:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling BEQ, BNE, BLTZ\n";
      uint32_t o0 = HandleAluSrcOperand(MI->getOperand(0));
      uint32_t o1 = HandleAluSrcOperand(MI->getOperand(1));
      if (o0 == o1)
        return HandleBranchTarget(MI->getOperand(2), true, CurPC);
      return CurPC + 8;
    }
  case Mips::BNE:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling BEQ, BNE, BLTZ\n";
      uint32_t o0 = HandleAluSrcOperand(MI->getOperand(0));
      uint32_t o1 = HandleAluSrcOperand(MI->getOperand(1));
      if (o0 != o1)
        return HandleBranchTarget(MI->getOperand(2), true, CurPC);
      return CurPC + 8;
    }
  case Mips::BLTZ:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling BEQ, BNE, BLTZ\n";
      int32_t o0 = HandleAluSrcOperand(MI->getOperand(0));
      if (o0 < 0)
        return HandleBranchTarget(MI->getOperand(1), true, CurPC);
      return CurPC + 8;
    }
  case Mips::BGTZ:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling BEQ, BNE, BLTZ\n";
      int32_t o0 = HandleAluSrcOperand(MI->getOperand(0));
      if (o0 > 0)
        return HandleBranchTarget(MI->getOperand(1), true, CurPC);
      return CurPC + 8;
    }
  case Mips::BGEZ:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling BGEZ\n";
      int32_t o0 = HandleAluSrcOperand(MI->getOperand(0));
      if (o0 >= 0)
        return HandleBranchTarget(MI->getOperand(1), true, CurPC);
      return CurPC + 8;
    }
  case Mips::BLEZ:
    {
      if (Verbosity > 0)
        DebugOut << " \tHandling BEQ, BNE, BLTZ\n";
      int32_t o0 = HandleAluSrcOperand(MI->getOperand(0));
      if (o0 <= 0)
        return HandleBranchTarget(MI->getOperand(1), true, CurPC);
      return CurPC + 8;
    }
  case Mips::LUi:
  case Mips::LUi64: {
    if (Verbosity > 0)
      DebugOut << " \tHandling LUi\n";
    HandleAluDstOperand(MI->getOperand(0));
    HandleLUiOperand(MI->getOperand(1));
#ifndef NDEBUG
    if (Verbosity > 0)
      DebugOut << "\t\033[0;31mWarning, LUI ignored\033[0m\n";
#endif
    //Bank[o0] = o1;
    return CurPC + 8;
  }
  case Mips::LW:
  case Mips::LW64: {
    if (Verbosity > 0)
      DebugOut << " \tHandling LW\n";
    uint32_t o0 = HandleAluDstOperand(MI->getOperand(0));
    uint32_t* o1 = HandleMemOperand(MI->getOperand(1), MI->getOperand(2));
#ifndef NDEBUG
    if (Verbosity >= 3) {
      DebugOut << "\t\033[0;36mBank[\033[1;36m" << o0 << "\033[0;36m]\033[0m <= "
               << "\033[0;31mMemory[\033[1;31m" 
               << format("%04" PRIx32, ((uint8_t*)o1 - Mem->memory))
               << "\033[0;31m]\033[0m\n\t\t <= "
               << "\033[1;35m" << *o1 << " \033[0;36m(0x" 
               << format("%04" PRIx32,(*o1)) << "\033[0;36m)\033[0m\n";
    }
#endif
    Bank[o0] = *o1;
    return CurPC + 8;
  }
//   case Mips::SPILLLW: {
//     if (Verbosity > 0)
//       DebugOut << " \tHandling SPILLLW\n";
//     uint32_t o0 = HandleAluDstOperand(MI->getOperand(0));
//     uint32_t* o1 = HandleMemOperand(MI->getOperand(1), MI->getOperand(2));
// #ifndef NDEBUG
//     if (Verbosity >= 3) {
//       DebugOut << "\t\033[0;36mBank[\033[1;36m" << o0 << "\033[0;36m]\033[0m <= "
//                << "\033[0;31mMemory[\033[1;31m" 
//                << format("%04" PRIx32,((char*)o1 - Mem->memory)) 
//                << "\033[0;31m]\033[0m\n\t\t <= "
//                << "\033[1;35m" << *o1 << " \033[0;36m(0x" 
//                << format("%04" PRIx32,(*o1)) << "\033[0;36m)\033[0m\n";
//     }
// #endif
//     Bank[o0] = *o1;
//     return CurPC + 8;
//   }
//   case Mips::SPILLSW: {
//     if (Verbosity > 0)
//       DebugOut << " \tHandling SPILLSW\n";
//     uint32_t o0 = HandleAluSrcOperand(MI->getOperand(0));
//     uint32_t* o1 = HandleMemOperand(MI->getOperand(1), MI->getOperand(2));
// #ifndef NDEBUG
//     if (Verbosity >= 3) {
//       DebugOut << "\t\033[0;31mMemory[\033[1;31m" 
//                << format("%04" PRIx32,((char*)o1 - Mem->memory)) << "\033[0;31m]\033[0m <= "
//                << "\033[1;35m" << o0 << " \033[0;36m(0x" 
//                << format("%04" PRIx32,(o0)) << "\033[0;36m)\033[0m\n";
//     }
// #endif
//     *o1 = o0;
//     return CurPC + 8;
//   }
  case Mips::LWL: {
    if (Verbosity > 0)
      DebugOut << " \tHandling LWL\n";
    uint32_t o0 = HandleAluDstOperand(MI->getOperand(0));
    uint8_t* o1_tmp = reinterpret_cast<uint8_t*>
      (HandleMemOperand(MI->getOperand(1), MI->getOperand(2)));
    --o1_tmp;
    uint16_t* o1 = reinterpret_cast<uint16_t*>(o1_tmp);
    uint32_t tmp = Bank[o0];
    Bank[o0] = (((uint32_t)(*o1) << 16) | tmp) & 0xFFFF;
    return CurPC + 8;
  }
  case Mips::LWR: {
    if (Verbosity > 0)
      DebugOut << " \tHandling LWR\n";
    uint32_t o0 = HandleAluDstOperand(MI->getOperand(0));
    uint16_t* o1 = reinterpret_cast<uint16_t*>
      (HandleMemOperand(MI->getOperand(1), MI->getOperand(2)));
    uint32_t tmp = Bank[o0];
    Bank[o0] = (((uint32_t)(*o1) &0xFFFF) | tmp) & 0xFFFF0000;
    return CurPC + 8;
  }
  case Mips::LHu: {
    if (Verbosity > 0)
      DebugOut << " \tHandling LHu\n";
    uint32_t o0 = HandleAluDstOperand(MI->getOperand(0));
    uint16_t* o1 = reinterpret_cast<uint16_t*>
      (HandleMemOperand(MI->getOperand(1), MI->getOperand(2)));
    Bank[o0] = *o1;
    return CurPC + 8;
  }
  case Mips::LH: {   
    if (Verbosity > 0)
      DebugOut << " \tHandling LH\n";
    uint32_t o0 = HandleAluDstOperand(MI->getOperand(0));
    int16_t* o1 = reinterpret_cast<int16_t*>
      (HandleMemOperand(MI->getOperand(1), MI->getOperand(2)));
    int32_t tmp = *o1;
    Bank[o0] = tmp;
    return CurPC + 8;
  }
  case Mips::LBu: {
    if (Verbosity > 0)
      DebugOut << " \tHandling LBu\n";
    uint32_t o0 = HandleAluDstOperand(MI->getOperand(0));
    uint8_t* o1 = reinterpret_cast<uint8_t*>
      (HandleMemOperand(MI->getOperand(1), MI->getOperand(2)));
    Bank[o0] = *o1;
    return CurPC + 8;
  }
  case Mips::LB: {   
    if (Verbosity > 0)
      DebugOut << " \tHandling LB\n";
    uint32_t o0 = HandleAluDstOperand(MI->getOperand(0));
    int8_t* o1 = reinterpret_cast<int8_t*>
      (HandleMemOperand(MI->getOperand(1), MI->getOperand(2)));
    int32_t tmp = *o1;
    Bank[o0] = tmp;
    return CurPC + 8;
  }
  case Mips::SWL: {
    if (Verbosity > 0)
      DebugOut << " \tHandling SWL\n";
    uint32_t o0 = HandleAluSrcOperand(MI->getOperand(0));
    uint8_t* o1_tmp = reinterpret_cast<uint8_t*>
      (HandleMemOperand(MI->getOperand(1), MI->getOperand(2)));
    --o1_tmp;
    uint16_t* o1 = reinterpret_cast<uint16_t*>(o1_tmp);
    *o1 = (uint16_t)(o0 >> 16);
    return CurPC + 8;
  }
  case Mips::SWR: {
    if (Verbosity > 0)
      DebugOut << " \tHandling SWR\n";
    uint32_t o0 = HandleAluSrcOperand(MI->getOperand(0));
    uint16_t* o1 = reinterpret_cast<uint16_t*>
      (HandleMemOperand(MI->getOperand(1), MI->getOperand(2)));
    *o1 = (uint16_t)(o0 & 0xFFFF);
    return CurPC + 8;
  }
  case Mips::SW:
  case Mips::SW64: {
    if (Verbosity > 0)
      DebugOut << " \tHandling SW\n";
    uint32_t o0 = HandleAluSrcOperand(MI->getOperand(0));
    uint32_t* o1 = HandleMemOperand(MI->getOperand(1), MI->getOperand(2));
#ifndef NDEBUG
    if (Verbosity >= 3) {
      DebugOut << "\t\033[0;31mMemory[\033[1;31m" 
               << format("%04" PRIx32, ((uint8_t *)o1 - Mem->memory)) << "\033[0;31m]\033[0m <= "
               << "\033[1;35m" << o0 << " \033[0;36m(0x" 
               << format("%04" PRIx32,(o0)) << "\033[0;36m)\033[0m\n";
    }
#endif
    *o1 = o0;
    return CurPC + 8;
  }
  case Mips::SB: {
    if (Verbosity > 0)
      DebugOut << " \tHandling SB\n";
    uint8_t o0 = HandleAluSrcOperand(MI->getOperand(0));
    uint8_t* o1 = reinterpret_cast<uint8_t*>
      (HandleMemOperand(MI->getOperand(1), MI->getOperand(2)));
    if (Verbosity >= 3) {
      DebugOut << "\t\033[0;31mMemory[\033[1;31m" 
               << format("%04" PRIx32, ((uint8_t *)o1 - Mem->memory)) << "\033[0;31m]\033[0m <= "
               << "\033[1;35m" << (uint32_t)(o0) << " \033[0;36m(0x" 
               << format("%02" PRIx8,(o0)) << "\033[0;36m)\033[0m\n";
    }
    *o1 = o0;
    return CurPC + 8;
  }
  case Mips::SH: {
    if (Verbosity > 0)
      DebugOut << " \tHandling SH\n";
    uint16_t o0 = HandleAluSrcOperand(MI->getOperand(0));
    uint16_t* o1 = reinterpret_cast<uint16_t*>
      (HandleMemOperand(MI->getOperand(1), MI->getOperand(2)));
    *o1 = o0;
    return CurPC + 8;
  }
  case Mips::JALR64:
  case Mips::JALR: {
    if (Verbosity > 0)
      DebugOut << " \tHandling JALR\n";
    Bank[31] = CurPC + 8;
    return HandleAluSrcOperand(MI->getOperand(1)); // Not operand 0!
  }
  case Mips::JAL: {
    if (Verbosity > 0)
      DebugOut << " \tHandling JAL\n";
    Bank[31] = CurPC + 8;
    return HandleBranchTarget(MI->getOperand(0), false, CurPC);
  }
  case Mips::JR64:
  case Mips::JR: {
    if (Verbosity > 0)
      DebugOut << " \tHandling JR\n";
    return HandleAluSrcOperand(MI->getOperand(0));
  }
  case Mips::SYSCALL: {
    ProcessSyscall(this);
    return CurPC + 8;
  }
  case Mips::NOP:
    if (Verbosity > 0)
      DebugOut << " \tHandling NOP\n";
    break;
  default: 
    llvm_unreachable("Unimplemented instruction!");
  }
  return CurPC + 8;
}

