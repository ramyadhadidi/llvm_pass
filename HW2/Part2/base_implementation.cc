

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/ConstantFolding.h"

#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

#include "llvm/IR/User.h"
#include "llvm/IR/Use.h"
#include "llvm/IR/Constants.h"



#include <list>
#include <map>
#include <iostream>
#include <set>
#include <string>

using namespace std;

using namespace llvm;


namespace {

  class My_const_propagation : public FunctionPass {
    public:
    static char ID;
    My_const_propagation(): FunctionPass(ID) {}


    /* all the constants reaching current basic block are recorded in map being passed as argument*/
    void do_const_prop_on_block(map<Value*,Value*> &cvalue_map, BasicBlock &B)
    {

      /* this is INCORRECT implementation of simple constant propagation, with folding */

      for(auto i=B.begin(); i!=B.end();)
      {
        bool ins_deleted=false;

        for(int j=0;j<i->getNumOperands();j++)
        {
          Value * val = i->getOperand(j);
          if(cvalue_map.count(val) >0)
          {
            if(i->getOpcode()==27) // load -- do not hard code, replace with appropriate call
            {
              errs() << "loadReplace!!!!\n";
              //delete the loads, they are not required now
              i->replaceAllUsesWith(cvalue_map[val]);
              i = i->eraseFromParent();             
              ins_deleted = true;
              break;
            } 
            else
            {
              errs() << "valReplace!!!!\n";
              val->replaceAllUsesWith(cvalue_map[val]);
            }
          }
        }


        if(!ins_deleted)
        {

          Constant *c = ConstantFoldInstruction(&*i, i->getModule()->getDataLayout(),NULL);
          if(c)
            errs()<<"Expression evaluates to Const of value : "<<(*c);
          i++;
        }
      }
    }


    map<Value*,Value*>* get_information(BasicBlock &B)
    {
      map<Value*,Value*> *const_map = new map<Value*,Value*>;

      for(auto i=B.begin(); i!= B.end(); i++)
      {
        if(i->getOpcode()!=28) //store
          continue;
        
        for(int j=0;j<i->getNumOperands();j++)
        {
          Value *v = i->getOperand(j);
          
          if(ConstantInt::classof(v))
          {
            errs() << "foundConst!!!!\n";
            const_map->insert(pair<Value*,Value*> (i->getOperand(j+1), v));
          }
        }
      }

      return const_map;
    }

    bool runOnFunction(Function &F) {

      BasicBlock *bb;
      
      int bb_count=0;
      map<Value*,Value*>* cmap;

      for(auto i=F.begin(); i!=F.end(); i++)
      {
        //errs()<<"Original BB ***********************************\n";
        //errs()<<(*i)<<"\n";
        //errs()<<"Modified BB  ------------------------------------\n";
        if(bb_count>0){ do_const_prop_on_block(*cmap, *i); }
        cmap = get_information(*i);
        bb_count++;
        //errs()<< (*i)<< "\n";
        //errs()<<"BB over ------------------------------------\n";
      }

      return true;

    }

    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.setPreservesAll();
    }
  };

}



char My_const_propagation::ID = 0;
static RegisterPass<My_const_propagation> Z("My_const_propagation", "My analysis pass");

