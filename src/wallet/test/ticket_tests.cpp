// Copyright (c) 2020 The Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/test/wallet_stake_test_fixture.h"

#include <boost/test/unit_test.hpp>

#include "stake/stakepoolfee.h"
#include "validation.h"
#include "policy/policy.h"
#include "wallet/coincontrol.h"
#include "net.h"
#include "rpc/server.h"
#include "miner.h"
#include "consensus/tx_verify.h"

BOOST_FIXTURE_TEST_SUITE(ticket_tests, WalletStakeTestingSetup)

// test correctness of stake pool fee percent validation
BOOST_AUTO_TEST_CASE(stake_pool_fee_percent_test)
{
    // tests with values as doubles

    struct TestDataDouble {
        double percent;
        bool expected;
    };

    std::vector<TestDataDouble> doubleTests {
        {0.01, true},
        {0.011, true},
        {0.011, true},
        {.05, true},
        {0.1, true},
        {0.1234567890, true},
        {1.0, true},
        {10.0, true},
        {25.0, true},
        {25.78, true},
        {50.0, true},
        {99.0, true},
        {99.99, true},
        {100.0, true},
        {0.0, false},
        {100.000001, false},
        {1000.0, false}
    };

    for (auto& test: doubleTests)
        BOOST_CHECK_EQUAL(IsValidPoolFeePercent(test.percent), test.expected);

    // tests with values as integers

    struct TestDataInteger {
        int percent;
        bool expected;
    };

    std::vector<TestDataInteger> integerTests {
        {1, true},
        {2, true},
        {10, true},
        {50, true},
        {100, true},
        {0, false},
        {101, false},
        {1000, false}
    };

    for (auto& test: integerTests)
        BOOST_CHECK_EQUAL(IsValidPoolFeePercent(test.percent), test.expected);
}

// test stake pool ticket fee calculation
BOOST_AUTO_TEST_CASE(stake_pool_ticket_fee_test)
{
    struct TestData {
        CAmount stakeDiff;
        CAmount fee;
        int height;
        double poolFee;
        CAmount expected;
    };

    // test with good data

    // TODO: Update these if still failing after finalizing all the implementations in the voter subsidy
    std::vector<TestData> goodTests {
        {10 * COIN, static_cast<CAmount>(0.01 * COIN), 25000, 1.00, static_cast<CAmount>(0.01500463 * COIN)},
        {20 * COIN, static_cast<CAmount>(0.01 * COIN), 25000, 1.00, static_cast<CAmount>(0.01621221 * COIN)},
        { 5 * COIN, static_cast<CAmount>(0.05 * COIN), 50000, 2.59, static_cast<CAmount>(0.03310616 * COIN)},
        {15 * COIN, static_cast<CAmount>(0.05 * COIN), 50000, 2.59, static_cast<CAmount>(0.03956376 * COIN)}
    };

    //for (auto& test: goodTests)
    //    BOOST_CHECK_EQUAL(StakePoolTicketFee(test.stakeDiff, test.fee, test.height, test.poolFee), test.expected);

    // test with bad data

    std::vector<TestData> badTests {
        {10 * COIN, static_cast<CAmount>(0.01 * COIN), 25000, 1.00, static_cast<CAmount>(0.12345678 * COIN)},
        {20 * COIN, static_cast<CAmount>(0.01 * COIN), 25000, 1.00, static_cast<CAmount>(0.01621222 * COIN)},
        { 5 * COIN, static_cast<CAmount>(0.05 * COIN), 50000, 2.59, static_cast<CAmount>(0.03313616 * COIN)},
        {15 * COIN, static_cast<CAmount>(0.05 * COIN), 50000, 2.59, static_cast<CAmount>(0.04956376 * COIN)},
        {11 * COIN, static_cast<CAmount>(0.01 * COIN), 25000, 1.00, static_cast<CAmount>(0.01500463 * COIN)},
        {20 * COIN, static_cast<CAmount>(0.02 * COIN), 25000, 1.00, static_cast<CAmount>(0.01621221 * COIN)},
        { 5 * COIN, static_cast<CAmount>(0.05 * COIN), 51000, 2.59, static_cast<CAmount>(0.03310616 * COIN)},
        {15 * COIN, static_cast<CAmount>(0.05 * COIN), 50000, 2.71, static_cast<CAmount>(0.03956376 * COIN)}
    };

    //for (auto& test: badTests)
    //    BOOST_CHECK(StakePoolTicketFee(test.stakeDiff, test.fee, test.height, test.poolFee) != test.expected);
}

// test fee limits for ticket (vote or revoke)
BOOST_FIXTURE_TEST_CASE(ticket_vote_or_revoke_fee_limits, WalletStakeTestingSetup)
{
    struct TestData {
        CAmount voteFeeLimit;
        CAmount revocationFeeLimit;

        CAmount expectedVoteFeeLimit;
        CAmount expectedRevocationFeeLimit;
    };

    std::vector<TestData> tests {
        {                    0,                    0,         0,          0},
        {                    1,                    1,         1,          1},
        {                    2,                    2,         2,          2},
        {                   10,                   11,        10,         11},
        {                 1000,                 1001,      1000,       1001},
        {              10*COIN,               5*COIN,   10*COIN,     5*COIN},
        {            1000*COIN,            2000*COIN, 1000*COIN,  2000*COIN},
        {            MAX_MONEY,            MAX_MONEY, MAX_MONEY,  MAX_MONEY},
        // values below should be capped at valid money amounts
        {                   -1,                   -1,         0,          0},
        {                -1000,                -1000,         0,          0},
        {             -10*COIN,             -10*COIN,         0,          0},
        {           -1000*COIN,           -1000*COIN,         0,          0},
        {           -MAX_MONEY,           -MAX_MONEY,         0,          0},
        { -MAX_MONEY-1000*COIN, -MAX_MONEY-1000*COIN,         0,          0},
        {          MAX_MONEY+1,          MAX_MONEY+1, MAX_MONEY,  MAX_MONEY},
        {  MAX_MONEY+1000*COIN,  MAX_MONEY+2000*COIN, MAX_MONEY,  MAX_MONEY}
    };

    // default fee

    BOOST_CHECK_EQUAL(TicketContribData::DefaultFeeLimit, 1LL<<20);

    // fee limit accessors

    for (auto& test: tests) {
        // constructor

        TicketContribData tcd1{1, CKeyID(), 0LL, test.voteFeeLimit, test.revocationFeeLimit};

        BOOST_CHECK_EQUAL(tcd1.voteFeeLimit(), test.expectedVoteFeeLimit);
        BOOST_CHECK_EQUAL(tcd1.revocationFeeLimit(), test.expectedRevocationFeeLimit);

        // setters

        TicketContribData tcd2;

        tcd2.setVoteFeeLimit(test.voteFeeLimit);
        BOOST_CHECK_EQUAL(tcd2.voteFeeLimit(), test.expectedVoteFeeLimit);

        tcd2.setRevocationFeeLimit(test.revocationFeeLimit);
        BOOST_CHECK_EQUAL(tcd2.revocationFeeLimit(), test.expectedRevocationFeeLimit);
    }

    // input validation

    LOCK2(cs_main, wallet->cs_wallet);

    BOOST_CHECK(GetSomeCoins());

    CPubKey pubKey;
    BOOST_CHECK(wallet->GetKeyFromPool(pubKey));
    const CTxDestination addr = pubKey.GetID();

    const CAmount& ticketPrice = CalculateNextRequiredStakeDifficulty(chainActive.Tip(), consensus);
    BOOST_CHECK_GT(ticketPrice, 0LL);

    uint256 splitTxtHash = SendMoney({std::make_pair(addr, ticketPrice)});
    BOOST_CHECK(splitTxtHash != emptyData.hash);

    CCoinsView viewDummy;
    CCoinsViewCache view(&viewDummy);
    CCoinsViewMemPool viewMempool(pcoinsTip, mempool);
    view.SetBackend(viewMempool);

    CMutableTransaction mtx;

    mtx.vin.push_back(CTxIn(splitTxtHash, 0));

    mtx.vout.push_back(CTxOut(0, GetScriptForBuyTicketDecl(BuyTicketData{1})));
    mtx.vout.push_back(CTxOut(ticketPrice, GetScriptForDestination(addr)));
    mtx.vout.push_back(CTxOut(0, GetScriptForTicketContrib(TicketContribData(1, addr, ticketPrice, 0, TicketContribData::DefaultFeeLimit))));
    mtx.vout.push_back(CTxOut(0, GetScriptForDestination(addr)));

    CAmount nFees{0};
    CValidationState state;

    BOOST_CHECK(Consensus::CheckTxInputs(mtx, state, view, GetSpendHeight(view), nFees, params));

    TicketContribData contrib;
    ParseTicketContrib(mtx, ticketContribOutputIndex, contrib);

    if (consensus.nMinimumTotalVoteFeeLimit > 0) {
        contrib.setVoteFeeLimit(consensus.nMinimumTotalVoteFeeLimit-1);
        mtx.vout[ticketContribOutputIndex].scriptPubKey = GetScriptForTicketContrib(contrib);
        BOOST_CHECK(!Consensus::CheckTxInputs(mtx, state, view, GetSpendHeight(view), nFees, params) && state.GetRejectReason() == "total-vote-fee-limit-too-low");
    }

    if (consensus.nMinimumTotalRevocationFeeLimit > 0) {
        contrib.setVoteFeeLimit(consensus.nMinimumTotalVoteFeeLimit);
        contrib.setRevocationFeeLimit(consensus.nMinimumTotalRevocationFeeLimit-1);
        mtx.vout[ticketContribOutputIndex].scriptPubKey = GetScriptForTicketContrib(contrib);
        BOOST_CHECK(!Consensus::CheckTxInputs(mtx, state, view, GetSpendHeight(view), nFees, params) && state.GetRejectReason() == "total-revocation-fee-limit-too-low");
    }

    view.SetBackend(viewDummy);
}

