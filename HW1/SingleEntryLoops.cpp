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

namespace {
  struct SingleEntryLoops: public ModulePass {
    static char ID;
    SingleEntryLoops() : ModulePass(ID) {}

    bool runOnModule(Module &M) override {
      int maxBasicBlocks = 0;
      int minBasicBlocks = std::numeric_limits<int>::max();
      int totalBasicBlocks = 0;
      float avgBasicBlocks = 0;
      for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
        int numBlocks = F->size();
        totalBasicBlocks += numBlocks;
        if (maxBasicBlocks < numBlocks)
          maxBasicBlocks = numBlocks;
        if (minBasicBlocks > numBlocks)
          minBasicBlocks = numBlocks;
      }
      avgBasicBlocks = totalBasicBlocks / (float)(M.size());
      errs() << "Min #Blocks across functions: " << minBasicBlocks << "\n";
      errs() << "Max #Blocks across functions: " << maxBasicBlocks << "\n";
      errs() << "Avg #Blocks across functions: " << avgBasicBlocks << "\n";

    return false;
    }

  };
}

char SingleEntryLoops::ID = 0;
static RegisterPass<SingleEntryLoops> X("SingleEntryLoops", "SingleEntryLoops Counter");
