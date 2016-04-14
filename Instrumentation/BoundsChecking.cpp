//===- BoundsChecking.cpp - Instrumentation for run-time bounds checking --===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements a pass that instruments the code to perform run-time
// bounds checking on loads, stores, and other memory intrinsics.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "bounds-checking"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Pass.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Analysis/TargetFolder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Transforms/Instrumentation.h"
#include "llvm/Transforms/Scalar.h"
#include <queue>
#include <list>
#include <set>
using namespace llvm;

#define DEBUG_IDENTIFY 0
#define DEBUG_LOCAL 0
#define DEBUG_SHOW_UNHANDLED_OPS 0
#define DEBUG_GLOBAL 0
#define DEBUG_LOOP 0
#define DEBUG_INSERT 0
#define DEBUG_INSTS 0
#define DEBUG_CODE 0

#include "ConstraintGraph.hpp"
#include "BoundsCheck.hpp"
#include "GlobalAnalysis.hpp"
#include "Monotonic.hpp"

static cl::opt<bool> SingleTrapBB("bounds-checking-single-trap",
                                  cl::desc("Use one trap block per function"));

STATISTIC(ChecksAdded, "Bounds checks added");
STATISTIC(ChecksSkipped, "Bounds checks skipped");
STATISTIC(ChecksUnable, "Bounds checks unable to add");

typedef IRBuilder<true, TargetFolder> BuilderTy;

namespace {
  struct BoundsChecking : public FunctionPass {
    static char ID;

    BoundsChecking() : FunctionPass(ID){
      initializeBoundsCheckingPass(*PassRegistry::getPassRegistry());
    }

    virtual bool runOnFunction(Function &F);

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      //AU.addRequired<DataLayout>();
      AU.addRequired<TargetLibraryInfoWrapperPass>();
      AU.addRequired<LoopInfoWrapperPass>();
      AU.addRequired<DominatorTreeWrapperPass>();
      //AU.addRequiredID(LoopSimplifyID);
    }

  private:
    int numChecksAdded;
    const DataLayout *TD;
    const TargetLibraryInfo *TLI;
    ObjectSizeOffsetEvaluator *ObjSizeEval;
    BuilderTy *Builder;
    Instruction *Inst;
    BasicBlock *TrapBB;

    BasicBlock *getTrapBB();
    void emitBranchToTrap(Value *Cmp = 0);
    bool computeAllocSize(Value *Ptr, APInt &Offset, Value* &OffsetValue,
                          APInt &Size, Value* &SizeValue);
    bool instrument(Value *Ptr, Value *Val);

    // Bounds Checks identifying Functions
    void IdentifyBoundsChecks(BasicBlock *blk, std::vector<BoundsCheck*> *boundsChecks);
    BoundsCheck* createBoundsCheck(Instruction *Inst, Value *Ptr, Value *Val);
    // Local Analysis Functions
    void LocalAnalysis(BasicBlock *blk, std::vector<BoundsCheck*> *boundsChecks, ConstraintGraph *cg);
    void getCheckVariables(std::vector<BoundsCheck*> *boundsChecks, ConstraintGraph *cg);
    void promoteCheck(BoundsCheck* check);
    void promoteLocalChecks(std::vector<BoundsCheck*> *boundsChecks);
    void EliminateBoundsChecks(std::vector<BoundsCheck*> *boundsChecks, ConstraintGraph *cg);
    void eliminateForwards(BoundsCheck* check1, BoundsCheck* check2, ConstraintGraph *cg);
    void eliminateBackwards(BoundsCheck* check1, BoundsCheck* check2, ConstraintGraph *cg);
    void buildConstraintGraph(BasicBlock *blk, ConstraintGraph *cg);
    // Global Analysis Functions
    void GlobalAnalysis(std::vector<BasicBlock*> *worklist, std::map<BasicBlock*,std::vector<BoundsCheck*>*> *blkChecks, std::map<BasicBlock*,ConstraintGraph*> *blkCG);
    // Loop Analysis Functions
    void LoopAnalysis(std::vector<BasicBlock*> *worklist, std::map<BasicBlock*, std::vector<BoundsCheck*>*> *blkChecks, std::map<BasicBlock*,ConstraintGraph*> *blkCG);
    void addLoopIntoQueue(Loop *L, std::list<Loop*> *loopQueue);
    void hoistCheck(BoundsCheck* check);
    bool handleCopyCheck(BoundsCheck *check);
    // Bounds Checks Insertion Functions
    bool InsertCheck(BoundsCheck* check);
    bool InsertChecks(std::vector<BoundsCheck*> *boundsCheck);
 };
}

char BoundsChecking::ID = 0;
INITIALIZE_PASS_BEGIN(BoundsChecking, "bounds-checking", "Run-time bounds checking", false, false)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
//INITIALIZE_PASS_DEPENDENCY(LoopSimplify)
INITIALIZE_PASS_DEPENDENCY(TargetLibraryInfoWrapperPass)
INITIALIZE_PASS_END(BoundsChecking, "bounds-checking", "Run-time bounds checking", false, false)


/// getTrapBB - create a basic block that traps. All overflowing conditions
/// branch to this block. There's only one trap block per function.
BasicBlock *BoundsChecking::getTrapBB() {
  if (TrapBB)
    return TrapBB;

  Function *Fn = Inst->getParent()->getParent();
  BasicBlock::iterator PrevInsertPoint = Builder->GetInsertPoint();
  TrapBB = BasicBlock::Create(Fn->getContext(), "trap", Fn);
  Builder->SetInsertPoint(TrapBB);

  llvm::Value *F = Intrinsic::getDeclaration(Fn->getParent(), Intrinsic::trap);
  CallInst *TrapCall = Builder->CreateCall(F);
  TrapCall->setDoesNotReturn();
  TrapCall->setDoesNotThrow();
  TrapCall->setDebugLoc(Inst->getDebugLoc());
  Builder->CreateUnreachable();

  Builder->SetInsertPoint(&*PrevInsertPoint);
  return TrapBB;
}


/// emitBranchToTrap - emit a branch instruction to a trap block.
/// If Cmp is non-null, perform a jump only if its value evaluates to true.
void BoundsChecking::emitBranchToTrap(Value *Cmp) {
#if DEBUG_LOCAL
  errs() << "Emitting Branch Instruction\n";
#endif
  // check if the comparison is always false
  ConstantInt *C = dyn_cast_or_null<ConstantInt>(Cmp);
  if (C) {
    ++ChecksSkipped;
    if (!C->getZExtValue())
      return;
    else
      Cmp = 0; // unconditional branch
  }

  Instruction *Inst = &*(Builder->GetInsertPoint());
  BasicBlock *OldBB = Inst->getParent();
  BasicBlock *Cont = OldBB->splitBasicBlock(Inst);
  OldBB->getTerminator()->eraseFromParent();
  if (Cmp)
    BranchInst::Create(getTrapBB(), Cont, Cmp, OldBB);
  else
    BranchInst::Create(getTrapBB(), OldBB);
}


