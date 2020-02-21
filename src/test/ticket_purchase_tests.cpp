// Copyright (c) 2020 The Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test/test_paicoin.h"
#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string.hpp>

#include "wallet/wallet.h"
#include "stake/stakepoolfee.h"
#include "stake/staketx.h"
#include "key_io.h"
#include "serialize.h"
#include "validation.h"
#include "stake/stakepoolfee.h"
#include "policy/policy.h"
#include "wallet/coincontrol.h"
#include "rpc/server.h"
#include "rpc/client.h"
#include "net.h"
#include "miner.h"
#include <univalue.h>
#include <limits>
#include <vector>

static void AddKey(CWallet& wallet, const CKey& key)
{
    LOCK(wallet.cs_wallet);
    wallet.AddKeyPubKey(key, key.GetPubKey());
}

class TicketPurchaseTestingSetup : public TestChain100Setup
{
public:
    TicketPurchaseTestingSetup() :
        TestChain100Setup(scriptPubKeyType::P2PKH)
    {
        CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
        ::bitdb.MakeMock();
        wallet.reset(new CWallet(std::unique_ptr<CWalletDBWrapper>(new CWalletDBWrapper(&bitdb, "wallet_test.dat"))));
        bool firstRun;
        wallet->LoadWallet(firstRun);
        AddKey(*wallet, coinbaseKey);
        wallet->ScanForWalletTransactions(chainActive.Genesis(), nullptr);
        wallet->SetBroadcastTransactions(true);
    }

    ~TicketPurchaseTestingSetup()
    {
        wallet.reset();
        ::bitdb.Flush(true);
        ::bitdb.Reset();
    }

    uint256 AddTicketTx()
    {
        // ticket settings

        CPubKey ticketPubKey;
        BOOST_CHECK(wallet->GetKeyFromPool(ticketPubKey));
        const CTxDestination ticketKeyId = ticketPubKey.GetID();

        const CAmount ticketPrice = CalculateNextRequiredStakeDifficulty(chainActive.Tip(), Params().GetConsensus());
        CFeeRate feeRate{100000};
        const CAmount ticketFee = feeRate.GetFee(GetEstimatedSizeOfBuyTicketTx(false));
        const CAmount contributedAmount = ticketPrice + ticketFee;

        // split transaction

        uint256 splitTxHash;
        CWalletError we;
        std::tie(splitTxHash, we) = wallet->CreateTicketPurchaseSplitTx("", ticketPrice, ticketFee);
        BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);

        const CWalletTx* splitWTx = wallet->GetWalletTx(splitTxHash);
        BOOST_CHECK(splitWTx != nullptr);
        BOOST_CHECK(splitWTx->tx != nullptr);

        CTransactionRef splitTx = splitWTx->tx;
        BOOST_CHECK(splitTx.get() != nullptr);

        BOOST_CHECK_GE(splitTx->vout.size(), 1);
        BOOST_CHECK_GE(splitTx->vout[0].nValue, ticketPrice + ticketFee);

        // ticket

        CMutableTransaction mtx;

        // input

        mtx.vin.push_back(CTxIn(splitTxHash, 0));

        // outputs

        BuyTicketData buyTicketData = {1};
        CScript declScript = GetScriptForBuyTicketDecl(buyTicketData);
        mtx.vout.push_back(CTxOut(0, declScript));

        CScript ticketScript = GetScriptForDestination(ticketKeyId);
        mtx.vout.push_back(CTxOut(ticketPrice, ticketScript));

        TicketContribData ticketContribData{1, ticketKeyId, contributedAmount, TicketContribData::NoFees, TicketContribData::NoFees};
        CScript contributorInfoScript = GetScriptForTicketContrib(ticketContribData);
        mtx.vout.push_back(CTxOut(0, contributorInfoScript));

        CScript changeScript = GetScriptForDestination(ticketKeyId);
        mtx.vout.push_back(CTxOut(0, changeScript));

        // signature

        BOOST_CHECK(wallet->SignTransaction(mtx));

        // commitment

        CWalletTx wtx;
        CValidationState state;
        wtx.fTimeReceivedIsTxTime = true;
        wtx.BindWallet(wallet.get());
        wtx.SetTx(MakeTransactionRef(std::move(mtx)));
        CReserveKey reservekey{wallet.get()};
        BOOST_CHECK(wallet->CommitTransaction(wtx, reservekey, g_connman.get(), state));

