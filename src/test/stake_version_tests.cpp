//
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017-2020 Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//

// Distributed under the MIT/X11 software license, see the accompanying

#include "chain.h"
#include "chainparams.h"
#include "random.h"
#include "util.h"
#include "test/test_paicoin.h"
#include "validation.h"
#include "stake/stakeversion.h"

#include <boost/test/unit_test.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
/*
struct TestingSetup_TEST : public TestingSetup
{
    explicit TestingSetup_TEST(const std::string& chainName = CBaseChainParams::TESTNET)
    : TestingSetup(chainName)
    {
        ;
    };
};

struct TestingSetup_REG : public TestingSetup
{
    explicit TestingSetup_REG(const std::string& chainName = CBaseChainParams::REGTEST)
    : TestingSetup(chainName)
    {
        ;
    };
};

struct CalcWantHeightTest
{
    std::string name;
    int64_t skip;
    int64_t interval;
    int64_t multiplier;
};

struct CalcStakeVersionTest
{
    std::string name;
    int64_t numNodes;
    uint64_t expectVersion;
    std::function<void(CBlockIndex*)> set;
};


static void CalcWantHeightLoop(const std::vector<CalcWantHeightTest>& tests)
{
    for (const auto& test : tests) {
        const auto& start = test.skip + test.interval*2;
        auto expectedHeight = start - 1; // zero based
        auto x = int64_t{0};
        for (int64_t i = start; i < test.multiplier*test.interval; i++) {
            if (x % test.interval == 0 && i != start) {
                expectedHeight += test.interval;
            }
            const auto& wantHeight = calcWantHeight(test.skip, test.interval, i);

            BOOST_CHECK_EQUAL(wantHeight, expectedHeight);

            x++;
        }
    }
}

BOOST_FIXTURE_TEST_SUITE(stake_version_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(calc_want_height_MAIN)
{
    // For example, if StakeVersionInterval = 11 and StakeValidationHeight = 13 the
    // windows start at 13 + (11 * 2) 25 and are as follows: 24-34, 35-45, 46-56 ...
    // If height comes in at 35 we use the 24-34 window, up to height 45.
    // If height comes in at 46 we use the 35-45 window, up to height 56 etc.
    const auto& mainparams = Params().GetConsensus();
    const auto& tests = std::vector<CalcWantHeightTest>{
        {
            "13 11 10000",
            13,
            11,
            10000,
        },
        {
            "27 33 10000",
            27,
            33,
            10000,
        },
        {
            "mainnet params",
            mainparams.nStakeValidationHeight,
            mainparams.nStakeVersionInterval,
            5000,
        },
        {
            "negative mainnet params",
            mainparams.nStakeValidationHeight,
            mainparams.nStakeVersionInterval,
            1000,
        },
    };
    
    CalcWantHeightLoop(tests);
}

BOOST_FIXTURE_TEST_CASE(calc_want_height_TEST, TestingSetup_TEST)
{
    const auto& testparams = Params().GetConsensus();
    const auto& tests = std::vector<CalcWantHeightTest>{
        {
            "testnet3 params",
            testparams.nStakeValidationHeight,
            testparams.nStakeVersionInterval,
            1000,
        },
    };
    
    CalcWantHeightLoop(tests);
}

BOOST_FIXTURE_TEST_CASE(calc_want_height_REG, TestingSetup_REG)
{
    const auto& regparams = Params().GetConsensus();
    const auto& tests = std::vector<CalcWantHeightTest>{
        {
            "regnet params",
            regparams.nStakeValidationHeight,
            regparams.nStakeVersionInterval,
            10000,
        },
    };
    
    CalcWantHeightLoop(tests);
}

// newFakeNode creates a block node connected to the passed parent with the
// provided fields populated and fake values for the other fields.
CBlockIndex* newFakeNode(CBlockIndex* parent,int32_t blockVersion, uint32_t stakeVersion, uint32_t bits, uint32_t timestamp)
{
    assert(parent != nullptr);

    // Make up a header and create a block node from it.
    const auto& nextHeight = parent->nHeight + 1;
    auto bheader = CBlockHeader{};
    bheader.nVersion = blockVersion;
    bheader.nVoteBits = VoteBits::rttAccepted;
    bheader.nBits = bits;
    bheader.nTime = timestamp;
    bheader.nStakeVersion = stakeVersion;

    auto block = new CBlockIndex(bheader);
    block->nHeight = nextHeight;
    block->pprev = parent;
    block->phashBlock = new uint256(bheader.GetHash());
    return block;
}

// appendFakeVotes appends the passed number of votes to the node with the
// provided version and vote bits.
void appendFakeVotes(CBlockIndex* node, uint16_t numVotes, uint32_t voteVersion, VoteBits voteBits) {
    for (auto i = uint16_t{0}; i < numVotes; i++) {
        node->votes.push_back(VoteVersion{voteVersion, voteBits});
    }
}

BOOST_FIXTURE_TEST_CASE(calc_stake_version_REG, TestingSetup_REG)
{
    const auto& regparams = Params().GetConsensus();
    const auto& svh = regparams.nStakeValidationHeight;
    const auto& svi = regparams.nStakeVersionInterval;
    const auto& tpb = regparams.nTicketsPerBlock;

    const auto& tests = std::vector<CalcStakeVersionTest>{
        {
            "headerStake 2 votes 3",
            svh + svi*3,
            3,
            [&](CBlockIndex* node) {
                if (node->nHeight > svh) {
                    appendFakeVotes(node, tpb, 3, VoteBits::allRejected);
                    node->nStakeVersion = 2;
                    node->nVersion = 3;
                }
            },
        },
        {
            "headerStake 3 votes 2",
            svh + svi*3,
            3,
            [&](CBlockIndex* node) {
                if (node->nHeight > svh) {
                    appendFakeVotes(node, tpb, 2, VoteBits::allRejected);
                    node->nStakeVersion = 3;
                    node->nVersion = 3;
                }
            },
        },
    };

    for (const auto& test : tests) {
        auto fakeChain = chainActive;
        auto node = fakeChain.Tip();

        for (auto i = int64_t{1}; i <= test.numNodes; i++) {
            node = newFakeNode(node, 1, 0, 0, time(nullptr));
            test.set(node);
            fakeChain.SetTip(node);
        }

        const auto& version = calcStakeVersion(fakeChain.Tip(), regparams);
        BOOST_CHECK_EQUAL(version, test.expectVersion);
    }
}

BOOST_AUTO_TEST_SUITE_END()
*/