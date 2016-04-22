/////Project CS6241
/////Alireza Nazari-Ramyad Hadidi

//#define DEBUG
#define PART2_LOCAL_ELIMINATION
#define DEBUG_TYPE "mytests"
//#define DEBUG_LOOP 0

#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/User.h"

#include "llvm/IR/Dominators.h"

#include "llvm/IR/DebugInfo.h"

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/Pass.h"

#include "llvm/Support/raw_ostream.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/DominanceFrontier.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/MemoryBuiltins.h"


#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/SmallVector.h"

#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetFolder.h"
#include "llvm/IR/IRBuilder.h"

#include <math.h>
#include <list>
#include <stack>
#include <deque>
#include <queue>
#include <map>

using namespace llvm;

typedef IRBuilder<true, TargetFolder> BuilderTy;

namespace {

    class LightCheck {
    public:
        CallInst* call;
        static bool constIsSame(Value*, Value*);
        static bool live(Instruction*, Instruction*);
        static bool instIsIdentical(Instruction*, Instruction*);
        static bool identicalValue(Value*, Value*);
        mutable Value* fakeindex;
        mutable Value* realindex;
        mutable long top_limit;
        mutable long bottom_limit;

        friend bool operator<(const LightCheck& l, const LightCheck& r) {
            if ((l.realindex < r.realindex)
                    )
                return true;
            if ((l.realindex == r.realindex) &&
                    ((l.top_limit < r.top_limit)
                    || (l.bottom_limit < r.bottom_limit)))
                return true;
            else return false;
        }

        friend bool operator==(const LightCheck& l, const LightCheck& r) {
            if ((l.top_limit == r.top_limit) &&
                    (l.bottom_limit == r.bottom_limit) &&
                    (identicalValue(l.realindex, r.realindex))
                    ) {

                return true;
            } else {
                return false;
            }
        }

        //void printch() const{
        //   errs()<<*realindex<<"\ttoplimit:\t"<<top_limit<<"\tbottom:"<<bottom_limit<<"\n";
        // }
    };

    //call is added after inst because of index to be checked with ArrayInfo

    // Compute a structure for ArrayIndex to include all details to help eliminate duplications

    struct ComputedArrayIndex {
        Value* index; // Instruction of the array index (e.g.  %idxprom2 = sext i32 %add to i64)
        CallInst* maxCall; // Upper bound of the array
        CallInst* minCall; // Lower bound of the array
        // Expression example: (a + 1 -> add.1.load.a., 10*b+5+20*a+6 -> add.6.add.mul.20.load.a.add.5.mul.10.load.b.)
        std::string index_expr; // String that encodes the index subscript expression
        std::set<Instruction*> def_instrs; // Definitions of the identifies used in the subscript expression
    };

    struct MCFG_Node {
        std::string label;
        std::vector<Instruction*> instrs;
        std::set<MCFG_Node*> preds;
        std::set<MCFG_Node*> succs;

        std::set<LightCheck> In_min;
        std::set<LightCheck> out_min;
        std::set<LightCheck> gen_min;
        std::set<LightCheck> kill_min;

        std::set<LightCheck> In_max;
        std::set<LightCheck> out_max;
        std::set<LightCheck> gen_max;
        std::set<LightCheck> kill_max;
    };

    enum forType {
        NONE,
        LT,
        LE,
        GT,
        GE
    };

    // For loop changes

    struct loopInfoBounds {
        MCFG_Node* current_node;
        MCFG_Node* move_to_node; //put after all instructions of this node
        forType style;
        Constant* c_max;
        Constant* c_min;
        Value* index;
        bool move_max;
        bool move_min;
        /*
        move_max or move_min means which bound check to move.
        if c_max or c_min is NULL, then check with current index. Else, check with these.
        style is NULL for variables that do not change in loop
         */

        ComputedArrayIndex* computed_array_index;
    };

    enum EffectType {
        unCHANGED = 0, INC = 1,
        DEC = 2, MUL = 3,
        DIVGT1 = 4, DIVLT1 = 5,
        CHANGED = -1
    };

    struct ArrayInfo {
        long top_limit;
        long bottom_limit;
        Value* dynSize;
        unsigned typeSize;
    };

    struct Checks {
        Value* index;
        Value* realindex;
        ArrayInfo var;
        Value* base;
        Instruction* inst;
    };

    void dumpArrayInfoMap(std::map<Value*, ArrayInfo*>* map) {
        errs() << "size:" << map->size() << "\n";
        for (std::map<Value*, ArrayInfo*>::iterator it = map->begin(); it != map->end(); it++) {
            errs() << "\n====== ";
            it->first->dump();
            errs() << "\ttop_limit: " << it->second->top_limit
                    << "\n\tbottom_limit: " << it->second->bottom_limit;
            if (it->second->dynSize != NULL) {
                errs() << "\n\tdynSize: ";
                it->second->dynSize->dump();
            } else
                errs() << "\n\tdynSize: NULL\n";

            errs() << "\ttypeSize: " << it->second->typeSize << "\n";
        }
    }

    // Calculate now many * are there for the pointer
    // Eq. int** x; will return 2

    int calPointerDepth(Type* type) {
        int depth = 1;

        if (!type->isPointerTy())
            return 0;
        SequentialType* seqType = static_cast<SequentialType*> (type);
        while (seqType->getElementType()->isPointerTy()) {
            depth++;
            seqType = static_cast<SequentialType*> (seqType->getElementType());
        }
        return depth;
    }

    // Assume the input type is ArrayType which is a static multidimension array
    // Eq. int x[10][20] will return 200 in size

    long calArraySize(Type* type) {
        if (type->getTypeID() == Type::ArrayTyID) {
            ArrayType *a = static_cast<ArrayType*> (type);
            long size = a->getNumElements();

            while (a->getElementType()->isArrayTy()) {
                a = static_cast<ArrayType*> (a->getElementType());
                size *= a->getNumElements();
            }

            return size;
        } else if (type->getTypeID() == Type::IntegerTyID) {
#ifdef DEBUG
            errs() << "[calArraySize] What to do with integer type? ";
            type->dump();
            errs() << "\n";
#endif
            return 0;
        } else {
#ifdef DEBUG
            errs() << "[calArraySize] Can not handle this case!\n";
#endif
            return 0;
        }
    }

    struct Project : public FunctionPass {
        static char ID;
        friend class LightCheck;
        //////////////////////////
    public:
        std::map<Value*, ArrayInfo*> arrayMap;
        std::set<Instruction*> MCFGInst;
        std::set<Value*> index;
        std::map<CallInst*, Checks> mincalltocheckinfo;
        std::map<CallInst*, Checks> maxcalltocheckinfo;
        std::map<Instruction*, CallInst*> indextocallmax;
        std::map<Instruction*, CallInst*> indextocallmin;
        std::map<Value*, Value*> indexdefmap;
        unsigned long int counter, modified;

        BuilderTy *Builder;
        const DataLayout *TD;
        //////////////////////////

        Project() : FunctionPass(ID) {
        }

        /////////////////////////////////
        /////////////////////////////////

        void AddMaxCall(Instruction* curindex, Instruction* instbefore, unsigned max, unsigned min) {
            Type * ArgTypes_i64_u64[3];
            Type * ArgTypes_i32_u64[3];
            Type * ArgTypes_i64_u32[3];
            Type * ArgTypes_i32_u32[3];

            Type *VoidTy = Type::getVoidTy(curindex -> getContext());
            IntegerType *Int64Ty = Type::getInt64Ty(curindex -> getContext());
            IntegerType *Int32Ty = Type::getInt32Ty(curindex -> getContext());

            ArgTypes_i64_u64[0] = ArgTypes_i64_u32[0] = Int64Ty;
            ArgTypes_i32_u64[0] = ArgTypes_i32_u32[0] = Int32Ty;

            ArgTypes_i64_u64[1] = ArgTypes_i64_u32[1] = Int64Ty;
            ArgTypes_i32_u64[1] = ArgTypes_i32_u32[1] = Int64Ty;

            ArgTypes_i64_u64[2] = ArgTypes_i32_u64[2] = Int64Ty;
            ArgTypes_i64_u32[2] = ArgTypes_i32_u32[2] = Int32Ty;

            ArrayRef<Type*> arg_types_i32_u64(ArgTypes_i32_u64, 3);
            ArrayRef<Type*> arg_types_i32_u32(ArgTypes_i32_u32, 3);
            ArrayRef<Type*> arg_types_i64_u64(ArgTypes_i64_u64, 3);
            ArrayRef<Type*> arg_types_i64_u32(ArgTypes_i64_u32, 3);

            FunctionType *ChkType_i64_u64 = FunctionType::get(VoidTy, arg_types_i64_u64, false);
            //FunctionType *ChkType_i64_u32 = FunctionType::get(VoidTy, arg_types_i64_u32, false);
            FunctionType *ChkType_i32_u64 = FunctionType::get(VoidTy, arg_types_i32_u64, false);
            //FunctionType *ChkType_i32_u32 = FunctionType::get(VoidTy, arg_types_i32_u32, false);

            // Insert or retrieve the checking function into the program Module
            Module *M = curindex->getParent()->getParent()->getParent();

            // Constant *Chk_i32_u32 = M->getOrInsertFunction("__checkArrayBounds_i32_u32", ChkType_i32_u32);
            Constant *Chk_i64_u64_max = M->getOrInsertFunction("__checkArrayBounds_max_i64_u64", ChkType_i64_u64);
            //Constant *Chk_i64_u32 = M->getOrInsertFunction("__checkArrayBounds_i64_u32", ChkType_i64_u32);
            Constant *Chk_i32_u64_max = M->getOrInsertFunction("__checkArrayBounds_max_i32_u64", ChkType_i32_u64);
            // Constant *Chk_i32_u32 = M->getOrInsertFunction("__checkArrayBounds_i32_u32", ChkType_i32_u32);



            // Create the arguments list
            Value * args[3];
            //Index
            args[0] = curindex;
            //Min
            args[1] = ConstantInt::get(Int64Ty, min);
            //Max
            args[2] = ConstantInt::get(Int64Ty, max);




            if (dyn_cast<Instruction>(args[0]))
                index.insert((Instruction*) args[0]);

            CallInst *hookmax;



            //errs()<<*calltocheckinfo[hook].index<<"\n";

            //MCFGInst.insert(hook);
            // Create Array Reference to the function arguments
            ArrayRef<Value*> func_args(args, 3);

            if (args[0]->getType()->getIntegerBitWidth() == 64) {

                // Create the function call

                hookmax = CallInst::Create(Chk_i64_u64_max, func_args, "");
                // Insert the function call
                //errs()<<*hookmax;
                hookmax->insertAfter(instbefore);

                indextocallmax[(Instruction*) args[0]] = hookmax;

                maxcalltocheckinfo[hookmax].index = args[0];
                maxcalltocheckinfo[hookmax].inst = instbefore;
                maxcalltocheckinfo[hookmax].base = NULL;
                maxcalltocheckinfo[hookmax].var.top_limit = max;
                maxcalltocheckinfo[hookmax].var.bottom_limit = min;

                modified++;

            } else if (args[0]->getType()->getIntegerBitWidth() == 32) {

                // Create the function call

                hookmax = CallInst::Create(Chk_i32_u64_max, func_args, "");
                // Insert the function call
                //errs()<<*hookmax;

                hookmax->insertAfter(instbefore);

                indextocallmax[(Instruction*) args[0]] = hookmax;

                maxcalltocheckinfo[hookmax].index = args[0];
                maxcalltocheckinfo[hookmax].inst = instbefore;
                maxcalltocheckinfo[hookmax].base = NULL;
                maxcalltocheckinfo[hookmax].var.top_limit = max;
                maxcalltocheckinfo[hookmax].var.bottom_limit = min;
                modified++;


            } else {
#ifdef DEBUG
                errs() << "Shouldn't come here\n";
#endif
            }


        }

        void AddMinCall(Instruction* curindex, Instruction* instbefore, unsigned max, unsigned min) {
            Type * ArgTypes_i64_u64[3];
            Type * ArgTypes_i32_u64[3];
            Type * ArgTypes_i64_u32[3];
            Type * ArgTypes_i32_u32[3];

            Type *VoidTy = Type::getVoidTy(curindex -> getContext());
            IntegerType *Int64Ty = Type::getInt64Ty(curindex -> getContext());
            IntegerType *Int32Ty = Type::getInt32Ty(curindex -> getContext());

            ArgTypes_i64_u64[0] = ArgTypes_i64_u32[0] = Int64Ty;
            ArgTypes_i32_u64[0] = ArgTypes_i32_u32[0] = Int32Ty;

            ArgTypes_i64_u64[1] = ArgTypes_i64_u32[1] = Int64Ty;
            ArgTypes_i32_u64[1] = ArgTypes_i32_u32[1] = Int64Ty;

            ArgTypes_i64_u64[2] = ArgTypes_i32_u64[2] = Int64Ty;
            ArgTypes_i64_u32[2] = ArgTypes_i32_u32[2] = Int32Ty;

            ArrayRef<Type*> arg_types_i32_u64(ArgTypes_i32_u64, 3);
            ArrayRef<Type*> arg_types_i32_u32(ArgTypes_i32_u32, 3);
            ArrayRef<Type*> arg_types_i64_u64(ArgTypes_i64_u64, 3);
            ArrayRef<Type*> arg_types_i64_u32(ArgTypes_i64_u32, 3);

            FunctionType *ChkType_i64_u64 = FunctionType::get(VoidTy, arg_types_i64_u64, false);
            //FunctionType *ChkType_i64_u32 = FunctionType::get(VoidTy, arg_types_i64_u32, false);
            FunctionType *ChkType_i32_u64 = FunctionType::get(VoidTy, arg_types_i32_u64, false);
            //FunctionType *ChkType_i32_u32 = FunctionType::get(VoidTy, arg_types_i32_u32, false);

            // Insert or retrieve the checking function into the program Module
            Module *M = curindex->getParent()->getParent()->getParent();

            // Constant *Chk_i32_u32 = M->getOrInsertFunction("__checkArrayBounds_i32_u32", ChkType_i32_u32);
            Constant *Chk_i64_u64_min = M->getOrInsertFunction("__checkArrayBounds_min_i64_u64", ChkType_i64_u64);
            //Constant *Chk_i64_u32 = M->getOrInsertFunction("__checkArrayBounds_i64_u32", ChkType_i64_u32);
            Constant *Chk_i32_u64_min = M->getOrInsertFunction("__checkArrayBounds_min_i32_u64", ChkType_i32_u64);
            // Constant *Chk_i32_u32 = M->getOrInsertFunction("__checkArrayBounds_i32_u32", ChkType_i32_u32);



            // Create the arguments list
            Value * args[3];
            //Index
            args[0] = curindex;
            //Min
            args[1] = ConstantInt::get(Int64Ty, min);
            //Max
            args[2] = ConstantInt::get(Int64Ty, max);




            if (dyn_cast<Instruction>(args[0]))
                index.insert((Instruction*) args[0]);

            CallInst *hookmin;



            //errs()<<*calltocheckinfo[hook].index<<"\n";

            //MCFGInst.insert(hook);
            // Create Array Reference to the function arguments
            ArrayRef<Value*> func_args(args, 3);

            if (args[0]->getType()->getIntegerBitWidth() == 64) {

                // Create the function call

                hookmin = CallInst::Create(Chk_i64_u64_min, func_args, "");
                // Insert the function call

                hookmin->insertAfter(instbefore);

                indextocallmin[(Instruction*) args[0]] = hookmin;

                mincalltocheckinfo[hookmin].index = args[0];
                mincalltocheckinfo[hookmin].inst = instbefore;
                mincalltocheckinfo[hookmin].base = NULL;
                mincalltocheckinfo[hookmin].var.top_limit = max;
                mincalltocheckinfo[hookmin].var.bottom_limit = min;

                modified++;

            } else if (args[0]->getType()->getIntegerBitWidth() == 32) {

                // Create the function call

                hookmin = CallInst::Create(Chk_i32_u64_min, func_args, "");
                // Insert the function call

                hookmin->insertAfter(instbefore);

                indextocallmin[(Instruction*) args[0]] = hookmin;

                mincalltocheckinfo[hookmin].index = args[0];
                mincalltocheckinfo[hookmin].inst = instbefore;
                mincalltocheckinfo[hookmin].base = NULL;
                mincalltocheckinfo[hookmin].var.top_limit = max;
                mincalltocheckinfo[hookmin].var.bottom_limit = min;
                modified++;


            } else {
#ifdef DEBUG
                errs() << "Shouldn't come here\n";
#endif
            }


        }

        /*******************************************************************/
        /* Functions for basic array bound check with constant upper bound */

        /*******************************************************************/
        void insertBoundCheck(Instruction* inst, GetElementPtrInst *GEP, unsigned num_array_elems) {
#ifdef PRINT_REMOVE_INFO
            errs() << "[insertBoundCheck-static] limit: " << num_array_elems << ", GEP: ";
            GEP->dump();
#endif
            unsigned a = GEP->getNumOperands();
            Value* tmp;
            //Index
            if (a == 3)
                tmp = GEP -> getOperand(2);
            else if (a == 2)
                tmp = GEP -> getOperand(1);

            ////////////////////////////////
            int min = 0;
            int max = num_array_elems;

            if (dyn_cast<ConstantInt>(tmp)) {
                //int min=0;
                //int max=num_array_elems;
                int ind = ((ConstantInt*) tmp)-> getSExtValue();
                //errs()<<ind<<" "<<min<<" "<<max<<"\n";
                if ((ind < min) || (ind > max)) {
                    errs() << "Line " << getLine((Instruction*) GEP) << ": Warning:: index " << ind << " out of band" << min << " : " << max << "\n";

                }
                return;
            }

            //////////////////////////////////
            // Create bounds-checking function call's prototype
            Type * ArgTypes_i64_u64[3];
            Type * ArgTypes_i32_u64[3];
            Type * ArgTypes_i64_u32[3];
            Type * ArgTypes_i32_u32[3];

            Type *VoidTy = Type::getVoidTy(inst -> getContext());
            IntegerType *Int64Ty = Type::getInt64Ty(inst -> getContext());
            IntegerType *Int32Ty = Type::getInt32Ty(inst -> getContext());

            ArgTypes_i64_u64[0] = ArgTypes_i64_u32[0] = Int64Ty;
            ArgTypes_i32_u64[0] = ArgTypes_i32_u32[0] = Int32Ty;

            ArgTypes_i64_u64[1] = ArgTypes_i64_u32[1] = Int64Ty;
            ArgTypes_i32_u64[1] = ArgTypes_i32_u32[1] = Int64Ty;

            ArgTypes_i64_u64[2] = ArgTypes_i32_u64[2] = Int64Ty;
            ArgTypes_i64_u32[2] = ArgTypes_i32_u32[2] = Int32Ty;

            ArrayRef<Type*> arg_types_i32_u64(ArgTypes_i32_u64, 3);
            ArrayRef<Type*> arg_types_i32_u32(ArgTypes_i32_u32, 3);
            ArrayRef<Type*> arg_types_i64_u64(ArgTypes_i64_u64, 3);
            ArrayRef<Type*> arg_types_i64_u32(ArgTypes_i64_u32, 3);

            FunctionType *ChkType_i64_u64 = FunctionType::get(VoidTy, arg_types_i64_u64, false);
            //FunctionType *ChkType_i64_u32 = FunctionType::get(VoidTy, arg_types_i64_u32, false);
            FunctionType *ChkType_i32_u64 = FunctionType::get(VoidTy, arg_types_i32_u64, false);
            //FunctionType *ChkType_i32_u32 = FunctionType::get(VoidTy, arg_types_i32_u32, false);

            // Insert or retrieve the checking function into the program Module
            Module *M = inst->getParent()->getParent()->getParent();
            Constant *Chk_i64_u64_max = M->getOrInsertFunction("__checkArrayBounds_max_i64_u64", ChkType_i64_u64);
            //Constant *Chk_i64_u32 = M->getOrInsertFunction("__checkArrayBounds_i64_u32", ChkType_i64_u32);
            Constant *Chk_i32_u64_max = M->getOrInsertFunction("__checkArrayBounds_max_i32_u64", ChkType_i32_u64);
            // Constant *Chk_i32_u32 = M->getOrInsertFunction("__checkArrayBounds_i32_u32", ChkType_i32_u32);
            Constant *Chk_i64_u64_min = M->getOrInsertFunction("__checkArrayBounds_min_i64_u64", ChkType_i64_u64);
            //Constant *Chk_i64_u32 = M->getOrInsertFunction("__checkArrayBounds_i64_u32", ChkType_i64_u32);
            Constant *Chk_i32_u64_min = M->getOrInsertFunction("__checkArrayBounds_min_i32_u64", ChkType_i32_u64);
            // Constant *Chk_i32_u32 = M->getOrInsertFunction("__checkArrayBounds_i32_u32", ChkType_i32_u32);


            unsigned num_gep_operands = GEP->getNumOperands();
            // Create the arguments list
            Value * args[3];
            //Index
            if (num_gep_operands == 3)
                args[0] = GEP -> getOperand(2);
            else if (num_gep_operands == 2)
                args[0] = GEP -> getOperand(1);
            else {
#ifdef DEBUG
                errs() << "[insertBoundCheck] Warning! wrong num of arguments!\n";
#endif
            }

            //Min
            args[1] = ConstantInt::get(Int64Ty, 0);
            //Max
            args[2] = ConstantInt::get(Int64Ty, num_array_elems);




            if (dyn_cast<Instruction>(args[0]))
                index.insert((Instruction*) args[0]);

            CallInst *hookmin;
            CallInst *hookmax;


            //errs()<<*calltocheckinfo[hook].index<<"\n";

            //MCFGInst.insert(hook);
            // Create Array Reference to the function arguments
            ArrayRef<Value*> func_args(args, 3);

            if (args[0]->getType()->getIntegerBitWidth() == 64) {

                // Create the function call
                hookmax = CallInst::Create(Chk_i64_u64_max, func_args, "");
                hookmin = CallInst::Create(Chk_i64_u64_min, func_args, "");
                // Insert the function call
                hookmax->insertAfter(inst);
                hookmin->insertAfter(inst);
                indextocallmax[(Instruction*) args[0]] = hookmax;
                indextocallmin[(Instruction*) args[0]] = hookmin;
                maxcalltocheckinfo[hookmax].index = args[0];
                maxcalltocheckinfo[hookmax].inst = inst;
                maxcalltocheckinfo[hookmax].base = GEP->getOperand(0);
                maxcalltocheckinfo[hookmax].var.top_limit = max;
                maxcalltocheckinfo[hookmax].var.bottom_limit = min;
                mincalltocheckinfo[hookmin].index = args[0];
                mincalltocheckinfo[hookmin].inst = inst;
                mincalltocheckinfo[hookmin].base = GEP->getOperand(0);
                mincalltocheckinfo[hookmin].var.top_limit = max;
                mincalltocheckinfo[hookmin].var.bottom_limit = min;

                counter++;
                counter++;
            } else if (args[0]->getType()->getIntegerBitWidth() == 32) {

                // Create the function call
                hookmax = CallInst::Create(Chk_i32_u64_max, func_args, "");
                hookmin = CallInst::Create(Chk_i32_u64_min, func_args, "");
                // Insert the function call
                hookmax->insertAfter(inst);
                hookmin->insertAfter(inst);
                indextocallmax[(Instruction*) args[0]] = hookmax;
                indextocallmin[(Instruction*) args[0]] = hookmin;
                maxcalltocheckinfo[hookmax].index = args[0];
                maxcalltocheckinfo[hookmax].inst = inst;
                maxcalltocheckinfo[hookmax].base = GEP->getOperand(0);
                maxcalltocheckinfo[hookmax].var.top_limit = max;
                maxcalltocheckinfo[hookmax].var.bottom_limit = min;
                mincalltocheckinfo[hookmin].index = args[0];
                mincalltocheckinfo[hookmin].inst = inst;
                mincalltocheckinfo[hookmin].base = GEP->getOperand(0);
                mincalltocheckinfo[hookmin].var.top_limit = max;
                mincalltocheckinfo[hookmin].var.bottom_limit = min;
                counter++;
                counter++;

            } else {
#ifdef DEBUG
                errs() << "Shouldn't come here\n";
#endif
            }
        }

        /******************************************************************/
        /* Functions for basic array bound check  with dynamic upper bound*/

        /******************************************************************/

