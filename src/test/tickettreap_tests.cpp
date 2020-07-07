// Copyright (c) 2013-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test/test_paicoin.h"
#include <boost/test/unit_test.hpp>
#include "stake/treap/tickettreap.h"

uint256 uint32ToKey(uint32_t val)
{
  unsigned char buf[4];
  uint32_t uval = val;
  buf[3] = uval;
  buf[2] = uval >> 8;
  buf[1] = uval >> 16;
  buf[0] = uval >> 24; 
  std::vector<unsigned char> v;
  v.assign(32,0);
  std::copy(std::begin(buf), std::end(buf), std::end(v)-4);
  return uint256(v);
}

uint256 uint32ToHash(uint32_t val)
{
  unsigned char buf[4];
  uint32_t uval = val;
  buf[3] = uval;
  buf[2] = uval >> 8;
  buf[1] = uval >> 16;
  buf[0] = uval >> 24;

  return Hash(std::begin(buf),std::end(buf)); 
}

BOOST_FIXTURE_TEST_SUITE(tickettreap_tests, BasicTestingSetup)

// TestParentStack ensures the treapParentStack functionality works as intended.
BOOST_AUTO_TEST_CASE(parent_stack)
{
    auto tests = std::array<int,3>{
        1,
        StaticDepth,
        StaticDepth+1
    };

    for( auto& num_nodes : tests) 
    {
        std::vector<TreapNodePtr> nodes;
        for(int j = 0; j < num_nodes; ++j) {
            const auto key = uint32ToKey(uint32_t(j));
            const auto value = Value( uint32_t(j) );
            const auto node = std::make_shared<TreapNode>(key, value, 0);
            nodes.push_back(node);
        }
        // Push all of the nodes onto the parent stack while testing
        // various stack properties.
        auto stack = ParentStack();
        for (int j = 0; j < nodes.size(); ++j) {
            stack.push(nodes[j]);
            // Ensure the stack length is the expected value.
            BOOST_CHECK(stack.len() == j+1);

            // Ensure the node at each index is the expected one.
            for(int k = 0; k <= j; ++k) {
                auto atNode = stack.at(j-k);
                BOOST_CHECK(atNode == nodes[k]);
            }
        }

        // Ensure each popped node is the expected one.
        for (int j = 0; j < nodes.size(); ++j) {
            auto node = stack.pop();
            auto expected = nodes[nodes.size()-j-1];
            BOOST_CHECK(node == expected);
        }

        // Ensure the stack is now empty.
        BOOST_CHECK(stack.len() == 0);

        // Ensure attempting to retrieve a node at an index beyond the
        // stack's length returns nullptr.
        BOOST_CHECK_EQUAL(stack.at(2), nullptr);

        // Ensure attempting to pop a node from an empty stack returns
        // nullptr.
        BOOST_CHECK_EQUAL(stack.pop(), nullptr);
    }
}

// TestImmutableEmpty ensures calling functions on an empty immutable treap
// works as expected.
BOOST_AUTO_TEST_CASE(empty_tickettreap)
{
    // Ensure the treap length is the expected value.
    auto testTreap = TicketTreap{};
    BOOST_CHECK(testTreap.len() == 0);

    // Ensure the reported size is 0.
    BOOST_CHECK(testTreap.size() == 0);

    // Ensure there are no errors with requesting keys from an empty treap.
    auto key = uint256();
    BOOST_CHECK(false == testTreap.has(key));
    BOOST_CHECK(!testTreap.get(key));

    // Ensure there are no errors when deleting keys from an empty treap.
    testTreap.deleteKey(key);

    // Ensure the number of keys iterated by ForEach on an empty treap is
    // zero.
    int numIterated = 0;
    testTreap.forEach(
        [&numIterated](const uint256&, const Value&)->bool {
            numIterated++;
            return true;
        }
    );
    BOOST_CHECK(numIterated == 0);

    BOOST_CHECK_THROW(testTreap.getByIndex(-1), std::runtime_error);

    BOOST_CHECK_THROW(testTreap.getByIndex(0), std::runtime_error);
}