/// instrument - adds run-time bounds checks to memory accessing instructions.
/// Ptr is the pointer that will be read/written, and InstVal is either the
/// result from the load or the value being stored. It is used to determine the
/// size of memory block that is touched.
/// Returns true if any change was made to the IR, false otherwise.
bool BoundsChecking::instrument(Value *Ptr, Value *InstVal) {
  uint64_t NeededSize = TD->getTypeStoreSize(InstVal->getType());
  DEBUG(dbgs() << "Instrument " << *Ptr << " for " << Twine(NeededSize)
              << " bytes\n");

  SizeOffsetEvalType SizeOffset = ObjSizeEval->compute(Ptr);

  if (!ObjSizeEval->bothKnown(SizeOffset)) {
    ++ChecksUnable;
    return false;
  }

  Value *Size   = SizeOffset.first;
  Value *Offset = SizeOffset.second;
  ConstantInt *SizeCI = dyn_cast<ConstantInt>(Size);

  Type *IntTy = TD->getIntPtrType(Ptr->getType());
  Value *NeededSizeVal = ConstantInt::get(IntTy, NeededSize);

  /**
  errs() << "===========================\n";
  errs() << "Array: " << Ptr->getName() << "\n";
  errs() << "Index: " << *Offset <<  "\n";
  errs() << " Size : " << *Size << "\n";
  */
  // three checks are required to ensure safety:
  // . Offset >= 0  (since the offset is given from the base ptr)
  // . Size >= Offset  (unsigned)
  // . Size - Offset >= NeededSize  (unsigned)
  //
  // optimization: if Size >= 0 (signed), skip 1st check
  // FIXME: add NSW/NUW here?  -- we dont care if the subtraction overflows
  Value *ObjSize = Builder->CreateSub(Size, Offset);
  Value *Cmp2 = Builder->CreateICmpULT(Size, Offset);
  Value *Cmp3 = Builder->CreateICmpULT(ObjSize, NeededSizeVal);
  Value *Or = Builder->CreateOr(Cmp2, Cmp3);
  if (!SizeCI || SizeCI->getValue().slt(0)) {
    Value *Cmp1 = Builder->CreateICmpSLT(Offset, ConstantInt::get(IntTy, 0));
    Or = Builder->CreateOr(Cmp1, Or);
  }
  emitBranchToTrap(Or);

  ++ChecksAdded;
  return true;
}



