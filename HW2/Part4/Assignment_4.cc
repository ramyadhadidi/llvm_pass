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
#include <map>
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
 *
 ** Getting Name of Variable in a Instruction
 *
  (inst->getPointerOperand())->getName()).str()
 *
 *
 ** Worklist processing
 *
  set<Instruction*> WorkList;
  for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++)
    for (BasicBlock::iterator iInst = bBlock->begin(); iInst != bBlock->end(); iInst++) {
      WorkList.insert(&*iInst);
    }

  while (!WorkList.empty()) {
    Instruction *iInst = *WorkList.begin();
    WorkList.erase(WorkList.begin());
  }
 *
 *
 ** Users
 *
  for (Value::user_iterator UI = inst->user_begin(), UE = inst->user_end(); UI != UE; UI++) {
    errs() << "1\n";
    if (Instruction *I2 = dyn_cast<Instruction>(*UI)) {
      string operandName = ((I2->getOperand(1))->getName()).str();
      errs() << "AL" << getLine(I2) << "::" << I2->getOpcodeName() << " " << operandName << "\n";
    }

  }
 *
 */

namespace {
  /*
   *
   */
  struct basicBlockDataUninit{
    set<Value*> killValues;   //Defined Values
    set<Value*> genValues;    //Used Values
    set<Value*> inValues;
    set<Value*> outValues;

    //
    void printBasicBlockData() {
      errs() << "Kill Values:\n";
      for (set<Value*>::iterator it=killValues.begin(); it!=killValues.end(); ++it)
        errs() << ((*it)->getName()).str() << "\t";
      errs() << "\nGen Values:\n";
      for (set<Value*>::iterator it=genValues.begin(); it!=genValues.end(); ++it)
        errs() << ((*it)->getName()).str() << "\t";
      errs() << "\nIn Values:\n";
      for (set<Value*>::iterator it=inValues.begin(); it!=inValues.end(); ++it)
        errs() << ((*it)->getName()).str() << "\t";
      errs() << "\nout Values:\n";
      for (set<Value*>::iterator it=outValues.begin(); it!=outValues.end(); ++it)
        errs() << ((*it)->getName()).str() << "\t";
      errs() << "\n\n";
    }
  };


  struct basicBlockDataUnsound{
  set<Value*> killValues;   //Defined Values
  set<Value*> genValues;    //Values that are using undefined variables
  set<Value*> inValues;
  set<Value*> outValues;

    //
    void printBasicBlockData() {
      errs() << "Kill Values:\n";
      for (set<Value*>::iterator it=killValues.begin(); it!=killValues.end(); ++it)
        errs() << ((*it)->getName()).str() << "\t";
      errs() << "\nGen Values:\n";
      for (set<Value*>::iterator it=genValues.begin(); it!=genValues.end(); ++it)
        errs() << ((*it)->getName()).str() << "\t";
      errs() << "\nIn Values:\n";
      for (set<Value*>::iterator it=inValues.begin(); it!=inValues.end(); ++it)
        errs() << ((*it)->getName()).str() << "\t";
      errs() << "\nout Values:\n";
      for (set<Value*>::iterator it=outValues.begin(); it!=outValues.end(); ++it)
        errs() << ((*it)->getName()).str() << "\t";
      errs() << "\n\n";
    }
  };


  /*========================================================*/
  struct unsoundDef: public FunctionPass {
    static char ID;
    // Naive Implementation
    set<string> unInitVarStrNaive;
    set<string> unUnsoundVarStrNaive;
    set<unsigned> unInitVarLineNaive;



    unsoundDef() : FunctionPass(ID) {}

