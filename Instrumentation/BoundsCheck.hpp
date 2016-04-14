
// BoundsCheck Class
class BoundsCheck
{
  public:
    BoundsCheck(Instruction *inst, Value *ptr, Value *ind, Value* off, Value *ub_val);
    ~BoundsCheck();
    

    Value*  getPointer();
    Value*  getUpperBound();
    Value*  getIndex();
    Value*  getOffset();
    Value*  getVariable();
    void setIndex(Value *val);
    void setOffset(Value *val);
    bool hasLowerBoundsCheck();
    bool hasUpperBoundsCheck();
    void deleteLowerBoundsCheck();
    void deleteUpperBoundsCheck();
    
    Instruction* getInsertPoint();
    void insertBefore(Instruction* I, bool upper);

    bool isCopy();
    bool stillExists();
    bool moveCheck();
    bool shouldHoistCheck();
    void print();
    
    void addLowerBoundsCheck();
    void addUpperBoundsCheck();
    Instruction* getInstruction();

    void hoistCheck(BasicBlock *blk);
    uint64_t lowerBoundValue();
    uint64_t upperBoundValue();
    
    void setVariable(Value *v, int64_t w, bool known);
    std::vector<Instruction*> dependentInsts;
    int64_t comparedToVar;
    bool comparisonKnown;
    
    void restoreOriginalCheck();
    BoundsCheck* createCopyAt(BasicBlock *blk);
    BoundsCheck* originalCheck;
    BasicBlock *originalBlock;
  private:
    // Value associated with the check
    Value *pointer;
    Instruction *inst;
    Instruction *insertLoc;
    Instruction *insertLBloc;
    Instruction *insertUBloc;
    bool move_check;
    Value *index;
    Value *offset;

    BasicBlock *hoistBlock;
    bool hoist_check;

    uint64_t lower_bound;
    bool lower_bound_static;
    uint64_t upper_bound;
    bool upper_bound_static;
    Value *upper_bound_value;
    bool insert_lower_bound;
    bool insert_upper_bound;

    Value *var;
    bool is_copy;
};


BoundsCheck::BoundsCheck(Instruction *I, Value *ptr, Value *ind, Value* off, Value *ub_val) 
{
  pointer = ptr;
  inst = I;
  index = ind;
  upper_bound_value = ub_val;
  offset = off;
  insertLoc = I;
  insertLBloc = I;
  insertUBloc = I;
  move_check = false;
  lower_bound = 0;
  lower_bound_static = true;
  
  originalBlock = inst->getParent();
  hoistBlock = NULL;
  hoist_check = false;
  
  originalCheck = NULL;
  ConstantInt *ub_const = dyn_cast<ConstantInt>(ub_val);
  if (ub_const != NULL) {
    upper_bound = ub_const->getZExtValue();
    upper_bound_static = true;
  } else {
    upper_bound_static = false;
  }
  
  var = NULL;  
  comparedToVar = 0;
  comparisonKnown = false;
  insert_lower_bound = true;
  insert_upper_bound = true;
  is_copy = false;
}


BoundsCheck::~BoundsCheck() 
{
}

void BoundsCheck::setIndex(Value *val) 
{
  index = val;
}

void BoundsCheck::setOffset(Value *val)
{
  offset = val;
}

void BoundsCheck::restoreOriginalCheck() 
{
  if (originalCheck == NULL)
    return;

  if (insert_lower_bound) {
    originalCheck->insert_lower_bound = true;
  }
  
  if (insert_upper_bound) {
    originalCheck->insert_upper_bound = true;
  }
}

BoundsCheck* BoundsCheck::createCopyAt(BasicBlock *blk)
{
  BoundsCheck *check = new BoundsCheck(inst, pointer, index, offset, upper_bound_value);
  check->originalCheck = this;
  check->is_copy = true;
  check->insertLoc = blk->getTerminator();
  check->insertUBloc = insertLoc;
  check->insertLBloc = insertLoc;
  check->comparedToVar = comparedToVar;
  check->var = var;
  check->comparisonKnown = comparisonKnown;
  return check;
}

bool BoundsCheck::shouldHoistCheck()
{
  return hoist_check;
}

bool BoundsCheck::isCopy()
{
  return is_copy;
}


void BoundsCheck::hoistCheck(BasicBlock *blk)
{
  hoistBlock = blk; 
  insertLoc = blk->getTerminator();
  
  insertUBloc = insertLoc;
  insertLBloc = insertLoc;
  hoist_check = true;
  move_check = true;
}

void BoundsCheck::setVariable(Value *v, int64_t w, bool known)
{
  var = v;
  comparedToVar = w;
  comparisonKnown = known;
}

Instruction* BoundsCheck::getInstruction() {
  return inst;
}
Value* BoundsCheck::getUpperBound() {
  return upper_bound_value;
}


Value* BoundsCheck::getIndex() {
  return index;
}

Value* BoundsCheck::getOffset() {
  return offset;
}

Value* BoundsCheck::getPointer() {
  return pointer;
}

Instruction* BoundsCheck::getInsertPoint() {
  return insertLoc;
}

Value* BoundsCheck::getVariable() {
  return var;
}

void BoundsCheck::insertBefore(Instruction *inst, bool upper) {
  move_check = true;
  insertLoc = inst;
  if (upper)
    insertUBloc = inst;
  else
    insertLBloc = inst;
}

bool BoundsCheck::moveCheck() {
  return move_check;
}

bool BoundsCheck::stillExists() {
  return insert_lower_bound || insert_upper_bound;
}

void BoundsCheck::print()
{
  errs() << "Instruction: " << *inst << "\n";
  if (insert_lower_bound) {
    errs() << "Lower Bound Check: " << lower_bound << "\n";
  } else {
    errs() << "Lower Bound Check (DELETED): " << lower_bound << "\n";
  }

  if (insert_upper_bound) {
    errs() << "Upper Bound Check: " << *upper_bound_value << "\n";
  } else {
    errs() << "Upper Bound Check (DELETED): " << *upper_bound_value << "\n";
  }
  errs() << "Index: " << *index << "\n";
  errs() << "Offset: " << *offset << "\n";
  errs() << "Moving Check: " << (move_check ? "Yes": "No") << "\n";
  errs() << "Insert Point: " << *insertLoc << "\n";
  if (hoist_check) {
    errs() << "Hoisted to Block: " << hoistBlock->getName() << "\n";
  }
  if (var != NULL) {
    if (comparisonKnown) {
      if (comparedToVar > 0)
        errs() << "Index: GREATER_THAN";
      else if (comparedToVar < 0)
        errs() << "Index: LESS_THAN";
      else
        errs() << "Index: EQUALS";
      errs() << " variable: " << var->getName() << "\n";
    } else {
      errs() << "Index: UNKNOWN compared to variable: " << var->getName() << "\n";
    }
  }
}

uint64_t BoundsCheck::lowerBoundValue() 
{
  return lower_bound;
}

uint64_t BoundsCheck::upperBoundValue() 
{
  return upper_bound;
}

bool BoundsCheck::hasLowerBoundsCheck() 
{
  return insert_lower_bound;
}

void BoundsCheck::addLowerBoundsCheck() {
  insert_lower_bound = true;
}

void BoundsCheck::deleteLowerBoundsCheck() {
  insert_lower_bound = false;
}


bool BoundsCheck::hasUpperBoundsCheck() 
{
  return insert_upper_bound;
}

void BoundsCheck::addUpperBoundsCheck() {
  insert_upper_bound = true;
}

void BoundsCheck::deleteUpperBoundsCheck() {
  insert_upper_bound = false;
}
