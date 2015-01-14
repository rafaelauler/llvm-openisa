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
def FrmR      : Format<1>;
def FrmI      : Format<2>;
def FrmJ      : Format<3>;
def FrmFR     : Format<4>;
def FrmFI     : Format<5>;
def FrmOther  : Format<6>; // Instruction w/ a custom format

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
  field bits<64> Inst;
  Format Form = f;

  let Namespace = "Mips";

  let Size = 8;

  bits<6> Opcode = 0;

  // Top 6 bits are the 'opcode' field
  let Inst{63-58} = Opcode;

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

//class MipsInstNOP <dag outs, dag ins, string asmstr, list<dag> pattern>:
//Instruction
//{
//  let Namespace = "Mips";
//  let DecoderNamespace = "Mips";
//
//
//  let OutOperandList = outs;
//  let InOperandList  = ins;
//
//  let AsmString   = asmstr;
//  let Pattern     = pattern;
//  let Itinerary   = NoItinerary;
//
//  field bits<8> Inst;
//  bits<8> Opcode = 0;
//
//  // Top 5 bits are the 'opcode' field
//  let Inst{7-0} = Opcode;
//  
//  let Opcode = 0;
//
//  let Size=1;
//  field bits<8> SoftFail = 0;
//}

//class MipsInstWide<dag outs, dag ins, string asmstr, list<dag> pattern,
//               InstrItinClass itin, Format f>: Instruction
//{
//  field bits<56> Inst;
//  Format Form = f;
//
//  let Namespace = "Mips";
//
//  let Size = 7;
//
//  bits<6> Opcode = 0;
//
//  // Top 6 bits are the 'opcode' field
//  let Inst{55-50} = Opcode;
//
//  let OutOperandList = outs;
//  let InOperandList  = ins;
//
//  let AsmString   = asmstr;
//  let Pattern     = pattern;
//  let Itinerary   = itin;
//
//  //
//  // Attributes specific to Mips instructions...
//  //
//  bits<4> FormBits = Form.Value;
//
//  // TSFlags layout should be kept in sync with MipsInstrInfo.h.
//  let TSFlags{3-0}   = FormBits;
//
//  let DecoderNamespace = "Mips";
//
//  field bits<32> SoftFail = 0;
//}
//
//// Mips32/64 Instruction Format
//class InstSEWide<dag outs, dag ins, string asmstr, list<dag> pattern,
//             InstrItinClass itin, Format f, string opstr = ""> :
//  MipsInstWide<outs, ins, asmstr, pattern, itin, f> {
//  let Predicates = [HasStdEnc];
//  string BaseOpcode = opstr;
//  string Arch;
//}


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
//===----------------------------------------------------------------------===//
// Format R instruction class in Mips : <|opcode|rs|rt|rd|shamt|funct|>
//===----------------------------------------------------------------------===//

class FR<bits<6> op, bits<8> _funct, dag outs, dag ins, string asmstr,
         list<dag> pattern, InstrItinClass itin>:
  InstSE<outs, ins, asmstr, pattern, itin, FrmR>
{
  bits<7>  rd;
  bits<7>  rs;
  bits<7>  rt;
  bits<5>  shamt;
  bits<8>  funct;

  let Opcode = op;
  let funct  = _funct;

  let Inst{33-27} = rs;
  let Inst{26-20} = rt;
  let Inst{19-13} = rd;
  let Inst{12-8}  = shamt;
  let Inst{7-0}   = funct;
}

//===----------------------------------------------------------------------===//
// Format I instruction class in Mips : <|opcode|rs|rt|immediate|>
//===----------------------------------------------------------------------===//

class FI<bits<6> op, dag outs, dag ins, string asmstr, list<dag> pattern,
         InstrItinClass itin>: InstSE<outs, ins, asmstr, pattern, itin, FrmI>
{
  bits<7>  rt;
  bits<7>  rs;
  bits<4> funct2;
  bits<32> imm32;

  let Opcode = op;

  let Inst{57-51} = rs;
  let Inst{50-44} = rt;
  let Inst{31-0}  = imm32;
}

//class FIWide<bits<6> op, dag outs, dag ins, string asmstr, list<dag> pattern,
//         InstrItinClass itin>: InstSEWide<outs, ins, asmstr, pattern, itin, FrmI>
//{
//  bits<7>  rt;
//  bits<7>  rs;
//  bits<4>  funct2;
//  bits<32> imm32;
//
//  let Opcode = op;
//
//  let Inst{49-43} = rs;
//  let Inst{42-36} = rt;
//  let Inst{35-32} = funct2;
//  let Inst{31-0}  = imm32;
//}

