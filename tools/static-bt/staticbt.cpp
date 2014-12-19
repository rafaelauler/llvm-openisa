//===-- Static Binary Translator ------------------------------------------===//
// main file staticbt.cpp
//===----------------------------------------------------------------------===//

//#define NDEBUG

#include "OiInstTranslate.h"
#include "StringRefMemoryObject.h"
#include "SBTUtils.h"
#include "OiCombinePass.h"
//#include "MCFunction.h"
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

namespace llvm {

namespace object {
  class COFFObjectFile;
  class ObjectFile;
  class RelocationRef;
}

extern cl::opt<std::string> TripleName;
extern cl::opt<std::string> ArchName;

// Various helper functions.
bool RelocAddressLess(object::RelocationRef a, object::RelocationRef b);
void DumpBytes(StringRef bytes);
void DisassembleInputMachO(StringRef Filename);
void printCOFFUnwindInfo(const object::COFFObjectFile* o);
void printELFFileHeader(const object::ObjectFile *o);

}
using namespace llvm;
using namespace object;

namespace llvm {
  extern Target TheOiTarget, TheOielTarget;
}

static cl::list<std::string>
InputFilenames(cl::Positional, cl::desc("<input object files>"),cl::ZeroOrMore);

static cl::opt<std::string>
OutputFilename("o", cl::desc("Output filename"),
               cl::value_desc("filename"));

static cl::opt<bool>
Optimize("optimize", cl::desc("Optimize the output LLVM bitcode file"));

static cl::opt<uint64_t>
StackSize("stacksize", cl::desc("Specifies the space reserved for the stack"
                                "(Default 300B)"),
          cl::init(300ULL));


static cl::opt<bool>
Dump("dump", cl::desc("Dump the output LLVM bitcode file"));

static cl::list<std::string>
MAttrs("mattr",
  cl::CommaSeparated,
  cl::desc("Target specific attributes"),
  cl::value_desc("a1,+a2,-a3,..."));

cl::opt<std::string>
llvm::TripleName("triple", cl::desc("Target triple to disassemble for, "
                                    "see -version for available targets"));

static StringRef ToolName;

static const Target *getTarget(const ObjectFile *Obj = NULL) {
  // Figure out the target triple.
  llvm::Triple TheTriple("unknown-unknown-unknown");
  if (TripleName.empty()) {
    if (Obj)
      TheTriple.setArch(Triple::ArchType(Obj->getArch()));
  } else
    TheTriple.setTriple(Triple::normalize(TripleName));

  // Get the target specific parser.
  const Target *TheTarget = &TheOielTarget; 
  if (!TheTarget) {
    errs() << "Could not load OpenISA target.";
    return 0;
  }

  // Update the triple name and return the found target.
  TripleName = TheTriple.getTriple();
  return TheTarget;
}

void llvm::DumpBytes(StringRef bytes) {
  static const char hex_rep[] = "0123456789abcdef";
  // FIXME: The real way to do this is to figure out the longest instruction
  //        and align to that size before printing. I'll fix this when I get
  //        around to outputting relocations.
  // 15 is the longest x86 instruction
  // 3 is for the hex rep of a byte + a space.
  // 1 is for the null terminator.
  enum { OutputSize = (15 * 3) + 1 };
  char output[OutputSize];

  assert(bytes.size() <= 15
    && "DumpBytes only supports instructions of up to 15 bytes");
  memset(output, ' ', sizeof(output));
  unsigned index = 0;
  for (StringRef::iterator i = bytes.begin(),
                           e = bytes.end(); i != e; ++i) {
    output[index] = hex_rep[(*i & 0xF0) >> 4];
    output[index + 1] = hex_rep[*i & 0xF];
    index += 3;
  }

  output[sizeof(output) - 1] = 0;
  outs() << output;
}

