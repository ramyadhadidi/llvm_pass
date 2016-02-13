// Ramyad Hadidi
// HW1
// Sp15

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/CFG.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include <set>
#include <map>
#include <time.h>

using namespace llvm;
using namespace std;

//#define DEBUG

#ifdef DEBUG
#define DEBUG_PRINT(x) errs() << x << "\n"
#else
#define DEBUG_PRINT(x) do {} while (0);
#endif


/**
 * 
 */
namespace {
  /**
   * 
   */
  struct timeReachStat {
    int numFunction;
    double totalTime;

    timeReachStat() {
      numFunction = 0;
      totalTime = 0;
    }

    void printStat() {
      if (numFunction != 0) {
        errs() << "Reachability Times: Reported for 1M try\n";
        errs() << "Average Time: " << totalTime / (float)(numFunction) << "\n";
        errs() << "Total Time: " << totalTime << "\n";
        errs() << "# Function: " << numFunction << "\n";
        errs() << "\n";
      }
      else 
        errs() << "No Function Detected\n";
    }

    ~timeReachStat(){
      printStat();
    }
  };

  /**
   * 
   */
  struct Reachable: public FunctionPass {
    static char ID;
    timeReachStat* Stat = new timeReachStat;
    Reachable() : FunctionPass(ID) {  
      srand (time(NULL));
    }

    ~Reachable() {
       delete Stat;
    }

    bool runOnFunction(Function &F) override {
      Stat->numFunction++;

      // create a pool of BBs      
      map<int, BasicBlock*> BBMap;
      unsigned int size;

      // create map
      unsigned int mapID= 0;
      for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) {
        BBMap.insert( pair<int, BasicBlock*>(mapID, &*bBlock) );
        mapID++;
      }
      size = BBMap.size();

      // Select two random BBs and run
      clock_t start;
      double elapsedTime;
      start = clock();
      for(int i=0; i<1000000; i++) {
        BasicBlock* src = BBMap[rand() % size];
        BasicBlock* dest = BBMap[rand() % size];

        set<BasicBlock*> visited;
        bool temp = isReachable(src, dest, visited);
        DEBUG_PRINT( temp << " ");
     } 
     DEBUG_PRINT("\n");

     elapsedTime = ( clock() - start ) / (double) CLOCKS_PER_SEC;

     Stat->totalTime += elapsedTime;

    return false;
    }

    bool isReachable(BasicBlock *src, BasicBlock *dest, set<BasicBlock*> &visited) {
      if(src == dest)
        return true;

      for (succ_iterator succIt = succ_begin(src); succIt != succ_end(src); succIt++) {
        if (visited.find(*succIt) != visited.end()) 
          continue;
        visited.insert(*succIt);
        if (*succIt == dest)
          return true;
        if (isReachable(*succIt, dest, visited))
          return true;
      }

      return false;
    }



  };
}


char Reachable::ID = 0;
static RegisterPass<Reachable> X("Reachable", "Reachable Function");

/**

namespace {
    
    struct p34 : public FunctionPass {
        // Pass identification, replacement for typeid
        static char ID;
                int funcnum;
                my_dstats* timestats =new my_dstats;
        p34() : FunctionPass(ID) {srand (time(NULL));
                funcnum=0;
                }

    virtual ~p34(){
            if (funcnum!=0){
        errs() << "\nMax time through all functions:           " << timestats->max;
                errs() << "\nMin time through all functions:           " << timestats->min;
                errs() << "\nAverage time through all functions:       " << timestats->total/funcnum;
        
        
                errs() << "\n";
            }
            else{
                errs()<<"no function to test reachability!\n";
            }
            }
        //Run for each function
        virtual bool runOnFunction(Function &F){
                    funcnum++;
const Function::BasicBlockListType &BBs = F.getBasicBlockList();
Function::iterator src;
Function::iterator dst;
int nBB=0;
Function::iterator bi=F.begin();
int randn;
srand (time(NULL));
Function::iterator be;

for (Function::iterator bi=F.begin(), be = F.end(); bi != be; bi++) {
nBB++;
}
errs()<<F.getName()<<"\t"<<"nbb:\t"<<nBB<<"\n";
    clock_t start;
    double duration;
start = clock();
for(int i=0;i<1000000;i++){
 src=F.begin();
 dst=F.begin();
 
 randn = rand() % nBB ;
// errs()<<"src:\t"<<randn<<"\n";
 for(int j=0;j<randn;j++)src++;
 //srand (time(NULL));
 randn = rand() % nBB ;
 for(int j=0;j<randn;j++)dst++;
//errs()<<"dst:\t"<<randn<<"\n";
//if(src==dst)continue;
 
SmallPtrSet<const BasicBlock*, 1000> Visited;
//if (isReachable(src,dst,Visited))
   // errs()<<src->getName()<<"\treached\t"<<dst->getName()<<"    \n";
//else 
   // errs()<<src->getName()<<"\tdidn't reach\t"<<dst->getName()<<"    \n";
}
duration = ( clock() - start ) / (double) CLOCKS_PER_SEC;
timestats->add(duration);
        }



*/
