// Copyright (c) 2015 The Bitcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chain.h"
#include "chainparams.h"
#include "pow.h"
#include "random.h"
#include "util.h"
#include "test/test_paicoin.h"
#include "validation.h"

#include <boost/test/unit_test.hpp>

// ticketInfo is used to control the tests by specifying the details
// about how many fake blocks to create with the specified number of
// ticket and stake difficulty.
struct TicketInfo 
{
    uint32_t numNodes;
    uint8_t  newTickets;
    int64_t  stakeDiff;
};

struct CalculateSDiffTest
{
    std::string name;
    std::vector<TicketInfo> vTicketInfo;
    int64_t expectedDiff;
};

struct EstimateSDiffTest
{
    std::string name;
    std::vector<TicketInfo> vTicketInfo;
    int64_t  newTickets;
    bool useMaxTickets;
    int64_t expectedDiff;
};

static void loopTicketInfo(CChain& fakeChain, const std::vector<TicketInfo>& vTicketInfo, const Consensus::Params& params)
{
    // const auto& stakeEnabledHeight = params.nStakeEnabledHeight;
    const auto& stakeValidationHeight = params.nStakeValidationHeight;
    const auto& ticketMaturity = params.nTicketMaturity;
    // const auto& ticketExpiry = params.nTicketExpiry;
    const auto& ticketsPerBlock = params.nTicketsPerBlock;
    // const auto& maxFreshTicketsPerBlock = params.nMaxFreshStakePerBlock;

    // immatureTickets track which height the purchased tickets will
    // mature and thus be eligible for admission to the live ticket
    // pool.
    auto immatureTickets = std::map<uint32_t,uint8_t>{};
    auto poolSize  = uint32_t{};

    for (const auto& ticketInfo: vTicketInfo) 
    {
        // Ensure the test data isn't faking ticket purchases at
        // an incorrect difficulty.
        auto tip = fakeChain.Tip();
        const auto& req_diff = CalculateNextRequiredStakeDifficulty(tip, params);
        BOOST_CHECK_EQUAL(req_diff, ticketInfo.stakeDiff);

        for (uint32_t i = 0; i < ticketInfo.numNodes; ++i) 
        {
            // Make up a header.
            const auto& nextHeight = tip->nHeight + 1;
            auto bheader = CBlockHeader{};
            bheader.nVersion = tip->nVersion;
            bheader.nStakeDifficulty = ticketInfo.stakeDiff;
            bheader.nFreshStake = ticketInfo.newTickets,
            bheader.nTicketPoolSize = poolSize;

            auto block = new CBlockIndex(bheader);
            block->nHeight = nextHeight;
            block->pprev = tip;
            tip = block;

            // Update the pool size for the next header.
            // Notice how tickets that mature for this block
            // do not show up in the pool size until the
            // next block.  This is correct behavior.
            poolSize += immatureTickets[nextHeight];
            immatureTickets[nextHeight] = 0;
            if (nextHeight >= stakeValidationHeight) {
                poolSize -= ticketsPerBlock;
            }

            // Track maturity height for new ticket
            // purchases.
            const auto& maturityHeight = nextHeight + ticketMaturity;
            immatureTickets[maturityHeight] = ticketInfo.newTickets;

            // Update the chain to use the new fake node as
            // the new best node.
            fakeChain.SetTip(tip);
        }
    }

}

static void CalculateSDiffLoop(const std::vector<CalculateSDiffTest>& tests, const Consensus::Params& params)
{
    for (const auto& test : tests)
    {
        auto fakeChain = chainActive;
        loopTicketInfo(fakeChain, test.vTicketInfo, params);
        // Ensure the calculated difficulty matches the expected value.
        const auto& stake_diff = CalculateNextRequiredStakeDifficulty(fakeChain.Tip(), params);
        BOOST_CHECK_EQUAL(stake_diff, test.expectedDiff);
    }
}

