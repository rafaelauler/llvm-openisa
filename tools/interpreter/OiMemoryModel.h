//=== OiMemoryModel.h ---*- C++ -*-==//
// 
// Contains methods useful for handling the model memory, e.g., 
// loading object files.
//
//===------------------------------------------------------------===//

#ifndef OIMEMORYMODEL_H
#define OIMEMORYMODEL_H

#include "llvm/Support/DataTypes.h"
#include "llvm/Support/MemoryObject.h"

class OiMemoryModel : public llvm::MemoryObject {
 public:
  OiMemoryModel() : TOTALSIZE(50 * (1 << 20)), heapPtr(0) {
    memory = new char[TOTALSIZE];
  }
  ~OiMemoryModel() {
    if (memory)
      delete [] memory;
  }
 OiMemoryModel(const OiMemoryModel &A) : TOTALSIZE(50 * (1 << 20)) {   
  }

  // Load ELF file into model memory and return ELF entry point
  uint64_t LoadELF(const char *filename);

  uint64_t getBase() const { return 0; }
  uint64_t getExtent() const { return TOTALSIZE; }

  int readByte(uint64_t Addr, uint8_t *Byte) const {
    if (Addr >= TOTALSIZE)
      return -1;
    *Byte = memory[Addr];
    return 0;
  }

  char * memory;
  const uint64_t TOTALSIZE;
  unsigned heapPtr;
};


#endif
