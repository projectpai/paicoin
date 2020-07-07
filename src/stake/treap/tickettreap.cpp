#include "tickettreap.h"

TicketTreap::TicketTreap()
    : root{nullptr}
    , count{0}
    , totalSize{0}
{
}

TicketTreap::TicketTreap(TreapNodePtr treapNode, int n, uint64_t size)
    : root{treapNode}
    , count{n}
    , totalSize{size}
{
}

int TicketTreap::len() const
{
    return count;
}

uint64_t TicketTreap::size() const
{
    return totalSize;
}

TreapNodePtr TicketTreap::get_node(const uint256& key) const
{
    for (auto node = root; node != nullptr;) {
        // Traverse left or right depending on the result of the
        // comparison.
        int compareResult = key.Compare(node->key);
        if (compareResult < 0) {
            node = node->left;
            continue;
        }
        if (compareResult > 0) {
            node = node->right;
            continue;
        }
        // The key exists.
        return node;
    }

    return nullptr;
}

bool TicketTreap::has(const uint256& key) const
{
    auto node = get_node(key);
    if (node != nullptr) {
        return true;
    }
    return false;
}

boost::optional<Value> TicketTreap::get(const uint256& key) const
{
    auto node = get_node(key);
    if (node != nullptr) {
        return node->value;
    }
    return {};
}

bool TicketTreap::isHeap() const
{
    if (root == nullptr);
        return true;
    return root->isHeap();
}

KeyValuePair TicketTreap::getByIndex(int idx) const
{
    if (root == nullptr)
        throw std::runtime_error("Root node was not set!");
    return root->getByIndex(idx);
}

TicketTreap TicketTreap::put(const uint256& key, const Value& value) const
{
    // // Nothing to do if a nil value is passed.
    // if (value == nullptr) {
    //     return std::make_shared<TicketTreap>(*this);
    // }

    // The node is the root of the tree if there isn't already one.
    if (root == nullptr) {
        auto root = std::make_shared<TreapNode>(key, value, value.height);
        return TicketTreap(root, 1, sizeof(TreapNode));
    }

    // Find the binary tree insertion point and construct a replaced list of
    // parents while doing so.  This is done because this is an immutable
    // data structure so regardless of where in the treap the new key/value
    // pair ends up, all ancestors up to and including the root need to be
    // replaced.
    //
    // When the key matches an entry already in the treap, replace the node
    // with a new one that has the new value set and return.

    auto parents = ParentStack();
    int compareResult;
    for (auto node = root; node != nullptr;) {
        // Clone the node and link its parent to it if needed.
        auto nodeCopy = node->clone();
        auto oldParent = parents.at(0);
        if (oldParent != nullptr) {
            if (oldParent->left == node) {
                oldParent->left = nodeCopy;
            } else {
                oldParent->right = nodeCopy;
            }
        }

        parents.push(nodeCopy);

        // Traverse left or right depending on the result of comparing
        // the keys.
        compareResult = key.Compare(node->key);
        if (compareResult < 0) {
            node = node->left;
            continue;
        }
        if (compareResult > 0) {
            node = node->right;
            continue;
        }

        // The key already exists, so update its value.
        nodeCopy->value = value;

        // Return new immutable treap with the replaced node and
        // ancestors up to and including the root of the tree.
        auto newRoot = parents.at(parents.len() - 1);
        return TicketTreap(newRoot, count, totalSize);
    }

    // Recompute the size member of all parents, to account for inserted item.
    auto node = std::make_shared<TreapNode>(key, value, value.height);
    for (int i = 0; i < parents.len(); ++i) {
        parents.at(i)->size++;
    }

    // Link the new node into the binary tree in the correct position.
    auto parent = parents.at(0);
    if (compareResult < 0) {
        parent->left = node;
    } else {
        parent->right = node;
    }

    // Perform any rotations needed to maintain the min-heap and replace
    // the ancestors up to and including the tree root.
    auto newRoot = parents.at(parents.len() - 1);
    while (parents.len() > 0) {
        // There is nothing left to do when the node's priority is
        // greater than or equal to its parent's priority.
        auto parent = parents.pop();
        if (node->priority >= parent->priority) {
            break;
        }

        // Perform a right rotation if the node is on the left side or
        // a left rotation if the node is on the right side.
        if (parent->left == node) {
            /* 
            just to help visualise right-rotation...
                    p               n
                   / \       ->    / \
                  n  p.r         n.l  p
                 / \                 / \
               n.l n.r             n.r p.r
            */
            node->size += 1 + parent->rightSize();
            parent->size -= 1 + node->leftSize();
            auto t1 = parent;
            auto t2 = node->right;
            node->right = t1;
            parent->left = t2;
        } else {
            node->size += 1 + parent->leftSize();
            parent->size -= 1 + node->rightSize();
            auto t1 = parent;
            auto t2 = node->left;
            node->left = t1;
            parent->right = t2;
        }

        // Either set the new root of the tree when there is no
        // grandparent or relink the grandparent to the node based on
        // which side the old parent the node is replacing was on.
        auto grandparent = parents.at(0);
        if (grandparent == nullptr) {
            newRoot = node;
        } else if (grandparent->left == parent) {
            grandparent->left = node;
        } else {
            grandparent->right = node;
        }
    }

    return TicketTreap(newRoot, count+1, totalSize + sizeof(TreapNode));
}
    