static void EstimateSDiffLoop(const std::vector<EstimateSDiffTest>& tests, const Consensus::Params& params)
{
    for (const auto& test : tests)
    {
        auto fakeChain = chainActive;
        loopTicketInfo(fakeChain, test.vTicketInfo,params);
        // Ensure the calculated difficulty matches the expected value.
        const auto& gotDiff = EstimateNextStakeDifficulty(fakeChain.Tip(), test.newTickets, test.useMaxTickets, params);
        BOOST_CHECK_EQUAL(gotDiff, test.expectedDiff);
    }
}

BOOST_FIXTURE_TEST_SUITE(stake_difficulty_tests, TestingSetup)

// get_next_required_stake_diff ensure the stake diff calculation function works as expected.
BOOST_AUTO_TEST_CASE(get_next_required_stake_diff_MAIN)
{
    // const auto& my_tip = chainActive.Tip();
    const auto& params = Params().GetConsensus();
    const auto& minStakeDiff = params.nMinimumStakeDiff;

    const auto& tests = std::vector<CalculateSDiffTest>{
        {
            // Next retarget is at 144.  Prior to coinbase maturity,
            // so will always be the minimum.
            "genesis block",
            {{0, 0, minStakeDiff}},
            minStakeDiff},
        {
            // Next retarget is at 144.  Prior to coinbase maturity,
            // so will always be the minimum.
            "1st retarget, before coinbase",
            {{143, 0, minStakeDiff}},
            minStakeDiff,
        },
        {
            // Next retarget is at 288.
            //
            // Tickets could not possibly have been bought yet, but
            // ensure the algorithm handles it properly.
            "coinbase maturity with impossible num tickets",
            {{255, 20, minStakeDiff} },
            minStakeDiff,
        },
        {
            // Next retarget is at 288.
            //
            // Block 0 has no spendable outputs, so tickets could
            // not have possibly been bought yet.
            "coinbase maturity + 1",
            {
                {256, 0, minStakeDiff},
            },
            minStakeDiff,
        },
        {
            // Next retarget is at 288.
            "2nd retarget interval - 1, 100% demand",
            {
                {256, 0, minStakeDiff}, // 256
                {30, 20, minStakeDiff}, // 286
            },
            minStakeDiff,
        },
        {
            // Next retarget is at 288.
            "2nd retarget interval, 100% demand",
            {
                {256, 0, minStakeDiff}, // 256
                {31, 20, minStakeDiff}, // 287
            },
            minStakeDiff,
        },
        {
            // Next retarget is at 432.
            "3rd retarget interval, 100% demand",
            {
                {256, 0, minStakeDiff},  // 256
                {175, 20, minStakeDiff}, // 431
            },
            minStakeDiff,
        },
        {
            // Next retarget is at 2304.
            "16th retarget interval - 1, 100% demand",
            {
                {256, 0, minStakeDiff},   // 256
                {2046, 20, minStakeDiff}, // 2302
            },
            minStakeDiff,
        },
        {
            // Next retarget is at 2304.
            "16th retarget interval, 100% demand",
            {
                {256, 0, minStakeDiff},   // 256
                {2047, 20, minStakeDiff}, // 2303
            },
            208418769,
        },
        {
            // Next retarget is at 2448.
            "17th retarget interval - 1, 100% demand",
            {
                {256, 0, minStakeDiff},   // 256
                {2047, 20, minStakeDiff}, // 2303
                {143, 20, 208418769},     // 2446
            },
            208418769,
        },
        {
            // Next retarget is at 2448.
            "17th retarget interval, 100% demand",
            {
                {256, 0, minStakeDiff},   // 256
                {2047, 20, minStakeDiff}, // 2303
                {144, 20, 208418769},     // 2447
            },
            231326567,
        },
        {
            // Next retarget is at 2592.
            "17th retarget interval+1, 100% demand",
            {
                {256, 0, minStakeDiff},   // 256
                {2047, 20, minStakeDiff}, // 2303
                {144, 20, 208418769},     // 2447
                {1, 20, 231326567},       // 2448
            },
            231326567,
        },
        {
            // Next retarget is at 3456.
            "24th retarget interval, varying demand",
            {
                {256, 0, minStakeDiff},  // 256
                {31, 20, minStakeDiff},  // 287
                {144, 10, minStakeDiff}, // 431
                {144, 20, minStakeDiff}, // 575
                {144, 10, minStakeDiff}, // 719
                {144, 20, minStakeDiff}, // 863
                {144, 10, minStakeDiff}, // 1007
                {144, 20, minStakeDiff}, // 1151
                {144, 10, minStakeDiff}, // 1295
                {144, 20, minStakeDiff}, // 1439
                {144, 10, minStakeDiff}, // 1583
                {144, 20, minStakeDiff}, // 1727
                {144, 10, minStakeDiff}, // 1871
                {144, 20, minStakeDiff}, // 2015
                {144, 10, minStakeDiff}, // 2159
                {144, 20, minStakeDiff}, // 2303
                {144, 10, minStakeDiff}, // 2447
                {144, 20, minStakeDiff}, // 2591
                {144, 10, minStakeDiff}, // 2735
                {144, 20, minStakeDiff}, // 2879
                {144, 9, 201743368},     // 3023
                {144, 20, 201093236},    // 3167
                {144, 8, 222625877},     // 3311
                {144, 20, 242331291},    // 3455
            },
            291317641,
        },
        // NOTE: our SubsidyHalvingInterval is much larger than Decred's SubsidyReductionInterval
        // these numbers would never work, we need to come up with an entire different set
        // {
        //     // Next retarget is at 4176.  Post stake validation
        //     // height.
        //     "29th retarget interval, 100% demand",
        //     {
        //         {256, 0, minStakeDiff},   // 256
        //         {2047, 20, minStakeDiff}, // 2303
        //         {144, 20, 208418769},     // 2447
        //         {144, 20, 231326567},     // 2591
        //         {144, 20, 272451490},     // 2735
        //         {144, 20, 339388424},     // 2879
        //         {144, 20, 445827839},     // 3023
        //         {144, 20, 615949254},     // 3167
        //         {144, 20, 892862990},     // 3311
        //         {144, 20, 1354989669},    // 3455
        //         {144, 20, 2148473276},    // 3599
        //         {144, 20, 3552797658},    // 3743
        //         {144, 20, 6116808441},    // 3887
        //         {144, 20, 10947547379},   // 4031
        //         {144, 20, 20338554623},   // 4175
        //     },
        //     22097687698,
        // },
        {
            // Next retarget is at 4176.  Post stake validation
            // height.
            "29th retarget interval, 50% demand",
            {
                {256, 0, minStakeDiff},   // 256
                {3919, 10, minStakeDiff}, // 4175
            },
            minStakeDiff,
        },
        // NOTE: our SubsidyHalvingInterval is much larger than Decred's SubsidyReductionInterval
        // these numbers would never work, we need to come up with an entire different set
        // {
        //     // Next retarget is at 4464.  Post stake validation
        //     // height.
        //     "31st retarget interval, waning demand",
        //     {
        //         {256, 0, minStakeDiff},   // 256
        //         {2047, 20, minStakeDiff}, // 2303
        //         {144, 20, 208418769},     // 2447
        //         {144, 20, 231326567},     // 2591
        //         {144, 20, 272451490},     // 2735
        //         {144, 20, 339388424},     // 2879
        //         {144, 20, 445827839},     // 3023
        //         {144, 20, 615949254},     // 3167
        //         {144, 20, 892862990},     // 3311
        //         {144, 20, 1354989669},    // 3455
        //         {144, 20, 2148473276},    // 3599
        //         {144, 20, 3552797658},    // 3743
        //         {144, 13, 6116808441},    // 3887
        //         {144, 0, 10645659768},    // 4031
        //         {144, 0, 18046712136},    // 4175
        //         {144, 0, 22097687698},    // 4319
        //         {144, 0, 22152524112},    // 4463
        //     },
        //     22207360526,
        // },
    };

    CalculateSDiffLoop(tests, params);
}

