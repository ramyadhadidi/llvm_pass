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
 * Find all BB in loops across functions
 * For each BB, see if its part of a particular loop or not
 */
namespace {
  struct BBinLoops: public ModulePass {
    static char ID;
    BBinLoops() : ModulePass(ID) {}

    bool runOnModule(Module &M) {
      int minBBinFunc = std::numeric_limits<int>::max();
      int maxBBinFunc = 0;
      int totalBBinFunc = 0;
      int totalFunc = 0;
      
      for (Module::iterator F = M.begin(); F != M.end(); ++F) {
        if (F->size() != 0) {
          totalFunc++;
          int BBinLoopsThisFunction = 0;
          LoopInfo &loopInfo = getAnalysis<LoopInfoWrapperPass>(*F).getLoopInfo();
          for (Function::iterator bBlock = F->begin(); bBlock != F->end(); ++bBlock) {
            Loop *loop = loopInfo.getLoopFor(&*bBlock);
            if (loop) {
              BBinLoopsThisFunction++;
            }
          }
          totalBBinFunc += BBinLoopsThisFunction;

          if (BBinLoopsThisFunction > maxBBinFunc)
            maxBBinFunc = BBinLoopsThisFunction;

          if (BBinLoopsThisFunction < minBBinFunc)
            minBBinFunc = BBinLoopsThisFunction;

        }
      }

      float avgBBinFunc = totalBBinFunc / (float)(totalFunc);
      
      errs() << "BB in Loops:\n";
      errs() << "Min #BB in loops across functions: " << minBBinFunc << "\n";
      errs() << "Max #BB in loops across functions: " << maxBBinFunc << "\n";
      errs() << "Avg #BB in loops across functions: " << avgBBinFunc << "\n";
      errs() << "All #BB in loops across functions: " << totalBBinFunc << "\n";
      errs() << "All #functions: " << totalFunc << "\n";
      errs() << "\n";
      

    return false;
    }

    void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<LoopInfoWrapperPass>();
    }

  };
}

char BBinLoops::ID = 0;
static RegisterPass<BBinLoops> X("Assignment_2_4", "BBinLoops Counter", false, false);