void BoundsChecking::buildConstraintGraph(BasicBlock *blk, ConstraintGraph *cg) {

  std::vector<Instruction*> WorkList;
  // Iterate over instructions in the basic block and build the constraint graph
  for (BasicBlock::iterator i = blk->begin(), e = blk->end(); i != e; ++i) {
    Instruction *I = &*i;
  #if DEBUG_LOCAL
    errs() << "===========================================\n";
    if (I->hasName()) {
      errs() << "Instruction Name: " << I->getName() << "\n";
    } else {
      errs() << "Instruction Name: No Name\n";
    }
  #endif
    if (isa<AllocaInst>(I)) {
    #if DEBUG_LOCAL
      errs() << "Allocate Instruction: " << *I << "\n";
    #endif
      cg->addMemoryNode(I);
    } else if (isa<CallInst>(I)) {
    #if DEBUG_LOCAL
      errs() << "Function Call: " << *I << "\n";
    #endif
      // Function call instruction, we must kill all variables
      cg->killMemoryLocations();
      //errs() << "Function Call: " << *I << "\n";
    } else if (I->isCast()) {
    #if DEBUG_LOCAL
      errs() << "Cast Operator: " << *I << "\n";
    #endif
      // If cast, basically set output equal to input
      Value *op2 = I->getOperand(0);
      cg->addCastEdge(op2, I);
      //errs() << "Cast Operator: " << *I << "\n";
      //errs() << "Casting: " << *op1 << "\n";
    } else if (isa<GetElementPtrInst>(I)) {
    #if DEBUG_LOCAL
      errs() << "GEP: " << *I << "\n";
    #endif
      Value *index = I->getOperand(I->getNumOperands()-1);
      cg->addGEPEdge(index, I);
      //errs() << "Get Element Pointer: " << *I << "\n";
      //errs() << "Index: " << *index << "\n";
    } else if (isa<LoadInst>(I)) {
    #if DEBUG_LOCAL
      errs() << "Load Operator: " << *I << "\n";
    #endif
      // If a load, associate register with memory identifier
      LoadInst *LI = dyn_cast<LoadInst>(I);
      Value *op1 = LI->getPointerOperand();
      cg->addLoadEdge(op1, I);
      //errs() << "Loading From: " << *op1 << "\n";
      //errs() << "Loading To: " << *I << "\n";
    } else if (isa<StoreInst>(I)) {
    #if DEBUG_LOCAL
      errs() << "Store Operator: " << *I << "\n";
    #endif
      // If a store instruction, we need to set that memory location to value in graph
      StoreInst *SI = dyn_cast<StoreInst>(I);
      Value *to = SI->getPointerOperand();
      Value *from = SI->getValueOperand();
      Type* T = to->getType();
      bool isPointer = T->isPointerTy() && T->getContainedType(0)->isPointerTy();
      cg->addStoreEdge(from, to, I);
      if (isPointer) {
      #if DEBUG_LOCAL
        errs() << "Storing From Pointer\n";
      #endif
        // If store to location was a pointer, then we must kill all memory locations
        cg->killMemoryLocations();
      }
      /**
      ConstantInt *ConstVal = dyn_cast<ConstantInt>(from);
      if (ConstVal != NULL) {
        errs() << "Storing Value: " << ConstVal->getSExtValue() << "\n";
      } else {
        errs() << "Storing From: " << *from << "\n";
      }

      if (isPointer) {
        errs() << "Storing To Pointer Location: " << *to << "\n";
      } else {
        errs() << "Storing To: " << *to << "\n";
      }
      */
    } else if (I->isBinaryOp()) {
      unsigned opcode = I->getOpcode();
      if (opcode == Instruction::Add) {
          int64_t val = 0;
          Value *var = NULL;
        #if DEBUG_LOCAL
          errs() << "Add Operator: " << *I << "\n";
        #endif
          Value *op1 = I->getOperand(0);
          Value *op2 = I->getOperand(1);

          ConstantInt *ConstVal1 = dyn_cast<ConstantInt>(op1);
          ConstantInt *ConstVal2 = dyn_cast<ConstantInt>(op2);

          if ((ConstVal1 != NULL) && (ConstVal2 !=NULL)) {
            // We know both operands so can just create blank node with value
            val = ConstVal1->getSExtValue() + ConstVal2->getSExtValue();
            cg->addConstantNode(I, val);
          } else if (ConstVal1 != NULL) {
            val = ConstVal1->getSExtValue();
            var = op2;
            cg->addAddEdge(var, I, val);
            //errs() << *var << "\n";
            //errs() << "Constant: " << val << "\n";
          } else if (ConstVal2 != NULL) {
            var = op1;
            val = ConstVal2->getSExtValue();
            cg->addAddEdge(var, I, val);
            //errs() << *var << "\n";
            //errs() << "Constant: " << val << "\n";
          } else {
            // Both operands are variables, so we must just create blank node
            cg->addNode(I);
          }
      } else if (opcode == Instruction::Sub) {
          int64_t val = 0;
          Value *var = NULL;
        #if DEBUG_LOCAL
          errs() << "Subtraction Operator: " << *I << "\n";
        #endif
          Value *op1 = I->getOperand(0);
          Value *op2 = I->getOperand(1);

          ConstantInt *ConstVal1 = dyn_cast<ConstantInt>(op1);
          ConstantInt *ConstVal2 = dyn_cast<ConstantInt>(op2);

          if ((ConstVal1 != NULL) && (ConstVal2 !=NULL)) {
            // We know both operands so can just create blank node with value
            val = ConstVal1->getSExtValue() - ConstVal2->getSExtValue();
            cg->addConstantNode(I, val);
          } else if (ConstVal1 != NULL) {
            // Second operand is variable so we can't determine much about operation
            cg->addNode(I);
          } else if (ConstVal2 != NULL) {
            var = op1;
            val = ConstVal2->getSExtValue();
            cg->addSubEdge(var, I, val);
            //errs() << *var << "\n";
            //errs() << "Constant: " << val << "\n";
          } else {
            // Both operands are variables, so we must just create blank node
            cg->addNode(I);
          }
      } else if (opcode == Instruction::Mul) {
          int64_t val = 0;
          Value *var = NULL;
        #if DEBUG_LOCAL
          errs() << "Multiply Operator: " << *I << "\n";
        #endif
          Value *op1 = I->getOperand(0);
          Value *op2 = I->getOperand(1);

          ConstantInt *ConstVal1 = dyn_cast<ConstantInt>(op1);
          ConstantInt *ConstVal2 = dyn_cast<ConstantInt>(op2);

          if ((ConstVal1 != NULL) && (ConstVal2 !=NULL)) {
            // We know both operands so can just create blank node with value
            val = ConstVal1->getSExtValue()*ConstVal2->getSExtValue();
            cg->addConstantNode(I, val);
          } else if (ConstVal1 != NULL) {
            val = ConstVal1->getSExtValue();
            var = op2;
            cg->addMulEdge(var, I, val);
            //errs() << *var << "\n";
            //errs() << "Constant: " << val << "\n";
          } else if (ConstVal2 != NULL) {
            var = op1;
            val = ConstVal2->getSExtValue();
            cg->addMulEdge(var, I, val);
            //errs() << *var << "\n";
            //errs() << "Constant: " << val << "\n";
          } else {
            // Both operands are variables, so we must just create blank node
            cg->addNode(I);
          }
      } else if (opcode == Instruction::UDiv) {
          int64_t val = 0;
          Value *var = NULL;
        #if DEBUG_LOCAL
          errs() << "Unsigned Division Operator: " << *I << "\n";
        #endif
          Value *op1 = I->getOperand(0);
          Value *op2 = I->getOperand(1);

          ConstantInt *ConstVal1 = dyn_cast<ConstantInt>(op1);
          ConstantInt *ConstVal2 = dyn_cast<ConstantInt>(op2);

          if ((ConstVal1 != NULL) && (ConstVal2 !=NULL)) {
            // We know both operands so can just create blank node with value
            val = (int64_t)(ConstVal1->getZExtValue()/ConstVal2->getZExtValue());
            cg->addConstantNode(I, val);
          } else if (ConstVal1 != NULL) {
            // Second operand is variable so we can't determine much about operation
            cg->addNode(I);
          } else if (ConstVal2 != NULL) {
            var = op1;
            val = (int64_t)ConstVal2->getZExtValue();
            cg->addDivEdge(var, I, val);
            errs() << *var << "\n";
            errs() << "Constant: " << val << "\n";
          } else {
            // Both operands are variables, so we must just create blank node
            cg->addNode(I);
          }
      } else if (opcode == Instruction::SDiv) {
          int64_t val = 0;
          Value *var = NULL;
        #if DEBUG_LOCAL
          errs() << "Signed Division Operator: " << *I << "\n";
        #endif
          Value *op1 = I->getOperand(0);
          Value *op2 = I->getOperand(1);

          ConstantInt *ConstVal1 = dyn_cast<ConstantInt>(op1);
          ConstantInt *ConstVal2 = dyn_cast<ConstantInt>(op2);

          if ((ConstVal1 != NULL) && (ConstVal2 !=NULL)) {
            // We know both operands so can just create blank node with value
            val = ConstVal1->getSExtValue() + ConstVal2->getSExtValue();
            cg->addConstantNode(I, val);
          } else if (ConstVal1 != NULL) {
            // Second operand is variable so we can't determine much about operation
            cg->addNode(I);
          } else if (ConstVal2 != NULL) {
            var = op1;
            val = ConstVal2->getSExtValue();
            if (val > 0) {
              cg->addDivEdge(var, I, val);
              //errs() << *var << "\n";
              //errs() << "Constant: " << val << "\n";
            } else {
              cg->addNode(I);
            }
          } else {
            // Both operands are variables, so we must just create blank node
            cg->addNode(I);
          }
      } else {
        cg->addNode(I);
      #if DEBUG_SHOW_UNHANDLED_OPS
        errs() << "Handle opcode: " << I->getOpcodeName() << "?: " << *I << "\n";
      #endif
      }
    } else {
      cg->addNode(I);
    #if DEBUG_SHOW_UNHANDLED_OPS
      errs() << "Handle opcode: " << I->getOpcodeName() << "?: " << *I << "\n";
    #endif
    }
  }
}

void BoundsChecking::IdentifyBoundsChecks(BasicBlock *blk, std::vector<BoundsCheck*> *boundsChecks) {
  for (BasicBlock::iterator i = blk->begin(), e = blk->end(); i != e; ++i) {
    Instruction *Inst = &*i;
    BoundsCheck *check = NULL;
    if (LoadInst *LI = dyn_cast<LoadInst>(Inst)) {
      check = createBoundsCheck(Inst, LI->getPointerOperand(), LI);
    } else if (StoreInst *SI = dyn_cast<StoreInst>(Inst)) {
      check = createBoundsCheck(Inst, SI->getPointerOperand(), SI->getValueOperand());
    } else if (AtomicCmpXchgInst *AI = dyn_cast<AtomicCmpXchgInst>(Inst)) {
      check = createBoundsCheck(Inst, AI->getPointerOperand(),AI->getCompareOperand());
    } else if (AtomicRMWInst *AI = dyn_cast<AtomicRMWInst>(Inst)) {
      check = createBoundsCheck(Inst, AI->getPointerOperand(), AI->getValOperand());
    }

    if (check != NULL) {
      boundsChecks->push_back(check);
    }
  }
}


