#include "stake/treap/treapnode.h"
#include "tinyformat.h"

TreapNode::TreapNode(const uint256& key, const Value& value, uint32_t priority) :
    key{key},
    value{value},
    priority{priority},
    size{1},
    left{nullptr},
    right{nullptr}
{
}

TreapNodePtr TreapNode::clone() const
{
    auto result = std::make_shared<TreapNode>(this->key, this->value, this->priority);
    result->size = this->size;
    result->left = this->left;
    result->right = this->right;
    return result;
}

KeyValuePair TreapNode::getByIndex(int idx) const
{
    if (idx < 0 || idx >= int(size)) {
        throw std::runtime_error(strprintf("getByIndex(%d) index out of bounds", idx));
    }
    auto node = this;
    while(true) {
        if (node->left == nullptr) {
            if (idx == 0) {
                return {node->key, node->value};
            }
            --idx;
            node = node->right.get();
        } else {
            if (idx < int(node->left->size)) {
                node = node->left.get();
            } else if (idx == int(node->left->size)) {
                return {node->key, node->value};
            } else {
                auto t1 = node->right.get();
                auto t2 = idx - int(node->left->size) - 1;
                node = t1;
                idx  = t2;
            }
        }
    }
}

bool TreapNode::isHeap() const
{
    auto bLeft  = left == nullptr || (left->priority >= priority && left->isHeap());
    auto bRight = right == nullptr || (right->priority >= priority && right->isHeap());

    return bLeft && bRight;
}

uint32_t TreapNode::leftSize() const
{
    if (left != nullptr) {
        return left->size;
    }
    return 0;
}

uint32_t TreapNode::rightSize() const
{
    if (right != nullptr) {
        return right->size;
    }
    return 0;
}



ParentStack::ParentStack()
 : index(0)
{
}

int ParentStack::len() const
{
    return index;
}

TreapNodePtr ParentStack::at(int n) const
{
    auto idx = index - n - 1;
    if (idx < 0) {
        return nullptr;
    }

    if (idx < items.size()) {
        return items[idx];
    }

    return overflow[idx-items.size()];
}

TreapNodePtr ParentStack::pop()
{
    if (index == 0) {
        return nullptr;
    }

    --index;
    if (index < items.size()) {
        auto node = items[index];
        items[index] = nullptr;
        return node;
    }

    auto node = overflow[index-items.size()];
    overflow[index-items.size()] = nullptr;
    return node;
}

void ParentStack::push(TreapNodePtr node)
{
    if (index < items.size()) {
        items[index] = node;
        ++index;
        return;
    }

    // TODO check if this is true in our case or is a Golang only issue

    // This approach is used over append because reslicing the slice to pop
    // the item causes the compiler to make unneeded allocations.  Also,
    // since the max number of items is related to the tree depth which
    // requires expontentially more items to increase, only increase the cap
    // one item at a time.  This is more intelligent than the generic append
    // expansion algorithm which often doubles the cap.
    auto idx = index - items.size();
    if (idx+1 > overflow.capacity()) {
        overflow.reserve(idx+1);
    }
    overflow.push_back(node);
    ++index;
}