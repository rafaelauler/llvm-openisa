//===-- MipsInstrFormats.td - Mips Instruction Formats -----*- tablegen -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//  Describe MIPS instructions format
//
//  CPU INSTRUCTION FORMATS
//
//  opcode  - operation code.
//  rs      - src reg.
//  rt      - dst reg (on a 2 regs instr) or src reg (on a 3 reg instr).
//  rd      - dst reg, only used on 3 regs instr.
//  shamt   - only used on shift instructions, contains the shift amount.
//  funct   - combined with opcode field give us an operation code.
//
//===----------------------------------------------------------------------===//

// Format specifies the encoding used by the instruction.  This is part of the
// ad-hoc solution used to emit machine instruction encodings by our machine
// code emitter.
class Format<bits<4> val> {
  bits<4> Value = val;
}

def Pseudo    : Format<0>;
def FrmPL26i  : Format<1>;
def FrmPL26j  : Format<2>;
def FrmPL26c  : Format<3>;
def FrmPL24   : Format<4>;
def FrmPL20   : Format<5>;
def FrmPL18   : Format<6>;
def FrmPL16   : Format<7>;
def FrmPL12   : Format<8>;
def FrmPL6    : Format<9>;
def FrmPL0    : Format<10>;
def FrmOther  : Format<11>; // Instruction w/ a custom format

class MMRel;

def Std2MicroMips : InstrMapping {
  let FilterClass = "MMRel";
  // Instructions with the same BaseOpcode and isNVStore values form a row.
  let RowFields = ["BaseOpcode"];
  // Instructions with the same predicate sense form a column.
  let ColFields = ["Arch"];
  // The key column is the unpredicated instructions.
  let KeyCol = ["se"];
  // Value columns are PredSense=true and PredSense=false
  let ValueCols = [["se"], ["micromips"]];
}

class StdArch {
  string Arch = "se";
}

// Generic Mips Format
class MipsInst<dag outs, dag ins, string asmstr, list<dag> pattern,
               InstrItinClass itin, Format f>: Instruction
{
  field bits<32> Inst;
  Format Form = f;

  let Namespace = "Mips";

  let Size = 4;

  bits<6> Opcode = 0;

  // Top 6 bits are the 'opcode' field
  let Inst{31-26} = Opcode;

  let OutOperandList = outs;
  let InOperandList  = ins;

  let AsmString   = asmstr;
  let Pattern     = pattern;
  let Itinerary   = itin;

  //
  // Attributes specific to Mips instructions...
  //
  bits<4> FormBits = Form.Value;

  // TSFlags layout should be kept in sync with MipsInstrInfo.h.
  let TSFlags{3-0}   = FormBits;

  let DecoderNamespace = "Mips";

  field bits<64> SoftFail = 0;
}

// Mips32/64 Instruction Format
class InstSE<dag outs, dag ins, string asmstr, list<dag> pattern,
             InstrItinClass itin, Format f, string opstr = ""> :
  MipsInst<outs, ins, asmstr, pattern, itin, f>, PredicateControl {
  let EncodingPredicates = [HasStdEnc];
  string BaseOpcode = opstr;
  string Arch;
}

// Mips Pseudo Instructions Format
class MipsPseudo<dag outs, dag ins, list<dag> pattern,
                 InstrItinClass itin = IIPseudo> :
  MipsInst<outs, ins, "", pattern, itin, Pseudo> {
  let isCodeGenOnly = 1;
  let isPseudo = 1;
}

// Mips32/64 Pseudo Instruction Format
class PseudoSE<dag outs, dag ins, list<dag> pattern,
               InstrItinClass itin = IIPseudo> :
  MipsPseudo<outs, ins, pattern, itin>, PredicateControl {
  let EncodingPredicates = [HasStdEnc];
}

// Pseudo-instructions for alternate assembly syntax (never used by codegen).
// These are aliases that require C++ handling to convert to the target
// instruction, while InstAliases can be handled directly by tblgen.
class MipsAsmPseudoInst<dag outs, dag ins, string asmstr>:
  MipsInst<outs, ins, asmstr, [], IIPseudo, Pseudo> {
  let isPseudo = 1;
  let Pattern = [];
}

class PL26j<bits<6> op> : StdArch
{
  bits<26> target;
  bits<32> Inst;