class BranchBase<bits<6> op, dag outs, dag ins, string asmstr,
                  list<dag> pattern, InstrItinClass itin>:
  InstSE<outs, ins, asmstr, pattern, itin, FrmI>
{
  bits<7>  rs;
  bits<7>  rt;
  bits<16> imm16;

  let Opcode = op;

  let Inst{33-27} = rs;
  let Inst{26-20} = rt;
  let Inst{15-0}  = imm16;
}

//===----------------------------------------------------------------------===//
// Format J instruction class in Mips : <|opcode|address|>
//===----------------------------------------------------------------------===//

class FJ<bits<6> op> : StdArch
{
  bits<34> target;

  bits<64> Inst;

  let Inst{63-58} = op;
  let Inst{33-0}  = target;
}

//===----------------------------------------------------------------------===//
// MFC instruction class in Mips : <|op|mf|rt|rd|0000000|sel|>
//===----------------------------------------------------------------------===//
class MFC3OP_FM<bits<6> op, bits<5> mfmt>
{
  bits<7> rt;
  bits<7> rd;
  bits<3> sel;

  bits<64> Inst;

  let Inst{63-58} = op;
  let Inst{33-32} = 0;
  let Inst{31-27} = mfmt;
  let Inst{26-20} = rt;
  let Inst{19-13} = rd;
  let Inst{12-3}  = 0;
  let Inst{2-0}   = sel;
}

class SYSCALL_FM : StdArch {
  bits<64> Inst;

  let Inst{63-58} = 0;
  let Inst{33-27} = 0;
  let Inst{26-20} = 0;
  let Inst{19-13} = 0;
  let Inst{12-8}  = 0;
  let Inst{7-0}   = 0xC;
}

class ADD_FM<bits<6> op, bits<8> funct> : StdArch {
  bits<7> rd;
  bits<7> rs;
  bits<7> rt;

  bits<64> Inst;

  let Inst{63-58} = op;
  let Inst{33-27} = rs;
  let Inst{26-20} = rt;
  let Inst{19-13} = rd;
  let Inst{12-8}  = 0;
  let Inst{7-0}   = funct;
}

class ADDI_FM<bits<6> op> : StdArch {
  bits<7>  rs;
  bits<7>  rt;
  bits<32> imm32;

  bits<64> Inst;

  let Inst{63-58} = op;
  let Inst{57-51} = rs;
  let Inst{50-44} = rt;
  let Inst{31-0}  = imm32;
}

class SRA_FM<bits<8> funct, bit rotate> : StdArch {
  bits<7> rd;
  bits<7> rt;
  bits<5> shamt;

  bits<64> Inst;

  let Inst{63-58} = 0;
  let Inst{33-28} = 0;
  let Inst{27}    = rotate;
  let Inst{26-20} = rt;
  let Inst{19-13} = rd;
  let Inst{12-8}  = shamt;
  let Inst{7-0}   = funct;
}

class SRLV_FM<bits<8> funct, bit rotate> : StdArch {
  bits<7> rd;
  bits<7> rt;
  bits<7> rs;

  bits<64> Inst;

  let Inst{63-58} = 0;
  let Inst{33-27} = rs;
  let Inst{26-20} = rt;
  let Inst{19-13} = rd;
  let Inst{12-9}  = 0;
  let Inst{8}     = rotate;
  let Inst{7-0}   = funct;
}

class BEQ_FM<bits<6> op> : StdArch {
  bits<7>  rs;
  bits<7>  rt;
  bits<16> offset;

  bits<64> Inst;

  let Inst{63-58} = op;
  let Inst{33-27} = rs;
  let Inst{26-20} = rt;
  let Inst{15-0}  = offset;
}

class BGEZ_FM<bits<6> op, bits<5> funct> : StdArch {
  bits<7>  rs;
  bits<16> offset;

  bits<64> Inst;

  let Inst{63-58} = op;
  let Inst{33-27} = rs;
  let Inst{20-16} = funct;
  let Inst{15-0}  = offset;
}

class SLTI_FM<bits<6> op> : StdArch {
  bits<7> rt;
  bits<7> rs;
  bits<32> imm32;

  bits<64> Inst;

  let Inst{63-58} = op;
  let Inst{57-51} = rs;
  let Inst{50-44} = rt;
  let Inst{31-0}  = imm32;
}

class MFLO_FM<bits<8> funct> {
  bits<7> rd;

  bits<64> Inst;

  let Inst{63-58} = 0;
  let Inst{33-20} = 0;
  let Inst{19-13} = rd;
  let Inst{12-8}  = 0;
  let Inst{7-0}   = funct;
}