        return wtx.GetHash();
    }

    std::vector<uint256> AddVoteTxs()
    {
        const Consensus::Params& consensus = Params().GetConsensus();

        // block being voted

        const CBlockIndex *tip = chainActive.Tip();
        BOOST_CHECK(tip != nullptr);

        const uint256& hash{tip->GetBlockHash()};
        BOOST_CHECK(hash != uint256());

        const int height = tip->nHeight;
        BOOST_CHECK_GT(height, 0);

        // TODO: Change this!!!
        const int ticketHeight = chainActive.Height() - consensus.nTicketMaturity - 1;
        BOOST_CHECK_GT(ticketHeight, 1);

        const CBlockIndex* ticketBlockIndex = chainActive[ticketHeight];
        BOOST_CHECK(ticketBlockIndex != nullptr);

        std::vector<uint256> voteHashes;

        // votes

        int majority{(consensus.nTicketsPerBlock / 2) + 1};
        int i{0};

        for (auto ticketHash: chainActive.Tip()->pstakeNode->Winners()) {
            if (++i > majority)
                break;

            // ticket

            const CWalletTx* ticket = wallet->GetWalletTx(ticketHash);

            BOOST_CHECK(ticketHash != uint256());
            BOOST_CHECK(ticket != nullptr);
            BOOST_CHECK(ticket->tx != nullptr);

            const CAmount ticketPrice = CalculateNextRequiredStakeDifficulty(ticketBlockIndex->pprev, consensus);
            BOOST_CHECK(MoneyRange(ticketPrice));

            TicketContribData ticketContribData;
            BOOST_CHECK(ParseTicketContrib(*(ticket->tx), ticketContribOutputIndex, ticketContribData));

            const CAmount voterSubsidy = GetVoterSubsidy(height + 1, consensus);
            BOOST_CHECK_GT(voterSubsidy, 0);

            const CAmount reward = CalcContributorRemuneration(ticketContribData.contributedAmount, ticketPrice, voterSubsidy, ticketContribData.contributedAmount);
            BOOST_CHECK_GT(reward, 0);

            // vote

            CMutableTransaction mtx;

            // inputs

            mtx.vin.push_back(CTxIn(COutPoint(), consensus.stakeBaseSigScript));
            mtx.vin.push_back(CTxIn(COutPoint(ticketHash, ticketStakeOutputIndex)));

            // outputs

            VoteData voteData{1, hash, static_cast<uint32_t>(height), 0x0001, 0};
            CScript declScript = GetScriptForVoteDecl(voteData);
            mtx.vout.push_back(CTxOut(0, declScript));

            CScript rewardScript = GetScriptForDestination(static_cast<CKeyID>(ticketContribData.rewardAddr));
            mtx.vout.push_back(CTxOut(reward, rewardScript));

            // signature

            std::map<uint256, CWalletTx>::const_iterator mi = wallet->mapWallet.find(mtx.vin[1].prevout.hash);
            BOOST_CHECK(mi != wallet->mapWallet.end());
            BOOST_CHECK(mtx.vin[1].prevout.n < mi->second.tx->vout.size());

            CTransaction tx(mtx);
            const CScript& scriptPubKey = mi->second.tx->vout[tx.vin[1].prevout.n].scriptPubKey;
            const CAmount& amount = mi->second.tx->vout[tx.vin[1].prevout.n].nValue;
            SignatureData sigdata;
            BOOST_CHECK(ProduceSignature(TransactionSignatureCreator(wallet.get(), &tx, 1, amount, SIGHASH_ALL), scriptPubKey, sigdata));
            UpdateTransaction(mtx, 1, sigdata);

            // commitment

            CWalletTx wtx;
            CValidationState state;
            wtx.fTimeReceivedIsTxTime = true;
            wtx.BindWallet(wallet.get());
            wtx.SetTx(MakeTransactionRef(std::move(mtx)));
            CReserveKey reservekey{wallet.get()};
            BOOST_CHECK(wallet->CommitTransaction(wtx, reservekey, g_connman.get(), state));

            voteHashes.push_back(wtx.GetHash());
        }

        return voteHashes;
    }

    bool MempoolHasTicket() {
        std::vector<TxMempoolInfo> txnInfos = mempool.infoAll();
        for (size_t j = 0; j < txnInfos.size(); ++j)
            if (ParseTxClass(*(txnInfos[j].tx)) == TX_BuyTicket)
                return true;

        return false;
    }

    bool MempoolHasEnoughTicketsForBlock() {
        int count{0};
        std::vector<TxMempoolInfo> txnInfos = mempool.infoAll();
        for (size_t j = 0; j < txnInfos.size(); ++j)
            if (ParseTxClass(*(txnInfos[j].tx)) == TX_BuyTicket)
                if ((++count) >= Params().GetConsensus().nTicketsPerBlock)
                    return true;

        return false;
    }

    CBlock CreateAndProcessBlock(const std::vector<CTransaction>& txns, const CScript& scriptPubKey)
    {
        const CChainParams& chainparams = Params();
        std::unique_ptr<CBlockTemplate> pblocktemplate = BlockAssembler(chainparams).CreateNewBlock(scriptPubKey);
        CBlock& block = pblocktemplate->block;

        // Replace mempool-selected txns with just coinbase plus passed-in txns:
        for (const CTransaction& tx : txns)
            block.vtx.push_back(MakeTransactionRef(tx));
        // IncrementExtraNonce creates a valid coinbase and merkleRoot
        unsigned int extraNonce = 0;
        IncrementExtraNonce(&block, chainActive.Tip(), extraNonce);

        while (!CheckProofOfWork(block.GetHash(), block.nBits, chainparams.GetConsensus())) ++block.nNonce;

        std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(block);
        ProcessNewBlock(chainparams, shared_pblock, true, nullptr);

        CBlock result = block;
        return result;
    }

    void ExtendChain(int blockCount)
    {
        if (blockCount <= 0)
            return;

        LOCK2(cs_main, wallet->cs_wallet);

        bool shouldRelock = wallet->IsLocked();
        if (shouldRelock)
            BOOST_CHECK(wallet->Unlock(passphrase));

        const Consensus::Params& consensus = Params().GetConsensus();

        CScript coinbaseScriptPubKey = coinbaseTxns[0].vout[0].scriptPubKey;

        latestTestTxns.clear();

        std::vector<CTransaction> txns;

        if (blockCount > 0) {
            std::vector<TxMempoolInfo> txnInfos = mempool.infoAll();
            for (size_t j = 0; j < txnInfos.size(); ++j)
                txns.push_back(*(txnInfos[j].tx));
        }

        for (int i = 0; i < blockCount; ++i) {
            if (chainActive.Tip()->nHeight >= consensus.nStakeValidationHeight - 1 - consensus.nTicketMaturity - 1)
                while (!MempoolHasEnoughTicketsForBlock())
                    AddTicketTx();

            if (chainActive.Tip()->nHeight >= consensus.nStakeValidationHeight - 1)
                AddVoteTxs();

            CBlock b = CreateAndProcessBlock({}, coinbaseScriptPubKey);

            coinbaseTxns.push_back(*b.vtx[0]);
        }

        if (blockCount > 0) {
            for (size_t i = 0; i < txns.size(); i++)
                latestTestTxns.push_back(txns[i]);
        }

        if (shouldRelock)
            BOOST_CHECK(wallet->Lock());
    }

    const SecureString passphrase = "aV3rySecurePassword!";
    std::unique_ptr<CWallet> wallet;
    std::vector<CTransaction> latestTestTxns;
};

