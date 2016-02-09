add_llvm_loadable_module( LLVM-HW1-P2
  BasicBlockCounter.cpp
  CFGEdges.cpp
  BBinLoops.cpp
  SingleEntryLoops.cpp
  DominatorCounter.cpp

  DEPENDS
  intrinsics_gen
  )