bool llvm::RelocAddressLess(RelocationRef a, RelocationRef b) {
  uint64_t a_addr, b_addr;
  if (error(a.getOffset(a_addr))) return false;
  if (error(b.getOffset(b_addr))) return false;
  return a_addr < b_addr;
}

static tool_output_file *GetBitcodeOutputStream() {
  std::error_code EC;
  tool_output_file *Out = new tool_output_file(OutputFilename, EC,
                                               sys::fs::F_None);
  if (EC) {
    errs() << EC.message() << '\n';
    delete Out;
    return nullptr;
  }

  return Out;
}

void OptimizeAndWriteBitcode(OiInstTranslate *oit) {
  Module *m = oit->takeModule();
  FunctionPassManager OurFPM(m);

  if (Optimize) {    
    OurFPM.add(createPromoteMemoryToRegisterPass());
    OurFPM.add(new OiCombinePass());
    OurFPM.add(createInstructionCombiningPass());
    OurFPM.add(createReassociatePass());
    OurFPM.add(createGVNPass());
    OurFPM.add(createCFGSimplificationPass());


    OurFPM.doInitialization();
    
    for (Module::iterator I = m->begin(); I != m->end(); ++I) {
      if (I->isDeclaration())
        continue;
      verifyFunction(*I);
      OurFPM.run(*I);
      }
  }

  // Set up the optimizer pipeline.  Start with registering info about how the
  // target lays out data structures.
  //OurFPM.add(new DataLayout(*TheExecutionEngine->getDataLayout()));
  // Provide basic AliasAnalysis support for GVN.
  //OurFPM.add(createBasicAliasAnalysisPass());
  // Promote allocas to registers.
  //  OurFPM.add(createPromoteMemoryToRegisterPass());
  //  OurFPM.add(new OiCombinePass());
  // Do simple "peephole" optimizations and bit-twiddling optzns.
  //  OurFPM.add(createInstructionCombiningPass());
  // Reassociate expressions.
  //  OurFPM.add(createReassociatePass());
  // Eliminate Common SubExpressions.
  //  OurFPM.add(createGVNPass());
  // Simplify the control flow graph (deleting unreachable blocks, etc).
  //  OurFPM.add(createCFGSimplificationPass());
 
  if (Dump) {
    m->dump();
  }
  if (OutputFilename != "") {
    std::unique_ptr<tool_output_file> outfile(GetBitcodeOutputStream());
    if (outfile) {
      WriteBitcodeToFile(m,outfile->os());
      outfile->keep();
    }
  }
  delete m;
}