BOOST_FIXTURE_TEST_SUITE(ticket_purchase_tests, TicketPurchaseTestingSetup)

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
BOOST_AUTO_TEST_CASE(ticket_vote_or_revoke_fee_limits)
{
    struct TestData {
        uint8_t voteFees;
        uint8_t revokeFees;

        bool expectedHasVoteFeeLimits;
        bool expectedHasRevokeFeeLimits;

        CAmount expectedVoteFeeLimits;
        CAmount expectedRevokeFeeLimits;
    };

    std::vector<TestData> tests {
        {  0,   0,  true,  true,          0x01,            0x01},
        {  1,   1,  true,  true,          0x02,            0x02},
        {  2,   2,  true,  true,          0x04,            0x04},
        { 10,  11,  true,  true,         0x400,           0x800},
        { 16,  32,  true,  true,       0x10000,     0x100000000},
        { 40,  50,  true,  true, 0x10000000000, 0x4000000000000},
        { 60,  62,  true,  true,     MAX_MONEY,       MAX_MONEY},
        { 63,  63,  true,  true,     MAX_MONEY,       MAX_MONEY},
        // values below should throw exceptions on expected fee limits
        { 64,  64, false, false,             0,               0},
        { 65,  65, false, false,             0,               0},
        {100, 100, false, false,             0,               0},
        {127, 127, false, false,             0,               0},
        {128, 128, false, false,             0,               0},
        {255, 255, false, false,             0,               0}
    };

    for (auto& test: tests) {
        TicketContribData tcd{1, CKeyID(), 0LL, test.voteFees, test.revokeFees};

        BOOST_CHECK_EQUAL(tcd.hasVoteFeeLimits(), test.expectedHasVoteFeeLimits);
        BOOST_CHECK_EQUAL(tcd.hasRevokeFeeLimits(), test.expectedHasRevokeFeeLimits);

        if (test.voteFees <= TicketContribData::MaxFees) {
            BOOST_CHECK_EQUAL(tcd.voteFeeLimits(), test.expectedVoteFeeLimits);
            BOOST_CHECK_NO_THROW(tcd.voteFeeLimits());

            if (test.expectedVoteFeeLimits < MAX_MONEY)
                BOOST_CHECK_EQUAL(TicketContribData::feeFromAmount(test.expectedVoteFeeLimits), test.voteFees);
        } else {
            BOOST_CHECK_THROW(tcd.voteFeeLimits(), std::range_error);
        }

        if (test.revokeFees <= TicketContribData::MaxFees) {
            BOOST_CHECK_EQUAL(tcd.revokeFeeLimits(), test.expectedRevokeFeeLimits);
            BOOST_CHECK_NO_THROW(tcd.revokeFeeLimits());

            if (test.expectedRevokeFeeLimits < MAX_MONEY)
                BOOST_CHECK_EQUAL(TicketContribData::feeFromAmount(test.expectedRevokeFeeLimits), test.revokeFees);
        } else {
            BOOST_CHECK_THROW(tcd.revokeFeeLimits(), std::range_error);
        }
    }

    BOOST_CHECK_EQUAL(TicketContribData::feeFromAmount(0), 0);

    BOOST_CHECK_EQUAL(TicketContribData::feeFromAmount(-1), 0);
    BOOST_CHECK_EQUAL(TicketContribData::feeFromAmount(-10), 0);
    BOOST_CHECK_EQUAL(TicketContribData::feeFromAmount(-1000), 0);
    BOOST_CHECK_EQUAL(TicketContribData::feeFromAmount(std::numeric_limits<int64_t>().min()), 0);

    BOOST_CHECK_EQUAL(TicketContribData::feeFromAmount(MAX_MONEY), TicketContribData::MaxFees);
    BOOST_CHECK_EQUAL(TicketContribData::feeFromAmount(MAX_MONEY+1), TicketContribData::MaxFees);
    BOOST_CHECK_EQUAL(TicketContribData::feeFromAmount(std::numeric_limits<int64_t>().max()), TicketContribData::MaxFees);
}

// test the serialization and deserialization of the ticket contribution data
BOOST_AUTO_TEST_CASE(ticket_contrib_data_serialization)
{
    struct TestData {
        TicketContribData tcd;

        CScript expectedScript;
        bool expectedParseStatus;
        TicketContribData expectedTcd;
    };

    CPubKey pubKey;
    BOOST_CHECK(wallet->GetKeyFromPool(pubKey));
    const CTxDestination addr = pubKey.GetID();

    auto scriptBuilder = [](TicketContribData tcd) -> CScript {
        return CScript() << OP_RETURN << OP_STRUCT << 1 << CLASS_Staking << STAKE_TicketContribution
                         << 1
                         << ToByteVector(tcd.rewardAddr) << tcd.whichAddr
                         << tcd.contributedAmount
                         << tcd.voteFees
                         << tcd.revokeFees;
    };

    std::vector<TestData> tests {
        {
            TicketContribData(1, addr, 5*COIN, 10, 0),
            scriptBuilder(TicketContribData(1, addr, 5*COIN, 10, 0)),
            true,
            TicketContribData(1, addr, 5*COIN, 10, 0)
        },
        {
            TicketContribData(2, addr, 10*COIN, 0, 10),
            scriptBuilder(TicketContribData(2, addr, 10*COIN, 0, 10)),
            true,
            TicketContribData(1, addr, 10*COIN, 0, 10)
        },
        {
            TicketContribData(3, addr, 20*COIN, 20, TicketContribData::NoFees),
            scriptBuilder(TicketContribData(3, addr, 20*COIN, 20, TicketContribData::NoFees)),
            true,
            TicketContribData(1, addr, 20*COIN, 20, TicketContribData::NoFees)
        },
        {
            TicketContribData(4, addr, 30*COIN, TicketContribData::NoFees, 30),
            scriptBuilder(TicketContribData(4, addr, 30*COIN, TicketContribData::NoFees, 30)),
            true,
            TicketContribData(1, addr, 30*COIN, TicketContribData::NoFees, 30)
        },
        {
            TicketContribData(5, addr, 40*COIN, 15, 25),
            scriptBuilder(TicketContribData(5, addr, 40*COIN, 15, 25)),
            true,
            TicketContribData(1, addr, 40*COIN, 15, 25)
        },
        {
            TicketContribData(6, addr, 100*COIN, 40, 30),
            scriptBuilder(TicketContribData(6, addr, 100*COIN, 40, 30)),
            true,
            TicketContribData(1, addr, 100*COIN, 40, 30)
        }
    };

    CMutableTransaction tx;
    std::vector<unsigned char> serializedTxOut;
    TicketContribData deserializedTcd;

    for (auto& test: tests) {
        BOOST_CHECK(GetScriptForTicketContrib(test.tcd) == test.expectedScript);

        tx.vout.clear();

        CTxOut txOut{0, GetScriptForTicketContrib(test.tcd)};
        tx.vout.push_back(txOut);
        BOOST_CHECK(ParseTicketContrib(tx, 0, deserializedTcd));

        BOOST_CHECK(deserializedTcd == test.expectedTcd);
    }
}

