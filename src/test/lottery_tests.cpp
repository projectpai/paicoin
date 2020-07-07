// Copyright (c) 2013-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test/test_paicoin.h"
#include <boost/test/unit_test.hpp>
#include "uint256.h"
#include "stake/hash256prng.h"
#include "stake/treap/tickettreap.h"

BOOST_FIXTURE_TEST_SUITE(lottery_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(basic_prng)
{
    std::string strMsg = "Very deterministic message";
    uint256 hashMsg = Hash(strMsg.begin(), strMsg.end());
    auto prng = Hash256PRNG(hashMsg);

    for (int i = 0; i < 10000;++i)
        prng.Hash256Rand();
    
    auto lastHash = prng.StateHash();

    auto lastHashExpected = uint256S("c2caca97080f1eb3b9de9c755587c81813c40865c11a24de2fd13991fc39e479");
    BOOST_CHECK(lastHash == lastHashExpected);
}

BOOST_AUTO_TEST_CASE(lottery_num_selection)
{
    static const char a_seed[] = {0x01};
    uint256 seed = Hash(std::begin(a_seed), std::end(a_seed));
    auto prng = Hash256PRNG(seed);
    auto ticketsInPool = 56789;
    auto tooFewTickets = 4;
    auto justEnoughTickets = 5;
    auto ticketsPerBlock = uint16_t(5);

    BOOST_CHECK_THROW(
        prng.FindTicketIdxs(tooFewTickets,ticketsPerBlock),
        std::runtime_error);

    { 
        auto tickets = prng.FindTicketIdxs(ticketsInPool, ticketsPerBlock);
        decltype(tickets) expectedTickets;
        expectedTickets.push_back(51344u);
        expectedTickets.push_back(39045u);
        expectedTickets.push_back(10107u);
        expectedTickets.push_back(31489u);
        expectedTickets.push_back(44461u);
        BOOST_CHECK(expectedTickets == tickets);
    }

    {
        auto tickets = prng.FindTicketIdxs(justEnoughTickets, ticketsPerBlock);
        decltype(tickets) expectedTickets;
        expectedTickets.push_back(2u);
        expectedTickets.push_back(4u);
        expectedTickets.push_back(3u);
        expectedTickets.push_back(0u);
        expectedTickets.push_back(1u);
        BOOST_CHECK(expectedTickets == tickets);
    }

    auto lastHash = prng.StateHash();
    auto lastHashExpected = uint256S("7b49de6cd86694985718b6ba5f815a2fdab7630f4eb1b75560a9b1589adcb41a");
    BOOST_CHECK(lastHash == lastHashExpected);
}

BOOST_AUTO_TEST_CASE(fetch_winners_errors)
{
    auto ticketTreap = TicketTreap();
    for (int i = 0; i < 0xff; ++i) {
        const char a_seed[] = {char(i)};
        uint256 hash = Hash(std::begin(a_seed), std::end(a_seed));
        auto value = Value(
            uint32_t(i),
            (i%2 == 0),
            (i%2 != 0),
            (i%2 == 0),
            (i%2 != 0)
        );
        ticketTreap = ticketTreap.put(hash, value);
    }

    // Bad index too big.
    prevector<64,uint32_t> idxs;
    idxs.push_back(1u);
    idxs.push_back(2u);
    idxs.push_back(3u);
    idxs.push_back(4u);
    idxs.push_back(256u);
    BOOST_CHECK_THROW(
        ticketTreap.fetchWinners(idxs),
         std::runtime_error);
}

BOOST_AUTO_TEST_SUITE_END()
