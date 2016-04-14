
class VarFlow {
  public:
    enum Change {
      UNKNOWN,
      DECREASING,
      EQUALS,
      INCREASING
    };
    VarFlow(BasicBlock *b, ConstraintGraph *graph, std::set<Value*> *checkVars, std::map<BasicBlock*, VarFlow*> *flows, bool isP, bool isE);
    std::map<Value*, VarFlow::Change> inSet;
    std::map<Value*, VarFlow::Change> outSet;
    bool identifyOutSet();

    void printOutSet();
    void printInSet();
    void printChangesSet();
    void copyInSetTo(std::map<Value*, VarFlow::Change> *newSet);
  private:
    BasicBlock *blk;
    ConstraintGraph *cg;
    std::set<Value*> *vars;
    bool isPreheader;
    bool isExit;
    std::map<BasicBlock*, VarFlow*> *var_flows;
    std::map<Value*, VarFlow::Change> blkChanges;
};

VarFlow::VarFlow(BasicBlock *b, ConstraintGraph *graph, std::set<Value*> *checkVars, std::map<BasicBlock*, VarFlow*> *flows, bool isP, bool isE)
{
  blk = b;
  cg = graph;
  vars = checkVars;
  isPreheader = isP;
  isExit = isE;
  var_flows = flows;
  for (std::set<Value*>::iterator i = vars->begin(), e = vars->end(); i != e; i++) {
    Value *var = *i;
    inSet[var] = VarFlow::EQUALS;
    outSet[var] = VarFlow::EQUALS;
  }
  if (isPreheader) {
    for (std::set<Value*>::iterator i = vars->begin(), e = vars->end(); i != e; i++) {
      Value *var = *i;
      blkChanges[var] = VarFlow::EQUALS;
    }
  } else if (!isExit) {
    for (std::set<Value*>::iterator i = vars->begin(), e = vars->end(); i != e; i++) {
      Value *var = *i;
      ConstraintGraph::CompareEnum change = cg->identifyMemoryChange(var);
      switch (change) {
        case ConstraintGraph::UNKNOWN:
          blkChanges[var] = VarFlow::UNKNOWN;
          break;
        case ConstraintGraph::LESS_THAN:
          blkChanges[var] = VarFlow::DECREASING;
          break;
        case ConstraintGraph::EQUALS:
          blkChanges[var] = VarFlow::EQUALS;
          break;
        case ConstraintGraph::GREATER_THAN:
          blkChanges[var] = VarFlow::INCREASING;
          break;
        default:
          errs() << "Should not reach this VarFlow switch statement";
          break;
      }
    }
  #if DEBUG_LOOP
    printChangesSet();
  #endif
  }
}