    /*
     *
     */
    bool runOnFunction(Function &F) override {
      //unInitializedVarNaive(F);
      //unSoundVarNaive(F);


      // --------------------------------------------------------------------------------------------------------//
      // ** Uninitialized Variables **
      map <BasicBlock*, set<Value*> > unInitVariablesPrint;
      set <Value*> unInitVariables;
      // Initialize gen and kill sets
      //  gen: the values that are used
      //  kill: the values that are killed
      //  Backward pass
      map<BasicBlock*, basicBlockDataUninit*> bbMapInit;
      // Update basicBlockDataUninit for across block processing
      for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) {
        bbMapInit[&*bBlock] = new basicBlockDataUninit;
        for (BasicBlock::iterator iInst = bBlock->begin(); iInst != bBlock->end(); iInst++) {

          if (StoreInst *inst = dyn_cast<StoreInst>(iInst)) {
            string operandName =  ((inst->getPointerOperand())->getName()).str();
            (bbMapInit[&*bBlock]->killValues).insert(inst->getPointerOperand());
            //FIXME: Here we must process self store and remove them from killSet 
          }

          if (LoadInst *inst = dyn_cast<LoadInst>(iInst)) {
            string operandName = ((inst->getPointerOperand())->getName()).str();
            (bbMapInit[&*bBlock]->genValues).insert(inst->getPointerOperand());
          }

        }
      }

      /*
      // Debug Print
      for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) {
        errs() << "A-BB " << bBlock->getName() << ":\n";
        bbMapInit[&*bBlock]->printBasicBlockData();
      }
      */


      // Process of uninitialized variables
      //  Based on written backward transfer function in homework documents
      //  Note: This is not a optimized version
      int change = 1;
      do {
        for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) {
          for (set<Value*>::iterator it=(bbMapInit[&*bBlock]->outValues).begin(); it!=(bbMapInit[&*bBlock]->outValues).end(); ++it)
            if ((bbMapInit[&*bBlock]->killValues).find(*it) == (bbMapInit[&*bBlock]->killValues).end())
              if ((bbMapInit[&*bBlock]->inValues).find(*it) == (bbMapInit[&*bBlock]->inValues).end()) {
                (bbMapInit[&*bBlock]->inValues).insert(*it);
                change = 2;
              }
          for (succ_iterator succIt = succ_begin(&*bBlock); succIt != succ_end(&*bBlock); succIt++) {
            for (set<Value*>::iterator it=(bbMapInit[*succIt]->inValues).begin(); it!=(bbMapInit[*succIt]->inValues).end(); ++it)
              if ((bbMapInit[&*bBlock]->killValues).find(*it) == (bbMapInit[&*bBlock]->killValues).end())
                if ((bbMapInit[&*bBlock]->outValues).find(*it) == (bbMapInit[&*bBlock]->outValues).end()) {
                  (bbMapInit[&*bBlock]->outValues).insert(*it);
                  change = 2;
                }
          }
          /*
          for (succ_iterator succIt = succ_begin(&*bBlock); succIt != succ_end(&*bBlock); succIt++) {
            for (set<Value*>::iterator it=(bbMapInit[*succIt]->inValues).begin(); it!=(bbMapInit[*succIt]->inValues).end(); ++it)
              if ((bbMapInit[&*bBlock]->outValues).find(*it) == (bbMapInit[&*bBlock]->killValues).end()) {
                (bbMapInit[*succIt]->inValues).erase(it);
                change = 1;
              }
          }
          */
          for (set<Value*>::iterator it=(bbMapInit[&*bBlock]->genValues).begin(); it!=(bbMapInit[&*bBlock]->genValues).end(); ++it)
            if ((bbMapInit[&*bBlock]->killValues).find(*it) == (bbMapInit[&*bBlock]->killValues).end())
              if ((bbMapInit[&*bBlock]->outValues).find(*it) == (bbMapInit[&*bBlock]->outValues).end()) {
                (bbMapInit[&*bBlock]->outValues).insert(*it);
                change = 2;
              }
        }

      } while(change--);


