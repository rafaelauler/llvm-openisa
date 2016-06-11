//===-- Static Binary Translator ------------------------------------------===//
// main file sbtpass2.cpp
//===----------------------------------------------------------------------===//

//#define NDEBUG

#include "SBTUtils.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/IR/Verifier.h"
#include "llvm/PassManager.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Triple.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDisassembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Object/Archive.h"
#include "llvm/Object/COFF.h"
#include "llvm/Object/ELF.h"
#include "llvm/Object/MachO.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/GraphWriter.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/MemoryObject.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/ToolOutputFile.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>

namespace llvm {

namespace object {
class COFFObjectFile;
class ObjectFile;
class RelocationRef;
}

// Various helper functions.
void DumpBytes(StringRef bytes);
}
using namespace llvm;
using namespace object;

namespace llvm {
extern Target TheMipsTarget, TheMipselTarget;
}

static cl::list<std::string> InputFilenames(cl::Positional,
                                            cl::desc("<input object files>"),
                                            cl::ZeroOrMore);

static cl::opt<std::string> OutputFilename("o", cl::desc("Output filename"),
                                           cl::value_desc("filename"));

static StringRef ToolName;

static void PatchBinary(const ObjectFile *Obj) {
  std::error_code ec;
  uint32_t NumElements = 0;
  std::vector<std::pair<uint32_t, uint32_t>> PatchList;

  for (const SectionRef &i : Obj->sections()) {
    if (error(ec))
      break;
    if (!i.isData())
      continue;

    // Make a list of all the symbols in this section.
    std::vector<std::pair<uint64_t, StringRef>> Symbols =
        GetSymbolsList(Obj, i);

    StringRef name;
    if (error(i.getName(name)))
      break;
    if (name != ".data")
      continue;

    uint64_t SecOffset = GetELFOffset(i);

    StringRef BytesStr;
    if (error(i.getContents(BytesStr)))
      break;
    ArrayRef<uint8_t> Bytes(reinterpret_cast<const uint8_t *>(BytesStr.data()),
                             BytesStr.size());

    bool Found = false;
    uint32_t ShadowOffset = 0;
    for (unsigned si = 0, se = Symbols.size(); si != se; ++si) {
      if (Symbols[si].second != "ShadowMemory")
        continue;

      Found = true;
      ShadowOffset = Symbols[si].first;
    }

    if (!Found) {
      outs() << ToolName << ": Could not find 'ShadowMemory' symbol in .data.\n";
      return;
    }

    Found = false;
    for (unsigned si = 0, se = Symbols.size(); si != se; ++si) {
      if (Symbols[si].second != "PatchSection")
        continue;
      uint64_t Offset = Symbols[si].first;

      Found = true;
      NumElements = *(const uint32_t*)&Bytes[Offset];
#ifndef NDEBUG
      outs() << "Dumping " << NumElements << " elements.\n";
#endif
      for (uint32_t I = 0; I < NumElements; ++I) {
        uint32_t Addr = *(const uint32_t*)&Bytes[((I << 1) << 2) + 4 + Offset];
        uint32_t Tgt = *(const uint32_t*)&Bytes[((I << 1) << 2) + 8 + Offset];
#ifndef NDEBUG
        outs() << format("%4" PRIx32, Addr) << " - "
               << format("%4" PRIx32, Tgt) << "\n";
#endif
        PatchList.push_back(std::make_pair(SecOffset + Addr + ShadowOffset, Tgt));
      }
    }

    if (!Found) {
      outs() << ToolName << ": Could not find 'PatchSection' symbol in .data.\n";
      return;
    }
  }

  std::ofstream fs;
  fs.open(Obj->getFileName(),
          std::ios::in | std::ios::out | std::ios::binary | std::ios::ate);
  if ((fs.rdstate() & std::ifstream::failbit) != 0) {
    outs() << ToolName << ": '" << Obj->getFileName() << "': Error opening for write\n";
    return;
  }

  for (auto Item : PatchList) {
    uint32_t Off = Item.first;
    uint32_t Contents = Item.second;
    char buf[4];
    memcpy(buf, &Contents, 4);
#ifndef NDEBUG
    outs() << "Writing at " << format("%4" PRIx32, Off) << " value "
           << format("%4" PRIx32, Contents)
           << "\n";
#endif
    fs.seekp(Off, std::ios_base::beg);
    fs.write(buf, 4);
  }

  fs.close();

  outs() << ToolName << ":  Patched " << NumElements << " locations.\n";

}

/// @brief Open file and figure out how to dump it.
static void PatchInput(StringRef file) {
  // If file isn't stdin, check that it exists.
  if (file != "-" && !sys::fs::exists(file)) {
    errs() << ToolName << ": '" << file << "': "
           << "No such file\n";
    return;
  }

  // Attempt to open the binary.
  std::unique_ptr<Binary> binary;
  ErrorOr<OwningBinary<Binary>> BinaryOrErr = createBinary(file);
  if (std::error_code EC = BinaryOrErr.getError()) {
    errs() << ToolName << ": '" << file << "': " << EC.message() << ".\n";
    return;
  }
  Binary &Binary = *BinaryOrErr.get().getBinary();

  if (isa<Archive>(&Binary)) {
    outs() << ToolName << ": '" << file << "':File is an archive. Aborted.\n";
    return;
  }
  if (ObjectFile *o = dyn_cast<ObjectFile>(&Binary))
    PatchBinary(o);
  else
    errs() << ToolName << ": '" << file << "': "
           << "Unrecognized file type.\n";
}

int main(int argc, char **argv) {
  // Print a stack trace if we signal out.
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);
  llvm_shutdown_obj Y; // Call llvm_shutdown() on exit.

  // Initialize targets and assembly printers/parsers.
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllDisassemblers();

  // Register the target printer for --version.
  cl::AddExtraVersionPrinter(TargetRegistry::printRegisteredTargetsForVersion);

  cl::ParseCommandLineOptions(argc, argv,
                              "Open-ISA Static Binary Translator Pass 2\n");
  ToolName = argv[0];

  // Defaults to a.out if no filenames specified.
  if (InputFilenames.size() == 0)
    InputFilenames.push_back("a.out");

  std::for_each(InputFilenames.begin(), InputFilenames.end(), PatchInput);

  return 0;
}
