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


namespace {
 
  struct reachDef: public FunctionPass {
    static char ID;
    reachDef() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
int change=0;       
 int count=0;     
do{
 count++;   
change=0;
map<BasicBlock*, map<Value*, set<Value*> >> ValueSet ;
map<BasicBlock*, map<Value*, set<uint64_t> >> constSet;
    
RD(F ,ValueSet,constSet);
     

  errs() << "********************\t iteration:"<<count<<"\t***************\n"; 
for (Function::iterator BB = F.begin(); BB != F.end(); BB++){
/////////////////////////////////////
       
    errs()<< "In BB "<<(*BB).getName()<< ":\n\n";
//   errs() << "ConstantMap Values in iteration "<<count<<" are:\n"; 
for (map<Value*, set<Value*> >::iterator it=ValueSet[BB].begin(); it!=ValueSet[BB].end(); ++it){
        if(it->second.size()==1) {
            
            errs()<<(it->first)->getName().str()<<":\t";
        for (set<Value*>::iterator itSet=(it->second).begin(); itSet!=(it->second).end(); ++itSet)
           if (ConstantInt* CI = dyn_cast<ConstantInt>(*itSet))errs() << " " <<*CI;
        errs() << "\n"; 
        }
        
}     
   errs() << "\n"; 
 /////////////////////////////////////////       
        
        for (BasicBlock::iterator iInst = BB->begin(); iInst != BB->end(); iInst++) {
         
            
          for(unsigned int oprandNum=0; oprandNum<(iInst->getNumOperands()); oprandNum++) {
          
              
              
            Value* operandValue = iInst->getOperand(oprandNum);
            map<Value*, set<Value*> >::iterator itMap = ValueSet[BB].find(operandValue);
            map<Value*, set<uint64_t> >::iterator itMap2 = constSet[BB].find(operandValue);
            if (itMap !=  ValueSet[BB].end()){
                
              if (((itMap->second).size() == 1 )) {
                Value *v = *((itMap->second).begin());
                bool instDelete = false;
               // errs()<< "size: " << (itMap->second).size()<<"\n";
                if (ConstantInt* CI = dyn_cast<ConstantInt>(v)) {
                 //   errs()<< operandValue->getName()<<" subs with "<< dyn_cast<ConstantInt>(v)->getLimitedValue()<<"\n";
                  //Now mapped value has 1 reaching definition, it is constant, and we are not in generating 
                  if(LoadInst *inst = dyn_cast<LoadInst>(iInst)) { //load

                    errs() << "!!Replacement Happened: '" <<*inst << "' replaced with '"  << *v << "' and got deleted\n";
                    iInst->replaceAllUsesWith(v);
                    iInst = iInst->eraseFromParent();
                    instDelete = true;   
                    change=1;
                    break;
                  }
                  else {
                    if (AllocaInst *inst = dyn_cast<AllocaInst>(iInst))
                      break;
                    if (AllocaInst *inst = dyn_cast<AllocaInst>(operandValue))
                      break;
                                        errs() << "!!Replacement Happened: '" <<*inst << "' replaced with '"  << *v << "'\n";
                    operandValue->replaceAllUsesWith(v);
                    change=1;
                  }
                }

                if (!instDelete) {
                  Constant *c = ConstantFoldInstruction(iInst, iInst->getModule()->getDataLayout(),NULL);
                  if(c)
                    errs()<<"Expression evaluates to Const of value : "<<(*c);
                }
                
              
              }
          }
          }



        }
    // printbeforeafter(BB);

}
////////////////////////////////////////
//    for (Function::iterator BB = F.begin(); BB != F.end(); BB++){
//        errs()<< (*BB)<< "\n";
//        errs() << "\n\tConstantMap Values in iteration "<<count<<":\n";
//        for (map<Value*, set<Value*> >::iterator it=ValueSet[BB].begin(); it!=ValueSet[BB].end(); ++it){
//            
//        for (set<Value*>::iterator itSet=(it->second).begin(); itSet!=(it->second).end(); ++itSet)
//          if (ConstantInt* CI = dyn_cast<ConstantInt>(*itSet))errs() << "\n" <<*CI;
//        errs() << "\n"; 
//        }
//            
//    }
//    
     //////////////////////////////////
  

      
    }while(change!=0) ;   
    
//    for (Function::iterator BB = F.begin(); BB != F.end(); BB++){
//     errs()<<"Modified BB  ------------------------------------\n";
//                
//                errs()<< (*BB)<< "\n";
//                errs()<<"BB over ------------------------------------\n";
//    }
     //////////////////////////////////