  let Inst{31-26} = op;
  let Inst{25-0} = target;
}

class PL26c<bits<6> op> : StdArch
{
  bits<20> target;
  bits<6>  num;
  bits<32> Inst;

  let Inst{31-26} = op;
  let Inst{25-20} = num;
  let Inst{19-0} = target;
}

class PL26i<bits<6> op, dag outs, dag ins, string asmstr, list<dag> pattern,
        InstrItinClass itin>: InstSE<outs, ins, asmstr, pattern, itin, FrmPL26i>
{
  bits<6>  rt;
  bits<6>  rs;
  bits<14> imm14;

  let Opcode = op;

  let Inst{25-12} = imm14;
  let Inst{11-6} = rs;
  let Inst{5-0} = rt;
}

class PL24<bits<6> op, bits<2> extop1,
         dag outs, dag ins, string asmstr, list<dag> pattern, InstrItinClass itin>:
  InstSE<outs, ins, asmstr, pattern, itin, FrmPL24>
{
  bits<6>  rv;
  bits<6>  rd;
  bits<6>  rs;
  bits<6>  rt;

  let Opcode = op;

  let Inst{25-24} = extop1;
  let Inst{23-18} = rv;
  let Inst{17-12} = rd;
  let Inst{11-6}  = rs;
  let Inst{5-0}   = rt;
}

class PL20<bits<6> op, bits<2> extop1, bits<4> extop2,
         dag outs, dag ins, string asmstr, list<dag> pattern, InstrItinClass itin>:
  InstSE<outs, ins, asmstr, pattern, itin, FrmPL20>
{
  bits<6>  rt;
  bits<14> imm14;

  let Opcode = op;

  let Inst{25-24} = extop1;
  let Inst{23-20} = extop2;
  let Inst{19-6} = imm14;
  let Inst{5-0}   = rt;
}

class PL18<bits<6> op, bits<2> extop1, bits<4> extop2, bits<2> extop3,
         dag outs, dag ins, string asmstr, list<dag> pattern, InstrItinClass itin>:
  InstSE<outs, ins, asmstr, pattern, itin, FrmPL18>
{
  bits<6>  rd;
  bits<6>  rs;
  bits<6>  rt;

  let Opcode = op;

  let Inst{25-24} = extop1;
  let Inst{23-20} = extop2;
  let Inst{19-18} = extop3;
  let Inst{17-12} = rd;
  let Inst{11-6}  = rs;
  let Inst{5-0}   = rt;
}

class PL18i<bits<6> op, bits<2> extop1, bits<4> extop2, bits<2> extop3,
         dag outs, dag ins, string asmstr, list<dag> pattern, InstrItinClass itin>:
  InstSE<outs, ins, asmstr, pattern, itin, FrmPL18>
{
  bits<18>  imm18;

  let Opcode = op;

  let Inst{25-24} = extop1;
  let Inst{23-20} = extop2;
  let Inst{19-18} = extop3;
  let Inst{17-0}  = imm18;
}

class PL12<bits<6> op, bits<2> extop1, bits<4> extop2, bits<2> extop3,
         bits<2> extop4, bits<4> extop5,
         dag outs, dag ins, string asmstr, list<dag> pattern, InstrItinClass itin>:
  InstSE<outs, ins, asmstr, pattern, itin, FrmPL12>
{
  bits<6>  rs;
  bits<6>  rt;

  let Opcode = op;

  let Inst{25-24} = extop1;
  let Inst{23-20} = extop2;
  let Inst{19-18} = extop3;
  let Inst{17-16} = extop4;
  let Inst{15-12} = extop5;
  let Inst{11-6}  = rs;
  let Inst{5-0}   = rt;
}

//===----------------------------------------------------------------------===//
// MFC instruction class in Mips : <|op|mf|rt|rd|0000000|sel|>
//===----------------------------------------------------------------------===//
class MFC3OP_FM<bits<4> mfmt>
{
  bits<6> rs;
  bits<6> rt;

  bits<32> Inst;

  let Inst{31-26} = 0x22;
  let Inst{25-24} = 0;
  let Inst{23-20} = 0;
  let Inst{19-18} = 0;
  let Inst{17-16} = 1;
  let Inst{15-12} = mfmt;
  let Inst{11-6}  = rs;
  let Inst{5-0}   = rt;
}

