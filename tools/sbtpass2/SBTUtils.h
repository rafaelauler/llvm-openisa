//=== SBTUtils.h - General utilities --------------------*- C++ -*-==//
//
// Convenience functions to convert register numbers when reading
// an OpenISA binary and converting it to IR.
//
//===------------------------------------------------------------===//
#ifndef SBTUTILS_H
#define SBTUTILS_H
#include "llvm/ADT/StringMap.h"
#include "llvm/Object/ObjectFile.h"
#include <system_error>
#include <vector>
#include <utility>

namespace llvm {

namespace object {
class ObjectFile;
}

using namespace object;

bool error(std::error_code ec);
uint64_t GetELFOffset(const SectionRef &i);
std::vector<std::pair<uint64_t, StringRef>>
GetSymbolsList(const ObjectFile *Obj, const SectionRef &i);
llvm::StringMap<uint64_t>
GetComdatSymbolsList(const ObjectFile *Obj, uint64_t &TotalSize);
}
#endif