// estimate_next_stake_diff ensures the function that estimates the stake
// diff calculation for the algorithm defined by DCP0001 works as expected.
BOOST_AUTO_TEST_CASE(estimate_next_stake_diff_MAIN)
{
    const auto& params = Params().GetConsensus();
    const auto& minStakeDiffMainNet = params.nMinimumStakeDiff;

    const auto& tests = std::vector<EstimateSDiffTest>{
        {
            // Regardless of claiming tickets will be purchased, the
            // resulting stake difficulty should be the minimum
            // because the first retarget is before the start
            // height.
            "genesis block",
            {{0, 0, minStakeDiffMainNet}},
            2860,
            false,
            minStakeDiffMainNet,
        },
        {
            // Next retarget is 144.  Resulting stake difficulty
            // should be the minimum regardless of claimed ticket
            // purchases because the previous pool size is still 0.
            "during retarget, but before coinbase",
            {{140, 0, minStakeDiffMainNet}},
            20 * 3, // blocks 141, 142, and 143.
            true,
            minStakeDiffMainNet,
        },
        {
            // Next retarget is at 288.  Regardless of claiming
            // tickets will be purchased, the resulting stake
            // difficulty should be the min because the previous
            // pool size is still 0.
            "at coinbase maturity",
            {{256, 0, minStakeDiffMainNet}},
            0,
            true,
            minStakeDiffMainNet,
        },
        {
            // Next retarget is at 288.  Regardless of actually
            // purchasing tickets and claiming more tickets will be
            // purchased, the resulting stake difficulty should be
            // the min because the previous pool size is still 0.
            "2nd retarget interval - 2, 100% demand",
            {
                {256, 0, minStakeDiffMainNet}, // 256
                {30, 20, minStakeDiffMainNet}, // 286
            },
            0,
            true,
            minStakeDiffMainNet,
        },
        {
            // Next retarget is at 288.  Still expect minimum stake
            // difficulty since the raw result would be lower.
            "2nd retarget interval - 1, 100% demand",
            {
                {256, 0, minStakeDiffMainNet}, // 256
                {31, 20, minStakeDiffMainNet}, // 287
            },
            0,
            true,
            minStakeDiffMainNet,
        },
        {
            // Next retarget is at 432.
            "3rd retarget interval, 100% demand, 1st block",
            {
                {256, 0, minStakeDiffMainNet}, // 256
                {32, 20, minStakeDiffMainNet}, // 288
            },
            0,
            true,
            minStakeDiffMainNet,
        },
        {
            // Next retarget is at 2304.
            "16th retarget interval, 100% demand, 1st block",
            {
                {256, 0, minStakeDiffMainNet},   // 256
                {1904, 20, minStakeDiffMainNet}, // 2160
            },
            0,
            true,
            208418769,
        },
        {
            // Next retarget is at 2304.
            "16th retarget interval, 100% demand, 2nd block",
            {
                {256, 0, minStakeDiffMainNet},   // 256
                {1905, 20, minStakeDiffMainNet}, // 2161
            },
            0,
            true,
            208418769,
        },
        {
            // Next retarget is at 2304.
            "16th retarget interval, 100% demand, final block",
            {
                {256, 0, minStakeDiffMainNet},   // 256
                {2047, 20, minStakeDiffMainNet}, // 2303
            },
            0,
            true,
            208418769,
        },
        {
            // Next retarget is at 3456.
            "24th retarget interval, varying demand, 5th block",
            {
                {256, 0, minStakeDiffMainNet},  // 256
                {31, 20, minStakeDiffMainNet},  // 287
                {144, 10, minStakeDiffMainNet}, // 431
                {144, 20, minStakeDiffMainNet}, // 575
                {144, 10, minStakeDiffMainNet}, // 719
                {144, 20, minStakeDiffMainNet}, // 863
                {144, 10, minStakeDiffMainNet}, // 1007
                {144, 20, minStakeDiffMainNet}, // 1151
                {144, 10, minStakeDiffMainNet}, // 1295
                {144, 20, minStakeDiffMainNet}, // 1439
                {144, 10, minStakeDiffMainNet}, // 1583
                {144, 20, minStakeDiffMainNet}, // 1727
                {144, 10, minStakeDiffMainNet}, // 1871
                {144, 20, minStakeDiffMainNet}, // 2015
                {144, 10, minStakeDiffMainNet}, // 2159
                {144, 20, minStakeDiffMainNet}, // 2303
                {144, 10, minStakeDiffMainNet}, // 2447
                {144, 20, minStakeDiffMainNet}, // 2591
                {144, 10, minStakeDiffMainNet}, // 2735
                {144, 20, minStakeDiffMainNet}, // 2879
                {144, 9, 201743368},            // 3023
                {144, 20, 201093236},           // 3167
                {144, 8, 222625877},            // 3311
                {5, 20, 242331291},             // 3316
            },
            0,
            true,
            291317641,
        },
        // {
        //     // Next retarget is at 4176.  Post stake validation
        //     // height.
        //     "29th retarget interval, 100% demand, 10th block",
        //     {
        //         {256, 0, minStakeDiffMainNet},   // 256
        //         {2047, 20, minStakeDiffMainNet}, // 2303
        //         {144, 20, 208418769},            // 2447
        //         {144, 20, 231326567},            // 2591
        //         {144, 20, 272451490},            // 2735
        //         {144, 20, 339388424},            // 2879
        //         {144, 20, 445827839},            // 3023
        //         {144, 20, 615949254},            // 3167
        //         {144, 20, 892862990},            // 3311
        //         {144, 20, 1354989669},           // 3455
        //         {144, 20, 2148473276},           // 3599
        //         {144, 20, 3552797658},           // 3743
        //         {144, 20, 6116808441},           // 3887
        //         {144, 20, 10947547379},          // 4031
        //         {10, 20, 20338554623},           // 4041
        //     },
        //     0,
        //     true,
        //     22097687698,
        // },
        {
            // Next retarget is at 4176.  Post stake validation
            // height.
            "29th retarget interval, 50% demand, 23rd block",
            {
                {256, 0, minStakeDiffMainNet},   // 256
                {3775, 10, minStakeDiffMainNet}, // 4031
                {23, 10, minStakeDiffMainNet},   // 4054
            },
            1210, // 121 * 10
            false,
            minStakeDiffMainNet,
        },
        // {
        //     // Next retarget is at 4464.  Post stake validation
        //     // height.
        //     "31st retarget interval, waning demand, 117th block",
        //     {
        //         {256, 0, minStakeDiffMainNet},   // 256
        //         {2047, 20, minStakeDiffMainNet}, // 2303
        //         {144, 20, 208418769},            // 2447
        //         {144, 20, 231326567},            // 2591
        //         {144, 20, 272451490},            // 2735
        //         {144, 20, 339388424},            // 2879
        //         {144, 20, 445827839},            // 3023
        //         {144, 20, 615949254},            // 3167
        //         {144, 20, 892862990},            // 3311
        //         {144, 20, 1354989669},           // 3455
        //         {144, 20, 2148473276},           // 3599
        //         {144, 20, 3552797658},           // 3743
        //         {144, 13, 6116808441},           // 3887
        //         {144, 0, 10645659768},           // 4031
        //         {144, 0, 18046712136},           // 4175
        //         {144, 0, 22097687698},           // 4319
        //         {117, 0, 22152524112},           // 4436
        //     },
        //     0,
        //     false,
        //     22207360526,
        // },
    };

    EstimateSDiffLoop(tests, params);
}