class SYSCALL_FM : StdArch {
  bits<32> Inst;

  let Inst{31-26} = 0x24;
  let Inst{25-24} = 0;
  let Inst{23-20} = 0;
  let Inst{19-18} = 0;
  let Inst{17-16} = 0;
  let Inst{15-12} = 0;
  let Inst{11-6}  = 0;
  let Inst{5-0}   = 1;
}

class ADD_FM<bits<4> opext2, bits<2> opext3> : StdArch {
  bits<6>  rd;
  bits<6>  rs;
  bits<6>  rt;

  bits<32> Inst;

  let Inst{31-26} = 0x20;
  let Inst{25-24} = 0;
  let Inst{23-20} = opext2;
  let Inst{19-18} = opext3;
  let Inst{17-12} = rd;
  let Inst{11-6}  = rs;
  let Inst{5-0}   = rt;
}

class ADDI_FM<bits<6> op> : StdArch {
  bits<6>  rt;
  bits<6>  rs;
  bits<14> imm14;

  bits<32> Inst;

  let Inst{31-26} = op;
  let Inst{25-12} = imm14;
  let Inst{11-6} = rs;
  let Inst{5-0} = rt;
}

class LW_FM<bits<6> op> : StdArch {
  bits<6> rt;
  bits<20> addr;

  bits<32> Inst;

  let Inst{31-26} = op;
  let Inst{25-6} = addr;
  let Inst{5-0}  = rt;
}

class SLTI_FM<bits<6> op> : StdArch {
  bits<6>  rt;
  bits<6>  rs;
  bits<14> imm14;

  bits<32> Inst;

  let Inst{31-26} = op;
  let Inst{25-12} = imm14;
  let Inst{11-6} = rs;
  let Inst{5-0} = rt;
}

class SRA_FM<bits<4> opext2, bits<2> opext3> : StdArch {
  bits<6>  rd;
  bits<6>  rt;
  bits<6>  shamt;

  bits<32> Inst;

  let Inst{31-26} = 0x20;
  let Inst{25-24} = 0;
  let Inst{23-20} = opext2;
  let Inst{19-18} = opext3;
  let Inst{17-12} = rd;
  let Inst{11-6}  = shamt;
  let Inst{5-0}   = rt;
}

class BEQ_FM<bits<6> op> : StdArch {
  bits<6>  rt;
  bits<6>  rs;
  bits<14> offset;

  bits<32> Inst;

  let Inst{31-26} = op;
  let Inst{25-12} = offset;
  let Inst{11-6} = rs;
  let Inst{5-0} = rt;
}

class BGEZ_FM<bits<6> op, bits<4> funct> : StdArch {
  bits<6>  rs;
  bits<14> offset;

  bits<32> Inst;

  let Inst{31-26} = op;
  let Inst{25-24} = 0;
  let Inst{23-20} = funct;
  let Inst{19-6} = offset;
  let Inst{5-0}   = rs;
}

class SEB_FM<bits<2> extop4, bits<4> extop5> {
  bits<6>  rd;
  bits<6>  rt;

  bits<32> Inst;

  let Inst{31-26} = 0x22;
  let Inst{25-24} = 0;
  let Inst{23-20} = 0;
  let Inst{19-18} = 0;
  let Inst{17-16} = extop4;
  let Inst{15-12} = extop5;
  let Inst{11-6}  = rd;
  let Inst{5-0}   = rt;
}

class CLO_FM<bits<2> extop4, bits<4> extop5> {
  bits<6>  rd;
  bits<6>  rt;

  bits<32> Inst;

  let Inst{31-26} = 0x22;
  let Inst{25-24} = 0;
  let Inst{23-20} = 0;
  let Inst{19-18} = 0;
  let Inst{17-16} = extop4;
  let Inst{15-12} = extop5;
  let Inst{11-6}  = rd;
  let Inst{5-0}   = rt;
}

class JUMPR_FM {
  bits<6>  rt;

  bits<32> Inst;

  let Inst{31-26} = 0x23;
  let Inst{25-24} = 0;
  let Inst{23-20} = 0;
  let Inst{19-18} = 0;
  let Inst{17-16} = 0;
  let Inst{15-12} = 0;
  let Inst{11-6}  = 0;
  let Inst{5-0}   = rt;
}

class JALR_FM {
  bits<6>  num;
  bits<6>  rs;

