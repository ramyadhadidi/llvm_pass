
class GlobalCheck
{
  public:
    GlobalCheck(BoundsCheck *chk, Value *v, Value *b, bool isU, int loc);
    BoundsCheck *check;
    Value *var;
    Value* bound;
    int insertLoc;
    bool isUpper;
    void print();
};

GlobalCheck::GlobalCheck(BoundsCheck *chk, Value *v, Value *b, bool isU, int loc)
{
  check = chk;
  var = v;
  bound = b;
  isUpper = isU;
  insertLoc = loc;
}

void GlobalCheck::print()
{
  if (isUpper) {
    errs() << "(" << var->getName() << " < " << *(bound) << ")\n";
  } else {
    errs() << "(0 < " << var->getName() << ")\n";
  }
}
class InSet
{
  public:
    InSet();
    std::vector<GlobalCheck*> checks;
    bool allChecks;
    void addCheck(GlobalCheck* chk);
};

InSet::InSet()
{
  allChecks = false;
}

void InSet::addCheck(GlobalCheck* chk)
{
  checks.push_back(chk);
}

class OutSet
{
  public:

    OutSet();
    ~OutSet();
    bool allChecks;
    std::vector<GlobalCheck*> checks;

    bool addAvailableCheck(GlobalCheck *chk);
};

OutSet::OutSet()
{
  allChecks = true;
}

OutSet::~OutSet()
{
}

bool OutSet::addAvailableCheck(GlobalCheck *chk)
{
 if (std::find(checks.begin(), checks.end(), chk) != checks.end()) {
  // Already exists in out set
  return false;
 } else {
  allChecks = false;
  checks.push_back(chk);
  return true;
 }
}


class BlockFlow
{
  public:
    enum ChangeType {
      UNKNOWN,
      LESS_THAN,
      EQUALS,
      GREATER_THAN
    };
    std::map<Value*, BlockFlow::ChangeType> varChanges;
    BlockFlow(BasicBlock *b, std::vector<BoundsCheck*> *chks, ConstraintGraph *graph, std::map<BasicBlock*,BlockFlow*> *f);
    ~BlockFlow();

    BasicBlock *blk;
    InSet inSet;
    OutSet outSet;
    std::map<BasicBlock*,BlockFlow*> *flows;
    bool isEntry;
    std::set<Value*> storeSet;

    void identifyInSet();
    bool identifyOutSet();
    void eliminateRedundantChecks();
    void print();
  private:
    unsigned int numInsts;
    std::vector<Instruction*> instructions;
    std::vector<BoundsCheck*> *checks;
    std::vector<GlobalCheck*> globalChecks;
    ConstraintGraph *cg;
    std::map<Instruction*, unsigned int> instLoc;
    std::map<Value*, unsigned int> lastStoreLoc;
    bool killAll;
    unsigned int killAllLoc;
};