static void DisassembleObject(const ObjectFile *Obj, bool InlineRelocs) {
  const Target *TheTarget = getTarget(Obj);
  // getTarget() will have already issued a diagnostic if necessary, so
  // just bail here if it failed.
  if (!TheTarget)
    return;

  // Package up features to be passed to target/subtarget
  std::string FeaturesStr;
  if (MAttrs.size()) {
    SubtargetFeatures Features;
    for (unsigned i = 0; i != MAttrs.size(); ++i)
      Features.AddFeature(MAttrs[i]);
    FeaturesStr = Features.getString();
  }

  std::unique_ptr<const MCRegisterInfo> MRI(
      TheTarget->createMCRegInfo(TripleName));
  if (!MRI) {
    errs() << "error: no register info for target " << TripleName << "\n";
    return;
  }


  // Set up disassembler.
  std::unique_ptr<const MCAsmInfo> AsmInfo(TheTarget->createMCAsmInfo(*MRI, TripleName));
  if (!AsmInfo) {
    errs() << "error: no assembly info for target " << TripleName << "\n";
    return;
  }

  std::unique_ptr<const MCSubtargetInfo> STI(
          TheTarget->createMCSubtargetInfo(TripleName, "", FeaturesStr));
  if (!STI) {
    errs() << "error: no subtarget info for target " << TripleName << "\n";
    return;
  }

  std::unique_ptr<const MCObjectFileInfo> MOFI(new MCObjectFileInfo);
  MCContext Ctx(AsmInfo.get(), MRI.get(), MOFI.get());

  std::unique_ptr<const MCDisassembler> DisAsm(
		TheTarget->createMCDisassembler(*STI, Ctx));
  if (!DisAsm) {
    errs() << "error: no disassembler for target " << TripleName << "\n";
    return;
  }

  std::unique_ptr<const MCInstrInfo> MII(TheTarget->createMCInstrInfo());
  if (!MII) {
    errs() << "error: no instruction info for target " << TripleName << "\n";
    return;
  }

  //  int AsmPrinterVariant = AsmInfo->getAssemblerDialect();

  std::unique_ptr<OiInstTranslate> IP(new OiInstTranslate(*AsmInfo, *MII, *MRI, Obj,
                                                    StackSize));
  // TheTarget->createMCInstPrinter(AsmPrinterVariant, *AsmInfo, *MII, *MRI, *STI));
  if (!IP) {
    errs() << "error: no instruction printer for target " << TripleName
           << '\n';
    return;
  }

  std::error_code ec;
  for (const SectionRef &i : Obj->sections()) {
    if (error(ec)) break;
    if (!i.isText()) continue;

    IP->SetCurSection(&i);

    uint64_t SectionAddr = i.getAddress();

    // Make a list of all the symbols in this section.
    std::vector<std::pair<uint64_t, StringRef> > Symbols = 
      GetSymbolsList(Obj, i);

    // Make a list of all the relocations for this section.
    std::vector<RelocationRef> Rels;
    if (InlineRelocs) {
      for (auto &ri : i.relocations()) {
        if (error(ec)) break;
        Rels.push_back(ri);
      }
    }

    // Sort relocations by address.
    std::sort(Rels.begin(), Rels.end(), RelocAddressLess);

    StringRef SegmentName = "";
    if (const MachOObjectFile *MachO =
        dyn_cast<const MachOObjectFile>(Obj)) {
      DataRefImpl DR = i.getRawDataRefImpl();
      SegmentName = MachO->getSectionFinalSegmentName(DR);
    }
    StringRef name;
    if (error(i.getName(name))) break;
#ifndef NDEBUG
    outs() << "Disassembly of section ";
    if (!SegmentName.empty())
      outs() << SegmentName << ",";
    outs() << name << ':';
#endif

    // If the section has no symbols just insert a dummy one and disassemble
    // the whole section.
    if (Symbols.empty())
      Symbols.push_back(std::make_pair(0, name));

    StringRef Bytes;
    if (error(i.getContents(Bytes))) break;
    StringRefMemoryObject memoryObject(Bytes);
    uint64_t Size;
    uint64_t Index;
    uint64_t SectSize = i.getSize();

    std::vector<RelocationRef>::const_iterator rel_cur = Rels.begin();
    std::vector<RelocationRef>::const_iterator rel_end = Rels.end();
    // Disassemble symbol by symbol.
    for (unsigned si = 0, se = Symbols.size(); si != se; ++si) {
      uint64_t Start = Symbols[si].first;
      uint64_t End;
      // The end is either the size of the section or the beginning of the next
      // symbol.
      if (si == se - 1)
        End = SectSize;
      // Make sure this symbol takes up space.
      else if (Symbols[si + 1].first != Start)
        End = Symbols[si + 1].first - 1;
      else
        // This symbol has the same address as the next symbol. Skip it.
        continue;

#ifndef NDEBUG
      outs() << '\n' << Symbols[si].second << ":\n";
#endif

#ifndef NDEBUG
        raw_ostream &DebugOut = DebugFlag ? dbgs() : nulls();
#else
        raw_ostream &DebugOut = nulls();
#endif
      uint64_t eoffset = SectionAddr;
      /* Relocatable object */
      if (SectionAddr == 0) 
        eoffset = GetELFOffset(i);

      IP->StartFunction(Twine("a").concat(Twine::utohexstr(Start + eoffset)).str());
      for (Index = Start; Index < End; Index += Size) {
        MCInst Inst;
        
        IP->UpdateCurAddr(Index + eoffset);
        if (DisAsm->getInstruction(Inst, Size, memoryObject, Index,
                                   DebugOut, nulls())) {
#ifndef NDEBUG
          outs() << format("%8" PRIx64 ":", eoffset + Index);
          outs() << "\t";
          DumpBytes(StringRef(Bytes.data() + Index, Size));
#endif
          IP->printInst(&Inst, outs(), "");
#ifndef NDEBUG
          outs() << "\n";
#endif
        } else {
          errs() << ToolName << ": warning: invalid instruction encoding\n";
          DumpBytes(StringRef(Bytes.data() + Index, Size));
          exit(1);
          if (Size == 0)
            Size = 1; // skip illegible bytes
        }

        // Print relocation for instruction.
        while (rel_cur != rel_end) {
          bool hidden = false;
          uint64_t addr;
          SmallString<16> name;
          SmallString<32> val;

          // If this relocation is hidden, skip it.
          if (error(rel_cur->getHidden(hidden))) goto skip_print_rel;
          if (hidden) goto skip_print_rel;

          if (error(rel_cur->getOffset(addr))) goto skip_print_rel;
          // Stop when rel_cur's address is past the current instruction.
          if (addr >= Index + Size) break;
          if (error(rel_cur->getTypeName(name))) goto skip_print_rel;
          if (error(rel_cur->getValueString(val))) goto skip_print_rel;

          outs() << format("\t\t\t%8" PRIx64 ": ", SectionAddr + addr) << name
                 << "\t" << val << "\n";

        skip_print_rel:
          ++rel_cur;
        }
      }
      IP->FinishFunction();
    }    
  }
  IP->FinishModule();
  OptimizeAndWriteBitcode(&*IP);
}