  bits<32> Inst;

  let Inst{31-26} = 0x22;
  let Inst{25-24} = 0;
  let Inst{23-20} = 0;
  let Inst{19-18} = 0;
  let Inst{17-16} = 0;
  let Inst{15-12} = 0;
  let Inst{11-6}  = num;
  let Inst{5-0}   = rs;
}

class SYNC_FM {
  bits<32> Inst;

  let Inst{31-26} = 0x24;
  let Inst{25-24} = 0;
  let Inst{23-20} = 0;
  let Inst{19-18} = 0;
  let Inst{17-16} = 0;
  let Inst{15-12} = 0;
  let Inst{11-6}  = 0;
  let Inst{5-0}   = 2;
}

class MULT_FM<bits<6> op, bits<2> extop1> : StdArch {
  bits<6>  rv;
  bits<6>  rd;
  bits<6>  rs;
  bits<6>  rt;

  bits<32> Inst;

  let Inst{31-26} = op;
  let Inst{25-24} = extop1;
  let Inst{23-18} = rv;
  let Inst{17-12} = rd;
  let Inst{11-6}  = rs;
  let Inst{5-0}   = rt;
}

class MULOI_FM<bits<6> op, bits<2> extop1> : StdArch {
  bits<6>  rv;
  bits<6>  rd;
  bits<6>  rs;
  bits<6>  rt;

  bits<32> Inst;

  let Inst{31-26} = op;
  let Inst{25-24} = extop1;
  let Inst{23-18} = rv;
  let Inst{17-12} = rd;
  let Inst{11-6}  = rs;
  let Inst{5-0}   = rt;
}

class MULHI_FM<bits<6> op, bits<2> extop1> : StdArch {
  bits<6>  rd;
  bits<6>  rs;
  bits<6>  rt;

  bits<32> Inst;

  let Inst{31-26} = op;
  let Inst{25-24} = extop1;
  let Inst{23-18} = rd;
  let Inst{17-12} = 0;
  let Inst{11-6}  = rs;
  let Inst{5-0}   = rt;
}

class EXT_FM {
  bits<6>  rt;
  bits<6>  rs;
  bits<6>  pos;
  bits<6>  size;

  bits<32> Inst;

  let Inst{31-26} = 0x1d;
  let Inst{25-24} = 3;
  let Inst{23-18} = rt;
  let Inst{17-12} = rs;
  let Inst{11-6}  = pos;
  let Inst{5-0}   = size;
}

class TEQ_FM : StdArch {
  bits<6>  rs;
  bits<6>  rt;

  bits<32> Inst;

  let Inst{31-26} = 0x22;
  let Inst{25-24} = 0;
  let Inst{23-20} = 0;
  let Inst{19-18} = 0;
  let Inst{17-16} = 0;
  let Inst{15-12} = 1;
  let Inst{11-6}  = rs;
  let Inst{5-0}   = rt;
}

class MTMR_FM<bits<6> funct> : StdArch {
  bits<5> rs;

  bits<64> Inst;

  let Inst{63-58} = 0x1c;
  let Inst{25-21} = rs;
  let Inst{20-6}  = 0;
  let Inst{5-0}   = funct;
}

//===----------------------------------------------------------------------===//
//  System calls format <op|code_|funct>
//===----------------------------------------------------------------------===//

class SYS_FM : StdArch
{
  bits<32> Inst;

  let Inst{31-26} = 0x24;
  let Inst{25-24} = 0;
  let Inst{23-20} = 0;
  let Inst{19-18} = 0;
  let Inst{17-16} = 0;
  let Inst{15-12} = 0;
  let Inst{11-6}  = 0;
  let Inst{5-0}   = 1;
}

//===----------------------------------------------------------------------===//
//  Break instruction format <op|code_1|funct>
//===----------------------------------------------------------------------===//

class BRK_FM : StdArch
{
  bits<32> Inst;

  let Inst{31-26} = 0x24;
  let Inst{25-24} = 0;
  let Inst{23-20} = 0;
  let Inst{19-18} = 0;
  let Inst{17-16} = 0;
  let Inst{15-12} = 0;
  let Inst{11-6}  = 0;
  let Inst{5-0}   = 0;
}

//===----------------------------------------------------------------------===//
//  Exception return format <Cop0|1|0|funct>
//===----------------------------------------------------------------------===//