// test the serialization and deserialization of the ticket contribution data
BOOST_AUTO_TEST_CASE(ticket_contrib_data_serialization)
{
    struct TestData {
        TicketContribData tcd;

        CScript expectedScript;
        TicketContribData expectedTcd;
    };

    CPubKey pubKey;
    BOOST_CHECK(wallet->GetKeyFromPool(pubKey));
    const CTxDestination addr = pubKey.GetID();

    auto scriptBuilder = [](TicketContribData tcd) -> CScript {
        return CScript() << OP_RETURN << OP_STRUCT << 1 << CLASS_Staking << STAKE_TicketContribution
                         << tcd.nVersion
                         << ToByteVector(tcd.rewardAddr) << tcd.whichAddr
                         << tcd.contributedAmount
                         << tcd.voteFeeLimit()
                         << tcd.revocationFeeLimit();
    };

    std::vector<TestData> tests {
        {
            TicketContribData(1, addr, 5*COIN, 10, 0),
            scriptBuilder(TicketContribData(1, addr, 5*COIN, 10, 0)),
            TicketContribData(1, addr, 5*COIN, 10, 0)
        },
        {
            TicketContribData(2, addr, 10*COIN, 0, 10),
            scriptBuilder(TicketContribData(2, addr, 10*COIN, 0, 10)),
            TicketContribData(2, addr, 10*COIN, 0, 10)
        },
        {
            TicketContribData(3, addr, 20*COIN, 20, 0),
            scriptBuilder(TicketContribData(3, addr, 20*COIN, 20, 0)),
            TicketContribData(3, addr, 20*COIN, 20, 0)
        },
        {
            TicketContribData(4, addr, 30*COIN, 0, 30),
            scriptBuilder(TicketContribData(4, addr, 30*COIN, 0, 30)),
            TicketContribData(4, addr, 30*COIN, 0, 30)
        },
        {
            TicketContribData(5, addr, 40*COIN, 15, 25),
            scriptBuilder(TicketContribData(5, addr, 40*COIN, 15, 25)),
            TicketContribData(5, addr, 40*COIN, 15, 25)
        },
        {
            TicketContribData(6, addr, 100*COIN, 40, 30),
            scriptBuilder(TicketContribData(6, addr, 100*COIN, 40, 30)),
            TicketContribData(6, addr, 100*COIN, 40, 30)
        },
        {
            TicketContribData(4, addr, 30*COIN, 0, TicketContribData::DefaultFeeLimit),
            scriptBuilder(TicketContribData(4, addr, 30*COIN, 0, TicketContribData::DefaultFeeLimit)),
            TicketContribData(4, addr, 30*COIN, 0, TicketContribData::DefaultFeeLimit)
        },
        {
            TicketContribData(4, addr, 30*COIN, TicketContribData::DefaultFeeLimit, 0),
            scriptBuilder(TicketContribData(4, addr, 30*COIN, TicketContribData::DefaultFeeLimit, 0)),
            TicketContribData(4, addr, 30*COIN, TicketContribData::DefaultFeeLimit, 0)
        },
        {
            TicketContribData(5, addr, 40*COIN, 1000*COIN, 25),
            scriptBuilder(TicketContribData(5, addr, 40*COIN, 1000*COIN, 25)),
            TicketContribData(5, addr, 40*COIN, 1000*COIN, 25)
        },
        {
            TicketContribData(6, addr, 100*COIN, 40, 1000*COIN),
            scriptBuilder(TicketContribData(6, addr, 100*COIN, 40, 1000*COIN)),
            TicketContribData(6, addr, 100*COIN, 40, 1000*COIN)
        },
        {
            TicketContribData(5, addr, 40*COIN, 10000000*COIN, 25),
            scriptBuilder(TicketContribData(5, addr, 40*COIN, 10000000*COIN, 25)),
            TicketContribData(5, addr, 40*COIN, 10000000*COIN, 25)
        },
        {
            TicketContribData(6, addr, 100*COIN, 40, 10000000*COIN),
            scriptBuilder(TicketContribData(6, addr, 100*COIN, 40, 10000000*COIN)),
            TicketContribData(6, addr, 100*COIN, 40, 10000000*COIN)
        },
        {
            TicketContribData(5, addr, 40*COIN, 10000000*COIN, 25),
            scriptBuilder(TicketContribData(5, addr, 40*COIN, 10000000*COIN, 25)),
            TicketContribData(5, addr, 40*COIN, 10000000*COIN, 25)
        },
        {
            TicketContribData(6, addr, 100*COIN, 40, 10000000*COIN),
            scriptBuilder(TicketContribData(6, addr, 100*COIN, 40, 10000000*COIN)),
            TicketContribData(6, addr, 100*COIN, 40, 10000000*COIN)
        },
        {
            TicketContribData(5, addr, 40*COIN, -1, MAX_MONEY+1),
            scriptBuilder(TicketContribData(5, addr, 40*COIN, 0, MAX_MONEY)),
            TicketContribData(5, addr, 40*COIN, 0, MAX_MONEY)
        },
        {
            TicketContribData(6, addr, 100*COIN, MAX_MONEY+1, -1),
            scriptBuilder(TicketContribData(6, addr, 100*COIN, MAX_MONEY, 0)),
            TicketContribData(6, addr, 100*COIN, MAX_MONEY, 0)
        }
    };

    CMutableTransaction tx;
    std::vector<unsigned char> serializedTxOut;
    TicketContribData deserializedTcd;

    for (auto& test: tests) {
        BOOST_CHECK(test.tcd == test.expectedTcd);

        CScript script = GetScriptForTicketContrib(test.tcd);
        BOOST_CHECK(script == test.expectedScript);

        tx.vout.clear();

        CTxOut txOut{0, script};
        tx.vout.push_back(txOut);
        BOOST_CHECK(ParseTicketContrib(tx, 0, deserializedTcd));

        BOOST_CHECK(deserializedTcd == test.expectedTcd);
    }

    // validated deserialization

    {
        const uint160 dummyAddr{};

        auto flScriptBuilder = [&dummyAddr](const CAmount& vfl, const CAmount& rfl) -> CScript {
            return CScript() << OP_RETURN << OP_STRUCT << 1 << CLASS_Staking << STAKE_TicketContribution
                             << 1
                             << ToByteVector(dummyAddr) << 1
                             << 1000*COIN
                             << vfl
                             << rfl;
        };

        CScript script = flScriptBuilder(-1, MAX_MONEY+1);
        tx.vout.clear(); tx.vout.push_back(CTxOut{0, script});
        BOOST_CHECK(!ParseTicketContrib(tx, 0, deserializedTcd));

        script = flScriptBuilder(MAX_MONEY+1, -1);
        tx.vout.clear(); tx.vout.push_back(CTxOut{0, script});
        BOOST_CHECK(!ParseTicketContrib(tx, 0, deserializedTcd));
    }
}