BlockFlow::BlockFlow(BasicBlock *b, std::vector<BoundsCheck*> *chks, ConstraintGraph *graph, std::map<BasicBlock*,BlockFlow*> *f)
{
  blk = b;
  flows = f;
  checks = chks;
  cg = graph;
  numInsts = 0;
  isEntry = false;
  outSet.allChecks = true;
  killAll = false;
  killAllLoc = 0;
  for (BasicBlock::iterator i = blk->begin(), e = blk->end(); i != e; ++i) {
    Instruction *inst = &*i;
    numInsts++;
    instructions.push_back(inst);
    instLoc[&*i] = numInsts;
    StoreInst *SI = dyn_cast<StoreInst>(inst);
    if (isa<CallInst>(inst)) {
      killAll = true;
      killAllLoc = numInsts;
    }
    if (SI != NULL) {
      storeSet.insert(SI->getPointerOperand());
      Value *to = SI->getPointerOperand();
      Type* T = to->getType();
      bool isPointer = T->isPointerTy() && T->getContainedType(0)->isPointerTy();
      if (isPointer) {
        killAll = true;
        killAllLoc = numInsts;
      }
      lastStoreLoc[to] = numInsts;
    }
  }

#if DEBUG_GLOBAL
  errs() << "Identifying Downward Exposed Bounds Checks:" << blk->getName() << "\n";
#endif
  for (std::vector<BoundsCheck*>::iterator it = checks->begin(), et = checks->end(); it != et; it++) {
    BoundsCheck *chk = *it;
    if (!chk->stillExists())
      continue;

    // May require fixing later?
    unsigned int loc = instLoc[chk->getInsertPoint()];
    Value *var = chk->getVariable();
    if (var == NULL) {
      var = chk->getIndex();
      if (var == NULL) {
        errs() << "Could not identify index value for following check:\n";
        chk->print();
        continue;
      }
    }
    /**
    bool downwardExposed = true;
    // Find downward exposed checks
    // Inefficient, basically go through the remaining instructions
    // and see if there is a store to the same location
    for (unsigned int i = loc; i <= numInsts; i++) {
      Instruction *inst = instructions.at(i-1);
      StoreInst *SI = dyn_cast<StoreInst>(inst);
      if (isa<CallInst>(inst)) {
        downwardExposed = false;
      #if DEBUG_GLOBAL
        errs() << "Following Check is not downward exposed\n";
        chk->print();
      #endif
        break;
      }
      if (SI != NULL) {
        if (var == SI->getPointerOperand()) {
          downwardExposed = false;
        #if DEBUG_GLOBAL
          errs() << "Following Check is not downward exposed\n";
          chk->print();
        #endif
          break;
        } else {
          Value *to = SI->getPointerOperand();
          Type* T = to->getType();
          bool isPointer = T->isPointerTy() && T->getContainedType(0)->isPointerTy();
          if (isPointer) {
            downwardExposed = false;
          #if DEBUG_GLOBAL
            errs() << "Following Check is not downward exposed\n";
            chk->print();
          #endif
            break;
          }
        }
      }
    }

    if (!downwardExposed)
      continue;
    **/
    if (loc < killAllLoc) {
      continue;
    }

    bool blkHasStore = false;
    if (lastStoreLoc.find(var) != lastStoreLoc.end()) {
      blkHasStore = true;
    }

    GlobalCheck *gCheck;
    // Insert lower bounds check if downward exposed, and index <= to variable it references
    if (chk->hasLowerBoundsCheck()){
      if (chk->comparisonKnown && chk->comparedToVar <= 0) {
        bool add = true;
        ConstraintGraph::CompareEnum varChange = cg->identifyMemoryChange(var);
        // Check if there is a store instruction after the check
        if (blkHasStore && (lastStoreLoc[var] > loc)) {
          // If index variable becomes smaller across basic block, can't add lower bounds check
          if (varChange == ConstraintGraph::LESS_THAN || varChange == ConstraintGraph::UNKNOWN) {
            add = false;
          }
        }
        for (std::vector<GlobalCheck*>::iterator gi = globalChecks.begin(), ge = globalChecks.end();
              gi != ge; gi++) {
          GlobalCheck *c = *gi;
          if (!c->isUpper && (c->var == var)) {
            add = false;
          }
        }
        if (add) {
        #if DEBUG_GLOBAL
          errs() << "==============================" << "\n";
          errs() << "Adding Lower-Bound Check to GEN set:\n";
          chk->print();
        #endif
          gCheck = new GlobalCheck(chk, var, NULL, false, loc);
          globalChecks.push_back(gCheck);
        }
      }
    }
    // Insert upper bounds check if downward exposed, and index >= variable it references
    if (chk->hasUpperBoundsCheck()){
      if (chk->comparisonKnown && chk->comparedToVar >= 0) {
        bool add = true;
        ConstraintGraph::CompareEnum varChange = cg->identifyMemoryChange(var);
        // Check if there is a store instruction after the check
        if (blkHasStore && (lastStoreLoc[var] > loc)) {
          // If index variable becomes bigger across basic block, can't add upper bounds check
          if (varChange == ConstraintGraph::GREATER_THAN || varChange == ConstraintGraph::UNKNOWN) {
            add = false;
          }
        }
        for (std::vector<GlobalCheck*>::iterator gi = globalChecks.begin(), ge = globalChecks.end();
              gi != ge; gi++) {
          GlobalCheck *c = *gi;
          if (c->isUpper && (c->var == var)) {
            add = false;
          }
        }
        if (add) {
        #if DEBUG_GLOBAL
          errs() << "==============================" << "\n";
          errs() << "Adding Exposed Upper-Bound Check to GEN set:\n";
          chk->print();
        #endif
          gCheck = new GlobalCheck(chk, var, chk->getUpperBound(), true, loc);
          globalChecks.push_back(gCheck);
        }
      }
    }
  }
}

