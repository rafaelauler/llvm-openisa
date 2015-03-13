#include "RelocationReader.h"
#include "SBTUtils.h"
#include "llvm/Object/ELF.h"
#include "llvm/IR/Value.h"

using namespace llvm;

bool RelocationReader::ResolveRelocation(uint64_t &Res, uint64_t *Type) {
  relocation_iterator Rel = (*CurSection).relocation_end();
  std::error_code ec;
  StringRef Name;
  if (!CheckRelocation(Rel, Name))
    return false;
  for (auto i : Obj->sections()) {
    if (error(ec))
      break;
    StringRef SecName;
    if (error(i.getName(SecName)))
      break;
    if (SecName != Name)
      continue;

    uint64_t SectionAddr = i.getAddress();

    // Relocatable file
    if (SectionAddr == 0) {
      SectionAddr = GetELFOffset(i);
    }

    Res = SectionAddr;
    if (Type) {
      if (error(Rel->getType(*Type)))
        llvm_unreachable("Error getting relocation type");
    }
    return true;
  }

  for (const auto &si : Obj->symbols()) {
    StringRef SName;
    if (error(si.getName(SName)))
      break;
    if (Name != SName)
      continue;

    uint64_t Address;
    if (error(si.getAddress(Address)))
      break;
    if (Address == UnknownAddressOrSize)
      continue;
    //        Address -= SectionAddr;
    Res = Address;

    section_iterator seci = Obj->section_end();
    // Check if it is relative to a section
    if ((!error(si.getSection(seci))) && seci != Obj->section_end()) {
      uint64_t SectionAddr = seci->getAddress();

      // Relocatable file
      if (SectionAddr == 0) {
        SectionAddr = GetELFOffset(*seci);
      }
      Res += SectionAddr;
    }

    if (Type) {
      if (error(Rel->getType(*Type)))
        llvm_unreachable("Error getting relocation type");
    }
    return true;
  }

  return false;
}

bool RelocationReader::CheckRelocation(relocation_iterator &Rel,
                                       StringRef &Name) {
  std::error_code ec;
  uint64_t offset = GetELFOffset(*CurSection);
  for (const SectionRef &RelocSec : SectionRelocMap[*CurSection]) {
    for (const RelocationRef &Reloc : RelocSec.relocations()) {
      if (error(ec))
        break;
      uint64_t addr;
      if (error(Reloc.getOffset(addr)))
        break;
      if (offset + addr != CurAddr)
        continue;

      Rel = Reloc;
      SymbolRef symb = *(Reloc.getSymbol());
      if (!error(symb.getName(Name))) {
        return true;
      }
    }
  }

  return false;
}
