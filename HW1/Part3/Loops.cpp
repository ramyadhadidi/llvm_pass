// Ramyad Hadidi
// HW1
// Sp15

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/CFG.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include <limits>

using namespace llvm;

/**
 * Count BB
 */
namespace {
  struct Loops: public ModulePass {
    static char ID;
    Loops() : ModulePass(ID) {}

    bool runOnModule(Module &M) override {

    return false;
    }

    void getAnalysisUsage(AnalysisUsage &AU) const {
      //AU.addRequired<LoopInfoWrapperPass>();
    }


  };
}

char Loops::ID = 0;
static RegisterPass<Loops> X("Loops", "Loop Info");