// test the serialization and deserialization of the ticket multiple contribution data
BOOST_AUTO_TEST_CASE(ticket_contribs_data_serialization)
{
    struct TestData {
        TicketContribData tcd;

        CScript expectedScript;
        TicketContribData expectedTcd;
    };

    CPubKey pubKey;
    BOOST_CHECK(wallet->GetKeyFromPool(pubKey));
    const CTxDestination addr = pubKey.GetID();

    auto scriptBuilder = [](TicketContribData tcd) -> CScript {
        return CScript() << OP_RETURN << OP_STRUCT << 1 << CLASS_Staking << STAKE_TicketContribution
                         << tcd.nVersion
                         << ToByteVector(tcd.rewardAddr) << tcd.whichAddr
                         << tcd.contributedAmount
                         << tcd.voteFeeLimit()
                         << tcd.revocationFeeLimit();
    };

    std::vector<TestData> tests {
        {
            TicketContribData(1, addr, 5*COIN, 10, 0),
            scriptBuilder(TicketContribData(1, addr, 5*COIN, 10, 0)),
            TicketContribData(1, addr, 5*COIN, 10, 0)
        },
        {
            TicketContribData(2, addr, 10*COIN, 0, 10),
            scriptBuilder(TicketContribData(2, addr, 10*COIN, 0, 10)),
            TicketContribData(2, addr, 10*COIN, 0, 10)
        },
        {
            TicketContribData(3, addr, 20*COIN, 20, 0),
            scriptBuilder(TicketContribData(3, addr, 20*COIN, 20, 0)),
            TicketContribData(3, addr, 20*COIN, 20, 0)
        },
        {
            TicketContribData(4, addr, 30*COIN, 0, 30),
            scriptBuilder(TicketContribData(4, addr, 30*COIN, 0, 30)),
            TicketContribData(4, addr, 30*COIN, 0, 30)
        },
        {
            TicketContribData(5, addr, 40*COIN, 15, 25),
            scriptBuilder(TicketContribData(5, addr, 40*COIN, 15, 25)),
            TicketContribData(5, addr, 40*COIN, 15, 25)
        },
        {
            TicketContribData(6, addr, 100*COIN, 40, 30),
            scriptBuilder(TicketContribData(6, addr, 100*COIN, 40, 30)),
            TicketContribData(6, addr, 100*COIN, 40, 30)
        },
        {
            TicketContribData(7, addr, 40*COIN, -1, MAX_MONEY+1),
            scriptBuilder(TicketContribData(7, addr, 40*COIN, 0, MAX_MONEY)),
            TicketContribData(7, addr, 40*COIN, 0, MAX_MONEY)
        },
        {
            TicketContribData(8, addr, 100*COIN, MAX_MONEY+1, -1),
            scriptBuilder(TicketContribData(8, addr, 100*COIN, MAX_MONEY, 0)),
            TicketContribData(8, addr, 100*COIN, MAX_MONEY, 0)
        }
    };

    CMutableTransaction mtx;
    for (unsigned i = 0; i < ticketContribOutputIndex; ++i)
        mtx.vout.push_back(CTxOut{1, coinbaseScriptPubKey}); // dummy output

    for (unsigned i = 0; i < tests.size(); ++i)  {
        BOOST_CHECK(GetScriptForTicketContrib(tests[i].tcd) == tests[i].expectedScript);

        mtx.vout.push_back(CTxOut{0, GetScriptForTicketContrib(tests[i].tcd)});

        mtx.vout.push_back(CTxOut{1, coinbaseScriptPubKey}); // dummy change
    }

    std::vector<TicketContribData> deserializedTcds;
    CAmount totalContribution;
    CAmount totalVoteFeeLimit;
    CAmount totalRevocationFeeLimit;

    BOOST_CHECK(ParseTicketContribs(mtx, deserializedTcds, totalContribution, totalVoteFeeLimit, totalRevocationFeeLimit));

    BOOST_CHECK(deserializedTcds.size() == tests.size());

    CAmount expectedTotalContribution{0};
    CAmount expectedTotalVoteFeeLimit{0};
    CAmount expectedTotalRevocationFeeLimit{0};
    for (unsigned i = 0; i < tests.size(); ++i) {
        BOOST_CHECK(deserializedTcds[i] == tests[i].expectedTcd);

        expectedTotalContribution += tests[i].expectedTcd.contributedAmount;
        expectedTotalVoteFeeLimit += tests[i].expectedTcd.voteFeeLimit();
        expectedTotalRevocationFeeLimit += tests[i].expectedTcd.revocationFeeLimit();
    }

    BOOST_CHECK(totalContribution == expectedTotalContribution);
    BOOST_CHECK(totalVoteFeeLimit == expectedTotalVoteFeeLimit);
    BOOST_CHECK(totalRevocationFeeLimit == expectedTotalRevocationFeeLimit);
}

// test the ticket purchase estimated sizes of inputs and outputs
BOOST_AUTO_TEST_CASE(ticket_purchase_estimated_sizes)
{
    BOOST_CHECK_EQUAL(GetEstimatedP2PKHTxInSize(), 148U);
    BOOST_CHECK_EQUAL(GetEstimatedP2PKHTxInSize(false), 180U);

    BOOST_CHECK_EQUAL(GetEstimatedP2PKHTxOutSize(), 34U);

    BOOST_CHECK_EQUAL(GetEstimatedBuyTicketDeclTxOutSize(), 16U);

    BOOST_CHECK_EQUAL(GetEstimatedTicketContribTxOutSize(), 64U);

    BOOST_CHECK_EQUAL(GetEstimatedSizeOfBuyTicketTx(true), 552U);
    BOOST_CHECK_EQUAL(GetEstimatedSizeOfBuyTicketTx(false), 306U);
}

