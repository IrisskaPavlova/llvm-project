set(LLVM_LINK_COMPONENTS
  Support
  )

add_clang_executable(InstrumentFunctionsPlugin
  InstrumentFunctionsPlugin.cpp
  )

target_link_libraries(InstrumentFunctionsPlugin
  clangAST
  clangBasic
  clangFrontend
  clangTooling
  LLVMSupport
  )
