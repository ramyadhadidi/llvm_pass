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
#include "llvm/Analysis/ConstantFolding.h"


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
    map<Value*, set<Value*> > valueMap;

    //
    void printBasicBlockData() {
      errs() << "\tGen Values:\n\t\t";
      for (set<Value*>::iterator it=genValues.begin(); it!=genValues.end(); ++it)
        errs() << ((*it)->getName()).str() << ", ";
      errs() << "\n\tOut Values:\n\t\t";
      for (set<Value*>::iterator it=outValues.begin(); it!=outValues.end(); ++it)
        errs() << ((*it)->getName()).str() << ", ";
      errs() << "\n\tConstantMap Values:\n";
      for (map<Value*, set<Value*> >::iterator it=valueMap.begin(); it!=valueMap.end(); ++it) {
        errs() << "\t\t" << (it->first)->getName().str() << ":: ";
        for (set<Value*>::iterator itSet=(it->second).begin(); itSet!=(it->second).end(); ++itSet)
          errs() << "\n\t\t\t" <<*(*itSet);
        errs() << "\n";
      } 

      errs() << "-------------------------\n";
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
      
      // Debug Print for BB data
      for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) {
        errs() << "BB Begin:" << bBlock->getName() << ":\n";
        bbMap[&*bBlock]->printBasicBlockData();
      }

      errs() << "----------------------------------------------------------\n";
      errs() << "Before:\n";
      for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++)
        errs() << *bBlock << "\n";

      // Replace Constants
      replaceConstants(F ,bbMap);


      errs() << "----------------------------------------------------------\n";
      errs() << "After:\n";
      for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++)
        errs() << *bBlock << "\n";

      
      /*
      for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) 
        for (BasicBlock::iterator iInst = bBlock->begin(); iInst != bBlock->end(); iInst++)
          for(int oprandNum=0; oprandNum<(iInst->getNumOperands()); oprandNum++)
            if ((bbMap[&*bBlock]->valueMap).find(iInst->getOperand(oprandNum)) !=  (bbMap[&*bBlock]->valueMap).end())
              errs() << *iInst << "\n";
      */

          /*
          if (StoreInst *inst = dyn_cast<StoreInst>(iInst)) {
            string operandName = ((inst->getPointerOperand())->getName()).str();
            errs() << "L" << getLine(iInst) << "::" << iInst->getOpcodeName() << " " << operandName << "\n";

            Value *v = inst->getOperand(0);
            if (ConstantInt* CI = dyn_cast<ConstantInt>(v)) {
              errs() << "Yay\n";
              errs() << CI->getLimitedValue() << "\n";
            }

            //ReplaceInstWithInst(inst->getParent()->getInstList(), iInst, new LoadInst((inst->getPointerOperand()), 0, "ptrToReplacedInt"));
          }*/



      return true;
    }


    void replaceConstants(Function &F, map<BasicBlock*, basicBlockData*> &bbMap) {

      for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) 
        for (BasicBlock::iterator iInst = bBlock->begin(); iInst != bBlock->end(); iInst++) {

          for(unsigned int oprandNum=0; oprandNum<(iInst->getNumOperands()); oprandNum++) {
            Value* operandValue = iInst->getOperand(oprandNum);
            map<Value*, set<Value*> >::iterator itMap = (bbMap[&*bBlock]->valueMap).find(operandValue);
            if (itMap !=  (bbMap[&*bBlock]->valueMap).end())
              if ((itMap->second).size() == 1) {
                Value *v = *((itMap->second).begin());
                bool instDelete = false;

                if (ConstantInt* CI = dyn_cast<ConstantInt>(v)) {
                  //Now mapped value has 1 reaching definition, it is constant, and we are not in generating 
                  if(LoadInst *inst = dyn_cast<LoadInst>(iInst)) { //load
                    errs() << "rep: " << *inst << "   "  << *v << "\n";
                    iInst->replaceAllUsesWith(v);
                    iInst = iInst->eraseFromParent();
                    instDelete = true;          
                    break;
                  }
                  else {
                    if (AllocaInst *inst = dyn_cast<AllocaInst>(iInst))
                      break;
                    if (AllocaInst *inst = dyn_cast<AllocaInst>(operandValue))
                      break;
                    errs() << "rep2: " << *inst << "   "  << *v << "\n";
                    operandValue->replaceAllUsesWith(v);
                  }
                }

                if (!instDelete) {
                  Constant *c = ConstantFoldInstruction(&*iInst, iInst->getModule()->getDataLayout(),NULL);
                  if(c)
                    errs()<<"Expression evaluates to Const of value : "<<(*c);
                }
                
              
              }
          }



        }


        //for (map<Value*, set<Value*> >::iterator it= (bbMap[&*bBlock]->valueMap).begin(); it!=(bbMap[&*bBlock]->valueMap).end(); ++it)


    }

    /*========================================================*/
    /*
     *
     */
    void createReachingDefinitionWithConstants(Function &F, map<BasicBlock*, basicBlockData*> &bbMap) {
      // Create individual gen and out set
      for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) {
        bbMap[&*bBlock] = new basicBlockData;
        for (BasicBlock::iterator iInst = bBlock->begin(); iInst != bBlock->end(); iInst++) 
          if (StoreInst *inst = dyn_cast<StoreInst>(iInst)) {
            string operandName =  ((inst->getPointerOperand())->getName()).str();
            (bbMap[&*bBlock]->genValues).insert(inst->getPointerOperand());
            // update value map, constant and others, so our pass can be safe
            (bbMap[&*bBlock]->valueMap)[inst->getPointerOperand()].insert(inst->getOperand(0));
          }
          // There is no kill set, so out=gen
          bbMap[&*bBlock]->outValues = bbMap[&*bBlock]->genValues;
      }

      // Walk through CFG and propagate definitions
      //  This is iterative because of back-edges
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

            // Copy value map values from predecessors to current block
            for (map<Value*, set<Value*> >::iterator it=(bbMap[*predIt]->valueMap).begin(); it!=(bbMap[*predIt]->valueMap).end(); ++it)
              for (set<Value*>::iterator itSet=(it->second).begin(); itSet!=(it->second).end(); ++itSet)
                if ((bbMap[&*bBlock]->valueMap)[it->first].find(*itSet) == (bbMap[&*bBlock]->valueMap)[it->first].end()) {
                  (bbMap[&*bBlock]->valueMap)[it->first].insert(*itSet);
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
