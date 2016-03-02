// Ramyad Hadidi
// HW2
// Sp15

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/CFG.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Metadata.h"

#include <set>
#include <string.h>
#include <sstream>


using namespace llvm;
using namespace std;

/**
 *
 */

/**
 *** Verified Guides:
 ** Debug Information in LLVM - need to compile with -g flag
 *
  BasicBlock::iterator iInst
  if (DILocation *Loc = iInst->getDebugLoc()) { // Here I is an LLVM instruction
   unsigned Line = Loc->getLine();
   errs() << Line << "\n";
   StringRef File = Loc->getFilename();
   errs() << File.str() << "\n";
   StringRef Dir = Loc->getDirectory();
   errs() << Dir.str() << "\n";
 }
 *
 *
 ** Printing out opcode Name(class)
 *
  BasicBlock::iterator iInst
  errs() << iInst->getOpcodeName() << "\n";
 *
 */

namespace {
  struct unsoundDef: public FunctionPass {
    static char ID;
    unsoundDef() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      std::set<Value*> allocated;
      std::set<Value*> defined;
      for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) {
        for (BasicBlock::iterator iInst = bBlock->begin(); iInst != bBlock->end(); iInst++) {
          if (AllocaInst *inst = dyn_cast<AllocaInst>(iInst)) {
            errs() << iInst->getOpcodeName() << "\n";
          }
          if (StoreInst *inst = dyn_cast<StoreInst>(iInst)) {
            errs() << iInst->getOpcodeName() << "\n"; 
          }
          if (LoadInst *inst = dyn_cast<LoadInst>(iInst)) {
            errs() << iInst->getOpcodeName() << "\n"; 
          }
        } 
      }

      return false;
    }

  };
}

char unsoundDef::ID = 0;
static RegisterPass<unsoundDef> X("Assignment_4", "Unsoundness Check");
