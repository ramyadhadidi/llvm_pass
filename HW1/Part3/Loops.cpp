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
 * Count BB
 */
namespace {
  struct statLoops {
    int numLoops;
    int numOuterLoops;
    int numExitCFG;

    statLoops() {
      numLoops = 0;
      numOuterLoops = 0;
      numExitCFG = 0;
    }

    void printStat() {
      errs() << "Loops:\n";
      errs() << "#Loops: " << numLoops << "\n";
      errs() << "#Outermost Loops: " << numOuterLoops << "\n";
      errs() << "#Exit CFG: " << numExitCFG << "\n";
      errs() << "\n";
    }

    ~statLoops(){
      printStat();
    }
  };
  struct Loops: public FunctionPass {
    static char ID;
    Loops() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      statLoops stats;
      int numLoopsFunc = 0;
      int numOuterLoopsFunc = 0;
      int numExitCFGFunc = 0;

      LoopInfo &loopInfo = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();

      //#loops by loop header, and outermost loops has depth of 1
      for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) {
        if (loopInfo.isLoopHeader(&*bBlock)) {
          numLoopsFunc++;
          if (loopInfo.getLoopDepth(&*bBlock) == 1)
            numOuterLoopsFunc++;
        }

        // Exit CFG from loop to non-in-loop BB
        if (loopInfo.getLoopDepth(&*bBlock) != 0) {// This BB is in a loop
          Loop* currLoop = loopInfo.getLoopFor(&*bBlock);
          for (succ_iterator child = succ_begin(&*bBlock); child != succ_end(&*bBlock); ++child) { // go Through all successor of this BB
            Loop* exitLoop = loopInfo.getLoopFor(*child);
            if (currLoop != exitLoop) // if they are not in same loop and child depth is larger than this BB, then there is a exit CFG that is not inside BB's loop
              if (loopInfo.getLoopDepth(&*bBlock) < loopInfo.getLoopDepth(*child))
                numExitCFGFunc++;

          }
        }
      }

      stats.numLoops += numLoopsFunc;
      stats.numOuterLoops += numOuterLoopsFunc;
      stats.numExitCFG += numExitCFGFunc;

    return false;
    }

    void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<LoopInfoWrapperPass>();
    }


  };
}

char Loops::ID = 0;
static RegisterPass<Loops> X("Loops", "Loop Info");