      return true;
    }
    void printbeforeafter(BasicBlock *b){
     
            
        
    }
    void RD(Function &F, 
    map<BasicBlock*, map<Value*, set<Value*> >> &ValueSet ,
    map<BasicBlock*, map<Value*, set<uint64_t> >> &constSet) {
      // Create individual gen and out set
       for (Function::iterator BB = F.begin(); BB != F.end(); BB++) {
    
        for (BasicBlock::iterator iInst = BB->begin(); iInst != BB->end(); iInst++) 
          if (StoreInst *inst = dyn_cast<StoreInst>(iInst)) {
            string operandName =  ((inst->getPointerOperand())->getName()).str();
            
            // update constant map info if storing a constant value
            Value *v = inst->getOperand(0);
            //errs()<<v->getName()<<"\n";
            //map<Value*, set<uint64_t> > tmp=ValueSet[BB];
            ValueSet[BB][inst->getPointerOperand()].insert(v);
            if (ConstantInt* ConstInt = dyn_cast<ConstantInt>(v)) {
              map<Value*, set<uint64_t> > tmp2=constSet[BB];  
              tmp2[inst->getPointerOperand()].insert(ConstInt->getLimitedValue());
              

            }
          }

      }


      int change = 0;
      do {
        change = 0;
        for (Function::iterator BB = F.begin(); BB != F.end(); BB++) 
          for (pred_iterator predIt = pred_begin(BB); predIt != pred_end(BB); predIt++) {
 
            for (map<Value*, set<uint64_t> >::iterator it=constSet[*predIt].begin(); it!=constSet[*predIt].end(); ++it)
              for (set<uint64_t>::iterator itSet=(it->second).begin(); itSet!=it->second.end(); ++itSet)
                if (constSet[BB][it->first].find(*itSet) == constSet[BB][it->first].end()) {
                  constSet[BB][it->first].insert(*itSet);
                  change = 1;
                }
            for (map<Value*, set<Value*> >::iterator it=ValueSet[*predIt].begin(); it!=ValueSet[*predIt].end(); ++it)
              for (set<Value*>::iterator itSet=(it->second).begin(); itSet!=(it->second).end(); ++itSet)
                if (ValueSet[BB][it->first].find(*itSet) == ValueSet[BB][it->first].end()) {
                  ValueSet[BB][it->first].insert(*itSet);
                  change = 1;
                }
          }
      } while(change!=1);

    }
///my set operations
   void myunion(set<Value*>* out,set<Value*>* in){
          
        for (set<Value*>::iterator it=in->begin(); it!=in->end(); ++it) {
           
            if(out->find(*it)==out->end()){
                out->insert(*it);
                 
            }
                
        }  
      }
     
       void mysubtract(set<Value*>* out,set<Value*>* in){
          
        for (set<Value*>::iterator it=in->begin(); it!=in->end(); ++it) {
            if(out->find(*it)!=out->end() ){
                //errs()<< "\n"<<(*it)->getName()<<" ";
                out->erase(*it);
                
                
            }
                
        }  
      }
 


  };
}

char reachDef::ID = 0;
static RegisterPass<reachDef> X("Assignment_2", "Mr.Compiler Reaching Definition");