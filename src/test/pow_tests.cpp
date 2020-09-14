// Copyright (c) 2015 The Bitcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chain.h"
#include "chainparams.h"
#include "pow.h"
#include "random.h"
#include "util.h"
#include "test/test_paicoin.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(pow_tests, BasicTestingSetup)

/* Test calculation of next difficulty target with no constraints applying */
BOOST_AUTO_TEST_CASE(get_next_work)
{
    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    int64_t nLastRetargetTime = 1261130161; // Block #30240
    CBlockIndex pindexLast;
    pindexLast.nHeight = 32255;
    pindexLast.nTime = 1262152739;  // Block #32255
    pindexLast.nBits = 0x1d00ffff;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), 0x1c09fe61);
}

/**
 * Note: if unit test updating is required
 * Methodology:
 * - update the nLastRetargetTime to the genesis block time;
 * - update the pindexLast.nTime to a date at 2055491 seconds from nLastRetargetTime;
 * - update the pindexLast.nBits to the value of genesis block nBits;
 * - update the desired output of the call of CalculateNextWorkRequired to the value of genesis block nBits;
 */
/* Test the constraint on the upper bound for next work */
BOOST_AUTO_TEST_CASE(get_next_work_pow_limit)
{
    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    int64_t nLastRetargetTime = 1509780574; // Block #0
    CBlockIndex pindexLast;
    pindexLast.nHeight = 2015;
    pindexLast.nTime = 1511836065;  // Block #2015
    pindexLast.nBits = 0x1d00ffff;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), 0x1c09fe61);
}

/* Test the constraint on the lower bound for actual time taken */
BOOST_AUTO_TEST_CASE(get_next_work_lower_limit_actual)
{
    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    int64_t nLastRetargetTime = 1279008237; // Block #66528
    CBlockIndex pindexLast;
    pindexLast.nHeight = 68543;
    pindexLast.nTime = 1279297671;  // Block #68543
    pindexLast.nBits = 0x1c05a3f4;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), 0x1c0168fd);
}

/* Test the constraint on the upper bound for actual time taken */
BOOST_AUTO_TEST_CASE(get_next_work_upper_limit_actual)
{
    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    int64_t nLastRetargetTime = 1263163443; // NOTE: Not an actual block time
    CBlockIndex pindexLast;
    pindexLast.nHeight = 46367;
    pindexLast.nTime = 1269211443;  // Block #46367
    pindexLast.nBits = 0x1c387f6f;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), 0x1c09fe61);
}

BOOST_AUTO_TEST_CASE(GetBlockProofEquivalentTime_test)
{
    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    std::vector<CBlockIndex> blocks(10000);
    for (int i = 0; i < 10000; i++) {
        blocks[i].pprev = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight = i;
        blocks[i].nTime = 1269211443 + i * chainParams->GetConsensus().nPowTargetSpacing;
        blocks[i].nBits = 0x207fffff; /* target 0x7fffff000... */
        blocks[i].nChainWork = i ? blocks[i - 1].nChainWork + GetBlockProof(blocks[i - 1]) : arith_uint256(0);
    }

    for (int j = 0; j < 1000; j++) {
        CBlockIndex *p1 = &blocks[InsecureRandRange(10000)];
        CBlockIndex *p2 = &blocks[InsecureRandRange(10000)];
        CBlockIndex *p3 = &blocks[InsecureRandRange(10000)];

        int64_t tdiff = GetBlockProofEquivalentTime(*p1, *p2, *p3, chainParams->GetConsensus());
        BOOST_CHECK_EQUAL(tdiff, p1->GetBlockTime() - p2->GetBlockTime());
    }
}

static CBlockIndex GetBlockIndex(CBlockIndex *pindexPrev, int64_t nTimeInterval,
                                 uint32_t nBits) {
    CBlockIndex block;
    block.pprev = pindexPrev;
    block.nHeight = pindexPrev->nHeight + 1;
    block.nTime = pindexPrev->nTime + nTimeInterval;
    block.nBits = nBits;

    block.nChainWork = pindexPrev->nChainWork + GetBlockProof(block);
    return block;
}

