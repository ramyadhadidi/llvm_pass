// Constraint Node
class ConstraintNode {
  public:
    enum EdgeType
    {
      UNKNOWN,
      LOAD,
      STORE,
      ADD,
      SUB,
      MUL,
      DIV,
      EQUALS
    };
    ConstraintNode* pred;
    int64_t pred_weight;
    EdgeType pred_edge_type;
    std::vector<ConstraintNode*> successors;
    std::vector<int64_t> weights;
    std::vector<EdgeType> types;
    
    
    ConstraintNode(Value *val, int id_num);
    ConstraintNode(Value *val, int id_num, Instruction *i);
    ~ConstraintNode();

    void setPredecessor(ConstraintNode* node, int64_t weight, EdgeType type);
    void addEdgeTo(ConstraintNode* node, int64_t weight, EdgeType type);
    void setConstantValue(int64_t val);
    int64_t getConstantValue();
    bool hasConstantValue();
    bool equals(Value *val, int id_num);
    void print();
    Value* getValue();
    int getID();
    Instruction* getInstruction();
    bool canMove;
  private:
    Value *llvmVal;
    Instruction *inst;
    int id;
    int64_t const_val;
    bool has_const_val;
};


ConstraintNode::ConstraintNode(Value *val, int id_num) 
{
  llvmVal = val;
  id = id_num;
  pred = NULL;
  pred_edge_type = ConstraintNode::UNKNOWN;
  has_const_val = false;
  canMove = true;
  inst = dyn_cast<Instruction>(val);
}

ConstraintNode::ConstraintNode(Value *val, int id_num, Instruction *i) 
{
  llvmVal = val;
  id = id_num;
  pred = NULL;
  pred_edge_type = ConstraintNode::UNKNOWN;
  has_const_val = false;
  canMove = true;
  inst = i;
}

ConstraintNode::~ConstraintNode() 
{
}

Instruction* ConstraintNode::getInstruction() {
  return inst;
}

bool ConstraintNode::equals(Value *val, int id_num)
{
  return (llvmVal == val) && (id == id_num);
}

void ConstraintNode::print() 
{
  errs() << "==================================\n";
  errs() << "Value: " << *llvmVal << "\n";
  if (inst && (Value*)inst != llvmVal) {
    errs() << "Instruction: " << *inst << "\n";
  }
  errs() << "id: " << id << "\n";
  errs() << "Can Move: " << (canMove ? "Yes" : "No") << "\n";
  if (has_const_val) {
    errs() << "Constant Value: " << const_val << "\n";
  }
  if (pred != NULL) {
    errs() << "Predecessor: [" << pred->getID() << "]" << *(pred->getValue()) << ": "; 
    if (pred_weight > 0) {
      errs() << "GREATER_THAN\n";
    } else if (pred_weight < 0) {
      errs() << "LESS_THAN\n";
    } else {
      errs() << "EQUALS\n";
    }
  } else {
    errs() << "Predecessor: NONE\n"; 
  }
  errs() << "Successors:";
  for (unsigned int i = 0; i < successors.size(); i++) {
    ConstraintNode *node = successors.at(i);
    Value *nodeVal = node->getValue();
    errs() << " ([" << node->getID() << "]" <<  *nodeVal << ": ";
    int weight = weights.at(i);
    if (weight > 0) {
      errs() << "GREATER_THAN)";
    } else if (weight < 0) {
      errs() << "LESS_THAN)";
    } else {
      errs() << "EQUALS)";
    }
  }
  errs() << "\n";
  errs() << "==================================\n";
}

Value* ConstraintNode::getValue() 
{
  return llvmVal;
}

int ConstraintNode::getID() 
{
  return id;
}

bool ConstraintNode::hasConstantValue() 
{
  return has_const_val;
}

int64_t ConstraintNode::getConstantValue() 
{
  return const_val;
}

void ConstraintNode::setPredecessor(ConstraintNode* node, int64_t weight, EdgeType type) 
{
  pred = node;
  pred_weight = weight;
  pred_edge_type = type;
}

void ConstraintNode::addEdgeTo(ConstraintNode* node, int64_t weight, EdgeType type) 
{
  successors.push_back(node);
  weights.push_back(weight);
  types.push_back(type);
}

void ConstraintNode::setConstantValue(int64_t val) 
{
  has_const_val = true;
  const_val = val;
}

