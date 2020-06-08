// Copyright (c) 2014-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "validation.h"
#include "consensus/tx_verify.h"
#include "net.h"

#include "test/test_paicoin.h"

#include <boost/signals2/signal.hpp>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(main_tests, TestingSetup)

static void TestBlockSubsidyHalvings(const Consensus::Params& consensusParams)
{
    int maxHalvings = 64;
    // PAICOIN Note: If the initial block subsidy has been changed,
    // update the subsidy with the correct value
    CAmount nInitialSubsidy = consensusParams.nTotalBlockSubsidy * COIN;

    CAmount nPreviousSubsidy = nInitialSubsidy * 2; // for height == 0
    BOOST_CHECK_EQUAL(nPreviousSubsidy, nInitialSubsidy * 2);
    for (int nHalvings = 0; nHalvings < maxHalvings; nHalvings++) {
        int nHeight = nHalvings * consensusParams.nSubsidyHalvingInterval;
        CAmount nSubsidy = GetTotalBlockSubsidy(nHeight, consensusParams);
        BOOST_CHECK(nSubsidy <= nInitialSubsidy);
        BOOST_CHECK_EQUAL(nSubsidy, nPreviousSubsidy / 2);
        nPreviousSubsidy = nSubsidy;
    }
    BOOST_CHECK_EQUAL(GetTotalBlockSubsidy(maxHalvings * consensusParams.nSubsidyHalvingInterval, consensusParams), 0);
}

static void TestBlockSubsidyHalvings(int nSubsidyHalvingInterval)
{
    Consensus::Params consensusParams;
    consensusParams.nSubsidyHalvingInterval = nSubsidyHalvingInterval;
    TestBlockSubsidyHalvings(consensusParams);
}

BOOST_AUTO_TEST_CASE(block_subsidy_test)
{
    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);

    TestBlockSubsidyHalvings(chainParams->GetConsensus()); // As in main
    TestBlockSubsidyHalvings(150); // As in regtest
    TestBlockSubsidyHalvings(1000); // Just another interval

    CAmount contributedAmount = 20;
    CAmount stakedAmount = 1000;
    CAmount subsidy = 50;
    CAmount contributionSum = 1000;
    CAmount reward = CalculateGrossRemuneration(contributedAmount, stakedAmount, subsidy, contributionSum);
    BOOST_CHECK(reward == 21);

    auto nStakeValidationHeight = chainParams->GetConsensus().nStakeValidationHeight;
    CAmount minerSubsidy = GetMinerSubsidy(nStakeValidationHeight-1, chainParams->GetConsensus());
    BOOST_CHECK(minerSubsidy == 1500 * COIN);
    minerSubsidy = GetMinerSubsidy(nStakeValidationHeight, chainParams->GetConsensus());
    BOOST_CHECK(minerSubsidy == 1050 * COIN);

    CAmount voterSubsidy = GetVoterSubsidy(nStakeValidationHeight-1, chainParams->GetConsensus());
    BOOST_CHECK(voterSubsidy == 0);
    voterSubsidy = GetVoterSubsidy(nStakeValidationHeight, chainParams->GetConsensus());
    BOOST_CHECK(voterSubsidy == 90 * COIN);
}

BOOST_AUTO_TEST_CASE(subsidy_limit_test)
{
    const auto chainParams = CreateChainParams(CBaseChainParams::MAIN);
    CAmount nSum = 0;
    for (int nHeight = 0; nHeight < 14000000; nHeight += 1000) {
        CAmount nSubsidy = GetTotalBlockSubsidy(nHeight, chainParams->GetConsensus());
        // PAICOIN Note: If the initial block subsidy has been changed,
        // update the subsidy with the correct value
        BOOST_CHECK(nSubsidy <= chainParams->GetConsensus().nTotalBlockSubsidy * COIN);
        nSum += nSubsidy * 1000;
        BOOST_CHECK(MoneyRange(nSum));
    }
    // PAICOIN Note: If the initial block subsidy has been changed,
    // update this sum with the correct value
    BOOST_CHECK_EQUAL(nSum, 62999999996850000ULL);
}

bool ReturnFalse() { return false; }
bool ReturnTrue() { return true; }

BOOST_AUTO_TEST_CASE(test_combiner_all)
{
    boost::signals2::signal<bool (), CombinerAll> Test;
    BOOST_CHECK(Test());
    Test.connect(&ReturnFalse);
    BOOST_CHECK(!Test());
    Test.connect(&ReturnTrue);
    BOOST_CHECK(!Test());
    Test.disconnect(&ReturnFalse);
    BOOST_CHECK(Test());
    Test.disconnect(&ReturnTrue);
    BOOST_CHECK(Test());
}
BOOST_AUTO_TEST_SUITE_END()
