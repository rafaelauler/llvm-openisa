
#include "llvm/Support/MemoryObject.h"
#include "llvm/ADT/StringRef.h"

namespace llvm {

class StringRefMemoryObject {
  virtual void anchor();
  StringRef Bytes;
public:
  StringRefMemoryObject(StringRef bytes) : Bytes(bytes) {}

  uint64_t getBase() const { return 0; }
  uint64_t getExtent() const { return Bytes.size(); }

  int readByte(uint64_t Addr, uint8_t *Byte) const {
    if (Addr >= getExtent())
      return -1;
    *Byte = Bytes[Addr];
    return 0;
  }

  uint64_t readBytes(uint8_t *buf, uint64_t address, uint64_t size)
    const {
    uint64_t Cur = address;
    for (; Cur < address + size; ++Cur)
      if (readByte(Cur, buf++) < 0)
	return Cur - address;
    return Cur - address;
  }
};

}