/// Ptr is the pointer that will be read/written, and InstVal is either the
/// result from the load or the value being stored. It is used to determine the
/// size of memory block that is touched.
/// Returns true if any change was made to the IR, false otherwise.
BoundsCheck* BoundsChecking::createBoundsCheck(Instruction *Inst, Value *Ptr, Value *InstVal) {
  uint64_t NeededSize = TD->getTypeStoreSize(InstVal->getType());
  SizeOffsetEvalType SizeOffset = ObjSizeEval->compute(Ptr);

  BoundsCheck *check = NULL;
  if (!ObjSizeEval->bothKnown(SizeOffset)) {
    return check;
  }

  Value *Size   = SizeOffset.first;
  Value *Offset = SizeOffset.second;
  ConstantInt *SizeCI = dyn_cast<ConstantInt>(Size);
  ConstantInt *OffsetCI = dyn_cast<ConstantInt>(Offset);

  // three checks are required to ensure safety:
  // . Offset >= 0  (since the offset is given from the base ptr)
  // . Size >= Offset  (unsigned)
  // . Size - Offset >= NeededSize  (unsigned)
  //
  // optimization: if Size >= 0 (signed), skip 1st check
  // FIXME: add NSW/NUW here?  -- we dont care if the subtraction overflows

      //errs() << "Instruction: " << *Inst << "\n";
      //errs() << "Offset: " << *Offset << "\n";
      //errs() << "Size: " << *Size << "\n";
      //errs() << "Needed Size: " << NeededSize << "\n";
  if (SizeCI) {
    if (OffsetCI != NULL) {
      uint64_t size = SizeCI->getZExtValue();
      uint64_t offset = OffsetCI->getZExtValue();

      //errs() << "Constant Size: " << size << "\n";
      //errs() << "Constant Offset: " << offset << "\n";

      if ((size >= offset) && ((size - offset) >= NeededSize)) {
        return check;
      }
    }
  }

  Value* Index = Ptr;
  GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(Ptr);
  if (gep != NULL) {
      Index = gep->getOperand(gep->getNumOperands()-1);
  }

  // Add check to work list
  check = new BoundsCheck(Inst, Ptr, Index, Offset, Size);
  return check;
}


void BoundsChecking::eliminateForwards(BoundsCheck* check1, BoundsCheck* check2,
                                       ConstraintGraph *cg) {
  Value *ub1 = check1->getUpperBound();
  Value *ub2 = check2->getUpperBound();
  Value *index1 = check1->getIndex();
  Value *index2 = check2->getIndex();

  ConstraintGraph::CompareEnum cmp1 = cg->compare(index1, index2);
  if (check1->hasLowerBoundsCheck() && check2->hasUpperBoundsCheck()) {
    // If check1 lower bounds check is valid
    switch (cmp1) {
      case ConstraintGraph::LESS_THAN:
      case ConstraintGraph::EQUALS:
      #if DEBUG_LOCAL
        errs() << "Has Lower Bound Subsuming...\n";
        errs() << "Deleting Lower Bounds Check for " << *index2 << "\n";
      #endif
        // If index1 <= index2, don't need 0 <= index2
        check2->deleteLowerBoundsCheck();
        break;
      #if DEBUG_LOCAL
      case ConstraintGraph::GREATER_THAN:
        break;
      #endif
      default:
      #if DEBUG_LOCAL
        errs() << "Unknown comparison between " << *index1 << " and " << *index2 <<"\n";
      #endif
        // Unknown value for indiciesi
        break;
    }
  }

  if (check1->hasUpperBoundsCheck() && check2->hasUpperBoundsCheck()) {
    ConstraintGraph::CompareEnum cmp2 = ConstraintGraph::UNKNOWN;
    switch (cmp1) {
      #if DEBUG_LOCAL
      case ConstraintGraph::LESS_THAN:
        break;
      #endif
      case ConstraintGraph::EQUALS:
      case ConstraintGraph::GREATER_THAN:
      #if DEBUG_LOCAL
        errs() << "Has Upper Bound Subsuming...\n";
      #endif
        // If check 1 is upper bounds check valid
        cmp2 = cg->compare(ub1, ub2);
        if (cmp2 == ConstraintGraph::LESS_THAN || cmp2 == ConstraintGraph::EQUALS) {
        #if DEBUG_LOCAL
          errs() << "Deleting Upper Bounds Check for " << *index2 << "\n";
        #endif
          // If index1 >= index2, and ub1 <= ub2, don't need index2 <= ub2
          check2->deleteUpperBoundsCheck();
        }
        #if DEBUG_LOCAL
         else if (cmp2 == ConstraintGraph::UNKNOWN) {
          errs() << "Unknown comparison between " << *ub1 << " and " << *ub2 <<"\n";
         }
        #endif
        break;
      default:
      #if DEBUG_LOCAL
        errs() << "Unknown comparison between " << *index1 << " and " << *index2 <<"\n";
      #endif
        // Unknown indicies, or unknown sizes
        break;
    }
  }
}

void BoundsChecking::eliminateBackwards(BoundsCheck* check1, BoundsCheck* check2,
                                        ConstraintGraph *cg) {
  Value *ub1 = check1->getUpperBound();
  Value *ub2 = check2->getUpperBound();
  Value *index1 = check1->getIndex();
  Value *index2 = check2->getIndex();

  // Compare index 1 to index 2
  ConstraintGraph::CompareEnum cmp1 = cg->compare(index1, index2);
  if (check2->hasLowerBoundsCheck() && check1->hasLowerBoundsCheck()) {
    // If check2 lower bounds check is valid
    switch (cmp1) {
      #if DEBUG_LOCAL
      case ConstraintGraph::LESS_THAN:
        break;
      #endif
      case ConstraintGraph::EQUALS:
      case ConstraintGraph::GREATER_THAN:
      #if DEBUG_LOCAL
        errs() << "Checking Lower Bound Subsuming...\n";
      #endif
      #if DEBUG_LOCAL
        errs() << "Deleting Lower Bounds Check for " << *index1 << "\n";
      #endif
        // If index1 >= index2, don't need 0 <= index1
        if (cg->findDependencyPath(index1, check2->getIndex(), &(check2->dependentInsts))) {
          check1->deleteLowerBoundsCheck();
          check2->insertBefore(dyn_cast<Instruction>(check1->getIndex()), false);
        }
      #if DEBUG_LOCAL
        else {
          errs() << "Could not move " << *index2 << " to " << *index1 << "\n";
        }
      #endif
        break;
      default:
      #if DEBUG_LOCAL
        errs() << "Unknown comparison between " << *index1 << " and " << *index2 <<"\n";
      #endif
        // Unknown value for indicies
        break;
    }
  }

  if (check2->hasUpperBoundsCheck() && check1->hasUpperBoundsCheck()) {
    // If check 2 is upper bounds check valid
    ConstraintGraph::CompareEnum cmp2 = cg->compare(ub1, ub2);
    switch (cmp1) {
      case ConstraintGraph::LESS_THAN:
      case ConstraintGraph::EQUALS:
      #if DEBUG_LOCAL
        errs() << "Checking Upper Bound Subsuming...\n";
      #endif
        if (cmp2 == ConstraintGraph::GREATER_THAN || cmp2 == ConstraintGraph::EQUALS) {
        #if DEBUG_LOCAL
          errs() << "Deleting Upper Bounds Check for " << *index1 << "\n";
        #endif
          // If index1 <= index2, and ub2 <= ub1, don't need index1 <= ub1
          if (cg->findDependencyPath(index1, check2->getOffset(), &(check2->dependentInsts))) {
            check1->deleteUpperBoundsCheck();
            check2->insertBefore(dyn_cast<Instruction>(check1->getIndex()), true);
          }
        #if DEBUG_LOCAL
          else {
            errs() << "Could not move " << *index2 << " to " << *index1 << "\n";
          }
        #endif
        }
      #if DEBUG_LOCAL
        else if (cmp2 == ConstraintGraph::UNKNOWN) {
          errs() << "Unknown comparison between " << *ub1 << " and " << *ub2 <<"\n";
        }
      #endif
        break;
      #if DEBUG_LOCAL
      case ConstraintGraph::GREATER_THAN:
      #endif
      default:
      #if DEBUG_LOCAL
        errs() << "Unknown comparison between " << *index1 << " and " << *index2 <<"\n";
      #endif
        // Unknown indicies, or unknown sizes
        break;
    }
  }
}