//class ER_FM<bits<6> funct> : StdArch
//{
//  bits<64> Inst;
//  let Inst{63-58} = 0x10;
//  let Inst{25}    = 1;
//  let Inst{24-6}  = 0;
//  let Inst{5-0}   = funct;
//}


//===----------------------------------------------------------------------===//
//  Enable/disable interrupt instruction format <Cop0|MFMC0|rt|12|0|sc|0|0>
//===----------------------------------------------------------------------===//

//class EI_FM<bits<1> sc> : StdArch
//{
//  bits<64> Inst;
//  bits<5> rt;
//  let Inst{63-58} = 0x10;
//  let Inst{25-21} = 0xb;
//  let Inst{20-16} = rt;
//  let Inst{15-11} = 0xc;
//  let Inst{10-6}  = 0;
//  let Inst{5}     = sc;
//  let Inst{4-0}   = 0;
//}

//===----------------------------------------------------------------------===//
//
//  FLOATING POINT INSTRUCTION FORMATS
//
//  opcode  - operation code.
//  fs      - src reg.
//  ft      - dst reg (on a 2 regs instr) or src reg (on a 3 reg instr).
//  fd      - dst reg, only used on 3 regs instr.
//  fmt     - double or single precision.
//  funct   - combined with opcode field give us an operation code.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Format FI instruction class in Mips : <|opcode|base|ft|immediate|>
//===----------------------------------------------------------------------===//

class ADDS_FM<bits<4> extop2, bits<2> extop3> {
  bits<6> fd;
  bits<6> fs;
  bits<6> ft;

  bits<32> Inst;

  let Inst{31-26} = 0x20;
  let Inst{25-24} = 0;
  let Inst{23-20} = extop2;
  let Inst{19-18} = extop3;
  let Inst{17-12} = fd;
  let Inst{11-6}  = fs;
  let Inst{5-0}   = ft;
}

class CMov_I_F_FM<bits<4> extop2, bits<2> extop3> {
  bits<6> fd;
  bits<6> fs;
  bits<6> rt;

  bits<32> Inst;

  let Inst{31-26} = 0x20;
  let Inst{25-24} = 0;
  let Inst{23-20} = extop2;
  let Inst{19-18} = extop3;
  let Inst{17-12} = fd;
  let Inst{11-6}  = fs;
  let Inst{5-0}   = rt;
}

class ABSS_FM<bits<2> extop4, bits<4> extop5> {
  bits<6>  fd;
  bits<6>  fs;

  bits<32> Inst;

  let Inst{31-26} = 0x22;
  let Inst{25-24} = 0;
  let Inst{23-20} = 0;
  let Inst{19-18} = 0;
  let Inst{17-16} = extop4;
  let Inst{15-12} = extop5;
  let Inst{11-6}  = fd;
  let Inst{5-0}   = fs;
}

class CMov_F_I_FM<bits<2> extop4, bits<4> extop5> {
  bits<6>  rd;
  bits<6>  rs;

  bits<32> Inst;

  let Inst{31-26} = 0x22;
  let Inst{25-24} = 0;
  let Inst{23-20} = 0;
  let Inst{19-18} = 0;
  let Inst{17-16} = extop4;
  let Inst{15-12} = extop5;
  let Inst{11-6}  = rd;
  let Inst{5-0}   = rs;
}

class CMov_F_F_FM<bits<2> extop4, bits<4> extop5> {
  bits<6>  fd;
  bits<6>  fs;

  bits<32> Inst;

  let Inst{31-26} = 0x22;
  let Inst{25-24} = 0;
  let Inst{23-20} = 0;
  let Inst{19-18} = 0;
  let Inst{17-16} = extop4;
  let Inst{15-12} = extop5;
  let Inst{11-6}  = fd;
  let Inst{5-0}   = fs;
}

class MFC1_FM<bits<2> extop4, bits<4> extop5> {
  bits<6> rt;
  bits<6> fs;

  bits<32> Inst;

  let Inst{31-26} = 0x22;
  let Inst{25-24} = 0;
  let Inst{23-20} = 0;
  let Inst{19-18} = 0;
  let Inst{17-16} = extop4;
  let Inst{15-12} = extop5;
  let Inst{11-6}  = rt;
  let Inst{5-0}   = fs;
}

