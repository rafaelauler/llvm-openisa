
#include "llvm/Support/MemoryObject.h"
#include "llvm/ADT/StringRef.h"

namespace llvm {

class StringRefMemoryObject : public MemoryObject {
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
};

}
