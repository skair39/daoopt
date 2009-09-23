/*
 * BranchAndBound.cpp
 *
 *  Created on: Nov 4, 2008
 *      Author: lars
 */

#include "BranchAndBound.h"

#ifdef PARALLEL_MODE
#undef DEBUG
#endif

#undef DEBUG

typedef PseudotreeNode PtNode;


void BranchAndBound::expandNext() {

//  myprint(".");

  SearchNode* node = m_stack.top();
  m_stack.pop();

  int var = node->getVar();
  // get the node's pseudo tree equivalent
  PtNode* ptnode = m_pseudotree->getNode(var);

  if (node->getType() == NODE_AND) { //===================== AND NODE =============================

    val_t val = node->getVal();

    // keep track of assignment
    m_assignment[var] = val;

#ifdef DEBUG
    {
      GETLOCK(mtx_io, lk);
      cout << *node << " (l=" << node->getLabel() << ")" << endl;// Assig. " << m_assignment << endl;
    }
#endif

    // dead end (label = 0)?
    if (node->getLabel() == ELEM_ZERO) {
      node->setLeaf(); // mark as leaf
      m_nextLeaf = node;
      return; // and abort
    }

#ifndef NO_PRUNING
    // check heuristic for pruning
    if (canBePruned(node)) {
#ifdef DEBUG
    {
      GETLOCK(mtx_io, lk);
      cout << "\t !pruning " << endl;
    }
#endif
      node->setLeaf();
      node->setPruned();
      node->setValue(ELEM_ZERO);
      m_nextLeaf = node;
      return;
    }
#endif


#ifndef NO_CACHING
    // Reset adaptive cache tables if required
    {
      const list<int>& resetList = ptnode->getCacheResetList();
      for (list<int>::const_iterator it=resetList.begin(); it!=resetList.end(); ++it)
        m_space->cache->reset(*it);
    }
#endif


    // create new OR children
    for (vector<PtNode*>::const_iterator it=ptnode->getChildren().begin();
        it!=ptnode->getChildren().end(); ++it)
    {
      int v = (*it)->getVar();

      SearchNodeOR* n = new SearchNodeOR(node, v);

      // Compute and set heuristic estimate
      heuristicOR(n);

      node->addChild(n);
      m_stack.push(n);
#ifdef DEBUG
      {
        GETLOCK(mtx_io,lk);
        cout << '\t' << *n;
        if (n->isCachable())
          cout << " - Cache: " << n->getCacheContext();
        cout << endl;
      }
#endif
    }

    if (!node->getChildren().size()) { // no children?
      m_nextLeaf = node;
      node->setLeaf(); // -> terminal node
    }

    ++m_nodesAND;

  } else { // if (node->getType() == NODE_OR) { //====================== OR NODE =======================

#ifdef DEBUG
    {
      GETLOCK(mtx_io,lk);
      cout << *node << endl;
    }
#endif

#ifndef NO_CACHING
    // Caching for this node
    if (ptnode->getFullContext().size() <= ptnode->getParent()->getFullContext().size()) {
      // add cache context information
      addCacheContext(node,ptnode->getCacheContext());
#ifdef DEBUG
      {
        GETLOCK(mtx_io, lk);
        cout << "    Context set: " << node->getCacheContext() << endl;
      }
#endif
      // try to get value from cache
      try {
        // will throw int(UNKNOWN) if not found
#ifndef NO_ASSIGNMENT
        pair<double,vector<val_t> > entry = m_space->cache->read(var, node->getCacheInst(), node->getCacheContext());
        node->setValue( entry.first ); // set value
        node->setOptAssig( entry.second ); // set assignment
#else
        double entry = m_space->cache->read(var, node->getCacheInst(), node->getCacheContext());
        node->setValue( entry ); // set value
#endif
        node->setLeaf(); // mark as leaf
        m_nextLeaf = node;
        node->clearHeurCache(); // clear precomputed heur./labels
#ifdef DEBUG
        {
          GETLOCK(mtx_io,lk);
          cout << "-Read " << *node << " with opt. solution " << node->getOptAssig() << endl;
        }
#endif
        return;
      } catch (...) { // cache lookup failed
        node->setCachable(); // mark for caching later
      }
    }
#endif


#ifndef NO_PRUNING
    // Pruning
    if (canBePruned(node)) {
#ifdef DEBUG
    {
      GETLOCK(mtx_io, lk);
      cout << "\t !pruning " << endl;
    }
#endif
      node->setLeaf();
      node->setPruned();
      node->setValue(ELEM_ZERO);
      m_nextLeaf = node;
      node->clearHeurCache();
      return;
    }
#endif


#ifdef PARALLEL_MODE
    // TODO currently outsource at fixed depth
//    if (ptnode->getDepth() == m_space->options->cutoff_depth) {
    if ( ptnode->getDepth() == m_space->options->cutoff_depth ||
        mylog10( ptnode->getSubsize() ) <= m_space->options->cutoff_size) {
//    if (ptnode->getSubwidth() == m_space->options->cutoff_depth) {

      {
        GETLOCK(mtx_io, lkio);
        cout << "** Parallelising at depth " << ptnode->getDepth() << endl;
      }

      addSubprobContext(node,ptnode->getFullContext()); // set subproblem context
      addPSTlist(node); // to allow pruning in the worker
      node->setExtern();
      node->clearHeurCache(); // clear from cache
      m_nextLeaf = node;
      return;
    }
#endif

    /////////////////////////////////////
    // actually create new AND children

    double* heur = node->getHeurCache();

    vector<SearchNode*> newNodes;
    newNodes.reserve(m_problem->getDomainSize(var));

    for (val_t i=m_problem->getDomainSize(var)-1; i>=0; --i) {

      SearchNodeAND* n = new SearchNodeAND(node, i);

      // get cached heur. value
      n->setHeur( heur[2*i] );
      // get cached label value
      n->setLabel( heur[2*i+1] );

      // add node as successor
      node->addChild(n);

      // remember new node
      newNodes.push_back(n);
    }

    // sort new nodes by increasing heuristic value
    sort(newNodes.begin(), newNodes.end(), SearchNode::heurLess);

    // add new child nodes to stack, highest heur. value will end up on top
    for (vector<SearchNode*>::iterator it = newNodes.begin(); it!=newNodes.end(); ++it) {
      m_stack.push(*it);
#ifdef DEBUG
      {
        GETLOCK(mtx_io, lk);
        cout << '\t' << *(*it) << " (l=" << (*it)->getLabel() << ")" << endl; // Assig. " << m_assignment << endl;
      }
#endif
    }

    node->clearHeurCache(); // clear heur. cache of OR node

    ++m_nodesOR;

  } //=========================================================== END =============================

}



BranchAndBound::BranchAndBound(Problem* prob, Pseudotree* pt, SearchSpace* space, Heuristic* heur) :
   Search(prob,pt,space,heur) {

  // create first node (dummy variable)
  PseudotreeNode* ptroot = m_pseudotree->getRoot();
  SearchNode* node = new SearchNodeOR(NULL, ptroot->getVar() );
  m_space->root = node;
  // create dummy variable's AND node (domain size 1)
  SearchNode* next = new SearchNodeAND(m_space->root, 0);
  m_space->root->addChild(next);

  m_stack.push(next);

}