// test the ticket purchase estimated sizes of inputs and outputs
BOOST_AUTO_TEST_CASE(ticket_purchase_estimated_sizes)
{
    BOOST_CHECK_EQUAL(GetEstimatedP2PKHTxInSize(), static_cast<size_t>(148));
    BOOST_CHECK_EQUAL(GetEstimatedP2PKHTxInSize(false), static_cast<size_t>(180));

    BOOST_CHECK_EQUAL(GetEstimatedP2PKHTxOutSize(), static_cast<size_t>(34));

    BOOST_CHECK_EQUAL(GetEstimatedBuyTicketDeclTxOutSize(), static_cast<size_t>(16));

    BOOST_CHECK_EQUAL(GetEstimatedTicketContribTxOutSize(), static_cast<size_t>(50));

    BOOST_CHECK_EQUAL(GetEstimatedSizeOfBuyTicketTx(true), static_cast<size_t>(524));
    BOOST_CHECK_EQUAL(GetEstimatedSizeOfBuyTicketTx(false), static_cast<size_t>(292));
}

// test the split transaction for funding a ticket purchase
BOOST_FIXTURE_TEST_CASE(ticket_purchase_split_transaction, TicketPurchaseTestingSetup)
{
    uint256 splitTxHash;
    CWalletError we;
    CAmount neededPerTicket{0};

    LOCK2(cs_main, wallet->cs_wallet);

    // Coin availability

    std::tie(splitTxHash, we) = wallet.get()->CreateTicketPurchaseSplitTx("", 1000000 * COIN, 1 * COIN);
    BOOST_CHECK_EQUAL(we.code, CWalletError::WALLET_INSUFFICIENT_FUNDS);

    std::tie(splitTxHash, we) = wallet.get()->CreateTicketPurchaseSplitTx("", 100000 * COIN, 1 * COIN);
    BOOST_CHECK_EQUAL(we.code, CWalletError::WALLET_INSUFFICIENT_FUNDS);

    std::tie(splitTxHash, we) = wallet.get()->CreateTicketPurchaseSplitTx("", 10000 * COIN, 1 * COIN);
    BOOST_CHECK_EQUAL(we.code, CWalletError::WALLET_INSUFFICIENT_FUNDS);

    // Single ticket

    std::tie(splitTxHash, we) = wallet.get()->CreateTicketPurchaseSplitTx("", 100 * COIN, 1 * COIN);
    BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);

    const CWalletTx* splitTxSingle = wallet.get()->GetWalletTx(splitTxHash);
    BOOST_CHECK(splitTxSingle != nullptr);

    BOOST_CHECK_EQUAL(splitTxSingle->tx->vin.size(), static_cast<size_t>(1));

    BOOST_CHECK_EQUAL(splitTxSingle->tx->vout.size(), static_cast<size_t>(2)); // ticket + change

    neededPerTicket = 100 * COIN + 1 * COIN; // ticket price + ticket fee

    BOOST_CHECK_EQUAL(splitTxSingle->tx->vout[0].nValue, neededPerTicket);

    // Multiple tickets

    std::tie(splitTxHash, we) = wallet.get()->CreateTicketPurchaseSplitTx("", 10 * COIN, 1 * COIN, 0, 10);
    BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);

    const CWalletTx* splitTxMultiple = wallet.get()->GetWalletTx(splitTxHash);
    BOOST_CHECK(splitTxMultiple != nullptr);

    BOOST_CHECK_EQUAL(splitTxMultiple->tx->vout.size(), static_cast<size_t>(10 + 1)); // tickets + change

    neededPerTicket = 10 * COIN + 1 * COIN; // ticket price + ticket fee

    for (size_t i = 0; i < 10; ++i)
        BOOST_CHECK_EQUAL(splitTxMultiple->tx->vout[i].nValue, neededPerTicket);

    // Use VSP (single ticket)

    std::tie(splitTxHash, we) = wallet.get()->CreateTicketPurchaseSplitTx("", 100 * COIN, 1 * COIN, 20 * COIN);
    BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);

    const CWalletTx* splitTxVspSingle = wallet.get()->GetWalletTx(splitTxHash);
    BOOST_CHECK(splitTxVspSingle != nullptr);

    BOOST_CHECK_EQUAL(splitTxVspSingle->tx->vout.size(), static_cast<size_t>(2 + 1)); // user + VSP + change

    neededPerTicket = 100 * COIN + 1 * COIN; // ticket price + ticket fee

    BOOST_CHECK_EQUAL(splitTxVspSingle->tx->vout[0].nValue, 20 * COIN); // VSP fee
    BOOST_CHECK_EQUAL(splitTxVspSingle->tx->vout[1].nValue, neededPerTicket - 20 * COIN); // user (needed per ticket - VSP)

    // Use VSP (multiple tickets)

    std::tie(splitTxHash, we) = wallet.get()->CreateTicketPurchaseSplitTx("", 10 * COIN, 1 * COIN, 5 * COIN, 10);
    BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);

    const CWalletTx* splitTxVspMultiple = wallet.get()->GetWalletTx(splitTxHash);
    BOOST_CHECK(splitTxVspMultiple != nullptr);

    BOOST_CHECK_EQUAL(splitTxVspMultiple->tx->vout.size(), static_cast<size_t>(10 * 2 + 1)); // users + VSPs + change

    neededPerTicket = 10 * COIN + 1 * COIN; // ticket price + ticket fee

    for (size_t i = 0; i < 10; ++i) {
        BOOST_CHECK_EQUAL(splitTxVspMultiple->tx->vout[2*i].nValue, 5 * COIN); // VSP fee
        BOOST_CHECK_EQUAL(splitTxVspMultiple->tx->vout[2*i+1].nValue, neededPerTicket - 5 * COIN); // user (needed per ticket - VSP)
    }

    // Fee rate

    std::tie(splitTxHash, we) = wallet.get()->CreateTicketPurchaseSplitTx("", 10 * COIN, 1 * COIN, 5 * COIN, 10, 1 * COIN);
    BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);

    const CWalletTx* splitTxFeeRate = wallet.get()->GetWalletTx(splitTxHash);
    BOOST_CHECK(splitTxFeeRate != nullptr);

    CFeeRate feeRate{1 * COIN};

    CAmount fee;
    std::string account;
    std::list<COutputEntry> received;
    std::list<COutputEntry> sent;
    splitTxFeeRate->GetAmounts(received, sent, fee, account, ISMINE_ALL);

    BOOST_CHECK_LE(feeRate.GetFee(static_cast<size_t>(GetVirtualTransactionSize(*(splitTxFeeRate->tx)))), fee);
}