bool VarFlow::identifyOutSet() 
{
  // If it is the preheader, there are never any changes to the set
  if (isPreheader)
    return false;

  for (pred_iterator PI = pred_begin(blk), E = pred_end(blk); PI != E; ++PI) {
    BasicBlock *pred_blk = *PI;
    // Insert check to see pred_blk is in flows map
    std::map<BasicBlock*, VarFlow*>::iterator it = var_flows->find(pred_blk);
    // Check to see if the store location exists already in memory map
    if (it == var_flows->end()) {
      errs() << "Could not find VarFlow object for Basic Block: " << pred_blk->getName() << "\n";
      return false;
    }
    
    VarFlow *pred_flow = (*var_flows)[pred_blk];
    for (std::set<Value*>::iterator i = vars->begin(), e = vars->end(); i != e; i++) {
      Value *var = *i;
      VarFlow::Change predChange = pred_flow->outSet[var];
      switch (inSet[var]) {
        case VarFlow::EQUALS:
          inSet[var] = predChange;
          break;
        case VarFlow::DECREASING:
          if (predChange == VarFlow::INCREASING || predChange == VarFlow::UNKNOWN) {
            inSet[var] = VarFlow::UNKNOWN;
          }
          break;
        case VarFlow::INCREASING:
          if (predChange == VarFlow::DECREASING || predChange == VarFlow::UNKNOWN) {
            inSet[var] = VarFlow::UNKNOWN;
          }
          break;
        case VarFlow::UNKNOWN:
          inSet[var] = VarFlow::UNKNOWN;
          break;
        default:
          errs() << "Should not reach this VarFlow switch statement";
          break;
      }
    }
  }
  // Only compute in set for exit
  if (isExit) {
    return false;
  }

  std::map<Value*, VarFlow::Change> nextOutSet;
  // Compute next out set by taking in set, and applying the blkChanges set to it
  for (std::set<Value*>::iterator i = vars->begin(), e = vars->end(); i != e; i++) {
    Value *var = *i;
    VarFlow::Change inChange = inSet[var];
    switch (blkChanges[var]) {
      case VarFlow::EQUALS:
        nextOutSet[var] = inChange;
        break;
      case VarFlow::DECREASING:
        if (inChange == VarFlow::INCREASING || inChange == VarFlow::UNKNOWN) {
          nextOutSet[var] = VarFlow::UNKNOWN;
        } else {
          nextOutSet[var] = VarFlow::DECREASING;
        }
        break;
      case VarFlow::INCREASING:
        if (inChange == VarFlow::DECREASING || inChange == VarFlow::UNKNOWN) {
          nextOutSet[var] = VarFlow::UNKNOWN;
        } else {
          nextOutSet[var] = VarFlow::INCREASING;
        }
        break;
      case VarFlow::UNKNOWN:
        nextOutSet[var] = VarFlow::UNKNOWN;
        break;
      default:
        errs() << "Should not reach this VarFlow switch statement";
        break;
    }
  }
 
  bool MadeChange = false;
  // Loop through and set out set to next out set and check for any changes
  for (std::set<Value*>::iterator i = vars->begin(), e = vars->end(); i != e; i++) {
    Value *var = *i;
    if (outSet[var] != nextOutSet[var]) {
      outSet[var] = nextOutSet[var];
      MadeChange = true;
    }
  }
  return MadeChange;
}

void VarFlow::printInSet() {
  for (std::set<Value*>::iterator i = vars->begin(), e = vars->end(); i != e; i++) {
    Value *var = *i;
    errs() << "--" << *var << ": ";
    switch (inSet[var]) {
      case VarFlow::EQUALS:
        errs() << "EQUALS\n";
        break;
      case VarFlow::DECREASING:
        errs() << "DECREASING\n";
        break;
      case VarFlow::INCREASING:
        errs() << "INCREASING\n";
        break;
      case VarFlow::UNKNOWN:
        errs() << "UNKNOWN\n";
    }
  }
}

void VarFlow::printChangesSet() {
  for (std::set<Value*>::iterator i = vars->begin(), e = vars->end(); i != e; i++) {
    Value *var = *i;
    errs() << "--" << *var << ": ";
    switch (blkChanges[var]) {
      case VarFlow::EQUALS:
        errs() << "EQUALS\n";
        break;
      case VarFlow::DECREASING:
        errs() << "DECREASING\n";
        break;
      case VarFlow::INCREASING:
        errs() << "INCREASING\n";
        break;
      case VarFlow::UNKNOWN:
        errs() << "UNKNOWN\n";
    }
  }
}

void VarFlow::printOutSet() {
  for (std::set<Value*>::iterator i = vars->begin(), e = vars->end(); i != e; i++) {
    Value *var = *i;
    errs() << "--" << *var << ": ";
    switch (outSet[var]) {
      case VarFlow::EQUALS:
        errs() << "EQUALS\n";
        break;
      case VarFlow::DECREASING:
        errs() << "DECREASING\n";
        break;
      case VarFlow::INCREASING:
        errs() << "INCREASING\n";
        break;
      case VarFlow::UNKNOWN:
        errs() << "UNKNOWN\n";
    }
  }
}
    
void VarFlow::copyInSetTo(std::map<Value*, VarFlow::Change> *newSet)
{
  for (std::set<Value*>::iterator i = vars->begin(), e = vars->end(); i != e; i++) {
    Value *var = *i;
    (*newSet)[var] = inSet[var];
  }
}