struct TestingSetup_TEST : public TestingSetup
{
    explicit TestingSetup_TEST(const std::string& chainName = CBaseChainParams::TESTNET)
    : TestingSetup(chainName)
    {
        ;
    };

    ~TestingSetup_TEST(){};
};

BOOST_FIXTURE_TEST_CASE(estimate_next_stake_diff_TEST, TestingSetup_TEST)
{
    // --------------------------
    // TestNet params start here.
    // --------------------------
    const auto& testNetParams = Params().GetConsensus();
    const auto& minStakeDiffTestNet = testNetParams.nMinimumStakeDiff;
    const auto& tests = std::vector<EstimateSDiffTest>{
        // TODO this returns the nStakeDiff of the genesis block which in our case is 
        //      initialized with zero
        // {
        //     // Regardless of claiming tickets will be purchased, the
        //     // resulting stake difficulty should be the minimum
        //     // because the first retarget is before the start
        //     // height.
        //     "genesis block",
        //     {{0, 0, minStakeDiffTestNet}},
        //     2860,
        //     false,
        //     minStakeDiffTestNet,
        // },
        {
            // Next retarget is at 144.  Regardless of claiming
            // tickets will be purchased, the resulting stake
            // difficulty should be the min because the previous
            // pool size is still 0.
            "at coinbase maturity",
            {{16, 0, minStakeDiffTestNet}},
            0,
            true,
            minStakeDiffTestNet,
        },
        {
            // Next retarget is at 144.  Regardless of actually
            // purchasing tickets and claiming more tickets will be
            // purchased, the resulting stake difficulty should be
            // the min because the previous pool size is still 0.
            "1st retarget interval - 2, 100% demand",
            {
                {16, 0, minStakeDiffTestNet},   // 16
                {126, 20, minStakeDiffTestNet}, // 142
            },
            0,
            true,
            minStakeDiffTestNet,
        },
        {
            // Next retarget is at 288.  Still expect minimum stake
            // difficulty since the raw result would be lower.
            "2nd retarget interval - 1, 30% demand",
            {
                {16, 0, minStakeDiffTestNet},  // 16
                {271, 6, minStakeDiffTestNet}, // 287
            },
            0,
            true,
            minStakeDiffTestNet,
        },
        {
            // Next retarget is at 288.  Still expect minimum stake
            // difficulty since the raw result would be lower.
            //
            // Since the ticket maturity is smaller than the
            // retarget interval, this case ensures some of the
            // nodes being estimated will mature during the
            // interval.
            "2nd retarget interval - 23, 30% demand",
            {
                {16, 0, minStakeDiffTestNet},  // 16
                {249, 6, minStakeDiffTestNet}, // 265
            },
            132, // 22 * 6
            false,
            minStakeDiffTestNet,
        },
        {
            // Next retarget is at 288.  Still expect minimum stake
            // difficulty since the raw result would be lower.
            //
            // None of the nodes being estimated will mature during the
            // interval.
            "2nd retarget interval - 11, 30% demand",
            {
                {16, 0, minStakeDiffTestNet},  // 16
                {261, 6, minStakeDiffTestNet}, // 277
            },
            60, // 10 * 6
            false,
            minStakeDiffTestNet,
        },
        {
            // Next retarget is at 432.
            "3rd retarget interval, 100% demand, 1st block",
            {
                {16, 0, minStakeDiffTestNet},   // 16
                {256, 20, minStakeDiffTestNet}, // 288
            },
            0,
            true,
            44505494,
        },
        {
            // Next retarget is at 432.
            //
            // None of the nodes being estimated will mature during the
            // interval.
            "3rd retarget interval - 11, 100% demand",
            {
                {16, 0, minStakeDiffTestNet},   // 16
                {271, 20, minStakeDiffTestNet}, // 287
                {134, 20, 44505494},            // 421
            },
            0,
            true,
            108661875,
        },
        {
            // Next retarget is at 576.
            "4th retarget interval, 100% demand, 1st block",
            {
                {16, 0, minStakeDiffTestNet},   // 16
                {271, 20, minStakeDiffTestNet}, // 287
                {144, 20, 44505494},            // 431
                {1, 20, 108661875},             // 432
            },
            0,
            true,
            314319918,
        },
        {
            // Next retarget is at 576.
            "4th retarget interval, 100% demand, 2nd block",
            {
                {16, 0, minStakeDiffTestNet},   // 16
                {271, 20, minStakeDiffTestNet}, // 287
                {144, 20, 44505494},            // 431
                {2, 20, 108661875},             // 433
            },
            0,
            true,
            314319918,
        },
        {
            // Next retarget is at 576.
            "4th retarget interval, 100% demand, final block",
            {
                {16, 0, minStakeDiffTestNet},   // 16
                {271, 20, minStakeDiffTestNet}, // 287
                {144, 20, 44505494},            // 431
                {144, 20, 108661875},           // 575
            },
            0,
            true,
            314319918,
        },
        {
            // Next retarget is at 1152.
            "9th retarget interval, varying demand, 137th block",
            {
                {16, 0, minStakeDiffTestNet},   // 16
                {127, 20, minStakeDiffTestNet}, // 143
                {144, 10, minStakeDiffTestNet}, // 287
                {144, 20, 24055097},            // 431
                {144, 10, 54516186},            // 575
                {144, 20, 105335577},           // 719
                {144, 10, 304330579},           // 863
                {144, 20, 772249463},           // 1007
                {76, 10, 2497324513},           // 1083
                {9, 0, 2497324513},             // 1092
                {1, 10, 2497324513},            // 1093
                {8, 0, 2497324513},             // 1101
                {1, 10, 2497324513},            // 1102
                {12, 0, 2497324513},            // 1114
                {1, 10, 2497324513},            // 1115
                {9, 0, 2497324513},             // 1124
                {1, 10, 2497324513},            // 1125
                {8, 0, 2497324513},             // 1133
                {1, 10, 2497324513},            // 1134
                {10, 0, 2497324513},            // 1144
            },
            10,
            false,
            6976183842,
        },
        {
            // Next retarget is at 1440.  The estimated number of
            // tickets are such that they span the ticket maturity
            // floor so that the estimation result is slightly
            // different as compared to what it would be if each
            // remaining node only had 10 ticket purchases.  This is
            // because it results in a different number of maturing
            // tickets depending on how they are allocated on each
            // side of the maturity floor.
            "11th retarget interval, 50% demand, 127th block",
            {
                {16, 0, minStakeDiffTestNet},   // 16
                {271, 10, minStakeDiffTestNet}, // 287
                {144, 10, 22252747},            // 431
                {144, 10, 27165468},            // 575
                {144, 10, 39289988},            // 719
                {144, 10, 66729608},            // 863
                {144, 10, 116554208},           // 1007
                {144, 10, 212709675},           // 1151
                {144, 10, 417424410},           // 1295
                {127, 10, 876591473},           // 1422
            },
            170, // 17 * 10
            false,
            1965171141,
        },
        {
            // Next retarget is at 1440.  This is similar to the
            // last test except all of the estimated tickets are
            // after the ticket maturity floor, so the estimate is
            // the same as if each remaining node only had 10 ticket
            // purchases.
            "11th retarget interval, 50% demand, 128th block",
            {
                {16, 0, minStakeDiffTestNet},   // 16
                {271, 10, minStakeDiffTestNet}, // 287
                {144, 10, 22252747},            // 431
                {144, 10, 27165468},            // 575
                {144, 10, 39289988},            // 719
                {144, 10, 66729608},            // 863
                {144, 10, 116554208},           // 1007
                {144, 10, 212709675},           // 1151
                {144, 10, 417424410},           // 1295
                {128, 10, 876591473},           // 1423
            },
            160, // 16 * 10
            false,
            1961558695,
        },
    };

    EstimateSDiffLoop(tests, testNetParams);

}

BOOST_AUTO_TEST_SUITE_END()