// TestImmutableSequential ensures that putting keys into an immutable treap in
// sequential order works as expected.
BOOST_AUTO_TEST_CASE(sequential_tickettreap)
{
    // Insert a bunch of sequential keys while checking several of the treap
    // functions work as expected.
    auto expectedSize = 0ull;
    auto numItems = 1000;
    auto testTreap = TicketTreap();
    for (int i = 0; i < numItems; i++) {
        auto key = uint32ToKey(uint32_t(i));
        auto value = Value( uint32_t(i) );
        testTreap = testTreap.put(key, value);

        // Ensure the treap length is the expected value.
        BOOST_CHECK(testTreap.len() == i+1);

        // Ensure the treap has the key.
        BOOST_CHECK(testTreap.has(key));

        // Get the key from the treap and ensure it is the expected
        // value.
        BOOST_CHECK(testTreap.get(key) == value);

        // Ensure the expected size is reported.
        expectedSize += sizeof(TreapNode);
        BOOST_CHECK (expectedSize == testTreap.size());

        // test GetByIndex
        const auto& pair = testTreap.getByIndex(i);
        BOOST_CHECK(key   == pair.first);
        BOOST_CHECK(value == pair.second);
    }

    // assert panic for GetByIndex out of bounds
    BOOST_CHECK_THROW(testTreap.getByIndex(numItems), std::runtime_error);

    BOOST_CHECK(testTreap.isHeap());

    // Ensure the all keys are iterated by ForEach in order.
    int numIterated = 0;
    testTreap.forEach(
        [&numIterated](const uint256& k, const Value& v) {
            // Ensure the key is as expected.
            auto wantKey = uint32ToKey(uint32_t(numIterated));
            BOOST_CHECK(k == wantKey);

            // Ensure the value is as expected.
            const auto wantValue = Value(numIterated);
            BOOST_CHECK(v == wantValue);

            numIterated++;
            return true;
        }
    );

    // Ensure all items were iterated.
    BOOST_CHECK(numItems == numIterated);

    numIterated = 0;
    // query top 5% of the tree, check height less than the less-than-height
    // requested
    auto queryHeight = uint32_t(50) / 20;
    testTreap.forEachByHeight( 
        queryHeight,
        [&numIterated, queryHeight](const uint256& k, const Value& v) {
            // Ensure the height is as expected.
            BOOST_CHECK(v.height < queryHeight);

            numIterated++;
            return true;
        }
    );

    // Ensure all items were iterated.
    BOOST_CHECK( numIterated == queryHeight);

    // Delete the keys one-by-one while checking several of the treap
    // functions work as expected.
    for (int i = 0; i < numItems; i++) {
        auto key = uint32ToKey(uint32_t(i));
        testTreap = testTreap.deleteKey(key);

        auto expectedLen = numItems - i - 1;
        auto expectedHeadValue = i + 1;

        // Ensure the treap length is the expected value.
        BOOST_CHECK(testTreap.len() == expectedLen);

        // Ensure the treap no longer has the key.
        BOOST_CHECK(false == testTreap.has(key));

        // test GetByIndex is correct at the head of the treap.
        if (expectedLen > 0) {
            const auto& pair = testTreap.getByIndex(0);
            BOOST_CHECK(pair.first == uint32ToKey(uint32_t(expectedHeadValue)));
        }

        // test GetByIndex is correct at the mid of the treap.
        if (expectedLen > 0) {
            auto halfIdx = expectedLen / 2;
            const auto& pair = testTreap.getByIndex(halfIdx);
            BOOST_CHECK(pair.first == uint32ToKey(uint32_t(expectedHeadValue + halfIdx )));
        }

        // test GetByIndex is correct at the tail of the treap.
        if (expectedLen > 0) {
            const auto& pair = testTreap.getByIndex(expectedLen - 1);
            BOOST_CHECK(pair.first == uint32ToKey(uint32_t(expectedHeadValue + expectedLen - 1)));
        }

        // Get the key that no longer exists from the treap and ensure
        // it is nil.
        BOOST_CHECK(!testTreap.get(key));

        // Ensure the expected size is reported.
        expectedSize -= sizeof(TreapNode);
        BOOST_CHECK(testTreap.size() == expectedSize);
    }
}

