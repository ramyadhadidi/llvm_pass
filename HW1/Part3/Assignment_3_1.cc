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
#include "llvm/ADT/SmallVector.h"
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
      }

      for (LoopInfo::iterator loopInfoIt = loopInfo.begin(); loopInfoIt != loopInfo.end(); ++loopInfoIt) {
        Loop* L=*loopInfoIt;
  
        SmallVector<std::pair< const BasicBlock*, const BasicBlock*>, 100> exitEdges;
        L->getExitEdges(exitEdges);
        int size=0;
        while(!exitEdges.empty()){
          exitEdges.pop_back ();
          size++;
        }
        numExitCFGFunc += size;
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
static RegisterPass<Loops> X("Assignment_3_1", "Loop Info");