static void PrintRelocations(const ObjectFile *o) {
  std::error_code ec;
  for (auto &si : o->sections()) {                                
    if (error(ec)) return;
    if (si.relocation_begin() == si.relocation_end())
      continue;
    StringRef secname;
    if (error(si.getName(secname))) continue;
    outs() << "RELOCATION RECORDS FOR [" << secname << "]:\n";
    for (auto &ri : si.relocations()){
      if (error(ec)) return;

      bool hidden;
      uint64_t address;
      SmallString<32> relocname;
      SmallString<32> valuestr;
      if (error(ri.getHidden(hidden))) continue;
      if (hidden) continue;
      if (error(ri.getTypeName(relocname))) continue;
      if (error(ri.getOffset(address))) continue;
      if (error(ri.getValueString(valuestr))) continue;
      outs() << address << " " << relocname << " " << valuestr << "\n";
    }
    outs() << "\n";
  }
}

static void PrintSectionHeaders(const ObjectFile *o) {
  outs() << "Sections:\n"
            "Idx Name          Size      Address          Type\n";
  std::error_code ec;
  unsigned i = 0;
  for (auto &si : o->sections()){
    if (error(ec)) return;
    StringRef Name;
    if (error(si.getName(Name))) return;
    uint64_t Address = si.getAddress();
    uint64_t Size = si.getSize();
    bool Text = si.isText();
    bool Data = si.isData();
    bool BSS  = si.isBSS();
    std::string Type = (std::string(Text ? "TEXT " : "") +
                        (Data ? "DATA " : "") + (BSS ? "BSS" : ""));
    outs() << format("%3d %-13s %08" PRIx64 " %016" PRIx64 " %s\n",
                     i, Name.str().c_str(), Size, Address, Type.c_str());
    ++i;
  }
}

