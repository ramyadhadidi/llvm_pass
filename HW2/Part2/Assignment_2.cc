// Ramyad Hadidi
// HW2
// Sp15

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/CFG.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"


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
  struct reachDef: public FunctionPass {
    static char ID;
    reachDef() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
    }

  };
}

char reachDef::ID = 0;
static RegisterPass<reachDef> X("Assignment_2", "Mr.Compiler Reaching Definition");