// Constraint Graph
class ConstraintGraph {
  public:
    enum CompareEnum
    {
      UNKNOWN = 0,
      LESS_THAN = 1,
      EQUALS = 2,
      GREATER_THAN = 3
    };
    ConstraintGraph();
    ~ConstraintGraph();

    CompareEnum compare(Value *val1, Value *val2);
    bool findDependencyPath(Value *val1, std::vector<Instruction*> *dependentInsts);
    bool findDependencyPath(Value *val1, Value *val2, std::vector<Instruction*> *dependentInsts);
    ConstraintNode* getNode(Value *val, int id);
    void addStoreEdge(Value *from, Value *to, Instruction *store);
    void addLoadEdge(Value *from, Value *to);
    void addCastEdge(Value *from, Value *to);
    void addGEPEdge(Value *index, Value *to);
    void addConstantNode(Value *val, int64_t const_val);
    ConstraintNode* addNode(Value *val);
    void addMemoryNode(Value *val); 
    void addAddEdge(Value *from, Value *to, int64_t weight);
    void addSubEdge(Value *from, Value *to, int64_t weight);
    void addMulEdge(Value *from, Value *to, int64_t weight);
    void addDivEdge(Value *from, Value *to, int64_t weight);
    void killMemoryLocations();
    void print();
    Value* findFirstLoad(Value *val1, int64_t *weight, bool *comparisonKnown);
    ConstraintGraph::CompareEnum identifyMemoryChange(Value *val);
  private:
    CompareEnum compareStrict(ConstraintNode *node1, ConstraintNode *node2);
    CompareEnum comparePropogate(ConstraintNode *node1, ConstraintNode *node2);

    int64_t treeSearch(ConstraintNode *root, ConstraintNode *target, int weight, bool *found, std::vector<ConstraintNode*> *visited);
    int64_t treeSearchPropogate(ConstraintNode *root, ConstraintNode *target, int weight, bool *found, std::vector<ConstraintNode*> *visited);
    
    bool findStrictPath(ConstraintNode *node1, ConstraintNode *node2, std::vector<Instruction*> *dependentInsts);
    bool findPropogatePath(ConstraintNode *node1, ConstraintNode *node2, std::vector<Instruction*> *dependentInsts);

    std::vector<ConstraintNode*> nodes;
    std::map<Value*, int> memoryNodes;
};

ConstraintGraph::ConstraintGraph() 
{
}

ConstraintGraph::~ConstraintGraph() 
{
  // Iterate and delete create constraint nodes
  for (std::vector<ConstraintNode*>::iterator i = nodes.begin(),  e = nodes.end(); i != e; i++) {
    delete *i;  
  }
  nodes.clear();
  memoryNodes.clear();
}

bool ConstraintGraph::findDependencyPath(Value *val1, std::vector<Instruction*> *dependentInsts)
{
  if (isa<ConstantInt>(val1)) {
    return true;
  }

  ConstraintNode *node1 = getNode(val1, 0);

  if (node1 == NULL) {
    errs() << "Node for value  " << *val1 << " does not exist.\n"; 
    return false;
  }
  dependentInsts->clear();
  ConstraintNode *root = node1;
  while (root != NULL) {
    if (!root->canMove) {
      dependentInsts->clear();
      return false;
    }
    dependentInsts->push_back(root->getInstruction());
    LoadInst* LI = dyn_cast<LoadInst>(root->getValue());
    if (LI != NULL) {
      return true;
    }

    if (root->pred == NULL) {
    #if DEBUG_LOOP
      errs() << "Could not find dependent load. Returning highest root: " << *(root->getValue()) << "\n";
    #endif
      return true;
    }
    root = root->pred;
  }
  return false;
}