// test the transaction for purchasing tickets
BOOST_FIXTURE_TEST_CASE(ticket_purchase_transaction, TicketPurchaseTestingSetup)
{
    std::vector<std::string> txHashes;
    CWalletError we;

    LOCK2(cs_main, wallet->cs_wallet);

    CPubKey ticketPubKey;
    BOOST_CHECK(wallet->GetKeyFromPool(ticketPubKey));
    const CTxDestination ticketKeyId = ticketPubKey.GetID();
    const std::string ticketAddress{EncodeDestination(ticketKeyId)};

    CPubKey vspPubKey;
    BOOST_CHECK(wallet->GetKeyFromPool(vspPubKey));
    const CTxDestination vspKeyId = vspPubKey.GetID();
    const std::string vspAddress{EncodeDestination(vspKeyId)};

    const CAmount spendLimit{100000 * COIN};
    const CAmount feeRate{1 * COIN};
    const double vspFeePercent{5.0};

    // validate a single ticket, with or without VSP fee
    auto checkTicket = [&](uint256 txHash, bool useVsp) {
        // Transaction

        BOOST_CHECK(txHash != uint256());

        const CWalletTx* wtx = wallet->GetWalletTx(txHash);
        BOOST_CHECK(wtx != nullptr);

        BOOST_CHECK(wtx->tx != nullptr);
        const CTransaction& tx = *(wtx->tx);

        // Inputs

        BOOST_CHECK_EQUAL(tx.vin.size(), static_cast<size_t>(useVsp ? 2 : 1)); // (vsp) + user

        // Outputs

        BOOST_CHECK_EQUAL(tx.vout.size(), static_cast<size_t>(useVsp ? 6 : 4)); // declaration + stake + (vsp commitment + vsp change +) user commitment + user change

        // check ticket declaration
        BOOST_CHECK_EQUAL(ParseTxClass(tx), TX_BuyTicket);
        BOOST_CHECK_EQUAL(tx.vout[0].nValue, 0);

        // make sure that the stake address and value are correct
        int64_t ticketPrice = CalculateNextRequiredStakeDifficulty(chainActive.Tip(), Params().GetConsensus());
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
            BOOST_CHECK_EQUAL(tcd.voteFees, TicketContribData::NoFees);
            BOOST_CHECK_EQUAL(tcd.revokeFees, TicketContribData::NoFees);
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
        BOOST_CHECK_EQUAL(tcd.voteFees, TicketContribData::NoFees);
        BOOST_CHECK_EQUAL(tcd.revokeFees, TicketContribData::NoFees);
        BOOST_CHECK_EQUAL(tcd.whichAddr, 1);
        BOOST_CHECK(wallet->IsMine(CTxOut(0, GetScriptForDestination(CKeyID(uint160(tcd.rewardAddr))))));
        BOOST_CHECK_EQUAL(tx.vout[userOutput].nValue, 0);

        BOOST_CHECK(wallet->IsMine(tx.vout[userOutput+1]));
        BOOST_CHECK_EQUAL(tx.vout[userOutput+1].nValue, 0);
    };

    // Coin availability
    {
        txHashes.clear();

        std::tie(txHashes, we) = wallet->PurchaseTicket("", 100000 * COIN, 1, ticketAddress, 10000, "", 0.0, 0, feeRate);
        BOOST_CHECK_EQUAL(we.code, CWalletError::WALLET_INSUFFICIENT_FUNDS);

        std::tie(txHashes, we) = wallet->PurchaseTicket("", 100 * COIN, 1, ticketAddress, 10000, "", 0.0, 0, feeRate);
        BOOST_CHECK_EQUAL(we.code, CWalletError::WALLET_INSUFFICIENT_FUNDS);
    }

    ExtendChain(Params().GetConsensus().nStakeValidationHeight + 1 - chainActive.Height());

    // Single purchase, no VSP
    {
        txHashes.clear();

        std::tie(txHashes, we) = wallet->PurchaseTicket("", spendLimit, 1, ticketAddress, 1, "", 0.0, 0, feeRate);
        BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);

        BOOST_CHECK_EQUAL(txHashes.size(), static_cast<size_t>(1));

        uint256 txHash = uint256S(txHashes[0]);

        checkTicket(txHash, false);
    }

    // Single purchase, with VSP
    {
        txHashes.clear();

        std::tie(txHashes, we) = wallet->PurchaseTicket("", spendLimit, 1, ticketAddress, 1, vspAddress, vspFeePercent, 0, feeRate);
        BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);

        BOOST_CHECK_EQUAL(txHashes.size(), static_cast<size_t>(1));

        uint256 txHash = uint256S(txHashes[0]);

        checkTicket(txHash, true);
    }

    // Multiple purchase, no VSP
    {
        txHashes.clear();

        unsigned int numTickets{10};

        std::tie(txHashes, we) = wallet->PurchaseTicket("", spendLimit, 1, ticketAddress, numTickets, "", 0.0, 0, feeRate);
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

        std::tie(txHashes, we) = wallet->PurchaseTicket("", spendLimit, 1, ticketAddress, numTickets, vspAddress, vspFeePercent, 0, feeRate);
        BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);

        BOOST_CHECK_EQUAL(txHashes.size(), static_cast<size_t>(numTickets));

        for (unsigned int i = 0; i < numTickets; ++i) {
            uint256 txHash = uint256S(txHashes[i]);
            checkTicket(txHash, true);
        }
    }
}