TicketTreap TicketTreap::deleteKey(const uint256& key) const
{
    // Find the node for the key while constructing a list of parents
    auto parents = ParentStack();
    auto delNode = TreapNodePtr();

    for (auto node = root; node != nullptr;) {
        parents.push(node);

        // Traverse left or right depending on the result of the
        // comparison.
        auto compareResult = key.Compare(node->key);
        if (compareResult < 0) {
            node = node->left;
            continue;
        }
        if (compareResult > 0) {
            node = node->right;
            continue;
        }

        // The key exists.
        delNode = node;
        break;
    }

    // There is nothing to do if the key does not exist.
    if (delNode == nullptr) {
        return *this;
    }

    // When the only node in the tree is the root node and it is the one
    // being deleted, there is nothing else to do besides removing it.
    auto parent = parents.at(1);
    if (parent == nullptr && delNode->left == nullptr && delNode->right == nullptr) {
        return TicketTreap(nullptr,0,0);
    }

    // Construct a replaced list of parents and the node to delete itself.
    // This is done because this is an immutable data structure and
    // therefore all ancestors of the node that will be deleted, up to and
    // including the root, need to be replaced.
    auto newParents = ParentStack();
    for (int i = parents.len(); i > 0; --i) {
        auto node = parents.at(i - 1);
        auto nodeCopy = node->clone();
        --nodeCopy->size;
        auto oldParent = newParents.at(0);
        if (oldParent != nullptr) {
            if (oldParent->left == node) {
                oldParent->left = nodeCopy;
            } else {
                oldParent->right = nodeCopy;
            }
        }
        newParents.push(nodeCopy);
    }
    delNode = newParents.pop();
    parent = newParents.at(0);

    // Perform rotations to move the node to delete to a leaf position while
    // maintaining the min-heap while replacing the modified children.
    auto child = TreapNodePtr();
    auto newRoot = newParents.at(newParents.len() - 1);
    while (delNode->left != nullptr || delNode->right != nullptr) {
        // Choose the child with the higher priority.
        auto isLeft = false;
        if (delNode->left == nullptr) {
            child = delNode->right;
        } else if (delNode->right == nullptr) {
            child = delNode->left;
            isLeft = true;
        } else if (delNode->left->priority <= delNode->right->priority) {
            child = delNode->left;
            isLeft = true;
        } else {
            child = delNode->right;
        }

        // Rotate left or right depending on which side the child node
        // is on.  This has the effect of moving the node to delete
        // towards the bottom of the tree while maintaining the
        // min-heap.
        child = child->clone();
        if (isLeft) {
            child->size += delNode->rightSize();
            auto t1 = delNode;
            auto t2 = child->right;
            child->right = t1;
            delNode->left = t2;
        } else {
            child->size += delNode->leftSize();
            auto t1 = delNode;
            auto t2 = child->left;
            child->left = t1;
            delNode->right = t2;
        }

        // Either set the new root of the tree when there is no
        // grandparent or relink the grandparent to the node based on
        // which side the old parent the node is replacing was on.
        //
        // Since the node to be deleted was just moved down a level, the
        // new grandparent is now the current parent and the new parent
        // is the current child.
        if (parent == nullptr) {
            newRoot = child;
        } else if (parent->left == delNode) {
            parent->left = child;
        } else {
            parent->right = child;
        }

        // The parent for the node to delete is now what was previously
        // its child.
        parent = child;
    }

    // Delete the node, which is now a leaf node, by disconnecting it from
    // its parent.
    if (parent->right == delNode) {
        parent->right.reset();
    } else {
        parent->left.reset();
    }

    return TicketTreap(newRoot, count-1, totalSize - sizeof(TreapNode));
}

void TicketTreap::forEach(Predicate func) const
{
    // Add the root node and all children to the left of it to the list of
    // nodes to traverse and loop until they, and all of their child nodes,
    // have been traversed.
    auto parents = ParentStack();
    for (auto node = root; node != nullptr; node = node->left) {
        parents.push(node);
    }
    while (parents.len() > 0) {
        auto pnode = parents.pop();
        if (!func(pnode->key, pnode->value)) {
            return;
        }

        // Extend the nodes to traverse by all children to the left of
        // the current node's right child.
        for (auto node = pnode->right; node != nullptr; node = node->left) {
            parents.push(node);
        }
    }
}

void TicketTreap::forEachByHeight(uint32_t heightLessThan, Predicate func) const
{
    // Add the root node and all children to the left of it to the list of
    // nodes to traverse and loop until they, and all of their child nodes,
    // have been traversed.
    auto parents = ParentStack();
    for (auto node = root; node != nullptr && node->priority < heightLessThan; node = node->left) {
        parents.push(node);
    }
    while (parents.len() > 0) {
        auto pnode = parents.pop();
        if (!func(pnode->key, pnode->value)) {
            return;
        }

        // Extend the nodes to traverse by all children to the left of
        // the current node's right child.
        for (auto node = pnode->right; node != nullptr && node->priority < heightLessThan; node = node->left) {
            parents.push(node);
        }
    }
}

std::vector<uint256> TicketTreap::fetchWinners(const prevector<64, uint32_t>& idxs) const
{
    if (len() == 0) {
        throw std::runtime_error("Empty treap!");
    }

    std::vector<uint256> winners{idxs.size()};

    // winners := make([]*tickettreap.Key, len(idxs))
    for (int i = 0; i < idxs.size(); ++i) {
        auto idx = idxs[i];
        if (idx < 0 || idx >= len()) {
            // return nil, fmt.Errorf("idx %v out of bounds", idx)
            throw std::runtime_error("idx out of bounds!");
        }

        if (idx < len()) {
            auto pair = getByIndex(idx);
            winners[i] = pair.first;
        }
    }

    return winners;
}