        void insertBoundCheck(Instruction* inst, GetElementPtrInst *GEP, Value* num_array_elems) {
#ifdef PRINT_REMOVE_INFO
            errs() << "[insertBoundCheck-dynamic] \n\tlimit: ";
            num_array_elems->dump();
            errs() << "\tGEP: ";
            GEP->dump();
#endif

            // Create bounds-checking function call's prototype
            Type * ArgTypes_i64_u64[3];
            Type * ArgTypes_i32_u64[3];
            Type * ArgTypes_i64_u32[3];
            Type * ArgTypes_i32_u32[3];

            Type *VoidTy = Type::getVoidTy(inst -> getContext());
            IntegerType *Int64Ty = Type::getInt64Ty(inst -> getContext());
            IntegerType *Int32Ty = Type::getInt32Ty(inst -> getContext());

            ArgTypes_i64_u64[0] = ArgTypes_i64_u32[0] = Int64Ty;
            ArgTypes_i32_u64[0] = ArgTypes_i32_u32[0] = Int32Ty;

            ArgTypes_i64_u64[1] = ArgTypes_i64_u32[1] = Int64Ty;
            ArgTypes_i32_u64[1] = ArgTypes_i32_u32[1] = Int64Ty;

            ArgTypes_i64_u64[2] = ArgTypes_i32_u64[2] = Int64Ty;
            ArgTypes_i64_u32[2] = ArgTypes_i32_u32[2] = Int32Ty;

            ArrayRef<Type*> arg_types_i32_u64(ArgTypes_i32_u64, 3);
            ArrayRef<Type*> arg_types_i32_u32(ArgTypes_i32_u32, 3);
            ArrayRef<Type*> arg_types_i64_u64(ArgTypes_i64_u64, 3);
            ArrayRef<Type*> arg_types_i64_u32(ArgTypes_i64_u32, 3);

            FunctionType *ChkType_i64_u64 = FunctionType::get(VoidTy, arg_types_i64_u64, false);
            FunctionType *ChkType_i64_u32 = FunctionType::get(VoidTy, arg_types_i64_u32, false);
            FunctionType *ChkType_i32_u64 = FunctionType::get(VoidTy, arg_types_i32_u64, false);
            FunctionType *ChkType_i32_u32 = FunctionType::get(VoidTy, arg_types_i32_u32, false);

            // Insert or retrieve the checking function into the program Module
            Module *M = inst->getParent()->getParent()->getParent();
            Constant *Chk_i64_u64_max = M->getOrInsertFunction("__checkArrayBounds_max_i64_u64", ChkType_i64_u64);
            Constant *Chk_i64_u32_max = M->getOrInsertFunction("__checkArrayBounds_max_i64_u32", ChkType_i64_u32);
            Constant *Chk_i32_u64_max = M->getOrInsertFunction("__checkArrayBounds_max_i32_u64", ChkType_i32_u64);
            Constant *Chk_i32_u32_max = M->getOrInsertFunction("__checkArrayBounds_max_i32_u32", ChkType_i32_u32);

            Constant *Chk_i64_u64_min = M->getOrInsertFunction("__checkArrayBounds_min_i64_u64", ChkType_i64_u64);
            Constant *Chk_i64_u32_min = M->getOrInsertFunction("__checkArrayBounds_min_i64_u32", ChkType_i64_u32);
            Constant *Chk_i32_u64_min = M->getOrInsertFunction("__checkArrayBounds_min_i32_u64", ChkType_i32_u64);
            Constant *Chk_i32_u32_min = M->getOrInsertFunction("__checkArrayBounds_min_i32_u32", ChkType_i32_u32);


            unsigned num_gep_operands = GEP->getNumOperands();
            // Create the arguments list
            Value * args[3];
            //Index
            if (num_gep_operands == 3)
                args[0] = GEP -> getOperand(2);
            else if (num_gep_operands == 2)
                args[0] = GEP -> getOperand(1);
            else {
#ifdef DEBUG
                errs() << "[insertBoundCheck] Warning! wrong num of arguments!\n";
#endif
            }
            //Min
            args[1] = ConstantInt::get(Int64Ty, 0);
            //Max
            args[2] = num_array_elems;

            CallInst *hookmin;
            CallInst *hookmax;
            if (dyn_cast<Instruction>(args[0]))
                index.insert((Instruction*) args[0]);

            //MCFGInst.insert(hook);
            // Create Array Reference to the function arguments
            ArrayRef<Value*> func_args(args, 3);

            if (args[0]->getType()->getIntegerBitWidth() == 64 && args[2]->getType()->getIntegerBitWidth() == 64) {

                // Create the function call
                hookmax = CallInst::Create(Chk_i64_u64_max, func_args, "");
                hookmin = CallInst::Create(Chk_i64_u64_min, func_args, "");
                // Insert the function call
                hookmax->insertAfter(inst);
                hookmin->insertAfter(inst);
                indextocallmax[(Instruction*) args[0]] = hookmax;
                indextocallmin[(Instruction*) args[0]] = hookmin;


                //                maxcalltocheckinfo[hookmax].index=args[0];
                //                maxcalltocheckinfo[hookmax].inst= inst;
                //                maxcalltocheckinfo[hookmax].base= GEP->getOperand(0);
                //                maxcalltocheckinfo[hookmax].var.dynSize=num_array_elems;
                //                maxcalltocheckinfo[hookmax].var.bottom_limit=0;
                //
                //                mincalltocheckinfo[hookmin].index=args[0];
                //                mincalltocheckinfo[hookmin].inst= inst;
                //                mincalltocheckinfo[hookmin].base= GEP->getOperand(0);
                //                mincalltocheckinfo[hookmin].var.dynSize=num_array_elems;
                //                mincalltocheckinfo[hookmin].var.bottom_limit=0;
                counter++;
                counter++;

            } else if (args[0]->getType()->getIntegerBitWidth() == 32 && args[2]->getType()->getIntegerBitWidth() == 64) {

                // Create the function call
                hookmax = CallInst::Create(Chk_i32_u64_max, func_args, "");
                hookmin = CallInst::Create(Chk_i32_u64_min, func_args, "");
                // Insert the function call
                hookmax->insertAfter(inst);
                hookmin->insertAfter(inst);
                indextocallmax[(Instruction*) args[0]] = hookmax;
                indextocallmin[(Instruction*) args[0]] = hookmin;


                //                maxcalltocheckinfo[hookmax].index=args[0];
                //                maxcalltocheckinfo[hookmax].inst= inst;
                //                maxcalltocheckinfo[hookmax].base= GEP->getOperand(0);
                //                maxcalltocheckinfo[hookmax].var.dynSize=num_array_elems;
                //                maxcalltocheckinfo[hookmax].var.bottom_limit=0;
                //
                //                mincalltocheckinfo[hookmin].index=args[0];
                //                mincalltocheckinfo[hookmin].inst= inst;
                //                mincalltocheckinfo[hookmin].base= GEP->getOperand(0);
                //                mincalltocheckinfo[hookmin].var.dynSize=num_array_elems;
                //                mincalltocheckinfo[hookmin].var.bottom_limit=0;
                counter++;
                counter++;
            } else if (args[0]->getType()->getIntegerBitWidth() == 64 && args[2]->getType()->getIntegerBitWidth() == 32) {

                // Create the function call
                hookmax = CallInst::Create(Chk_i64_u32_max, func_args, "");
                hookmin = CallInst::Create(Chk_i64_u32_min, func_args, "");
                // Insert the function call
                hookmax->insertAfter(inst);
                hookmin->insertAfter(inst);
                indextocallmax[(Instruction*) args[0]] = hookmax;
                indextocallmin[(Instruction*) args[0]] = hookmin;

                //                maxcalltocheckinfo[hookmax].index=args[0];
                //                maxcalltocheckinfo[hookmax].inst= inst;
                //                maxcalltocheckinfo[hookmax].base= GEP->getOperand(0);
                //                maxcalltocheckinfo[hookmax].var.dynSize=num_array_elems;
                //                maxcalltocheckinfo[hookmax].var.bottom_limit=0;
                //
                //                mincalltocheckinfo[hookmin].index=args[0];
                //                mincalltocheckinfo[hookmin].inst= inst;
                //                mincalltocheckinfo[hookmin].base= GEP->getOperand(0);
                //                mincalltocheckinfo[hookmin].var.dynSize=num_array_elems;
                //                mincalltocheckinfo[hookmin].var.bottom_limit=0;
                counter++;
                counter++;
            } else if (args[0]->getType()->getIntegerBitWidth() == 32 && args[2]->getType()->getIntegerBitWidth() == 32) {

                // Create the function call

                hookmax = CallInst::Create(Chk_i32_u32_max, func_args, "");
                hookmin = CallInst::Create(Chk_i32_u32_min, func_args, "");
                // Insert the function call
                hookmax->insertAfter(inst);
                hookmin->insertAfter(inst);
                indextocallmax[(Instruction*) args[0]] = hookmax;
                indextocallmin[(Instruction*) args[0]] = hookmin;

                //                maxcalltocheckinfo[hookmax].index=args[0];
                //                maxcalltocheckinfo[hookmax].inst= inst;
                //                maxcalltocheckinfo[hookmax].base= GEP->getOperand(0);
                //                maxcalltocheckinfo[hookmax].var.dynSize=num_array_elems;
                //                maxcalltocheckinfo[hookmax].var.bottom_limit=0;
                //
                //                mincalltocheckinfo[hookmin].index=args[0];
                //                mincalltocheckinfo[hookmin].inst= inst;
                //                mincalltocheckinfo[hookmin].base= GEP->getOperand(0);
                //                mincalltocheckinfo[hookmin].var.dynSize=num_array_elems;
                //                mincalltocheckinfo[hookmin].var.bottom_limit=0;
                counter++;
                counter++;
            } else {
#ifdef DEBUG
                errs() << "Shouldn't come here\n";
#endif
            }
        }

        void processAllocateInst(Instruction* inst, DataLayout &dLayout) {
            AllocaInst* alloca = static_cast<AllocaInst*> ((Instruction*) inst);

            // This is static array declaration, int x[10]
            if (alloca->getType()->getElementType()->isArrayTy()) {

                ArrayType *a = static_cast<ArrayType*> (alloca->getType()->getElementType());
                ArrayInfo* info = new struct ArrayInfo;
                info->top_limit = calArraySize(a);
                info->bottom_limit = 0;
                info->dynSize = NULL;
                info->typeSize = dLayout.getTypeAllocSize(inst->getOperand(0)->getType());
                arrayMap.insert(std::pair<Value*, ArrayInfo*>(inst, info));
            }                // This is just a pointer declaration, need to capture malloc to get the size
            else if (alloca->getType()->getElementType()->isPointerTy()) {

                ArrayInfo* info = new struct ArrayInfo;
                info->top_limit = -1;
                info->bottom_limit = -1;
                info->dynSize = NULL;
                info->typeSize = dLayout.getTypeAllocSize(inst->getOperand(0)->getType());
                arrayMap.insert(std::pair<Value*, ArrayInfo*>(inst, info));
            }
        }

        void processCallInst(Instruction* inst, DataLayout &dLayout, TargetLibraryInfo &tlInfo) {
            CallInst* call = static_cast<CallInst*> ((Instruction*) inst);

            // Check for dynamic array declaration
            if (isAllocationFn(call, &tlInfo)) { // Assume it is dynamic array declaration with constant size

                Value* arraySize = getMallocArraySize(call, dLayout, &tlInfo);

                ConstantInt* size = NULL;
                if (arraySize->getValueID() == Value::ConstantIntVal)
                    size = static_cast<ConstantInt*> (getMallocArraySize(call, dLayout, &tlInfo));
                else {
                    Instruction* ttInst = static_cast<Instruction*> (arraySize);
                    arraySize = getInstLoad(ttInst);
                }

                std::stack<Value*> stack;
                stack.push(inst);
                while (stack.size() != 0) {

                    Value* tempValue = stack.top();
                    stack.pop();
                    //for(Value::use_iterator use = tempValue->use_begin(); use != tempValue->use_end(); ++use) {
                    for (Value::user_iterator use = tempValue->user_begin(); use != tempValue->user_end(); ++use) {
                        //   for ( user_iterator *use : tempValue->users()){
                        Instruction* tempInst = (Instruction*) (Value*) (*use);

                        if ((Value*) * use == tempValue)continue;


                        //Instruction* tempInst = dyn_cast<Instruction*>( use);
                        if (tempInst->getOpcode() == Instruction::Store) {
                            std::map<Value*, ArrayInfo*>::iterator it = arrayMap.find(tempInst->getOperand(1));
                            if (it != arrayMap.end()) {
                                if (size != NULL)
                                    it->second->top_limit = size->getSExtValue();
                                else
                                    it->second->dynSize = arraySize;
                                it->second->bottom_limit = 0;
                            } else {
#ifdef DEBUG
                                errs() << "Cannot find store target!\n";
#endif
                            }
                        } else
                            stack.push(*use);
                    }
                }
            }
        }

        void processGEP(Instruction* inst, Function &F) {
            unsigned num_array_elems;

            GetElementPtrInst *GEP = static_cast<GetElementPtrInst*> ((Instruction*) inst);
            std::map<Value*, ArrayInfo*>::iterator it = arrayMap.find(GEP->getPointerOperand());

            if (it != arrayMap.end()) {

                Type * ptr_type = GEP->getPointerOperandType()->getPointerElementType();
                num_array_elems = cast<ArrayType>(ptr_type) -> getNumElements();
                insertBoundCheck(inst, GEP, num_array_elems);

            }
            else {
                Instruction* tempInst = static_cast<Instruction*> (GEP->getPointerOperand());

                if (tempInst->getOpcode() == Instruction::Load) {
                    std::map<Value*, ArrayInfo*>::iterator it2 = arrayMap.find(tempInst->getOperand(0));
                    if (it2 != arrayMap.end()) {
                        if (it2->second->top_limit != -1)
                            insertBoundCheck(inst, GEP, it2->second->top_limit);
                        else if (it2->second->dynSize != NULL)
                            insertBoundCheck(inst, GEP, it2->second->dynSize);
                        else {
#ifdef DEBUG
                            errs() << "\tWARNING! Can not find the size of the array. Skip insert boundary check. " << GEP->getParent()->getName() << ", ";
                            GEP->dump();
#endif
                        }
                    } else {
#ifdef DEBUG
                        errs() << "\tWARNING 1! Haven't considered this case!\n";
#endif
                    }
                } else if (tempInst->getOpcode() == Instruction::GetElementPtr) {
                    GetElementPtrInst* GEP2 = static_cast<GetElementPtrInst*> (tempInst);
                    Instruction* tempInst2 = static_cast<Instruction*> (GEP2->getPointerOperand());

                    std::map<Value*, ArrayInfo*>::iterator it3 = arrayMap.find(tempInst2);
                    if (it3 != arrayMap.end()) {
                        Type * ptr_type = GEP2->getPointerOperandType()->getPointerElementType();
                        num_array_elems = it3->second->top_limit/cast<ArrayType>(ptr_type) -> getNumElements();
                        insertBoundCheck(inst, GEP, num_array_elems);
                    } else {
                        GetElementPtrInst* GEP3 = static_cast<GetElementPtrInst*> (tempInst2);
                        Instruction* tempInst3 = static_cast<Instruction*> (GEP3->getPointerOperand());

                        std::map<Value*, ArrayInfo*>::iterator it4 = arrayMap.find(tempInst3);
                        if (it4 != arrayMap.end()) {
                            if (it4->second->top_limit != -1)
                                insertBoundCheck(inst, GEP, it4->second->top_limit);
                            else if (it4->second->dynSize != NULL)
                                insertBoundCheck(inst, GEP, it4->second->dynSize);
                            else {
#ifdef DEBUG
                                errs() << "\tWARNING 2! Can not find the size of the array! ";
                                GEP->dump();
#endif
                            }
                        } else {
#ifdef DEBUG
                            errs() << "\tWARNING 3! Haven't considered this case!\n";
#endif
                        }
                    }
                }
            }

        }

        Value* getInstLoad(Value* value) {
            std::stack<Value*> stack;

            stack.push(value);
            while (!stack.empty()) {
                Value* tempValue = stack.top();
                stack.pop();

                if (LoadInst::classof(tempValue))
                    return tempValue;

                if (Instruction::classof(tempValue)) {
                    Instruction* tempInst = static_cast<Instruction*> (tempValue);
                    for (unsigned i = 0; i < tempInst->getNumOperands(); i++) {
                        stack.push(tempInst->getOperand(i));
                    }
                }
            }

            return NULL;
        }

        virtual void getAnalysisUsage(AnalysisUsage &AU) const {
            AU.setPreservesAll();
            AU.addRequired<TargetLibraryInfoWrapperPass>();
            //AU.addRequired<DataLayout>();
            //AU.addRequired<LoopInfo>();

            AU.addRequired<DominatorTreeWrapperPass>();

            AU.addRequired<DominanceFrontier>();
            AU.addRequired<PostDominatorTree>();
        }

        virtual bool runOnFunction(Function &F) {
            TD = &(F.getParent()->getDataLayout());
            BuilderTy TheBuilder(F.getContext(), TargetFolder(*TD));
            Builder = &TheBuilder;

            errs() << "\n=============================================\n";
            errs() << "[ runOnFunction - " << F.getName() << " ]\n";
            errs() << "=============================================\n";
            mincalltocheckinfo.clear();
            maxcalltocheckinfo.clear();
            arrayMap.clear();
            MCFGInst.clear();
            index.clear();
            counter = 0;
            indextocallmax.clear();
            indextocallmin.clear();
            indexdefmap.clear();


            Module* M = F.getParent();
            // Create meta data for instructions

            TargetLibraryInfo tlInfo = getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();

            DataLayout dLayout = M->getDataLayout();
            for (Function::iterator bb = F.begin(); bb != F.end(); ++bb) {
                for (BasicBlock::iterator inst = bb->begin(); inst != bb->end(); ++inst) {

                    if (inst->getOpcode() == Instruction::Alloca) { // Check for static array or pointer declaration
                        processAllocateInst(inst, dLayout);

                    } else if (inst->getOpcode() == Instruction::Call) {

                        processCallInst(inst, dLayout, tlInfo);

                    }
                }
            }



            // Insert bound checks
            errs() << "\n*** Start inserting bound checks ***\n";
            //long insertedChecks = 0;
            for (Function::iterator bb = F.begin(); bb != F.end(); ++bb) {
                for (BasicBlock::iterator inst = bb->begin(); inst != bb->end(); ++inst) {

                    if (inst->getOpcode() == Instruction::GetElementPtr) { // Check for pointer element access

                        Instruction * inst_copy = inst;



                        if (inst_copy) {

                            processGEP(inst, F);

                        }
#ifdef DEBUG
                        else {
                            errs() << "Do NOT insert checks for ";
                            inst->dump();
                        }
#endif
                    }
                }
            }

            //errs() << "[End runOnFunction] - inserted " << insertedChecks << " checks\n";
            //printCFG(F);

            dumpArrayInfoMap(&arrayMap);
            findindexwithgvn();
            std::vector<MCFG_Node*> MCFG;

            MCFGreset(MCFG, F);


            printresults("Naive");






            ///////////////////////////////////////////////////////
            // //debug index
            //errs()<<"Index set is:\n";
            //for(std::set<Value*>::iterator it=index.begin();it!=index.end();it++)
            //    errs()<<  *(Instruction*)*it<<"\n";
            //errs()<<"\n";
            //
            ////debug indexdefmap
            //errs()<<"indexdefmap set is:\n";
            //for(std::map<Value*,Value*>::iterator it=indexdefmap.begin();it!=indexdefmap.end();it++){
            //    errs()<<*(Instruction*)(*it).first<<"\t"<<*(Instruction*)(*it).second <<"\n";
            //}
            //errs()<<"\n";
            //// //debug MCFGinst
            //errs()<<"MinCall to check set is:\n";
            //for(std::map<CallInst*, Checks> ::iterator it=mincalltocheckinfo.begin();it!=mincalltocheckinfo.end();it++)
            //errs()<<  *((*it).first)<<*((*it).second.inst)<<"\n";
            //
            //errs()<<"MaxCall to check set is:\n";
            //for(std::map<CallInst*, Checks> ::iterator it=maxcalltocheckinfo.begin(); it!=maxcalltocheckinfo.end();it++)
            //errs()<<  *((*it).first)<<*((*it).second.inst)<<"\n";

            ////////////////////////////////////////////////////
            //printCFG(F);

            //printMCFG(MCFG);

            localopt(MCFG);
            //localoptsubsum(MCFG, F);




            //printCFG(F);
            //printMCFG(MCFG);

            resetcalltocheckinfo(F);
            MCFGreset(MCFG, F);
            printresults("Local");



            globalelimination(MCFG, F);
            resetcalltocheckinfo(F);
            MCFGreset(MCFG, F);


            //printMCFG(MCFG);

            printresults("Global");
           // printMCFG(MCFG);
            ///////////////////////////////


            //loopopt(F, MCFG);

            MCFGreset(MCFG, F);
            //printMCFG(MCFG);
            //printCFG(F);


            return false;
        }




        //////////////////////////////
        ///////////////////////////////

        void resetcalltocheckinfo(Function& F) {
            std::map<CallInst*, Checks> tmpmax, tmpmin;
            for (Function::iterator bbIt = F.begin(); bbIt != F.end(); bbIt++) {
                for (BasicBlock::iterator inst = bbIt->begin(); inst != bbIt->end(); inst++) {
                    if (isMaxcall(inst)) tmpmax[dyn_cast<CallInst>(inst)] = maxcalltocheckinfo[dyn_cast<CallInst>(inst)];
                    if (isMincall(inst)) tmpmin[dyn_cast<CallInst>(inst)] = mincalltocheckinfo[dyn_cast<CallInst>(inst)];
                }
            }
            maxcalltocheckinfo = tmpmax;
            mincalltocheckinfo = tmpmin;
        }

        void printresults(std::string a) {
            errs() << "Number of checks added after" << a << " stage:\t" << counter << "\n";
            // errs()<<mincalltocheckinfo.size()<<"\n";
//            for (std::map<CallInst*, Checks> ::iterator it = mincalltocheckinfo.begin(); it != mincalltocheckinfo.end(); it++) {
//                errs() << *((*it).first) << "\n"; //<<" added after line \n"<<getLine((*it).second.inst)<<" to check "<< *indexdefmap[((*it).second.index)]<<"\n";
//            }
//            for (std::map<CallInst*, Checks> ::iterator it = maxcalltocheckinfo.begin(); it != maxcalltocheckinfo.end(); it++) {
//                errs() << *((*it).first) << "\n"; //<<" added after line \n"<<getLine((*it).second.inst)<<" to check "<< *indexdefmap[((*it).second.index)]<<"\n";
//            }
            errs() << "\n\n";


        }

        void MCFGreset(std::vector<MCFG_Node*> &MCFG, Function& F) {

            MCFG.clear();
            MCFGInst.clear();

            for (std::map<CallInst*, Checks> ::iterator it = mincalltocheckinfo.begin(); it != mincalltocheckinfo.end(); it++) {
                MCFGInst.insert((*it).first);
            }
            for (std::map<CallInst*, Checks> ::iterator it = maxcalltocheckinfo.begin(); it != maxcalltocheckinfo.end(); it++) {
                MCFGInst.insert((*it).first);
            }

            for (std::map<Value*, ArrayInfo*>::iterator it = arrayMap.begin(); it != arrayMap.end(); it++) {
                MCFGInst.insert((Instruction*) it->first);
            }


            getMCFGInst(F);

            constructMCFG(F, MCFGInst, MCFG);

        }

        void loopopt(Function &F, std::vector<MCFG_Node*> &MCFG) {
            errs() << "--------------------Loop Optimization-------------------------\n";
            //Find Dominators
            std::map<MCFG_Node*, std::set<MCFG_Node*> > dominatorSet;
            getDominators(dominatorSet, MCFG);
#ifdef DEBUG_LOOP
            printDominators(dominatorSet);
#endif

            //Find Backedges
            std::set<std::pair<MCFG_Node*, MCFG_Node*> > backEdges;
            findBackEdges(backEdges, dominatorSet);
#ifdef DEBUG_LOOP
            printBackEdges(backEdges);
#endif
            //Find the loops in MCFG
            std::map<std::pair<MCFG_Node*, MCFG_Node*>, std::set<MCFG_Node*> > loops;
            findLoops(loops, backEdges, dominatorSet);
#ifdef DEBUG_LOOP
            printLoops(loops);
#endif
            //Preprocessed data from now on
            std::set<Instruction*> allInstrsOld;
            std::map<Value*, ComputedArrayIndex*> computedIndexes;
            getAllRelatedInstrs(MCFG, allInstrsOld, computedIndexes);
#ifdef DEBUG_LOOP
            printComputedIndexes(computedIndexes);
#endif
            //Find For loops
            std::map<MCFG_Node*, std::set<loopInfoBounds*> > loopMoves;
#ifdef DEBUG_LOOP
            printLoops(loops);
#endif
            findForMonoloticalLoops(loopMoves, loops, computedIndexes);
#ifdef DEBUG_LOOP
            printLoopMoves(loopMoves);
#endif
            mergeNestedLoopMoves(loopMoves);
            mergeNestedLoopMovesDuplicates(loopMoves);
#ifdef DEBUG_LOOP
            printLoopMovesAfterNestedMerge(loopMoves);
#endif
            moveCallsLoops(F, loopMoves, MCFG);

        }

        void localoptsubsum(std::vector<MCFG_Node*> &MCFG, Function& F) {
            errs() << "Starting Local Elimination..\n";
            std::set<Instruction*> tobeDeleted;
            for (Function::iterator it = F.begin(); it != F.end(); it++) {
                std::set<LightCheck> tmpset;
                for (BasicBlock::iterator it2 = (*it).begin(); it2 != (*it).end(); it2++) {
                    LightCheck first_ch;
                    first_ch.bottom_limit = mincalltocheckinfo[(dyn_cast<CallInst>(&*it2))].var.bottom_limit;
                    first_ch.top_limit = mincalltocheckinfo[(dyn_cast<CallInst>(&*it2))].var.top_limit;
                    first_ch.realindex = mincalltocheckinfo[(dyn_cast<CallInst>(&*it2))].index;
                    Instruction* instr = mincalltocheckinfo[(dyn_cast<CallInst>(&*it2))].inst;
                    bool deleted = false;
                    if (isMincall(&*it2))
                        if (tobeDeleted.find(&*it2) == tobeDeleted.end()) {
                            for (BasicBlock::iterator it3 = it2; it3 != (*it).end(); it3++) {
                                if (it2 == it3)continue;

                                if (isMincall(&*it3)) {
                                    LightCheck cur_ch;
                                    cur_ch.bottom_limit = mincalltocheckinfo[(dyn_cast<CallInst>(&*it3))].var.bottom_limit;
                                    cur_ch.top_limit = mincalltocheckinfo[(dyn_cast<CallInst>(&*it3))].var.top_limit;
                                    cur_ch.realindex = mincalltocheckinfo[(dyn_cast<CallInst>(&*it3))].index;

                                    if (identicalValue(first_ch.realindex, cur_ch.realindex)) {


                                        first_ch.bottom_limit = std::max(first_ch.bottom_limit, cur_ch.bottom_limit);
                                        first_ch.top_limit = std::min(first_ch.top_limit, cur_ch.top_limit);
                                        first_ch.realindex = first_ch.realindex;


                                        //deletefromMCFG(MCFG,*it2);
                                        //deletefromMCFG(MCFG,*it3);

                                        //(&*it3)->eraseFromParent();
                                        errs() << "added to delete: " << *first_ch.realindex << "\n";
                                        errs() << "Identical to  " << *cur_ch.realindex << "\n";
                                        tobeDeleted.insert(&*it3);
                                        //
                                        deleted = true;
                                        counter--;
                                        //errs()<<"Min added";
                                    }
                                }
                            }
                            if (deleted) {
                                AddMinCall((Instruction*) first_ch.realindex, instr, first_ch.top_limit, first_ch.bottom_limit);
                                tobeDeleted.insert(&*it2);
                            }
                        }
                }

            }
            MCFGreset(MCFG, F);


            //max

            for (Function::iterator it = F.begin(); it != F.end(); it++) {
                std::set<LightCheck> tmpset;
                for (BasicBlock::iterator it2 = (*it).begin(); it2 != (*it).end(); it2++) {
                    LightCheck first_ch;
                    first_ch.bottom_limit = maxcalltocheckinfo[(dyn_cast<CallInst>(&*it2))].var.bottom_limit;
                    first_ch.top_limit = maxcalltocheckinfo[(dyn_cast<CallInst>(&*it2))].var.top_limit;
                    first_ch.realindex = maxcalltocheckinfo[(dyn_cast<CallInst>(&*it2))].index;
                    Instruction* instr = maxcalltocheckinfo[(dyn_cast<CallInst>(&*it2))].inst;
                    bool deleted = false;
                    if (isMaxcall(&*it2))
                        if (tobeDeleted.find(&*it2) == tobeDeleted.end()) {
                            for (BasicBlock::iterator it3 = it2; it3 != (*it).end(); it3++) {
                                if (it2 == it3)continue;

                                if (isMaxcall(&*it3)) {
                                    LightCheck cur_ch;
                                    cur_ch.bottom_limit = maxcalltocheckinfo[(dyn_cast<CallInst>(&*it3))].var.bottom_limit;
                                    cur_ch.top_limit = maxcalltocheckinfo[(dyn_cast<CallInst>(&*it3))].var.top_limit;
                                    cur_ch.realindex = maxcalltocheckinfo[(dyn_cast<CallInst>(&*it3))].index;

                                    if (identicalValue(first_ch.realindex, cur_ch.realindex)) {

                                        first_ch.bottom_limit = std::max(first_ch.bottom_limit, cur_ch.bottom_limit);
                                        first_ch.top_limit = std::min(first_ch.top_limit, cur_ch.top_limit);
                                        first_ch.realindex = first_ch.realindex;


                                        //deletefromMCFG(MCFG,*it2);
                                        //deletefromMCFG(MCFG,*it3);

                                        //(&*it3)->eraseFromParent();
                                        tobeDeleted.insert(&*it3);
                                        //
                                        deleted = true;
                                        counter--;
                                        //errs()<<"Min added";
                                    }
                                }
                            }
                            if (deleted) {
                                AddMaxCall((Instruction*) first_ch.realindex, instr, first_ch.top_limit, first_ch.bottom_limit);
                                //(&*it2)->eraseFromParent();
                                tobeDeleted.insert(&*it2);
                            }
                        }
                }

            }

            for (auto it = tobeDeleted.begin(); it != tobeDeleted.end(); it++) {
                (*it)->eraseFromParent();

            }
            MCFGreset(MCFG, F);
            printMCFG(MCFG);
        }

