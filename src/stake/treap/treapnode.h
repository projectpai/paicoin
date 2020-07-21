#ifndef PAICOIN_STAKE_TREAPNODE_H
#define PAICOIN_STAKE_TREAPNODE_H

#include <tuple>
#include <memory>
#include "stake/treap/value.h"
#include "uint256.h"

#include <array>

constexpr u_int8_t StaticDepth = 128;

class TreapNode;
typedef std::shared_ptr<TreapNode>  TreapNodePtr;
typedef std::pair<uint256,Value> KeyValuePair; 

class TicketTreap;

class TreapNode final
{
friend TicketTreap;

public:
    // Creates a node from the given key, value, and priority. The node is not initially linked to any others.
    TreapNode(const uint256& key, const Value& value, uint32_t priority);

public:
    // Returns the (Key, Value) at the given position.
    KeyValuePair getByIndex(int idx) const;
    // Tests whether the treap meets the min-heap invariant.
    bool isHeap() const;
    // Returns the size of the subtree on the left-hand side, and zero if there is no tree present there.
    uint32_t leftSize() const;
    // Returns the size of the subtree on the right-hand side, and zero if there is no tree present there.
    uint32_t rightSize() const;

    TreapNodePtr clone() const;

private:
    uint256 key;
    Value value;
    uint32_t priority;
    uint32_t size; // Count of items within this treap - the node itself counts as 1.
    TreapNodePtr left;
    TreapNodePtr right;
};

class ParentStack final
{
public:
    ParentStack();
public:
    // Len returns the current number of items in the stack.
    int len() const;

    // At returns the item n number of items from the top of the stack, where 0 is
    // the topmost item, without removing it.  It returns nil if n exceeds the
    // number of items on the stack.
    TreapNodePtr at(int n) const;

    // Pop removes the top item from the stack.  It returns nil if the stack is
    // empty.
    TreapNodePtr pop();

    // Push pushes the passed item onto the top of the stack.
    void push(TreapNodePtr node);
private:
    int index;
    std::array<TreapNodePtr,StaticDepth> items;
    std::vector<TreapNodePtr> overflow;
};

#endif // PAICOIN_STAKE_TREAPNODE_H
