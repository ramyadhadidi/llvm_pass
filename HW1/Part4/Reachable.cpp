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
  struct Reachable: public ModulePass {
    static char ID;
    Reachable() : ModulePass(ID) {}

    bool runOnModule(Module &M) override {

    return false;
    }

  };
}

char Reachable::ID = 0;
static RegisterPass<Reachable> X("Reachable", "Reachable Function");