        void localopt(std::vector<MCFG_Node*> &MCFG) {
            for (std::vector<MCFG_Node*>::iterator it = MCFG.begin(); it != MCFG.end(); it++) {
                std::set<LightCheck> tmpset;
                std::set<LightCheck> tmpvalset;
                for (std::vector<Instruction*>::iterator it2 = (*it)->instrs.begin(); it2 != (*it)->instrs.end(); it2++) {
                    if (dyn_cast<CallInst>(*it2)) {
                        if (isMincall(*it2)) {

                            Value* tmp = mincalltocheckinfo[(dyn_cast<CallInst>(*it2))].index;
                            //long curmin=mincalltocheckinfo[(dyn_cast<CallInst>(*it2))].var.bottom_limit;

                            //printMCFG(MCFG);
                            //errs()<<"for call: "<<*(Instruction*)*it2<<"\n";
                            // errs()<<"for call: "<<*(Instruction*)*it2<<"\n";
                            // errs()<<"index is: "<<*tmp<<"\n";
                            LightCheck ch;
                            //LghtCheck        chval;
                            ch.bottom_limit = mincalltocheckinfo[(dyn_cast<CallInst>(*it2))].var.bottom_limit;
                            ch.top_limit = mincalltocheckinfo[(dyn_cast<CallInst>(*it2))].var.top_limit;
                            ch.realindex = tmp;
                            //errs()<<"size "<<tmpset.size();
                            if (!find(tmpset, ch)) {
                                tmpset.insert(ch);
                                //errs()<<"insertd:";
                                //ch.printch();
                            }
                                //                            errs()<<*(Instruction*)tmp<<"    inserted to set\n";

                            else {
                                //                      errs()<<"Erased: "<<*(Instruction*)*it2<<"\n";
                                //std::set<LightCheck>::iterator a=tmpset.find(ch);

                                // errs()<<"found:";
                                //ch.printch();
                                //errs()<<"paired with:";
                                //a->printch();
                                //ool result=find(tmpset,ch);
                                //errs()<<"identical:"<<result<<"\n";
                                ((Instruction*) * it2)->eraseFromParent();

                                counter--;
                                indextocallmin.erase(indextocallmin.find((Instruction*) mincalltocheckinfo[(dyn_cast<CallInst>(*it2))].index));
                                mincalltocheckinfo.erase(mincalltocheckinfo.find((CallInst*) * it2));

                                //indextocall.erase(*it2);
                                MCFGInst.erase((Instruction*) * it2);
                                deletefromMCFG(MCFG, *it2);
                                continue;
                            }

                        }

                    }
                }
                //for(std::set<LightCheck>::iterator u=tmpset.begin();u!=tmpset.end();u++)
                //  (*u).printch();
            }

            for (std::vector<MCFG_Node*>::iterator it = MCFG.begin(); it != MCFG.end(); it++) {
                std::set<LightCheck> tmpset;
                std::set<LightCheck> tmpvalset;
                for (std::vector<Instruction*>::iterator it2 = (*it)->instrs.begin(); it2 != (*it)->instrs.end(); it2++) {
                    if (dyn_cast<CallInst>(*it2)) {
                        if (isMaxcall(*it2)) {

                            Value* tmp = maxcalltocheckinfo[(dyn_cast<CallInst>(*it2))].index;
                            //long curmax=maxcalltocheckinfo[(dyn_cast<CallInst>(*it2))].var.top_limit;

                            //printMCFG(MCFG);
                            //errs()<<"for call: "<<*(Instruction*)*it2<<"\n";
                            // errs()<<"for call: "<<*(Instruction*)*it2<<"\n";
                            // errs()<<"index is: "<<*tmp<<"\n";
                            LightCheck ch;
                            //  LightCheck        chval;
                            ch.bottom_limit = maxcalltocheckinfo[(dyn_cast<CallInst>(*it2))].var.bottom_limit;
                            ch.top_limit = maxcalltocheckinfo[(dyn_cast<CallInst>(*it2))].var.top_limit;
                            ch.realindex = tmp;

                            if (!find(tmpset, ch)) {
                                tmpset.insert(ch);
                            }
                                //                            errs()<<*(Instruction*)tmp<<"    inserted to set\n";

                            else if (!(dyn_cast<PHINode>(tmp))) {
                                //                                errs()<<"Erased: "<<*(Instruction*)*it2<<"\n";


                                ((Instruction*) * it2)->eraseFromParent();

                                counter--;

                                continue;
                            }



                        }

                    }
                }

            }


        }

        StringRef get_function_name(CallInst *call) {
            Function *fun = call->getCalledFunction();
            if (fun)
                return fun->getName();
            else
                return StringRef("indirect call");
        }

        bool isMincall(Instruction* inst) {

            if (dyn_cast<CallInst>(inst)) {
                std::string a = get_function_name((dyn_cast<CallInst>(inst)));
                std::string::size_type n;
                n = a.find("_min");
                if (n == std::string::npos) {
                    return false;
                } else
                    return true;
            }
            return false;
        }

        bool isMaxcall(Instruction* inst) {
            if (dyn_cast<CallInst>(inst)) {
                std::string a = get_function_name((dyn_cast<CallInst>(inst)));
                std::string::size_type n;
                n = a.find("_max");
                if (n == std::string::npos) {
                    return false;
                } else
                    return true;
            }
            return false;
        }

        void globalelimination(std::vector<MCFG_Node*> &MCFG, Function& F) {
            // printMCFG(MCFG);
            //init_gen(MCFG);



            //
            //printnodeinfo(MCFG);
            //
            //print kill and gen
            errs() << "/////////Backward Algorithm:\n\n";
            std::set<Instruction*> tobeDeleted;
            init_gen_bw(MCFG);
            init_kill_BW(MCFG);
            ComputeBackward(MCFG);

            ModifyMin(MCFG,tobeDeleted);
           
            
            
            ModifyMax(MCFG,tobeDeleted);
            for (auto it = tobeDeleted.begin(); it != tobeDeleted.end(); it++) {
                (*it)->eraseFromParent();

            }

            
            //printnodeinfo(MCFG);
            resetcalltocheckinfo(F);
            MCFGreset(MCFG, F);
            //printMCFG(MCFG);
            for (std::vector<MCFG_Node*>::iterator it = MCFG.begin(); it != MCFG.end(); it++) {
                (*it)->In_max.clear();
                (*it)->out_max.clear();
                (*it)->gen_max.clear();
                (*it)->kill_max.clear();
                (*it)->In_min.clear();
                (*it)->out_min.clear();
                (*it)->gen_min.clear();
                (*it)->kill_min.clear();
            }
            errs() << "/////////Forward Algorithm:\n\n";
            resetcalltocheckinfo(F);
            MCFGreset(MCFG, F);
            //printnodeinfo(MCFG);

            init_gen(MCFG);
            init_kill(MCFG);
            ComputeFirst(MCFG);
            std::set<Instruction*> tobeDeleted2;
            EliminateRedundanteMin(MCFG,tobeDeleted2);
            
            EliminateRedundanteMax(MCFG,tobeDeleted2);
            for (auto it = tobeDeleted2.begin(); it != tobeDeleted2.end(); it++) {
                (*it)->eraseFromParent();

            }
            resetcalltocheckinfo(F);
            MCFGreset(MCFG, F);
            //printMCFG(MCFG);
            //printnodeinfo(MCFG);
        }

        void ModifyMin(std::vector<MCFG_Node*>& MCFG,std::set<Instruction*>& tobeDeleted) {
            
            for (std::vector<MCFG_Node*>::iterator it = MCFG.begin(); it != MCFG.end(); it++) {
                std::set<Value*> tmpset;
                for (std::vector<Instruction*>::iterator it2 = (*it)->instrs.begin(); it2 != (*it)->instrs.end(); it2++) {
                    //bool deleted=false;
                    if (isMincall(*it2)) {


                        Value* tmp = indexdefmap[mincalltocheckinfo[(dyn_cast<CallInst>(*it2))].index];
                        Instruction* instbefore = mincalltocheckinfo[(dyn_cast<CallInst>(*it2))].inst;
                        Instruction* curcall = (dyn_cast<Instruction>(*it2));
                        Instruction* curindex = dyn_cast<Instruction>(mincalltocheckinfo[(dyn_cast<CallInst>(*it2))].index);

                        LightCheck rch;
                        rch.bottom_limit = mincalltocheckinfo[(dyn_cast<CallInst>(*it2))].var.bottom_limit;
                        rch.top_limit = mincalltocheckinfo[(dyn_cast<CallInst>(*it2))].var.top_limit;

                        rch.realindex = tmp;
                        std::set<LightCheck> out = (*it)->out_min;
                        for (std::set<LightCheck>::iterator oit = out.begin(); oit != out.end(); oit++) {
                            if (LightCheck::identicalValue(rch.realindex, (*oit).realindex)) {

                                if ((rch.bottom_limit) < ((*oit).bottom_limit)) {

                                    //errs()<<"Modified min\n";
                                    //rch.printch();
                                    //oit->printch();
                                    rch.bottom_limit = (*oit).bottom_limit;
                                    AddMinCall(curindex, instbefore, rch.top_limit, rch.bottom_limit);
                                    //deletefromMCFG(MCFG,curcall);
                                    //curcall->eraseFromParent();
                                    //deleted=true;
                                    tobeDeleted.insert(curcall);
                                }
                            }
                        }



                    }
                }
            }

        }

        void ModifyMax(std::vector<MCFG_Node*>& MCFG,std::set<Instruction*>& tobeDeleted) {
            // debug calltochcheckinfo
        
            for (std::vector<MCFG_Node*>::iterator it = MCFG.begin(); it != MCFG.end(); it++) {
                std::set<Value*> tmpset;
                for (std::vector<Instruction*>::iterator it2 = (*it)->instrs.begin(); it2 != (*it)->instrs.end(); it2++) {
                    if (isMaxcall(*it2)) {

                        Instruction* instbefore = maxcalltocheckinfo[(CallInst*) * it2].inst;
                        //errs()<<"man: "<<(maxcalltocheckinfo[(CallInst*)*it2].inst)<<"\n";
                        Instruction* curcall = (dyn_cast<Instruction>(*it2));

                        Instruction* curindex = dyn_cast<Instruction>(maxcalltocheckinfo[(CallInst*) * it2].index);
                        Value* tmp = indexdefmap[curindex];
                        //errs()<<"curindex is : " <<*curindex;
                        LightCheck rch;
                        rch.bottom_limit = maxcalltocheckinfo[(dyn_cast<CallInst>(*it2))].var.bottom_limit;
                        rch.top_limit = maxcalltocheckinfo[(dyn_cast<CallInst>(*it2))].var.top_limit;
                        rch.realindex = tmp;

                        // rch.printch();
                        std::set<LightCheck> out = (*it)->out_max;
                        for (std::set<LightCheck>::iterator oit = out.begin(); oit != out.end(); oit++) {

                            if (LightCheck::identicalValue(rch.realindex, (*oit).realindex)) {

                                if (rch.top_limit > (*oit).top_limit) {
                                    rch.top_limit = (*oit).top_limit;
                                    //errs()<<"Modified max\n";
                                    //rch.printch();
                                    //oit->printch();

                                    AddMaxCall(curindex, instbefore, rch.top_limit, rch.bottom_limit);
                                    //errs()<<"Modified!!!!\n";
                                    //deletefromMCFG(MCFG,curcall);
                                    //curcall->eraseFromParent();
                                    tobeDeleted.insert(curcall);
                                }
                            }
                        }



                    }
                }
            }

        }

        void EliminateRedundanteMin(std::vector<MCFG_Node*>& MCFG,std::set<Instruction*>& tobeDeleted) {
            
            for (std::vector<MCFG_Node*>::iterator it = MCFG.begin(); it != MCFG.end(); it++) {
                std::set<Value*> tmpset;

                for (std::vector<Instruction*>::iterator it2 = (*it)->instrs.begin(); it2 != (*it)->instrs.end(); it2++) {
                    if (isMincall(*it2)) {

                        Value* tmp = indexdefmap[mincalltocheckinfo[(dyn_cast<CallInst>(*it2))].index];
                        //Instruction* instbefore=mincalltocheckinfo[(dyn_cast<CallInst>(*it2))].inst;
                        //Instruction* curcall=(dyn_cast<Instruction>(*it2));
                        //Instruction* curindex=dyn_cast<Instruction>(mincalltocheckinfo[(dyn_cast<CallInst>(*it2))].index);

                        LightCheck rch;
                        rch.bottom_limit = mincalltocheckinfo[(dyn_cast<CallInst>(*it2))].var.bottom_limit;
                        rch.top_limit = mincalltocheckinfo[(dyn_cast<CallInst>(*it2))].var.top_limit;

                        rch.realindex = tmp;
                        std::set<LightCheck> in = (*it)->In_min;

                        for (std::set<LightCheck>::iterator iit = in.begin(); iit != in.end(); iit++) {
                            if (LightCheck::identicalValue(rch.realindex, (*iit).realindex)) {

                                if ((rch.bottom_limit) <= ((*iit).bottom_limit)) {
                                    counter--;
                                    //indextocallmin.erase(indextocallmin.find((Instruction*)mincalltocheckinfo[(dyn_cast<CallInst>(*it2))].index));
                                    //mincalltocheckinfo.erase(mincalltocheckinfo.find((CallInst*)*it2));

                                    //MCFGInst.erase((Instruction*)*it2);

                                    //deletefromMCFG(MCFG,*it2);
                                    //((Instruction*)*it2)->eraseFromParent();
                                    tobeDeleted.insert(*it2);
                                }
                            }
                        }
                    }
                }
            }
        }

        void EliminateRedundanteMax(std::vector<MCFG_Node*>& MCFG,std::set<Instruction*>& tobeDeleted) {
    
            for (std::vector<MCFG_Node*>::iterator it = MCFG.begin(); it != MCFG.end(); it++) {
                std::set<Value*> tmpset;
                for (std::vector<Instruction*>::iterator it2 = (*it)->instrs.begin(); it2 != (*it)->instrs.end(); it2++) {
                    if (isMaxcall(*it2)) {


                        //errs()<<"man: "<<(maxcalltocheckinfo[(CallInst*)*it2].inst)<<"\n";
                        //Instruction* curcall=(dyn_cast<Instruction>(*it2));

                        Instruction* curindex = dyn_cast<Instruction>(maxcalltocheckinfo[(CallInst*) * it2].index);
                        Value* tmp = indexdefmap[curindex];
                        //errs()<<"curindex is : " <<*curindex;
                        LightCheck rch;
                        rch.bottom_limit = maxcalltocheckinfo[(dyn_cast<CallInst>(*it2))].var.bottom_limit;
                        rch.top_limit = maxcalltocheckinfo[(dyn_cast<CallInst>(*it2))].var.top_limit;
                        rch.realindex = tmp;

                        // rch.printch();
                        std::set<LightCheck> in = (*it)->In_max;
                        for (std::set<LightCheck>::iterator iit = in.begin(); iit != in.end(); iit++) {
                            if (LightCheck::identicalValue(rch.realindex, (*iit).realindex)) {

                                if (rch.top_limit >= (*iit).top_limit) {
                                    counter--;
                                    //errs()<<indextocallmax.find((Instruction*)maxcalltocheckinfo[(dyn_cast<CallInst>(*it2))].index)<<"\n";
                                    //indextocallmax.erase(indextocallmax.find((Instruction*)maxcalltocheckinfo[(dyn_cast<CallInst>(*it2))].index));
                                    // maxcalltocheckinfo.erase(maxcalltocheckinfo.find((CallInst*)*it2));

                                    //MCFGInst.erase((Instruction*)*it2);
                                    //deletefromMCFG(MCFG,*it2);
                                    //((Instruction*)*it2)->eraseFromParent();
                                    tobeDeleted.insert(*it2);
                                }
                            }
                        }
                    }
                }
            }
            
        }

        void ComputeBackward(std::vector<MCFG_Node*>& MCFG) {

            errs() << "Calculation Backward:\n";
            //min
            int changed = 0;
            //for(std::vector<MCFG_Node*>::iterator it=MCFG.begin();it!=MCFG.end();it++){
            //    MCFG_Node* curNode = *it;
            //    myunion2(&(curNode->out_min),&(curNode->gen_min));
            //    myintersect2(&(curNode->out_min),&(curNode->kill_min));
            //}

            do {
                changed = 0;
                for (std::vector<MCFG_Node*>::iterator it = MCFG.begin(); it != MCFG.end(); it++) {
                    MCFG_Node* curNode = *it;
                    std::set<LightCheck> oldOut = curNode->In_min;

                    if ((*it)->succs.size() > 0)curNode->out_min = (*((*it)->succs.begin()))->In_min;

                    for (std::set<MCFG_Node*>::iterator it2 = (*it)->succs.begin(); it2 != (*it)->succs.end(); it2++) {

                        myintersect2(&(curNode->out_min), &((*it2)->In_min));
                    }
                    //printnodeinfo(MCFG);

                    //errs()<<"before calc\n";
                    //
                    std::set<LightCheck> curout = curNode->out_min;

                    //mysubtract2(&(curout),&(curNode->kill_min));
                    //errs()<<"after sub\n";
                    myunion2(&(curout), &(curNode->gen_min));
                    //errs()<<"after union\n";
                    //printnodeinfo(MCFG);
                    curNode->In_min = curout;


                    for (std::set<LightCheck>::iterator it = curNode->In_min.begin(); it != curNode->In_min.end(); ++it) {
                        if (!find(oldOut, *it)) {
                            changed = 1;
                            ///errs()<<**it<<"here1\n";
                        }

                    }
                    for (std::set<LightCheck>::iterator it = oldOut.begin(); it != oldOut.end(); ++it) {
                        if (!find(curNode->In_min, *it)) {
                            changed = 1;
                            //errs()<<**it<<"here2\n";
                        }
                    }



                }

            } while (changed == 1);


            //max

            changed = 0;
            do {
                changed = 0;
                for (std::vector<MCFG_Node*>::iterator it = MCFG.begin(); it != MCFG.end(); it++) {
                    MCFG_Node* curNode = *it;
                    std::set<LightCheck> oldOut = curNode->In_max;

                    if ((*it)->succs.size() > 0)curNode->out_max = (*((*it)->succs.begin()))->In_max;

                    for (std::set<MCFG_Node*>::iterator it2 = (*it)->succs.begin(); it2 != (*it)->succs.end(); it2++) {

                        myintersect2(&(curNode->out_max), &((*it2)->In_max));
                    }

                    std::set<LightCheck> curout = curNode->out_max;

                    mysubtract2(&(curout), &(curNode->kill_max));
                    //errs()<<"after sub\n";
                    myunion2(&(curout), &(curNode->gen_max));
                    //errs()<<"after union\n";
                    //printnodeinfo(MCFG);
                    curNode->In_max = curout;


                    for (std::set<LightCheck>::iterator it = curNode->In_max.begin(); it != curNode->In_max.end(); ++it) {
                        if (!find(oldOut, *it)) {
                            changed = 1;
                            ///errs()<<**it<<"here1\n";
                        }

                    }
                    for (std::set<LightCheck>::iterator it = oldOut.begin(); it != oldOut.end(); ++it) {
                        if (!find(curNode->In_max, *it)) {
                            changed = 1;
                            //errs()<<**it<<"here2\n";
                        }
                    }



                }


            } while (changed == 1);



        }

        void ComputeFirst(std::vector<MCFG_Node*>& MCFG) {

            errs() << "Calculation Forward:\n";
            //min
            int changed = 0;
            do {
                changed = 0;
                for (std::vector<MCFG_Node*>::iterator it = MCFG.begin(); it != MCFG.end(); it++) {
                    MCFG_Node* curNode = *it;
                    std::set<LightCheck> oldOut = curNode->out_min;

                    if ((*it)->preds.size() > 0)curNode->In_min = (*((*it)->preds.begin()))->out_min;

                    for (std::set<MCFG_Node*>::iterator it2 = (*it)->preds.begin(); it2 != (*it)->preds.end(); it2++) {

                        myintersect(&(curNode->In_min), &((*it2)->out_min));
                    }
                    //printnodeinfo(MCFG);



                    //errs()<<"before calc\n";
                    //
                    std::set<LightCheck> curIn = curNode->In_min;

                    mysubtract2(&(curIn), &(curNode->kill_min));
                    //errs()<<"after sub\n";
                    myunion(&(curIn), &(curNode->gen_min));
                    //errs()<<"after union\n";
                    //printnodeinfo(MCFG);
                    curNode->out_min = curIn;


                    for (std::set<LightCheck>::iterator it = curNode->out_min.begin(); it != curNode->out_min.end(); ++it) {
                        if (oldOut.find(*it) == oldOut.end()) {
                            changed = 1;
                            ///errs()<<**it<<"here1\n";
                        }

                    }
                    for (std::set<LightCheck>::iterator it = oldOut.begin(); it != oldOut.end(); ++it) {
                        if (curNode->out_min.find(*it) == curNode->out_min.end()) {
                            changed = 1;
                            //errs()<<**it<<"here2\n";
                        }
                    }



                }


            } while (changed == 1);

            //max
            changed = 0;
            do {
                changed = 0;
                for (std::vector<MCFG_Node*>::iterator it = MCFG.begin(); it != MCFG.end(); it++) {
                    MCFG_Node* curNode = *it;
                    std::set<LightCheck> oldOut = curNode->out_max;

                    if ((*it)->preds.size() > 0)curNode->In_max = (*((*it)->preds.begin()))->out_max;

                    for (std::set<MCFG_Node*>::iterator it2 = (*it)->preds.begin(); it2 != (*it)->preds.end(); it2++) {

                        myintersect(&(curNode->In_max), &((*it2)->out_max));
                    }
                    //printnodeinfo(MCFG);



                    //errs()<<"before calc\n";
                    //
                    std::set<LightCheck> curIn = curNode->In_max;

                    mysubtract2(&(curIn), &(curNode->kill_max));
                    //errs()<<"after sub\n";
                    myunion(&(curIn), &(curNode->gen_max));
                    //errs()<<"after union\n";
                    //printnodeinfo(MCFG);
                    curNode->out_max = curIn;

                    //printnodeinfo(MCFG);
                    for (std::set<LightCheck>::iterator it = curNode->out_max.begin(); it != curNode->out_max.end(); ++it) {
                        if (oldOut.find(*it) == oldOut.end()) {
                            changed = 1;
                            //errs()<<BB->getName()<<"here1\n";
                        }

                    }
                    for (std::set<LightCheck>::iterator it = oldOut.begin(); it != oldOut.end(); ++it) {
                        if (curNode->out_max.find(*it) == curNode->out_max.end()) {
                            changed = 1;
                            //errs()<<BB->getName()<<"here1\n";
                        }
                    }



                }


            } while (changed == 1);



            //

        }

        void printnodeinfo(std::vector<MCFG_Node*>& MCFG) {
            errs() << "\n\nNODE INFO MIN:\n";
            for (std::vector<MCFG_Node*>::iterator it = MCFG.begin(); it != MCFG.end(); it++) {
                errs() << (*it)->label << "\n";
                errs() << "Gen:\n";

                for (std::set<LightCheck>::iterator it2 = (*it)->gen_min.begin(); it2 != (*it)->gen_min.end(); it2++) {
                    errs() << *(*it2).realindex << " (" << (*it2).bottom_limit << "," << (*it2).top_limit << ")\t" << *(*it2).fakeindex << "\n";
                }
                errs() << "Kill:\n";

                for (std::set<LightCheck>::iterator it2 = (*it)->kill_min.begin(); it2 != (*it)->kill_min.end(); it2++) {
                    errs() << *(*it2).realindex << " (" << (*it2).bottom_limit << "," << (*it2).top_limit << ")\t" << *(*it2).fakeindex << "\n";
                }
                errs() << "In:\n";

                for (std::set<LightCheck>::iterator it2 = (*it)->In_min.begin(); it2 != (*it)->In_min.end(); it2++) {
                    errs() << *(*it2).realindex << " (" << (*it2).bottom_limit << "," << (*it2).top_limit << ")\t" << *(*it2).fakeindex << "\n";
                }
                errs() << "out:\n";

                for (std::set<LightCheck>::iterator it2 = (*it)->out_min.begin(); it2 != (*it)->out_min.end(); it2++) {
                    errs() << *(*it2).realindex << " (" << (*it2).bottom_limit << "," << (*it2).top_limit << ")\t" << *(*it2).fakeindex << "\n";
                }
            }
            errs() << "NODE INFO Max:\n";


            for (std::vector<MCFG_Node*>::iterator it = MCFG.begin(); it != MCFG.end(); it++) {
                errs() << (*it)->label << "\n";
                errs() << "Gen:\n";

                for (std::set<LightCheck>::iterator it2 = (*it)->gen_max.begin(); it2 != (*it)->gen_max.end(); it2++) {
                    errs() << *(*it2).realindex << " (" << (*it2).bottom_limit << "," << (*it2).top_limit << ")\t" << *(*it2).fakeindex << "\n";
                }
                errs() << "Kill:\n";

                for (std::set<LightCheck>::iterator it2 = (*it)->kill_max.begin(); it2 != (*it)->kill_max.end(); it2++) {
                    errs() << *(*it2).realindex << " (" << (*it2).bottom_limit << "," << (*it2).top_limit << ")\t" << *(*it2).fakeindex << "\n";

                }
                errs() << "In:\n";

                for (std::set<LightCheck>::iterator it2 = (*it)->In_max.begin(); it2 != (*it)->In_max.end(); it2++) {
                    errs() << *(*it2).realindex << " (" << (*it2).bottom_limit << "," << (*it2).top_limit << ")\t" << *(*it2).fakeindex << "\n";

                }
                errs() << "out:\n";

                for (std::set<LightCheck>::iterator it2 = (*it)->out_max.begin(); it2 != (*it)->out_max.end(); it2++) {
                    errs() << *(*it2).realindex << " (" << (*it2).bottom_limit << "," << (*it2).top_limit << ")\t" << *(*it2).fakeindex << "\n";

                }
            }
        }