static void PrintSectionContents(const ObjectFile *o) {
  std::error_code ec;
  for (auto &si : o->sections()) {
    if (error(ec)) return;
    StringRef Name;
    StringRef Contents;
    uint64_t BaseAddr;
    bool BSS;
    if (error(si.getName(Name))) continue;
    if (error(si.getContents(Contents))) continue;
    BaseAddr = si.getAddress();
    BSS = si.isBSS();

    outs() << "Contents of section " << Name << ":\n";
    if (BSS) {
      outs() << format("<skipping contents of bss section at [%04" PRIx64
                       ", %04" PRIx64 ")>\n", BaseAddr,
                       BaseAddr + Contents.size());
      continue;
    }

    // Dump out the content as hex and printable ascii characters.
    for (std::size_t addr = 0, end = Contents.size(); addr < end; addr += 16) {
      outs() << format(" %04" PRIx64 " ", BaseAddr + addr);
      // Dump line of hex.
      for (std::size_t i = 0; i < 16; ++i) {
        if (i != 0 && i % 4 == 0)
          outs() << ' ';
        if (addr + i < end)
          outs() << hexdigit((Contents[addr + i] >> 4) & 0xF, true)
                 << hexdigit(Contents[addr + i] & 0xF, true);
        else
          outs() << "  ";
      }
      // Print ascii.
      outs() << "  ";
      for (std::size_t i = 0; i < 16 && addr + i < end; ++i) {
        if (std::isprint(static_cast<unsigned char>(Contents[addr + i]) & 0xFF))
          outs() << Contents[addr + i];
        else
          outs() << ".";
      }
      outs() << "\n";
    }
  }
}

static void PrintCOFFSymbolTable(const COFFObjectFile *coff) {
}

static void PrintSymbolTable(const ObjectFile *o) {
  outs() << "SYMBOL TABLE:\n";

  if (const COFFObjectFile *coff = dyn_cast<const COFFObjectFile>(o))
    PrintCOFFSymbolTable(coff);
  else {
    std::error_code ec;
    for (auto &si : o->symbols()) {
      if (error(ec)) return;
      StringRef Name;
      uint64_t Address;
      SymbolRef::Type Type;
      uint64_t Size;
      uint32_t Flags;
      section_iterator Section = o->section_end();
      if (error(si.getName(Name))) continue;
      if (error(si.getAddress(Address))) continue;
      Flags = si.getFlags();
      if (error(si.getType(Type))) continue;
      if (error(si.getSize(Size))) continue;
      if (error(si.getSection(Section))) continue;

      bool Global = Flags & SymbolRef::SF_Global;
      bool Weak = Flags & SymbolRef::SF_Weak;
      bool Absolute = Flags & SymbolRef::SF_Absolute;

      if (Address == UnknownAddressOrSize)
        Address = 0;
      if (Size == UnknownAddressOrSize)
        Size = 0;
      char GlobLoc = ' ';
      if (Type != SymbolRef::ST_Unknown)
        GlobLoc = Global ? 'g' : 'l';
      char Debug = (Type == SymbolRef::ST_Debug || Type == SymbolRef::ST_File)
                   ? 'd' : ' ';
      char FileFunc = ' ';
      if (Type == SymbolRef::ST_File)
        FileFunc = 'f';
      else if (Type == SymbolRef::ST_Function)
        FileFunc = 'F';

      const char *Fmt = o->getBytesInAddress() > 4 ? "%016" PRIx64 :
                                                     "%08" PRIx64;

      outs() << format(Fmt, Address) << " "
             << GlobLoc // Local -> 'l', Global -> 'g', Neither -> ' '
             << (Weak ? 'w' : ' ') // Weak?
             << ' ' // Constructor. Not supported yet.
             << ' ' // Warning. Not supported yet.
             << ' ' // Indirect reference to another symbol.
             << Debug // Debugging (d) or dynamic (D) symbol.
             << FileFunc // Name of function (F), file (f) or object (O).
             << ' ';
      if (Absolute)
        outs() << "*ABS*";
      else if (Section == o->section_end())
        outs() << "*UND*";
      else {
        if (const MachOObjectFile *MachO =
            dyn_cast<const MachOObjectFile>(o)) {
          DataRefImpl DR = Section->getRawDataRefImpl();
          StringRef SegmentName = MachO->getSectionFinalSegmentName(DR);
          outs() << SegmentName << ",";
        }
        StringRef SectionName;
        if (error(Section->getName(SectionName)))
          SectionName = "";
        outs() << SectionName;
      }
      outs() << '\t'
             << format("%08" PRIx64 " ", Size)
             << Name
             << '\n';
    }
  }
}

