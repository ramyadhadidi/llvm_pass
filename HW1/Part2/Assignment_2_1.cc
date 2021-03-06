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
  struct BasicBlockCounter: public ModulePass {
    static char ID;
    BasicBlockCounter() : ModulePass(ID) {}

    bool runOnModule(Module &M) override {
      int maxBasicBlocks = 0;
      int minBasicBlocks = std::numeric_limits<int>::max();
      int totalBasicBlocks = 0;
      float avgBasicBlocks = 0;
      int totalFunc = 0;
      
      for (Module::iterator F = M.begin(), E = M.end(); F != E; ++F) {
        if (F->size() != 0) {
          totalFunc++;
          int numBlocks = F->size();
          totalBasicBlocks += numBlocks;
          if (maxBasicBlocks < numBlocks)
            maxBasicBlocks = numBlocks;
          if (minBasicBlocks > numBlocks)
            minBasicBlocks = numBlocks;
        }
      }

      avgBasicBlocks = totalBasicBlocks / (float)(totalFunc);
      errs() << "Basic Blocks:\n";
      errs() << "Min #Blocks across functions: " << minBasicBlocks << "\n";
      errs() << "Max #Blocks across functions: " << maxBasicBlocks << "\n";
      errs() << "Avg #Blocks across functions: " << avgBasicBlocks << "\n";
      errs() << "All #Blocks across functions: " << totalBasicBlocks << "\n";
      errs() << "All #functions: " << totalFunc << "\n";
      errs() << "\n";

    return false;
    }

  };
}

char BasicBlockCounter::ID = 0;
static RegisterPass<BasicBlockCounter> X("Assignment_2_1", "BasicBlockCounter Counter");
