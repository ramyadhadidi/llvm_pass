// Ramyad Hadidi
// HW2
// Sp15

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/CFG.h"
#include "llvm/Pass.h"


#include <limits>
#include <map>
#include <set>
#include <string.h>
#include <sstream>


using namespace llvm;
using namespace std;

/**
 *
 */
namespace {
  struct unsoundDef: public FunctionPass {
    static char ID;
    unsoundDef() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      return false;
    }

  };
}

char unsoundDef::ID = 0;
static RegisterPass<unsoundDef> X("Assignment_4", "Unsoundness Check");
