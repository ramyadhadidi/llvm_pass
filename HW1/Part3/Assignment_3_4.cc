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

    /**
     * Recursive reachable function based on visited BBs and succ_iteration
     */
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
static RegisterPass<Reachable> X("Assignment_3_4", "Reachable Function");