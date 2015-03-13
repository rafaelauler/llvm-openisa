//=== RelocationReader.h - ------------------------------------- -*- C++ -*-==//
//
// Reads and resolves relocation in a relocatable ELF.
//
//===----------------------------------------------------------------------===//
#ifndef RELOCATIONREADER_H
#define RELOCATIONREADER_H

#include "llvm/Object/ObjectFile.h"
#include <map>

namespace llvm {

namespace object {
class ObjectFile;
}

class OiIREmitter;

using namespace object;

class RelocationReader {
public:
  RelocationReader(const ObjectFile *obj, const SectionRef *&secptr,
                   uint64_t &addrptr)
      : Obj(obj), CurSection(secptr), CurAddr(addrptr) {
    for (const SectionRef &Section : Obj->sections()) {
      section_iterator Sec2 = Section.getRelocatedSection();
      if (Sec2 != Obj->section_end())
        SectionRelocMap[*Sec2].push_back(Section);
    }
  }
  bool ResolveRelocation(uint64_t &Res, uint64_t *Type = 0);
  bool CheckRelocation(relocation_iterator &Rel, StringRef &Name);

private:
  const ObjectFile *Obj;
  const SectionRef *&CurSection;
  uint64_t &CurAddr;
  // Set of relocation sections for the each section
  std::map<SectionRef, SmallVector<SectionRef, 1>> SectionRelocMap;
};
}

#endif