        void subsumeGen(std::set<LightCheck>* out) {
            //std::set<LightCheck>* chs;
            for (std::set<LightCheck>::iterator it = out->begin(); it != out->end(); ++it) {
                for (std::set<LightCheck>::iterator it2 = it; it2 != out->end(); ++it2) {
                    if (it == it2)continue;
                    if (LightCheck::identicalValue((*it).realindex, (*it2).realindex)) {
                        //LightCheck ch;
                        (*it).bottom_limit = std::max((*it).bottom_limit, (*it2).bottom_limit);
                        (*it).top_limit = std::min((*it).top_limit, (*it2).top_limit);
                        (*it).realindex = (*it).realindex;
                        out->erase(it2);

                        break;
                    }

                }
            }
        }

        void myintersect(std::set<LightCheck>* out, std::set<LightCheck>* in) {

            for (std::set<LightCheck>::iterator it = out->begin(); it != out->end(); ++it) {

                //if(in->find(*it)==in->end()){
                if (!find(in, *it)) {
                    erase(out, *it);

                }

            }
        }

        void myintersect2(std::set<LightCheck>* out, std::set<LightCheck>* in) {

            for (std::set<LightCheck>::iterator it = out->begin(); it != out->end(); ++it) {
                int found = 0;
                for (std::set<LightCheck>::iterator it2 = in->begin(); it2 != in->end(); ++it2) {
                    //if(in->find(*it)==in->end()){

                    if (LightCheck::identicalValue((*it).realindex, (*it2).realindex)) {
                        (*it).bottom_limit = std::min((*it).bottom_limit, (*it2).bottom_limit);
                        (*it).top_limit = std::max((*it).top_limit, (*it2).top_limit);
                        (*it).realindex = (*it).realindex;
                        found = 1;
                    }

                }
                if (found == 0) {
                    out->erase(it);
                }
            }
        }

        void mysubtract(std::set<LightCheck>* out, std::set<LightCheck>* in) {

            for (std::set<LightCheck>::iterator it = in->begin(); it != in->end(); ++it) {
                //if(out->find(*it)!=out->end() ){
                if (find(out, *it)) {
                    //errs()<< "\n"<<(*it)->getName()<<" ";
                    erase(out, *it);


                }

            }
        }

        void mysubtract2(std::set<LightCheck>* out, std::set<LightCheck>* in) {

            for (std::set<LightCheck>::iterator it = out->begin(); it != out->end(); ++it) {
                int found = 0;
                for (std::set<LightCheck>::iterator it2 = in->begin(); it2 != in->end(); ++it2) {
                    //if(in->find(*it)==in->end()){

                    if (LightCheck::identicalValue((*it).realindex, (*it2).realindex)) {
                        found = 1;

                    }
                }
                if (found == 1) {
                    out->erase(it);
                }
            }
        }

        void myunion(std::set<LightCheck>* out, std::set<LightCheck>* in) {

            for (std::set<LightCheck>::iterator it = in->begin(); it != in->end(); ++it) {


                out->insert(*it);


            }
        }

        void myunion2(std::set<LightCheck>* out, std::set<LightCheck>* in) {

            for (std::set<LightCheck>::iterator it = in->begin(); it != in->end(); ++it) {
                int found = 0;
                for (std::set<LightCheck>::iterator it2 = out->begin(); it2 != out->end(); ++it2) {
                    //if(in->find(*it)==in->end()){

                    if (LightCheck::identicalValue((*it).realindex, (*it2).realindex)) {
                        (*it2).bottom_limit = std::max((*it).bottom_limit, (*it2).bottom_limit);
                        (*it2).top_limit = std::min((*it).top_limit, (*it2).top_limit);
                        //(*it2).realindex=(*it).realindex;
                        found = 1;
                    }

                }
                if (found == 0) {
                    out->insert(*it);
                }

            }
        }

        bool find(std::set<LightCheck> setcheck, LightCheck check) {

            for (std::set<LightCheck>::iterator it = setcheck.begin(); it != setcheck.end(); it++) {
                //errs()<<"check:\n" ;(*it).printch();
                //check.printch();
                LightCheck a = (LightCheck) (*it);
                if (a == check) {
                    return true;
                    // errs()<<"trueeee\n";
                }

            }

            return false;
        }

        bool find(std::set<LightCheck>* setcheck, const LightCheck check) {

            for (std::set<LightCheck>::iterator it = setcheck->begin(); it != setcheck->end(); it++) {
                //errs()<<"check:\n" ;(*it).printch();
                //check.printch();
                LightCheck a = (LightCheck) (*it);
                if (a == check) {
                    return true;
                    // errs()<<"trueeee\n";
                }

            }

            return false;
        }

        bool insert(std::set<LightCheck>* setcheck, const LightCheck check) {

            if (!find(setcheck, check))
                setcheck->insert(check);

            return false;
        }

        void erase(std::set<LightCheck>* setcheck, LightCheck check) {
            for (std::set<LightCheck>::iterator it = setcheck->begin(); it != setcheck->end(); it++) {
                LightCheck a = (LightCheck) (*it);
                if (a == check) {
                    setcheck->erase(it);
                    break;
                }
            }
        }
        ////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////

        void init_gen_bw(std::vector<MCFG_Node*> &MCFG) {
            errs() << "BW GEN GENERATING :\n";
            for (std::vector<MCFG_Node*>::iterator it = MCFG.begin(); it != MCFG.end(); it++) {

                for (std::vector<Instruction*>::iterator it2 = (*it)->instrs.begin(); it2 != (*it)->instrs.end(); it2++) {
                    if (isMincall(*it2)) {
                        //  errs()<<"\nfor call: "<<**it2<<"\n";
                        Value* tmp = indexdefmap[mincalltocheckinfo[(dyn_cast<CallInst>(*it2))].index];
                        int flag = 0;
                        // errs()<<**it2<<"\t";
                        // errs()<<*tmp<<"\n";
                        for (std::vector<Instruction*>::iterator it3 = it2; it3 != (*it)->instrs.begin(); it3--) {
                            //errs()<<*(Instruction*)*it3<<"\n";
                            if (dyn_cast<StoreInst>(*it3)) {

                                if ((dyn_cast<StoreInst>(*it3))->getPointerOperand() == tmp) {
                                    //errs()<<"store: "<< *(Instruction*)*it3<<"\t";
                                    // errs()<<"effect:"<<getEffect(*it3)<<"\n";
                                    if ((getEffect(*it3) == CHANGED) ||
                                            (getEffect(*it3) == INC) ||
                                            (getEffect(*it3) == MUL) ||
                                            (getEffect(*it3) == DIVLT1)
                                            ) {
                                        flag = 1;
                                        //errs()<<getEffect(*it3)<<"\n";
                                        //errs()<<"changed!!"<<"\n";
                                        break;
                                    }

                                }


                            }
                        }
                        if (flag == 0) {
                            LightCheck ch;
                            ch.bottom_limit = mincalltocheckinfo[(dyn_cast<CallInst>(*it2))].var.bottom_limit;
                            ch.top_limit = mincalltocheckinfo[(dyn_cast<CallInst>(*it2))].var.top_limit;
                            //ch.realindex=mincalltocheckinfo[(dyn_cast<CallInst>(*it2))].index;
                            ch.realindex = tmp;
                            ch.fakeindex = tmp;
                            (*it)->gen_min.insert(ch);
                        }

                    }
                }
                subsumeGen(&(*it)->gen_min);

            }



            //max
            for (std::vector<MCFG_Node*>::iterator it = MCFG.begin(); it != MCFG.end(); it++) {

                for (std::vector<Instruction*>::iterator it2 = (*it)->instrs.begin(); it2 != (*it)->instrs.end(); it2++) {
                    if (isMaxcall(*it2)) {
                        //  errs()<<"\nfor call: "<<**it2<<"\n";
                        Value* tmp = indexdefmap[maxcalltocheckinfo[(dyn_cast<CallInst>(*it2))].index];
                        int flag = 0;
                        //errs()<<**it2<<"\t";
                        //errs()<<*tmp<<"\n";
                        for (std::vector<Instruction*>::iterator it3 = it2; it3 != (*it)->instrs.begin(); it3--) {
                            //errs()<<*(Instruction*)*it3<<"\n";
                            if (dyn_cast<StoreInst>(*it3)) {
                                //errs()<<"store: "<< *(Instruction*)*it3<<"\n";
                                if ((dyn_cast<StoreInst>(*it3))->getPointerOperand() == tmp) {
                                    //errs()<<"store: "<< *(Instruction*)*it3<<"\t";
                                    //errs()<<"effect:"<<getEffect(*it3)<<"\n";
                                    if ((getEffect(*it3) == CHANGED) ||
                                            (getEffect(*it3) == DEC) ||
                                            (getEffect(*it3) == DIVGT1)) {
                                        flag = 1;
                                        //errs()<<getEffect(*it3)<<"\n";
                                        //  errs()<<"changed!!"<<"\n";
                                        flag = 1;
                                        break;
                                    }

                                }


                            }
                        }
                        if (flag == 0) {
                            LightCheck ch;
                            ch.bottom_limit = maxcalltocheckinfo[(dyn_cast<CallInst>(*it2))].var.bottom_limit;
                            ch.top_limit = maxcalltocheckinfo[(dyn_cast<CallInst>(*it2))].var.top_limit;
                            //ch.realindex=maxcalltocheckinfo[(dyn_cast<CallInst>(*it2))].index;
                            ch.realindex = tmp;
                            ch.fakeindex = tmp;
                            (*it)->gen_max.insert(ch);
                        }

                    }

                }
                subsumeGen(&(*it)->gen_max);
            }


        }






        ///////////////////////////////////////////////////////////
        /////////////////////////////////////////////////////////

        void init_gen(std::vector<MCFG_Node*> &MCFG) {
            errs() << "GEN GENERATING:\n";

            for (std::vector<MCFG_Node*>::iterator it = MCFG.begin(); it != MCFG.end(); it++) {

                for (std::vector<Instruction*>::iterator it2 = (*it)->instrs.begin(); it2 != (*it)->instrs.end(); it2++) {
                    if (isMincall(*it2)) {
                        //errs()<<**it2<<"\n";
                        Value* tmp = indexdefmap[mincalltocheckinfo[(dyn_cast<CallInst>(*it2))].index];
                        int flag = 0;
                        //errs()<<**it2<<"\n";
                        //errs()<<*tmp<<"\n\n";
                        for (std::vector<Instruction*>::iterator it3 = it2; it3 != (*it)->instrs.end(); it3++) {
                            //errs()<<*(Instruction*)*it3<<"\n";
                            if (dyn_cast<StoreInst>(*it3)) {
                                // errs()<<"store: "<< *(Instruction*)*it3<<"\n";
                                if ((dyn_cast<StoreInst>(*it3))->getPointerOperand() == tmp) {
                                    // errs()<<getEffect(*it3)<<"\n";
                                    if ((getEffect(*it3) == CHANGED) ||
                                            (getEffect(*it3) == DEC) ||
                                            (getEffect(*it3) == DIVGT1)) {
                                        flag = 1;
                                        break;
                                    }

                                }


                            }
                        }
                        if (flag == 0) {
                            LightCheck ch;
                            ch.bottom_limit = mincalltocheckinfo[(dyn_cast<CallInst>(*it2))].var.bottom_limit;
                            ch.top_limit = mincalltocheckinfo[(dyn_cast<CallInst>(*it2))].var.top_limit;
                            //ch.realindex=mincalltocheckinfo[(dyn_cast<CallInst>(*it2))].index;
                            ch.realindex = tmp;
                            ch.fakeindex = tmp;
                            (*it)->gen_min.insert(ch);
                        }

                    }
                }
            }


            //max
            for (std::vector<MCFG_Node*>::iterator it = MCFG.begin(); it != MCFG.end(); it++) {

                for (std::vector<Instruction*>::iterator it2 = (*it)->instrs.begin(); it2 != (*it)->instrs.end(); it2++) {
                    if (isMaxcall(*it2)) {
                        Value* tmp = indexdefmap[maxcalltocheckinfo[(dyn_cast<CallInst>(*it2))].index];
                        int flag = 0;
                        //errs()<<**it2<<"\n";
                        //errs()<<*tmp<<"\n\n";
                        for (std::vector<Instruction*>::iterator it3 = it2; it3 != (*it)->instrs.end(); it3++) {
                            //errs()<<*(Instruction*)*it3<<"\n";
                            if (dyn_cast<StoreInst>(*it3)) {
                                //errs()<<"store: "<< *(Instruction*)*it3<<"\n";
                                if ((dyn_cast<StoreInst>(*it3))->getPointerOperand() == tmp) {
                                    // errs()<<getEffect(*it3)<<"\n";
                                    if ((getEffect(*it3) == CHANGED) ||
                                            (getEffect(*it3) == INC) ||
                                            (getEffect(*it3) == MUL) ||
                                            (getEffect(*it3) == DIVLT1)
                                            ) {
                                        flag = 1;
                                        break;
                                    }

                                }


                            }
                        }
                        if (flag == 0) {
                            LightCheck ch;
                            ch.bottom_limit = maxcalltocheckinfo[(dyn_cast<CallInst>(*it2))].var.bottom_limit;
                            ch.top_limit = maxcalltocheckinfo[(dyn_cast<CallInst>(*it2))].var.top_limit;
                            //ch.realindex=maxcalltocheckinfo[(dyn_cast<CallInst>(*it2))].index;
                            ch.realindex = tmp;
                            ch.fakeindex = tmp;
                            (*it)->gen_max.insert(ch);
                        }

                    }
                }
            }
        }

        void init_kill(std::vector<MCFG_Node*> &MCFG) {
            errs() << "\nKILL GENERATING:\n";
            for (std::map<CallInst*, Checks>::iterator c = mincalltocheckinfo.begin(); c != mincalltocheckinfo.end(); c++) {
                Value* call = c->first;
                Value* tmp = indexdefmap[mincalltocheckinfo[(dyn_cast<CallInst>(call))].index];

                //errs()<<*call<<"\n";
                for (std::vector<MCFG_Node*>::iterator it = MCFG.begin(); it != MCFG.end(); it++) {
                    for (std::vector<Instruction*>::iterator it2 = (*it)->instrs.begin(); it2 != (*it)->instrs.end(); it2++) {

                        if (dyn_cast<StoreInst>(*it2)) {
                            // errs()<<*tmp<<"   "<<**it2<<"\n";
                            if ((dyn_cast<StoreInst>(*it2))->getPointerOperand() == tmp) {
                                if ((getEffect(*it2) == CHANGED) ||
                                        (getEffect(*it2) == DEC) ||
                                        (getEffect(*it2) == DIVGT1)) {
                                    //errs()<<"here\n";
                                    LightCheck ch;
                                    ch.bottom_limit = 0;
                                    ch.top_limit = 0;
                                    //ch.realindex=mincalltocheckinfo[(dyn_cast<CallInst>(call))].index;
                                    ch.realindex = tmp;
                                    ch.fakeindex = tmp;
                                    (*it)->kill_min.insert(ch);
                                }
                            }


                        }
                    }

                }
            }


            //max
            for (std::map<CallInst*, Checks>::iterator c = maxcalltocheckinfo.begin(); c != maxcalltocheckinfo.end(); c++) {
                Value* call = c->first;
                Value* tmp = indexdefmap[maxcalltocheckinfo[(dyn_cast<CallInst>(call))].index];

                //errs()<<*call<<"\n";
                for (std::vector<MCFG_Node*>::iterator it = MCFG.begin(); it != MCFG.end(); it++) {
                    for (std::vector<Instruction*>::iterator it2 = (*it)->instrs.begin(); it2 != (*it)->instrs.end(); it2++) {

                        if (dyn_cast<StoreInst>(*it2)) {
                            if ((dyn_cast<StoreInst>(*it2))->getPointerOperand() == tmp) {
                                if ((getEffect(*it2) == CHANGED) ||
                                        (getEffect(*it2) == INC) ||
                                        (getEffect(*it2) == MUL) ||
                                        (getEffect(*it2) == DIVLT1)
                                        ) {
                                    LightCheck ch;
                                    ch.bottom_limit = 0;
                                    ch.top_limit = 0;
                                    //ch.realindex=maxcalltocheckinfo[(dyn_cast<CallInst>(call))].index;
                                    ch.realindex = tmp;
                                    ch.fakeindex = tmp;
                                    (*it)->kill_max.insert(ch);
                                }
                            }


                        }
                    }

                }
            }

        }
        void init_kill_BW(std::vector<MCFG_Node*> &MCFG) {
            errs() << "\nKILL GENERATING:\n";
            for (std::map<CallInst*, Checks>::iterator c = mincalltocheckinfo.begin(); c != mincalltocheckinfo.end(); c++) {
                Value* call = c->first;
                Value* tmp = indexdefmap[mincalltocheckinfo[(dyn_cast<CallInst>(call))].index];

                //errs()<<*call<<"\n";
                for (std::vector<MCFG_Node*>::iterator it = MCFG.begin(); it != MCFG.end(); it++) {
                    for (std::vector<Instruction*>::iterator it2 = (*it)->instrs.begin(); it2 != (*it)->instrs.end(); it2++) {

                        if (dyn_cast<StoreInst>(*it2)) {
                            // errs()<<*tmp<<"   "<<**it2<<"\n";
                            if ((dyn_cast<StoreInst>(*it2))->getPointerOperand() == tmp) {

                                if ((getEffect(*it2) == CHANGED) ||
                                        (getEffect(*it2) == INC) ||
                                        (getEffect(*it2) == MUL) ||
                                        (getEffect(*it2) == DIVLT1)) {
                                        //errs()<<"here\n";
                                        LightCheck ch;
                                        ch.bottom_limit = 0;
                                        ch.top_limit = 0;
                                        //ch.realindex=mincalltocheckinfo[(dyn_cast<CallInst>(call))].index;
                                        ch.realindex = tmp;
                                        ch.fakeindex = tmp;
                                        (*it)->kill_min.insert(ch);
                                    }
                                }


                        }
                    }

                }
            }


            //max
            for (std::map<CallInst*, Checks>::iterator c = maxcalltocheckinfo.begin(); c != maxcalltocheckinfo.end(); c++) {
                Value* call = c->first;
                        Value* tmp = indexdefmap[maxcalltocheckinfo[(dyn_cast<CallInst>(call))].index];

                        //errs()<<*call<<"\n";
                for (std::vector<MCFG_Node*>::iterator it = MCFG.begin(); it != MCFG.end(); it++) {
                    for (std::vector<Instruction*>::iterator it2 = (*it)->instrs.begin(); it2 != (*it)->instrs.end(); it2++) {

                        if (dyn_cast<StoreInst>(*it2)) {
                            if ((dyn_cast<StoreInst>(*it2))->getPointerOperand() == tmp) {
                                if ((getEffect(*it2) == CHANGED) ||
                                        (getEffect(*it2) == DEC) ||
                                        (getEffect(*it2) == DIVGT1)) {
                                    LightCheck ch;
                                            ch.bottom_limit = 0;
                                            ch.top_limit = 0;
                                            //ch.realindex=maxcalltocheckinfo[(dyn_cast<CallInst>(call))].index;
                                            ch.realindex = tmp;
                                            ch.fakeindex = tmp;
                                            (*it)->kill_max.insert(ch);
                                }
                            }


                        }
                    }

                }
            }

        }


        EffectType getEffect(Instruction* inst) {
            Instruction* def_var = dyn_cast<Instruction>(inst->getOperand(1));
            Value* value = inst->getOperand(0);

            if (isa<Constant>(value)) {

                return CHANGED;
            }

            Instruction* op = dyn_cast<Instruction>(value);

            // If the definition is a = a + c
            // %3 = load i32* %a, align 4
            // %inc = add nsw i32 %3, 1
            // store i32 %inc, i32* %a, align 4
            if (op->getNumOperands() < 2) return CHANGED;

            const char* opName = op->getOpcodeName();
            Value* value1 = op->getOperand(0);
            Value* value2 = op->getOperand(1);

            Instruction* operand1;
            Instruction* operand2;

            Constant* finalC;

            if (isa<Constant>(value1) && isa<Constant>(value2)) {
                // z = 1 + 1
                return CHANGED;
            } else if (isa<Constant>(value1) && !isa<Constant>(value2)) {
                // z = 1 + z
                operand2 = dyn_cast<Instruction>(value2);
                if (operand2->getOpcode() != 27 || operand2->getOperand(0) != def_var) {
                    return CHANGED;
                }
                finalC = dyn_cast<Constant>(value1);
            } else if (!isa<Constant>(value1) && isa<Constant>(value2)) {
                operand1 = dyn_cast<Instruction>(value1);
                if (operand1->getOpcode() != 27 || operand1->getOperand(0) != def_var) {
                    return CHANGED;
                }
                finalC = dyn_cast<Constant>(value2);
            } else if (!isa<Constant>(value1) && !isa<Constant>(value2)) {
                // z = x + y
                return CHANGED;
            }


            APInt result = finalC->getUniqueInteger();
            if (strcmp(opName, "add") == 0) {
                if (result.isNonNegative()) {
                    if (result.sgt(0)) return INC;
                } else
                    return DEC;
            } else if (strcmp(opName, "sub") == 0) {
                if (result.sgt(0)) return DEC;
            } else if (strcmp(opName, "mul") == 0) {
                if (result.sgt(1)) return MUL;
            } else if (strcmp(opName, "sdiv") == 0) {
                if (result.sgt(1)) return DIVGT1;
                if (result.sgt(0) && result.slt(1)) return DIVLT1;
            } else {


                return CHANGED;
            }
            //errs()<<opName<<"\n";

            return CHANGED;

        }

        /*
        EffectType getEffect(Instruction* inst){
                        Instruction* def_var = dyn_cast<Instruction>(inst->getOperand(1));
                        Value* value = inst->getOperand(0);

                        if(isa<Constant>(value)){
                                        return CHANGED;
                        }

                        Instruction* op = dyn_cast<Instruction>(value);

                        // If the definition is a = a + c
                        // %3 = load i32* %a, align 4
                        // %inc = add nsw i32 %3, 1
                        // store i32 %inc, i32* %a, align 4
                        if(op->getNumOperands() < 2) return CHANGED;

                        const char* opName = op->getOpcodeName();
                        Value* value1 = op->getOperand(0);
                        Value* value2 = op->getOperand(1);

                        Instruction* operand1;
                        Instruction* operand2;

                        Constant* finalC;

                        if(isa<Constant>(value1) && isa<Constant>(value2)){
                                        // z = 1 + 1
                                        return CHANGED;
                        }
                        else if(isa<Constant>(value1) && !isa<Constant>(value2)){
                                        // z = 1 + z
                                        operand2 = dyn_cast<Instruction>(value2);
                                        if(operand2->getOpcode() != 27 || operand2->getOperand(0) != def_var){
                                                        return CHANGED;
                                        }
                                        finalC = dyn_cast<Constant>(value1);
                        }else if(!isa<Constant>(value1) && isa<Constant>(value2)){
                                        operand1 = dyn_cast<Instruction>(value1);
                                        if(operand1->getOpcode() != 27 || operand1->getOperand(0) != def_var){
                                                        return CHANGED;
                                        }
                                        finalC =dyn_cast<Constant>(value2);
                        }else if(!isa<Constant>(value1) && !isa<Constant>(value2)){
                                        // z = x + y
                                        return CHANGED;
                        }


                        APInt result = finalC->getUniqueInteger();
                        if(strcmp(opName, "add") == 0){
                                        if(result.sgt(0)) return INC;
                        }else if(strcmp(opName, "sub") == 0){
                                        if(result.sgt(0)) return DEC;
                        }else if(strcmp(opName, "mul") == 0){
                                        if(result.sgt(1)) return MUL;
                        }else if(strcmp(opName, "sdiv") == 0){
                                        if(result.sgt(1)) return DIVGT1;
                                        if(result.sgt(0) && result.slt(1)) return DIVLT1;
                        }else{
                                        return CHANGED;
                        }


                        return CHANGED;

        }
         */
        void deletefromMCFG(std::vector<MCFG_Node*> &MCFG, Instruction* i) {
            for (std::vector<MCFG_Node*>::iterator it = MCFG.begin(); it != MCFG.end(); it++) {
                for (std::vector<Instruction*>::iterator it2 = (*it)->instrs.begin(); it2 != (*it)->instrs.end(); it2++) {
                    if ((*it2) == i)
                        (*it)->instrs.erase(it2, it2);
                }

            }

        }

        unsigned getLine(Instruction* iInst) {
            if (DILocation * Loc = iInst->getDebugLoc())
                return Loc->getLine();
            else
                return 0;
        }

        void findindexwithgvn() {
            for (std::set<Value*>::iterator it = index.begin(); it != index.end(); it++) {
                if (dyn_cast<ConstantInt>(*it)) {
                    continue;
                }
                if (!(dyn_cast<Instruction>(*it))) {
                    indexdefmap[*it] = *it;

                }
                std::stack<Value*> worklist;
                Instruction* varInstr;
                varInstr = (Instruction*) * it;
                worklist.push(varInstr);
                while (!worklist.empty()) {
                    Value* var = (worklist.top());
                    worklist.pop();
                    if (dyn_cast<LoadInst>(var)) {
                        indexdefmap[varInstr] = (cast<LoadInst>(var))->getPointerOperand();
                        //errs()<<*(Instruction*)((cast<LoadInst>(var))->getPointerOperand())<<"\n";
                    } else if (dyn_cast<SExtInst>(var)) {

                        worklist.push((((Instruction*) var)->getOperand(0)));

                    }

                    else {

                        indexdefmap[varInstr] = var;
                    }


                }



            }


        }