// test the ticket ownership verification
BOOST_FIXTURE_TEST_CASE(is_my_ticket, WalletStakeTestingSetup)
{
    GetSomeCoins();

    LOCK2(cs_main, wallet->cs_wallet);

    // dummy transaction

    {
        CMutableTransaction mtx;
        mtx.vin.push_back(CTxIn(emptyData.hash, 0));
        BOOST_CHECK_EQUAL(wallet->IsMyTicket(mtx), false);
        BOOST_CHECK_EQUAL(wallet->IsMyTicket(mtx.GetHash()), false);
        mtx.vout.push_back(CTxOut(1000, CScript()));
        BOOST_CHECK_EQUAL(wallet->IsMyTicket(mtx), false);
        BOOST_CHECK_EQUAL(wallet->IsMyTicket(mtx.GetHash()), false);
    }

    // valid transaction (not owned output)

    {
        CKey key;
        key.MakeNewKey(true);

        CMutableTransaction mtx;
        mtx.vout.push_back(CTxOut(1 * COIN, GetScriptForDestination(key.GetPubKey().GetID())));
        BOOST_CHECK_EQUAL(wallet->IsMyTicket(mtx), false);
        BOOST_CHECK_EQUAL(wallet->IsMyTicket(mtx.GetHash()), false);

        CAmount feeRet;
        int changePos{-1};
        std::string reason;
        BOOST_CHECK_MESSAGE(wallet->FundTransaction(mtx, feeRet, changePos, reason, false, {}, CCoinControl()), ((reason.size() > 0) ? reason : "FundTransaction"));
        BOOST_CHECK_EQUAL(wallet->IsMyTicket(mtx), false);
        BOOST_CHECK_EQUAL(wallet->IsMyTicket(mtx.GetHash()), false);
    }

    // valid transaction (owned output)

    {
        CPubKey pubKey;
        wallet->GetKeyFromPool(pubKey);

        CMutableTransaction mtx;
        mtx.vout.push_back(CTxOut(1 * COIN, GetScriptForDestination(pubKey.GetID())));
        BOOST_CHECK_EQUAL(wallet->IsMyTicket(mtx), false);
        BOOST_CHECK_EQUAL(wallet->IsMyTicket(mtx.GetHash()), false);

        CAmount feeRet;
        int changePos{-1};
        std::string reason;
        BOOST_CHECK_MESSAGE(wallet->FundTransaction(mtx, feeRet, changePos, reason, false, {}, CCoinControl()), ((reason.size() > 0) ? reason : "FundTransaction"));
        BOOST_CHECK_EQUAL(wallet->IsMyTicket(mtx), false);
        BOOST_CHECK_EQUAL(wallet->IsMyTicket(mtx.GetHash()), false);
    }

    // valid ticket (not owned stake output)

    {
        CKey key;
        key.MakeNewKey(true);

        std::vector<std::string> txHashes;
        CWalletError we;

        std::tie(txHashes, we) = wallet->PurchaseTicket("", MAX_MONEY, 1, key.GetPubKey().GetID(), 1, CNoDestination(), 0.0, 0);
        BOOST_CHECK(we.code == CWalletError::SUCCESSFUL);
        BOOST_CHECK(txHashes.size() > 0);

        BOOST_CHECK_EQUAL(wallet->IsMyTicket(uint256S(txHashes[0])), false);
    }

    // valid ticket

    {
        CPubKey pubKey;
        wallet->GetKeyFromPool(pubKey);

        std::vector<std::string> txHashes;
        CWalletError we;

        std::tie(txHashes, we) = wallet->PurchaseTicket("", MAX_MONEY, 1, pubKey.GetID(), 1, CNoDestination(), 0.0, 0);
        BOOST_CHECK(we.code == CWalletError::SUCCESSFUL);
        BOOST_CHECK(txHashes.size() > 0);

        BOOST_CHECK_EQUAL(wallet->IsMyTicket(uint256S(txHashes[0])), true);
    }
}

// test the split transaction for funding a ticket purchase
BOOST_FIXTURE_TEST_CASE(ticket_purchase_split_transaction, WalletStakeTestingSetup)
{
    uint256 splitTxHash;
    CWalletError we;
    CAmount neededPerTicket{0};
    const CAmount ticketFee{10000};

    GetSomeCoins();

    LOCK2(cs_main, wallet->cs_wallet);

    // Coin availability

    std::tie(splitTxHash, we) = wallet.get()->CreateTicketPurchaseSplitTx("", 1000000000 * COIN, ticketFee);
    BOOST_CHECK_EQUAL(we.code, CWalletError::WALLET_INSUFFICIENT_FUNDS);

    std::tie(splitTxHash, we) = wallet.get()->CreateTicketPurchaseSplitTx("", 100000000 * COIN, ticketFee);
    BOOST_CHECK_EQUAL(we.code, CWalletError::WALLET_INSUFFICIENT_FUNDS);

    std::tie(splitTxHash, we) = wallet.get()->CreateTicketPurchaseSplitTx("", 10000000 * COIN, ticketFee);
    BOOST_CHECK_EQUAL(we.code, CWalletError::WALLET_INSUFFICIENT_FUNDS);

    // Single ticket

    std::tie(splitTxHash, we) = wallet.get()->CreateTicketPurchaseSplitTx("", 100 * COIN, ticketFee);
    BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);

    const CWalletTx* splitTxSingle = wallet.get()->GetWalletTx(splitTxHash);
    BOOST_CHECK(splitTxSingle != nullptr);

    BOOST_CHECK_EQUAL(splitTxSingle->tx->vin.size(), 1U);

    BOOST_CHECK_EQUAL(splitTxSingle->tx->vout.size(), 2U); // ticket + change

    neededPerTicket = 100 * COIN + ticketFee; // ticket price + ticket fee

    BOOST_CHECK_EQUAL(splitTxSingle->tx->vout[0].nValue, neededPerTicket);

    // Multiple tickets

    std::tie(splitTxHash, we) = wallet.get()->CreateTicketPurchaseSplitTx("", 10 * COIN, ticketFee, 0, 10);
    BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);

    const CWalletTx* splitTxMultiple = wallet.get()->GetWalletTx(splitTxHash);
    BOOST_CHECK(splitTxMultiple != nullptr);

    BOOST_CHECK_EQUAL(splitTxMultiple->tx->vout.size(), 10U + 1U); // tickets + change

    neededPerTicket = 10 * COIN + ticketFee; // ticket price + ticket fee

    for (size_t i = 0; i < 10; ++i)
        BOOST_CHECK_EQUAL(splitTxMultiple->tx->vout[i].nValue, neededPerTicket);

    // Use VSP (single ticket)

    std::tie(splitTxHash, we) = wallet.get()->CreateTicketPurchaseSplitTx("", 100 * COIN, ticketFee, 20 * COIN);
    BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);

    const CWalletTx* splitTxVspSingle = wallet.get()->GetWalletTx(splitTxHash);
    BOOST_CHECK(splitTxVspSingle != nullptr);

    BOOST_CHECK_EQUAL(splitTxVspSingle->tx->vout.size(), 2U + 1U); // user + VSP + change

    neededPerTicket = 100 * COIN + ticketFee; // ticket price + ticket fee

    BOOST_CHECK_EQUAL(splitTxVspSingle->tx->vout[0].nValue, 20 * COIN); // VSP fee
    BOOST_CHECK_EQUAL(splitTxVspSingle->tx->vout[1].nValue, neededPerTicket - 20 * COIN); // user (needed per ticket - VSP)

    // Use VSP (multiple tickets)

    std::tie(splitTxHash, we) = wallet.get()->CreateTicketPurchaseSplitTx("", 10 * COIN, ticketFee, 5 * COIN, 10);
    BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);

    const CWalletTx* splitTxVspMultiple = wallet.get()->GetWalletTx(splitTxHash);
    BOOST_CHECK(splitTxVspMultiple != nullptr);

    BOOST_CHECK_EQUAL(splitTxVspMultiple->tx->vout.size(), 10U * 2U + 1U); // users + VSPs + change

    neededPerTicket = 10 * COIN + ticketFee; // ticket price + ticket fee

    for (size_t i = 0; i < 10; ++i) {
        BOOST_CHECK_EQUAL(splitTxVspMultiple->tx->vout[2*i].nValue, 5 * COIN); // VSP fee
        BOOST_CHECK_EQUAL(splitTxVspMultiple->tx->vout[2*i+1].nValue, neededPerTicket - 5 * COIN); // user (needed per ticket - VSP)
    }

    // Fee rate

    std::tie(splitTxHash, we) = wallet.get()->CreateTicketPurchaseSplitTx("", 10 * COIN, ticketFee, 5 * COIN, 10, ticketFee);
    BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);

    const CWalletTx* splitTxFeeRate = wallet.get()->GetWalletTx(splitTxHash);
    BOOST_CHECK(splitTxFeeRate != nullptr);

    CFeeRate feeRate{ticketFee};

    CAmount fee;
    std::string account;
    std::list<COutputEntry> received;
    std::list<COutputEntry> sent;
    splitTxFeeRate->GetAmounts(received, sent, fee, account, ISMINE_ALL);

    BOOST_CHECK_LE(feeRate.GetFee(static_cast<size_t>(GetVirtualTransactionSize(*(splitTxFeeRate->tx)))), fee);
}