// TestImmutableReverseSequential ensures that putting keys into an immutable
// treap in reverse sequential order works as expected.
BOOST_AUTO_TEST_CASE(reverse_sequential_tickettreap)
{
    // Insert a bunch of sequential keys while checking several of the treap
    // functions work as expected.
    auto expectedSize = 0ull;
    auto numItems = 1000;
    auto testTreap = TicketTreap();
    for (int i = 0; i < numItems; i++) {
        const auto key = uint32ToKey(uint32_t(numItems - i - 1));
        const auto value = Value(uint32_t(numItems - i - 1));
        testTreap = testTreap.put(key, value);

        // Ensure the treap length is the expected value.
        BOOST_CHECK(testTreap.len() == i + 1);

        // Ensure the treap has the key.
        BOOST_CHECK(testTreap.has(key));

        // Get the key from the treap and ensure it is the expected
        // value.
        BOOST_CHECK(*testTreap.get(key) == value);

        // Ensure the expected size is reported.
        expectedSize += sizeof(TreapNode);
        BOOST_CHECK(testTreap.size() == expectedSize);
    }

    BOOST_CHECK(testTreap.isHeap());

    // Ensure the all keys are iterated by ForEach in order.
    int numIterated = 0;
    testTreap.forEach(
        [&numIterated](const uint256& k, const Value& v) {
            // Ensure the key is as expected.
            const auto wantKey = uint32ToKey(uint32_t(numIterated));
            BOOST_CHECK(k == wantKey);

            // Ensure the value is as expected.
            const auto wantValue = Value(numIterated);
            BOOST_CHECK(v == wantValue);

            numIterated++;
            return true;
        }
    );

    // Ensure all items were iterated.
    BOOST_CHECK(numItems == numIterated);

    // Delete the keys one-by-one while checking several of the treap
    // functions work as expected.
    for (int i = 0; i < numItems; i++) {
        // Intentionally use the reverse order they were inserted here.
        const auto key = uint32ToKey(uint32_t(i));
        testTreap = testTreap.deleteKey(key);

        // Ensure the treap length is the expected value.
        BOOST_CHECK(testTreap.len() == numItems-i-1);

        // Ensure the treap no longer has the key.
        BOOST_CHECK(false == testTreap.has(key));

        // Get the key that no longer exists from the treap and ensure
        // it is nil.
        BOOST_CHECK(!testTreap.get(key));

        BOOST_CHECK(testTreap.isHeap());

        // Ensure the expected size is reported.
        expectedSize -= sizeof(TreapNode);
        BOOST_CHECK(testTreap.size() == expectedSize);
    }
}

// TestImmutableUnordered ensures that putting keys into an immutable treap in
// no paritcular order works as expected.
BOOST_AUTO_TEST_CASE(unordered_tickettreap)
{
    // Insert a bunch of out-of-order keys while checking several of the
    // treap functions work as expected.
    auto expectedSize = 0ull;
    auto numItems = 1000;
    auto testTreap = TicketTreap();
    for (int i = 0; i < numItems; i++) {
        // Hash the serialized int to generate out-of-order keys.
        const auto key = uint32ToHash(uint32_t(i));
        const auto value = Value(uint32_t(i));

        testTreap = testTreap.put(key, value);

        // Ensure the treap length is the expected value.
        BOOST_CHECK(testTreap.len() == i + 1);

        // Ensure the treap has the key.
        BOOST_CHECK(testTreap.has(key));

        // Get the key from the treap and ensure it is the expected
        // value.
        BOOST_CHECK(*testTreap.get(key) == value);

        // Ensure the expected size is reported.
        expectedSize += sizeof(TreapNode);
        BOOST_CHECK(testTreap.size() == expectedSize);
    }

    // Delete the keys one-by-one while checking several of the treap
    // functions work as expected.
    for (int i = 0; i < numItems; i++) {
        // Hash the serialized int to generate out-of-order keys.
        const auto key = uint32ToHash(uint32_t(i));
        testTreap = testTreap.deleteKey(key);

        // Ensure the treap length is the expected value.
        BOOST_CHECK(testTreap.len() == numItems-i-1);

        // Ensure the treap no longer has the key.
        BOOST_CHECK(false == testTreap.has(key));

        // Get the key that no longer exists from the treap and ensure
        // it is nil.
        BOOST_CHECK(!testTreap.get(key));

        // Ensure the expected size is reported.
        expectedSize -= sizeof(TreapNode);
        BOOST_CHECK(testTreap.size() == expectedSize);
    }
}