        void getMCFGInst(Function &F) {
            //errs()<<"inside MCFG\n";


            std::set<Instruction*> used_base;

            for (std::set<Value*>::iterator it = index.begin(); it != index.end(); it++) {
                //errs() << "****index: " << **it << "\n";
                if (dyn_cast<ConstantInt>(*it))continue;
                std::stack<llvm::Use*> worklist;
                Instruction* varInstr;
                varInstr = (Instruction*) * it;
                //MCFGInst.insert(varInstr);
                MCFGInst.insert((indextocallmax[(Instruction*) * it]));
                MCFGInst.insert((indextocallmin[(Instruction*) * it]));
                MCFGInst.insert(varInstr);
                //worklist.push((Use*)varInstr);

                for (Instruction::op_iterator opI = varInstr->op_begin(); opI != varInstr->op_end(); ++opI)
                    worklist.push(&*opI);


                while (!worklist.empty()) {
                    llvm::Use* var = worklist.top();
                    worklist.pop();

                    if (dyn_cast<Instruction>((*var))) {
                        Instruction* inst = dyn_cast<Instruction>((*var));
                        MCFGInst.insert(inst);

                        // Add opcode (operator) to the expression

                        // Add all the operands to the list
                        for (Instruction::op_iterator opI = (*inst).op_begin(); opI != (*inst).op_end(); ++opI) {
                            Constant *op = dyn_cast<Constant>(*opI);

                            if (!op) {
                                // If not a constant
                                Instruction* opInst = dyn_cast<Instruction>((*opI));

                                if (opInst->getOpcode() == 26) {


                                    MCFGInst.insert(opInst);
                                } else {
                                    worklist.push(opI);
                                }
                            } else {
                                // If a constant, do not add to worklist

                            }
                        }

                        // If it is a 'load' instruction, need to find the closest 'store' instruction
                        if (inst->getOpcode() == 27) {
                            std::set<Instruction*> visited;
                            std::set<Instruction*> result;
                            findDefinitions(inst, inst, visited, result);
                            for (std::set<Instruction*>::iterator defI = result.begin(); defI != result.end(); defI++) {
                                MCFGInst.insert(*defI);
                                Value* base = (*defI)->getOperand(1);

                                for (Function::iterator bbIt = F.begin(); bbIt != F.end(); bbIt++)
                                    for (BasicBlock::iterator iInst = bbIt->begin(); iInst != bbIt->end(); iInst++)
                                        for (Instruction::op_iterator opI3 = (*iInst).op_begin(); opI3 != (*iInst).op_end(); ++opI3)
                                            if (dyn_cast<StoreInst>(&*iInst)) {
                                                Value* me = *opI3;
                                                if (base->getName() == me->getName()) {
                                                    if (used_base.find(&*iInst) == used_base.end())
                                                        used_base.insert(&*iInst);
                                                }
                                            }
                            }
                        }
                    }
                }
            }

            //for (std::set<Instruction*>::iterator it = used_base.begin(); it != used_base.end(); it++)
            //errs() << "RAMYAD: " << **it << "\n";


            for (std::set<Instruction*>::iterator itInst = used_base.begin(); itInst != used_base.end(); itInst++) {
                Value* it = *itInst;
                if (dyn_cast<ConstantInt>(it)) continue;
                std::stack<llvm::Use*> worklist;
                Instruction* varInstr;
                varInstr = *itInst;
                //MCFGInst.insert(varInstr);
                //MCFGInst.insert((indextocallmax[(Instruction*)*it]));
                //MCFGInst.insert((indextocallmin[(Instruction*)*it]));
                MCFGInst.insert(varInstr);
                //worklist.push((Use*)varInstr);

                //errs() << "RAMYAD2: " << *varInstr << "\n";

                for (Instruction::op_iterator opI = varInstr->op_begin(); opI != varInstr->op_end(); ++opI)
                    worklist.push(&*opI);


                while (!worklist.empty()) {
                    llvm::Use* var = worklist.top();
                    worklist.pop();
                    if (dyn_cast<Instruction>(*var)) {
                        Instruction* inst = dyn_cast<Instruction>(*var);
                        MCFGInst.insert(inst);

                        //errs() << "RAMYAD2: " << *inst << "\n";


                        // Add opcode (operator) to the expression

                        // Add all the operands to the list
                        for (Instruction::op_iterator opI = (*inst).op_begin(); opI != (*inst).op_end(); ++opI) {
                            Constant *op = dyn_cast<Constant>(*opI);

                            if (!op) {
                                // If not a constant
                                Instruction* opInst = dyn_cast<Instruction>((*opI));

                                if (opInst->getOpcode() == 26) {


                                    MCFGInst.insert(opInst);
                                } else {
                                    worklist.push(opI);
                                }
                            } else {
                                // If a constant, do not add to worklist

                            }
                        }
                    }

                    //    // If it is a 'load' instruction, need to find the closest 'store' instruction
                    //    if (inst->getOpcode() == 27) {
                    //      std::set<Instruction*> visited;
                    //      std::set<Instruction*> result;
                    //      findDefinitions(inst, inst, visited, result);
                    //      for (std::set<Instruction*>::iterator defI = result.begin(); defI != result.end(); defI++) {
                    //        MCFGInst.insert(*defI);
                    //        Value* base = (*defI)->getOperand(1);

                    //        for (Function::iterator bbIt = F.begin(); bbIt != F.end(); bbIt++)
                    //          for (BasicBlock::iterator iInst = bbIt->begin(); iInst != bbIt->end(); iInst++)
                    //            for (Instruction::op_iterator opI3 = (*iInst).op_begin(); opI3 != (*iInst).op_end(); ++opI3)
                    //              if (dyn_cast<StoreInst>(&*iInst)) {
                    //                Value* me = *opI3;
                    //                if (base->getName() == me->getName()) {
                    //                  if (used_base.find(&*iInst) == used_base.end())
                    //                    used_base.insert(&*iInst);
                    //                }
                    //              }
                    //      }
                    //    }

                }
            }


        }

        void findDefinitions(Instruction* curInstr, Instruction* startInstr, std::set<Instruction*>& visited, std::set<Instruction*>& result) {
            if (visited.find(curInstr) == visited.end()) {
                visited.insert(curInstr);

                //If found
                if (curInstr->getOpcode() == 28 && curInstr->getOperand(1) == startInstr->getOperand(0)) {
                    result.insert(curInstr);
                    return;
                }

                // If not found, backward traverse
                BasicBlock* curBB = curInstr->getParent();
                Instruction* first = curBB->begin();
                if (curInstr == first) {
                    // If curInstr is the first instruction of current basic block, back to previous basic blocks
                    for (pred_iterator PI = pred_begin(curBB); PI != pred_end(curBB); PI++) {
                        if (*PI) {
                            BasicBlock* preBB = *PI;
                            // Ignore empty block
                            findDefinitions(preBB->getTerminator(), startInstr, visited, result);
                        }
                    }
                } else {
                    findDefinitions(curInstr->getPrevNode(), startInstr, visited, result);
                }
            }
        }

        //MCFG

        void constructMCFG(Function& func, std::set<Instruction*>& allInstrs, std::vector<MCFG_Node*>& MCFG) {
            // Step 2.0: Initialize MCFG (copy CFG)
            initializeMCFG(func, MCFG);
            // Step 2.1: Only leave array subscript expression and definitions related
            optimizeMCFG1(allInstrs, MCFG);
            // Step 2.2: Remove empty nodes and duplicate edges (T1, T2, T3, figure 3, def-use optimization in the paper).
            // Refer to comments above function declaration for more details of optimizations conducted
            optimizeMCFG2(MCFG);
            // Step 2.3: Merge check nodes (T4, T5, T6)
            optimizeMCFG3(MCFG);

        }

        void initializeMCFG(Function& F, std::vector<MCFG_Node*>& MCFG) {
            // Basic block <--> MCFG node
            std::map<BasicBlock*, MCFG_Node*> visited;
            std::vector<BasicBlock*> worklist;
            for (Function::iterator bbIt = F.begin(); bbIt != F.end(); ++bbIt) {
                BasicBlock* bb = bbIt;
                MCFG_Node* node = new MCFG_Node();

                for (BasicBlock::iterator instrIt = bb->begin();
                        instrIt != bb->end(); ++instrIt) {
                    Instruction* curInstr = instrIt;
                    node->instrs.push_back(curInstr);
                }
                node->label = bb->getName();

                // Add the new visited node to MCFG
                MCFG.push_back(node);

                // Mark the basic block as visited
                visited[bb] = node;

                //Resolve predecessors
                for (pred_iterator PI = pred_begin(bb); PI != pred_end(bb); PI++) {
                    BasicBlock* pred = *PI;
                    // If the predecessor is visited, resolve the predecessor for current block
                    if (visited.find(pred) != visited.end()) {
                        MCFG_Node* pred_node = visited[pred];

                        // Do not insert duplicated predecessors and successors
                        if (node->preds.find(pred_node) == node->preds.end()) {
                            node->preds.insert(pred_node);
                        }

                        if (pred_node->succs.find(node) == pred_node->succs.end()) {
                            pred_node->succs.insert(node);
                        }
                    }
                }

                // Resolve successors
                for (succ_iterator SI = succ_begin(bb); SI != succ_end(bb); SI++) {
                    BasicBlock* succ = *SI;
                    if (visited.find(succ) != visited.end()) {
                        MCFG_Node* succ_node = visited[succ];

                        // Do not insert duplicated predecessors and successors
                        if (node->succs.find(succ_node) == node->succs.end()) {
                            node->succs.insert(succ_node);
                        }

                        if (succ_node->preds.find(node) == succ_node->preds.end()) {
                            succ_node->preds.insert(node);
                        }
                    }
                }
            }
        }

        void optimizeMCFG1(std::set<Instruction*>& allInstrs, std::vector<MCFG_Node*>& MCFG) {
            // Step 1: Remove all inrelevant instructions. Only leave array subscript expression and definitions related.
            for (std::vector<MCFG_Node*>::iterator it = MCFG.begin(); it != MCFG.end(); it++) {
                MCFG_Node* curNode = *it;
                std::vector<Instruction*> remained;

                // Leave related instructions and icmp in for/while condition check for loop bound
                for (std::vector<Instruction*>::iterator it2 = curNode->instrs.begin(); it2 != curNode->instrs.end(); ++it2) {
                    Instruction* curInstr = *it2;
                    if (allInstrs.find(curInstr) != allInstrs.end()
                            || (curNode->label.find(".cond") != std::string::npos && strcmp(curInstr->getOpcodeName(), "icmp") == 0)) {
                        remained.push_back(curInstr);
                    }
                }

                curNode->instrs = remained;
            }

        }

        void optimizeMCFG2(std::vector<MCFG_Node*>& MCFG) {
            // Unnecessary to apply T3 optimization (remove duplicated edges), since:
            // 1) During initialization, no duplicated edges
            // 2) When updating predecessors/successors after removing empty nodes, no duplicated edges
            std::vector<MCFG_Node*> toRemove;

            for (unsigned i = 0; i < MCFG.size(); i++) {
                MCFG_Node* curNode = MCFG[i];
                if (curNode->instrs.empty()) {
                    // Apply T2 optimization (remove self loop)
                    if (curNode->preds.find(curNode) != curNode->preds.end()) {
                        curNode->preds.erase(curNode);
                        curNode->succs.erase(curNode);
                        i--;
                        continue;
                    }

                    // Apply T1 optimization (remove empty node and update predecessors and successors) after all self loops for current node are removed
                    toRemove.push_back(curNode);
                    // Update predecessors and successors
                    std::set<MCFG_Node*> predes = curNode->preds;
                    std::set<MCFG_Node*> succs = curNode->succs;

                    if (!predes.empty() && !succs.empty()) {
                        if (predes.size() == 1 && succs.size() == 1) {
                            // If current node has both predecessors and successors, connect predecessors to successors
                            // Also apply the optimization illustrated in figure 3 here
                            for (std::set<MCFG_Node*>::iterator predI =
                                    predes.begin(); predI != predes.end();
                                    predI++) {
                                MCFG_Node* pre_node = *predI;
                                for (std::set<MCFG_Node*>::iterator succI =
                                        succs.begin(); succI != succs.end();
                                        succI++) {
                                    MCFG_Node* succ_node = *succI;

                                    // Update successors (do not insert duplicated ones)
                                    pre_node->succs.erase(curNode);
                                    if (pre_node->succs.find(succ_node)
                                            == pre_node->succs.end()) {
                                        pre_node->succs.insert(succ_node);
                                    }

                                    // Update predecessors (do not insert duplicated ones)
                                    succ_node->preds.erase(curNode);
                                    if (succ_node->preds.find(pre_node)
                                            == succ_node->preds.end()) {
                                        succ_node->preds.insert(pre_node);
                                    }
                                }
                            }
                        }
                    } else if (predes.empty() && !succs.empty()) {
                        // If current node only has successors, remove current node from predecessor list of each successor
                        for (std::set<MCFG_Node*>::iterator succI = succs.begin(); succI != succs.end(); succI++) {
                            MCFG_Node* succ_node = *succI;
                            succ_node->preds.erase(curNode);
                        }
                    } else if (!predes.empty() && succs.empty()) {
                        // If current node only has predecessors, remove current node from successor list of each predecessor
                        for (std::set<MCFG_Node*>::iterator predI = predes.begin(); predI != predes.end(); predI++) {
                            MCFG_Node* pre_node = *predI;
                            pre_node->succs.erase(curNode);
                        }
                    }

                }
            }

            // Remove empty nodes
            for (std::vector<MCFG_Node*>::iterator removeI = toRemove.begin(); removeI != toRemove.end(); removeI++) {
                MCFG_Node* curNode = *removeI;
                // Remove empty node first
                std::vector<MCFG_Node*>::iterator it = std::find(MCFG.begin(),
                        MCFG.end(), curNode);
                MCFG.erase(it);
            }
        }

        /*
         * Step 3: Optimization by applying T4, T5, T6 (only combine here)
         * 1. Check whether a node only contains 'check' (no 'store' instructions)
         * 2. If only checks, remove self loop if it has ( we don't remove self-loop here, to help hoist loop invariant later)
         * 3. Apply T5 and T6, i.e. combine T5 and T6 together, if predecessor only has one successor, and current node only has one predecessor
         * move current node to predecessor
         */
        void optimizeMCFG3(std::vector<MCFG_Node*>& MCFG) {
            std::vector<MCFG_Node*> toRemove;

            for (unsigned i = 0; i < MCFG.size(); i++) {
                MCFG_Node* curNode = MCFG[i];

                // Check if single predecessor - single successor
                if (curNode->preds.size() == 1 && (*(curNode->preds.begin()))->succs.size() == 1) {
                    // Merge current node to its predecessor
                    MCFG_Node* pred = *(curNode->preds.begin());
                    std::set<MCFG_Node*> succs = curNode->succs;

                    // Merge instructions first
                    for (std::vector<Instruction*>::iterator it = curNode->instrs.begin(); it != curNode->instrs.end(); it++) {
                        pred->instrs.push_back(*it);
                    }

                    // Connect predecessor and successors
                    pred->succs.erase(curNode);
                    curNode->preds.erase(pred);
                    for (std::set<MCFG_Node*>::iterator it2 = succs.begin(); it2 != succs.end(); it2++) {
                        pred->succs.insert(*it2);
                        (*it2)->preds.insert(pred);
                        (*it2)->preds.erase(curNode);
                    }

                    // Remove current node
                    toRemove.push_back(curNode);

                }
            }

            for (unsigned i = 0; i < toRemove.size(); i++) {
                MCFG_Node* curNode = toRemove[i];
                std::vector<MCFG_Node*>::iterator it = std::find(MCFG.begin(), MCFG.end(), curNode);
                MCFG.erase(it);
            }
        }

        void printMCFG(std::vector<MCFG_Node*>& MCFG) {
            errs() << "-------------------------------------------------------\n";
            errs() << "# Print MCFG:\n";
            errs() << "-------------------------------------------------------\n";
            MCFG_Node* entry = *(MCFG.begin());
            std::queue<MCFG_Node*> worklist;
            std::set<MCFG_Node*> visited;
            worklist.push(entry);

            while (!worklist.empty()) {
                MCFG_Node* curNode = worklist.front();
                worklist.pop();

                if (visited.find(curNode) == visited.end()) {
                    errs() << "# [" << curNode->label << "]                     ";

                    // Print predecessors
                    errs() << ";succs = ";
                    for (std::set<MCFG_Node*>::iterator preI = curNode->succs.begin(); preI != curNode->succs.end(); preI++) {
                        errs() << (*preI)->label << ", ";
                    }
                    errs() << "\t;preds = ";
                    for (std::set<MCFG_Node*>::iterator preI = curNode->preds.begin(); preI != curNode->preds.end(); preI++) {
                        errs() << (*preI)->label << ", ";
                    }
                    errs() << "\n";

                    for (std::vector<Instruction*>::iterator it =
                            curNode->instrs.begin(); it != curNode->instrs.end();
                            ++it) {
                        Instruction* curInstr = *it;
                        errs() << "    " << *curInstr << "\n";
                    }
                    errs() << "\n";

                    visited.insert(curNode);
                    for (std::set<MCFG_Node*>::iterator it2 =
                            curNode->succs.begin(); it2 != curNode->succs.end();
                            ++it2) {
                        MCFG_Node* nextNode = *it2;
                        if (visited.find(nextNode) == visited.end()) {
                            worklist.push(nextNode);
                        }
                    }
                }

            }
        }

        void printCFG(Function& F) {
            errs() << "-------------------------------------------------------\n";
            errs() << "# Print CFG:\n";
            errs() << "-------------------------------------------------------\n";
            BasicBlock* entry = &F.getEntryBlock();
            std::queue<BasicBlock*> worklist;
            std::set<BasicBlock*> visited;
            worklist.push(entry);

            //      while(!worklist.empty()){
            //        BasicBlock* curBB = worklist.front();
            //        worklist.pop();
            //
            //        if(visited.find(curBB) == visited.end()){
            //          errs() << "# [" << curBB->getName() << "]\n";
            //          for (BasicBlock::iterator instrIt = curBB->begin();
            //              instrIt != curBB->end(); ++instrIt) {
            //            Instruction* curInstr = instrIt;
            //            errs() << "    " << *curInstr << "\n";
            //          }
            //          errs() << "\n";
            //
            //          visited.insert(curBB);
            //          for (succ_iterator SI = succ_begin(curBB);
            //              SI != succ_end(curBB); ++SI) {
            //            BasicBlock* nextBB = *SI;
            //            if (visited.find(nextBB) == visited.end()) {
            //              worklist.push(nextBB);
            //            }
            //          }
            //        }
            //
            //      }
            for (Function::iterator bbIt = F.begin(); bbIt != F.end(); bbIt++) {

                errs() << *bbIt;



                errs() << "\n";
            }

        }

        static bool identicalValue(Value* left, Value* right) {
            if (left == right) return true;
            if ((dyn_cast<Instruction>(left))) {
                if (dyn_cast<Instruction>(right)) {
                    if (instIsIdentical((Instruction*) left, (Instruction*) right)) {
                        return true;
                    }
                }
            } else {
                errs() << "excepion happened in identicalValue()\n";
            }
            return false;


        }

        /*
        void recordHistory(Value* value, std::deque<User*>* hist) {

        if(dyn_cast<Instruction>(value)){
                Instruction* inst =dyn_cast<Instruction>(value);
                std::stack<llvm::User*> worklist;



        worklist.push((User*)value);
        while (!worklist.empty()){
                Value* cur=worklist.top();
                worklist.pop();


                 hist.push(cur->getOperator());
                for (Instruction::op_iterator opI = (*inst).op_begin();opI != (*inst).op_end(); ++opI) {
                        if(dyn_cast<Instruction>(*opI)){

                                        if(dyn_cast<LoadInst>(*opI))
                                                hist.push((LoadInst*)(*opI)->getPointerOperand());
                                        if(dyn_cast<ConstantInst>(*opI))
                                                hist.push(*opI)
                                        if else(dyn_cast<PHINode>(*opI))
                                                        hist.push(*opI);
                                        else{
                                                hist.push((*opI)->getOperator());
                                                worklist.push(*opI);
                                        }

                        }


                }

        }


        }
        else
                hist.push(value);


        errs()<<"record for:"<<*value<<"is: \n";
                std::stack<User*> s;

        while(!stack.empty()){
                errs()<<*stack.top()<<" ";
                s.push(stack.top());
                stack.pop();

        }

        }
         */
        static bool instIsIdentical(Instruction* l, Instruction* r) {
            if (l->getOpcode() != r->getOpcode())
                return false;
            else {
                //        errs()<<"identical?"<<"\n";
                //        errs()<<*l<<"  " <<*r<<"\n";
                if (l->getOpcode() == Instruction::Add || l->getOpcode() == Instruction::Mul ||
                        l->getOpcode() == Instruction::FAdd || l->getOpcode() == Instruction::FMul) {
                    if (l->getOperand(0)->getValueID() == Value::ConstantIntVal && r->getOperand(0)->getValueID() == Value::ConstantIntVal) {
                        if (constIsSame(l->getOperand(0), r->getOperand(0))) {
                            if (l->getOperand(1)->getValueID() == Value::ConstantIntVal && r->getOperand(1)->getValueID() == Value::ConstantIntVal)
                                return constIsSame(l->getOperand(1), r->getOperand(1));
                            else
                                return instIsIdentical(static_cast<Instruction*> (l->getOperand(1)), static_cast<Instruction*> (r->getOperand(1)));
                        } else
                            return false;
                    } else if (l->getOperand(0)->getValueID() == Value::ConstantIntVal && r->getOperand(1)->getValueID() == Value::ConstantIntVal) {
                        if (constIsSame(l->getOperand(0), r->getOperand(1))) {
                            if (l->getOperand(1)->getValueID() == Value::ConstantIntVal && r->getOperand(0)->getValueID() == Value::ConstantIntVal)
                                return constIsSame(l->getOperand(1), r->getOperand(0));
                            else
                                return instIsIdentical(static_cast<Instruction*> (l->getOperand(1)), static_cast<Instruction*> (r->getOperand(0)));
                        } else
                            return false;
                    } else if (l->getOperand(1)->getValueID() == Value::ConstantIntVal && r->getOperand(0)->getValueID() == Value::ConstantIntVal) {
                        if (constIsSame(l->getOperand(1), r->getOperand(0))) {
                            if (l->getOperand(0)->getValueID() == Value::ConstantIntVal && r->getOperand(1)->getValueID() == Value::ConstantIntVal)
                                return constIsSame(l->getOperand(0), r->getOperand(1));
                            else
                                return instIsIdentical(static_cast<Instruction*> (l->getOperand(0)), static_cast<Instruction*> (r->getOperand(1)));
                        } else
                            return false;

                    } else if (l->getOperand(1)->getValueID() == Value::ConstantIntVal && r->getOperand(1)->getValueID() == Value::ConstantIntVal) {
                        if (constIsSame(l->getOperand(1), r->getOperand(1))) {
                            if (l->getOperand(0)->getValueID() == Value::ConstantIntVal && r->getOperand(0)->getValueID() == Value::ConstantIntVal)
                                return constIsSame(l->getOperand(0), r->getOperand(0));
                            else
                                return instIsIdentical(static_cast<Instruction*> (l->getOperand(0)), static_cast<Instruction*> (r->getOperand(0)));
                        } else
                            return false;
                    } else {
                        bool case1a = instIsIdentical(static_cast<Instruction*> (l->getOperand(0)), static_cast<Instruction*> (r->getOperand(0)));
                        bool case1b = instIsIdentical(static_cast<Instruction*> (l->getOperand(1)), static_cast<Instruction*> (r->getOperand(1)));
                        bool case2a = instIsIdentical(static_cast<Instruction*> (l->getOperand(0)), static_cast<Instruction*> (r->getOperand(1)));
                        bool case2b = instIsIdentical(static_cast<Instruction*> (l->getOperand(1)), static_cast<Instruction*> (r->getOperand(0)));
                        return (case1a && case1b) || (case2a && case2b);
                    }
                } else if (l->getOpcode() == Instruction::Sub || l->getOpcode() == Instruction::FSub ||
                        l->getOpcode() == Instruction::UDiv || l->getOpcode() == Instruction::SDiv || l->getOpcode() == Instruction::FDiv) {
                    if (l->getOperand(0)->getValueID() == Value::ConstantIntVal && r->getOperand(0)->getValueID() == Value::ConstantIntVal) {
                        if (constIsSame(l->getOperand(0), r->getOperand(0))) {
                            if (l->getOperand(1)->getValueID() == Value::ConstantIntVal && r->getOperand(1)->getValueID() == Value::ConstantIntVal)
                                return constIsSame(l->getOperand(1), r->getOperand(1));
                            else
                                return instIsIdentical(static_cast<Instruction*> (l->getOperand(1)), static_cast<Instruction*> (r->getOperand(1)));
                        } else
                            return false;
                    } else if (l->getOperand(1)->getValueID() == Value::ConstantIntVal && r->getOperand(1)->getValueID() == Value::ConstantIntVal) {
                        if (constIsSame(l->getOperand(1), r->getOperand(1))) {
                            if (l->getOperand(0)->getValueID() == Value::ConstantIntVal && r->getOperand(0)->getValueID() == Value::ConstantIntVal)
                                return constIsSame(l->getOperand(0), r->getOperand(0));
                            else
                                return instIsIdentical(static_cast<Instruction*> (l->getOperand(0)), static_cast<Instruction*> (r->getOperand(0)));
                        } else
                            return false;
                    } else {
                        bool case1a = instIsIdentical(static_cast<Instruction*> (l->getOperand(0)), static_cast<Instruction*> (r->getOperand(0)));
                        bool case1b = instIsIdentical(static_cast<Instruction*> (l->getOperand(1)), static_cast<Instruction*> (r->getOperand(1)));
                        return (case1a && case1b);
                    }
                } else if (l->getOpcode() == Instruction::Load)
                    return (l->getOperand(0) == r->getOperand(0)) && live(l, r);
                else if (l->getOpcode() == Instruction::SExt || l->getOpcode() == Instruction::ZExt)
                    return instIsIdentical(static_cast<Instruction*> (l->getOperand(0)), static_cast<Instruction*> (r->getOperand(0)));
                else
                    return false;

            }
            return false;
        }

        static bool live(Instruction* l, Instruction* r) {
            if (l->getOperand(0) != r->getOperand(0)) return false;
            if (l == r) return true;
            BasicBlock* bb = l->getParent();
            Instruction* var = (Instruction*) r->getOperand(0);
            for (BasicBlock::iterator inst = bb->begin(); inst != bb->end(); ++inst) {

                if (((&*inst) == l) || ((&*inst) == r)) {
                    inst++;
                    for (BasicBlock::iterator inst2 = inst; inst2 != bb->end(); ++inst2) {
                        if (((&*inst2) == l) || ((&*inst2) == r)) {
                            return true;
                        }
                        if (dyn_cast<StoreInst>(inst2)) {
                            if (var == inst2->getOperand(1)) return false;
                        }
                    }
                }
            }

            errs() << "exception in live!!\n" << *l << *r;
            return true;
        }

        static bool constIsSame(Value* l, Value* r) {
            ConstantInt* leftArg = static_cast<ConstantInt*> (l);
            ConstantInt* rightArg = static_cast<ConstantInt*> (r);
            int64_t leftConst = leftArg->getZExtValue();
            int64_t rightConst = rightArg->getZExtValue();

            return (leftConst == rightConst);

        }

        /////////////////////////////////////
        /////////////////////////////////////

        //Find dominator set of each MCFG Node