void BlockFlow::identifyInSet()
{
#if DEBUG_GLOBAL
  errs() << "Generating In-Set: " << blk->getName() << "\n";
#endif
  inSet.checks.clear();
  std::vector<GlobalCheck*> inChecks;
  bool foundChecks = false;
  for (pred_iterator PI = pred_begin(blk), E = pred_end(blk); PI != E; ++PI) {
    BasicBlock *pred = *PI;
    BlockFlow *pred_flow = (*flows)[pred];
    if (!pred_flow->outSet.allChecks) {
      #if DEBUG_GLOBAL
        errs() << "Adding checks from predecessor: " << pred->getName() << "\n";
      #endif
      foundChecks = true;
      for (std::vector<GlobalCheck*>::iterator i = pred_flow->outSet.checks.begin(), e = pred_flow->outSet.checks.end();
              i != e; i++) {
        inChecks.push_back(*i);
      }
      break;
    }
  }

  if (inChecks.empty() && foundChecks) {
    inSet.allChecks = false;
    return;
  } else if (inChecks.empty()) {
    inSet.allChecks = true;
    return;
  }

  inSet.allChecks = false;
  for (std::vector<GlobalCheck*>::iterator i = inChecks.begin(), e = inChecks.end(); i != e; i++) {
    GlobalCheck* chk = *i;
    bool existsInAllPreds = true;
    for (pred_iterator PI = pred_begin(blk), E = pred_end(blk); PI != E; ++PI) {
      BasicBlock *pred = *PI;
      BlockFlow *pred_flow = (*flows)[pred];
      bool found = false;
      if (!pred_flow->outSet.allChecks) {
        for (std::vector<GlobalCheck*>::iterator pred_i = pred_flow->outSet.checks.begin(), pred_e = pred_flow->outSet.checks.end();
              pred_i != pred_e; pred_i++) {
          GlobalCheck *predCheck = *pred_i;
          ConstantInt *const1 = dyn_cast<ConstantInt>(chk->var);
          ConstantInt *const2 = dyn_cast<ConstantInt>(predCheck->var);
          if (chk->isUpper && predCheck->isUpper) {
            // Both upper bounds checks
            if (const1 != NULL && const2 != NULL) {
              // If both constants, replace with the less strict version
              if (const1->getSExtValue() > const2->getSExtValue()) {
                chk = predCheck;
              }
              found = true;
              break;
            } else if (const1 == NULL && const2 == NULL) {
              if (chk->var == predCheck->var) {
                ConstantInt *bound1 = dyn_cast<ConstantInt>(chk->bound);
                ConstantInt *bound2 = dyn_cast<ConstantInt>(predCheck->bound);
                if (bound1 != NULL && bound2 != NULL) {
                  if (bound1->getZExtValue() < bound2->getZExtValue()) {
                    // Replace with larger upper bound
                    chk = predCheck;
                  }
                  found = true;
                  break;
                } else if (bound1 == NULL && bound2 == NULL) {
                  if (bound1 == bound2) {
                    found = true;
                    break;
                  }
                }
              }
            }
          } else if (!chk->isUpper && !predCheck->isUpper) {
            // Both lower bounds checks
            if (const1 != NULL && const2 != NULL) {
              // If both constants, replace with the less strict version
              if (const1->getSExtValue() < const2->getSExtValue()) {
                chk = predCheck;
              }
              found = true;
              break;
            } else if (const1 == NULL && const2 == NULL) {
              if (chk->var == predCheck->var) {
                found = true;
                break;
              }
            }
          }
        }
      } else {
        found = true;
      }
      if (!found) {
        existsInAllPreds = false;
        break;
      }
    }
    // If we found the check in all predecessors
    if (existsInAllPreds) {
      inSet.addCheck(chk);
    }
  }
}