void BoundsChecking::getCheckVariables(std::vector<BoundsCheck*> *boundsChecks, ConstraintGraph *cg) {
  for (std::vector<BoundsCheck*>::iterator i = boundsChecks->begin(), e = boundsChecks->end();
          i != e; i++ ) {
    BoundsCheck *check = *i;

    bool known;
    int64_t weight;
    Value *val = cg->findFirstLoad(check->getIndex(), &weight, &known);
    check->setVariable(val, weight, known);
  }
}

void BoundsChecking::EliminateBoundsChecks(std::vector<BoundsCheck*> *boundsChecks,
                                           ConstraintGraph *cg) {
#if DEBUG_LOCAL
  errs() << "Forward Elimination...\n";
#endif
  // Forward analysis to identify if higher occuring bounds check
  // is stricter than lower occuring bounds check
  for (int i = 0; i < ((int)boundsChecks->size())-1; i++) {
    BoundsCheck *check = boundsChecks->at(i);

    if (check->stillExists()) {
      for (unsigned int j = i + 1; j < boundsChecks->size(); j++) {
        BoundsCheck* tmp = boundsChecks->at(j);
        if (tmp->stillExists()) {
          eliminateForwards(check, tmp, cg);
        }
      }
    }
  }


#if DEBUG_LOCAL
  errs() << "Backwards Elimination...\n";
#endif
  // Backwards analysis to identify if lower occuring bounds check
  // is stricter than higher occuring bounds check
  for (int i = boundsChecks->size()-1; i >= 1; i--) {
    BoundsCheck *check = boundsChecks->at(i);

    if (check->stillExists()) {
      for (int j = i - 1; j >= 0;  j--) {
        BoundsCheck* tmp = boundsChecks->at(j);
        if (tmp->stillExists()) {
          eliminateBackwards(tmp, check, cg);
        }
      }
    }
  }
}

void BoundsChecking::hoistCheck(BoundsCheck* check)
{
  if (!check->stillExists())
    return;
  if (check->shouldHoistCheck()) {
    // Propogate the instructions to their new location
    Instruction *insertPoint = check->getInsertPoint();
    #if DEBUG_LOOP
      errs() << "Inserting Instructions at: " <<  *insertPoint << "\n";
    #endif
    for (std::vector<Instruction*>::iterator i = check->dependentInsts.begin(),
             e = check->dependentInsts.end(); i != e; i++) {
      Instruction *inst = *i;
    #if DEBUG_LOOP
      errs() << "Hoisting instruction: " << *inst << " before " << *insertPoint << "\n";
    #endif
      inst->moveBefore(insertPoint);
      insertPoint = inst;
    }
  }
}

void BoundsChecking::promoteCheck(BoundsCheck* check) {
  if (!check->stillExists())
    return;

  if (check->moveCheck()) {
    // Propogate the instructions to their new location
    Instruction *insertPoint = check->getInsertPoint();
    #if DEBUG_LOCAL
      errs() << "Inserting Instructions at: " <<  *insertPoint << "\n";
    #endif
    for (std::vector<Instruction*>::iterator i = check->dependentInsts.begin(),
             e = check->dependentInsts.end(); i != e; i++) {
      Instruction *inst = *i;
    #if DEBUG_LOCAL
      errs() << "Moving instruction: " << *inst << " before " << *insertPoint << "\n";
    #endif
      inst->moveBefore(insertPoint);
      insertPoint = inst;
    }
  }
}

void BoundsChecking::promoteLocalChecks(std::vector<BoundsCheck*> *boundsChecks)
{
  for (std::vector<BoundsCheck*>::iterator i = boundsChecks->begin(),
            e = boundsChecks->end(); i != e; i++) {
    promoteCheck(*i);
  }
}

void BoundsChecking::LocalAnalysis(BasicBlock *blk, std::vector<BoundsCheck*> *boundsChecks, ConstraintGraph* cg)
{
  // Identify bounds checks in block
  IdentifyBoundsChecks(blk, boundsChecks);
#if DEBUG_IDENTIFY
  errs() << "===================================\n";
  errs() << "Identified Bounds Checks: " << blk->getName() << "\n";
  for (std::vector<BoundsCheck*>::iterator i = boundsChecks->begin(),
        e = boundsChecks->end(); i != e; i++) {
    BoundsCheck* check = *i;
    check->print();
  }
#endif

#if DEBUG_LOCAL
  errs() << "===================================\n";
#endif

#if DEBUG_LOCAL
  errs() << "===================================\n";
  errs() << "Building Constraints Graph\n";
#endif
  // Build the Constraits Graph for blk
  buildConstraintGraph(blk, cg);
#if DEBUG_LOCAL
  cg->print();
  errs() << "===================================\n";
#endif

#if DEBUG_LOCAL
  errs() << "===================================\n";
  errs() << "Eliminating Bounds Checks\n";
#endif
  // Eliminate bounds checks from block
  getCheckVariables(boundsChecks, cg);
  EliminateBoundsChecks(boundsChecks, cg);
#if DEBUG_LOCAL
  errs() << "===================================\n";
#endif

#if DEBUG_LOCAL
  errs() << "Promoting Checks\n";
#endif
  promoteLocalChecks(boundsChecks);
#if DEBUG_LOCAL
  errs() << "===================================\n";
#endif
#if DEBUG_LOCAL
  for (std::vector<BoundsCheck*>::iterator i = boundsChecks->begin(),
        e = boundsChecks->end(); i != e; i++) {
    BoundsCheck* check = *i;
    check->print();
  }
  errs() << "===================================\n";
#endif
}