// test the reordering of split transactions in a block
BOOST_FIXTURE_TEST_CASE(split_transaction_reordering, WalletStakeTestingSetup)
{
    const int count = 5;
    const CAmount ticketFee{10000};

    LOCK2(cs_main, wallet->cs_wallet);

    CPubKey splitTxPubKeys[count];
    CTxDestination splitTxKeyIds[count];
    CScript splitTxScriptPubKey[count];
    for (size_t i = 0; i < count; ++i) {
        BOOST_CHECK(wallet->GetKeyFromPool(splitTxPubKeys[i]));
        splitTxKeyIds[i] = splitTxPubKeys[i].GetID();
        splitTxScriptPubKey[i] = GetScriptForDestination(splitTxKeyIds[i]);
    }

    CPubKey ticketPubKeys[count];
    CTxDestination ticketKeyIds[count];
    CScript ticketScriptPubKey[count];
    for (size_t i = 0; i < count; ++i) {
        BOOST_CHECK(wallet->GetKeyFromPool(ticketPubKeys[i]));
        ticketKeyIds[i] = ticketPubKeys[i].GetID();
        ticketScriptPubKey[i] = GetScriptForDestination(ticketKeyIds[i]);
    }

    ExtendChain(consensus.nStakeEnabledHeight + 1 - chainActive.Height());

    CAmount ticketPrice = CalculateNextRequiredStakeDifficulty(chainActive.Tip(), consensus);
    CAmount neededPerTicket = ticketPrice + ticketFee;

    // create some interdependent split transactions
    // the first one has a large change, in order to cover the subsequent ones

    CMutableTransaction splitTxs[count];
    CWalletError we;
    for (int i = count-1; i >= 0 ; --i) {
        if (i == count-1) {
            CAmount feeRet{0};
            int changePos{1};
            std::string reason;

            splitTxs[i].vout.push_back(CTxOut(neededPerTicket + 5 * neededPerTicket, splitTxScriptPubKey[i]));

            BOOST_CHECK(wallet->FundTransaction(splitTxs[i], feeRet, changePos, reason, false, {}, CCoinControl()));

            splitTxs[i].vout[0].nValue = neededPerTicket;

            splitTxs[i].vout[1].nValue += 5 * neededPerTicket;
        } else {
            splitTxs[i].vin.push_back(CTxIn(splitTxs[i+1].GetHash(), 1));

            splitTxs[i].vout.push_back(CTxOut(neededPerTicket, splitTxScriptPubKey[i]));

            splitTxs[i].vout.push_back(CTxOut(splitTxs[i+1].vout[1].nValue - neededPerTicket, splitTxScriptPubKey[i]));

            BOOST_CHECK(wallet->SignTransaction(splitTxs[i]));

            CAmount fee = CFeeRate(10000).GetFee(GetVirtualTransactionSize(splitTxs[i]));
            splitTxs[i].vout[1].nValue -= fee;
        }

        BOOST_CHECK(wallet->SignTransaction(splitTxs[i]));

        CWalletTx wtx;
        CValidationState state;
        wtx.fTimeReceivedIsTxTime = true;
        wtx.BindWallet(wallet.get());
        wtx.SetTx(MakeTransactionRef(splitTxs[i]));
        CReserveKey reservekey{wallet.get()};
        BOOST_CHECK(wallet->CommitTransaction(wtx, reservekey, g_connman.get(), state));
    }

    // create the tickets

    CMutableTransaction tickets[count];
    for (size_t i = 0; i < count ; ++i) {
        tickets[i].vin.push_back(CTxIn(splitTxs[i].GetHash(), 0));

        tickets[i].vout.push_back(CTxOut(0, GetScriptForBuyTicketDecl(BuyTicketData{1})));

        tickets[i].vout.push_back(CTxOut(ticketPrice, GetScriptForDestination(ticketKeyIds[i])));

        tickets[i].vout.push_back(CTxOut(0, GetScriptForTicketContrib(TicketContribData{1, ticketKeyIds[i], neededPerTicket, 0, TicketContribData::DefaultFeeLimit})));

        tickets[i].vout.push_back(CTxOut(splitTxs[i].vout[0].nValue - neededPerTicket, GetScriptForDestination(ticketKeyIds[i])));

        BOOST_CHECK(wallet->SignTransaction(tickets[i]));

        CWalletTx wtx;
        CValidationState state;
        wtx.fTimeReceivedIsTxTime = true;
        wtx.BindWallet(wallet.get());
        wtx.SetTx(MakeTransactionRef(tickets[i]));
        CReserveKey reservekey{wallet.get()};
        BOOST_CHECK(wallet->CommitTransaction(wtx, reservekey, g_connman.get(), state));
    }

    // create a block template and mine it

    std::unique_ptr<CBlockTemplate> pblocktemplate;
    BOOST_CHECK_NO_THROW(pblocktemplate = BlockAssembler(params).CreateNewBlock(coinbaseScriptPubKey));
    BOOST_CHECK(pblocktemplate.get());

    CBlock& block = pblocktemplate->block;

    unsigned int extraNonce = 0;
    IncrementExtraNonce(&block, chainActive.Tip(), extraNonce);

    while (!CheckProofOfWork(block.GetHash(), block.nBits, consensus)) ++block.nNonce;

    std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(block);
    BOOST_CHECK(ProcessNewBlock(params, shared_pblock, true, nullptr));
}