Value* ConstraintGraph::findFirstLoad(Value *val1, int64_t *weight, bool *comparisonKnown) 
{
  if (isa<ConstantInt>(val1)) {
    return val1;
  }

  ConstraintNode *node1 = getNode(val1, 0);

  if (node1 == NULL) {
    errs() << "Node for value  " << *val1 << " does not exist.\n"; 
    return NULL;
  }

  ConstraintNode *root = node1;
  int64_t w = 0;
  bool known = true;
  while (root != NULL) {
    LoadInst* LI = dyn_cast<LoadInst>(root->getValue());
    if (LI != NULL) {
      *comparisonKnown = known;
      *weight = w;
      return LI->getPointerOperand();
    }

    if (root->pred == NULL) {
    #if DEBUG_LOCAL
      errs() << "Could not find dependent load. Returning highest root: " << *(root->getValue()) << "\n";
    #endif
      *comparisonKnown = known;
      *weight = w;
      return root->getValue();
    }
    int pred_weight = root->pred_weight;
    if ((w > 0) && (pred_weight >= 0)) {
      // keep weight at same value
    } else if ((w < 0) && (pred_weight <= 0)) {
      // keep weight at same value
    } else if (w == 0) {
      // Change weight to pred_weight value regardless as it may be more strict
      w = pred_weight;
    }else {
      known = false;
    }    
    root = root->pred;
  }
  return NULL;
}

bool ConstraintGraph::findDependencyPath(Value *val1, Value *val2, std::vector<Instruction*> *dependentInsts)
{
  dependentInsts->clear();
  ConstraintNode* node1 = getNode(val1, 0);
  ConstraintNode* node2 = getNode(val2, 0);
  
  ConstantInt *val_const1 = dyn_cast<ConstantInt>(val1);
  ConstantInt *val_const2 = dyn_cast<ConstantInt>(val1);

  if (val_const2 != NULL) {
    // If second value is constant, no dependent instructions
    return false;
  }
  
  if (node2 == NULL) {
    errs() << "Path does not exist for: " << *val2 << "\n";
    return false;
  }
  if (val_const1 != NULL) {
    // If first value constant, add all predecessors to tree 
    ConstraintNode *root = node2;
    while (root != NULL) {
      if (!root->canMove) {
      #if DEBUG_LOCAL
        errs() << "Could not move due to instruction: " << *(root->getValue()) << "\n";
      #endif
        dependentInsts->clear();
        return false;
      }
      Instruction *inst = root->getInstruction();
      if (inst != NULL) {
        dependentInsts->push_back(inst);
      } else {
        errs() << "Identified non-instruction node: " << *(root->getValue()) << "\n";
        dependentInsts->clear();
        return false;
      }
      root = root->pred;
    }
    return false;
  }

  if (node1 == NULL) {
    errs() << "Destination " << *val1 << "does not exist\n";
    return false;
  }

  if (node1 == node2) {
    errs() << "Comparing two of the same nodes\n";
    return false;
  }

  bool val = findStrictPath(node1, node2, dependentInsts);
  if (!val) {
    dependentInsts->clear();
    val = findPropogatePath(node1, node2, dependentInsts);
  }

  if (!val) {
    dependentInsts->clear();
    errs() << "Error: Path was not identified for " << *val2 << "\n";
    return false;
  }
  return val;
}

bool ConstraintGraph::findStrictPath(ConstraintNode *node1, ConstraintNode *node2, std::vector<Instruction*> *dependentInsts) 
{
  ConstraintNode *root = node2;
  int64_t weight = 0;
  bool found = false;
  std::vector<ConstraintNode*> visited;
  while (root != node1) {
    if (root == NULL) {
      dependentInsts->clear();
      return false;
    }
    // Do a search from the current root
    treeSearch(root, node1, weight, &found, &visited);
    if (found) {
      return true;
    }
    
    if (!root->canMove) {
    #if DEBUG_LOCAL
      errs() << "Could not move due to instruction: " << *(root->getValue()) << "\n";
    #endif
      dependentInsts->clear();
      return false;
    }
    Instruction *inst = root->getInstruction();
    if (inst != NULL) {
      dependentInsts->push_back(inst);
    } else {
      errs() << "Identified non-instruction node: " << *(root->getValue()) << "\n";
      dependentInsts->clear();
      return false;
    }
    visited.push_back(root);
    int pred_weight = root->pred_weight;
    if ((weight > 0) && (pred_weight >= 0)) {
      // keep weight at same value
    } else if ((weight < 0) && (pred_weight <= 0)) {
      // keep weight at same value
    } else if (weight == 0) {
      // Change weight to pred_weight value regardless as it may be more strict
      weight = pred_weight;
    }else {
      dependentInsts->clear();
      return false;
    }
    if (root->pred == node1) {
      return true;
    }
    root = root->pred;
  }
  dependentInsts->clear();
  return false;
}


