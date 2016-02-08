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
  struct CFGEdges: public ModulePass {
    static char ID;
    CFGEdges() : ModulePass(ID) {}

    bool runOnModule(Module &M) override {
      int numTotalCFG = 0;
      int max = 0;
      int min = std::numeric_limits<int>::max();
      for (Module::iterator F = M.begin(); F != M.end(); ++F) {
        int numCFG = 0;
        for (Function::iterator BB = F->begin(); BB != F->end(); ++BB) {
          //Find terminator instruction and count successors
          TerminatorInst* termInst =  static_cast<TerminatorInst*>(BB->getTerminator());
          numCFG += termInst->getNumSuccessors();
        }
        // Do computation for this function
        numTotalCFG += numCFG;
        if (max < numCFG)
          max = numCFG;
        if (min > numCFG)
          min = numCFG;
      }

      double averageCFG = numTotalCFG / (float)(M.size());
      errs() << "Min #CFG across functions: " << min << "\n";
      errs() << "Max #CFG across functions: " << max << "\n";
      errs() << "Avg #CFG across functions: " << averageCFG << "\n";

      return false;
    }

  };
}

char CFGEdges::ID = 0;
static RegisterPass<CFGEdges> X("CFGEdges", "CFG Edges Counter");