void BoundsChecking::GlobalAnalysis(std::vector<BasicBlock*> *worklist, std::map<BasicBlock*,std::vector<BoundsCheck*>*> *blkChecks, std::map<BasicBlock*,ConstraintGraph*> *blkCG)
{
  std::vector<GlobalCheck*> allChecks;
  std::map<BasicBlock*,BlockFlow*> flows;

  // Create a block flow object for each valid block
  for (std::vector<BasicBlock*>::iterator i = worklist->begin(), e = worklist->end();
              i != e; i++) {
    BasicBlock *blk = *i;
#if DEBUG_GLOBAL
    errs() << "===================================\n";
    errs() << "Creating Flow Block for Block: " << blk->getName() << "\n";
#endif
    BlockFlow *blk_flow = new BlockFlow(blk, (*blkChecks)[blk], (*blkCG)[blk], &flows);
    flows[blk] = blk_flow;
  }

  BasicBlock *entry = &(worklist->at(0)->getParent()->getEntryBlock());
  flows[entry]->isEntry = true;
  flows[entry]->identifyOutSet();

#if DEBUG_GLOBAL
  errs() << "Performing Available Check Analysis:\n";
  int iteration = 0;
#endif
  // Perform the available expression analysis
  bool change;
  do {
  #if DEBUG_GLOBAL
    errs() << "Running iteration: " << iteration << "\n";
    iteration++;
  #endif
    change = false;
    for (std::vector<BasicBlock*>::iterator i = worklist->begin(), e = worklist->end(); i != e; i++) {
      BasicBlock *blk = *i;
      if (blk != entry) {
        change |= flows[blk]->identifyOutSet();
      }
    }
  } while (change);
#if DEBUG_GLOBAL
  for (std::vector<BasicBlock*>::iterator i = worklist->begin(), e = worklist->end();
              i != e; i++) {
    BasicBlock *blk = *i;
    BlockFlow *blk_flow = flows[blk];
    blk_flow->print();
  }
  errs() << "==============================\n";
#endif
#if DEBUG_GLOBAL
  errs() << "Eliminating Redundant Checks\n";
#endif
  // Eliminate checks based on in set values
  for (std::vector<BasicBlock*>::iterator i = worklist->begin(), e = worklist->end(); i != e; i++) {
    BasicBlock *blk = *i;
    BlockFlow *blk_flow = flows[blk];
  #if DEBUG_GLOBAL
    errs() << "Eliminating Checks for Block:" << blk->getName() << "\n";
  #endif
    blk_flow->eliminateRedundantChecks();
  }
#if DEBUG_GLOBAL
  errs() << "==============================\n";
#endif
}

void BoundsChecking::addLoopIntoQueue(Loop *L, std::list<Loop*> *loopQueue)
{
  loopQueue->push_front(L);
  const std::vector<Loop *> &subLoop = L->getSubLoops();
  for (size_t I = 0; I < subLoop.size(); ++I) {
    addLoopIntoQueue(subLoop[I], loopQueue);
  }
}