bool ConstraintGraph::findPropogatePath(ConstraintNode *node1, ConstraintNode *node2, std::vector<Instruction*> *dependentInsts) 
{
  ConstraintNode *root = node2;
  int64_t weight = 0;
  bool found = false;
  std::vector<ConstraintNode*> visited;
  while (root != node1) {
    if (root == NULL) {
      dependentInsts->clear();
      return false;
    }
    // Do a search from the current root
    treeSearchPropogate(root, node1, weight, &found, &visited);
    if (found) {
      return true;
    }
    
    if (!root->canMove) {
    #if DEBUG_LOCAL
      errs() << "Could not move due to instruction: " << *(root->getValue()) << "\n";
    #endif
      dependentInsts->clear();
      return false;
    }
    Instruction *inst = root->getInstruction();
    if (inst != NULL) {
      dependentInsts->push_back(inst);
    } else {
      errs() << "Identified non-instruction node: " << *(root->getValue()) << "\n";
      dependentInsts->clear();
      return false;
    }
    visited.push_back(root);
    if (root->pred_edge_type == ConstraintNode::UNKNOWN || root->pred_edge_type == ConstraintNode::MUL || root->pred_edge_type == ConstraintNode::DIV) {
      return ConstraintGraph::UNKNOWN;
    } 
    weight = weight + root->pred_weight;
    if (root->pred == node1) {
      return true;
    }
    root = root->pred;
  }
  
  dependentInsts->clear();
  return false;

}

ConstraintGraph::CompareEnum ConstraintGraph::compare(Value *val1, Value *val2) 
{
#if DEBUG_LOCAL
  errs() << "Comparing " << *val1 << " to " << *val2 << "\n";
#endif
  ConstraintNode* node1 = getNode(val1, 0);
  ConstraintNode* node2 = getNode(val2, 0);

  ConstantInt *val_const1 = dyn_cast<ConstantInt>(val1);
  ConstantInt *val_const2 = dyn_cast<ConstantInt>(val2);
  bool identifiedConstants = false;
  int64_t v1 = 0;
  int64_t v2 = 0;
  if (val_const1 != NULL) {
    v1 = val_const1->getSExtValue();
    if (val_const2 != NULL) {
      v2 = val_const1->getSExtValue();
      identifiedConstants = true;
    } else {
      if (node2 == NULL) {
        errs() << "Comparions Value 2 was not identified: " << *val2 << "\n";
        return ConstraintGraph::UNKNOWN;
      } else if (node2->hasConstantValue()) {
        v2 = node2->getConstantValue();
        identifiedConstants = true;
      }
    }
  } else if (val_const2 != NULL) {
    v2 = val_const2->getSExtValue();
    if (node1 == NULL) {
      errs() << "Comparison Value 1 was not identified: " << *val1 << "\n";
      return ConstraintGraph::UNKNOWN;
    } else if (node1->hasConstantValue()) {
      v1 = node1->getConstantValue(); 
      identifiedConstants = true;
    }
  }
  if (identifiedConstants) {
    if (v1 > v2) 
      return ConstraintGraph::GREATER_THAN;
    else if (v1 < v2)
      return ConstraintGraph::LESS_THAN;
    else
      return ConstraintGraph::EQUALS;
  }


  if (node1 == NULL) {
    errs() << "Comparison Value 1 was not identified: " << *val1 << "\n";
    return ConstraintGraph::UNKNOWN;
  }
  if (node2 == NULL) {
    errs() << "Comparison Value 2 was not identified: " << *val2 << "\n";
    return ConstraintGraph::UNKNOWN;
  }

  if (node1 == node2) {
  #if DEBUG_LOCAL
    errs() << "Comparing two of the same nodes\n";
  #endif
    return ConstraintGraph::EQUALS;
  }

  if (node1->hasConstantValue() && node2->hasConstantValue()) {
    v1 = node1->getConstantValue();
    v2 = node2->getConstantValue();
    if (v1 > v2) 
      return ConstraintGraph::GREATER_THAN;
    else if (v1 < v2)
      return ConstraintGraph::LESS_THAN;
    else
      return ConstraintGraph::EQUALS;
  }
  ConstraintGraph::CompareEnum val = compareStrict(node1, node2);
  if (val == ConstraintGraph::UNKNOWN) {
    val = comparePropogate(node1, node2);
  }
  return val;
}

