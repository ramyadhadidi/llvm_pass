// Ramyad Hadidi
// HW1
// Sp15

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/LoopInfo.h"

#include <limits>

using namespace llvm;

/**
 * Find all single entry loops by checking purely for back edges across functions
 */
namespace {
  struct SingleEntryLoops: public ModulePass {
    static char ID;
    SingleEntryLoops() : ModulePass(ID) {}

    bool runOnModule(Module &M) override {
      int minSingEntryLoopFunc = std::numeric_limits<int>::max();
      int maxSingEntryLoopFunc = 0;
      int totalSingEntryLoop = 0;
      int totalFunc = 0;
      
      for (Module::iterator F = M.begin(); F != M.end(); ++F) {
        if (F->size() != 0) {
          totalFunc++;
          int SingEntryLoopThisFunction = 0;
          LoopInfo &loopInfo = getAnalysis<LoopInfoWrapperPass>(*F).getLoopInfo();
          for (LoopInfo::iterator loopInfoIt = loopInfo.begin(); loopInfoIt != loopInfo.end(); ++loopInfoIt) {
            Loop* L = *loopInfoIt;
            SingEntryLoopThisFunction += L->getNumBackEdges(); 
          }

          totalSingEntryLoop += SingEntryLoopThisFunction;
          if (maxSingEntryLoopFunc < SingEntryLoopThisFunction)
            maxSingEntryLoopFunc = SingEntryLoopThisFunction;
          if (minSingEntryLoopFunc > SingEntryLoopThisFunction)
            minSingEntryLoopFunc = SingEntryLoopThisFunction;
        }
      }

      float avgSingleEntryLoopinFunc = totalSingEntryLoop / (float)(totalFunc);
      
      errs() << "Single Entry Loops:\n";
      errs() << "Min #SingleEntryLoops across functions: " << minSingEntryLoopFunc << "\n";
      errs() << "Max #SingleEntryLoops across functions: " << maxSingEntryLoopFunc << "\n";
      errs() << "Avg #SingleEntryLoops across functions: " << avgSingleEntryLoopinFunc << "\n";
      errs() << "All #SingleEntryLoops across functions: " << totalSingEntryLoop << "\n";
      errs() << "All #functions: " << totalFunc << "\n";
      errs() << "\n";

      return false;
    }


    void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<LoopInfoWrapperPass>();
    }

  };
}

char SingleEntryLoops::ID = 0;
static RegisterPass<SingleEntryLoops> X("Assignment_2_3", "SingleEntryLoops Counter");