// test the transaction for purchasing tickets
BOOST_FIXTURE_TEST_CASE(ticket_purchase_transaction, WalletStakeTestingSetup)
{
    std::vector<std::string> txHashes;
    CWalletError we;

    LOCK2(cs_main, wallet->cs_wallet);

    CPubKey ticketPubKey;
    BOOST_CHECK(wallet->GetKeyFromPool(ticketPubKey));
    CTxDestination ticketKeyId = ticketPubKey.GetID();

    CPubKey vspPubKey;
    BOOST_CHECK(wallet->GetKeyFromPool(vspPubKey));
    CTxDestination vspKeyId = vspPubKey.GetID();

    const CAmount spendLimit{100000 * COIN};
    const CAmount feeRate{10000};
    const double vspFeePercent{5.0};

    // validate a single ticket, with or without VSP fee
    auto checkTicket = [&](uint256 txHash, bool useVsp) {
        // Transaction

        BOOST_CHECK(txHash != emptyData.hash);

        const CWalletTx* wtx = wallet->GetWalletTx(txHash);
        BOOST_CHECK(wtx != nullptr);

        BOOST_CHECK(wtx->tx != nullptr);
        const CTransaction& tx = *(wtx->tx);

        // Inputs

        BOOST_CHECK_EQUAL(tx.vin.size(), useVsp ? 2U : 1U); // (vsp) + user

        // Outputs

        BOOST_CHECK_EQUAL(tx.vout.size(), useVsp ? 6U : 4U); // declaration + stake + (vsp commitment + vsp change +) user commitment + user change

        // check ticket declaration
        BOOST_CHECK_EQUAL(ParseTxClass(tx), TX_BuyTicket);
        BOOST_CHECK_EQUAL(tx.vout[0].nValue, 0);

        // make sure that the stake address and value are correct
        int64_t ticketPrice = CalculateNextRequiredStakeDifficulty(chainActive.Tip(), consensus);
        BOOST_CHECK(tx.vout[1].scriptPubKey == GetScriptForDestination(ticketKeyId));
        BOOST_CHECK_EQUAL(tx.vout[1].nValue, ticketPrice);

        // precalculate the necessary values
        size_t txSize = GetEstimatedSizeOfBuyTicketTx(useVsp);
        CFeeRate txFeeRate{feeRate};
        CAmount ticketFee{std::max(txFeeRate.GetFee(txSize), wallet->minTxFee.GetFee(txSize))};
        CAmount neededPerTicket = ticketPrice + ticketFee;
        CAmount vspFee = (useVsp ? StakePoolTicketFee(ticketPrice, ticketFee, chainActive.Height(), vspFeePercent) : 0);
        uint32_t userOutput = 2 + (useVsp ? 2 : 0);

        // check VSP outputs, if present
        // make sure that the contributor address matches the VSP address, the contributed amount is the VSP fee
        // and that the change is zero
        if (useVsp) {
            TicketContribData tcd;
            BOOST_CHECK(ParseTicketContrib(tx, 2, tcd));
            BOOST_CHECK_EQUAL(tcd.nVersion, 1);
            BOOST_CHECK_EQUAL(tcd.contributedAmount, vspFee);
            BOOST_CHECK_EQUAL(tcd.voteFeeLimit(), 0);
            BOOST_CHECK_EQUAL(tcd.revocationFeeLimit(), TicketContribData::DefaultFeeLimit);
            BOOST_CHECK_EQUAL(tcd.whichAddr, 1);
            BOOST_CHECK(tcd.rewardAddr == boost::get<CKeyID>(vspKeyId));
            BOOST_CHECK_EQUAL(tx.vout[2].nValue, 0);

            BOOST_CHECK(tx.vout[3].scriptPubKey == GetScriptForDestination(vspKeyId));
            BOOST_CHECK_EQUAL(tx.vout[3].nValue, 0);
        }

        // check user outputs
        // make sure that the contributor address is owned by the wallet, the contributed amount reflects the correct value,
        // also considering the VSP fee if present and that the change is zero
        TicketContribData tcd;
        BOOST_CHECK(ParseTicketContrib(tx, userOutput, tcd));
        BOOST_CHECK_EQUAL(tcd.nVersion, 1);
        BOOST_CHECK_EQUAL(tcd.contributedAmount, neededPerTicket - vspFee);
        BOOST_CHECK_EQUAL(tcd.voteFeeLimit(), 0);
        BOOST_CHECK_EQUAL(tcd.revocationFeeLimit(), TicketContribData::DefaultFeeLimit);
        BOOST_CHECK_EQUAL(tcd.whichAddr, 1);
        BOOST_CHECK(wallet->IsMine(CTxOut(0, GetScriptForDestination(CKeyID(uint160(tcd.rewardAddr))))));
        BOOST_CHECK_EQUAL(tx.vout[userOutput].nValue, 0);

        BOOST_CHECK(wallet->IsMine(tx.vout[userOutput+1]));
        BOOST_CHECK_EQUAL(tx.vout[userOutput+1].nValue, 0);
    };

    // Coin availability
    {
        txHashes.clear();

        std::tie(txHashes, we) = wallet->PurchaseTicket("", 100000 * COIN, 1, ticketKeyId, 10, CNoDestination(), 0.0, 0, 1000 * COIN);
        BOOST_CHECK_EQUAL(we.code, CWalletError::WALLET_INSUFFICIENT_FUNDS);

        GetSomeCoins();

        std::tie(txHashes, we) = wallet->PurchaseTicket("", 100 * COIN, 1, ticketKeyId, 10000, CNoDestination(), 0.0, 0, feeRate);
        BOOST_CHECK_EQUAL(we.code, CWalletError::WALLET_ERROR);
    }

    ExtendChain(consensus.nStakeValidationHeight + 1 - chainActive.Height());

    // Single purchase, no VSP
    {
        txHashes.clear();

        std::tie(txHashes, we) = wallet->PurchaseTicket("", spendLimit, 1, ticketKeyId, 1, CNoDestination(), 0.0, 0, feeRate);
        BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);

        BOOST_CHECK_EQUAL(txHashes.size(), 1U);

        uint256 txHash = uint256S(txHashes[0]);

        checkTicket(txHash, false);
    }

    // Single purchase, with VSP
    {
        txHashes.clear();

        std::tie(txHashes, we) = wallet->PurchaseTicket("", spendLimit, 1, ticketKeyId, 1, vspKeyId, vspFeePercent, 0, feeRate);
        BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);

        BOOST_CHECK_EQUAL(txHashes.size(), 1U);

        uint256 txHash = uint256S(txHashes[0]);

        checkTicket(txHash, true);
    }

    // Multiple purchase, no VSP
    {
        txHashes.clear();

        unsigned int numTickets{10};

        std::tie(txHashes, we) = wallet->PurchaseTicket("", spendLimit, 1, ticketKeyId, numTickets, CNoDestination(), 0.0, 0, feeRate);
        BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);

        BOOST_CHECK_EQUAL(txHashes.size(), static_cast<size_t>(numTickets));

        for (unsigned int i = 0; i < numTickets; ++i) {
            uint256 txHash = uint256S(txHashes[i]);
            checkTicket(txHash, false);
        }
    }

    // Multiple purchase, with VSP
    {
        txHashes.clear();

        unsigned int numTickets{10};

        std::tie(txHashes, we) = wallet->PurchaseTicket("", spendLimit, 1, ticketKeyId, numTickets, vspKeyId, vspFeePercent, 0, feeRate);
        BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);

        BOOST_CHECK_EQUAL(txHashes.size(), static_cast<size_t>(numTickets));

        for (unsigned int i = 0; i < numTickets; ++i) {
            uint256 txHash = uint256S(txHashes[i]);
            checkTicket(txHash, true);
        }
    }
}