        void getDominators(std::map<MCFG_Node*, std::set<MCFG_Node*> > &dominatorSet, std::vector<MCFG_Node*> MCFG) {
            errs() << ">>>>>>>>>>>>>>>>>>>>>>>>>> Get Dominators\n";
            MCFG_Node* entry = NULL;
            for (std::vector<MCFG_Node*>::iterator it = MCFG.begin(); it != MCFG.end(); ++it) {
                MCFG_Node* node = *it;
                if (node->preds.empty()) {
                    entry = node;
                    break;
                }
            }

            if (entry == NULL)
                return;
            if (entry->succs.empty())
                return;

            std::list<MCFG_Node*> nodeList;
            nodeList.push_back(entry);
            int current = 1;
            int next = 0;
            std::set<MCFG_Node*> visited;

            while (!nodeList.empty()) {
                MCFG_Node* node = nodeList.front();
                nodeList.pop_front();
                visited.insert(node);
                std::set<MCFG_Node*> succNodes = node->succs;
                for (std::set<MCFG_Node*>::iterator it = succNodes.begin(); it != succNodes.end(); ++it) {
                    MCFG_Node* succ = *it;
                    if (visited.find(succ) == visited.end()) {
                        nodeList.push_back(succ);
                        next++;
                    }
                }

                std::set<MCFG_Node*> dominators;
                std::map<MCFG_Node*, std::set<MCFG_Node*> > predDoms;

                std::set<MCFG_Node*> predNodes = node->preds;
                for (std::set<MCFG_Node*>::iterator it = predNodes.begin(); it != predNodes.end(); ++it) {
                    MCFG_Node* pred = *it;
                    if (dominatorSet.find(pred) != dominatorSet.end())
                        predDoms.insert(std::pair<MCFG_Node*, std::set<MCFG_Node*> >(pred, (*dominatorSet.find(pred)).second));
                }

                bool first = true;
                for (std::map<MCFG_Node*, std::set<MCFG_Node*> >::iterator it = predDoms.begin(); it != predDoms.end(); ++it) {
                    std::set<MCFG_Node*> doms = (*it).second;
                    std::set<MCFG_Node*> temp;
                    if (first) {
                        //The dominators of the first pred
                        for (std::set<MCFG_Node*>::iterator setItr = doms.begin(); setItr != doms.end(); ++setItr)
                            dominators.insert(*setItr);
                        first = false;
                    } else {
                        //Do Intersection
                        std::set<MCFG_Node*>::iterator setItr;
                        for (setItr = dominators.begin(); setItr != dominators.end(); ++setItr)
                            if (doms.find(*setItr) == doms.end())
                                temp.insert(*setItr);
                        for (setItr = temp.begin(); setItr != temp.end(); ++setItr)
                            if (dominators.find(*setItr) != dominators.end())
                                dominators.erase(dominators.find(*setItr));
                    }
                }

                dominators.insert(node);
                dominatorSet.insert(std::pair<MCFG_Node*, std::set<MCFG_Node*> >(node, dominators));

                current--;
                if (current == 0) {
                    current = next;
                    next = 0;
                }
            }
        }

        void printDominators(std::map<MCFG_Node*, std::set<MCFG_Node*> > dominatorSet) {
            errs() << "---------debug----------------------------------------------\n";
            errs() << "# Print Dominators:\n";
            errs() << "------------------------------------------------------------\n";
            for (std::map<MCFG_Node*, std::set<MCFG_Node*> >::iterator it = dominatorSet.begin(); it != dominatorSet.end(); ++it) {
                errs() << "\t|++++Node: " << it->first->label << "\n";
                errs() << "\t|++++Dominators:\n";

                for (std::set<MCFG_Node*>::iterator domItr = it->second.begin(); domItr != it->second.end(); ++domItr) {
                    errs() << "\t\t----Node: " << (*domItr)->label << "\n";
                }
            }
        }

        void findBackEdges(std::set<std::pair<MCFG_Node*, MCFG_Node*> > &backEdges, std::map<MCFG_Node*, std::set<MCFG_Node*> > dominatorSet) {
            errs() << ">>>>>>>>>>>>>>>>>>>>>>>>>> Get Backedges\n";
            std::set<MCFG_Node*> nodes;
            for (std::map<MCFG_Node*, std::set<MCFG_Node*> >::iterator it = dominatorSet.begin(); it != dominatorSet.end(); ++it)
                nodes.insert(it->first);
            for (std::set<MCFG_Node*>::iterator it = nodes.begin(); it != nodes.end(); ++it)
                for (std::set<MCFG_Node*>::iterator it2 = nodes.begin(); it2 != nodes.end(); ++it2) {
                    MCFG_Node* node = *it2;
                    if (dominates(*it, node, dominatorSet) && (((*it)->succs).find(node) != ((*it)->succs).end()))
                        backEdges.insert(std::pair<MCFG_Node*, MCFG_Node*>((*it), (*it2)));
                }
        }

        //Find whether MCFG_Node B is the dominator of A

        bool dominates(MCFG_Node* A, MCFG_Node* B, std::map<MCFG_Node*, std::set<MCFG_Node*> > dominatorSet) {
            std::set<MCFG_Node*> dominator = dominatorSet.find(A)->second;
            if (dominator.find(B) != dominator.end())
                return true;
            else
                return false;
        }

        void printBackEdges(std::set<std::pair<MCFG_Node*, MCFG_Node*> > &backEdges) {
            errs() << "---------debug----------------------------------------------\n";
            errs() << "# Print Backedge:\n";
            errs() << "------------------------------------------------------------\n";
            for (std::set<std::pair<MCFG_Node*, MCFG_Node*> >::iterator it = backEdges.begin(); it != backEdges.end(); it++) {
                MCFG_Node* first = it->first;
                MCFG_Node* second = it->second;
                errs() << "\t|++++Pair: " << first->label;
                errs() << "  <>   " << second->label << "\n";
            }
        }

        void findLoops(
                std::map<std::pair<MCFG_Node*, MCFG_Node*>, std::set<MCFG_Node*> > &loops,
                std::set<std::pair<MCFG_Node*, MCFG_Node*> > backEdges, std::map<MCFG_Node*,
                std::set<MCFG_Node*> > dominatorSet
                ) {
            errs() << ">>>>>>>>>>>>>>>>>>>>>>>>>> Finding Loops\n";
            for (std::set<std::pair<MCFG_Node*, MCFG_Node*> >::iterator it = backEdges.begin(); it != backEdges.end(); ++it) {
                std::set<MCFG_Node*> loopNodes;

                std::list<MCFG_Node*> nodeList;
                nodeList.push_back(it->first);
                int current = 1;
                int next = 0;
                std::set<MCFG_Node*> visited;

                while (!nodeList.empty()) {
                    MCFG_Node* node = nodeList.front();
                    nodeList.pop_front();
                    visited.insert(node);
                    if (node->label == (it->second)->label) {
                        loopNodes.insert(node);
                        //continue;
                    }
                    std::set<MCFG_Node*> predNodes = node->preds;
                    for (std::set<MCFG_Node*>::iterator predIt = predNodes.begin(); predIt != predNodes.end(); ++predIt) {
                        MCFG_Node* pred = *predIt;
                        if (visited.find(pred) == visited.end()) {
                            nodeList.push_back(pred);
                            next++;
                        }
                    }

                    if (dominates(node, (it->second), dominatorSet)) {
                        loopNodes.insert(node);
                    }


                    current--;
                    if (current == 0) {
                        current = next;
                        next = 0;
                    }
                }

                loops.insert(std::pair<std::pair<MCFG_Node*, MCFG_Node*>, std::set<MCFG_Node*> >(*it, loopNodes));
            }
        }

        void printLoops(std::map<std::pair<MCFG_Node*, MCFG_Node*>, std::set<MCFG_Node*> > &loops) {
            errs() << "---------debug----------------------------------------------\n";
            errs() << "# Print Loops:\n";
            errs() << "------------------------------------------------------------\n";
            for (std::map<std::pair<MCFG_Node*, MCFG_Node*>, std::set<MCFG_Node*> >::iterator it = loops.begin(); it != loops.end(); ++it) {
                MCFG_Node* pairFirst = (it->first).first;
                MCFG_Node* pairSecond = (it->first).second;
                errs() << "\t|++++Pair: " << pairFirst->label;
                errs() << "  <>   " << pairSecond->label << "\n";
                for (std::set<MCFG_Node*>::iterator it2 = (it->second).begin(); it2 != (it->second).end(); ++it2)
                    errs() << "\t\t|++Node: " << (*it2)->label << "\n";

            }

        }

        /*
         * Parameter 1 (allInstrs): A set of all instructions related to array index (including definition and alloca declaration)
         * Parameter 2 (computedIndexes): A map computed to include index details for each array index
         */
        void getAllRelatedInstrs(
                std::vector<MCFG_Node*> &MCFG,
                std::set<Instruction*>& allInstrs, std::map<Value*, ComputedArrayIndex*>& computedIndexes
                ) {
            errs() << ">>>>>>>>>>>>>>>>>>>>>>>>>> Getting all related instructions\n";
            for (std::vector<MCFG_Node*>::iterator it = MCFG.begin(); it != MCFG.end(); it++)
                for (std::vector<Instruction*>::iterator it2 = (*it)->instrs.begin(); it2 != (*it)->instrs.end(); it2++) {
                    Value* curr_index = NULL;

                    if (isMincall(*it2)) {
                        allInstrs.insert(*it2);
                        CallInst* call = static_cast<CallInst*> (*it2);
                        curr_index = mincalltocheckinfo[call].index;

                        ComputedArrayIndex* curr_computed_index = NULL;
                        if (computedIndexes.find(curr_index) == computedIndexes.end()) {
                            ComputedArrayIndex* newIndex = new ComputedArrayIndex();
                            computedIndexes[curr_index] = newIndex;
                            curr_computed_index = newIndex;
                        } else {
                            curr_computed_index = computedIndexes[curr_index];
                        }

                        curr_computed_index->index = curr_index;
                        curr_computed_index->minCall = call;

                    }



                    if (isMaxcall(*it2)) {
                        allInstrs.insert(*it2);
                        CallInst* call = static_cast<CallInst*> (*it2);
                        curr_index = maxcalltocheckinfo[call].index;

                        ComputedArrayIndex* curr_computed_index = NULL;
                        if (computedIndexes.find(curr_index) == computedIndexes.end()) {
                            ComputedArrayIndex* newIndex = new ComputedArrayIndex();
                            computedIndexes[curr_index] = newIndex;
                            curr_computed_index = newIndex;
                        } else {
                            curr_computed_index = computedIndexes[curr_index];
                        }

                        curr_computed_index->index = curr_index;
                        curr_computed_index->maxCall = call;



                    }

                }

            for (std::map<Value*, ComputedArrayIndex*>::iterator it = computedIndexes.begin(); it != computedIndexes.end(); ++it) {
                Instruction* varInstr = dyn_cast<Instruction>((*it).first);

                allInstrs.insert(varInstr);
            }

            for (std::map<Value*, ComputedArrayIndex*>::iterator it = computedIndexes.begin(); it != computedIndexes.end(); ++it) {
                std::stack<llvm::Use*> worklist;
                Instruction* varInstr = dyn_cast<Instruction>((*it).first);
                for (Instruction::op_iterator opI = varInstr->op_begin(); opI != varInstr->op_end(); ++opI)
                    worklist.push(&*opI);

                while (!worklist.empty()) {
                    llvm::Use* var = worklist.top();
                    worklist.pop();

                    if (dyn_cast<Instruction>((*var))) {
                        Instruction* inst = dyn_cast<Instruction>((*var));
                        allInstrs.insert(inst);

                        // Add opcode (operator) to the expression
                        it->second->index_expr.append(inst->getOpcodeName());
                        it->second->index_expr.append(".");

                        // Add all the operands to the list
                        for (Instruction::op_iterator opI = (*inst).op_begin(); opI != (*inst).op_end(); ++opI) {
                            Constant *op = dyn_cast<Constant>(*opI);

                            if (!op) {
                                // If not a constant
                                Instruction* opInst = dyn_cast<Instruction>((*opI));

                                if (opInst->getOpcode() == 26) {
                                    // If it is a variable declaration, do not need to propagate
                                    it->second->index_expr.append(opInst->getName());
                                    it->second->index_expr.append(".");

                                    allInstrs.insert(opInst);
                                } else
                                    worklist.push(&*opI);
                            } else {
                                // If a constant, do not add to worklist
                                it->second->index_expr.append(
                                        op->getUniqueInteger().toString(10, true));
                                it->second->index_expr.append(".");
                            }
                        }

                        // If it is a 'load' instruction, need to find the closest 'store' instruction
                        if (inst->getOpcode() == 27) {
                            std::set<Instruction*> visited;
                            std::set<Instruction*> result;
                            findDefinitions(inst, inst, visited, result);
                            for (std::set<Instruction*>::iterator defI = result.begin(); defI != result.end(); defI++) {
                                allInstrs.insert(*defI);
                                it->second->def_instrs.insert(*defI);
                            }
                        }
                    }
                }
            }
        }

        void printComputedIndexes(std::map<Value*, ComputedArrayIndex*>& computedIndexes) {
            errs() << "---------debug----------------------------------------------\n";
            errs() << "# Print Computed Indexes:\n";
            errs() << "------------------------------------------------------------\n";
            for (std::map<Value*, ComputedArrayIndex*>::iterator it = computedIndexes.begin(); it != computedIndexes.end(); ++it) {
                Instruction* varInstr = dyn_cast<Instruction>((*it).first);
                errs() << "\t|++++Index Inst: " << *varInstr << "\n";
                errs() << "\t\t|++IndexExp: " << (it->second)->index_expr << "\n";
                for (std::set<Instruction*>::iterator it2 = ((it->second)->def_instrs).begin(); it2 != ((it->second)->def_instrs).end(); it2++)
                    errs() << "\t\t|++DefInst: " << *(*it2) << "\n";
                errs() << "\n";
            }

        }