BOOST_AUTO_TEST_CASE(cash_difficulty_test)
{
    // TODO: update this test to use the hybridConsensusPowLimit when needed

    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);

    std::vector<CBlockIndex> blocks(3000);

    const Consensus::Params &params = chainParams->GetConsensus();
    const arith_uint256 powLimit = UintToArith256(params.powLimit);
    uint32_t powLimitBits = powLimit.GetCompact();
    arith_uint256 currentPow = powLimit >> 4;
    uint32_t initialBits = currentPow.GetCompact();

    // Genesis block.
    blocks[0] = CBlockIndex();
    blocks[0].nHeight = 0;
    blocks[0].nTime = 1269211443;
    blocks[0].nBits = initialBits;

    blocks[0].nChainWork = GetBlockProof(blocks[0]);

    // Block counter.
    size_t i;

    // Pile up some blocks every 10 mins to establish some history.
    for (i = 1; i < 2050; i++) {
        blocks[i] = GetBlockIndex(&blocks[i - 1], 600, initialBits);
    }

    CBlockHeader blkHeaderDummy;
    uint32_t nBits =
        GetNextCashWorkRequired(&blocks[2049], &blkHeaderDummy, params);

    // Difficulty stays the same as long as we produce a block every 10 mins.
    for (size_t j = 0; j < 10; i++, j++) {
        blocks[i] = GetBlockIndex(&blocks[i - 1], 600, nBits);
        BOOST_CHECK_EQUAL(
            GetNextCashWorkRequired(&blocks[i], &blkHeaderDummy, params),
            nBits);
    }

    // Make sure we skip over blocks that are out of wack. To do so, we produce
    // a block that is far in the future, and then produce a block with the
    // expected timestamp.
    blocks[i] = GetBlockIndex(&blocks[i - 1], 6000, nBits);
    BOOST_CHECK_EQUAL(
        GetNextCashWorkRequired(&blocks[i++], &blkHeaderDummy, params), nBits);
    blocks[i] = GetBlockIndex(&blocks[i - 1], 2 * 600 - 6000, nBits);
    BOOST_CHECK_EQUAL(
        GetNextCashWorkRequired(&blocks[i++], &blkHeaderDummy, params), nBits);

    // The system should continue unaffected by the block with a bogous
    // timestamps.
    for (size_t j = 0; j < 20; i++, j++) {
        blocks[i] = GetBlockIndex(&blocks[i - 1], 600, nBits);
        BOOST_CHECK_EQUAL(
            GetNextCashWorkRequired(&blocks[i], &blkHeaderDummy, params),
            nBits);
    }

    // We start emitting blocks slightly faster. The first block has no impact.
    blocks[i] = GetBlockIndex(&blocks[i - 1], 550, nBits);
    BOOST_CHECK_EQUAL(
        GetNextCashWorkRequired(&blocks[i++], &blkHeaderDummy, params), nBits);

    // Now we should see difficulty increase slowly.
    for (size_t j = 0; j < 10; i++, j++) {
        blocks[i] = GetBlockIndex(&blocks[i - 1], 550, nBits);
        const uint32_t nextBits =
            GetNextCashWorkRequired(&blocks[i], &blkHeaderDummy, params);

        arith_uint256 currentTarget;
        currentTarget.SetCompact(nBits);
        arith_uint256 nextTarget;
        nextTarget.SetCompact(nextBits);

        // Make sure that difficulty increases very slowly.
        BOOST_CHECK(nextTarget < currentTarget);
        BOOST_CHECK((currentTarget - nextTarget) < (currentTarget >> 10));

        nBits = nextBits;
    }

    // Check the actual value.
    BOOST_CHECK_EQUAL(nBits, 0x1c009ef3);// changed from 0x1c0fe7b1 due to different powLimit setting

    // If we dramatically shorten block production, difficulty increases faster.
    for (size_t j = 0; j < 20; i++, j++) {
        blocks[i] = GetBlockIndex(&blocks[i - 1], 10, nBits);
        const uint32_t nextBits =
            GetNextCashWorkRequired(&blocks[i], &blkHeaderDummy, params);

        arith_uint256 currentTarget;
        currentTarget.SetCompact(nBits);
        arith_uint256 nextTarget;
        nextTarget.SetCompact(nextBits);

        // Make sure that difficulty increases faster.
        BOOST_CHECK(nextTarget < currentTarget);
        BOOST_CHECK((currentTarget - nextTarget) < (currentTarget >> 4));

        nBits = nextBits;
    }

    // Check the actual value.
    BOOST_CHECK_EQUAL(nBits, 0x1c0088d9);// changed from 0x1c0db19f due to different powLimit setting

    // We start to emit blocks significantly slower. The first block has no
    // impact.
    blocks[i] = GetBlockIndex(&blocks[i - 1], 6000, nBits);
    nBits = GetNextCashWorkRequired(&blocks[i++], &blkHeaderDummy, params);

    // Check the actual value.
    BOOST_CHECK_EQUAL(nBits, 0x1c00879f); // changed from 0x1c0d9222 due to different powLimit setting

    // If we dramatically slow down block production, difficulty decreases.
    for (size_t j = 0; j < 93; i++, j++) {
        blocks[i] = GetBlockIndex(&blocks[i - 1], 6000, nBits);
        const uint32_t nextBits =
            GetNextCashWorkRequired(&blocks[i], &blkHeaderDummy, params);

        arith_uint256 currentTarget;
        currentTarget.SetCompact(nBits);
        arith_uint256 nextTarget;
        nextTarget.SetCompact(nextBits);

        // Check the difficulty decreases.
        BOOST_CHECK(nextTarget <= powLimit);
        BOOST_CHECK(nextTarget > currentTarget);
        BOOST_CHECK((nextTarget - currentTarget) < (currentTarget >> 3));

        nBits = nextBits;
    }

    // Check the actual value.
    BOOST_CHECK_EQUAL(nBits, 0x1c01d677); // changed from 0x1c2f13b9 due to different powLimit setting

    // Due to the window of time being bounded, next block's difficulty actually
    // gets harder.
    blocks[i] = GetBlockIndex(&blocks[i - 1], 6000, nBits);
    nBits = GetNextCashWorkRequired(&blocks[i++], &blkHeaderDummy, params);
    BOOST_CHECK_EQUAL(nBits, 0x1c01d4d4); // changed from 0x1c2ee9bf due to different powLimit setting

    // And goes down again. It takes a while due to the window being bounded and
    // the skewed block causes 2 blocks to get out of the window.
    for (size_t j = 0; j < 192; i++, j++) {
        blocks[i] = GetBlockIndex(&blocks[i - 1], 6000, nBits);
        const uint32_t nextBits =
            GetNextCashWorkRequired(&blocks[i], &blkHeaderDummy, params);

        arith_uint256 currentTarget;
        currentTarget.SetCompact(nBits);
        arith_uint256 nextTarget;
        nextTarget.SetCompact(nextBits);

        // Check the difficulty decreases.
        BOOST_CHECK(nextTarget <= powLimit);
        BOOST_CHECK(nextTarget > currentTarget);
        BOOST_CHECK((nextTarget - currentTarget) < (currentTarget >> 3));

        nBits = nextBits;
    }

    // Check the actual value.
    BOOST_CHECK_EQUAL(nBits, 0x1c09fe61); // changed from 0x1d00ffff due to different powLimit setting

    // Once the difficulty reached the minimum allowed level, it doesn't get any
    // easier.
    for (size_t j = 0; j < 5; i++, j++) {
        blocks[i] = GetBlockIndex(&blocks[i - 1], 6000, nBits);
        const uint32_t nextBits =
            GetNextCashWorkRequired(&blocks[i], &blkHeaderDummy, params);

        // Check the difficulty stays constant.
        BOOST_CHECK_EQUAL(nextBits, powLimitBits);
        nBits = nextBits;
    }
}

BOOST_AUTO_TEST_SUITE_END()