void BoundsChecking::LoopAnalysis(std::vector<BasicBlock*> *worklist, std::map<BasicBlock*,std::vector<BoundsCheck*>*> *blkChecks, std::map<BasicBlock*,ConstraintGraph*> *blkCG)
{
  LoopInfo *LoopInf = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
  // Candidate check which can be hoisted for each bbl
  std::map<BasicBlock*, std::vector<BoundsCheck*>*> candidates;

  std::list<Loop*> LoopQueue;

  for (LoopInfo::iterator LoopI = LoopInf->begin(), LoopE = LoopInf->end(); LoopI != LoopE; ++LoopI) {
    addLoopIntoQueue(*LoopI, &LoopQueue);
  }

  for (std::list<Loop*>::iterator LoopI = LoopQueue.begin(), LoopE = LoopQueue.end(); LoopI != LoopE; ++LoopI) {
    Loop* loop = *LoopI;
    bool isSimplified = loop->isLoopSimplifyForm();
    BasicBlock *PreHeader = NULL;
    BasicBlock *ExitBlock = NULL;
    if (isSimplified) {
      PreHeader = loop->getLoopPreheader();
      ExitBlock = loop->getExitBlock();
    #if DEBUG_LOOP
      errs() << "Loop Preheader: " << PreHeader->getName() << "\n";
      errs() << "Loop Exit: " << ExitBlock->getName() << "\n";
    #endif
    } else {
      errs() << "Loop is not in simplified form. Run Loop Simplify Pass.\n";
    #if DEBUG_LOOP
      errs() << "==============================\n";
      loop->dump();
      errs() << "==============================\n";
      return;
    #endif
    }
    if (ExitBlock && PreHeader) {
      std::vector<BoundsCheck*> *preheaderChecks = (*blkChecks)[PreHeader];
      // Identify variables that change values in loops and variables that do not change values
      std::set<Value*> storeSet;
      bool canHoist = true;
      for (Loop::block_iterator loopBlkI = loop->block_begin(), loopBlkE = loop->block_end(); loopBlkI != loopBlkE; ++loopBlkI) {
        BasicBlock *loopBlk = *loopBlkI;
        for (BasicBlock::iterator i = loopBlk->begin(), e = loopBlk->end(); i != e; ++i) {
          Instruction *inst = &*i;
          if (isa<CallInst>(inst)) {
            canHoist = false;
            break;
          }
          StoreInst *SI = dyn_cast<StoreInst>(inst);
          if (SI != NULL) {
            storeSet.insert(SI->getPointerOperand());
            Value *to = SI->getPointerOperand();
            Type* T = to->getType();
            bool isPointer = T->isPointerTy() && T->getContainedType(0)->isPointerTy();
            if (isPointer) {
              canHoist = false;
            }
          }
        }
      }

      if (!canHoist) {
      #if DEBUG_LOOP
        errs() << "Cannot hoist operations out of loop due to pointer aliasing or function call\n";
      #endif
        continue;
      }
      for (Loop::block_iterator loopBlkI = loop->block_begin(), loopBlkE = loop->block_end(); loopBlkI != loopBlkE; ++loopBlkI) {
        BasicBlock *loopBlk = *loopBlkI;
        std::vector<BoundsCheck*> *checks = (*blkChecks)[loopBlk];
        ConstraintGraph *cg = (*blkCG)[loopBlk];
        for (std::vector<BoundsCheck*>::iterator chkI = checks->begin(); chkI != checks->end(); ) {
          BoundsCheck* chk = *chkI;
          if (!chk->stillExists()) {
            // Skip deleted checks
            chkI++;
            continue;
          }

          if (loop->isLoopInvariant(chk->getIndex())) {
          #if DEBUG_LOOP
            errs() << "===========================\n";
            errs() << "--Identified Invariant Check:\n";
            chk->print();
          #endif
            if (chk->shouldHoistCheck()) {
              // Check has been hoisted once already
              chk->hoistCheck(PreHeader);
              // Move the check up to the preheader
              preheaderChecks->push_back(chk);
              chkI = checks->erase(chkI);
              continue;
            } else if(cg->findDependencyPath(chk->getOffset(), &(chk->dependentInsts))) {
              // Hoist Check
              chk->hoistCheck(PreHeader);
              chk->originalBlock = loopBlk;
              // Move the check up to the preheader
              preheaderChecks->push_back(chk);
              chkI = checks->erase(chkI);
              continue;
            }
          #if DEBUG_LOOP
            else {
              errs() << "Could not hoist check...\n";
            }
          #endif
          } else {
            Value *var = chk->getVariable();
            if (var != NULL) {
              if (isa<AllocaInst>(var)) {
                if (storeSet.find(var) == storeSet.end()) {
                #if DEBUG_LOOP
                  errs() << "===========================\n";
                  errs() << "--Identified Invariant Check:\n";
                  chk->print();
                #endif
                  if (chk->shouldHoistCheck()) {
                    // Check has been hoisted once already
                    chk->hoistCheck(PreHeader);
                    // Move the check up to the preheader
                    preheaderChecks->push_back(chk);
                    chkI = checks->erase(chkI);
                    continue;
                  } else if (cg->findDependencyPath(chk->getOffset(), &(chk->dependentInsts))) {
                    // Check has been hoisted once already
                    chk->hoistCheck(PreHeader);
                    chk->originalBlock = loopBlk;
                    // Move the check up to the preheader
                    preheaderChecks->push_back(chk);
                    chkI = checks->erase(chkI);
                    continue;
                  }
                #if DEBUG_LOOP
                  else {
                    errs() << "Could not hoist check...\n";
                  }
                #endif
                }
              }
            }
          #if DEBUG_LOOP
            errs() << "===========================\n";
            errs() << "--Following Check is not Invariant\n";
            chk->print();
            errs() << "===========================\n";
          #endif
          }
          chkI++;
        }
      }
      // Identify monotonocity in checks
      std::map<BasicBlock*, VarFlow*> flows;
      std::set<Value*> checkVars;
      for (Loop::block_iterator loopBlkI = loop->block_begin(), loopBlkE = loop->block_end(); loopBlkI != loopBlkE; ++loopBlkI) {
        BasicBlock *loopBlk = *loopBlkI;
        std::vector<BoundsCheck*> *checks = (*blkChecks)[loopBlk];
        for (std::vector<BoundsCheck*>::iterator chkI = checks->begin(); chkI != checks->end(); chkI++) {
          BoundsCheck *check = *chkI;
          if (check->stillExists()) {
            Value* var = check->getVariable();
            if (var != NULL) {
              checkVars.insert(var);
            }
          }
        }
      }

      #if DEBUG_LOOP
        errs() << "Creating Preheader Flow Block: " << PreHeader->getName() << "\n";
      #endif
      flows[PreHeader] = new VarFlow(PreHeader, (*blkCG)[PreHeader], &checkVars, &flows, true, false);
      #if DEBUG_LOOP
        errs() << "Creating Exit Flow Block: " << ExitBlock->getName() << "\n";
      #endif
      flows[ExitBlock] = new VarFlow(ExitBlock, (*blkCG)[ExitBlock], &checkVars, &flows, false, true);
      for (Loop::block_iterator loopBlkI = loop->block_begin(), loopBlkE = loop->block_end(); loopBlkI != loopBlkE; ++loopBlkI) {
        BasicBlock *loopBlk = *loopBlkI;
        ConstraintGraph *cg = (*blkCG)[loopBlk];
      #if DEBUG_LOOP
        errs() << "Creating Var Flow for Block: " << loopBlk->getName() << "\n";
      #endif
        flows[loopBlk] = new VarFlow(loopBlk, cg, &checkVars, &flows, false, false);
      }
    #if DEBUG_LOOP
      int iteration = 0;
    #endif
      bool changes = false;
      do {
        changes = false;
      #if DEBUG_LOOP
         errs() << "Running iteration: " << iteration++ << "\n";
      #endif
        for (std::map<BasicBlock*,VarFlow*>::iterator it = flows.begin(), et = flows.end(); it != et; it++) {
        #if DEBUG_LOOP
          errs() << "Generating In Set: " << it->first->getName() << "\n";
          it->second->printInSet();
        #endif
          changes |= it->second->identifyOutSet();
        #if DEBUG_LOOP
          errs() << "Generating Out Set: " << it->first->getName() << "\n";
          it->second->printOutSet();
        #endif
        }
      } while (changes);
    #if DEBUG_LOOP
      errs() << "Identified Monotonic Changes...\n";
      flows[ExitBlock]->printInSet();
    #endif

      std::map<Value*,VarFlow::Change> changeSet;
      flows[ExitBlock]->copyInSetTo(&changeSet);
      for (Loop::block_iterator loopBlkI = loop->block_begin(), loopBlkE = loop->block_end(); loopBlkI != loopBlkE; ++loopBlkI) {
        BasicBlock *loopBlk = *loopBlkI;
        std::vector<BoundsCheck*> *checks = (*blkChecks)[loopBlk];
        ConstraintGraph *cg = (*blkCG)[loopBlk];
        for (std::vector<BoundsCheck*>::iterator chkI = checks->begin(); chkI != checks->end(); chkI++) {
          BoundsCheck *check = *chkI;
          if (check->stillExists()) {
            Value *var = check->getVariable();
            if (var != NULL && check->comparisonKnown) {
              switch (changeSet[var]) {
                case VarFlow::EQUALS:
                  errs() << "Identified Bounds Check for non-changing variable. This should not occur due to invariant check motion that occurred previously.\n";
                  break;
                case VarFlow::INCREASING:
                  // Only move lower-bound check out, if we know that the computed index is larger than the var
                  if (check->comparedToVar >= 0) {
                    BoundsCheck *monCheck = check->createCopyAt(PreHeader);
                    monCheck->deleteUpperBoundsCheck(); // Not using upper bound check for monotonic check
                    // Find path from load to index
                  #if DEBUG_LOOP
                    errs() << "===========================\n";
                    errs() << "--Identified Monotonic Increasing LB Move\n";
                    monCheck->print();
                  #endif
                    if (check->shouldHoistCheck()) {
                      cg = (*blkCG)[check->originalBlock];
                    }
                    if (cg->findDependencyPath(monCheck->getIndex(), &(monCheck->dependentInsts))) {
                      check->deleteLowerBoundsCheck();
                      preheaderChecks->push_back(monCheck);
                    } else {
                  #if DEBUG_LOOP
                    errs() << "Could not find dependent instructions path. Did not add check";
                  #endif
                      delete monCheck;
                    }
                  }
                  break;
                case VarFlow::DECREASING:
                  // Only move upper-bound check out, if we know that the computed index is smaller than the var
                  if (check->comparedToVar <=0) {
                    BoundsCheck *monCheck = check->createCopyAt(PreHeader);
                    monCheck->deleteLowerBoundsCheck(); // Don't need lower bound
                    // Find path from load to index
                  #if DEBUG_LOOP
                    errs() << "===========================\n";
                    errs() << "--Identified Monotonic Increasing UB Move\n";
                    monCheck->print();
                  #endif
                    if (check->shouldHoistCheck()) {
                      cg = (*blkCG)[check->originalBlock];
                    }
                    if (cg->findDependencyPath(monCheck->getIndex(), &(monCheck->dependentInsts))) {
                      check->deleteUpperBoundsCheck(); // Delete upper bound that is being replaced
                      preheaderChecks->push_back(monCheck);
                    } else {
                  #if DEBUG_LOOP
                    errs() << "Could not find dependent instructions path. Did not add check";
                  #endif
                      delete monCheck;
                    }
                  }
                  break;
                default:
                  // Can't do anything for unknown case
                  break;
              }
            }
          }
        }
      }
    }
  }
  for (std::vector<BasicBlock*>::iterator i = worklist->begin(), e = worklist->end(); i != e; i++) {
    BasicBlock *blk = *i;
    std::vector<BoundsCheck*> *checks  = (*blkChecks)[blk];
    for (std::vector<BoundsCheck*>::iterator ci = checks->begin(), ce = checks->end(); ci != ce; ci++) {
      hoistCheck(*ci);
    }
  }


}

