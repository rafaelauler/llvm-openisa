set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  DebugInfo
  MC
  MCDisassembler
  Object
  Support
  )

include_directories(../../../obj/lib/Target/Mips)

add_llvm_tool(oii
  interpreter.cpp
  InterpUtils.cpp
  OiMachineModel.cpp
  OiMemoryModel.cpp
  StringRefMemoryObject.cpp
  SyscallWrapper.cpp
  )