bool BlockFlow::identifyOutSet()
{
  bool MadeChange = false;
  std::vector<GlobalCheck*> outChecks;
  if (!isEntry && !killAll) {
    identifyInSet();
  #if DEBUG_GLOBAL
    errs() << "Generating Out-Set: " << blk->getName() << "\n";
  #endif
    // Identify checks from inSet that should be killed
    for (std::vector<GlobalCheck*>::iterator i = inSet.checks.begin(), e = inSet.checks.end();
            i != e; i++) {
      // For each global check, see if it survives past block, or if we can tell how variable changes
      GlobalCheck *chk = *i;
      std::set<Value*>::iterator it = storeSet.find(chk->var);
      if (it == storeSet.end()) {
        outChecks.push_back(chk);
      } else {
        ConstraintGraph::CompareEnum change = cg->identifyMemoryChange(chk->var);
        if (chk->isUpper) {
          // Upper-bound check
          switch (change) {
            case ConstraintGraph::EQUALS:
            case ConstraintGraph::LESS_THAN:
              outChecks.push_back(chk);
              break;
            default:
              break;
          }
        } else {
          // Lower-bound check
          switch (change) {
            case ConstraintGraph::EQUALS:
            case ConstraintGraph::GREATER_THAN:
              outChecks.push_back(chk);
              break;
            default:
              break;
          }
        }
      }
    }
  }
#if DEBUG_GLOBAL
  else {
    errs() << "Generating Out-Set: " << blk->getName() << "\n";
  }
#endif
  // Just identify checks that are live at the end of set
  for (std::vector<GlobalCheck*>::iterator i = globalChecks.begin(), e = globalChecks.end();
              i != e; i++) {
    GlobalCheck *chk = *i;
    bool add = true;
    for (unsigned int o = 0; o < outChecks.size(); o++) {
      GlobalCheck *oCheck = outChecks.at(o);
      ConstantInt *const1 = dyn_cast<ConstantInt>(chk->var);
      ConstantInt *const2 = dyn_cast<ConstantInt>(oCheck->var);
      if (chk->isUpper && oCheck->isUpper) {
        // Both upper bounds checks
        if (const1 != NULL && const2 != NULL) {
          // If both constants, replace with the more strict version
          if (const1->getSExtValue() > const2->getSExtValue()) {
            outChecks.at(o) = chk;
          }
          add = false;
          break;
        } else if (const1 == NULL && const2 == NULL) {
          if (chk->var == oCheck->var) {
            ConstantInt *bound1 = dyn_cast<ConstantInt>(chk->bound);
            ConstantInt *bound2 = dyn_cast<ConstantInt>(oCheck->bound);
            if (bound1 != NULL && bound2 != NULL) {
              if (bound1->getZExtValue() < bound2->getZExtValue()) {
                outChecks.at(o) = chk;
              }
              add = false;
              break;
            } else if (bound1 == NULL && bound2 == NULL) {
              if (bound1 == bound2) {
                add = false;
                break;
              }
            }
          }
        }
      } else if (!chk->isUpper && !oCheck->isUpper) {
        // Both lower bounds checks
        if (const1 != NULL && const2 != NULL) {
          // If both constants, replace with the more strict version
          if (const1->getSExtValue() < const2->getSExtValue()) {
            outChecks.at(o) =  chk;
          }
          add = false;
          break;
        } else if (const1 == NULL && const2 == NULL) {
          if (chk->var == oCheck->var) {
            add = false;
            break;
          }
        }
      }
    }
    if (add) {
      outChecks.push_back(chk);
    }
  }

  if (isEntry && outChecks.empty()) {
    outSet.allChecks = false;
    return true;
  }

  if (outChecks.empty()) {
    if (inSet.allChecks) {
      bool oldState = outSet.allChecks;
      outSet.checks.clear();
      outSet.allChecks = true;
      return oldState != true;
    } else {
      outSet.allChecks = false;
      if (outSet.checks.empty()) {
        return false;
      } else {
        outSet.checks.clear();
        return true;
      }
    }
  }

  for (std::vector<GlobalCheck*>::iterator i = outChecks.begin(), e = outChecks.end(); i != e; i++) {
  #if DEBUG_GLOBAL
    errs() << "Adding Check to Out Set:\n";
    (*i)->print();
  #endif
    MadeChange |= outSet.addAvailableCheck(*i);
  }
  return MadeChange;
}

