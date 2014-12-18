//=== RelocationReader.h - ------------------------------------- -*- C++ -*-==//
//
// Reads and resolves relocation in a relocatable ELF.
//
//===----------------------------------------------------------------------===//
#ifndef RELOCATIONREADER_H
#define RELOCATIONREADER_H

#include "llvm/Object/ObjectFile.h"

namespace llvm {

namespace object{
class ObjectFile;
}

class OiIREmitter;

using namespace object;

class RelocationReader {
 public:
 RelocationReader(const ObjectFile *obj, section_iterator *&secptr,
                  uint64_t &addrptr): Obj(obj), CurSection(secptr),
    CurAddr(addrptr){
  }
  bool ResolveRelocation(uint64_t &Res, uint64_t *Type = 0);
  bool CheckRelocation(relocation_iterator &Rel, StringRef &Name);
  
 private:
  const ObjectFile *Obj;
  section_iterator* &CurSection;
  uint64_t &CurAddr;
};

}

#endif