class MTLO_FM<bits<8> funct> {
  bits<7> rs;

  bits<64> Inst;

  let Inst{63-58} = 0;
  let Inst{33-27} = rs;
  let Inst{26-8}  = 0;
  let Inst{7-0}   = funct;
}

class SEB_FM<bits<5> funct, bits<8> funct2> {
  bits<7> rd;
  bits<7> rt;

  bits<64> Inst;

  let Inst{63-58} = 0x1f;
  let Inst{33-27} = 0;
  let Inst{26-20} = rt;
  let Inst{19-13} = rd;
  let Inst{12-8}  = funct;
  let Inst{7-0}   = funct2;
}

class CLO_FM<bits<8> funct> {
  bits<7> rd;
  bits<7> rs;
  bits<7> rt;

  bits<64> Inst;

  let Inst{63-58} = 0x1c;
  let Inst{33-27} = rs;
  let Inst{26-20} = rt;
  let Inst{19-13} = rd;
  let Inst{12-8}  = 0;
  let Inst{7-0}   = funct;
  let rt = rd;
}

class LUI_FM {
  bits<7> rt;
  bits<16> imm16;

  bits<64> Inst;

  let Inst{63-58} = 0xf;
  let Inst{33-27} = 0;
  let Inst{26-20} = rt;
  let Inst{15-0}  = imm16;
}

class JALR_FM {
  bits<7> rd;
  bits<7> rs;

  bits<64> Inst;

  let Inst{63-58} = 0;
  let Inst{33-27} = rs;
  let Inst{26-20} = 0;
  let Inst{19-13} = rd;
  let Inst{12-8}  = 0;
  let Inst{7-0}   = 9;
}

class BGEZAL_FM<bits<8> funct> {
  bits<7>  rs;
  bits<16> offset;

  bits<64> Inst;

  let Inst{63-58} = 1;
  let Inst{33-27} = rs;
  let Inst{26-19} = funct;
  let Inst{15-0}  = offset;
}

class SYNC_FM {
  bits<5> stype;

  bits<64> Inst;

  let Inst{63-58} = 0;
  let Inst{12-8}  = stype;
  let Inst{7-0}   = 0xf;
}

class SYNCI_FM : StdArch {
  // Produced by the mem_simm16 address as reg << 16 | imm (see getMemEncoding).
  bits<23> addr;
  bits<7> rs = addr{22-16};
  bits<16> offset = addr{15-0};

  bits<64> Inst;

  let Inst{63-58} = 0b000001;
  let Inst{33-27} = rs;
  let Inst{20-16} = 0b11111;
  let Inst{15-0}  = offset;
}

class MULT_FM<bits<6> op, bits<8> funct> : StdArch {
  bits<7>  rs;
  bits<7>  rt;

  bits<64> Inst;

  let Inst{63-58} = op;
  let Inst{33-27} = rs;
  let Inst{26-20} = rt;
  let Inst{19-8}  = 0;
  let Inst{7-0}   = funct;
}

class EXT_FM<bits<8> funct> {
  bits<7> rt;
  bits<7> rs;
  bits<5> pos;
  bits<7> size;

  bits<64> Inst;

  let Inst{63-58} = 0x1f;
  let Inst{33-27} = rs;
  let Inst{26-20} = rt;
  let Inst{19-13} = size;
  let Inst{12-8}  = pos;
  let Inst{7-0}   = funct;
}

class RDHWR_FM : StdArch {
  bits<7> rt;
  bits<7> rd;

  bits<64> Inst;

  let Inst{63-58} = 0x1f;
  let Inst{33-27} = 0;
  let Inst{26-20} = rt;
  let Inst{19-13} = rd;
  let Inst{12-8}  = 0;
  let Inst{7-0}   = 0x3b;
}

class TEQ_FM<bits<6> funct> : StdArch {
  bits<7> rs;
  bits<7> rt;
  bits<10> code_;

  bits<64> Inst;

  let Inst{63-58} = 0;
  let Inst{57-51} = rs;
  let Inst{50-44} = rt;
  let Inst{15-6}  = code_;
  let Inst{5-0}   = funct;
}

class TEQI_FM<bits<5> funct> : StdArch {
  bits<7> rs;
  bits<32> imm32;

  bits<64> Inst;

  let Inst{63-58} = 1;
  let Inst{57-51} = rs;
  let Inst{48-44} = funct;
  let Inst{31-0}  = imm32;
}

class WAIT_FM : StdArch {
  bits<64> Inst;