        void findForMonoloticalLoops(
                std::map<MCFG_Node*, std::set<loopInfoBounds*> > &loopMoves,
                std::map<std::pair<MCFG_Node*, MCFG_Node*>, std::set<MCFG_Node*> > &loops,
                std::map<Value*, ComputedArrayIndex*> computedIndexes
                ) {
            errs() << ">>>>>>>>>>>>>>>>>>>>>>>>>> Finding For Monotonic & Invariant Loops\n";

            for (std::map<std::pair<MCFG_Node*, MCFG_Node*>, std::set<MCFG_Node*> >::iterator it = loops.begin(); it != loops.end(); ++it) {
                MCFG_Node* firstNode = (it->first).second;
                MCFG_Node* lastNode = (it->first).first;
                std::set<MCFG_Node*> loopNode = it->second;

                Value* max = NULL;
                Value* min = NULL;

                //Find cmp instruction
                if ((firstNode->instrs).size() > 0) {
                    Instruction* forCmp = (firstNode->instrs)[0]; //Most loop.cond has only one instruction which is used for comparison
                    if (dyn_cast<PHINode>(forCmp))
                        forCmp = (firstNode->instrs)[1];
#ifdef DEBUG_LOOP
                    errs() << "---------debug----------------------------------------------\n";
                    errs() << "Finding For Monotonic Loops" << "\n";
                    errs() << "------------------------------------------------------------\n";
                    errs() << "Nodes:" << firstNode->label << "\n";
                    errs() << "\tCmp: " << *forCmp << "\n";
#endif
                    if ((!isa<ConstantInt>(forCmp->getOperand(1))) || forCmp->getOpcode() != 46) {
                        continue;
                    }
                    ConstantInt* bound = dyn_cast<ConstantInt>(forCmp->getOperand(1));
                    forType style = NONE; //1 - i < c; 2 - i <= c; 3 - i > c; 4 - i >= c
                    CmpInst *cmpInst = dyn_cast<CmpInst>(forCmp);
                    if (cmpInst->getPredicate() == CmpInst::ICMP_SLT) {
                        style = LT;
                        int tempMax = bound->getUniqueInteger().getSExtValue();
                        tempMax--;
                        max = ConstantInt::get((forCmp->getOperand(0))->getType(), tempMax);
                    } else if (cmpInst->getPredicate() == CmpInst::ICMP_SLE) {
                        style = LE;
                        max = bound;
                    } else if (cmpInst->getPredicate() == CmpInst::ICMP_SGT) {
                        style = GT;
                        int tempMin = bound->getUniqueInteger().getSExtValue();
                        tempMin++;
                        min = ConstantInt::get((forCmp->getOperand(0))->getType(), tempMin);
                    } else if (cmpInst->getPredicate() == CmpInst::ICMP_SGE) {
                        style = GE;
                        min = bound;
                    } else {
                        errs() << "**WARNING! Cannot detect style for for loop\n";
                        continue;
                    }


                    //Find pred (where loop variable is stored)
                    std::set<MCFG_Node*> loopPred = firstNode->preds;
                    MCFG_Node* pred;
                    for (std::set<MCFG_Node*>::iterator predIt = loopPred.begin(); predIt != loopPred.end(); predIt++)
                        if (*predIt != lastNode) {
                            pred = *predIt;
#ifdef DEBUG_LOOP
                            errs() << "\tpred: " << pred->label << "\n";
#endif
                        }

                    // Find loop variable
                    std::vector<Instruction*>::iterator itPred = pred->instrs.end();
                    BasicBlock* terminatingBB = (*itPred)->getParent();
                    Instruction* TerminatorInst = terminatingBB->getTerminator();
                    BasicBlock::iterator it_move_bb = TerminatorInst;
                    TerminatorInst = &*it_move_bb;
                    bool FoundStore = true;
                    while (!dyn_cast<StoreInst>(TerminatorInst)) {
                        it_move_bb--;
                        if (it_move_bb == terminatingBB->begin()) {
                            FoundStore = false;
                            break;
                        }
                        TerminatorInst = &*it_move_bb;
                    }

                    const Instruction* storeInst;
                    Value* varTemp = NULL;
                    std::string var = "0";
                    if (FoundStore) {
                        storeInst = dyn_cast<StoreInst>(TerminatorInst);
                        varTemp = storeInst->getOperand(1);
                        var = varTemp->getName().data();
                    }

#ifdef DEBUG_LOOP
                    if (FoundStore) {
                        errs() << "\tstore: " << *storeInst << "\n";
                        errs() << "\tvarTemp: " << *varTemp << "\n";
                    }
                    errs() << "\tvar: " << var << "\n";
                    if (max != NULL)
                        errs() << "\tMax: " << *max << "\n";
                    if (min != NULL)
                        errs() << "\tMin: " << *min << "\n";
#endif

                    std::map<MCFG_Node*, std::set<Instruction*> > curDeleteSet;
                    std::map<MCFG_Node*, std::set<Instruction*> > curHoistSet;

                    for (std::set<MCFG_Node*>::iterator loopIt = loopNode.begin(); loopIt != loopNode.end(); loopIt++) {
                        MCFG_Node* node = *loopIt;
                        std::vector<Instruction*> nodeInst = node->instrs;
                        for (std::vector<Instruction*>::iterator instIt = nodeInst.begin(); instIt != nodeInst.end(); instIt++) {
                            Instruction* inst = *instIt;
                            if (computedIndexes.find(inst) != computedIndexes.end()) {

                                ComputedArrayIndex* index = (computedIndexes.find(inst))->second;

                                // Check for definitions
                                bool defLoc = true; //false - don't hoist it
                                std::set<Instruction*> initial; //size = 0 or all the inst are not store using the same constant - don't hoist // all definition outside loop
                                std::set<Instruction*> def = index->def_instrs;
                                for (std::set<Instruction*>::iterator defIt = def.begin(); defIt != def.end(); defIt++) {
                                    bool inLoop = false;
                                    for (std::set<MCFG_Node*>::iterator dIt = loopNode.begin(); dIt != loopNode.end(); dIt++) {
                                        MCFG_Node* curNode = *dIt;
                                        std::vector<Instruction*> curInst = curNode->instrs;
                                        for (std::vector<Instruction*>::iterator ciIt = curInst.begin(); ciIt != curInst.end(); ciIt++) {
                                            if (*ciIt == *defIt) {
                                                inLoop = true;
                                                if (curNode != lastNode) {
                                                    // errs() << "*index: " << *(index->index) << "\n";
                                                    // errs() << "*defIT: " << **defIt << "\n";
                                                    // errs() << "*curNode: " << curNode->label << "\n";
                                                    // errs() << "*lastNode: " << lastNode->label << "\n";
                                                    defLoc = false;
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                    if (!defLoc) {
                                        break;
                                    }
                                    if (!inLoop) {
                                        initial.insert(*defIt);
                                    }
                                }

#ifdef DEBUG_LOOP
                                errs() << "\t\tindex: " << *(index->index) << "\n";
                                errs() << "\t\tindexExp: " << index->index_expr << "\n";
                                errs() << "\t\tdefLoc: " << defLoc << "\n";
#endif

                                // If one of the boundaries is constant
                                bool toDelete = false;
                                Constant* c = NULL;
                                if (defLoc && initial.size() > 0) {
                                    bool firstC = true;
                                    for (std::set<Instruction*>::iterator initIt = initial.begin(); initIt != initial.end(); initIt++) {
                                        //errs() << "*inst: " << **initIt << "\n";
                                        Value* op1 = (*initIt)->getOperand(0);
                                        if (isa<Constant>(op1)) {
                                            //errs() << "*op1: " << *op1 << "\n";
                                            if (firstC) {
                                                firstC = false;
                                                toDelete = true;
                                                c = dyn_cast<Constant>(op1);
                                            } else {
                                                Constant* c1 = dyn_cast<Constant>(op1);
                                                if (c1->getUniqueInteger() == c->getUniqueInteger()) {
                                                    toDelete = true;
                                                } else {
                                                    toDelete = false;
                                                }
                                            }
                                        } else {
                                            toDelete = false;
                                        }
                                    }
                                }

#ifdef DEBUG_LOOP
                                errs() << "\t\tdefDel: " << toDelete << "\n";
                                errs() << "\t\tStyle: " << style << "\n";
                                if (c != NULL)
                                    errs() << "\t\tconst: " << *c << "\n";
#endif

                                // if changes to index is monotonic
                                bool monotonic = true;

                                // check monotonic
                                std::string expr = index->index_expr;
                                std::vector<std::string> exprParts = split(expr, '.', 0);
                                //errs() << "\t\tsizeExp: " << exprParts.size() << "\n";

                                // monotonic find
                                // increasing
                                if (style == LT || style == LE)
                                    for (std::vector<std::string>::iterator it = exprParts.begin(); it != exprParts.end(); ++it) {
                                        if (*it == "add") {
                                            std::string numberAdd = *(++it);
                                            std::string::size_type sz;
                                            if (!is_number(numberAdd)) {
                                                monotonic = false;
                                                break;
                                            }
                                            int i_dec = std::stoi(numberAdd, &sz);
                                            if (i_dec < 0)
                                                monotonic = false;
                                        }
                                        if (*it == "sub") {
                                            std::string numberAdd = *(++it);
                                            std::string::size_type sz;
                                            if (!is_number(numberAdd)) {
                                                monotonic = false;
                                                break;
                                            }
                                            int i_dec = std::stoi(numberAdd, &sz);
                                            if (i_dec > 0)
                                                monotonic = false;
                                        }
                                        if (*it == "mul") {
                                            std::string numberAdd = *(++it);
                                            std::string::size_type sz;
                                            if (!is_number(numberAdd)) {
                                                monotonic = false;
                                                break;
                                            }
                                            int i_dec = std::stoi(numberAdd, &sz);
                                            if (i_dec < 1 && i_dec > 0)
                                                monotonic = false;
                                            else if (i_dec < 0)
                                                monotonic = false;
                                        }
                                        if (*it == "div") {
                                            std::string numberAdd = *(++it);
                                            std::string::size_type sz;
                                            if (!is_number(numberAdd)) {
                                                monotonic = false;
                                                break;
                                            }
                                            int i_dec = std::stoi(numberAdd, &sz);
                                            if (!(i_dec < 1 && i_dec > 0))
                                                monotonic = false;
                                        }
                                    }
                                    // decreasing
                                else if (style == GT || style == GE)
                                    for (std::vector<std::string>::iterator it = exprParts.begin(); it != exprParts.end(); ++it) {
                                        if (*it == "add") {
                                            std::string numberAdd = *(++it);
                                            std::string::size_type sz;
                                            if (!is_number(numberAdd)) {
                                                monotonic = false;
                                                break;
                                            }
                                            if (!is_number(numberAdd)) {
                                                monotonic = false;
                                                break;
                                            }
                                            int i_dec = std::stoi(numberAdd, &sz);
                                            if (i_dec > 0)
                                                monotonic = false;
                                        }
                                        if (*it == "sub") {
                                            std::string numberAdd = *(++it);
                                            std::string::size_type sz;
                                            if (!is_number(numberAdd)) {
                                                monotonic = false;
                                                break;
                                            }
                                            int i_dec = std::stoi(numberAdd, &sz);
                                            if (i_dec < 0)
                                                monotonic = false;
                                        }
                                        if (*it == "mul") {
                                            std::string numberAdd = *(++it);
                                            std::string::size_type sz;
                                            if (!is_number(numberAdd)) {
                                                monotonic = false;
                                                break;
                                            }
                                            int i_dec = std::stoi(numberAdd, &sz);
                                            if (i_dec > 1)
                                                monotonic = false;
                                        }
                                        if (*it == "div") {
                                            std::string numberAdd = *(++it);
                                            std::string::size_type sz;
                                            if (!is_number(numberAdd)) {
                                                monotonic = false;
                                                break;
                                            }
                                            int i_dec = std::stoi(numberAdd, &sz);
                                            if (i_dec < 1 && i_dec > 0)
                                                monotonic = false;
                                        }
                                    }

                                //loop invariant find
                                bool SameVarAsLoop = false;
                                bool loopInvariant = true;
                                for (std::vector<std::string>::iterator it = exprParts.begin(); it != exprParts.end(); ++it)
                                    if (*it == var) {
                                        loopInvariant = false;
                                        SameVarAsLoop = true;
                                    }

                                //loop invariant defDel fix
                                // if is loop invariant and def is outside the loop, change to delete to true
                                if (loopInvariant && !toDelete) {
                                    toDelete = true;
                                    for (std::set<Instruction*>::iterator initIt = initial.begin(); initIt != initial.end(); initIt++) {
                                        const BasicBlock* bbMe = (*initIt)->getParent();
                                        std::string bbName = bbMe->getName();
                                        for (std::set<MCFG_Node*>::iterator dIt = loopNode.begin(); dIt != loopNode.end(); dIt++)
                                            if ((*dIt)->label == bbName) {
                                                toDelete = false;
                                            }
                                    }
                                }

                                //loop invariant after use change fix
                                if (loopInvariant) {

                                    std::set<Instruction*> def = index->def_instrs;
                                    for (std::set<Instruction*>::iterator defIt = def.begin(); defIt != def.end(); defIt++) {
                                        MCFG_Node* curNode = firstNode;
                                        std::vector<Instruction*> curInst = curNode->instrs;
                                        for (std::vector<Instruction*>::iterator ciIt = curInst.begin(); ciIt != curInst.end(); ciIt++)
                                            if (*ciIt == *defIt)
                                                loopInvariant = false;
                                    }

                                }

                                //2 loop invariant after use change fix
                                if (!loopInvariant && !SameVarAsLoop) {
                                    toDelete = true;
                                    std::set<Instruction*> def = index->def_instrs;
                                    for (std::set<Instruction*>::iterator defIt = def.begin(); defIt != def.end(); defIt++)
                                        for (std::set<MCFG_Node*>::iterator lIt = loopNode.begin(); lIt != loopNode.end(); lIt++) {
                                            std::vector<Instruction*> curInstVec = (*lIt)->instrs;
                                            for (std::vector<Instruction*>::iterator it = curInstVec.begin(); it != curInstVec.end(); it++)
                                                if (*it == *defIt) {
                                                    // safe to not move it
                                                    toDelete = false;
                                                }
                                        }
                                }

                                // Fix for iterators that are not in the for
                                // if (!loopInvariant && toDelete) {
                                //  for (std::vector<std::string>::iterator it = exprParts.begin(); it != exprParts.end(); ++it)
                                //    if (*it == "load") {
                                //      std::string thisVar = *(++it);
                                //      if (var != thisVar) {
                                //        toDelete = false;
                                //        break;
                                //      }
                                //    }
                                // }


#ifdef DEBUG_LOOP
                                errs() << "\t\tdefDelAfter: " << toDelete << "\n";
                                errs() << "\t\tmonotonic: " << monotonic << "\n";
                                errs() << "\t\tloopInvariant: " << loopInvariant << "\n";
                                errs() << "\t\tsameVarAsLoop: " << SameVarAsLoop << "\n";
                                if (max != NULL)
                                    errs() << "\t\tMaxAfter: " << *max << "\n";
                                if (min != NULL)
                                    errs() << "\t\tMinAfter: " << *min << "\n";
#endif

                                /////// MAKE OPTIMIZATIONS COMMANDS

                                // Save this and let others know
                                // Loops optimizations for max and min
                                if (toDelete && monotonic && defLoc && !loopInvariant) {
#ifdef DEBUG_LOOP
                                    errs() << "\t\tWill move this (monotonic)!\n";
#endif
                                    // index calls, move it to the end of pred
                                    // need to send constant as well
                                    // std::map<MCFG_Node*, std::set<loopInfoBounds*> > &loopMoves,
                                    loopInfoBounds* thisMove = new struct loopInfoBounds;
                                    thisMove->style = style;
                                    thisMove->current_node = firstNode;
                                    thisMove->move_to_node = pred;
                                    thisMove->index = index->index;
                                    if (style == LT || style == LE) { //increasing
                                        thisMove->move_max = false;
                                        thisMove->move_min = true;
                                        thisMove->c_min = c;
                                        if (max != NULL) {
                                            thisMove->move_max = true;
                                            thisMove->c_max = dyn_cast<Constant>(max);
#ifdef DEBUG_LOOP
                                            errs() << "\t\tEdit: Will move this (monotonic, bounds known A)!\n";
#endif
                                        }
                                    } else if (style == GT || style == GE) { //decreasing
                                        thisMove->move_max = true;
                                        thisMove->move_min = false;
                                        thisMove->c_max = c;
                                        if (min != NULL) {
                                            thisMove->move_min = true;
                                            thisMove->c_min = dyn_cast<Constant>(min);
#ifdef DEBUG_LOOP
                                            errs() << "\t\tEdit: Will move this (monotonic, bounds known B)!\n";
#endif
                                        }
                                    }
                                    thisMove->computed_array_index = index;

                                    loopMoves[firstNode].insert(thisMove);

                                }


                                    // Invariant optimizations
                                else if (toDelete && defLoc && loopInvariant) {
#ifdef DEBUG_LOOP
                                    errs() << "\t\tWill move all this (Invariant)!\n";
#endif
                                    //!! check if you relate indexes to i
                                    // index calls, move it to the end of pred
                                    // need to send constant as well
                                    // std::map<MCFG_Node*, std::set<loopInfoBounds*> > &loopMoves,
                                    loopInfoBounds* thisMove = new struct loopInfoBounds;
                                    thisMove->style = NONE;
                                    thisMove->current_node = firstNode;
                                    thisMove->move_to_node = pred;
                                    //thisMove->c = c;
                                    thisMove->index = index->index;
                                    thisMove->move_max = true;
                                    thisMove->move_min = true;

                                    thisMove->c_max = NULL;
                                    thisMove->c_min = NULL;

                                    thisMove->computed_array_index = index;

                                    loopMoves[firstNode].insert(thisMove);

                                }


                            }
                        }
                    }

                }
            }
        }

        std::vector<std::string> split(std::string work, char delim, int rep) {
            std::vector<std::string> flds;
            std::string buf = "";
            unsigned long i = 0;
            while (i < work.size()) {
                if (work[i] != delim)
                    buf += work[i];
                else if (rep == 1) {
                    flds.push_back(buf);
                    buf = "";
                } else if (buf.length() > 0) {
                    flds.push_back(buf);
                    buf = "";
                }
                i++;
            }
            if (!buf.empty())
                flds.push_back(buf);
            return flds;
        }

        bool is_number(const std::string& s) {
            std::string::const_iterator it = s.begin();
            while (it != s.end() && (std::isdigit(*it) || *it == '-')) ++it;
            return !s.empty() && it == s.end();
        }

        void printLoopMoves(std::map<MCFG_Node*, std::set<loopInfoBounds*> > &loopMoves) {
            errs() << "---------debug----------------------------------------------\n";
            errs() << "# Print Loop Info Bounds:\n";
            errs() << "------------------------------------------------------------\n";
            for (std::map<MCFG_Node*, std::set<loopInfoBounds*> >::iterator itLoop = loopMoves.begin(); itLoop != loopMoves.end(); ++itLoop) {
                errs() << "++Node: " << itLoop->first->label << "\n";
                for (std::set<loopInfoBounds*>::iterator it = (itLoop->second).begin(); it != (itLoop->second).end(); ++it) {
                    errs() << "++++CurrNode: " << (*it)->current_node->label << "\n";
                    errs() << "++++MoveNode: " << (*it)->move_to_node->label << "\n";
                    errs() << "++++Style: " << (*it)->style << "\n";
                    errs() << "++++MoveMax: " << (*it)->move_max << "\n";
                    errs() << "++++MoveMin: " << (*it)->move_min << "\n";
                    if (((*it)->c_max) == NULL)
                        errs() << "++++Max: " << "NULL" << "\n";
                    else
                        errs() << "++++Max: " << *((*it)->c_max) << "\n";
                    if (((*it)->c_min) == NULL)
                        errs() << "++++Min: " << "NULL" << "\n";
                    else
                        errs() << "++++Min: " << *((*it)->c_min) << "\n";
                    errs() << "++++Index: " << *((*it)->index) << "\n";
                    errs() << "++++MaxCall: " << *((*it)->computed_array_index->maxCall) << "\n";
                    errs() << "++++MinCall: " << *((*it)->computed_array_index->minCall) << "\n";
                    errs() << "++++IndexExp: " << (*it)->computed_array_index->index_expr << "\n";
                    for (std::set<Instruction*>::iterator it2 = ((*it)->computed_array_index->def_instrs).begin(); it2 != ((*it)->computed_array_index->def_instrs).end(); it2++)
                        errs() << "+++++++DefInst: " << *(*it2) << "\n";
                    errs() << "--------------------------------------------\n\n";
                }
            }
        }

        //Merge nested loops moves

        void mergeNestedLoopMoves(std::map<MCFG_Node*, std::set<loopInfoBounds*> > &loopMoves) {
            int mergeCount = 0;
            errs() << ">>>>>>>>>>>>>>>>>>>>>>>>>> Merging Nested Loops Moves\n";
            for (std::map<MCFG_Node*, std::set<loopInfoBounds*> >::iterator itLoop1 = loopMoves.begin(); itLoop1 != loopMoves.end(); ++itLoop1)
                for (std::map<MCFG_Node*, std::set<loopInfoBounds*> >::iterator itLoop2 = loopMoves.begin(); itLoop2 != loopMoves.end(); ++itLoop2) {
                    if (itLoop1->first->label == itLoop2->first->label)
                        continue;
                    for (std::set<loopInfoBounds*>::iterator it1 = (itLoop1->second).begin(); it1 != (itLoop1->second).end(); ++it1)
                        for (std::set<loopInfoBounds*>::iterator it2 = (itLoop2->second).begin(); it2 != (itLoop2->second).end(); /*++it2*/) {
                            if (((*it1)->index) == ((*it2)->index)) {
                                if ((*it1)->current_node->label == (*it2)->move_to_node->label) {
                                    //it1 should stay, and get its curr_node from it2, remove it2
                                    if (((*it1)->move_max == (*it2)->move_max) && ((*it1)->move_min == (*it2)->move_min)) {
                                        if (((*it1)->c_min == (*it2)->c_min) && ((*it1)->c_max == (*it2)->c_max)) {
                                            (*it1)->current_node = (*it2)->current_node;
                                            it2 = (itLoop2->second).erase(it2);
                                            mergeCount++;
                                        } else
                                            ++it2;
                                    } else
                                        ++it2;
                                } else
                                    ++it2;
                            } else
                                ++it2;
                        }
                }

            errs() << "------- Merged Nested Loops: " << mergeCount << "\n";
        }

        //choose the weakest one on same index
        // FIXME: choose the weakest

        void mergeNestedLoopMovesDuplicates(std::map<MCFG_Node*, std::set<loopInfoBounds*> > &loopMoves) {
            int mergeWeakestCount = 0;
            errs() << ">>>>>>>>>>>>>>>>>>>>>>>>>> Merging Nested Loops Moves Duplicates (Choosing weakest one)\n";
            for (std::map<MCFG_Node*, std::set<loopInfoBounds*> >::iterator itLoop1 = loopMoves.begin(); itLoop1 != loopMoves.end(); ++itLoop1)
                for (std::map<MCFG_Node*, std::set<loopInfoBounds*> >::iterator itLoop2 = loopMoves.begin(); itLoop2 != loopMoves.end(); ++itLoop2) {
                    if (itLoop1->first->label == itLoop2->first->label)
                        continue;
                    for (std::set<loopInfoBounds*>::iterator it1 = (itLoop1->second).begin(); it1 != (itLoop1->second).end(); /*++it1*/) {
                        bool flagDelete = false;
                        for (std::set<loopInfoBounds*>::iterator it2 = (itLoop2->second).begin(); it2 != (itLoop2->second).end(); /*++it2*/) {
                            if (((*it1)->index) == ((*it2)->index)) {
                                bool weak = findWeakestBounds(*it1, *it2);
                                if (weak) {
                                    it1 = (itLoop1->second).erase(it1);
                                    flagDelete = true;
                                    mergeWeakestCount++;
                                    break;
                                } else
                                    it2 = (itLoop2->second).erase(it2);
                                mergeWeakestCount++;
                            } else
                                ++it2;
                        }
                        if (flagDelete == false)
                            it1++;
                    }

                }


            errs() << "------- Merged Duplicates Loops: " << mergeWeakestCount << "\n";
        }

        // returns true if bound1 is weaker

        bool findWeakestBounds(loopInfoBounds* bound1, loopInfoBounds* bound2) {
            int weakPoints1 = 0;
            int weakPoints2 = 0;

            if (bound1->move_max == false)
                weakPoints1++;

            if (bound2->move_max == false)
                weakPoints2++;

            if (bound1->move_min == false)
                weakPoints1++;

            if (bound2->move_min == false)
                weakPoints2++;

            if (bound1->c_max == NULL)
                weakPoints1++;

            if (bound2->c_max == NULL)
                weakPoints2++;

            if (bound1->c_min == NULL)
                weakPoints1++;

            if (bound2->c_min == NULL)
                weakPoints2++;

            if (bound1->style == 0)
                weakPoints1++;

            if (bound2->style == 0)
                weakPoints2++;

            std::vector<std::string> exprs_vector1 = split(bound1->computed_array_index->index_expr, '.', 0);
            std::vector<std::string> exprs_vector2 = split(bound2->computed_array_index->index_expr, '.', 0);

            if (exprs_vector1.size() > exprs_vector2.size())
                weakPoints1++;
            else
                weakPoints2++;

#ifdef DEBUG_LOOP
            errs() << "\t1 : " << bound1->current_node->label << " P: " << weakPoints1 << "\n";
            errs() << "\t2 : " << bound2->current_node->label << " P: " << weakPoints2 << "\n";
#endif

            if (weakPoints1 > weakPoints2)
                return true;
            else
                return false;
        }

        void printLoopMovesAfterNestedMerge(std::map<MCFG_Node*, std::set<loopInfoBounds*> > &loopMoves) {
            errs() << "---------debug----------------------------------------------\n";
            errs() << "# Print Loop Info Bounds After Merging Nested Loops:\n";
            errs() << "------------------------------------------------------------\n";
            for (std::map<MCFG_Node*, std::set<loopInfoBounds*> >::iterator itLoop = loopMoves.begin(); itLoop != loopMoves.end(); ++itLoop) {
                errs() << "++Node: " << itLoop->first->label << "\n";
                for (std::set<loopInfoBounds*>::iterator it = (itLoop->second).begin(); it != (itLoop->second).end(); ++it) {
                    errs() << "++++CurrNode: " << (*it)->current_node->label << "\n";
                    errs() << "++++MoveNode: " << (*it)->move_to_node->label << "\n";
                    errs() << "++++Style: " << (*it)->style << "\n";
                    errs() << "++++MoveMax: " << (*it)->move_max << "\n";
                    errs() << "++++MoveMin: " << (*it)->move_min << "\n";
                    if (((*it)->c_max) == NULL)
                        errs() << "++++Max: " << "NULL" << "\n";
                    else
                        errs() << "++++Max: " << *((*it)->c_max) << "\n";
                    if (((*it)->c_min) == NULL)
                        errs() << "++++Min: " << "NULL" << "\n";
                    else
                        errs() << "++++Min: " << *((*it)->c_min) << "\n";
                    errs() << "++++Index: " << *((*it)->index) << "\n";
                    errs() << "++++MaxCall: " << *((*it)->computed_array_index->maxCall) << "\n";
                    errs() << "++++MinCall: " << *((*it)->computed_array_index->minCall) << "\n";
                    errs() << "++++IndexExp: " << (*it)->computed_array_index->index_expr << "\n";
                    for (std::set<Instruction*>::iterator it2 = ((*it)->computed_array_index->def_instrs).begin(); it2 != ((*it)->computed_array_index->def_instrs).end(); it2++)
                        errs() << "+++++++DefInst: " << *(*it2) << "\n";
                    errs() << "--------------------------------------------\n\n";
                }
            }
        }

        void moveCallsLoops(Function &F, std::map<MCFG_Node*, std::set<loopInfoBounds*> > &loopMoves, std::vector<MCFG_Node*> &MCFG) {
            errs() << ">>>>>>>>>>>>>>>>>>>>>>>>>> Moving Calls for Loops\n";
            int maxMovesConstant = 0;
            int minMovesConstant = 0;
            int maxMovesInvariant = 0;
            int minMovesInvariant = 0;
            for (std::map<MCFG_Node*, std::set<loopInfoBounds*> >::iterator itLoop = loopMoves.begin(); itLoop != loopMoves.end(); ++itLoop) {
                //MCFG_Node* node = itLoop->first;
                for (std::set<loopInfoBounds*>::iterator it = (itLoop->second).begin(); it != (itLoop->second).end(); ++it) {
#ifdef DEBUG_LOOP
                    errs() << "+++Processing: " << (*it)->current_node->label << "\n";
                    errs() << "\tFor: " << *((*it)->index) << "\n";
#endif
                    //****** First Delete them
                    removeInstForLoops(**it, MCFG);

                    //****** Then create the Moves
                    // ++1. if simply checking max and min
                    if (((*it)->move_max) && ((*it)->c_max != NULL)) {
#ifdef DEBUG_LOOP
                        errs() << "\tIn: " << "insertBoundCheckLoopMax" << "\n";
#endif
                        insertBoundCheckLoopMax((*it)->index, (*it)->c_max, (*it)->move_to_node);
                        maxMovesConstant++;
                    }

                    if (((*it)->move_min) && ((*it)->c_min != NULL)) {
#ifdef DEBUG_LOOP
                        errs() << "\tIn: " << "insertBoundCheckLoopMin" << "\n";
#endif
                        insertBoundCheckLoopMin((*it)->index, (*it)->c_min, (*it)->move_to_node);
                        minMovesConstant++;
                    }

                    // ++2. if need to check invariant loop variables (no boundary)
                    // should also add load before it
                    bool moveBeforeUntilLoad = false;
                    if (((*it)->move_max) && ((*it)->c_max == NULL)) {
#ifdef DEBUG_LOOP
                        errs() << "\tIn: " << "insertInvariantCheckLoopMax" << "\n";
#endif
                        insertInvariantCheckLoopMax((*it)->index, (*it)->move_to_node);
                        maxMovesInvariant++;
                        moveBeforeUntilLoad = true;
                    }

                    if (((*it)->move_min) && ((*it)->c_min == NULL)) {
#ifdef DEBUG_LOOP
                        errs() << "\tIn: " << "insertInvariantCheckLoopMin" << "\n";
#endif
                        insertInvariantCheckLoopMin((*it)->index, (*it)->move_to_node);
                        minMovesInvariant++;
                        moveBeforeUntilLoad = true;
                    }

                    if (moveBeforeUntilLoad) {
#ifdef DEBUG_LOOP
                        errs() << "\tIn: " << "invariantChackLoopMoveDepInst" << "\n";
#endif
                        invariantChackLoopMoveDepInst((*it)->index, (*it)->move_to_node);
                    }

                }
            }

            errs() << "------- Loop Max Check Constant: " << maxMovesConstant << "\n";
            errs() << "------- Loop Min Check Constant: " << minMovesConstant << "\n";
            errs() << "------- Loop Max Check Invariant: " << maxMovesInvariant << "\n";
            errs() << "------- Loop Min Check Invariant: " << minMovesInvariant << "\n";
        }

        void removeInstForLoops(loopInfoBounds &info, std::vector<MCFG_Node*> &MCFG) {
            //MCFG_Node* node = info.current_node;
            if (info.move_max) {
                CallInst* maxCall = (info.computed_array_index)->maxCall;
                Instruction* inst = dyn_cast<Instruction>(maxCall);
                deletefromMCFG(MCFG, inst);
                inst->eraseFromParent();
            }
            if (info.move_min) {
                CallInst* minCall = (info.computed_array_index)->minCall;
                Instruction* inst = dyn_cast<Instruction>(minCall);
                deletefromMCFG(MCFG, inst);
                inst->eraseFromParent();
            }
        }

        void insertBoundCheckLoopMax(Value* index, Constant* c_max, MCFG_Node* node_move) {
            // Find index and GEP instructions
            Instruction* indexInst = dyn_cast<Instruction>(index);
            BasicBlock::iterator backIt = indexInst;
            backIt++;
            GetElementPtrInst *GEP = static_cast<GetElementPtrInst*> ((Instruction*) (&*backIt));

            //Find array bounds
            long top_limit;
            std::map<Value*, ArrayInfo*>::iterator it = arrayMap.find(GEP->getPointerOperand());
            if (it != arrayMap.end()) {
                top_limit = it->second->top_limit;
            } else {
#ifdef DEBUG
                errs() << "\tWARNING! Should not be here! ArrayMapInfo!\n";
#endif
            }

            //Find last Inst of move_to block (non Terminator)
            BasicBlock* bb_move = (node_move->instrs[0])->getParent();
            Instruction* terminator_move_inst = bb_move->getTerminator();
            BasicBlock::iterator it_move_bb = terminator_move_inst;
            while (dyn_cast<TerminatorInst>(terminator_move_inst) || dyn_cast<CallInst>(terminator_move_inst)) {
                it_move_bb--;
                terminator_move_inst = &*it_move_bb;
            }

            ConstantInt* max = dyn_cast<ConstantInt>(c_max);
            AddMaxCallLoop(max->getSExtValue(), top_limit, terminator_move_inst);

        }

        void insertInvariantCheckLoopMax(Value* index, MCFG_Node* node_move) {
            // Find index and GEP instructions
            Instruction* indexInst = dyn_cast<Instruction>(index);
            BasicBlock::iterator backIt = indexInst;
            backIt++;
            GetElementPtrInst *GEP = static_cast<GetElementPtrInst*> ((Instruction*) (&*backIt));

            //Find array bounds
            long top_limit;
            std::map<Value*, ArrayInfo*>::iterator it = arrayMap.find(GEP->getPointerOperand());
            if (it != arrayMap.end()) {
                top_limit = it->second->top_limit;
            } else {
#ifdef DEBUG
                errs() << "\tWARNING! Should not be here! ArrayMapInfo!\n";
#endif
            }

            //Find last Inst of move_to block (non Terminator)
            BasicBlock* bb_move = (node_move->instrs[0])->getParent();
            Instruction* terminator_move_inst = bb_move->getTerminator();
            BasicBlock::iterator it_move_bb = terminator_move_inst;
            while (dyn_cast<TerminatorInst>(terminator_move_inst) || dyn_cast<CallInst>(terminator_move_inst)) {
                it_move_bb--;
                terminator_move_inst = &*it_move_bb;
            }

            AddMaxCallLoop(index, top_limit, terminator_move_inst);
        }

        void insertBoundCheckLoopMin(Value* index, Constant* c_min, MCFG_Node* node_move) {
            // Find index and GEP instructions
            Instruction* indexInst = dyn_cast<Instruction>(index);
            BasicBlock::iterator backIt = indexInst;
            backIt++;
            GetElementPtrInst *GEP = static_cast<GetElementPtrInst*> ((Instruction*) (&*backIt));

            //Find array bounds
            long bottom_limit;
            std::map<Value*, ArrayInfo*>::iterator it = arrayMap.find(GEP->getPointerOperand());
            if (it != arrayMap.end()) {
                bottom_limit = it->second->bottom_limit;
            } else {
#ifdef DEBUG
                errs() << "\tWARNING! Should not be here! ArrayMapInfo!\n";
#endif
            }

            //Find last Inst of move_to block (non Terminator)
            BasicBlock* bb_move = (node_move->instrs[0])->getParent();
            Instruction* terminator_move_inst = bb_move->getTerminator();
            BasicBlock::iterator it_move_bb = terminator_move_inst;
            while (dyn_cast<TerminatorInst>(terminator_move_inst) || dyn_cast<CallInst>(terminator_move_inst)) {
                it_move_bb--;
                terminator_move_inst = &*it_move_bb;
            }

            ConstantInt* min = dyn_cast<ConstantInt>(c_min);
            AddMinCallLoop(min->getSExtValue(), bottom_limit, terminator_move_inst);

        }

        void insertInvariantCheckLoopMin(Value* index, MCFG_Node* node_move) {
            // Find index and GEP instructions
            Instruction* indexInst = dyn_cast<Instruction>(index);
            BasicBlock::iterator backIt = indexInst;
            backIt++;
            GetElementPtrInst *GEP = static_cast<GetElementPtrInst*> ((Instruction*) (&*backIt));

            //Find array bounds
            long bottom_limit;
            std::map<Value*, ArrayInfo*>::iterator it = arrayMap.find(GEP->getPointerOperand());
            if (it != arrayMap.end()) {
                bottom_limit = it->second->bottom_limit;
            } else {
#ifdef DEBUG
                errs() << "\tWARNING! Should not be here! ArrayMapInfo!\n";
#endif
            }

            //Find last Inst of move_to block (non Terminator)
            BasicBlock* bb_move = (node_move->instrs[0])->getParent();
            Instruction* terminator_move_inst = bb_move->getTerminator();
            BasicBlock::iterator it_move_bb = terminator_move_inst;
            while (dyn_cast<TerminatorInst>(terminator_move_inst) || dyn_cast<CallInst>(terminator_move_inst)) {
                it_move_bb--;
                terminator_move_inst = &*it_move_bb;
            }

            AddMinCallLoop(index, bottom_limit, terminator_move_inst);

        }

        void invariantChackLoopMoveDepInst(Value* index, MCFG_Node* node_move) {
            errs() << ">>>>>>>>>>>>>>>>>>>>>>>>>> Moving Dependents Instructions for Checks (Loop Invariant Instructions)\n";
            //Find last Inst of move_to block (non Terminator)
            BasicBlock* bb_move = (node_move->instrs[0])->getParent();
            Instruction* terminator_move_inst = bb_move->getTerminator();
            BasicBlock::iterator it_move_bb = terminator_move_inst;
            while (dyn_cast<TerminatorInst>(terminator_move_inst) || dyn_cast<CallInst>(terminator_move_inst)) {
                it_move_bb--;
                terminator_move_inst = &*it_move_bb;
            }

#ifdef DEBUG_LOOP
            errs() << "\t\tNow Done with TermInst: " << *terminator_move_inst << "\n";
#endif

            // move all instructions before it (until its load)
            Instruction* indexInst = dyn_cast<Instruction>(index);
            BasicBlock::iterator backItIndexInst = indexInst;
            while (!dyn_cast<LoadInst>(indexInst)) {
                backItIndexInst--;

                indexInst->removeFromParent();
                indexInst->insertBefore(terminator_move_inst);
                it_move_bb--;
                terminator_move_inst = &*it_move_bb;

                indexInst = dyn_cast<Instruction>(&*backItIndexInst);
            }
            LoadInst* LI = dyn_cast<LoadInst>(indexInst);
            LI->removeFromParent();
            LI->insertBefore(terminator_move_inst);
        }

        void AddMaxCallLoop(unsigned max, long top_limit, Instruction* instbefore) {
            Type * ArgTypes_i64_u64[3];
            Type * ArgTypes_i32_u64[3];
            Type * ArgTypes_i64_u32[3];
            Type * ArgTypes_i32_u32[3];

            Type *VoidTy = Type::getVoidTy(instbefore -> getContext());
            IntegerType *Int64Ty = Type::getInt64Ty(instbefore -> getContext());
            IntegerType *Int32Ty = Type::getInt32Ty(instbefore -> getContext());

            ArgTypes_i64_u64[0] = ArgTypes_i64_u32[0] = Int64Ty;
            ArgTypes_i32_u64[0] = ArgTypes_i32_u32[0] = Int32Ty;

            ArgTypes_i64_u64[1] = ArgTypes_i64_u32[1] = Int64Ty;
            ArgTypes_i32_u64[1] = ArgTypes_i32_u32[1] = Int64Ty;

            ArgTypes_i64_u64[2] = ArgTypes_i32_u64[2] = Int64Ty;
            ArgTypes_i64_u32[2] = ArgTypes_i32_u32[2] = Int32Ty;

            ArrayRef<Type*> arg_types_i32_u64(ArgTypes_i32_u64, 3);
            ArrayRef<Type*> arg_types_i32_u32(ArgTypes_i32_u32, 3);
            ArrayRef<Type*> arg_types_i64_u64(ArgTypes_i64_u64, 3);
            ArrayRef<Type*> arg_types_i64_u32(ArgTypes_i64_u32, 3);

            FunctionType *ChkType_i64_u64 = FunctionType::get(VoidTy, arg_types_i64_u64, false);
            //FunctionType *ChkType_i64_u32 = FunctionType::get(VoidTy, arg_types_i64_u32, false);
            FunctionType *ChkType_i32_u64 = FunctionType::get(VoidTy, arg_types_i32_u64, false);
            //FunctionType *ChkType_i32_u32 = FunctionType::get(VoidTy, arg_types_i32_u32, false);

            // Insert or retrieve the checking function into the program Module
            Module *M = instbefore->getParent()->getParent()->getParent();

            // Constant *Chk_i32_u32 = M->getOrInsertFunction("__checkArrayBounds_i32_u32", ChkType_i32_u32);
            Constant *Chk_i64_u64_max = M->getOrInsertFunction("__checkArrayBounds_max_i64_u64", ChkType_i64_u64);
            //Constant *Chk_i64_u32 = M->getOrInsertFunction("__checkArrayBounds_i64_u32", ChkType_i64_u32);
            Constant *Chk_i32_u64_max = M->getOrInsertFunction("__checkArrayBounds_max_i32_u64", ChkType_i32_u64);
            // Constant *Chk_i32_u32 = M->getOrInsertFunction("__checkArrayBounds_i32_u32", ChkType_i32_u32);



            // Create the arguments list
            Value * args[3];
            //Index
            args[0] = ConstantInt::get(Int64Ty, max);
            //Min
            args[1] = ConstantInt::get(Int64Ty, 0);
            //Max
            args[2] = ConstantInt::get(Int64Ty, top_limit);

            //if(dyn_cast<Instruction>(args[0]))
            //index.insert((Instruction*)args[0]);

            CallInst *hookmax;

            //errs()<<*calltocheckinfo[hook].index<<"\n";

            //MCFGInst.insert(hook);
            // Create Array Reference to the function arguments
            ArrayRef<Value*> func_args(args, 3);

            if (args[0]->getType()->getIntegerBitWidth() == 64) {

                // Create the function call

                hookmax = CallInst::Create(Chk_i64_u64_max, func_args, "");
                // Insert the function call

                hookmax->insertAfter(instbefore);

                indextocallmax[(Instruction*) args[0]] = hookmax;

                maxcalltocheckinfo[hookmax].index = args[0];
                maxcalltocheckinfo[hookmax].inst = instbefore;
                maxcalltocheckinfo[hookmax].base = NULL;
                maxcalltocheckinfo[hookmax].var.top_limit = top_limit;
                maxcalltocheckinfo[hookmax].var.bottom_limit = 0;


            } else if (args[0]->getType()->getIntegerBitWidth() == 32) {

                // Create the function call

                hookmax = CallInst::Create(Chk_i32_u64_max, func_args, "");
                // Insert the function call

                hookmax->insertAfter(instbefore);

                indextocallmax[(Instruction*) args[0]] = hookmax;

                maxcalltocheckinfo[hookmax].index = args[0];
                maxcalltocheckinfo[hookmax].inst = instbefore;
                maxcalltocheckinfo[hookmax].base = NULL;
                maxcalltocheckinfo[hookmax].var.top_limit = top_limit;
                maxcalltocheckinfo[hookmax].var.bottom_limit = 0;
            } else {
#ifdef DEBUG
                errs() << "Shouldn't come here\n";
#endif
            }

        }

        void AddMaxCallLoop(Value* index, long top_limit, Instruction* instbefore) {
            Type * ArgTypes_i64_u64[3];
            Type * ArgTypes_i32_u64[3];
            Type * ArgTypes_i64_u32[3];
            Type * ArgTypes_i32_u32[3];

            Type *VoidTy = Type::getVoidTy(instbefore -> getContext());
            IntegerType *Int64Ty = Type::getInt64Ty(instbefore -> getContext());
            IntegerType *Int32Ty = Type::getInt32Ty(instbefore -> getContext());

            ArgTypes_i64_u64[0] = ArgTypes_i64_u32[0] = Int64Ty;
            ArgTypes_i32_u64[0] = ArgTypes_i32_u32[0] = Int32Ty;

            ArgTypes_i64_u64[1] = ArgTypes_i64_u32[1] = Int64Ty;
            ArgTypes_i32_u64[1] = ArgTypes_i32_u32[1] = Int64Ty;

            ArgTypes_i64_u64[2] = ArgTypes_i32_u64[2] = Int64Ty;
            ArgTypes_i64_u32[2] = ArgTypes_i32_u32[2] = Int32Ty;

            ArrayRef<Type*> arg_types_i32_u64(ArgTypes_i32_u64, 3);
            ArrayRef<Type*> arg_types_i32_u32(ArgTypes_i32_u32, 3);
            ArrayRef<Type*> arg_types_i64_u64(ArgTypes_i64_u64, 3);
            ArrayRef<Type*> arg_types_i64_u32(ArgTypes_i64_u32, 3);

            FunctionType *ChkType_i64_u64 = FunctionType::get(VoidTy, arg_types_i64_u64, false);
            //FunctionType *ChkType_i64_u32 = FunctionType::get(VoidTy, arg_types_i64_u32, false);
            FunctionType *ChkType_i32_u64 = FunctionType::get(VoidTy, arg_types_i32_u64, false);
            //FunctionType *ChkType_i32_u32 = FunctionType::get(VoidTy, arg_types_i32_u32, false);

            // Insert or retrieve the checking function into the program Module
            Module *M = instbefore->getParent()->getParent()->getParent();

            // Constant *Chk_i32_u32 = M->getOrInsertFunction("__checkArrayBounds_i32_u32", ChkType_i32_u32);
            Constant *Chk_i64_u64_max = M->getOrInsertFunction("__checkArrayBounds_max_i64_u64", ChkType_i64_u64);
            //Constant *Chk_i64_u32 = M->getOrInsertFunction("__checkArrayBounds_i64_u32", ChkType_i64_u32);
            Constant *Chk_i32_u64_max = M->getOrInsertFunction("__checkArrayBounds_max_i32_u64", ChkType_i32_u64);
            // Constant *Chk_i32_u32 = M->getOrInsertFunction("__checkArrayBounds_i32_u32", ChkType_i32_u32);



            // Create the arguments list
            Value * args[3];
            //Index
            args[0] = index;
            //Min
            args[1] = ConstantInt::get(Int64Ty, 0);
            //Max
            args[2] = ConstantInt::get(Int64Ty, top_limit);

            //if(dyn_cast<Instruction>(args[0]))
            //index.insert((Instruction*)args[0]);

            CallInst *hookmax;

            //errs()<<*calltocheckinfo[hook].index<<"\n";

            //MCFGInst.insert(hook);
            // Create Array Reference to the function arguments
            ArrayRef<Value*> func_args(args, 3);

            if (args[0]->getType()->getIntegerBitWidth() == 64) {

                // Create the function call

                hookmax = CallInst::Create(Chk_i64_u64_max, func_args, "");
                // Insert the function call

                hookmax->insertAfter(instbefore);

                indextocallmax[(Instruction*) args[0]] = hookmax;

                maxcalltocheckinfo[hookmax].index = args[0];
                maxcalltocheckinfo[hookmax].inst = instbefore;
                maxcalltocheckinfo[hookmax].base = NULL;
                maxcalltocheckinfo[hookmax].var.top_limit = top_limit;
                maxcalltocheckinfo[hookmax].var.bottom_limit = 0;


            } else if (args[0]->getType()->getIntegerBitWidth() == 32) {

                // Create the function call

                hookmax = CallInst::Create(Chk_i32_u64_max, func_args, "");
                // Insert the function call

                hookmax->insertAfter(instbefore);

                indextocallmax[(Instruction*) args[0]] = hookmax;

                maxcalltocheckinfo[hookmax].index = args[0];
                maxcalltocheckinfo[hookmax].inst = instbefore;
                maxcalltocheckinfo[hookmax].base = NULL;
                maxcalltocheckinfo[hookmax].var.top_limit = top_limit;
                maxcalltocheckinfo[hookmax].var.bottom_limit = 0;
            } else {
#ifdef DEBUG
                errs() << "Shouldn't come here\n";
#endif
            }
        }

        void AddMinCallLoop(unsigned min, long bottom_limit, Instruction* instbefore) {
            Type * ArgTypes_i64_u64[3];
            Type * ArgTypes_i32_u64[3];
            Type * ArgTypes_i64_u32[3];
            Type * ArgTypes_i32_u32[3];

            Type *VoidTy = Type::getVoidTy(instbefore -> getContext());
            IntegerType *Int64Ty = Type::getInt64Ty(instbefore -> getContext());
            IntegerType *Int32Ty = Type::getInt32Ty(instbefore -> getContext());

            ArgTypes_i64_u64[0] = ArgTypes_i64_u32[0] = Int64Ty;
            ArgTypes_i32_u64[0] = ArgTypes_i32_u32[0] = Int32Ty;

            ArgTypes_i64_u64[1] = ArgTypes_i64_u32[1] = Int64Ty;
            ArgTypes_i32_u64[1] = ArgTypes_i32_u32[1] = Int64Ty;

            ArgTypes_i64_u64[2] = ArgTypes_i32_u64[2] = Int64Ty;
            ArgTypes_i64_u32[2] = ArgTypes_i32_u32[2] = Int32Ty;

            ArrayRef<Type*> arg_types_i32_u64(ArgTypes_i32_u64, 3);
            ArrayRef<Type*> arg_types_i32_u32(ArgTypes_i32_u32, 3);
            ArrayRef<Type*> arg_types_i64_u64(ArgTypes_i64_u64, 3);
            ArrayRef<Type*> arg_types_i64_u32(ArgTypes_i64_u32, 3);

            FunctionType *ChkType_i64_u64 = FunctionType::get(VoidTy, arg_types_i64_u64, false);
            //FunctionType *ChkType_i64_u32 = FunctionType::get(VoidTy, arg_types_i64_u32, false);
            FunctionType *ChkType_i32_u64 = FunctionType::get(VoidTy, arg_types_i32_u64, false);
            //FunctionType *ChkType_i32_u32 = FunctionType::get(VoidTy, arg_types_i32_u32, false);

            // Insert or retrieve the checking function into the program Module
            Module *M = instbefore->getParent()->getParent()->getParent();

            // Constant *Chk_i32_u32 = M->getOrInsertFunction("__checkArrayBounds_i32_u32", ChkType_i32_u32);
            Constant *Chk_i64_u64_min = M->getOrInsertFunction("__checkArrayBounds_min_i64_u64", ChkType_i64_u64);
            //Constant *Chk_i64_u32 = M->getOrInsertFunction("__checkArrayBounds_i64_u32", ChkType_i64_u32);
            Constant *Chk_i32_u64_min = M->getOrInsertFunction("__checkArrayBounds_min_i32_u64", ChkType_i32_u64);
            // Constant *Chk_i32_u32 = M->getOrInsertFunction("__checkArrayBounds_i32_u32", ChkType_i32_u32);



            // Create the arguments list
            Value * args[3];
            //Index
            args[0] = ConstantInt::get(Int64Ty, min);
            //Min
            args[1] = ConstantInt::get(Int64Ty, bottom_limit);
            //Max
            args[2] = ConstantInt::get(Int64Ty, 0);




            //if(dyn_cast<Instruction>(args[0]))
            //index.insert((Instruction*)args[0]);

            CallInst *hookmin;



            //errs()<<*calltocheckinfo[hook].index<<"\n";

            //MCFGInst.insert(hook);
            // Create Array Reference to the function arguments
            ArrayRef<Value*> func_args(args, 3);

            if (args[0]->getType()->getIntegerBitWidth() == 64) {

                // Create the function call

                hookmin = CallInst::Create(Chk_i64_u64_min, func_args, "");
                // Insert the function call

                hookmin->insertAfter(instbefore);

                indextocallmin[(Instruction*) args[0]] = hookmin;

                mincalltocheckinfo[hookmin].index = args[0];
                mincalltocheckinfo[hookmin].inst = instbefore;
                mincalltocheckinfo[hookmin].base = NULL;
                mincalltocheckinfo[hookmin].var.top_limit = 0;
                mincalltocheckinfo[hookmin].var.bottom_limit = bottom_limit;

            } else if (args[0]->getType()->getIntegerBitWidth() == 32) {

                // Create the function call

                hookmin = CallInst::Create(Chk_i32_u64_min, func_args, "");
                // Insert the function call

                hookmin->insertAfter(instbefore);

                indextocallmin[(Instruction*) args[0]] = hookmin;

                mincalltocheckinfo[hookmin].index = args[0];
                mincalltocheckinfo[hookmin].inst = instbefore;
                mincalltocheckinfo[hookmin].base = NULL;
                mincalltocheckinfo[hookmin].var.top_limit = 0;
                mincalltocheckinfo[hookmin].var.bottom_limit = bottom_limit;
                ;
            } else {
#ifdef DEBUG
                errs() << "Shouldn't come here\n";
#endif
            }


        }

        void AddMinCallLoop(Value* index, long bottom_limit, Instruction* instbefore) {
            Type * ArgTypes_i64_u64[3];
            Type * ArgTypes_i32_u64[3];
            Type * ArgTypes_i64_u32[3];
            Type * ArgTypes_i32_u32[3];

            Type *VoidTy = Type::getVoidTy(instbefore -> getContext());
            IntegerType *Int64Ty = Type::getInt64Ty(instbefore -> getContext());
            IntegerType *Int32Ty = Type::getInt32Ty(instbefore -> getContext());

            ArgTypes_i64_u64[0] = ArgTypes_i64_u32[0] = Int64Ty;
            ArgTypes_i32_u64[0] = ArgTypes_i32_u32[0] = Int32Ty;

            ArgTypes_i64_u64[1] = ArgTypes_i64_u32[1] = Int64Ty;
            ArgTypes_i32_u64[1] = ArgTypes_i32_u32[1] = Int64Ty;

            ArgTypes_i64_u64[2] = ArgTypes_i32_u64[2] = Int64Ty;
            ArgTypes_i64_u32[2] = ArgTypes_i32_u32[2] = Int32Ty;

            ArrayRef<Type*> arg_types_i32_u64(ArgTypes_i32_u64, 3);
            ArrayRef<Type*> arg_types_i32_u32(ArgTypes_i32_u32, 3);
            ArrayRef<Type*> arg_types_i64_u64(ArgTypes_i64_u64, 3);
            ArrayRef<Type*> arg_types_i64_u32(ArgTypes_i64_u32, 3);

            FunctionType *ChkType_i64_u64 = FunctionType::get(VoidTy, arg_types_i64_u64, false);
            //FunctionType *ChkType_i64_u32 = FunctionType::get(VoidTy, arg_types_i64_u32, false);
            FunctionType *ChkType_i32_u64 = FunctionType::get(VoidTy, arg_types_i32_u64, false);
            //FunctionType *ChkType_i32_u32 = FunctionType::get(VoidTy, arg_types_i32_u32, false);

            // Insert or retrieve the checking function into the program Module
            Module *M = instbefore->getParent()->getParent()->getParent();

            // Constant *Chk_i32_u32 = M->getOrInsertFunction("__checkArrayBounds_i32_u32", ChkType_i32_u32);
            Constant *Chk_i64_u64_min = M->getOrInsertFunction("__checkArrayBounds_min_i64_u64", ChkType_i64_u64);
            //Constant *Chk_i64_u32 = M->getOrInsertFunction("__checkArrayBounds_i64_u32", ChkType_i64_u32);
            Constant *Chk_i32_u64_min = M->getOrInsertFunction("__checkArrayBounds_min_i32_u64", ChkType_i32_u64);
            // Constant *Chk_i32_u32 = M->getOrInsertFunction("__checkArrayBounds_i32_u32", ChkType_i32_u32);



            // Create the arguments list
            Value * args[3];
            //Index
            args[0] = index;
            //Min
            args[1] = ConstantInt::get(Int64Ty, bottom_limit);
            //Max
            args[2] = ConstantInt::get(Int64Ty, 0);




            //if(dyn_cast<Instruction>(args[0]))
            //index.insert((Instruction*)args[0]);

            CallInst *hookmin;



            //errs()<<*calltocheckinfo[hook].index<<"\n";

            //MCFGInst.insert(hook);
            // Create Array Reference to the function arguments
            ArrayRef<Value*> func_args(args, 3);

            if (args[0]->getType()->getIntegerBitWidth() == 64) {

                // Create the function call

                hookmin = CallInst::Create(Chk_i64_u64_min, func_args, "");
                // Insert the function call

                hookmin->insertAfter(instbefore);

                indextocallmin[(Instruction*) args[0]] = hookmin;

                mincalltocheckinfo[hookmin].index = args[0];
                mincalltocheckinfo[hookmin].inst = instbefore;
                mincalltocheckinfo[hookmin].base = NULL;
                mincalltocheckinfo[hookmin].var.top_limit = 0;
                mincalltocheckinfo[hookmin].var.bottom_limit = bottom_limit;

            } else if (args[0]->getType()->getIntegerBitWidth() == 32) {

                // Create the function call

                hookmin = CallInst::Create(Chk_i32_u64_min, func_args, "");
                // Insert the function call

                hookmin->insertAfter(instbefore);

                indextocallmin[(Instruction*) args[0]] = hookmin;

                mincalltocheckinfo[hookmin].index = args[0];
                mincalltocheckinfo[hookmin].inst = instbefore;
                mincalltocheckinfo[hookmin].base = NULL;
                mincalltocheckinfo[hookmin].var.top_limit = 0;
                mincalltocheckinfo[hookmin].var.bottom_limit = bottom_limit;
                ;
            } else {
#ifdef DEBUG
                errs() << "Shouldn't come here\n";
#endif
            }


        }

        /////////////////////////////////////
        /////////////////////////////////////


    };

    bool LightCheck::identicalValue(Value* left, Value* right) {
        if (left == right) return true;
        if ((dyn_cast<Instruction>(left))) {
            if (dyn_cast<Instruction>(right)) {
                if (instIsIdentical((Instruction*) left, (Instruction*) right)) {
                    return true;
                }
            }
        } else {
            errs() << "excepion happened in identicalValue()\n";
        }
        return false;

        /*    std::deque<User*> lefth, righth;

                recordHistory(left, &lefth);
                recordHistory(right, &righth);
                if(lefth.size() !=righth.size())
                        return false;
                for(unsigned int i=0;i<lefth.size();i++)
                        if(lefth[i]!=righth[i])
                                return false;
                return true;
         */
    }

    /*
    void recordHistory(Value* value, std::deque<User*>* hist) {

    if(dyn_cast<Instruction>(value)){
            Instruction* inst =dyn_cast<Instruction>(value);
            std::stack<llvm::User*> worklist;



    worklist.push((User*)value);
    while (!worklist.empty()){
            Value* cur=worklist.top();
            worklist.pop();


             hist.push(cur->getOperator());
            for (Instruction::op_iterator opI = (*inst).op_begin();opI != (*inst).op_end(); ++opI) {
                    if(dyn_cast<Instruction>(*opI)){

                                    if(dyn_cast<LoadInst>(*opI))
                                            hist.push((LoadInst*)(*opI)->getPointerOperand());
                                    if(dyn_cast<ConstantInst>(*opI))
                                            hist.push(*opI)
                                    if else(dyn_cast<PHINode>(*opI))
                                                    hist.push(*opI);
                                    else{
                                            hist.push((*opI)->getOperator());
                                            worklist.push(*opI);
                                    }

                    }


            }

    }


    }
    else
            hist.push(value);


    errs()<<"record for:"<<*value<<"is: \n";
            std::stack<User*> s;

    while(!stack.empty()){
            errs()<<*stack.top()<<" ";
            s.push(stack.top());
            stack.pop();

    }

    }
     */
    bool LightCheck::instIsIdentical(Instruction* l, Instruction* r) {
        if (l->getOpcode() != r->getOpcode()) {
            //errs()<<"opcodes: "<<l->getOpcode()<<" "<<r->getOpcode()<<"\n";
            return false;
        } else {
            //        errs()<<"identical?"<<"\n";
            //        errs()<<*l<<"  " <<*r<<"\n";
            if (l->getOpcode() == Instruction::Add || l->getOpcode() == Instruction::Mul ||
                    l->getOpcode() == Instruction::FAdd || l->getOpcode() == Instruction::FMul) {
                if (l->getOperand(0)->getValueID() == Value::ConstantIntVal && r->getOperand(0)->getValueID() == Value::ConstantIntVal) {
                    if (constIsSame(r->getOperand(0), r->getOperand(0))) {
                        if (l->getOperand(1)->getValueID() == Value::ConstantIntVal && r->getOperand(1)->getValueID() == Value::ConstantIntVal)
                            return constIsSame(l->getOperand(1), r->getOperand(1));
                        else
                            return instIsIdentical(static_cast<Instruction*> (l->getOperand(1)), static_cast<Instruction*> (r->getOperand(1)));
                    } else
                        return false;
                } else if (l->getOperand(0)->getValueID() == Value::ConstantIntVal && r->getOperand(1)->getValueID() == Value::ConstantIntVal) {
                    if (constIsSame(r->getOperand(0), r->getOperand(1))) {
                        if (l->getOperand(1)->getValueID() == Value::ConstantIntVal && r->getOperand(0)->getValueID() == Value::ConstantIntVal)
                            return constIsSame(l->getOperand(1), r->getOperand(0));
                        else
                            return instIsIdentical(static_cast<Instruction*> (l->getOperand(1)), static_cast<Instruction*> (r->getOperand(0)));
                    } else
                        return false;
                } else if (l->getOperand(1)->getValueID() == Value::ConstantIntVal && r->getOperand(0)->getValueID() == Value::ConstantIntVal) {
                    if (constIsSame(r->getOperand(1), r->getOperand(0))) {
                        if (l->getOperand(0)->getValueID() == Value::ConstantIntVal && r->getOperand(1)->getValueID() == Value::ConstantIntVal)
                            return constIsSame(l->getOperand(0), r->getOperand(1));
                        else
                            return instIsIdentical(static_cast<Instruction*> (l->getOperand(0)), static_cast<Instruction*> (r->getOperand(1)));
                    } else
                        return false;

                } else if (l->getOperand(1)->getValueID() == Value::ConstantIntVal && r->getOperand(1)->getValueID() == Value::ConstantIntVal) {
                    if (constIsSame(r->getOperand(1), r->getOperand(1))) {
                        if (l->getOperand(0)->getValueID() == Value::ConstantIntVal && r->getOperand(0)->getValueID() == Value::ConstantIntVal)
                            return constIsSame(l->getOperand(0), r->getOperand(0));
                        else
                            return instIsIdentical(static_cast<Instruction*> (l->getOperand(0)), static_cast<Instruction*> (r->getOperand(0)));
                    } else
                        return false;
                } else {
                    bool case1a = instIsIdentical(static_cast<Instruction*> (l->getOperand(0)), static_cast<Instruction*> (r->getOperand(0)));
                    bool case1b = instIsIdentical(static_cast<Instruction*> (l->getOperand(1)), static_cast<Instruction*> (r->getOperand(1)));
                    bool case2a = instIsIdentical(static_cast<Instruction*> (l->getOperand(0)), static_cast<Instruction*> (r->getOperand(1)));
                    bool case2b = instIsIdentical(static_cast<Instruction*> (l->getOperand(1)), static_cast<Instruction*> (r->getOperand(0)));
                    return (case1a && case1b) || (case2a && case2b);
                }
            } else if (l->getOpcode() == Instruction::Sub || l->getOpcode() == Instruction::FSub ||
                    l->getOpcode() == Instruction::UDiv || l->getOpcode() == Instruction::SDiv || l->getOpcode() == Instruction::FDiv) {
                if (l->getOperand(0)->getValueID() == Value::ConstantIntVal && r->getOperand(0)->getValueID() == Value::ConstantIntVal) {
                    if (constIsSame(r->getOperand(0), r->getOperand(0))) {
                        if (l->getOperand(1)->getValueID() == Value::ConstantIntVal && r->getOperand(1)->getValueID() == Value::ConstantIntVal)
                            return constIsSame(l->getOperand(1), r->getOperand(1));
                        else
                            return instIsIdentical(static_cast<Instruction*> (l->getOperand(1)), static_cast<Instruction*> (r->getOperand(1)));
                    } else
                        return false;
                } else if (l->getOperand(1)->getValueID() == Value::ConstantIntVal && r->getOperand(1)->getValueID() == Value::ConstantIntVal) {
                    if (constIsSame(r->getOperand(1), r->getOperand(1))) {
                        if (l->getOperand(0)->getValueID() == Value::ConstantIntVal && r->getOperand(0)->getValueID() == Value::ConstantIntVal)
                            return constIsSame(l->getOperand(0), r->getOperand(0));
                        else
                            return instIsIdentical(static_cast<Instruction*> (l->getOperand(0)), static_cast<Instruction*> (r->getOperand(0)));
                    } else
                        return false;
                } else {
                    bool case1a = instIsIdentical(static_cast<Instruction*> (l->getOperand(0)), static_cast<Instruction*> (r->getOperand(0)));
                    bool case1b = instIsIdentical(static_cast<Instruction*> (l->getOperand(1)), static_cast<Instruction*> (r->getOperand(1)));
                    return (case1a && case1b);
                }
            } else if (l->getOpcode() == Instruction::Load)
                return (l->getOperand(0) == r->getOperand(0));
            else if (l->getOpcode() == Instruction::SExt || l->getOpcode() == Instruction::ZExt)
                return instIsIdentical(static_cast<Instruction*> (l->getOperand(0)), static_cast<Instruction*> (r->getOperand(0)));
            else
                return false;

        }
        return false;
    }

    bool LightCheck::live(Instruction* l, Instruction* r) {
        if (l->getOperand(0) != r->getOperand(0)) return false;
        if (l == r) return true;
        BasicBlock* bb = l->getParent();
        Instruction* var = (Instruction*) r->getOperand(0);
        for (BasicBlock::iterator inst = bb->begin(); inst != bb->end(); ++inst) {

            if (((&*inst) == l) || ((&*inst) == r)) {
                inst++;
                for (BasicBlock::iterator inst2 = inst; inst2 != bb->end(); ++inst2) {
                    if (((&*inst2) == l) || ((&*inst2) == r)) {
                        return true;
                    }
                    if (dyn_cast<StoreInst>(inst2)) {
                        if (var == inst2->getOperand(1)) return false;
                    }
                }
            }
        }

        //errs()<<"exception in live!!\n";
        return true;
    }

    bool LightCheck::constIsSame(Value* l, Value* r) {
        ConstantInt* leftArg = static_cast<ConstantInt*> (l);
        ConstantInt* rightArg = static_cast<ConstantInt*> (r);
        int64_t leftConst = leftArg->getZExtValue();
        int64_t rightConst = rightArg->getZExtValue();
        return (leftConst == rightConst);
    }
}
//std::map<Value*,  ArrayInfo*> Project::arrayMap;

// Static variables for Part2 - Global eliminate



char Project::ID = 0;
// Register Project pass command
static RegisterPass<Project> X("project", "project tests");

