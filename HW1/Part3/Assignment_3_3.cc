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
#include <map>
#include <set>

using namespace llvm;
using namespace std;

/**
 * Control Dependent 
 * J is control dependent on I, if
 *  1) J is not a Post-Dominator of I
 *  2) J Post-Dominate one of the successors of I, but not all of them (not all of them will be satisfied with condition 1) (
 *    Note: if J==succ(I) dominate() will return true as well)
 */
namespace {
  struct controlDep: public FunctionPass {
    static char ID;
    controlDep() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      map<BasicBlock*, set<BasicBlock*> > controlDepMap;

      PostDominatorTree &PDT = getAnalysis<PostDominatorTree>();

      for (Function::iterator ctrDepBlockIt = F.begin(); ctrDepBlockIt != F.end(); ctrDepBlockIt++)
        for (Function::iterator DepDomBlockIt = F.begin(); DepDomBlockIt != F.end(); DepDomBlockIt++)
          // J is not a Post-Dominator of I
          if (PDT.dominates(&*ctrDepBlockIt, &*DepDomBlockIt) == false)
            for (succ_iterator succIt = succ_begin(&*DepDomBlockIt); succIt != succ_end(&*DepDomBlockIt); succIt++)
              if (PDT.dominates(&*ctrDepBlockIt, *succIt))
                controlDepMap[&*ctrDepBlockIt].insert(*succIt);

    for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) {
      if (controlDepMap[&*bBlock].size() == 0)
        continue;
      errs() << "*)BasicBlock '" << bBlock->getName() << "' is ContDep on:\n\t";
      for (set<BasicBlock*>::iterator itSet = controlDepMap[&*bBlock].begin(); itSet !=controlDepMap[&*bBlock].end(); itSet++)
        errs() << "BasicBlock '" << (*itSet)->getName() << "', ";
      errs() << "\n\n";
    }


    return false;
    }

    void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<PostDominatorTree>();
    }

  };
}

char controlDep::ID = 0;
static RegisterPass<controlDep> X("Assignment_3_3", "Control Dependence check");
