//=== RelocationReader.h - ------------------------------------- -*- C++ -*-==//
//
// Reads and resolves relocation in a relocatable ELF.
//
//===----------------------------------------------------------------------===//
#ifndef RELOCATIONREADER_H
#define RELOCATIONREADER_H

#include "llvm/ADT/StringMap.h"
#include "llvm/Object/ObjectFile.h"
#include <map>

namespace llvm {

namespace object {
class ObjectFile;
}

class OiIREmitter;
class Module;
class Value;

using namespace object;

class RelocationReader {
public:
  RelocationReader(llvm::Module *M, const ObjectFile *obj,
                   const SectionRef *&secptr, uint64_t &addrptr,
                   llvm::StringMap<uint64_t> &commonsymbols)
      : TheModule(M), Obj(obj), CurSection(secptr), CurAddr(addrptr),
        CommonSymbols(commonsymbols) {
    for (const SectionRef &Section : Obj->sections()) {
      section_iterator Sec2 = Section.getRelocatedSection();
      if (Sec2 != Obj->section_end())
        SectionRelocMap[*Sec2].push_back(Section);
    }
  }
  bool ResolveRelocation(uint64_t &Res, uint64_t *Type,
                         StringRef &SymbolNotFound,
                         bool DirectCall);
  bool ResolveRelocation(llvm::Value *&Res, uint64_t *Type,
                         bool *UndefinedSymbol, bool *IsFuncAddr = 0,
                         bool DirectCall = false);
  bool CheckRelocation(relocation_iterator &Rel, StringRef &Name);
  void ResolveAllDataRelocations(std::vector<uint8_t>& ShadowImage);

private:
  Module *TheModule;
  const ObjectFile *Obj;
  const SectionRef *&CurSection;
  uint64_t &CurAddr;
  llvm::StringMap<uint64_t> &CommonSymbols;
  // Set of relocation sections for each section
  std::map<SectionRef, SmallVector<SectionRef, 1>> SectionRelocMap;
};
}

#endif
