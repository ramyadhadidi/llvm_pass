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
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/Constants.h"


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
    set<Value*> genValues;
    set<Value*> outValues;
    map<Value*, set<uint64_t> > constantMap;

    //
    void printBasicBlockData() {
      errs() << "\tGen Values:\n\t\t";
      for (set<Value*>::iterator it=genValues.begin(); it!=genValues.end(); ++it)
        errs() << ((*it)->getName()).str() << ", ";
      errs() << "\n\tOut Values:\n\t\t";
      for (set<Value*>::iterator it=outValues.begin(); it!=outValues.end(); ++it)
        errs() << ((*it)->getName()).str() << ", ";
      errs() << "\n\tConstantMap Values:\n";
      for (map<Value*, set<uint64_t> >::iterator it=constantMap.begin(); it!=constantMap.end(); ++it) {
        errs() << "\t\t" << (it->first)->getName().str() << ":: ";
        for (set<uint64_t>::iterator itSet=(it->second).begin(); itSet!=(it->second).end(); ++itSet)
          errs() << (*itSet) << ", ";
        errs() << "\n";
      } 

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
      createReachingDefinitionWithConstants(F ,bbMap);
      
      // Debug Print
      for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) {
        errs() << "BB " << bBlock->getName() << ":\n";
        bbMap[&*bBlock]->printBasicBlockData();
      }

      for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) 
        for (BasicBlock::iterator iInst = bBlock->begin(); iInst != bBlock->end(); iInst++)
          if (StoreInst *inst = dyn_cast<StoreInst>(iInst)) {
            string operandName = ((inst->getPointerOperand())->getName()).str();
            errs() << "L" << getLine(iInst) << "::" << iInst->getOpcodeName() << " " << operandName << "\n";

            Value *v = inst->getOperand(0);
            if (ConstantInt* CI = dyn_cast<ConstantInt>(v)) {
              errs() << "Yay\n";
              errs() << CI->getLimitedValue() << "\n";
            }

            //ReplaceInstWithInst(inst->getParent()->getInstList(), iInst, new LoadInst((inst->getPointerOperand()), 0, "ptrToReplacedInt"));
          }



      return true;
    }

    void createReachingDefinitionWithConstants(Function &F, map<BasicBlock*, basicBlockData*> &bbMap) {
      // Create individual gen and out set
      for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) {
        bbMap[&*bBlock] = new basicBlockData;
        for (BasicBlock::iterator iInst = bBlock->begin(); iInst != bBlock->end(); iInst++) 
          if (StoreInst *inst = dyn_cast<StoreInst>(iInst)) {
            string operandName =  ((inst->getPointerOperand())->getName()).str();
            (bbMap[&*bBlock]->genValues).insert(inst->getPointerOperand());

            // update constant map info if storing a constant value
            Value *v = inst->getOperand(0);
            if (ConstantInt* ConstInt = dyn_cast<ConstantInt>(v)) {
              (bbMap[&*bBlock]->constantMap)[inst->getPointerOperand()].insert(ConstInt->getLimitedValue());
            }
          }
          // There is no kill set, so out=gen
          bbMap[&*bBlock]->outValues = bbMap[&*bBlock]->genValues;
      }

      // Walk through CFG and propagate definitions
      bool change = false;
      do {
        change = false;
        for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) 
          for (pred_iterator predIt = pred_begin(&*bBlock); predIt != pred_end(&*bBlock); predIt++) {
            // Copy out values from predecessors to current block
            for (set<Value*>::iterator it=(bbMap[*predIt]->outValues).begin(); it!=(bbMap[*predIt]->outValues).end(); ++it) 
              if ((bbMap[&*bBlock]->outValues).find(*it)==(bbMap[&*bBlock]->outValues).end()) {
                (bbMap[&*bBlock]->outValues).insert(*it);
                change = true;
              }

            // Copy constant map values from predecessors to current block
            for (map<Value*, set<uint64_t> >::iterator it=(bbMap[*predIt]->constantMap).begin(); it!=(bbMap[*predIt]->constantMap).end(); ++it)
              for (set<uint64_t>::iterator itSet=(it->second).begin(); itSet!=(it->second).end(); ++itSet)
                if ((bbMap[&*bBlock]->constantMap)[it->first].find(*itSet) == (bbMap[&*bBlock]->constantMap)[it->first].end()) {
                  (bbMap[&*bBlock]->constantMap)[it->first].insert(*itSet);
                  change = true;
                }

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
