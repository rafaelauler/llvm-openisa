//=== SBTUtils.cpp - General utilities ------------------*- C++ -*-==//
//
// Convenience functions to convert register numbers when reading
// an OpenISA binary and converting it to IR.
//
//===------------------------------------------------------------===//

#include "SBTUtils.h"
#include "../lib/Target/Mips/MipsInstrInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Object/ELF.h"

namespace llvm {

bool error(std::error_code ec) {
  if (!ec)
    return false;

  outs() << "error reading file: " << ec.message() << ".\n";
  outs().flush();
  return true;
}

unsigned conv32(unsigned regnum) {
  switch (regnum) {
  case Mips::AT_64:
    return Mips::AT;
  case Mips::FP_64:
    return Mips::FP;
  case Mips::SP_64:
    return Mips::SP;
  case Mips::RA_64:
    return Mips::RA;
  case Mips::ZERO_64:
    return Mips::ZERO;
  case Mips::GP_64:
    return Mips::GP;
  case Mips::A0_64:
    return Mips::A0;
  case Mips::A1_64:
    return Mips::A1;
  case Mips::A2_64:
    return Mips::A2;
  case Mips::A3_64:
    return Mips::A3;
  case Mips::V0_64:
    return Mips::V0;
  case Mips::V1_64:
    return Mips::V1;
  case Mips::S0_64:
    return Mips::S0;
  case Mips::S1_64:
    return Mips::S1;
  case Mips::S2_64:
    return Mips::S2;
  case Mips::S3_64:
    return Mips::S3;
  case Mips::S4_64:
    return Mips::S4;
  case Mips::S5_64:
    return Mips::S5;
  case Mips::S6_64:
    return Mips::S6;
  case Mips::S7_64:
    return Mips::S7;
  case Mips::K0_64:
    return Mips::K0;
  case Mips::K1_64:
    return Mips::K1;
  case Mips::T0_64:
    return Mips::T0;
  case Mips::T1_64:
    return Mips::T1;
  case Mips::T2_64:
    return Mips::T2;
  case Mips::T3_64:
    return Mips::T3;
  case Mips::T4_64:
    return Mips::T4;
  case Mips::T5_64:
    return Mips::T5;
  case Mips::T6_64:
    return Mips::T6;
  case Mips::T7_64:
    return Mips::T7;
  case Mips::T8_64:
    return Mips::T8;
  case Mips::T9_64:
    return Mips::T9;
  case Mips::D0_64:
    return Mips::F0;
  case Mips::D1_64:
    return Mips::F1;
  case Mips::D2_64:
    return Mips::F2;
  case Mips::D3_64:
    return Mips::F3;
  case Mips::D4_64:
    return Mips::F4;
  case Mips::D5_64:
    return Mips::F5;
  case Mips::D6_64:
    return Mips::F6;
  case Mips::D7_64:
    return Mips::F7;
  case Mips::D8_64:
    return Mips::F8;
  case Mips::D9_64:
    return Mips::F9;
  case Mips::D10_64:
    return Mips::F10;
  case Mips::D11_64:
    return Mips::F11;
  case Mips::D12_64:
    return Mips::F12;
  case Mips::D13_64:
    return Mips::F13;
  case Mips::D14_64:
    return Mips::F14;
  case Mips::D15_64:
    return Mips::F15;
  case Mips::D16_64:
    return Mips::F16;
  case Mips::D17_64:
    return Mips::F17;
  case Mips::D18_64:
    return Mips::F18;
  case Mips::D19_64:
    return Mips::F19;
  case Mips::D20_64:
    return Mips::F20;
  case Mips::D21_64:
    return Mips::F21;
  case Mips::D22_64:
    return Mips::F22;
  case Mips::D23_64:
    return Mips::F23;
  case Mips::D24_64:
    return Mips::F24;
  case Mips::D25_64:
    return Mips::F25;
  case Mips::D26_64:
    return Mips::F26;
  case Mips::D27_64:
    return Mips::F27;
  case Mips::D28_64:
    return Mips::F28;
  case Mips::D29_64:
    return Mips::F29;
  case Mips::D30_64:
    return Mips::F30;
  case Mips::D31_64:
    return Mips::F31;
  case Mips::D32_64:
    return Mips::F32;
  case Mips::D33_64:
    return Mips::F33;
  case Mips::D34_64:
    return Mips::F34;
  case Mips::D35_64:
    return Mips::F35;
  case Mips::D36_64:
    return Mips::F36;
  case Mips::D37_64:
    return Mips::F37;
  case Mips::D38_64:
    return Mips::F38;
  case Mips::D39_64:
    return Mips::F39;
  case Mips::D40_64:
    return Mips::F40;
  case Mips::D41_64:
    return Mips::F41;
  case Mips::D42_64:
    return Mips::F42;
  case Mips::D43_64:
    return Mips::F43;
  case Mips::D44_64:
    return Mips::F44;
  case Mips::D45_64:
    return Mips::F45;
  case Mips::D46_64:
    return Mips::F46;
  case Mips::D47_64:
    return Mips::F47;
  case Mips::D48_64:
    return Mips::F48;
  case Mips::D49_64:
    return Mips::F49;
  case Mips::D50_64:
    return Mips::F50;
  case Mips::D51_64:
    return Mips::F51;
  case Mips::D52_64:
    return Mips::F52;
  case Mips::D53_64:
    return Mips::F53;
  case Mips::D54_64:
    return Mips::F54;
  case Mips::D55_64:
    return Mips::F55;
  case Mips::D56_64:
    return Mips::F56;
  case Mips::D57_64:
    return Mips::F57;
  case Mips::D58_64:
    return Mips::F58;
  case Mips::D59_64:
    return Mips::F59;
  case Mips::D60_64:
    return Mips::F60;
  case Mips::D61_64:
    return Mips::F61;
  case Mips::D62_64:
    return Mips::F62;
  case Mips::D63_64:
    return Mips::F63;

    //    return regnum - 1;
  }
  return regnum;
}

unsigned ConvFromDirective(unsigned regnum) {
  switch (regnum) {
  case 0:
    return Mips::ZERO;
  case 1:
    return Mips::AT;
  case 4:
    return Mips::A0;
  case 5:
    return Mips::A1;
  case 6:
    return Mips::A2;
  case 7:
    return Mips::A3;
  case 2:
    return Mips::V0;
  case 3:
    return Mips::V1;
  case 16:
    return Mips::S0;
  case 17:
    return Mips::S1;
  case 18:
    return Mips::S2;
  case 19:
    return Mips::S3;
  case 20:
    return Mips::S4;
  case 21:
    return Mips::S5;
  case 22:
    return Mips::S6;
  case 23:
    return Mips::S7;
  case 26:
    return Mips::K0;
  case 27:
    return Mips::K1;
  case 29:
    return Mips::SP;
  case 30:
    return Mips::FP;
  case 28:
    return Mips::GP;
  case 31:
    return Mips::RA;
  case 8:
    return Mips::T0;
  case 9:
    return Mips::T1;
  case 10:
    return Mips::T2;
  case 11:
    return Mips::T3;
  case 12:
    return Mips::T4;
  case 13:
    return Mips::T5;
  case 14:
    return Mips::T6;
  case 15:
    return Mips::T7;
  case 24:
    return Mips::T8;
  case 25:
    return Mips::T9;
  }
  llvm_unreachable("Invalid register");
  return -1;
}

unsigned ConvToDirective(unsigned regnum) {
  switch (regnum) {
  case Mips::ZERO:
    return 0;
  case Mips::AT:
    return 1;
  case Mips::A0:
    return 4;
  case Mips::A1:
    return 5;
  case Mips::A2:
    return 6;
  case Mips::A3:
    return 7;
  case Mips::V0:
    return 2;
  case Mips::V1:
    return 3;
  case Mips::S0:
    return 16;
  case Mips::S1:
    return 17;
  case Mips::S2:
    return 18;
  case Mips::S3:
    return 19;
  case Mips::S4:
    return 20;
  case Mips::S5:
    return 21;
  case Mips::S6:
    return 22;
  case Mips::S7:
    return 23;
  case Mips::K0:
    return 26;
  case Mips::K1:
    return 27;
  case Mips::SP:
    return 29;
  case Mips::FP:
    return 30;
  case Mips::GP:
    return 28;
  case Mips::RA:
    return 31;
  case Mips::T0:
    return 8;
  case Mips::T1:
    return 9;
  case Mips::T2:
    return 10;
  case Mips::T3:
    return 11;
  case Mips::T4:
    return 12;
  case Mips::T5:
    return 13;
  case Mips::T6:
    return 14;
  case Mips::T7:
    return 15;
  case Mips::T8:
    return 24;
  case Mips::T9:
    return 25;
  case Mips::R32:
    return 32;
  case Mips::R33:
    return 33;
  case Mips::R34:
    return 34;
  case Mips::R35:
    return 35;
  case Mips::R36:
    return 36;
  case Mips::R37:
    return 37;
  case Mips::R38:
    return 38;
  case Mips::R39:
    return 39;
  case Mips::R40:
    return 40;
  case Mips::R41:
    return 41;
  case Mips::R42:
    return 42;
  case Mips::R43:
    return 43;
  case Mips::R44:
    return 44;
  case Mips::R45:
    return 45;
  case Mips::R46:
    return 46;
  case Mips::R47:
    return 47;
  case Mips::R48:
    return 48;
  case Mips::R49:
    return 49;
  case Mips::R50:
    return 50;
  case Mips::R51:
    return 51;
  case Mips::R52:
    return 52;
  case Mips::R53:
    return 53;
  case Mips::R54:
    return 54;
  case Mips::R55:
    return 55;
  case Mips::R56:
    return 56;
  case Mips::R57:
    return 57;
  case Mips::R58:
    return 58;
  case Mips::R59:
    return 59;
  case Mips::R60:
    return 60;
  case Mips::R61:
    return 61;
  case Mips::R62:
    return 62;
  case Mips::R63:
    return 63;
  case Mips::R64:
    return 64;
  case Mips::R65:
    return 65;

  // Floating point registers
  case Mips::D0:
  case Mips::F0:
    return 128;
  case Mips::F1:
    return 129;
  case Mips::D1:
  case Mips::F2:
    return 130;
  case Mips::F3:
    return 131;
  case Mips::D2:
  case Mips::F4:
    return 132;
  case Mips::F5:
    return 133;
  case Mips::D3:
  case Mips::F6:
    return 134;
  case Mips::F7:
    return 135;
  case Mips::D4:
  case Mips::F8:
    return 136;
  case Mips::F9:
    return 137;
  case Mips::D5:
  case Mips::F10:
    return 138;
  case Mips::F11:
    return 139;
  case Mips::D6:
  case Mips::F12:
    return 140;
  case Mips::F13:
    return 141;
  case Mips::D7:
  case Mips::F14:
    return 142;
  case Mips::F15:
    return 143;
  case Mips::D8:
  case Mips::F16:
    return 144;
  case Mips::F17:
    return 145;
  case Mips::D9:
  case Mips::F18:
    return 146;
  case Mips::F19:
    return 147;
  case Mips::D10:
  case Mips::F20:
    return 148;
  case Mips::F21:
    return 149;
  case Mips::D11:
  case Mips::F22:
    return 150;
  case Mips::F23:
    return 151;
  case Mips::D12:
  case Mips::F24:
    return 152;
  case Mips::F25:
    return 153;
  case Mips::D13:
  case Mips::F26:
    return 154;
  case Mips::F27:
    return 155;
  case Mips::D14:
  case Mips::F28:
    return 156;
  case Mips::F29:
    return 157;
  case Mips::D15:
  case Mips::F30:
    return 158;
  case Mips::F31:
    return 159;
  case Mips::D16:
  case Mips::F32:
    return 160;
  case Mips::F33:
    return 161;
  case Mips::D17:
  case Mips::F34:
    return 162;
  case Mips::F35:
    return 163;
  case Mips::D18:
  case Mips::F36:
    return 164;
  case Mips::F37:
    return 165;
  case Mips::D19:
  case Mips::F38:
    return 166;
  case Mips::F39:
    return 167;
  case Mips::D20:
  case Mips::F40:
    return 168;
  case Mips::F41:
    return 169;
  case Mips::D21:
  case Mips::F42:
    return 170;
  case Mips::F43:
    return 171;
  case Mips::D22:
  case Mips::F44:
    return 172;
  case Mips::F45:
    return 173;
  case Mips::D23:
  case Mips::F46:
    return 174;
  case Mips::F47:
    return 175;
  case Mips::D24:
  case Mips::F48:
    return 176;
  case Mips::F49:
    return 177;
  case Mips::D25:
  case Mips::F50:
    return 178;
  case Mips::F51:
    return 179;
  case Mips::D26:
  case Mips::F52:
    return 180;
  case Mips::F53:
    return 181;
  case Mips::D27:
  case Mips::F54:
    return 182;
  case Mips::F55:
    return 183;
  case Mips::D28:
  case Mips::F56:
    return 184;
  case Mips::F57:
    return 185;
  case Mips::D29:
  case Mips::F58:
    return 186;
  case Mips::F59:
    return 187;
  case Mips::D30:
  case Mips::F60:
    return 188;
  case Mips::F61:
    return 189;
  case Mips::D31:
  case Mips::F62:
    return 190;
  case Mips::F63:
    return 191;
  }
  outs() << regnum << "\n";
  llvm_unreachable("Invalid register");
  return -1;
}

unsigned ConvToDirectiveDbl(unsigned regnum) {
  return (ConvToDirective(regnum) - 128) >> 1;
}

uint64_t GetELFOffset(const SectionRef &i) {
  DataRefImpl Sec = i.getRawDataRefImpl();
  const object::Elf_Shdr_Impl<object::ELFType<support::little, 2, false>> *sec =
      reinterpret_cast<const object::Elf_Shdr_Impl<
          object::ELFType<support::little, 2, false>> *>(Sec.p);
  return sec->sh_offset;
}

std::vector<std::pair<uint64_t, StringRef>>
GetSymbolsList(const ObjectFile *Obj, const SectionRef &i) {
  uint64_t SectionAddr = i.getAddress();

  std::error_code ec;
  // Make a list of all the symbols in this section.
  std::vector<std::pair<uint64_t, StringRef>> Symbols;
  for (auto si : Obj->symbols()) {
    if (!i.containsSymbol(si))
      continue;

    uint64_t Address;
    if (error(si.getAddress(Address)))
      break;
    if (Address == UnknownAddressOrSize)
      continue;
    Address -= SectionAddr;

    StringRef Name;
    if (error(si.getName(Name)))
      break;
    Symbols.push_back(std::make_pair(Address, Name));
  }

  // Sort the symbols by address, just in case they didn't come in that way.
  array_pod_sort(Symbols.begin(), Symbols.end());
  return Symbols;
}

llvm::StringMap<uint64_t>
GetComdatSymbolsList(const ObjectFile *o, uint64_t &TotalSize) {
  TotalSize = 0;

  std::error_code ec;
  llvm::StringMap<uint64_t> Symbols;
  section_iterator BSSSection = o->section_end();
  // first put in BSS symbols
  for (auto &i : o->sections()) {
    if (!i.isBSS())
      continue;
    uint64_t SectSize = i.getSize();
    TotalSize += SectSize;
    BSSSection = i;
    break;
  }
  if (BSSSection != o->section_end() && TotalSize > 0) {
    for (auto Symbol : o->symbols()) {
      section_iterator Section = o->section_end();
      if (error(Symbol.getSection(Section)))
        continue;
      if (Section != BSSSection)
        continue;
      uint64_t Address;
      StringRef Name;
      if (error(Symbol.getName(Name)))
        continue;
      if (error(Symbol.getAddress(Address)))
        continue;
      assert(Address < TotalSize);
      Symbols[Name] = Address;
    }
  }
  for (auto Symbol : o->symbols()) {
    section_iterator Section = o->section_end();
    uint64_t Size;
    SymbolRef::Type Type;
    uint32_t Flags = Symbol.getFlags();
    if (error(Symbol.getType(Type)))
      continue;
    if (error(Symbol.getSize(Size)))
      continue;
    if (error(Symbol.getSection(Section)))
      continue;

    if (Section != o->section_end())
      continue;
    if (Type != SymbolRef::ST_Data)
      continue;
    if ((Flags & SymbolRef::SF_Common) == 0)
      continue;

    StringRef Name;
    if (error(Symbol.getName(Name)))
      continue;
    Symbols[Name] = TotalSize;
    TotalSize += Size;
  }

  return Symbols;
}
Value *GetFirstInstruction(Value *o0, Value *o1) {
  if (o0 && isa<Instruction>(o0))
    return o0;
  return o1;
}

Value *GetFirstInstruction(Value *o0, Value *o1, Value *o2) {
  if (o0 && isa<Instruction>(o0))
    return o0;
  if (o1 && isa<Instruction>(o1))
    return o1;
  return o2;
}

Value *GetFirstInstruction(Value *o0, Value *o1, Value *o2, Value *o3) {
  if (o0 && isa<Instruction>(o0))
    return o0;
  if (o1 && isa<Instruction>(o1))
    return o1;
  if (o2 && isa<Instruction>(o2))
    return o2;
  return o3;
}

Value *GetFirstInstruction(Value *o0, Value *o1, Value *o2, Value *o3,
                           Value *o4) {
  if (o0 && isa<Instruction>(o0))
    return o0;
  if (o1 && isa<Instruction>(o1))
    return o1;
  if (o2 && isa<Instruction>(o2))
    return o2;
  if (o3 && isa<Instruction>(o3))
    return o3;
  return o4;
}

uint32_t GetInstructionSize() { return 4; }
}