void CheckTicketPurchase(const CTransaction& tx, std::vector<TicketContribData> ticketContribDatas, bool validateAmount = false)
{
    std::string reason;

    BOOST_CHECK_MESSAGE(ValidateBuyTicketStructure(tx, reason), ((reason.size() > 0) ? reason : "ValidateBuyTicketStructure"));

    for (uint32_t i = 2; i < ticketContribDatas.size(); ++i) {
        TicketContribData tcd;
        TicketContribData expectedTcd = ticketContribDatas[i];

        BOOST_CHECK(ParseTicketContrib(tx, 2*i, tcd));

        BOOST_CHECK_EQUAL(tcd.nVersion, expectedTcd.nVersion);
        BOOST_CHECK(tcd.rewardAddr == expectedTcd.rewardAddr);
        BOOST_CHECK_EQUAL(tcd.whichAddr, expectedTcd.whichAddr);
        if (validateAmount)
            BOOST_CHECK_EQUAL(tcd.contributedAmount, expectedTcd.contributedAmount);
        BOOST_CHECK_EQUAL(tcd.voteFees, expectedTcd.voteFees);
        BOOST_CHECK_EQUAL(tcd.revokeFees, expectedTcd.revokeFees);
    }
}

// test the ticket buyer
BOOST_FIXTURE_TEST_CASE(ticket_buyer, TicketPurchaseTestingSetup)
{
    CPubKey ticketPubKey;
    CPubKey vspPubKey;

    {
        LOCK2(cs_main, wallet->cs_wallet);
        BOOST_CHECK(wallet->GetKeyFromPool(ticketPubKey));
        BOOST_CHECK(wallet->GetKeyFromPool(vspPubKey));
    }

    const CTxDestination ticketKeyId = ticketPubKey.GetID();
    const std::string ticketAddress{EncodeDestination(ticketKeyId)};

    const CTxDestination vspKeyId = vspPubKey.GetID();
    const std::string vspAddress{EncodeDestination(vspKeyId)};

    ExtendChain(Params().GetConsensus().nStakeEnabledHeight + 1 - chainActive.Height());

    BOOST_CHECK_GE(chainActive.Tip()->nTx, 1);
    BOOST_CHECK_GE(chainActive.Tip()->nFreshStake, 0);

    CTicketBuyer* tb = wallet->GetTicketBuyer();
    BOOST_CHECK(tb != nullptr);

    BOOST_CHECK(!tb->isStarted());

    CTicketBuyerConfig& cfg = tb->GetConfig();

    BOOST_CHECK_EQUAL(cfg.buyTickets, false);

    cfg.account = "";
    cfg.votingAccount = "";
    cfg.votingAddress = ticketAddress;
    cfg.poolFeeAddress = "";
    cfg.poolFees = 0.0;
    cfg.limit = 1;
    cfg.passphrase = "";

    // Coin availability

    cfg.maintain = wallet->GetBalance() - 1000;

    tb->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    ExtendChain(1);

    BOOST_CHECK_GE(chainActive.Tip()->nTx, 1);
    BOOST_CHECK_GE(chainActive.Tip()->nFreshStake, 0);

    // Single ticket, no VSP

    tb->stop();

    cfg.maintain = 0;

    tb->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ExtendChain(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ExtendChain(1);

    BOOST_CHECK_GE(chainActive.Tip()->nTx, 1 + 1 + 1);
    BOOST_CHECK_GE(chainActive.Tip()->nFreshStake, 1);

    BOOST_CHECK_EQUAL(latestTestTxns.size(), 2);

    BOOST_CHECK_EQUAL(ParseTxClass(latestTestTxns[0]), TX_Regular);
    BOOST_CHECK_EQUAL(latestTestTxns[0].vout.size(), 1 + 1);

    // TODO: Add amount validation
    CheckTicketPurchase(latestTestTxns[1], {TicketContribData(1, ticketKeyId, 0, TicketContribData::NoFees, TicketContribData::NoFees)});

    // Single ticket, VSP

    tb->stop();

    ExtendChain(Params().GetConsensus().nStakeValidationHeight + 1 - chainActive.Height());
    ExtendChain(5); // since no purchases are made prior to the interval switch,
                    // make sure that the tests do not overlap with this switch.

    cfg.poolFeeAddress = vspAddress;
    cfg.poolFees = 5.0;

    tb->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ExtendChain(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ExtendChain(1);

    BOOST_CHECK_GE(chainActive.Tip()->nTx, 1 + 1 + 1);
    BOOST_CHECK_GE(chainActive.Tip()->nFreshStake, 1);

    BOOST_CHECK_EQUAL(ParseTxClass(latestTestTxns[0]), TX_Regular);
    BOOST_CHECK_EQUAL(latestTestTxns[0].vout.size(), 1 + 1 + 1);

    // TODO: Add amount validation
    CheckTicketPurchase(latestTestTxns[1], {TicketContribData(1, vspKeyId, 0, TicketContribData::NoFees, TicketContribData::NoFees), TicketContribData(1, ticketKeyId, 0, TicketContribData::NoFees, TicketContribData::NoFees)});

    // Multiple tickets, no VSP

    tb->stop();

    cfg.poolFeeAddress = "";
    cfg.poolFees = 0.0;
    cfg.limit = 5;

    tb->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ExtendChain(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ExtendChain(1);

    BOOST_CHECK_GE(chainActive.Tip()->nTx, 1 + 1 + cfg.limit);
    BOOST_CHECK_GE(chainActive.Tip()->nFreshStake, cfg.limit);

    BOOST_CHECK_EQUAL(ParseTxClass(latestTestTxns[0]), TX_Regular);
    BOOST_CHECK_EQUAL(latestTestTxns[0].vout.size(), cfg.limit + 1);

    // TODO: Add amount validation
    for (size_t i = 0; static_cast<int>(i) < cfg.limit; ++i)
        CheckTicketPurchase(latestTestTxns[1 + i], {TicketContribData(1, ticketKeyId, 0, TicketContribData::NoFees, TicketContribData::NoFees)});

    // Multiple ticket, VSP

    tb->stop();

    cfg.poolFeeAddress = vspAddress;
    cfg.poolFees = 5.0;

    tb->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ExtendChain(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ExtendChain(1);

    BOOST_CHECK_GE(chainActive.Tip()->nTx, 1 + 1 + cfg.limit);
    BOOST_CHECK_GE(chainActive.Tip()->nFreshStake, cfg.limit);

    BOOST_CHECK_EQUAL(ParseTxClass(latestTestTxns[0]), TX_Regular);
    BOOST_CHECK_EQUAL(latestTestTxns[0].vout.size(), 2 * cfg.limit + 1);

    // TODO: Add amount validation
    for (size_t i = 0; static_cast<int>(i) < cfg.limit; ++i)
        CheckTicketPurchase(latestTestTxns[1 + i], {TicketContribData(1, vspKeyId, 0, TicketContribData::NoFees, TicketContribData::NoFees), TicketContribData(1, ticketKeyId, 0, TicketContribData::NoFees, TicketContribData::NoFees)});

    LOCK2(cs_main, wallet->cs_wallet);

    tb->stop();
}

// test the ticket buyer on encrypted wallet
BOOST_FIXTURE_TEST_CASE(ticket_buyer_encrypted, TicketPurchaseTestingSetup)
{
    CPubKey ticketPubKey;
    CPubKey vspPubKey;

    {
        LOCK2(cs_main, wallet->cs_wallet);
        BOOST_CHECK(wallet->GetKeyFromPool(ticketPubKey));
        BOOST_CHECK(wallet->GetKeyFromPool(vspPubKey));
        wallet->EncryptWallet(passphrase);
    }

    const CTxDestination ticketKeyId = ticketPubKey.GetID();
    const std::string ticketAddress{EncodeDestination(ticketKeyId)};

    const CTxDestination vspKeyId = vspPubKey.GetID();
    const std::string vspAddress{EncodeDestination(vspKeyId)};

    ExtendChain(Params().GetConsensus().nStakeValidationHeight + 1 - chainActive.Height() + 7); // since no purchases are made right before the interval switch,
                                                                                                // make sure that the tests do not overlap with this switch.

    CTicketBuyer* tb = wallet->GetTicketBuyer();
    BOOST_CHECK(tb != nullptr);

    BOOST_CHECK(!tb->isStarted());

    CTicketBuyerConfig& cfg = tb->GetConfig();

    BOOST_CHECK_EQUAL(cfg.buyTickets, false);

    cfg.account = "";
    cfg.votingAccount = "";
    cfg.votingAddress = ticketAddress;
    cfg.poolFeeAddress = vspAddress;
    cfg.poolFees = 5.0;
    cfg.limit = 5;
    cfg.passphrase = passphrase;

    BOOST_CHECK(wallet->IsLocked());

    tb->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ExtendChain(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ExtendChain(1);

    BOOST_CHECK_GE(chainActive.Tip()->nTx, 1 + 1 + cfg.limit);
    BOOST_CHECK_GE(chainActive.Tip()->nFreshStake, cfg.limit);

    BOOST_CHECK_EQUAL(ParseTxClass(latestTestTxns[0]), TX_Regular);
    BOOST_CHECK_EQUAL(latestTestTxns[0].vout.size(), 2 * cfg.limit + 1);

    // TODO: Add amount validation
    for (size_t i = 0; static_cast<int>(i) < cfg.limit; ++i)
        CheckTicketPurchase(latestTestTxns[1 + i], {TicketContribData(1, vspKeyId, 0, TicketContribData::NoFees, TicketContribData::NoFees), TicketContribData(1, ticketKeyId, 0, TicketContribData::NoFees, TicketContribData::NoFees)});

    LOCK2(cs_main, wallet->cs_wallet);

    tb->stop();
}

UniValue CallRPC(std::string args)
{
    std::vector<std::string> vArgs;
    boost::split(vArgs, args, boost::is_any_of(" \t"));
    std::string strMethod = vArgs[0];
    vArgs.erase(vArgs.begin());
    JSONRPCRequest request;
    request.strMethod = strMethod;
    request.params = RPCConvertValues(strMethod, vArgs);
    request.fHelp = false;
    BOOST_CHECK(tableRPC[strMethod]);
    rpcfn_type method = tableRPC[strMethod]->actor;
    try {
        UniValue result = (*method)(request);
        return result;
    }
    catch (const UniValue& objError) {
        throw std::runtime_error(find_value(objError, "message").get_str());
    }
}

// test the ticket buyer RPCs
BOOST_FIXTURE_TEST_CASE(ticket_buyer_rpc, TicketPurchaseTestingSetup)
{
    vpwallets.insert(vpwallets.begin(), wallet.get());

    RegisterWalletRPCCommands(tableRPC);

    CPubKey ticketPubKey;
    BOOST_CHECK(wallet->GetKeyFromPool(ticketPubKey));
    const CTxDestination ticketKeyId = ticketPubKey.GetID();
    const std::string ticketAddress{EncodeDestination(ticketKeyId)};

    CPubKey vspPubKey;
    BOOST_CHECK(wallet->GetKeyFromPool(vspPubKey));
    const CTxDestination vspKeyId = vspPubKey.GetID();
    const std::string vspAddress{EncodeDestination(vspKeyId)};

    CTicketBuyer* tb = wallet->GetTicketBuyer();
    BOOST_CHECK(tb != nullptr);

    BOOST_CHECK(!tb->isStarted());

    CTicketBuyerConfig& cfg = tb->GetConfig();

    // Settings (write)

    BOOST_CHECK_THROW(CallRPC("setticketbuyeraccount"), std::runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("setticketbuyeraccount abc"));

    BOOST_CHECK_THROW(CallRPC("setticketbuyerbalancetomaintain"), std::runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("setticketbuyerbalancetomaintain 123"));

    BOOST_CHECK_THROW(CallRPC("setticketbuyervotingaddress"), std::runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("setticketbuyervotingaddress " + ticketAddress));

    BOOST_CHECK_THROW(CallRPC("setticketbuyerpooladdress"), std::runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("setticketbuyerpooladdress " + vspAddress));

    BOOST_CHECK_THROW(CallRPC("setticketbuyerpoolfees"), std::runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("setticketbuyerpoolfees 5.0"));

    BOOST_CHECK_THROW(CallRPC("setticketbuyermaxperblock"), std::runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("setticketbuyermaxperblock 5"));

    BOOST_CHECK_EQUAL(cfg.buyTickets, false);
    BOOST_CHECK_EQUAL(cfg.account, "abc");
    BOOST_CHECK_EQUAL(cfg.maintain, 12300000000);
    BOOST_CHECK_EQUAL(cfg.votingAccount, "");
    BOOST_CHECK_EQUAL(cfg.votingAddress, ticketAddress);
    BOOST_CHECK_EQUAL(cfg.poolFeeAddress, vspAddress);
    BOOST_CHECK_EQUAL(cfg.poolFees, 5.0);
    BOOST_CHECK_EQUAL(cfg.limit, 5);

    // Settings (read)

    UniValue r;
    BOOST_CHECK_NO_THROW(r = CallRPC("ticketbuyerconfig"));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "buyTickets").get_bool(), false);
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "account").get_str(), "abc");
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "maintain").get_int64(), 12300000000);
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "votingAccount").get_str(), "");
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "votingAddress").get_str(), ticketAddress);
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "poolFeeAddress").get_str(), vspAddress);
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "poolFees").get_real(), 5.0);
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "limit").get_int(), 5);
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "minConf").get_int(), 1);

    // Start (with minimal settings)

    BOOST_CHECK_THROW(CallRPC("startticketbuyer"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("startticketbuyer \"\""), std::runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("startticketbuyer def 124"));

    BOOST_CHECK_EQUAL(cfg.buyTickets, true);
    BOOST_CHECK_EQUAL(cfg.account, "def");
    BOOST_CHECK_EQUAL(cfg.maintain, 12400000000);
    BOOST_CHECK_EQUAL(cfg.votingAccount, "");
    BOOST_CHECK_EQUAL(cfg.votingAddress, "");
    BOOST_CHECK_EQUAL(cfg.poolFeeAddress, "");
    BOOST_CHECK_EQUAL(cfg.poolFees, 0.0);
    BOOST_CHECK_EQUAL(cfg.limit, 1);

    // Start (with full settings)

    tb->stop();

    BOOST_CHECK_EQUAL(cfg.buyTickets, false);

    BOOST_CHECK_NO_THROW(CallRPC(std::string("startticketbuyer fromaccount 125 \"\" votingaccount ") + ticketAddress + " " + vspAddress + " 10.0 8"));

    BOOST_CHECK_EQUAL(cfg.buyTickets, true);
    BOOST_CHECK_EQUAL(cfg.account, "fromaccount");
    BOOST_CHECK_EQUAL(cfg.votingAccount, "votingaccount");
    BOOST_CHECK_EQUAL(cfg.maintain, 12500000000);
    BOOST_CHECK_EQUAL(cfg.votingAddress, ticketAddress);
    BOOST_CHECK_EQUAL(cfg.poolFeeAddress, vspAddress);
    BOOST_CHECK_EQUAL(cfg.poolFees, 10.0);
    BOOST_CHECK_EQUAL(cfg.limit, 8);

    // Stop

    BOOST_CHECK_NO_THROW(CallRPC("stopticketbuyer"));

    BOOST_CHECK_EQUAL(cfg.buyTickets, false);

    // Start with passphrase

    tb->stop();

    BOOST_CHECK_EQUAL(cfg.buyTickets, false);

    wallet->EncryptWallet(passphrase);
    BOOST_CHECK(wallet->IsLocked());

    BOOST_CHECK_THROW(CallRPC("startticketbuyer def 124"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("startticketbuyer def 124 wrongP4ssword!"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("startticketbuyer def 124 wrongP4ssword votingaccount " + ticketAddress + " " + vspAddress + " 10.0 8"), std::runtime_error);

    BOOST_CHECK_NO_THROW(CallRPC(std::string("startticketbuyer fromaccount 125 ") + std::string(passphrase.c_str()) + " votingaccount " + ticketAddress + " " + vspAddress + " 10.0 8"));

    BOOST_CHECK_EQUAL(cfg.buyTickets, true);
    BOOST_CHECK_EQUAL(cfg.account, "fromaccount");
    BOOST_CHECK_EQUAL(cfg.votingAccount, "votingaccount");
    BOOST_CHECK_EQUAL(cfg.maintain, 12500000000);
    BOOST_CHECK_EQUAL(cfg.votingAddress, ticketAddress);
    BOOST_CHECK_EQUAL(cfg.poolFeeAddress, vspAddress);
    BOOST_CHECK_EQUAL(cfg.poolFees, 10.0);
    BOOST_CHECK_EQUAL(cfg.limit, 8);
    BOOST_CHECK_EQUAL(cfg.passphrase, passphrase);

    tb->stop();
}

BOOST_AUTO_TEST_SUITE_END()