// TestImmutableDuplicatePut ensures that putting a duplicate key into an
// immutable treap works as expected.
BOOST_AUTO_TEST_CASE(duplicate_put_tickettreap)
{
    const auto expectedVal = Value(10000);
    auto expectedSize = 0ull;
    const auto numItems = 1000;
    auto testTreap = TicketTreap();
    for (int i = 0; i < numItems; i++) {
        const auto key = uint32ToKey(uint32_t(i));
        const auto value = Value(uint32_t(i));
        testTreap = testTreap.put(key, value);
        expectedSize += sizeof(TreapNode);

        // Put a duplicate key with the the expected final value.
        testTreap = testTreap.put(key, expectedVal);

        // Ensure the key still exists and is the new value.
        BOOST_CHECK(testTreap.has(key));
        BOOST_CHECK(*testTreap.get(key) == expectedVal);

        // Ensure the expected size is reported.
        BOOST_CHECK(testTreap.size() == expectedSize);
    }
}

// TestImmutableNilValue ensures that putting a nil value into an immutable
// treap results in a NOOP.
BOOST_AUTO_TEST_CASE(null_value_tickettreap)
{
    auto key = uint32ToKey(0);

    // Put the key with a nil value.
    auto testTreap = TicketTreap();
    // testTreap = testTreap.put(key, nullptr);

    // Ensure the key does NOT exist.
    BOOST_CHECK(false == testTreap.has(key));
    BOOST_CHECK(!testTreap.get(key));
}

// TestImmutableForEachStopIterator ensures that returning false from the ForEach
// callback on an immutable treap stops iteration early.
BOOST_AUTO_TEST_CASE(foreach_stopiterator_tickettreap)
{
    // Insert a few keys.
    auto numItems = 10;
    auto testTreap = TicketTreap();
    for (int i = 0; i < numItems; i++) {
        auto key = uint32ToKey(uint32_t(i));
        auto value = Value(uint32_t(i));
        testTreap = testTreap.put(key, value);
    }

    // Ensure ForEach exits early on false return by caller.
    int numIterated = 0;
    testTreap.forEach(
        [&numIterated, numItems](const uint256& k, const Value&) {
            numIterated++;
            return numIterated != numItems/2;
        }
    );

    BOOST_CHECK(numIterated == numItems/2);
}

// TestImmutableSnapshot ensures that immutable treaps are actually immutable by
// keeping a reference to the previous treap, performing a mutation, and then
// ensuring the referenced treap does not have the mutation applied.
BOOST_AUTO_TEST_CASE(snapshot_tickettreap)
{
    // Insert a bunch of sequential keys while checking several of the treap
    // functions work as expected.
    auto expectedSize = 0ull;
    auto numItems = 1000;
    auto testTreap = TicketTreap();
    for (int i = 0; i < numItems; i++) {
        const auto treapSnap = testTreap;

        const auto key = uint32ToKey(uint32_t(i));
        const auto value = Value(uint32_t(i));
        testTreap = testTreap.put(key, value);

        // Ensure the length of the treap snapshot is the expected
        // value.
        BOOST_CHECK(treapSnap.len() == i);

        // Ensure the treap snapshot does not have the key.
        BOOST_CHECK(false == treapSnap.has(key));

        // Get the key that doesn't exist in the treap snapshot and
        // ensure it is nil.
        BOOST_CHECK(!treapSnap.get(key));

        // Ensure the expected size is reported.
        BOOST_CHECK(treapSnap.size() == expectedSize);

        expectedSize += sizeof(TreapNode);
    }

    // Delete the keys one-by-one while checking several of the treap
    // functions work as expected.
    for (int i = 0; i < numItems; i++) {
        auto treapSnap = testTreap;

        auto key = uint32ToKey(uint32_t(i));
        auto value = std::make_shared<Value>(uint32_t(i));
        testTreap = testTreap.deleteKey(key);

        // Ensure the length of the treap snapshot is the expected
        // value.
        BOOST_CHECK(treapSnap.len() == numItems-i);

        // Ensure the treap snapshot still has the key.
        BOOST_CHECK(treapSnap.has(key));

        // Get the key from the treap snapshot and ensure it is still
        // the expected value.
        BOOST_CHECK(*treapSnap.get(key) == *value);

        // Ensure the expected size is reported.
        BOOST_CHECK(treapSnap.size() == expectedSize);

        expectedSize -= sizeof(TreapNode);
    }
}

BOOST_AUTO_TEST_SUITE_END()
