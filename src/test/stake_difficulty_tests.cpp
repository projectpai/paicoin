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

struct SDiffTest
{
    std::string name;
    std::vector<TicketInfo> vTicketInfo;
    int64_t expectedDiff;
};

BOOST_FIXTURE_TEST_SUITE(stake_difficulty_tests, TestingSetup)

// get_next_required_stake_diff ensure the stake diff calculation function works as expected.
BOOST_AUTO_TEST_CASE(get_next_required_stake_diff)
{
    // const auto& my_tip = chainActive.Tip();
    const CChainParams& chainparams = Params();
    // const auto& stakeEnabledHeight = chainparams.GetConsensus().nStakeEnabledHeight;
    const auto& stakeValidationHeight = chainparams.GetConsensus().nStakeValidationHeight;
    const auto& minStakeDiff = chainparams.GetConsensus().nMinimumStakeDiff /*+ 2e4*/;
    const auto& ticketMaturity = chainparams.GetConsensus().nTicketMaturity;
    // const auto& ticketExpiry = chainparams.GetConsensus().nTicketExpiry;
    const auto& ticketsPerBlock = chainparams.GetConsensus().nTicketsPerBlock;
    // const auto& maxFreshTicketsPerBlock = chainparams.GetConsensus().nMaxFreshStakePerBlock;

    const auto& tests = std::vector<SDiffTest>{
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

    for (const auto& test : tests) {
        auto fakeChain = chainActive;

        auto poolSize = uint32_t{0};

        // immatureTickets tracks which height the purchased tickets
        // will mature and thus be eligible for admission to the live
        // ticket pool.
        auto immatureTickets = std::map<uint32_t,uint8_t>{};

        for (const auto& ticketInfo: test.vTicketInfo) 
        {
            auto tip = fakeChain.Tip();
            const auto& stake_diff = CalculateNextRequiredStakeDifficulty(tip, chainparams.GetConsensus());
            BOOST_CHECK_EQUAL(stake_diff, ticketInfo.stakeDiff);

            for (uint32_t i = 0; i < ticketInfo.numNodes; ++i) 
            {
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

        const auto& stake_diff = CalculateNextRequiredStakeDifficulty(fakeChain.Tip(), chainparams.GetConsensus());
        BOOST_CHECK_EQUAL(stake_diff, test.expectedDiff);
    }
}


BOOST_AUTO_TEST_SUITE_END()
