// Ramyad Hadidi
// HW1
// Sp15

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/CFG.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/LoopInfo.h"

#include <limits>
#include <map>
#include <set>

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

/*
Warshall's algortihm is all pair shortest path algorithm (in this case all pair reachability). 
We can discover cycles using that information and in this process we may produce the same loop with different permutations 
(identifying same loop multiple times).
 
a->b->c->a <=> b->c->a->b ... 
 
It will happen with both single & multiple entry loops. 
In fact you can design a clever algorithm which excludes all the redundant permutations and outputs each loop exactly once, 
but it is out of the scope for this course. You are free to handle permutations in any way (as long as it is correct!).
*/

namespace {
  struct warshalLoop: public FunctionPass {
    static char ID;
    warshalLoop() : FunctionPass(ID) {}

    /**
     *
     */
    void printWarshalMatrix(bool** warshal, unsigned int size) {
      #ifdef DEBUG
      for (unsigned int row=0; row<size; row++) {
        for (unsigned int col=0; col<size; col++) {
          errs() << warshal[row][col] << " ";
        }
        errs() << "\n";
      }
      errs() << "\n";
      #endif

    }

    /**
     *
     */
    bool runOnFunction(Function &F) override {
      map<BasicBlock*, int> BBMap;
      unsigned int size;


      // create map
      unsigned int mapID= 0;
      for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) {
        BBMap.insert( pair<BasicBlock*, int>(&*bBlock, mapID) );
        mapID++;
      }

      // check map size
      if (mapID != BBMap.size()) {
        errs() << "ERR: Wrong Map Size\n";
        return false;
      }
      size = BBMap.size();

      // create Warshal matrix and init it
      bool **warshal = new bool*[size];
      for (unsigned int row=0; row<size; row++) {
        warshal[row] = new bool[size];
        for (unsigned int col=0; col<size; col++)
          warshal[row][col] = 0;
      }

      // fill warshal matrix
      for (Function::iterator bBlock = F.begin(); bBlock != F.end(); bBlock++) {
        int row_index = BBMap[&*bBlock];
        for (succ_iterator succIt = succ_begin(&*bBlock); succIt != succ_end(&*bBlock); succIt++) {
          int col_index = BBMap[*succIt];
          warshal[row_index][col_index] = 1;
        }
      }

      DEBUG_PRINT("Warshal R(0):");
      printWarshalMatrix(warshal, size);

      // update warshal
      for (unsigned int row=0; row<size; row++)
        for (unsigned int k1=0; k1<size; k1++)
          if (warshal[row][k1])
            for (unsigned int k2=0; k2<size; k2++)
              if (warshal[k2][row])
                warshal[k2][k1] = 1;

      DEBUG_PRINT("Warshal R(n):");
      printWarshalMatrix(warshal, size);



      set <pair<int, int>> loops;

      // calculate path (i,j) and (j,i)
      for (unsigned int bID=0; bID<size; bID++)
        for (unsigned int k=0; k<size; k++)
        if (warshal[bID][k] && warshal[k][bID])
          if (bID != k) {
            loops.insert(pair<int,int>(bID, k));
            loops.insert(pair<int,int>(k, bID));
            DEBUG_PRINT("(" << bID << ", " << k << ")");
          }

      // we should have 2 of each pair
      if (loops.size() % 2) {
        errs() << "ERR: Pair insertion error\n";
        return false;
      }

      errs() << "Loops(Warshal):\n";
      errs() << "Number of Loops(Warshal): " << loops.size() / 2 << "\n";
      errs() << "Note: LLVM base algorithm is in part 2-3:\n";


    return false;
    }

    void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<LoopInfoWrapperPass>();
    }

  };
}

char warshalLoop::ID = 0;
static RegisterPass<warshalLoop> X("warshalLoop", "WarshalLoop Counter");