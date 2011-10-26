/*
 * BranchAndBound.h
 *
 *  Created on: Nov 3, 2008
 *      Author: lars
 */

#ifndef BRANCHANDBOUND_H_
#define BRANCHANDBOUND_H_

#include "Search.h"


#ifdef ANYTIME_BREADTH
/*
 * Minor extension of STL stack class, used by AOBB for the 'stack tree'
 */
class MyStack : public stack<SearchNode*> {
protected:
  size_t m_children;
  MyStack* m_parent;
public:
  size_t getChildren() const { return m_children; }
  void addChild() { m_children += 1; }
  void delChild() { m_children -= 1; }
  MyStack* getParent() const { return m_parent; }
public:
  MyStack(MyStack* p) : m_children(0), m_parent(p) {}
};
#endif


/* Branch and Bound search, implements pure virtual functions
 * from Search.h */
class BranchAndBound : virtual public Search {

protected:
#ifdef ANYTIME_BREADTH
  size_t m_stackCount;        // counter for current stack
  size_t m_stackLimit;        // expansion limit for stack rotation
  MyStack* m_rootStack;       // the root stack
  queue<MyStack*> m_stacks;   // the queue of active stacks
#else
#ifdef ANYTIME_DEPTH
  stack<SearchNode*> m_stackDive; // first stack for initial dives
#endif
  stack<SearchNode*> m_stack; // The DFS stack of nodes
#endif


protected:

  bool isDone() const;

  bool doExpand(SearchNode* n);

  void resetSearch(SearchNode* p);

  SearchNode* nextNode();

  bool isMaster() const { return false; }

public:

#ifdef ANYTIME_BREADTH
  void setStackLimit(size_t s) { m_stackLimit = s; }
#endif

  /*
  void setInitialSolution(double
#ifndef NO_ASSIGNMENT
    , const vector<val_t>&
#endif
  ) const;
  */

public:
  BranchAndBound(Problem* prob, Pseudotree* pt, SearchSpace* space, Heuristic* heur) ;
  virtual ~BranchAndBound() {}
};


/* Inline definitions */


inline bool BranchAndBound::isDone() const {
#if defined ANYTIME_BREADTH
  assert(false); // TODO
#elif defined ANYTIME_DEPTH
  return (m_stack.empty() && m_stackDive.empty());
#else
  return m_stack.empty();
#endif
}

inline void BranchAndBound::resetSearch(SearchNode* p) {
  assert(p);
#ifdef ANYTIME_BREADTH
  // TODO
  while (m_stacks.size()) {
    delete m_stacks.front();
    m_stacks.pop();
  }
  m_rootStack = new MyStack(NULL);
  m_rootStack->push(p);
#else
#ifdef ANYTIME_DEPTH
  while (!m_stackDive.empty())
    m_stackDive.pop();
#endif
  while (!m_stack.empty())
      m_stack.pop();
  m_stack.push(p);
#endif
}


#endif /* BRANCHANDBOUND_H_ */
