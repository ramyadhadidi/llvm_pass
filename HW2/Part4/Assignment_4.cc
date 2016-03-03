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
  struct basicBlockData{
    BasicBlock* bBlock;
    set<string> definedValues;
  };

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
      //unSoundVar(F);

      // Process in Each Basic Block
      for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) {
        set<string> defined;
        for (BasicBlock::iterator iInst = bBlock->begin(); iInst != bBlock->end(); iInst++) {

          if (StoreInst *inst = dyn_cast<StoreInst>(iInst)) {
            string operandName =  ((inst->getPointerOperand())->getName()).str();
            defined.insert(operandName);
          }

          if (LoadInst *inst = dyn_cast<LoadInst>(iInst)) {
            string operandName = ((inst->getPointerOperand())->getName()).str();
              if (defined.find(operandName) == defined.end()) {
                errs() << "WARNING: '" << operandName << "' not initialized in " << getFunctionname(inst) \
                  <<". (" << getFilename(iInst) << "::" << getLine(iInst) << ")\n";
              }
          }

        }
      }

      //Now Lets Process across Basic Block


      
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
    void unSoundVar(Function &F) {
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