void BlockFlow::eliminateRedundantChecks()
{
  for (std::vector<BoundsCheck*>::iterator it = checks->begin(), et = checks->end(); it != et; it++) {
    BoundsCheck *boundsCheck = *it;
    for (std::vector<GlobalCheck*>::iterator i = inSet.checks.begin(), e = inSet.checks.end(); i != e; i++) {
      GlobalCheck *gCheck = *i;
      ConstantInt *const1 = dyn_cast<ConstantInt>(boundsCheck->getIndex());
      ConstantInt *const2 = dyn_cast<ConstantInt>(gCheck->var);
      if (boundsCheck->hasUpperBoundsCheck() && gCheck->isUpper) {
        // Both upper bounds checks
        if (const1 != NULL && const2 != NULL) {
          // If both constants, replace with the more strict version
          if (const1->getSExtValue() <= const2->getSExtValue()) {
            boundsCheck->deleteUpperBoundsCheck();
          }
        } else if (const1 == NULL && const2 == NULL) {
          if ((boundsCheck->getVariable() == gCheck->var) && (boundsCheck->comparisonKnown && (boundsCheck->comparedToVar <= 0))) {
            ConstantInt *bound1 = dyn_cast<ConstantInt>(boundsCheck->getUpperBound());
            ConstantInt *bound2 = dyn_cast<ConstantInt>(gCheck->bound);
            if (bound1 != NULL && bound2 != NULL) {
              if (bound1->getZExtValue() >= bound2->getZExtValue()) {
                boundsCheck->deleteUpperBoundsCheck();
              }
            } else if (bound1 == NULL && bound2 == NULL) {
              if (bound1 == bound2) {
                if (boundsCheck->comparisonKnown && boundsCheck->comparedToVar <= 0) {
                  boundsCheck->deleteUpperBoundsCheck();
                }
              }
            }
          }
        }
      } else if (boundsCheck->hasLowerBoundsCheck() && !gCheck->isUpper) {
        // Both lower bounds checks
        if (const1 != NULL && const2 != NULL) {
          // If both constants, replace with the more strict version
          if (const1->getSExtValue() >= const2->getSExtValue()) {
            boundsCheck->deleteLowerBoundsCheck();
          }
        } else if (const1 == NULL && const2 == NULL) {
          if ((boundsCheck->getVariable() == gCheck->var) && (boundsCheck->comparisonKnown && (boundsCheck->comparedToVar >= 0))) {
            boundsCheck->deleteLowerBoundsCheck();
          }
        }
      }
    }
  }
}

void BlockFlow::print()
{
  errs() << "==============================" << "\n";
  errs() << "Basic Block: " << blk->getName() << "\n";
  errs() << "In Set: ";
  if (inSet.checks.empty()) {
    errs() << "EMPTY" << "\n";
  } else {
    for (std::vector<GlobalCheck*>::iterator i = inSet.checks.begin(), e = inSet.checks.end(); i != e; i++) {
      GlobalCheck *chk = *i;
      if (chk->isUpper) {
        errs() << "(" << chk->var->getName() << "< " << *(chk->bound) << ") ";
      } else {
        errs() << "(0 < " << chk->var->getName() << ") ";
      }
    }
    errs() << "\n";
  }
  errs() << "Out Set: ";
  if (outSet.checks.empty()) {
    if (outSet.allChecks) {
      errs() << "ALL CHECKS" << "\n";
    } else {
      errs() << "EMPTY" << "\n";
    }
  } else {
    for (std::vector<GlobalCheck*>::iterator i = outSet.checks.begin(), e = outSet.checks.end(); i != e; i++) {
      GlobalCheck *chk = *i;
      if (chk->isUpper) {
        errs() << "(" << chk->var->getName() << " < " << *(chk->bound) << ") ";
      } else {
        errs() << "(0 < " << chk->var->getName() << ") ";
      }
    }
    errs() << "\n";
  }
}