ConstraintGraph::CompareEnum ConstraintGraph::compareStrict(ConstraintNode *node1, ConstraintNode *node2) 
{
  ConstraintNode *root = node2;
  int64_t weight = 0;
  bool found = false;
  int64_t tmp_weight = 0;
  std::vector<ConstraintNode*> visited;
  while (root != node1) {
    if (root == NULL) {
      return ConstraintGraph::UNKNOWN;
    } 
    // Do a search from the current root
    tmp_weight = treeSearch(root, node1, weight, &found, &visited);
    if (found) {
      weight = tmp_weight;
      break;
    }
    visited.push_back(root);
    int pred_weight = root->pred_weight;
    if ((weight > 0) && (pred_weight >= 0)) {
      // keep weight at same value
    } else if ((weight < 0) && (pred_weight <= 0)) {
      // keep weight at same value
    } else if (weight == 0) {
      // Change weight to pred_weight value regardless as it may be more strict
      weight = pred_weight;
    } else {
      // Differing weights so return unknown
      return ConstraintGraph::UNKNOWN;
    }
    if (root->pred == node1) {
      found = true;
    }
    root = root->pred;
  }

  if (found) {
    if (weight > 0) 
      return ConstraintGraph::LESS_THAN;
    else if (weight < 0)
      return ConstraintGraph::GREATER_THAN;
    else
      return ConstraintGraph::EQUALS;
  }
  return ConstraintGraph::UNKNOWN;
}

int64_t ConstraintGraph::treeSearch(ConstraintNode *root, ConstraintNode *target, int weight, bool *found, std::vector<ConstraintNode*> *visited)
{
  if (root == target) {
    visited->push_back(target);
    *found = true;
    return weight;
  }
  
  std::vector<ConstraintNode*> *successors = &(root->successors);
  std::vector<int64_t> *weights = &(root->weights);
  if (successors->empty()) {
    visited->push_back(root);
    *found = false;
    return 0;
  }
  int tempWeight;
  bool tempFound = false;
  for (unsigned int i = 0; i < successors->size(); i++) {
    ConstraintNode *succ = successors->at(i);
    // Skip node if it has been visited
    if (std::find(visited->begin(), visited->end(), succ) != visited->end()) 
      continue;
    int succ_weight = weights->at(i);
    if ((weight > 0) && (succ_weight >= 0)) {
      tempWeight = treeSearch(succ, target, weight, &tempFound, visited);  
    } else if ((weight < 0) && (succ_weight <= 0)) {
      tempWeight = treeSearch(succ, target, weight, &tempFound, visited);  
    } else if (weight == 0) {
      tempWeight = treeSearch(succ, target, succ_weight, &tempFound, visited);
    } else {
      continue;
    }
    if (tempFound) {
      visited->push_back(root);
      *found = true;
      return tempWeight;
    }
  }
  visited->push_back(root);
  *found = false;
  return 0;
}

ConstraintGraph::CompareEnum ConstraintGraph::comparePropogate(ConstraintNode *node1, ConstraintNode *node2) 
{
  ConstraintNode *root = node2;
  int64_t weight = 0;
  bool found = false;
  int64_t tmp_weight = 0;
  std::vector<ConstraintNode*> visited;
  while (root != node1) {
    if (root == NULL) {
      return ConstraintGraph::UNKNOWN;
    } 
    // Do a search from the current root
    tmp_weight = treeSearchPropogate(root, node1, weight, &found, &visited);
    if (found) {
      weight = tmp_weight;
      break;
    }
    visited.push_back(root);
    if (root->pred_edge_type == ConstraintNode::UNKNOWN || root->pred_edge_type == ConstraintNode::MUL || root->pred_edge_type == ConstraintNode::DIV) {
      return ConstraintGraph::UNKNOWN;
    } 
    weight = weight + root->pred_weight;
    if (root->pred == node1) {
      found = true;
      break;
    }  
    root = root->pred;
  }

  if (found) {
    if (weight > 0) 
      return ConstraintGraph::LESS_THAN;
    else if (weight < 0)
      return ConstraintGraph::GREATER_THAN;
    else
      return ConstraintGraph::EQUALS;
  }
  return ConstraintGraph::UNKNOWN;
}

