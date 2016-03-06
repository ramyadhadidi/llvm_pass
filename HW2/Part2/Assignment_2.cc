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
  /*
   *
   */
  struct basicBlockData{
    set<Value*> genValues;    //Used Values
    set<Value*> outValues;

    //
    void printBasicBlockData() {
      errs() << "\tGen Values:\n";
      for (set<Value*>::iterator it=genValues.begin(); it!=genValues.end(); ++it)
        errs() << "\t" << ((*it)->getName()).str() << "\t";
      errs() << "\n\tOut Values:\n";
      for (set<Value*>::iterator it=outValues.begin(); it!=outValues.end(); ++it)
        errs() << "\t" << ((*it)->getName()).str() << "\t";
      errs() << "\n--------------------\n";
    }
  };


  /*========================================================*/
  struct reachDef: public FunctionPass {
    static char ID;
    reachDef() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {

      // Create reaching definitions
      map<BasicBlock*, basicBlockData*> bbMap;
      createReachingDefinition(F ,bbMap);
      
      // Debug Print
      for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) {
        errs() << "BB " << bBlock->getName() << ":\n";
        bbMap[&*bBlock]->printBasicBlockData();
      }



      return false;
    }

    void createReachingDefinition(Function &F, map<BasicBlock*, basicBlockData*> &bbMap) {
      // Create individual gen and out set
      for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) {
        bbMap[&*bBlock] = new basicBlockData;
        for (BasicBlock::iterator iInst = bBlock->begin(); iInst != bBlock->end(); iInst++) 
          if (StoreInst *inst = dyn_cast<StoreInst>(iInst)) {
            string operandName =  ((inst->getPointerOperand())->getName()).str();
            (bbMap[&*bBlock]->genValues).insert(inst->getPointerOperand());
          }
          // There is no kill set, so out=gen
          bbMap[&*bBlock]->outValues = bbMap[&*bBlock]->genValues;
      }

      // Walk of CFG and propagate definitions
      bool change = false;
      do {
        change = false;
        for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) 
          for (pred_iterator predIt = pred_begin(&*bBlock); predIt != pred_end(&*bBlock); predIt++)
            for (set<Value*>::iterator it=(bbMap[*predIt]->outValues).begin(); it!=(bbMap[*predIt]->outValues).end(); ++it) 
              if ((bbMap[&*bBlock]->outValues).find(*it)==(bbMap[&*bBlock]->outValues).end()) {
                (bbMap[&*bBlock]->outValues).insert(*it);
                change = true;
              }
      } while(change);
      
    }


    /*========================================================*/
    // Print functions
    /*
     *
     */
    unsigned getLine(BasicBlock::iterator iInst) {
      if (DILocation *Loc = iInst->getDebugLoc())
        return Loc->getLine();
      else 
        return 0;
    }

    unsigned getLine(Instruction* iInst) {
      if (DILocation *Loc = iInst->getDebugLoc())
        return Loc->getLine();
      else 
        return 0;
    }

    /*
     *
     */
    string getFilename(BasicBlock::iterator iInst) {
      if (DILocation *Loc = iInst->getDebugLoc())
        return Loc->getFilename();
      else 
        return "";
    }

    /*
     *
     */
    string getFunctionname(BasicBlock::iterator iInst) {
      Function* func = iInst->getFunction();
      string name = (func->getName()).str();
      return name;
    }

    string getFunctionname(Instruction* iInst) {
      Function* func = iInst->getFunction();
      string name = (func->getName()).str();
      return name;
    }



  };
}

char reachDef::ID = 0;
static RegisterPass<reachDef> X("Assignment_2", "Mr.Compiler Reaching Definition");
