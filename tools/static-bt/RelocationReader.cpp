#include "RelocationReader.h"
#include "SBTUtils.h"
#include "llvm/Object/ELF.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define NDEBUG

bool RelocationReader::ResolveRelocation(uint64_t &Res, uint64_t *Type,
                                         StringRef &SymbolNotFound,
                                         bool DirectCall) {
  relocation_iterator Rel = (*CurSection).relocation_end();
  std::error_code ec;
  StringRef Name;
  if (!CheckRelocation(Rel, Name))
    return false;

  if (Type) {
    if (error(Rel->getType(*Type)))
      llvm_unreachable("Error getting relocation type");
  }

  auto it = CommonSymbols.find(Name);
  if (it != CommonSymbols.end()) {
    Res = it->getValue();
    return true;
  }

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
      StringRef SecName;
      if (!error(seci->getName(SecName)) && SecName == ".text" && !DirectCall) {
        // If it is relative to text and it is not an indirect call target, it
        // should be an indirect call
        SymbolNotFound = SecName;;
      }

      uint64_t SectionAddr = seci->getAddress();
      // Relocatable file
      if (SectionAddr == 0) {
        SectionAddr = GetELFOffset(*seci);
      }
      Res += SectionAddr;
    }

    return true;
  }

#ifndef NDEBUG
  outs() << "Unresolved relocation: " << Name << "\n";
#endif
  SymbolNotFound = Name;
  return false;
}

bool RelocationReader::ResolveRelocation(Value *&Res, uint64_t *Type,
                                         bool *UndefinedSymbol, bool *IsFuncAddr,
                                         bool DirectCall) {
  uint64_t IntRes;
  StringRef SymbolNotFound;
  if (UndefinedSymbol)
    *UndefinedSymbol = false;
  if (IsFuncAddr)
    *IsFuncAddr = false;
  if (ResolveRelocation(IntRes, Type, SymbolNotFound, DirectCall)) {
    Res = ConstantInt::get(Type::getInt32Ty(getGlobalContext()), IntRes);
    if (!SymbolNotFound.size()) {
      return true;
    }
    assert(SymbolNotFound == ".text");
    if (IsFuncAddr)
      *IsFuncAddr = true;
    return false;
  }
  if (SymbolNotFound.size() > 0) {
    Res = ConstantExpr::getPointerCast(
        TheModule->getOrInsertGlobal(SymbolNotFound,
                                     Type::getInt32Ty(getGlobalContext())),
        Type::getInt32Ty(getGlobalContext()));
    if (UndefinedSymbol)
      *UndefinedSymbol = true;
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

void RelocationReader::ResolveAllDataRelocations(
    std::vector<uint8_t> &ShadowImage) {
  for (auto MapEntry : SectionRelocMap) {
    SectionRef Section = MapEntry.first;
    if (!Section.isData() || Section.isText())
      continue;
    uint64_t offset = GetELFOffset(Section);
    // For all relocation sections that contains relocations to this data
    // section...
    for (const SectionRef &RelocSec : MapEntry.second) {
      // For all relocations in this relocation section...
      for (const RelocationRef &Reloc : RelocSec.relocations()) {
        uint64_t PatchAddress;
        if (error(Reloc.getOffset(PatchAddress)))
          break;
        PatchAddress += offset;

        // Now get information about the target
        SymbolRef symb = *(Reloc.getSymbol());
        StringRef Name;
        if (error(symb.getName(Name))) {
          return;
        }
        // If the target of this relocation is the code section, leave
        // this to ProcessIndirectJumps()
        if (Name == ".text")
          continue;

        auto it = CommonSymbols.find(Name);
        if (it != CommonSymbols.end()) {
          // Patch it!
          *(int *)(&ShadowImage[PatchAddress]) =
              it->getValue() + *(int *)(&ShadowImage[PatchAddress]);
#ifndef NDEBUG
          outs() << "Patching " << format("%8" PRIx64, PatchAddress) << " with "
                 << format("%8" PRIx64, *(int *)(&ShadowImage[PatchAddress]))
                 << "\n";
#endif
          continue;
        }

#ifndef NDEBUG
        bool Patched = false;
#endif
        // Now we look up for this symbol in the list of all symbols...
        for (const auto &si : Obj->symbols()) {
          StringRef SName;
          if (error(si.getName(SName)))
            break;
          if (Name != SName)
            continue;

          // Found it, now get its address
          uint64_t TargetAddress;
          if (error(si.getAddress(TargetAddress)))
            break;
          if (TargetAddress == UnknownAddressOrSize)
            continue;

          section_iterator seci = Obj->section_end();
          // Check if it is relative to a section
          if ((!error(si.getSection(seci))) && seci != Obj->section_end()) {
            uint64_t SectionAddr = seci->getAddress();

            StringRef SecName;
            if (error(seci->getName(SecName)))
              continue;
            // If the target of this relocation is the code section, leave
            // this to ProcessIndirectJumps()
            if (SecName == ".text")
              continue;

            // Relocatable file
            if (SectionAddr == 0) {
              SectionAddr = GetELFOffset(*seci);
            }
            TargetAddress += SectionAddr;
          }

          // Patch it!
          *(int *)(&ShadowImage[PatchAddress]) =
              TargetAddress + *(int *)(&ShadowImage[PatchAddress]);
#ifndef NDEBUG
          outs() << "Patching " << format("%8" PRIx64, PatchAddress) << " with "
                 << format("%8" PRIx64, *(int *)(&ShadowImage[PatchAddress]))
                 << "\n";
          Patched = true;
#endif
          break;
        }

#ifndef NDEBUG
        if (!Patched)
          outs() << "Unresolved data relocation: " << Name << "\n";
#endif

      }
    }
  }
}