class MADDS_FM<bits<2> extop1> {
  bits<6>  fd;
  bits<6>  fr;
  bits<6>  fs;
  bits<6>  ft;

  bits<32> Inst;

  let Inst{31-26} = 0x1e;
  let Inst{25-24} = extop1;
  let Inst{23-18} = fr;
  let Inst{17-12} = fd;
  let Inst{11-6}  = fs;
  let Inst{5-0}   = ft;
}

class LWXC1_FM<bits<4> extop2, bits<2> extop3> {
  bits<6>  fd;
  bits<6>  base;
  bits<6>  index;

  bits<32> Inst;

  let Inst{31-26} = 0x20;
  let Inst{25-24} = 0;
  let Inst{23-20} = extop2;
  let Inst{19-18} = extop3;
  let Inst{17-12} = fd;
  let Inst{11-6}  = index;
  let Inst{5-0}   = base;
}

class BC1F_FM<bits<2> extop4> {
  bits<16> offset;

  bits<32> Inst;

  let Inst{31-26} = 0x21;
  let Inst{25-24} = 0;
  let Inst{23-20} = 0;
  let Inst{19-18} = 0;
  let Inst{17-16} = extop4;
  let Inst{15-0} = offset;
}

class BAL_FM<bits<2> extop3, bits<2> extop4> {
  bits<16> offset;

  bits<32> Inst;

  let Inst{31-26} = 0x21;
  let Inst{25-24} = 0;
  let Inst{23-20} = 0;
  let Inst{19-18} = extop3;
  let Inst{17-16} = extop4;
  let Inst{15-0} = offset;
}

class CEQS_FM<bits<2> op1, bits<4> op2>  {
  bits<6> fs;
  bits<6> ft;

  bits<32> Inst;

  let Inst{31-26} = 0x22; // op: 6 bits
  let Inst{25-24} = 0;    // opext1: 2 bits
  let Inst{23-20} = 0;    // opext2: 4 bits
  let Inst{19-18} = 0;    // opext3: 2 bits
  let Inst{17-16} = op1;  // opext4: 2 bits
  let Inst{15-12} = op2;  // opext5: 4 bits
  let Inst{11-6}  = fs;
  let Inst{5-0}   = ft;
}

class BARRIER_FM<bits<5> op> : StdArch {
  bits<64> Inst;

  let Inst{63-58} = 0; // SPECIAL
  let Inst{25-21} = 0;
  let Inst{20-16} = 0; // rt = 0
  let Inst{15-11} = 0; // rd = 0
  let Inst{10-6} = op; // Operation
  let Inst{5-0} = 0;   // SLL
}

class SDBBP_FM : StdArch {
  bits<20> code_;

  bits<64> Inst;

  let Inst{63-58} = 0b011100; // SPECIAL2
  let Inst{25-6} = code_;
  let Inst{5-0} = 0b111111;   // SDBBP
}

class JR_HB_FM<bits<6> op> : StdArch{
  bits<5> rs;

  bits<64> Inst;

  let Inst{63-58} = 0; // SPECIAL
  let Inst{25-21} = rs;
  let Inst{20-11} = 0;
  let Inst{10} = 1;
  let Inst{9-6} = 0;
  let Inst{5-0} = op;
}

class JALR_HB_FM<bits<6> op> : StdArch {
  bits<5> rd;
  bits<5> rs;

  bits<64> Inst;

  let Inst{63-58} = 0; // SPECIAL
  let Inst{25-21} = rs;
  let Inst{20-16} = 0;
  let Inst{15-11} = rd;
  let Inst{10} = 1;
  let Inst{9-6} = 0;
  let Inst{5-0} = op;
}

class COP0_TLB_FM<bits<6> op> : StdArch {
  bits<64> Inst;

  let Inst{63-58} = 0x10; // COP0
  let Inst{25} = 1;       // CO
  let Inst{24-6} = 0;
  let Inst{5-0} = op;     // Operation
}

class CACHEOP_FM<bits<6> op> : StdArch {
  bits<21> addr;
  bits<5> hint;
  bits<5> base = addr{20-16};
  bits<16> offset = addr{15-0};

  bits<64> Inst;

  let Inst{63-58} = op;
  let Inst{25-21} = base;
  let Inst{20-16} = hint;
  let Inst{15-0}  = offset;
}
