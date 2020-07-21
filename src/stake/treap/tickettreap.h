#ifndef PAICOIN_STAKE_TICKETTREAP_H
#define PAICOIN_STAKE_TICKETTREAP_H

#include "treapnode.h"
#include "prevector.h"
#include <functional>
#include <boost/optional.hpp>

typedef std::function<bool(const uint256&, const Value&)> Predicate;
typedef std::shared_ptr<TicketTreap> TicketTreapPtr;
// Immutable class that represents a treap data structure which is used to hold ordered
// key/value pairs using a combination of binary search tree and heap semantics.
//
// In other parts of the code base, we see a traditional implementation of the
// treap with a randomised priority.
//
// Note that this treap has been modified from the original in that the
// priority, rather than being a random value, is deterministically assigned the
// monotonically increasing block height.
//
// However what is interesting is that we see similar behaviour to the treap
// structure, because the keys themselves are totally randomised, degenerate
// cases of trees with a bias for leftness or rightness cannot occur.
//
// Do make note however, if the keys were not to be randomised, this would not
// be a structure which can be relied upon to self-balance as treaps do.
//
// What motivated this alteration of the treap priority was that it can be used
// as a priority queue to discover the elements at the smallest height, which
// substantially improves application performance in one critical spot, via
// use of ForEachByHeight.
//
// All operations which result in modifying the treap return a new version of
// the treap with only the modified nodes updated.  All unmodified nodes are
// shared with the previous version.  This is extremely useful in concurrent
// applications since the caller only has to atomically replace the treap
// pointer with the newly returned version after performing any mutations.  All
// readers can simply use their existing pointer as a snapshot since the treap
// it points to is immutable.  This effectively provides O(1) snapshot
// capability with efficient memory usage characteristics since the old nodes
// only remain allocated until there are no longer any references to them.
class TicketTreap final
{
public:
    TicketTreap();
private:
    explicit TicketTreap(TreapNodePtr treapNode, int count, uint64_t totalSize);

public:
    // Len returns the number of items stored in the treap.
    int len() const;

    // Size returns a best estimate of the total number of bytes the treap is
    // consuming including all of the fields used to represent the nodes as well as
    // the size of the keys and values.  Shared values are not detected, so the
    // returned size assumes each value is pointing to different memory.
    uint64_t size() const;

    // Has returns whether or not the passed key exists.
    bool has(const uint256& key) const;

    // Get returns the value for the passed key.  The function will return nil when
    // the key does not exist.
    boost::optional<Value> get(const uint256& key) const;

    // GetByIndex returns the (Key, *Value) at the given position and panics if idx
    // is out of bounds.
    KeyValuePair getByIndex(int idx) const;

    // Put inserts the passed key/value pair.  Passing a nil value will result in a
    // NOOP.
    TicketTreap put(const uint256& key, const Value& value) const;

    // Delete removes the passed key from the treap and returns the resulting treap
    // if it exists.  The original immutable treap is returned if the key does not
    // exist.
    TicketTreap deleteKey(const uint256& key) const;

    // ForEach invokes the passed function with every key/value pair in the treap
    // in ascending order.
    void forEach(Predicate func) const;

    // ForEachByHeight iterates all elements in the tree less than a given height in
    // the blockchain.
    void forEachByHeight(uint32_t heightLessThan, Predicate func) const;

    // fetchWinners is a ticket database specific function which iterates over the
    // entire treap and finds winners at selected indexes.  These are returned
    // as a slice of pointers to keys, which can be recast as []*chainhash.Hash.
    // Importantly, it maintains the list of winners in the same order as specified
    // in the original idxs passed to the function.
    std::vector<uint256> fetchWinners(const prevector<64, uint32_t>& idxs) const;

    // Tests whether the treap meets the min-heap invariant.
    bool isHeap() const;
private:
    // get returns the treap node that contains the passed key.  It will return nil
    // when the key does not exist.
    TreapNodePtr get_node(const uint256& key) const;
private:
    TreapNodePtr    root;
    int             count;

    // totalSize is the best estimate of the total size of of all data in
    // the treap including the keys, values, and node sizes.
    uint64_t        totalSize;
};

#endif // PAICOIN_STAKE_TICKETTREAP_H