set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Object
  Support
  )

add_llvm_tool(sbtpass2
  sbtpass2.cpp
  SBTUtils.cpp
  )