int64_t ConstraintGraph::treeSearchPropogate(ConstraintNode *root, ConstraintNode *target, int weight, bool *found, std::vector<ConstraintNode*> *visited)
{
  if (root == target) {
    visited->push_back(target);
    *found = true;
    return weight;
  }
  
  std::vector<ConstraintNode*> *successors = &(root->successors);
  std::vector<int64_t> *weights = &(root->weights);
  std::vector<ConstraintNode::EdgeType> *types = &(root->types);
  if (successors->empty()) {
    visited->push_back(root);
    *found = false;
    return 0;
  }
  int tempWeight;
  bool tempFound = false;
  for (unsigned int i = 0; i < successors->size(); i++) {
    ConstraintNode *succ = successors->at(i);
    ConstraintNode::EdgeType type = types->at(i); 
    // Skip node if it has been visited
    if (std::find(visited->begin(), visited->end(), succ) != visited->end()) 
      continue;
    if (type == ConstraintNode::UNKNOWN || type == ConstraintNode::MUL || type == ConstraintNode::DIV) {
      return ConstraintGraph::UNKNOWN;
    } 
    tempWeight = weights->at(i) + weight;
    tempWeight = treeSearchPropogate(succ, target, tempWeight, &tempFound, visited);
    if (tempFound) {
      visited->push_back(root);
      *found = true;
      return tempWeight;
    }
  }
  visited->push_back(root);
  *found = false;
  return 0;
}


ConstraintNode* ConstraintGraph::getNode(Value *val, int id) 
{
  for (std::vector<ConstraintNode*>::iterator i = nodes.begin(), e = nodes.end(); i != e; i++) {
    ConstraintNode *node = *i;
    if (node->equals(val, id)) {
      return node;
    }
  }
  return NULL;
}

void ConstraintGraph::addStoreEdge(Value *from, Value *to, Instruction *store) 
{
  std::map<Value*,int>::iterator it = memoryNodes.find(to);
  int id = 1;
  // Check to see if the store location exists already in memory map
  if (it != memoryNodes.end()) {
    // If exists, store node should be created with next id
    // Else, start id at 1
    id = memoryNodes[to];
    id = id + 1;
  }
  memoryNodes[to] = id; // Store new id in memory map
  // Create toNode and to nodes list
  ConstraintNode* toNode = new ConstraintNode(to, id, store);
  // hack: consider fixing later
  toNode->canMove = false; // can't move past a store instruction

  nodes.push_back(toNode); 
  // Check if from Node is constant
  ConstantInt *ConstVal = dyn_cast<ConstantInt>(from); 
  if (ConstVal != NULL) {
    // If from node is constant value, then store value in node
    toNode->setConstantValue(ConstVal->getSExtValue());
  } else {
    // See if we can find 
    ConstraintNode* fromNode = getNode(from, 0);
    if (fromNode == NULL) {
      // If fromNode did not exist, create new from Node and add to list
      fromNode = new ConstraintNode(from, 0);
      nodes.push_back(fromNode);
    }
    // If the from node has a constant value, then set that value in store
    if (fromNode->hasConstantValue()) {
      toNode->setConstantValue(fromNode->getConstantValue());
    }
    // Set predecessor of to node to previous location
    toNode->setPredecessor(fromNode, 0, ConstraintNode::STORE);
    // Add edge from node to store node
    fromNode->addEdgeTo(toNode, 0, ConstraintNode::STORE);
  }
}

void ConstraintGraph::addLoadEdge(Value *from, Value *to) 
{
  // Try to find loading from memory location in map
  ConstraintNode *fromNode;
  std::map<Value*,int>::iterator it = memoryNodes.find(from);
  if (it == memoryNodes.end()) {
    // If it does not exist, create dummy node
    memoryNodes[from] = 0;
    fromNode = new ConstraintNode(from, 0);
    fromNode->canMove = false;
    nodes.push_back(fromNode);
  } else {
    fromNode = getNode(from, memoryNodes[from]);
    if (fromNode == NULL) {
      errs() << "Could not find store node for: " << *from << ", ID = " << memoryNodes[to] << "\n";
    }
  }

  // Create loading to node 
  ConstraintNode* toNode = new ConstraintNode(to, 0);
  nodes.push_back(toNode);
  // Set the predecesor as the from node
  toNode->setPredecessor(fromNode, 0, ConstraintNode::LOAD);
  if (fromNode->hasConstantValue()) {
    // If the node has a constant value, then set it in the to node
    toNode->setConstantValue(fromNode->getConstantValue());
  }
  // Add a node from the store node to load location
  fromNode->addEdgeTo(toNode, 0, ConstraintNode::LOAD);
}