// test the ticket buyer
BOOST_FIXTURE_TEST_CASE(ticket_buyer, WalletStakeTestingSetup)
{
    CPubKey ticketPubKey;
    CPubKey vspPubKey;

    {
        LOCK(wallet->cs_wallet);
        BOOST_CHECK(wallet->GetKeyFromPool(ticketPubKey));
        BOOST_CHECK(wallet->GetKeyFromPool(vspPubKey));
    }

    CTxDestination ticketKeyId = ticketPubKey.GetID();

    CTxDestination vspKeyId = vspPubKey.GetID();

    ExtendChain(consensus.nStakeEnabledHeight + 1 - chainActive.Height());

    BOOST_CHECK_GE(chainActive.Tip()->nTx, 1U);
    BOOST_CHECK_GE(chainActive.Tip()->nFreshStake, 0U);

    CTicketBuyer* tb = wallet->GetTicketBuyer();
    BOOST_CHECK(tb != nullptr);

    BOOST_CHECK(!tb->isStarted());

    CTicketBuyerConfig& cfg = tb->GetConfig();

    BOOST_CHECK_EQUAL(cfg.buyTickets, false);

    cfg.account = "";
    cfg.votingAccount = "";
    cfg.votingAddress = ticketKeyId;
    cfg.poolFeeAddress = CNoDestination();
    cfg.poolFees = 0.0;
    cfg.limit = 1;
    cfg.passphrase = "";

    // Coin availability

    cfg.maintain = wallet->GetBalance() - 1000;

    tb->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    ExtendChain(1);

    BOOST_CHECK_GE(chainActive.Tip()->nTx, 1U);
    BOOST_CHECK_GE(chainActive.Tip()->nFreshStake, 0U);

    // Single ticket, no VSP

    tb->stop();

    cfg.maintain = 0;

    tb->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ExtendChain(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ExtendChain(1);

    BOOST_CHECK_GE(chainActive.Tip()->nTx, 1U + 1U + 1U);
    BOOST_CHECK_GE(chainActive.Tip()->nFreshStake, 1U);

    BOOST_CHECK_EQUAL(txsInMempoolBeforeLastBlock.size(), 2U);

    BOOST_CHECK_EQUAL(ParseTxClass(*txsInMempoolBeforeLastBlock[0].get()), TX_Regular);
    BOOST_CHECK_EQUAL(txsInMempoolBeforeLastBlock[0]->vout.size(), 1U + 1U);

    // TODO: Add amount validation
    CheckTicket(*txsInMempoolBeforeLastBlock[1].get(), {TicketContribData(1, ticketKeyId, 0, 0, TicketContribData::DefaultFeeLimit)});

    // Single ticket, VSP

    tb->stop();

    ExtendChain(consensus.nStakeValidationHeight + 1 - chainActive.Height());
    ExtendChain(5); // since no purchases are made prior to the interval switch,
                    // make sure that the tests do not overlap with this switch.

    cfg.poolFeeAddress = vspKeyId;
    cfg.poolFees = 5.0;

    tb->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ExtendChain(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ExtendChain(1);

    BOOST_CHECK_GE(chainActive.Tip()->nTx, 1U + 1U + 1U);
    BOOST_CHECK_GE(chainActive.Tip()->nFreshStake, 1U);

    BOOST_CHECK_EQUAL(ParseTxClass(*txsInMempoolBeforeLastBlock[0].get()), TX_Regular);
    BOOST_CHECK_EQUAL(txsInMempoolBeforeLastBlock[0]->vout.size(), 1U + 1U + 1U);

    // TODO: Add amount validation
    CheckTicket(*txsInMempoolBeforeLastBlock[1].get(), {TicketContribData(1, vspKeyId, 0, 0, TicketContribData::DefaultFeeLimit), TicketContribData(1, ticketKeyId, 0, 0, TicketContribData::DefaultFeeLimit)});

    // Multiple tickets, no VSP

    tb->stop();

    cfg.poolFeeAddress = CNoDestination();
    cfg.poolFees = 0.0;
    cfg.limit = 5;

    tb->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ExtendChain(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ExtendChain(1);

    BOOST_CHECK_GE(chainActive.Tip()->nTx, static_cast<unsigned int>(1 + 1 + cfg.limit));
    BOOST_CHECK_GE(chainActive.Tip()->nFreshStake, static_cast<uint8_t>(cfg.limit));

    BOOST_CHECK_EQUAL(ParseTxClass(*txsInMempoolBeforeLastBlock[0].get()), TX_Regular);
    BOOST_CHECK_EQUAL(txsInMempoolBeforeLastBlock[0]->vout.size(), static_cast<size_t>(cfg.limit + 1));

    // TODO: Add amount validation
    for (size_t i = 0; static_cast<int>(i) < cfg.limit; ++i)
        CheckTicket(*txsInMempoolBeforeLastBlock[1 + i].get(), {TicketContribData(1, ticketKeyId, 0, 0, TicketContribData::DefaultFeeLimit)});

    // Multiple ticket, VSP

    tb->stop();

    cfg.poolFeeAddress = vspKeyId;
    cfg.poolFees = 5.0;

    tb->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ExtendChain(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ExtendChain(1);

    BOOST_CHECK_GE(chainActive.Tip()->nTx, static_cast<unsigned int>(1 + 1 + cfg.limit));
    BOOST_CHECK_GE(chainActive.Tip()->nFreshStake, static_cast<uint8_t>(cfg.limit));

    BOOST_CHECK_EQUAL(ParseTxClass(*txsInMempoolBeforeLastBlock[0].get()), TX_Regular);
    BOOST_CHECK_EQUAL(txsInMempoolBeforeLastBlock[0]->vout.size(), static_cast<size_t>(2 * cfg.limit + 1));

    // TODO: Add amount validation
    for (size_t i = 0; static_cast<int>(i) < cfg.limit; ++i)
        CheckTicket(*txsInMempoolBeforeLastBlock[1 + i].get(), {TicketContribData(1, vspKeyId, 0, 0, TicketContribData::DefaultFeeLimit), TicketContribData(1, ticketKeyId, 0, 0, TicketContribData::DefaultFeeLimit)});

    tb->stop();
}

// test the ticket buyer on encrypted wallet
BOOST_FIXTURE_TEST_CASE(ticket_buyer_encrypted, WalletStakeTestingSetup)
{
    CPubKey ticketPubKey;
    CPubKey vspPubKey;

    {
        LOCK(wallet->cs_wallet);
        BOOST_CHECK(wallet->GetKeyFromPool(ticketPubKey));
        BOOST_CHECK(wallet->GetKeyFromPool(vspPubKey));
        wallet->EncryptWallet(passphrase);
    }

    CTxDestination ticketKeyId = ticketPubKey.GetID();

    CTxDestination vspKeyId = vspPubKey.GetID();

    ExtendChain(consensus.nStakeValidationHeight + 1 - chainActive.Height() + 7); // since no purchases are made right before the interval switch,
                                                                                  // make sure that the tests do not overlap with this switch.

    CTicketBuyer* tb = wallet->GetTicketBuyer();
    BOOST_CHECK(tb != nullptr);

    BOOST_CHECK(!tb->isStarted());

    CTicketBuyerConfig& cfg = tb->GetConfig();

    BOOST_CHECK_EQUAL(cfg.buyTickets, false);

    cfg.account = "";
    cfg.votingAccount = "";
    cfg.votingAddress = ticketKeyId;
    cfg.poolFeeAddress = vspKeyId;
    cfg.poolFees = 5.0;
    cfg.limit = 5;
    cfg.passphrase = passphrase;

    BOOST_CHECK(wallet->IsLocked());

    tb->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ExtendChain(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ExtendChain(1);

    BOOST_CHECK_GE(chainActive.Tip()->nTx, static_cast<unsigned int>(1 + 1 + cfg.limit));
    BOOST_CHECK_GE(chainActive.Tip()->nFreshStake, static_cast<uint8_t>(cfg.limit));

    BOOST_CHECK_EQUAL(ParseTxClass(*txsInMempoolBeforeLastBlock[0].get()), TX_Regular);
    BOOST_CHECK_EQUAL(txsInMempoolBeforeLastBlock[0]->vout.size(), static_cast<size_t>(2 * cfg.limit + 1));

    // TODO: Add amount validation
    for (size_t i = 0; static_cast<int>(i) < cfg.limit; ++i)
        CheckTicket(*txsInMempoolBeforeLastBlock[1 + i].get(), {TicketContribData(1, vspKeyId, 0, 0, TicketContribData::DefaultFeeLimit), TicketContribData(1, ticketKeyId, 0, 0, TicketContribData::DefaultFeeLimit)});

    tb->stop();
}

// test the password validation from optional value
BOOST_FIXTURE_TEST_CASE(password_validation_from_optional_value, WalletStakeTestingSetup)
{
    LOCK2(cs_main, wallet->cs_wallet);

    // unencrypted wallet

    BOOST_CHECK_NO_THROW(ValidatedPasswordFromOptionalValue(wallet.get(), UniValue()));
    BOOST_CHECK_THROW(ValidatedPasswordFromOptionalValue(wallet.get(), UniValue(10)), UniValue);
    BOOST_CHECK_THROW(ValidatedPasswordFromOptionalValue(wallet.get(), UniValue(1.001)), UniValue);
    BOOST_CHECK_NO_THROW(ValidatedPasswordFromOptionalValue(wallet.get(), UniValue("")));
    BOOST_CHECK_THROW(ValidatedPasswordFromOptionalValue(wallet.get(), UniValue("aWr0ngP4$$word")), UniValue);
    BOOST_CHECK_THROW(ValidatedPasswordFromOptionalValue(wallet.get(), UniValue(passphrase.c_str())), UniValue);
    BOOST_CHECK_NO_THROW(ValidatedPasswordFromOptionalValue(wallet.get(), UniValue(emptyData.string)));
    BOOST_CHECK_THROW(ValidatedPasswordFromOptionalValue(wallet.get(), UniValue(std::string("aWr0ngP4$$word"))), UniValue);
    BOOST_CHECK_THROW(ValidatedPasswordFromOptionalValue(wallet.get(), UniValue(std::string(passphrase.c_str()))), UniValue);

    // encrypted wallet (wrong password)

    wallet->EncryptWallet(passphrase);
    BOOST_CHECK(wallet->IsLocked());

    BOOST_CHECK_THROW(ValidatedPasswordFromOptionalValue(wallet.get(), UniValue()), UniValue);
    BOOST_CHECK_THROW(ValidatedPasswordFromOptionalValue(wallet.get(), UniValue(10)), UniValue);
    BOOST_CHECK_THROW(ValidatedPasswordFromOptionalValue(wallet.get(), UniValue(1.001)), UniValue);
    BOOST_CHECK_THROW(ValidatedPasswordFromOptionalValue(wallet.get(), UniValue("")), UniValue);
    BOOST_CHECK_THROW(ValidatedPasswordFromOptionalValue(wallet.get(), UniValue("aWr0ngP4$$word")), UniValue);
    BOOST_CHECK_THROW(ValidatedPasswordFromOptionalValue(wallet.get(), UniValue(emptyData.string)), UniValue);
    BOOST_CHECK_THROW(ValidatedPasswordFromOptionalValue(wallet.get(), UniValue(std::string("aWr0ngP4$$word"))), UniValue);

    // encrypted wallet (good password)

    BOOST_CHECK_NO_THROW(ValidatedPasswordFromOptionalValue(wallet.get(), UniValue(passphrase.c_str())));
    BOOST_CHECK_NO_THROW(ValidatedPasswordFromOptionalValue(wallet.get(), UniValue(std::string(passphrase.c_str()))));
}

// test the ticket buyer RPCs
BOOST_FIXTURE_TEST_CASE(ticket_buyer_rpc, WalletStakeTestingSetup)
{
    vpwallets.insert(vpwallets.begin(), wallet.get());

    RegisterWalletRPCCommands(tableRPC);

    CPubKey ticketPubKey;
    BOOST_CHECK(wallet->GetKeyFromPool(ticketPubKey));
    CTxDestination ticketKeyId = ticketPubKey.GetID();
    std::string ticketAddress{EncodeDestination(ticketKeyId)};

    CPubKey vspPubKey;
    BOOST_CHECK(wallet->GetKeyFromPool(vspPubKey));
    CTxDestination vspKeyId = vspPubKey.GetID();
    std::string vspAddress{EncodeDestination(vspKeyId)};

    CTicketBuyer* tb = wallet->GetTicketBuyer();
    BOOST_CHECK(tb != nullptr);

    BOOST_CHECK(!tb->isStarted());

    CTicketBuyerConfig& cfg = tb->GetConfig();

    // Settings (write)

    BOOST_CHECK_THROW(CallRpc("setticketbuyeraccount"), std::runtime_error);
    BOOST_CHECK_NO_THROW(CallRpc("setticketbuyeraccount abc"));

    BOOST_CHECK_THROW(CallRpc("setticketbuyerbalancetomaintain"), std::runtime_error);
    BOOST_CHECK_NO_THROW(CallRpc("setticketbuyerbalancetomaintain 123"));

    BOOST_CHECK_THROW(CallRpc("setticketbuyervotingaddress"), std::runtime_error);
    BOOST_CHECK_NO_THROW(CallRpc("setticketbuyervotingaddress " + ticketAddress));

    BOOST_CHECK_THROW(CallRpc("setticketbuyerpooladdress"), std::runtime_error);
    BOOST_CHECK_NO_THROW(CallRpc("setticketbuyerpooladdress " + vspAddress));

    BOOST_CHECK_THROW(CallRpc("setticketbuyerpoolfees"), std::runtime_error);
    BOOST_CHECK_NO_THROW(CallRpc("setticketbuyerpoolfees 5.0"));

    BOOST_CHECK_THROW(CallRpc("setticketbuyermaxperblock"), std::runtime_error);
    BOOST_CHECK_NO_THROW(CallRpc("setticketbuyermaxperblock 5"));

    BOOST_CHECK_EQUAL(cfg.buyTickets, false);
    BOOST_CHECK_EQUAL(cfg.account, "abc");
    BOOST_CHECK_EQUAL(cfg.maintain, 12300000000);
    BOOST_CHECK_EQUAL(cfg.votingAccount, "");
    BOOST_CHECK(cfg.votingAddress == ticketKeyId);
    BOOST_CHECK(cfg.poolFeeAddress == vspKeyId);
    BOOST_CHECK_EQUAL(cfg.poolFees, 5.0);
    BOOST_CHECK_EQUAL(cfg.limit, 5);

    // Settings (read)

    UniValue r;
    BOOST_CHECK_NO_THROW(r = CallRpc("ticketbuyerconfig"));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "buytickets").get_bool(), false);
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "account").get_str(), "abc");
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "maintain").get_int64(), 12300000000);
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "votingAccount").get_str(), "");
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "votingAddress").get_str(), ticketAddress);
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "poolFeeAddress").get_str(), vspAddress);
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "poolFees").get_real(), 5.0);
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "limit").get_int(), 5);
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "minConf").get_int(), 1);

    // Start (with minimal settings)

    BOOST_CHECK_THROW(CallRpc("startticketbuyer"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc("startticketbuyer \"\""), std::runtime_error);
    BOOST_CHECK_NO_THROW(CallRpc("startticketbuyer def 124"));

    BOOST_CHECK_EQUAL(cfg.buyTickets, true);
    BOOST_CHECK_EQUAL(cfg.account, "def");
    BOOST_CHECK_EQUAL(cfg.maintain, 12400000000);
    BOOST_CHECK_EQUAL(cfg.votingAccount, "");
    BOOST_CHECK(cfg.votingAddress.which() == 0);
    BOOST_CHECK(cfg.poolFeeAddress.which() == 0);
    BOOST_CHECK_EQUAL(cfg.poolFees, 0.0);
    BOOST_CHECK_EQUAL(cfg.limit, 1);

    // Start (with full settings)

    tb->stop();

    BOOST_CHECK_EQUAL(cfg.buyTickets, false);

    BOOST_CHECK_NO_THROW(CallRpc(std::string("startticketbuyer fromaccount 125 \"\" votingaccount ") + ticketAddress + " " + vspAddress + " 10.0 8"));

    BOOST_CHECK_EQUAL(cfg.buyTickets, true);
    BOOST_CHECK_EQUAL(cfg.account, "fromaccount");
    BOOST_CHECK_EQUAL(cfg.votingAccount, "votingaccount");
    BOOST_CHECK_EQUAL(cfg.maintain, 12500000000);
    BOOST_CHECK(cfg.votingAddress == ticketKeyId);
    BOOST_CHECK(cfg.poolFeeAddress == vspKeyId);
    BOOST_CHECK_EQUAL(cfg.poolFees, 10.0);
    BOOST_CHECK_EQUAL(cfg.limit, 8);

    // Stop

    BOOST_CHECK_NO_THROW(CallRpc("stopticketbuyer"));

    BOOST_CHECK_EQUAL(cfg.buyTickets, false);

    // Start with passphrase

    tb->stop();

    BOOST_CHECK_EQUAL(cfg.buyTickets, false);

    wallet->EncryptWallet(passphrase);
    BOOST_CHECK(wallet->IsLocked());

    BOOST_CHECK_THROW(CallRpc("startticketbuyer def 124"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc("startticketbuyer def 124 wrongP4ssword!"), std::runtime_error);
    BOOST_CHECK_THROW(CallRpc("startticketbuyer def 124 wrongP4ssword votingaccount " + ticketAddress + " " + vspAddress + " 10.0 8"), std::runtime_error);

    BOOST_CHECK_NO_THROW(CallRpc(std::string("startticketbuyer fromaccount 125 ") + std::string(passphrase.c_str()) + " votingaccount " + ticketAddress + " " + vspAddress + " 10.0 8"));

    BOOST_CHECK_EQUAL(cfg.buyTickets, true);
    BOOST_CHECK_EQUAL(cfg.account, "fromaccount");
    BOOST_CHECK_EQUAL(cfg.votingAccount, "votingaccount");
    BOOST_CHECK_EQUAL(cfg.maintain, 12500000000);
    BOOST_CHECK(cfg.votingAddress == ticketKeyId);
    BOOST_CHECK(cfg.poolFeeAddress == vspKeyId);
    BOOST_CHECK_EQUAL(cfg.poolFees, 10.0);
    BOOST_CHECK_EQUAL(cfg.limit, 8);
    BOOST_CHECK_EQUAL(cfg.passphrase, passphrase);

    tb->stop();

    vpwallets.erase(std::remove(vpwallets.begin(), vpwallets.end(), wallet.get()), vpwallets.end());
}

BOOST_AUTO_TEST_SUITE_END()