  let Inst{63-58} = 0x10;
  let Inst{25}    = 1;
  let Inst{24-6}  = 0;
  let Inst{5-0}   = 0x20;
}

class EXTS_FM<bits<6> funct> : StdArch {
  bits<5> rt;
  bits<5> rs;
  bits<5> pos;
  bits<5> lenm1;

  bits<64> Inst;

  let Inst{63-58} = 0x1c;
  let Inst{25-21} = rs;
  let Inst{20-16} = rt;
  let Inst{15-11} = lenm1;
  let Inst{10-6}  = pos;
  let Inst{5-0}   = funct;
}

class MTMR_FM<bits<6> funct> : StdArch {
  bits<5> rs;

  bits<64> Inst;

  let Inst{63-58} = 0x1c;
  let Inst{25-21} = rs;
  let Inst{20-6}  = 0;
  let Inst{5-0}   = funct;
}

class POP_FM<bits<6> funct> : StdArch {
  bits<5> rd;
  bits<5> rs;

  bits<64> Inst;

  let Inst{63-58} = 0x1c;
  let Inst{25-21} = rs;
  let Inst{20-16} = 0;
  let Inst{15-11} = rd;
  let Inst{10-6}  = 0;
  let Inst{5-0}   = funct;
}

class SEQ_FM<bits<6> funct> : StdArch {
  bits<5> rd;
  bits<5> rs;
  bits<5> rt;

  bits<64> Inst;

  let Inst{63-58} = 0x1c;
  let Inst{25-21} = rs;
  let Inst{20-16} = rt;
  let Inst{15-11} = rd;
  let Inst{10-6}  = 0;
  let Inst{5-0}   = funct;
}

class SEQI_FM<bits<6> funct> : StdArch {
  bits<5> rs;
  bits<5> rt;
  bits<10> imm10;

  bits<64> Inst;

  let Inst{63-58} = 0x1c;
  let Inst{25-21} = rs;
  let Inst{20-16} = rt;
  let Inst{15-6}  = imm10;
  let Inst{5-0}   = funct;
}

//===----------------------------------------------------------------------===//
//  System calls format <op|code_|funct>
//===----------------------------------------------------------------------===//

class SYS_FM<bits<6> funct> : StdArch
{
  bits<20> code_;
  bits<64> Inst;
  let Inst{63-58} = 0x0;
  let Inst{25-6} = code_;
  let Inst{5-0}  = funct;
}

//===----------------------------------------------------------------------===//
//  Break instruction format <op|code_1|funct>
//===----------------------------------------------------------------------===//

class BRK_FM<bits<6> funct> : StdArch
{
  bits<10> code_1;
  bits<10> code_2;
  bits<64> Inst;
  let Inst{63-58} = 0x0;
  let Inst{25-16} = code_1;
  let Inst{15-6}  = code_2;
  let Inst{5-0}   = funct;
}

//===----------------------------------------------------------------------===//
//  Exception return format <Cop0|1|0|funct>
//===----------------------------------------------------------------------===//

class ER_FM<bits<6> funct> : StdArch
{
  bits<64> Inst;
  let Inst{63-58} = 0x10;
  let Inst{25}    = 1;
  let Inst{24-6}  = 0;
  let Inst{5-0}   = funct;
}


//===----------------------------------------------------------------------===//
//  Enable/disable interrupt instruction format <Cop0|MFMC0|rt|12|0|sc|0|0>
//===----------------------------------------------------------------------===//

class EI_FM<bits<1> sc> : StdArch
{
  bits<64> Inst;
  bits<5> rt;
  let Inst{63-58} = 0x10;
  let Inst{25-21} = 0xb;
  let Inst{20-16} = rt;
  let Inst{15-11} = 0xc;
  let Inst{10-6}  = 0;
  let Inst{5}     = sc;
  let Inst{4-0}   = 0;
}

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

class FFI<bits<6> op, dag outs, dag ins, string asmstr, list<dag> pattern>:
  InstSE<outs, ins, asmstr, pattern, NoItinerary, FrmFI>
{
  bits<7>  ft;
  bits<7>  base;
  bits<32> imm32;

  let Opcode = op;

  let Inst{57-51} = base;
  let Inst{50-44} = ft;
  let Inst{31-0}  = imm32;
}

class ADDS_FM<bits<6> funct, bits<7> fmt> {
  bits<7> fd;
  bits<7> fs;
  bits<7> ft;

  bits<64> Inst;

  let Inst{63-58} = 0x11;
  let Inst{33-27} = fmt;
  let Inst{26-20} = ft;
  let Inst{19-13} = fs;
  let Inst{12-6}  = fd;
  let Inst{5-0}   = funct;
}