void ConstraintGraph::addCastEdge(Value *from, Value *to) 
{
  ConstraintNode* fromNode = getNode(from, 0);
  if (fromNode == NULL) {
    errs() << "Casting a value that did not exist: " << *from << "\n";
    fromNode = addNode(from);
  }
  ConstraintNode* toNode = getNode(to, 0);
  if (toNode != NULL) {
    errs() << "Storing to a value that already had a node created " << *to << "\n";
  }
  toNode = new ConstraintNode(to, 0);
  nodes.push_back(toNode);
  // Check if from Node is constant
  ConstantInt *ConstVal = dyn_cast<ConstantInt>(from); 
  if (ConstVal != NULL) {
    // If from node is constant value, then store value in node
    toNode->setConstantValue(ConstVal->getSExtValue());
  } else {
    // If the from node has a constant value, then set that value in store
    if (fromNode->hasConstantValue()) {
      toNode->setConstantValue(fromNode->getConstantValue());
    }
    toNode->setPredecessor(fromNode, 0, ConstraintNode::EQUALS);
    fromNode->addEdgeTo(toNode, 0, ConstraintNode::EQUALS);
  }
}

void ConstraintGraph::addGEPEdge(Value* index, Value* to) {
  ConstraintNode* toNode = getNode(to, 0);
  if (toNode != NULL) {
    errs() << "Storing to a value that already had a node created " << *to << "\n";
  }
  toNode = new ConstraintNode(to, 0);
  nodes.push_back(toNode);
  // Check if from Node is constant
  ConstantInt *ConstVal = dyn_cast<ConstantInt>(index); 
  if (ConstVal != NULL) {
    // If from node is constant value, then store value in node
    toNode->setConstantValue(ConstVal->getSExtValue());
  } else {
    ConstraintNode* indexNode = getNode(index, 0);
    if (indexNode == NULL) {
      errs() << "Referenced an index that did not exist: " << *index << "\n";
      indexNode = addNode(index);
    }
    // If the from node has a constant value, then set that value in store
    if (indexNode->hasConstantValue()) {
      toNode->setConstantValue(indexNode->getConstantValue());
    }
    toNode->setPredecessor(indexNode, 0, ConstraintNode::EQUALS);
    indexNode->addEdgeTo(toNode, 0, ConstraintNode::EQUALS);
  }
}

void ConstraintGraph::addConstantNode(Value *val, int64_t const_val) 
{
  // Create a node showing the store of the constant into the register
  ConstraintNode *node = new ConstraintNode(val, 0);
  node->setConstantValue(const_val);
  nodes.push_back(node);
}

ConstraintNode* ConstraintGraph::addNode(Value *val) 
{
  ConstraintNode *node = new ConstraintNode(val, 0);
  nodes.push_back(node);
  node->canMove = false;
  return node;
}

void ConstraintGraph::addMemoryNode(Value *val) 
{
  ConstraintNode *node = new ConstraintNode(val, 0);
  memoryNodes[val] = 0;
  nodes.push_back(node);
}

void ConstraintGraph::addAddEdge(Value *from, Value *to, int64_t weight) 
{
  ConstraintNode* fromNode = getNode(from, 0);
  if (fromNode == NULL) {
    fromNode = addNode(from);
    errs() << "Referenced an Add operand that did not exist: " << *from << "\n";
  }
  ConstraintNode* toNode = getNode(to, 0);
  if (toNode != NULL) {
    errs() << "Storing to a value that already had a node created " << *to << "\n";
  }
  toNode = new ConstraintNode(to, 0);
  nodes.push_back(toNode);
  if (fromNode->hasConstantValue()) {
    // If op node has constant value, get it and perform addition
    int64_t const_val = fromNode->getConstantValue();
    const_val += weight;
    toNode->setConstantValue(const_val);
  }
  toNode->setPredecessor(fromNode, weight, ConstraintNode::ADD);
  fromNode->addEdgeTo(toNode, -weight, ConstraintNode::ADD);  
}

