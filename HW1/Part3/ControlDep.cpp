// Ramyad Hadidi
// HW1
// Sp15

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/CFG.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/PostDominators.h"

#include <limits>

using namespace llvm;

/**
 * Control Dependent 
 * J is control dependent on I, if"
 *  1) J is not a Post-Dominator of I
 *  2) J Post-Dominate one of the successors of I, but not all of them (if J==succ(I) dominate() will return true as well)
 */
namespace {
  struct controlDep: public FunctionPass {
    static char ID;
    controlDep() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      PostDominatorTree &PDT = getAnalysis<PostDominatorTree>();

      for (Function::iterator ctrDepBlockIt = F.begin(); ctrDepBlockIt != F.end(); ctrDepBlockIt++)
        for (Function::iterator DepDomBlockIt = F.begin(); DepDomBlockIt != F.end(); DepDomBlockIt++)


    return false;
    }

    void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<PostDominatorTree>();
    }

  };
}

char controlDep::ID = 0;
static RegisterPass<controlDep> X("ControlDep", "Control Dependence check");