class ABSS_FM<bits<6> funct, bits<7> fmt> {
  bits<7> fd;
  bits<7> fs;

  bits<64> Inst;


  let Inst{63-58} = 0x11;
  let Inst{33-27} = fmt;
  let Inst{26-20} = 0;
  let Inst{19-13} = fs;
  let Inst{12-6}  = fd;
  let Inst{5-0}   = funct;
}

class MFC1_FM<bits<7> funct> {
  bits<7> rt;
  bits<7> fs;

  bits<64> Inst;

  let Inst{63-58} = 0x11;
  let Inst{33-27} = funct;
  let Inst{26-20} = rt;
  let Inst{19-13} = fs;
  let Inst{12-0}  = 0;
}

class LW_FM<bits<6> op> : StdArch {
  bits<7> rt;
  bits<39> addr;

  bits<64> Inst;

  let Inst{63-58} = op;
  let Inst{57-51} = addr{38-32};
  let Inst{50-44} = rt;
  let Inst{31-0}  = addr{31-0};
}

class MADDS_FM<bits<3> funct, bits<3> fmt> {
  bits<7> fd;
  bits<7> fr;
  bits<7> fs;
  bits<7> ft;

  bits<64> Inst;

  let Inst{63-58} = 0x13;
  let Inst{33-27} = fr;
  let Inst{26-20} = ft;
  let Inst{19-13} = fs;
  let Inst{12-6}  = fd;
  let Inst{5-3}   = funct;
  let Inst{2-0}   = fmt;
}

class LWXC1_FM<bits<6> funct> {
  bits<7> fd;
  bits<7> base;
  bits<7> index;

  bits<64> Inst;

  let Inst{63-58} = 0x13;
  let Inst{33-27} = base;
  let Inst{26-20} = index;
  let Inst{19-13} = 0;
  let Inst{12-6}  = fd;
  let Inst{5-0}   = funct;
}

class SWXC1_FM<bits<6> funct> {
  bits<7> fs;
  bits<7> base;
  bits<7> index;

  bits<64> Inst;

  let Inst{63-58} = 0x13;
  let Inst{33-27} = base;
  let Inst{26-20} = index;
  let Inst{19-13} = fs;
  let Inst{12-6}  = 0;
  let Inst{5-0}   = funct;
}

class BC1F_FM<bit nd, bit tf> {
  bits<16> offset;

  bits<64> Inst;

  let Inst{63-58} = 0x11;
  let Inst{33-27} = 0x8;
  let Inst{26-24} = 0; // cc
  let Inst{23} = nd;
  let Inst{22} = tf;
  let Inst{15-0} = offset;
}

class CEQS_FM<bits<7> fmt> {
  bits<7> fs;
  bits<7> ft;
  bits<4> cond;

  bits<64> Inst;

  let Inst{63-58} = 0x11;
  let Inst{33-27} = fmt;
  let Inst{26-20} = ft;
  let Inst{19-13} = fs;
  let Inst{12-8} = 0; // cc
  let Inst{7-4} = 0x3;
  let Inst{3-0} = cond;
}

class C_COND_FM<bits<7> fmt, bits<4> c> : CEQS_FM<fmt> {
  let cond = c;
}

class CMov_I_F_FM<bits<6> funct, bits<7> fmt> {
  bits<7> fd;
  bits<7> fs;
  bits<7> rt;

  bits<64> Inst;

  let Inst{63-58} = 0x11;
  let Inst{33-27} = fmt;
  let Inst{26-20} = rt;
  let Inst{19-13} = fs;
  let Inst{12-6} = fd;
  let Inst{5-0} = funct;
}

class CMov_F_I_FM<bit tf> {
  bits<7> rd;
  bits<7> rs;

  bits<64> Inst;

  let Inst{63-58} = 0;
  let Inst{33-27} = rs;
  let Inst{26-24} = 0; // cc
  let Inst{23} = 0;
  let Inst{22} = tf;
  let Inst{21-20} = 0; // filling
  let Inst{19-13} = rd;
  let Inst{12-6} = 0;
  let Inst{5-0} = 1;
}

class CMov_F_F_FM<bits<7> fmt, bit tf> {
  bits<7> fd;
  bits<7> fs;

  bits<64> Inst;

  let Inst{63-58} = 0x11;
  let Inst{33-27} = fmt;
  let Inst{26-24} = 0; // cc
  let Inst{23} = 0;
  let Inst{22} = tf;
  let Inst{21-20} = 0; //filling
  let Inst{19-13} = fs;
  let Inst{12-6} = fd;
  let Inst{5-0} = 0x11;
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
