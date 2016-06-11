//=== SBTUtils.cpp - General utilities ------------------*- C++ -*-==//
//
// Convenience functions to convert register numbers when reading
// an OpenISA binary and converting it to IR.
//
//===------------------------------------------------------------===//

#include "SBTUtils.h"
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
}