bool BoundsChecking::runOnFunction(Function &F) {
  TD = &(F.getParent()->getDataLayout());
  TLI = &getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();;

  TrapBB = 0;
  BuilderTy TheBuilder(F.getContext(), TargetFolder(*TD));
  Builder = &TheBuilder;
  ObjectSizeOffsetEvaluator TheObjSizeEval(*TD, TLI, F.getContext());
  ObjSizeEval = &TheObjSizeEval;

  std::vector<BasicBlock*> worklist;
  std::map<BasicBlock*, std::vector<BoundsCheck*>*> blkChecks;
  std::map<BasicBlock*, ConstraintGraph*> blkCG;

  numChecksAdded = 0;
  // Identify basic blocks in function
  for (Function::iterator i = F.begin(), e = F.end(); i != e; ++i) {
    BasicBlock* blk =  &*i;
    worklist.push_back(blk);
    blkCG[blk] = new ConstraintGraph();
    blkChecks[blk] = new std::vector<BoundsCheck*>();
  }

  errs() << "--Performing Local Analysis\n";
  // Iterate over the Basic Blocks and perform local analysis
  for (std::vector<BasicBlock*>::iterator i = worklist.begin(), e = worklist.end(); i != e; i++) {
    BasicBlock *blk = *i;
    LocalAnalysis(blk, blkChecks[blk], blkCG[blk]);
  }

  // Perform Global Analysis
  errs() << "--Performing Global Analysis\n";
  GlobalAnalysis(&worklist, &blkChecks, &blkCG);
  // Perform Loop Analysis
  errs() << "--Performing Loop Analysis\n";
  LoopAnalysis(&worklist, &blkChecks, &blkCG);
  // Insert identified checks
  errs() << "--Inserting Bounds Checks\n";
  bool MadeChange = true;
  int prevNumberChecks = numChecksAdded;
  for (std::vector<BasicBlock*>::iterator i = worklist.begin(), e = worklist.end();
              i != e; i++) {
    // Inserting Checks for given basic block
    BasicBlock* blk =  *i;
    MadeChange |= InsertChecks(blkChecks[blk]);
    errs() << "Basic Block (name=" << blk->getName() << "):";
    errs() << (numChecksAdded - prevNumberChecks)  << " Checks Added\n";
    prevNumberChecks = numChecksAdded;
  }
  errs() << "===================================\n";
  errs() << "--Total Number of Checks Addded: " << numChecksAdded << "\n";

#if DEBUG_INSTS
  for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
    Instruction *I = &*i;
    errs() << *I << "\n";
  }
#endif
  return MadeChange;
}

bool BoundsChecking::handleCopyCheck(BoundsCheck* check)
{
  if (!check->isCopy()) {
    return false;
  }

  std::vector<Instruction*> *insts = &(check->dependentInsts);
  Builder->SetInsertPoint(check->getInsertPoint());

  if (insts->empty()) {
    errs() << "Copy Check did not have any dependent instructions. Restoring Original Check\n";
    check->restoreOriginalCheck();
  }


  int i = ((int)insts->size())-1;
  Instruction *I = insts->at(i);
  Value *prevValue = NULL;
  std::vector<Instruction*> addedInsts;
  if (isa<LoadInst>(I)) {
    LoadInst* LI = dyn_cast<LoadInst>(I);
    prevValue = Builder->CreateLoad(LI->getPointerOperand());
    addedInsts.push_back(dyn_cast<Instruction>(prevValue));
  } else {
    errs() << "Expected original instruction to be LOAD\n";
    check->restoreOriginalCheck();
  }
  i--;
  for (; i >= 0; i--) {
    I = insts->at(i);
    bool error = false;
    Value *op1 = NULL;
    Value *op2 = NULL;
    if(isa<BinaryOperator>(I)) {
      BinaryOperator* BO = dyn_cast<BinaryOperator>(I);
      ConstantInt* c1 = dyn_cast<ConstantInt>(BO->getOperand(0));
      ConstantInt* c2 = dyn_cast<ConstantInt>(BO->getOperand(1));
      if ((c1 == NULL) && (c2 == NULL)) {
        error = true;
      } else if (c1 != NULL) {
        op1 = BO->getOperand(0);
        op2 = prevValue;
      } else if (c2 != NULL) {
        op1 = prevValue;
        op2 = BO->getOperand(1);
      } else {
        errs() << "Met Binary operator instruction with constant operands: " << *I << "\n";
        op1 = BO->getOperand(0);
        op2 = BO->getOperand(1);
      }

      if (!error)
        prevValue = Builder->CreateBinOp(BO->getOpcode(), op1, op2);
    } else if (isa<CastInst>(I)) {
      CastInst *CI = dyn_cast<CastInst>(I);
      prevValue = Builder->CreateCast(CI->getOpcode(), prevValue, CI->getDestTy());
    } else {
      errs() << "[MODULE BROKEN] COULD NOT COPY FOLLOWING INSTRUCTION: " << *I << "n";
      error = true;
    }
    if (error) {
      check->restoreOriginalCheck();
      for (std::vector<Instruction*>::iterator i = addedInsts.begin(), e = addedInsts.end(); i != e; i++) {
        (*i)->eraseFromParent();
      }
      return false;
    } else {
      addedInsts.push_back(dyn_cast<Instruction>(prevValue));
    }
  }
  if (addedInsts.empty()) {
    return false;
  } else {
    if (check->hasLowerBoundsCheck()) {
      check->setIndex(prevValue);
    } else {
      check->setOffset(prevValue);

    }
    return true;
  }
}

bool BoundsChecking::InsertCheck(BoundsCheck* check) {
  if (!check->stillExists())
    return false;

  bool MadeChanges = false;
  if (check->isCopy()) {
    if (!handleCopyCheck(check)) {
      return false;
    } else {
      MadeChanges = true;
    }
  }
#if DEBUG_INSERT
  errs() << "===================================\n";
  check->print();
#endif
  Inst = check->getInstruction();
  Value *Size = check->getUpperBound();
  Value *Index = check->getIndex();
  Value *Offset = check->getOffset();


  Builder->SetInsertPoint(check->getInsertPoint());
  Value *llvmCheck = NULL;
  if (check->hasUpperBoundsCheck()) {
    llvmCheck = Builder->CreateICmpULT(Size, Offset);
    numChecksAdded++;
  }

  if (check->hasLowerBoundsCheck()) {
    Type *T = Index->getType();
    numChecksAdded++;
    bool isPointer = T->isPointerTy();
    if (!isPointer) {
      Value *lowerCheck = Builder->CreateICmpSLT(Index, ConstantInt::get(T, 0));
      if (llvmCheck != NULL) {
        llvmCheck = Builder->CreateOr(lowerCheck, llvmCheck);
      } else {
        llvmCheck = lowerCheck;
      }
    }
  }

  if (llvmCheck != NULL) {
    emitBranchToTrap(llvmCheck);
    MadeChanges = true;
  }
  return MadeChanges;
}

bool BoundsChecking::InsertChecks(std::vector<BoundsCheck*> *boundsChecks) {
  bool MadeChange = false;
  for (std::vector<BoundsCheck*>::iterator i = boundsChecks->begin(),
            e = boundsChecks->end(); i != e; i++) {
    MadeChange |= InsertCheck(*i);
  }
  return MadeChange;
}


FunctionPass *llvm::createBoundsCheckingPass() {
  return new BoundsChecking();
}

/*
char BoundsChecking::ID = 0;
static RegisterPass<BoundsChecking> X("BoundsChecking", "Unsoundness Check");
*/