void ConstraintGraph::addSubEdge(Value *from, Value *to, int64_t weight)
{
  ConstraintNode* fromNode = getNode(from, 0);
  if (fromNode == NULL) {
    fromNode = addNode(from);
    errs() << "Referenced an Sub operand that did not exist: " << *from << "\n";
  }
  ConstraintNode* toNode = getNode(to, 0);
  if (toNode != NULL) {
    errs() << "Storing to a value that already had a node created " << *to << "\n";
  }
  toNode = new ConstraintNode(to, 0);
  nodes.push_back(toNode);
  if (fromNode->hasConstantValue()) {
    // If op node has constant value, get it and perform subtraction
    int64_t const_val = fromNode->getConstantValue();
    const_val -= weight;
    toNode->setConstantValue(const_val);
  }
  toNode->setPredecessor(fromNode, -weight, ConstraintNode::SUB);
  fromNode->addEdgeTo(toNode, weight, ConstraintNode::SUB);  
}
 
void ConstraintGraph::addMulEdge(Value *from, Value *to, int64_t weight) 
{
  ConstraintNode* fromNode = getNode(from, 0);
  if (fromNode == NULL) {
    fromNode = addNode(from);
    errs() << "Referenced an Mul operand that did not exist: " << *from << "\n";
  }
  ConstraintNode* toNode = getNode(to, 0);
  if (toNode != NULL) {
    errs() << "Storing to a value that already had a node created " << *to << "\n";
  }
  toNode = new ConstraintNode(to, 0);
  nodes.push_back(toNode);
  if (fromNode->hasConstantValue()) {
    // If op node has constant value, get it and perform multiplication
    int64_t const_val = fromNode->getConstantValue();
    const_val *= weight;
    toNode->setConstantValue(const_val);
  }
  if (weight == 0) {
    weight = -1;
  }
  toNode->setPredecessor(fromNode, weight, ConstraintNode::MUL);
  fromNode->addEdgeTo(toNode, -weight, ConstraintNode::MUL);  
}
 
void ConstraintGraph::addDivEdge(Value *from, Value *to, int64_t weight) 
{
  ConstraintNode* fromNode = getNode(from, 0);
  if (fromNode == NULL) {
    fromNode = addNode(from);
    errs() << "Referenced an Div operand that did not exist: " << *from << "\n";
  }
  ConstraintNode* toNode = getNode(to, 0);
  if (toNode != NULL) {
    errs() << "Storing to a value that already had a node created " << *to << "\n";
  }
  toNode = new ConstraintNode(to, 0);
  nodes.push_back(toNode);
  if (fromNode->hasConstantValue()) {
    // If op node has constant value, get it and perform division
    int64_t const_val = fromNode->getConstantValue();
    const_val *= weight;
    toNode->setConstantValue(const_val);
  }
  toNode->setPredecessor(fromNode, -1, ConstraintNode::DIV);
  fromNode->addEdgeTo(toNode, 1, ConstraintNode::DIV);  
}

void ConstraintGraph::killMemoryLocations() 
{
  // Kill all memory locations by incrementing id to next number
  for (std::map<Value*,int>::iterator it=memoryNodes.begin(), e=memoryNodes.end(); it != e; ++it) {
    Value *val = it->first;
    int id = it->second;
    memoryNodes[val] = id + 1;
    nodes.push_back(new ConstraintNode(val, id + 1));
  }
}

void ConstraintGraph::print() 
{
  errs() << "Constraint Graph\n";
  for (std::vector<ConstraintNode*>::iterator it = nodes.begin(); it != nodes.end(); ++it) {
    ConstraintNode* node = *it;
    node->print();
  }
}

ConstraintGraph::CompareEnum ConstraintGraph::identifyMemoryChange(Value *val)
{
  std::map<Value*,int>::iterator it = memoryNodes.find(val);
  int id = 1;
  // Check to see if the store location exists already in memory map
  if (it != memoryNodes.end()) {
    // If it exists, store node should be created with next id
    // Else, start id at 1
    id = memoryNodes[val];
  } else {
    return ConstraintGraph::EQUALS;
  }

  if (id == 0) {
    return ConstraintGraph::EQUALS;
  }
  ConstraintNode *node1 = getNode(val, 0);
  if (node1 == NULL) {
    return ConstraintGraph::UNKNOWN;
  }
  ConstraintNode *node2 = getNode(val, id);
    
  ConstraintGraph::CompareEnum cmp = compareStrict(node1, node2);
  switch (cmp) {
    case ConstraintGraph::LESS_THAN:
      return ConstraintGraph::GREATER_THAN;
    case ConstraintGraph::GREATER_THAN:
      return ConstraintGraph::LESS_THAN;
    default:
      return cmp;
  }
}