static void PrintUnwindInfo(const ObjectFile *o) {
  outs() << "Unwind info:\n\n";

  if (const COFFObjectFile *coff = dyn_cast<COFFObjectFile>(o)) {
    //printCOFFUnwindInfo(coff);
    errs() << "This operation is only currently supported "
              "for COFF object files.\n";
  } else {
    // TODO: Extract DWARF dump tool to objdump.
    errs() << "This operation is only currently supported "
              "for COFF object files.\n";
    return;
  }
}

static void DumpObject(const ObjectFile *o) {
  outs() << '\n';
  outs() << o->getFileName()
         << ":\tfile format " << o->getFileFormatName() << "\n\n";

  DisassembleObject(o, false);
  //  if (Relocations && !Disassemble)
  //  PrintRelocations(o);
  //  PrintSectionHeaders(o);
  //  PrintSectionContents(o);
  //  PrintSymbolTable(o);
  //  PrintUnwindInfo(o);
  //if (PrivateHeaders && o->isELF())
  //  printELFFileHeader(o);
}

/// @brief Dump each object file in \a a;
static void DumpArchive(const Archive *a) {
  for (Archive::child_iterator i = a->child_begin(), e = a->child_end(); i != e;
       ++i) {
    ErrorOr<std::unique_ptr<Binary>> ChildOrErr = i->getAsBinary();
    if (std::error_code EC = ChildOrErr.getError()) {
      // Ignore non-object files.
      if (EC != object_error::invalid_file_type)
        errs() << ToolName << ": '" << a->getFileName() << "': " << EC.message()
               << ".\n";
      continue;
    }
    if (ObjectFile *o = dyn_cast<ObjectFile>(&*ChildOrErr.get()))
      DumpObject(o);
    else
      errs() << ToolName << ": '" << a->getFileName() << "': "
              << "Unrecognized file type.\n";
  }
}

/// @brief Open file and figure out how to dump it.
static void DumpInput(StringRef file) {
  // If file isn't stdin, check that it exists.
  if (file != "-" && !sys::fs::exists(file)) {
    errs() << ToolName << ": '" << file << "': " << "No such file\n";
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

  if (Archive *a = dyn_cast<Archive>(&Binary))
    DumpArchive(a);
  else if (ObjectFile *o = dyn_cast<ObjectFile>(&Binary))
    DumpObject(o);
  else
    errs() << ToolName << ": '" << file << "': " << "Unrecognized file type.\n";
}

int main(int argc, char **argv) {
  // Print a stack trace if we signal out.
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);
  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.

  // Initialize targets and assembly printers/parsers.
  //
  // No need to initialize the OpenISA target. It is initialized by a global
  // constructor.

  // Register the target printer for --version.
  cl::AddExtraVersionPrinter(TargetRegistry::printRegisteredTargetsForVersion);

  cl::ParseCommandLineOptions(argc, argv, "Open-ISA Static Binary Translator\n");
  TripleName = Triple::normalize(TripleName);

  ToolName = argv[0];

  // Defaults to a.out if no filenames specified.
  if (InputFilenames.size() == 0)
    InputFilenames.push_back("a.out");

  std::for_each(InputFilenames.begin(), InputFilenames.end(),
                DumpInput);

  return 0;
}
