set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  BitWriter
  DebugInfo
  MC
  MCDisassembler
  Object
  Support
  )

include_directories(../../../obj/lib/Target/Mips)

add_llvm_tool(static-bt
  staticbt.cpp
  SBTUtils.cpp
  OiCombinePass.cpp
  OiInstTranslate.cpp
  OiIREmitter.cpp
  RelocationReader.cpp
  StringRefMemoryObject.cpp
  SyscallsIface.cpp
  )
