// Ramyad Hadidi
// HW1
// Sp15

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/LoopInfo.h"


#include <limits>

using namespace llvm;

/**
 * Count BB
 */
namespace {
  struct DominatorCounter: public ModulePass {
    static char ID;
    DominatorCounter() : ModulePass(ID) {}

    bool runOnModule(Module &M) override {
      int totalFunc = 0;
      int numTotalDominator = 0;
      int numTotalBB = 0;
      int minDominatorBBinFunc = std::numeric_limits<int>::max();
      int maxDominatorBBinFunc = 0;

      for (Module::iterator F = M.begin(); F != M.end(); ++F) {
        if(F->size() != 0) {
          DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>(*F).getDomTree();
          int numDomintorBBinFunc = 0;
          int numBB = 0;
          for (Function::iterator bBlock = F->begin(); bBlock != F->end(); bBlock++) {
            numBB++;
            for (Function::iterator bBlock2 = F->begin(); bBlock2 != F->end(); bBlock2++) {
              if (DT.dominates(&*bBlock, &*bBlock2))
                numDomintorBBinFunc++;
            }
          }

          numTotalDominator += numDomintorBBinFunc;
          numTotalBB += numBB;
          if (numDomintorBBinFunc > maxDominatorBBinFunc)
            maxDominatorBBinFunc = numDomintorBBinFunc;
          if (numDomintorBBinFunc < minDominatorBBinFunc)
            minDominatorBBinFunc = numDomintorBBinFunc;
        }

      }

      float avgDominatorBBinFunc = numTotalDominator / (float)(numTotalBB);
      
      errs() << "Dominator BB Counter:\n";
      errs() << "Min #DominatorBB across functions: " << minDominatorBBinFunc << "\n";
      errs() << "Max #DominatorBB across functions: " << maxDominatorBBinFunc << "\n";
      errs() << "Avg #DominatorBB across functions: " << avgDominatorBBinFunc << "\n";
      errs() << "All #DominatorBB across functions: " << numTotalDominator << "\n";
      errs() << "All #BB: " << numTotalBB << "\n";
      errs() << "\n";

      return false;
    }

    void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<DominatorTreeWrapperPass>();
    }

  };
}

char DominatorCounter::ID = 0;
static RegisterPass<DominatorCounter> X("DominatorCounter", "Dominator Counter");
