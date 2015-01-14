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
    memory = new uint8_t[TOTALSIZE];
  }
  ~OiMemoryModel() {
    if (memory)
      delete [] memory;
  }
 OiMemoryModel(const OiMemoryModel &A) : TOTALSIZE(50 * (1 << 20)) {   
  }

  // Load ELF file into model memory and return ELF entry point
  uint64_t LoadELF(const char *filename);

  uint64_t getExtent() const override { return TOTALSIZE; }

  int readByte(uint64_t Addr, uint8_t *Byte) const {
    if (Addr >= TOTALSIZE)
      return -1;
    *Byte = memory[Addr];
    return 0;
  }

  uint64_t readBytes(uint8_t *buf, uint64_t address, uint64_t size)
    const override {
    uint64_t Cur = address;
    for (; Cur < address + size; ++Cur)
      if (readByte(Cur, buf++) < 0)
	return Cur - address;
    return Cur - address;
  }

  const uint8_t *getPointer(uint64_t address, uint64_t size) const
    override {
    return &memory[address];
  }

  bool isValidAddress(uint64_t address) const override {
    if (address >= TOTALSIZE)
      return false;
    return true;
  }

  uint8_t * memory;
  const uint64_t TOTALSIZE;
  unsigned heapPtr;
};


#endif