      // Print unInitVariables variables
      for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) {
        set<Value*> &bBlockDataInValue = bbMapInit[&*bBlock]->inValues;
        set<Value*> &bBlockDataGenValue = bbMapInit[&*bBlock]->genValues;
        set<Value*> &bBlockDataInValueEntry = bbMapInit[&(F.getEntryBlock())]->inValues;
        errs() << bBlock->getName() << ":\n";
        for (set<Value*>::iterator it=bBlockDataInValue.begin(); it!=bBlockDataInValue.end(); ++it)
          //Entry block inValue is all correct
          if (bBlockDataInValueEntry.find(*it) != bBlockDataInValueEntry.end() )
            //Print if it is actually used in this basic block
            if (bBlockDataGenValue.find(*it) != bBlockDataGenValue.end() )
            {
              errs() << "WARNING: '" << (*it)->getName() << "' not initialized in Basic Block "<< bBlock->getName() << "\n";
              // Save them for next unsound pass
              unInitVariables.insert(*it);
              // Save them for print
              unInitVariablesPrint[&*bBlock].insert(*it);
            }
        errs() << "\n";
      }      
      // --------------------------------------------------------------------------------------------------------//

      for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) {
        errs() << "A-BB " << bBlock->getName() << ":\n";
        bbMapInit[&*bBlock]->printBasicBlockData();
      }


      // --------------------------------------------------------------------------------------------------------//
      // ** Unsound Variables **
      // Initialize gen and kill sets
      //  gen: the values that are using uninit and unsound variables
      //  kill: the values that are killed
      map<BasicBlock*, basicBlockDataUnsound*> bbMapSound;

      for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++)
        bbMapSound[&*bBlock] = new basicBlockDataUnsound;

      change = 1;
      do {

        // Create gen and kill set
        for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) {
          set<Value*> &bBlockDataGenValue = bbMapSound[&*bBlock]->genValues;
          set<Value*> &bBlockDataKillValue = bbMapSound[&*bBlock]->killValues;
          set<Value*> &bBlockDataOutValue = bbMapSound[&*bBlock]->outValues;
          set<Value*> &bBlockDataUnInitValue = unInitVariables;
          for (BasicBlock::iterator iInst = bBlock->begin(); iInst != bBlock->end(); iInst++) {
            //Check loads to create Gen set
            if (LoadInst *inst = dyn_cast<LoadInst>(iInst))
              // If loading from uninit value
              if ((bBlockDataUnInitValue.find(inst->getPointerOperand()) != bBlockDataUnInitValue.end()) || \
                   (bBlockDataOutValue.find(inst->getPointerOperand()) != bBlockDataOutValue.end() ) )
                for (Value::user_iterator ui = inst->user_begin(); ui != inst->user_end(); ui++) {
                  //Single assignment a=b
                  if (StoreInst *instStore = dyn_cast<StoreInst>(*ui))
                    if ((instStore->getParent()) == (&*bBlock))
                      if (bBlockDataGenValue.find(instStore->getPointerOperand()) == bBlockDataGenValue.end()) {
                        bBlockDataGenValue.insert(instStore->getPointerOperand());
                      }
                  //self assignment and computational a=a+1 or a+a+b
                  if (Instruction *instChain = dyn_cast<Instruction>(*ui))
                    for (Value::user_iterator ui = instChain->user_begin(); ui != instChain->user_end(); ui++)
                      if (StoreInst *instStore = dyn_cast<StoreInst>(*ui))
                        if ((instStore->getParent()) == (&*bBlock))
                          if (bBlockDataGenValue.find(instStore->getPointerOperand()) == bBlockDataGenValue.end()) {
                            bBlockDataGenValue.insert(instStore->getPointerOperand());
                          }    
                }                      
            //Now check stores for other
            if (StoreInst *inst = dyn_cast<StoreInst>(iInst))
              if (bBlockDataGenValue.find(inst->getPointerOperand()) == bBlockDataGenValue.end())
                if (bBlockDataKillValue.find(inst->getPointerOperand()) == bBlockDataKillValue.end())
                  bBlockDataKillValue.insert(inst->getPointerOperand());
          }
        }



        // Now lets propagate
        // Based on forward pass in homework document
        for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) {
          set<Value*> &bBlockDataInValue = bbMapSound[&*bBlock]->inValues;
          set<Value*> &bBlockDataOutValue = bbMapSound[&*bBlock]->outValues;
          set<Value*> &bBlockDataGenValue = bbMapSound[&*bBlock]->genValues;
          set<Value*> &bBlockDataKillValue = bbMapSound[&*bBlock]->killValues;
          for (pred_iterator predIt = pred_begin(&*bBlock); predIt != pred_end(&*bBlock); predIt++)
            for (set<Value*>::iterator it=(bbMapSound[*predIt]->outValues).begin(); it!=(bbMapSound[*predIt]->outValues).end(); ++it)
              if (bBlockDataInValue.find(*it) == bBlockDataInValue.end()) {
                bBlockDataInValue.insert(*it);
                change = 2;
              }

          for (set<Value*>::iterator it=bBlockDataInValue.begin(); it!=bBlockDataInValue.end(); ++it)
            if(bBlockDataKillValue.find(*it) == bBlockDataKillValue.end())
              if (bBlockDataOutValue.find(*it) == bBlockDataOutValue.end()) {
                bBlockDataOutValue.insert(*it);
                change = 2;
              }

          for (set<Value*>::iterator it=bBlockDataGenValue.begin(); it!=bBlockDataGenValue.end(); ++it)
            if (bBlockDataOutValue.find(*it) == bBlockDataOutValue.end()) {
              bBlockDataOutValue.insert(*it);
              change = 2;
            }

        }

      } while(change--);



      // Print unSound variables
      for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) {
        set<Value*> &bBlockDataOutValue = bbMapSound[&*bBlock]->outValues;
        errs() << bBlock->getName() << ":\n";
        for (set<Value*>::iterator it=bBlockDataOutValue.begin(); it!=bBlockDataOutValue.end(); ++it)
              errs() << "WARNING: '" << (*it)->getName() << "' unsound in Basic Block "<< bBlock->getName() << "\n";
        errs() << "\n";
      }  


      
      // Debug Print 
      
      for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) {
        errs() << "B-BB " << bBlock->getName() << ":\n";
        bbMapSound[&*bBlock]->printBasicBlockData();
      }
      
      


      return false;

    }

    /*
     * Naive uninitialized variables
     */
    void unInitializedVarNaive(Function &F) {
      std::set<string> defined;
      for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) {
        for (BasicBlock::iterator iInst = bBlock->begin(); iInst != bBlock->end(); iInst++) {

          if (StoreInst *inst = dyn_cast<StoreInst>(iInst)) {
            string operandName =  ((inst->getPointerOperand())->getName()).str();
            errs() << "L" << getLine(iInst) << "::" << iInst->getOpcodeName() << " " << operandName << "\n";
            defined.insert(operandName);
          }
          if (LoadInst *inst = dyn_cast<LoadInst>(iInst)) {
            string operandName = ((inst->getPointerOperand())->getName()).str();
            errs() << "L" << getLine(iInst) << "::" << iInst->getOpcodeName() << " " << operandName << "\n";
            if (defined.find(operandName) == defined.end()) {
              errs() << "WARNING: '" << operandName << "' not initialized. (" << \
                getFilename(iInst) << "::" << getLine(iInst) << ")\n";
              unInitVarStrNaive.insert(operandName);
              unInitVarLineNaive.insert(getLine(iInst));
            }
          }

        }
      }
    }

    /*
     * Naive unsound variables
     */
    void unSoundVarNaive(Function &F) {
      bool change = false;
      do {
        change = false;
        for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++)
          for (BasicBlock::iterator iInst = bBlock->begin(); iInst != bBlock->end(); iInst++) {

            if (unInitVarLineNaive.find(getLine(iInst)) != unInitVarLineNaive.end()) {
              if (StoreInst *inst = dyn_cast<StoreInst>(iInst)) {
                //errs() << "L" << getLine(iInst) << "::" << iInst->getOpcodeName() << " \n";
                string operandName = ((inst->getPointerOperand())->getName()).str();
                if (unInitVarStrNaive.find(operandName) == unInitVarStrNaive.end()) {
                  unInitVarStrNaive.insert(operandName);
                  unUnsoundVarStrNaive.insert(operandName);
                  change = true;
                  errs() << "WARNING: '" << operandName << "' unsound. (" << \
                    getFilename(iInst) << "::" << getLine(iInst) << ")\n";
                }
              }
            }

          }
      } while(change);
    }


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

char unsoundDef::ID = 0;
static RegisterPass<unsoundDef> X("Assignment_4", "Unsoundness Check");
