// Copyright (c) 2020 The Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "test/test_paicoin.h"
#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string.hpp>

#include "wallet/wallet.h"
#include "stake/staketx.h"
#include "validation.h"
#include "wallet/coincontrol.h"
#include "rpc/server.h"
#include "rpc/client.h"
#include "net.h"
#include "miner.h"
#include <univalue.h>
#include <vector>

static void AddKey(CWallet& wallet, const CKey& key)
{
    LOCK(wallet.cs_wallet);
    wallet.AddKeyPubKey(key, key.GetPubKey());
}

class VoteTestingSetup : public TestChain100Setup
{
public:
    VoteTestingSetup() :
        TestChain100Setup(scriptPubKeyType::P2PKH),
        validLargeEvbVector(ExtendedVoteBits::maxSize(), 0xab),
        invalidLargeEvbVector(ExtendedVoteBits::maxSize() + 1, 0xab)
    {
        for (unsigned i = 0; i < ExtendedVoteBits::maxSize(); ++i)
            validLargeEvbString += "ab";
        invalidLargeEvbString = validLargeEvbString;
        invalidLargeEvbString += "ab";

        CreateAndProcessBlock(GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
        ::bitdb.MakeMock();
        wallet.reset(new CWallet(std::unique_ptr<CWalletDBWrapper>(new CWalletDBWrapper(&bitdb, "vote_test.dat"))));
        bool firstRun;
        wallet->LoadWallet(firstRun);
        AddKey(*wallet, coinbaseKey);
        wallet->ScanForWalletTransactions(chainActive.Genesis(), nullptr);
        wallet->SetBroadcastTransactions(true);
    }

    ~VoteTestingSetup()
    {
        wallet.reset();
        ::bitdb.Flush(true);
        ::bitdb.Reset();
    }

    uint256 SendMoney(const CTxDestination &address1, CAmount value1, const CTxDestination &address2 = CNoDestination(), CAmount value2 = 0)
    {
        BOOST_CHECK_NE(address1.which(), 0);
        BOOST_CHECK(MoneyRange(value1));

        CWalletTx wtx;
        CValidationState state;
        CAmount nFeeRequired;
        std::string strError;
        std::vector<CRecipient> vecSend;

        CRecipient recipient1 = {GetScriptForDestination(address1), value1, false};
        vecSend.push_back(recipient1);
        int nChangePosRet{1};

        if (address2.which() != 0) {
            BOOST_CHECK(MoneyRange(value2));
            CRecipient recipient2 = {GetScriptForDestination(address2), value2, false};
            vecSend.push_back(recipient2);
            nChangePosRet = 2;
        }

        CReserveKey reservekey{wallet.get()};
        BOOST_CHECK(wallet->CreateTransaction(vecSend, wtx, reservekey, nFeeRequired, nChangePosRet, strError, CCoinControl()));
        BOOST_CHECK(wallet->CommitTransaction(wtx, reservekey, g_connman.get(), state));
        BOOST_CHECK(state.IsValid());

        return wtx.GetHash();
    }

    uint256 AddTicketTx(bool useVsp = false, bool foreign = false)
    {
        const CBlockIndex* chainTip = chainActive.Tip();
        BOOST_CHECK_NE(chainTip, nullptr);

        // ticket settings

        CKey ticketKey;
        CPubKey ticketPubKey;
        CTxDestination ticketKeyId = CNoDestination();
        if (foreign) {
            ticketKey.MakeNewKey(true);
            ticketPubKey = ticketKey.GetPubKey();
            ticketKeyId = ticketPubKey.GetID();
        } else {
            wallet->GetKeyFromPool(ticketPubKey);
            ticketKeyId = ticketPubKey.GetID();
        }
        BOOST_CHECK_NE(ticketKeyId.which(), 0);

        CKey vspKey;
        CPubKey vspPubKey;
        CTxDestination vspKeyId = CNoDestination();
        if (useVsp) {
            vspKey.MakeNewKey(true);
            vspPubKey = vspKey.GetPubKey();
            vspKeyId = vspPubKey.GetID();
            BOOST_CHECK_NE(vspKeyId.which(), 0);
        }

        const CAmount ticketPrice = CalculateNextRequiredStakeDifficulty(chainTip, Params().GetConsensus());
        CFeeRate feeRate{10000};
        const CAmount ticketFee = feeRate.GetFee(GetEstimatedSizeOfBuyTicketTx(false));
        const CAmount contributedAmount = ticketPrice + ticketFee;

        const CAmount vspFeePercent = useVsp ? 20 : 0;
        const CAmount vspFee = contributedAmount * vspFeePercent / 100;

        BOOST_CHECK(MoneyRange(ticketPrice));
        BOOST_CHECK(MoneyRange(ticketFee));
        BOOST_CHECK(MoneyRange(contributedAmount));
        BOOST_CHECK(MoneyRange(vspFee));

        // split transaction

        uint256 splitTxHash;

        if (foreign) {
            if (useVsp)
                splitTxHash = SendMoney(vspKeyId, vspFee, ticketKeyId, contributedAmount - vspFee);
            else
                splitTxHash = SendMoney(ticketKeyId, contributedAmount);
        } else {
            CWalletError we;
            std::tie(splitTxHash, we) = wallet->CreateTicketPurchaseSplitTx("", ticketPrice, ticketFee, vspFee);
            BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);
        }
        BOOST_CHECK(splitTxHash != emptyHash);

        const CWalletTx* splitWTx = wallet->GetWalletTx(splitTxHash);
        BOOST_CHECK_NE(splitWTx, nullptr);
        BOOST_CHECK_NE(splitWTx->tx, nullptr);

        CTransactionRef splitTx = splitWTx->tx;
        if (useVsp) {
            BOOST_CHECK_GE(splitTx->vout.size(), 2U);
            BOOST_CHECK_GE(splitTx->vout[0].nValue, vspFee);
            BOOST_CHECK_GE(splitTx->vout[1].nValue, contributedAmount - vspFee);
        } else {
            BOOST_CHECK_GE(splitTx->vout.size(), 1U);
            BOOST_CHECK_GE(splitTx->vout[0].nValue, contributedAmount);
        }

        // ticket

        CMutableTransaction mtx;

        // input

        mtx.vin.push_back(CTxIn(splitTxHash, 0));
        if (useVsp)
            mtx.vin.push_back(CTxIn(splitTxHash, 1));

        // outputs

        BuyTicketData buyTicketData = {1};
        CScript declScript = GetScriptForBuyTicketDecl(buyTicketData);
        mtx.vout.push_back(CTxOut(0, declScript));

        CScript ticketScript = GetScriptForDestination(ticketKeyId);
        mtx.vout.push_back(CTxOut(ticketPrice, ticketScript));

        if (useVsp) {
            const CScript& multisigScript = GetScriptForMultisig(1, {vspPubKey, ticketPubKey});
            CScriptID multisigScriptId{multisigScript};

            TicketContribData ticketContribData{1, multisigScriptId, vspFee, TicketContribData::NoFees, TicketContribData::NoFees};
            CScript contributorInfoScript = GetScriptForTicketContrib(ticketContribData);
            mtx.vout.push_back(CTxOut(0, contributorInfoScript));

            CScript changeScript = GetScriptForDestination(multisigScriptId);
            mtx.vout.push_back(CTxOut(0, changeScript));
        }

        TicketContribData ticketContribData{1, ticketKeyId, contributedAmount - vspFee, TicketContribData::NoFees, TicketContribData::NoFees};
        CScript contributorInfoScript = GetScriptForTicketContrib(ticketContribData);
        mtx.vout.push_back(CTxOut(0, contributorInfoScript));

        CScript changeScript = GetScriptForDestination(ticketKeyId);
        mtx.vout.push_back(CTxOut(0, changeScript));

        // signature

        if (foreign) {
            CBasicKeyStore keyStore;

            if (useVsp)
                keyStore.AddKey(vspKey);

            keyStore.AddKey(ticketKey);

            CTransaction tx(mtx);

            SignatureData sigdata;
            BOOST_CHECK(ProduceSignature(TransactionSignatureCreator(&keyStore, &tx, 0, splitTx->vout[0].nValue, SIGHASH_ALL), splitTx->vout[0].scriptPubKey, sigdata));
            UpdateTransaction(mtx, 0, sigdata);

            if (useVsp) {
                sigdata.scriptSig.clear();
                sigdata.scriptWitness.SetNull();
                BOOST_CHECK(ProduceSignature(TransactionSignatureCreator(&keyStore, &tx, 1, splitTx->vout[1].nValue, SIGHASH_ALL), splitTx->vout[1].scriptPubKey, sigdata));
                UpdateTransaction(mtx, 1, sigdata);
            }
        } else {
            BOOST_CHECK(wallet->SignTransaction(mtx));
        }

        // commitment

        if (foreign) {
            BOOST_CHECK(mempool.addUnchecked(mtx.GetHash(), CTxMemPoolEntry(MakeTransactionRef(mtx), ticketFee, GetTime(), static_cast<unsigned int>(chainTip->nHeight+1), false, 0, LockPoints())));
            return mtx.GetHash();
        } else {
            CWalletTx wtx;
            CValidationState state;
            wtx.fTimeReceivedIsTxTime = true;
            wtx.BindWallet(wallet.get());
            wtx.SetTx(MakeTransactionRef(std::move(mtx)));
            CReserveKey reservekey{wallet.get()};
            BOOST_CHECK(wallet->CommitTransaction(wtx, reservekey, g_connman.get(), state));
            BOOST_CHECK(state.IsValid());
            return wtx.GetHash();
        }

        return emptyHash;
    }

    uint256 AddVoteTx(const uint256& ticketHash)
    {
        // block being voted

        const CBlockIndex *tip = chainActive.Tip();
        BOOST_CHECK_NE(tip, nullptr);

        const uint256& hash{tip->GetBlockHash()};
        BOOST_CHECK(hash != emptyHash);

        const int height = tip->nHeight;
        BOOST_CHECK_GT(height, 0);

        std::string reason;

        // ticket

        const CWalletTx* ticket = wallet->GetWalletTx(ticketHash);

        BOOST_CHECK(ticketHash != emptyHash);
        BOOST_CHECK_NE(ticket, nullptr);
        BOOST_CHECK_NE(ticket->tx, nullptr);

        BOOST_CHECK_MESSAGE(ValidateBuyTicketStructure(*(ticket->tx), reason), ((reason.size() > 0) ? reason : "ValidateBuyTicketStructure"));

        const CAmount ticketPrice = ticket->tx->vout[ticketStakeOutputIndex].nValue;
        BOOST_CHECK(MoneyRange(ticketPrice));

        std::vector<TicketContribData> contributions;
        CAmount contributionSum{0};
        for (unsigned i = ticketContribOutputIndex; i < ticket->tx->vout.size(); i += 2) {
            TicketContribData contrib;
            BOOST_CHECK(ParseTicketContrib(*(ticket->tx), i, contrib));

            contributions.push_back(contrib);

            contributionSum += contrib.contributedAmount;
        }

        // vote

        CMutableTransaction mtx;

        // inputs

        mtx.vin.push_back(CTxIn(COutPoint(), consensus.stakeBaseSigScript));
        mtx.vin.push_back(CTxIn(COutPoint(ticketHash, ticketStakeOutputIndex)));

        // outputs

        const CAmount voterSubsidy = GetVoterSubsidy(height + 1, consensus);
        BOOST_CHECK_NE(voterSubsidy, 0);

        VoteData voteData{1, hash, static_cast<uint32_t>(height), 0x0001, 0, defaultEvb};
        CScript declScript = GetScriptForVoteDecl(voteData);
        mtx.vout.push_back(CTxOut(0, declScript));

        for (const TicketContribData& contrib: contributions) {
            const CAmount reward = CalculateGrossRemuneration(contrib.contributedAmount, ticketPrice, voterSubsidy, contributionSum);
            BOOST_CHECK_GT(reward, 0);

            const CScript& rewardDestination = contrib.whichAddr == 1 ? GetScriptForDestination(CKeyID(contrib.rewardAddr)) : GetScriptForDestination(CScriptID(contrib.rewardAddr));
            mtx.vout.push_back(CTxOut(reward, rewardDestination));
        }

        // signature

        std::map<uint256, CWalletTx>::const_iterator mi = wallet->mapWallet.find(mtx.vin[1].prevout.hash);
        BOOST_CHECK(mi != wallet->mapWallet.end());
        BOOST_CHECK_LT(mtx.vin[1].prevout.n, static_cast<uint32_t>(mi->second.tx->vout.size()));

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
        BOOST_CHECK(state.IsValid());

        return wtx.GetHash();
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

    bool MempoolHasVoteForTicket(const uint256& ticketHash) {
        std::vector<TxMempoolInfo> txnInfos = mempool.infoAll();
        for (size_t j = 0; j < txnInfos.size(); ++j) {
            const CTransactionRef tx = txnInfos[j].tx;
            if (ParseTxClass(*(tx.get())) == TX_Vote && tx->vin.size() > voteStakeInputIndex && tx->vin[voteStakeInputIndex].prevout.hash == ticketHash)
                return true;
        }

        return false;
    }

    bool MempoolHasEnoughVotesForBlock() {
        const int majority{(Params().GetConsensus().nTicketsPerBlock / 2) + 1};
        int count{0};
        std::vector<TxMempoolInfo> txnInfos = mempool.infoAll();
        for (size_t j = 0; j < txnInfos.size(); ++j)
            if (ParseTxClass(*(txnInfos[j].tx)) == TX_Vote)
                if ((++count) >= majority)
                    return true;

        return false;
    }

    CBlock CreateAndProcessBlock(const CScript& scriptPubKey)
    {
        const CChainParams& chainparams = Params();
        std::unique_ptr<CBlockTemplate> pblocktemplate = BlockAssembler(chainparams).CreateNewBlock(scriptPubKey);
        CBlock& block = pblocktemplate->block;

        unsigned int extraNonce = 0;
        IncrementExtraNonce(&block, chainActive.Tip(), extraNonce);

        while (!CheckProofOfWork(block.GetHash(), block.nBits, chainparams.GetConsensus())) ++block.nNonce;

        std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(block);
        ProcessNewBlock(chainparams, shared_pblock, true, nullptr);

        CBlock result = block;
        return result;
    }

    bool ExtendChain(int blockCount, bool withImplicitVotes = true, bool useVsp = false, bool foreign = false)
    {
        if (blockCount <= 0)
            return false;

        LOCK2(cs_main, wallet->cs_wallet);

        CScript coinbaseScriptPubKey = coinbaseTxns[0].vout[0].scriptPubKey;

        latestTestVotes.clear();

        bool result = true;

        bool shouldRelock = wallet->IsLocked();

        for (int i = 0; i < blockCount; ++i) {
            if (shouldRelock)
                wallet->Unlock(passphrase);

            if (chainActive.Tip()->nHeight >= consensus.nStakeValidationHeight - 1 - consensus.nTicketMaturity - 1)
                while (!MempoolHasEnoughTicketsForBlock())
                    BOOST_CHECK(AddTicketTx(useVsp, foreign) != emptyHash);

            if (withImplicitVotes && chainActive.Tip()->nHeight >= consensus.nStakeValidationHeight - 1)
                for (const uint256& ticketHash : chainActive.Tip()->pstakeNode->Winners())
                    if (!MempoolHasVoteForTicket(ticketHash))
                        BOOST_CHECK(AddVoteTx(ticketHash) != emptyHash);

            if (shouldRelock)
                wallet->Lock();

            try {
                CBlock b = CreateAndProcessBlock(coinbaseScriptPubKey);

                coinbaseTxns.push_back(*b.vtx[0]);

                const StakeSlice votes(b.vtx, TX_Vote);
                latestTestVotes.insert(latestTestVotes.end(), votes.begin(), votes.end());
            } catch (...) {
                result = false;
            }
        }

        return result;
    }

    void CheckTicket(const CTransaction& ticket)
    {
        std::string reason;
        BOOST_CHECK_MESSAGE(ValidateBuyTicketStructure(ticket, reason), ((reason.size() > 0) ? reason : "ValidateBuyTicketStructure"));
    }

    void CheckVote(const CTransaction& vote, const CTransaction& ticket, bool inMempool = true)
    {
        // ticket

        CheckTicket(ticket);

        const CBlockIndex* chainTip = chainActive.Tip();
        std::string reason;

        // structure

        BOOST_CHECK_MESSAGE(ValidateVoteStructure(vote, reason), ((reason.size() > 0) ? reason : "ValidateVoteStructure"));

        // inputs

        BOOST_CHECK(vote.vin[voteSubsidyInputIndex].prevout.hash == uint256());
        BOOST_CHECK_EQUAL(vote.vin[voteSubsidyInputIndex].prevout.n, static_cast<uint32_t>(-1));
        BOOST_CHECK(vote.vin[voteSubsidyInputIndex].scriptSig == consensus.stakeBaseSigScript);

        const CAmount& ticketPrice = ticket.vout[ticketStakeOutputIndex].nValue;

        BOOST_CHECK(vote.vin[voteStakeInputIndex].prevout.hash == ticket.GetHash());
        BOOST_CHECK_EQUAL(vote.vin[voteStakeInputIndex].prevout.n, ticketStakeOutputIndex);

        // vote output

        VoteData voteData;
        BOOST_CHECK(ParseVote(vote, voteData));

        BOOST_CHECK_EQUAL(voteData.nVersion, 1);
        BOOST_CHECK(voteData.blockHash == (inMempool ? chainTip->GetBlockHash() : chainTip->pprev->GetBlockHash()));
        BOOST_CHECK_EQUAL(voteData.blockHeight, static_cast<uint32_t>(inMempool ? chainTip->nHeight : chainTip->pprev->nHeight));
        BOOST_CHECK_EQUAL(voteData.voterStakeVersion, 0U);
        BOOST_CHECK(voteData.extendedVoteBits.isValid());

        // reward outputs

        CAmount totalContribution{0};
        std::vector<TicketContribData> contributions;
        for (unsigned i = ticketContribOutputIndex; i < ticket.vout.size(); i += 2) {
            TicketContribData contrib;
            BOOST_CHECK(ParseTicketContrib(ticket, i, contrib));
            contributions.push_back(contrib);
            totalContribution += contrib.contributedAmount;
        }

        const CAmount& voteSubsidy = GetVoterSubsidy(inMempool ? chainTip->nHeight + 1 : chainTip->nHeight, consensus);

        for (unsigned i = voteRewardOutputIndex; i < vote.vout.size(); ++i) {
            const TicketContribData& contrib = contributions[i-voteRewardOutputIndex];

            const CScript& rewardDestination = contrib.whichAddr == 1 ? GetScriptForDestination(CKeyID(contrib.rewardAddr)) : GetScriptForDestination(CScriptID(contrib.rewardAddr));
            BOOST_CHECK(vote.vout[i].scriptPubKey == rewardDestination);

            const CAmount& reward = CalculateGrossRemuneration(contrib.contributedAmount, ticketPrice, voteSubsidy, totalContribution);
            BOOST_CHECK_EQUAL(vote.vout[i].nValue, reward);
        }
    }

    void CheckLatestVotes(bool inMempool = false, bool alsoIfAllMine = true)
    {
        std::string reason;

        HashVector winners;
        std::vector<CTransactionRef> votes;

        if (inMempool) {
            winners = chainActive.Tip()->pstakeNode->Winners();

            std::vector<TxMempoolInfo> txnInfos = mempool.infoAll();
            for (size_t j = 0; j < txnInfos.size(); ++j)
                if (ParseTxClass(*(txnInfos[j].tx)) == TX_Vote)
                    votes.push_back(txnInfos[j].tx);
        } else {
            winners = chainActive.Tip()->pprev->pstakeNode->Winners();

            votes.insert(votes.end(), latestTestVotes.begin(), latestTestVotes.end());
        }

        BOOST_CHECK(votes.size() == winners.size());

        for (CTransactionRef vote : votes) {
            // vote transaction

            BOOST_CHECK_MESSAGE(ValidateVoteStructure(*vote, reason), ((reason.size() > 0) ? reason : "ValidateVoteStructure"));

            const uint256 ticketHash = vote->vin[voteStakeInputIndex].prevout.hash;

            BOOST_CHECK(std::find_if(winners.begin(), winners.end(), [&ticketHash](const uint256 hash) { return hash == ticketHash; }) != winners.end());

            BOOST_CHECK(vote->vin[voteStakeInputIndex].prevout.n == ticketStakeOutputIndex);

            // wallet transactions

            if (alsoIfAllMine) {
                const CWalletTx* voteWtx = wallet->GetWalletTx(vote->GetHash());
                BOOST_CHECK(voteWtx != nullptr);
                BOOST_CHECK(voteWtx->tx != nullptr);

                const CWalletTx* ticketWtx = wallet->GetWalletTx(vote->vin[voteStakeInputIndex].prevout.hash);
                BOOST_CHECK(ticketWtx != nullptr);
                BOOST_CHECK(ticketWtx->tx != nullptr);

                CheckVote(*(voteWtx->tx), *(ticketWtx->tx), inMempool);
            }
        }
    }

    void CheckVote(const uint256& voteHash, const uint256& ticketHash, const VoteData& voteData)
    {
        BOOST_CHECK(voteHash != emptyHash);
        BOOST_CHECK(ticketHash != emptyHash);

        const CWalletTx* voteWtx = wallet->GetWalletTx(voteHash);
        BOOST_CHECK_NE(voteWtx, nullptr);

        const CTransactionRef vote = voteWtx->tx;
        BOOST_CHECK_NE(vote.get(), nullptr);

        const CWalletTx* ticketWtx = wallet->GetWalletTx(ticketHash);
        BOOST_CHECK_NE(ticketWtx, nullptr);

        const CTransactionRef ticket = ticketWtx->tx;
        BOOST_CHECK_NE(ticket.get(), nullptr);

        CheckVote(*vote, *ticket);

        VoteData vd;
        BOOST_CHECK(ParseVote(*vote, vd));

        BOOST_CHECK_EQUAL(vd.nVersion, voteData.nVersion);
        BOOST_CHECK(vd.blockHash == voteData.blockHash);
        BOOST_CHECK_EQUAL(vd.blockHeight, voteData.blockHeight);
        BOOST_CHECK(vd.voteBits == voteData.voteBits);
        BOOST_CHECK_EQUAL(vd.voterStakeVersion, voteData.voterStakeVersion);
        BOOST_CHECK(vd.extendedVoteBits == voteData.extendedVoteBits);
    }

    const Consensus::Params& consensus = Params().GetConsensus();
    const uint256 emptyHash = uint256();

    const ExtendedVoteBits defaultEvb;

    const std::vector<unsigned char> emptyVector;
    const std::vector<unsigned char> validLargeEvbVector;
    const std::vector<unsigned char> invalidLargeEvbVector;

    const std::string emptyString;
    std::string validLargeEvbString;
    std::string invalidLargeEvbString;

    const SecureString passphrase = "aV3rySecurePassword!";
    std::unique_ptr<CWallet> wallet;
    std::vector<CTransactionRef> latestTestVotes;
};

BOOST_FIXTURE_TEST_SUITE(vote_tests, VoteTestingSetup)

// test the stakebase content verifier
BOOST_FIXTURE_TEST_CASE(stakebase_detection, VoteTestingSetup)
{
    const uint256& dummyOutputHash = GetRandHash();
    const uint256& prevOutputHash = uint256();
    const uint32_t& prevOutputIndex = static_cast<uint32_t>(-1);
    const CScript& stakeBaseSigScript = consensus.stakeBaseSigScript;

    // prevout.n
    BOOST_CHECK_EQUAL(HasStakebaseContents(CTxIn(prevOutputHash, 0, stakeBaseSigScript)), false);
    BOOST_CHECK_EQUAL(HasStakebaseContents(CTxIn(prevOutputHash, 1, stakeBaseSigScript)), false);
    BOOST_CHECK_EQUAL(HasStakebaseContents(CTxIn(prevOutputHash, 2, stakeBaseSigScript)), false);

    // sigScript
    BOOST_CHECK_EQUAL(HasStakebaseContents(CTxIn(prevOutputHash, prevOutputIndex, CScript())), false);
    BOOST_CHECK_EQUAL(HasStakebaseContents(CTxIn(prevOutputHash, prevOutputIndex, CScript() << 0x01)), false);
    BOOST_CHECK_EQUAL(HasStakebaseContents(CTxIn(prevOutputHash, prevOutputIndex, CScript() << 0x01 << 0x02)), false);
    BOOST_CHECK_EQUAL(HasStakebaseContents(CTxIn(prevOutputHash, prevOutputIndex, CScript() << 0x01)), false);

    // prevout.hash
    BOOST_CHECK_EQUAL(HasStakebaseContents(CTxIn(dummyOutputHash, prevOutputIndex, stakeBaseSigScript)), false);

    // correct
    BOOST_CHECK(HasStakebaseContents(CTxIn(prevOutputHash, prevOutputIndex, stakeBaseSigScript)));
}

// test the vote bits
BOOST_FIXTURE_TEST_CASE(vote_bits, VoteTestingSetup)
{
    // default construction

    BOOST_CHECK(VoteBits().getBits() == 0x0000);

    // construction from uint16_t

    BOOST_CHECK(VoteBits(0x0000).getBits() == 0x0000);
    BOOST_CHECK(VoteBits(0x0001).getBits() == 0x0001);
    BOOST_CHECK(VoteBits(0x1234).getBits() == 0x1234);
    BOOST_CHECK(VoteBits(0xFFFF).getBits() == 0xFFFF);

    // construction with signaled bit

    BOOST_CHECK(VoteBits(VoteBits::Rtt, true).getBits() == 0x0001);
    BOOST_CHECK(VoteBits(VoteBits::Rtt, false).getBits() == 0x0000);
    BOOST_CHECK(VoteBits(1, true).getBits() == 0x0002);
    BOOST_CHECK(VoteBits(1, false).getBits() == 0x0000);
    BOOST_CHECK(VoteBits(7, true).getBits() == 0x0080);
    BOOST_CHECK(VoteBits(7, false).getBits() == 0x0000);
    BOOST_CHECK(VoteBits(15, true).getBits() == 0x8000);
    BOOST_CHECK(VoteBits(15, false).getBits() == 0x0000);

    // copy constructor (also testing getting all the bits)

    VoteBits vb1;
    BOOST_CHECK(VoteBits(vb1).getBits() == 0x0000);

    vb1 = 0x0001;
    BOOST_CHECK(VoteBits(vb1).getBits() == 0x0001);

    vb1 = 0x1234;
    BOOST_CHECK(VoteBits(vb1).getBits() == 0x1234);

    vb1 = 0xFFFF;
    BOOST_CHECK(VoteBits(vb1).getBits() == 0xFFFF);

    // move constructor

    vb1 = 0x0000;
    BOOST_CHECK(VoteBits(std::move(vb1)).getBits() == 0x0000);

    vb1 = 0x0001;
    BOOST_CHECK(VoteBits(std::move(vb1)).getBits() == 0x0001);

    vb1 = 0x1234;
    BOOST_CHECK(VoteBits(std::move(vb1)).getBits() == 0x1234);

    vb1 = 0xFFFF;
    BOOST_CHECK(VoteBits(std::move(vb1)).getBits() == 0xFFFF);

    // copy assignment

    vb1 = 0x0000;
    VoteBits vb2 = vb1;
    BOOST_CHECK(vb2.getBits() == 0x0000);

    vb1 = 0x0001;
    vb2 = vb1;
    BOOST_CHECK(vb2.getBits() == 0x0001);

    vb1 = 0x1234;
    vb2 = vb1;
    BOOST_CHECK(vb2.getBits() == 0x1234);

    vb1 = 0xFFFF;
    vb2 = vb1;
    BOOST_CHECK(vb2.getBits() == 0xFFFF);

    // move assignment

    vb1 = 0x0000;
    vb2 = std::move(vb1);
    BOOST_CHECK(vb2.getBits() == 0x0000);

    vb1 = 0x0001;
    vb2 = std::move(vb1);
    BOOST_CHECK(vb2.getBits() == 0x0001);

    vb1 = 0x1234;
    vb2 = std::move(vb1);
    BOOST_CHECK(vb2.getBits() == 0x1234);

    vb1 = 0xFFFF;
    vb2 = std::move(vb1);
    BOOST_CHECK(vb2.getBits() == 0xFFFF);

    // equality

    vb1 = 0x0000;
    BOOST_CHECK(vb1 == VoteBits(0x0000));

    vb1 = 0x0001;
    BOOST_CHECK(vb1 == VoteBits(0x0001));

    vb1 = 0x1234;
    BOOST_CHECK(vb1 == VoteBits(0x1234));

    vb1 = 0xFFFF;
    BOOST_CHECK(vb1 == VoteBits(0xFFFF));

    // inequality

    vb1 = 0x0000;
    BOOST_CHECK(vb1 != VoteBits(0x0001));
    BOOST_CHECK(vb1 != VoteBits(0x1234));
    BOOST_CHECK(vb1 != VoteBits(0xFFFF));

    vb1 = 0x0001;
    BOOST_CHECK(vb1 != VoteBits(0x0000));
    BOOST_CHECK(vb1 != VoteBits(0x1234));
    BOOST_CHECK(vb1 != VoteBits(0xFFFF));

    vb1 = 0x1234;
    BOOST_CHECK(vb1 != VoteBits(0x0000));
    BOOST_CHECK(vb1 != VoteBits(0x0001));
    BOOST_CHECK(vb1 != VoteBits(0xFFFF));

    vb1 = 0xFFFF;
    BOOST_CHECK(vb1 != VoteBits(0x0000));
    BOOST_CHECK(vb1 != VoteBits(0x0001));
    BOOST_CHECK(vb1 != VoteBits(0x1234));

    // set bit

    vb1 = 0x0000;

    vb1.setBit(VoteBits::Rtt, true);
    BOOST_CHECK(vb1.getBits() == 0x0001);
    vb1.setBit(VoteBits::Rtt, false);
    BOOST_CHECK(vb1.getBits() == 0x0000);

    vb1.setBit(1, true);
    BOOST_CHECK(vb1.getBits() == 0x0002);
    vb1.setBit(1, false);
    BOOST_CHECK(vb1.getBits() == 0x0000);

    vb1.setBit(7, true);
    BOOST_CHECK(vb1.getBits() == 0x0080);
    vb1.setBit(7, false);
    BOOST_CHECK(vb1.getBits() == 0x0000);

    vb1.setBit(15, true);
    BOOST_CHECK(vb1.getBits() == 0x8000);
    vb1.setBit(15, false);
    BOOST_CHECK(vb1.getBits() == 0x0000);

    // get bit

    vb1 = 0x0001;
    BOOST_CHECK(vb1.getBit(VoteBits::Rtt));
    vb1 = 0x0000;
    BOOST_CHECK(!vb1.getBit(VoteBits::Rtt));

    vb1 = 0x0002;
    BOOST_CHECK(vb1.getBit(1));
    vb1 = 0x0000;
    BOOST_CHECK(!vb1.getBit(1));

    vb1 = 0x0080;
    BOOST_CHECK(vb1.getBit(7));
    vb1 = 0x0000;
    BOOST_CHECK(!vb1.getBit(7));

    vb1 = 0x8000;
    BOOST_CHECK(vb1.getBit(15));
    vb1 = 0x0000;
    BOOST_CHECK(!vb1.getBit(15));

    vb1 = 0x0003;
    BOOST_CHECK(vb1.getBit(0));
    BOOST_CHECK(vb1.getBit(1));
    BOOST_CHECK(!vb1.getBit(2));

    vb1 = 0xFFFE;
    BOOST_CHECK(!vb1.getBit(0));
    BOOST_CHECK(vb1.getBit(1));
    BOOST_CHECK(vb1.getBit(2));
    BOOST_CHECK(vb1.getBit(7));
    BOOST_CHECK(vb1.getBit(15));

    // RTT acceptance verification

    vb1 = 0x0001;
    BOOST_CHECK(vb1.isRttAccepted());
    vb1 = 0x4321;
    BOOST_CHECK(vb1.isRttAccepted());
    vb1 = 0xCDEF;
    BOOST_CHECK(vb1.isRttAccepted());

    vb1 = 0x0000;
    BOOST_CHECK(!vb1.isRttAccepted());
    vb1 = 0x1234;
    BOOST_CHECK(!vb1.isRttAccepted());
    vb1 = 0xFFFE;
    BOOST_CHECK(!vb1.isRttAccepted());

    // RTT acceptance control

    vb1 = 0x0000;
    vb1.setRttAccepted(true);
    BOOST_CHECK(vb1.getBits() == 0x0001);
    vb1.setRttAccepted(false);
    BOOST_CHECK(vb1.getBits() == 0x0000);

    vb1 = 0x1234;
    vb1.setRttAccepted(true);
    BOOST_CHECK(vb1.getBits() == 0x1235);
    vb1.setRttAccepted(false);
    BOOST_CHECK(vb1.getBits() == 0x1234);

    vb1 = 0xFFFE;
    vb1.setRttAccepted(true);
    BOOST_CHECK(vb1.getBits() == 0xFFFF);
    vb1.setRttAccepted(false);
    BOOST_CHECK(vb1.getBits() == 0xFFFE);

    vb1 = 0xFFFF;
    vb1.setRttAccepted(true);
    BOOST_CHECK(vb1.getBits() == 0xFFFF);
    vb1.setRttAccepted(false);
    BOOST_CHECK(vb1.getBits() == 0xFFFE);
}

// test the extended vote bits functions
BOOST_FIXTURE_TEST_CASE(extended_vote_bits, VoteTestingSetup)
{
    // string validation

    BOOST_CHECK(ExtendedVoteBits::containsValidExtendedVoteBits("00"));
    BOOST_CHECK(ExtendedVoteBits::containsValidExtendedVoteBits("01"));
    BOOST_CHECK(ExtendedVoteBits::containsValidExtendedVoteBits("0123"));
    BOOST_CHECK(ExtendedVoteBits::containsValidExtendedVoteBits("1234567890abcdef"));
    BOOST_CHECK(ExtendedVoteBits::containsValidExtendedVoteBits("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef"));
    BOOST_CHECK(ExtendedVoteBits::containsValidExtendedVoteBits(validLargeEvbString));

    BOOST_CHECK(!ExtendedVoteBits::containsValidExtendedVoteBits("0"));
    BOOST_CHECK(!ExtendedVoteBits::containsValidExtendedVoteBits("1"));
    BOOST_CHECK(!ExtendedVoteBits::containsValidExtendedVoteBits("012"));
    BOOST_CHECK(!ExtendedVoteBits::containsValidExtendedVoteBits("12345"));
    BOOST_CHECK(!ExtendedVoteBits::containsValidExtendedVoteBits("abc"));
    BOOST_CHECK(!ExtendedVoteBits::containsValidExtendedVoteBits("lala"));
    BOOST_CHECK(!ExtendedVoteBits::containsValidExtendedVoteBits("a!"));
    BOOST_CHECK(!ExtendedVoteBits::containsValidExtendedVoteBits(" "));
    BOOST_CHECK(!ExtendedVoteBits::containsValidExtendedVoteBits("a "));
    BOOST_CHECK(!ExtendedVoteBits::containsValidExtendedVoteBits(emptyString));
    BOOST_CHECK(!ExtendedVoteBits::containsValidExtendedVoteBits(invalidLargeEvbString));

    // vector validation

    std::vector<unsigned char> vect = {0x00};
    BOOST_CHECK(ExtendedVoteBits::containsValidExtendedVoteBits(vect));
    vect = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    BOOST_CHECK(ExtendedVoteBits::containsValidExtendedVoteBits(vect));
    BOOST_CHECK(ExtendedVoteBits::containsValidExtendedVoteBits(validLargeEvbVector));

    BOOST_CHECK(!ExtendedVoteBits::containsValidExtendedVoteBits(emptyVector));
    BOOST_CHECK(!ExtendedVoteBits::containsValidExtendedVoteBits(invalidLargeEvbVector));

    // vote data size

    VoteData voteData;
    BOOST_CHECK_EQUAL(ExtendedVoteBits::minSize(), 1U);
    BOOST_CHECK_EQUAL(ExtendedVoteBits::maxSize(), nMaxStructDatacarrierBytes - GetVoteDataSizeWithEmptyExtendedVoteBits() - 1);
    BOOST_CHECK_EQUAL(GetVoteDataSizeWithEmptyExtendedVoteBits(), GetSerializeSize(GetScriptForVoteDecl(voteData), SER_NETWORK, PROTOCOL_VERSION));

    // default construction

    BOOST_CHECK(ExtendedVoteBits().getVector() == defaultEvb.getVector());

    // construction from string

    vect = {0x00};
    BOOST_CHECK(ExtendedVoteBits("00").getVector() == vect);

    vect = {0x01};
    BOOST_CHECK(ExtendedVoteBits("01").getVector() == vect);

    vect = {0x01, 0x23};
    BOOST_CHECK(ExtendedVoteBits("0123").getVector() == vect);

    vect = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    BOOST_CHECK(ExtendedVoteBits("0123456789abcdef").getVector() == vect);

    BOOST_CHECK(ExtendedVoteBits(validLargeEvbString).getVector() == validLargeEvbVector);

    BOOST_CHECK(ExtendedVoteBits(emptyString).getVector() == defaultEvb.getVector());
    BOOST_CHECK(ExtendedVoteBits("").getVector() == defaultEvb.getVector());
    BOOST_CHECK(ExtendedVoteBits("0").getVector() == defaultEvb.getVector());
    BOOST_CHECK(ExtendedVoteBits("1").getVector() == defaultEvb.getVector());
    BOOST_CHECK(ExtendedVoteBits("123").getVector() == defaultEvb.getVector());
    BOOST_CHECK(ExtendedVoteBits("12345").getVector() == defaultEvb.getVector());
    BOOST_CHECK(ExtendedVoteBits("abc").getVector() == defaultEvb.getVector());
    BOOST_CHECK(ExtendedVoteBits("lala").getVector() == defaultEvb.getVector());
    BOOST_CHECK(ExtendedVoteBits("a!").getVector() == defaultEvb.getVector());
    BOOST_CHECK(ExtendedVoteBits(" ").getVector() == defaultEvb.getVector());
    BOOST_CHECK(ExtendedVoteBits("a ").getVector() == defaultEvb.getVector());
    BOOST_CHECK(ExtendedVoteBits(invalidLargeEvbString).getVector() == defaultEvb.getVector());

    // construction from vector (also testing vector access)

    vect = {0x00};
    BOOST_CHECK(ExtendedVoteBits(vect).getVector() == vect);

    vect = {0x01};
    BOOST_CHECK(ExtendedVoteBits(vect).getVector() == vect);

    vect = {0x01, 0x23};
    BOOST_CHECK(ExtendedVoteBits(vect).getVector() == vect);

    vect = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    BOOST_CHECK(ExtendedVoteBits(vect).getVector() == vect);

    BOOST_CHECK(ExtendedVoteBits(validLargeEvbVector).getVector() == validLargeEvbVector);

    vect.clear();
    BOOST_CHECK(ExtendedVoteBits(vect).getVector() == defaultEvb.getVector());

    BOOST_CHECK(ExtendedVoteBits(invalidLargeEvbVector).getVector() == defaultEvb.getVector());

    // copy constructor

    vect = {0x00};
    BOOST_CHECK(ExtendedVoteBits(ExtendedVoteBits(vect)).getVector() == vect);

    vect = {0x01, 0x23};
    BOOST_CHECK(ExtendedVoteBits(ExtendedVoteBits(vect)).getVector() == vect);

    vect = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    BOOST_CHECK(ExtendedVoteBits(ExtendedVoteBits(vect)).getVector() == vect);

    BOOST_CHECK(ExtendedVoteBits(ExtendedVoteBits(validLargeEvbVector)).getVector() == validLargeEvbVector);

    BOOST_CHECK(ExtendedVoteBits(ExtendedVoteBits()).getVector() == defaultEvb.getVector());
    BOOST_CHECK(ExtendedVoteBits(ExtendedVoteBits(invalidLargeEvbVector)).getVector() == defaultEvb.getVector());

    // move constructor

    vect = {0x00};
    ExtendedVoteBits evb1 = ExtendedVoteBits(vect);
    BOOST_CHECK(ExtendedVoteBits(std::move(evb1)).getVector() == vect);

    vect = {0x01, 0x23};
    evb1 = ExtendedVoteBits(vect);
    BOOST_CHECK(ExtendedVoteBits(std::move(evb1)).getVector() == vect);

    vect = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    evb1 = ExtendedVoteBits(vect);
    BOOST_CHECK(ExtendedVoteBits(std::move(evb1)).getVector() == vect);

    evb1 = ExtendedVoteBits(validLargeEvbVector);
    BOOST_CHECK(ExtendedVoteBits(std::move(evb1)).getVector() == validLargeEvbVector);

    evb1 = ExtendedVoteBits();
    BOOST_CHECK(ExtendedVoteBits(std::move(evb1)).getVector() == defaultEvb.getVector());

    evb1 = ExtendedVoteBits(invalidLargeEvbVector);
    BOOST_CHECK(ExtendedVoteBits(std::move(evb1)).getVector() == defaultEvb.getVector());

    // copy assignment

    vect = {0x00};
    evb1 = ExtendedVoteBits(vect);
    ExtendedVoteBits evb2 = evb1;
    BOOST_CHECK(evb2.getVector() == vect);

    vect = {0x01, 0x23};
    evb1 = ExtendedVoteBits(vect);
    evb2 = evb1;
    BOOST_CHECK(evb2.getVector() == vect);

    vect = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    evb1 = ExtendedVoteBits(vect);
    evb2 = evb1;
    BOOST_CHECK(evb2.getVector() == vect);

    evb1 = ExtendedVoteBits(validLargeEvbVector);
    evb2 = evb1;
    BOOST_CHECK(evb2.getVector() == validLargeEvbVector);

    evb1 = ExtendedVoteBits();
    evb2 = evb1;
    BOOST_CHECK(evb2.getVector() == defaultEvb.getVector());

    evb1 = ExtendedVoteBits(invalidLargeEvbVector);
    evb2 = evb1;
    BOOST_CHECK(evb2.getVector() == defaultEvb.getVector());

    // move assignment

    vect = {0x00};
    evb1 = ExtendedVoteBits(vect);
    evb2 = std::move(evb1);
    BOOST_CHECK(evb2.getVector() == vect);

    vect = {0x01, 0x23};
    evb1 = ExtendedVoteBits(vect);
    evb2 = std::move(evb1);
    BOOST_CHECK(evb2.getVector() == vect);

    vect = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    evb1 = ExtendedVoteBits(vect);
    evb2 = std::move(evb1);
    BOOST_CHECK(evb2.getVector() == vect);

    evb1 = ExtendedVoteBits(validLargeEvbVector);
    evb2 = std::move(evb1);
    BOOST_CHECK(evb2.getVector() == validLargeEvbVector);

    evb1 = ExtendedVoteBits();
    evb2 = std::move(evb1);
    BOOST_CHECK(evb2.getVector() == defaultEvb.getVector());

    evb1 = ExtendedVoteBits(invalidLargeEvbVector);
    evb2 = std::move(evb1);
    BOOST_CHECK(evb2.getVector() == defaultEvb.getVector());

    // equality

    vect = {0x00};
    BOOST_CHECK(ExtendedVoteBits(vect) == ExtendedVoteBits("00"));

    vect = {0x01, 0x23};
    BOOST_CHECK(ExtendedVoteBits(vect) == ExtendedVoteBits("0123"));

    vect = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    BOOST_CHECK(ExtendedVoteBits(vect) == ExtendedVoteBits("0123456789abcdef"));

    BOOST_CHECK(ExtendedVoteBits(validLargeEvbVector) == ExtendedVoteBits(validLargeEvbString));
    BOOST_CHECK(ExtendedVoteBits() == defaultEvb);
    BOOST_CHECK(ExtendedVoteBits(invalidLargeEvbVector) == ExtendedVoteBits());

    // inequality

    vect = {0x00};
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits("01"));
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits("0123"));
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits("0123456789abcdef"));
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits(validLargeEvbString));

    vect = {0x01};
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits("00"));
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits("0123"));
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits("0123456789abcdef"));
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits(validLargeEvbString));

    vect = {0x01, 0x23};
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits("00"));
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits("01"));
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits("0123456789abcdef"));
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits(validLargeEvbString));

    vect = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits("00"));
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits("01"));
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits("0123"));
    BOOST_CHECK(ExtendedVoteBits(vect) != ExtendedVoteBits(validLargeEvbString));

    BOOST_CHECK(ExtendedVoteBits(validLargeEvbString) != ExtendedVoteBits("00"));
    BOOST_CHECK(ExtendedVoteBits(validLargeEvbString) != ExtendedVoteBits("01"));
    BOOST_CHECK(ExtendedVoteBits(validLargeEvbString) != ExtendedVoteBits("0123"));
    BOOST_CHECK(ExtendedVoteBits(validLargeEvbString) != ExtendedVoteBits("0123456789abcdef"));

    // validity

    BOOST_CHECK(ExtendedVoteBits().isValid());
    BOOST_CHECK(ExtendedVoteBits("00").isValid());
    BOOST_CHECK(ExtendedVoteBits("01").isValid());
    BOOST_CHECK(ExtendedVoteBits("0123").isValid());
    BOOST_CHECK(ExtendedVoteBits("0123456789abcdef").isValid());
    BOOST_CHECK(ExtendedVoteBits(validLargeEvbString).isValid());
    BOOST_CHECK(ExtendedVoteBits(invalidLargeEvbString).isValid());

    // vector access (mostly already tested in the constructor area above)
    // const_cast like this is highly discouraged

    evb1 = ExtendedVoteBits();
    const_cast<std::vector<unsigned char>&>(evb1.getVector()).clear();
    BOOST_CHECK(!evb1.isValid());

    evb1 = ExtendedVoteBits();
    const_cast<std::vector<unsigned char>&>(evb1.getVector()) = validLargeEvbVector;
    BOOST_CHECK(evb1.getVector() == validLargeEvbVector);

    evb1 = ExtendedVoteBits();
    const_cast<std::vector<unsigned char>&>(evb1.getVector()) = invalidLargeEvbVector;
    BOOST_CHECK(!evb1.isValid());

    // hexadecimal string

    BOOST_CHECK(ExtendedVoteBits().getHex() == "00");

    vect = {0x00};
    BOOST_CHECK(ExtendedVoteBits(vect).getHex() == "00");

    vect = {0x01, 0x23};
    BOOST_CHECK(ExtendedVoteBits(vect).getHex() == "0123");

    vect = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    BOOST_CHECK(ExtendedVoteBits(vect).getHex() == "0123456789abcdef");

    BOOST_CHECK(ExtendedVoteBits(validLargeEvbVector).getHex() == validLargeEvbString);

    BOOST_CHECK(ExtendedVoteBits(invalidLargeEvbVector).getHex() == "00");
}

// test the ticket ownership verification
BOOST_FIXTURE_TEST_CASE(is_my_ticket, VoteTestingSetup)
{
    LOCK2(cs_main, wallet->cs_wallet);

    // dummy transaction

    {
        CMutableTransaction mtx;
        mtx.vin.push_back(CTxIn(uint256(), 0));
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

// test the transaction for votes
BOOST_FIXTURE_TEST_CASE(vote_transaction, VoteTestingSetup)
{
    std::string voteHashStr;
    CWalletError we;

    LOCK2(cs_main, wallet->cs_wallet);

    // Premature votes and preconditions

    {
        // ticket hash
        std::tie(voteHashStr, we) = wallet->Vote(uint256(), chainActive.Tip()->GetBlockHash(), chainActive.Tip()->nHeight, 0U, defaultEvb);
        BOOST_CHECK_EQUAL(we.code, CWalletError::INVALID_PARAMETER);

        // stake validation height not reached yet
        std::tie(voteHashStr, we) = wallet->Vote(chainActive.Tip()->GetBlockHash(), chainActive.Tip()->GetBlockHash(), chainActive.Tip()->nHeight, 0U, defaultEvb);
        BOOST_CHECK_EQUAL(we.code, CWalletError::INVALID_PARAMETER);

        ExtendChain(consensus.nStakeValidationHeight - chainActive.Height());

        // block hash
        std::tie(voteHashStr, we) = wallet->Vote(chainActive.Tip()->GetBlockHash(), uint256(), chainActive.Tip()->nHeight, 0U, defaultEvb);
        BOOST_CHECK_EQUAL(we.code, CWalletError::INVALID_PARAMETER);
        std::tie(voteHashStr, we) = wallet->Vote(chainActive.Tip()->GetBlockHash(), chainActive.Tip()->pprev->GetBlockHash(), chainActive.Tip()->nHeight, 0U, defaultEvb);
        BOOST_CHECK_EQUAL(we.code, CWalletError::INVALID_PARAMETER);

        // block height
        std::tie(voteHashStr, we) = wallet->Vote(chainActive.Tip()->GetBlockHash(), chainActive.Tip()->GetBlockHash(), chainActive.Tip()->nHeight - 1, 0U, defaultEvb);
        BOOST_CHECK_EQUAL(we.code, CWalletError::INVALID_PARAMETER);
        std::tie(voteHashStr, we) = wallet->Vote(chainActive.Tip()->GetBlockHash(), chainActive.Tip()->GetBlockHash(), chainActive.Tip()->nHeight + 1, 0U, defaultEvb);
        BOOST_CHECK_EQUAL(we.code, CWalletError::INVALID_PARAMETER);

        // ticket
        std::tie(voteHashStr, we) = wallet->Vote(chainActive.Tip()->GetBlockHash(), chainActive.Tip()->GetBlockHash(), chainActive.Tip()->nHeight, 0U, defaultEvb);
        BOOST_CHECK_EQUAL(we.code, CWalletError::INVALID_ADDRESS_OR_KEY);
    }

    // Single vote

    {
        BOOST_CHECK(chainActive.Tip()->pstakeNode->Winners().size() > 0);

        const uint256 ticketHash = chainActive.Tip()->pstakeNode->Winners()[0];
        BOOST_CHECK(ticketHash != uint256());

        std::tie(voteHashStr, we) = wallet->Vote(ticketHash, chainActive.Tip()->GetBlockHash(), chainActive.Tip()->nHeight, 1U, defaultEvb);
        BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);
        BOOST_CHECK(voteHashStr.length() > 0);

        const uint256& voteHash = uint256S(voteHashStr);

        BOOST_CHECK(MempoolHasVoteForTicket(ticketHash));

        const CWalletTx* ticket = wallet->GetWalletTx(ticketHash);
        BOOST_CHECK(ticket != nullptr);
        BOOST_CHECK(ticket->tx != nullptr);

        const CWalletTx* vote = wallet->GetWalletTx(voteHash);
        BOOST_CHECK(vote != nullptr);
        BOOST_CHECK(vote->tx != nullptr);

        CheckVote(*vote->tx, *ticket->tx);

        ExtendChain(1);

        BOOST_CHECK(std::find_if(latestTestVotes.begin(), latestTestVotes.end(), [&voteHash](const CTransactionRef tx) { return tx->GetHash() == voteHash; }) != latestTestVotes.end());
    }

    // Multiple votes

    {
        BOOST_CHECK(chainActive.Tip()->pstakeNode->Winners().size() > 0);

        std::vector<uint256> voteHashes;

        for (const uint256& ticketHash : chainActive.Tip()->pstakeNode->Winners()) {
            BOOST_CHECK(ticketHash != uint256());

            std::tie(voteHashStr, we) = wallet->Vote(ticketHash, chainActive.Tip()->GetBlockHash(), chainActive.Tip()->nHeight, 1U, defaultEvb);
            BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);
            BOOST_CHECK(voteHashStr.length() > 0);

            const uint256 voteHash = uint256S(voteHashStr);

            BOOST_CHECK(MempoolHasVoteForTicket(ticketHash));

            const CWalletTx* ticket = wallet->GetWalletTx(ticketHash);
            BOOST_CHECK(ticket != nullptr);
            BOOST_CHECK(ticket->tx != nullptr);

            const CWalletTx* vote = wallet->GetWalletTx(voteHash);
            BOOST_CHECK(vote != nullptr);
            BOOST_CHECK(vote->tx != nullptr);

            CheckVote(*vote->tx, *ticket->tx);

            voteHashes.push_back(voteHash);
        }

        ExtendChain(1);

        for (const uint256& voteHash : voteHashes)
            BOOST_CHECK(std::find_if(latestTestVotes.begin(), latestTestVotes.end(), [&voteHash](const CTransactionRef tx) { return tx->GetHash() == voteHash; }) != latestTestVotes.end());
    }

    // Encrypted wallet

    {
        wallet->EncryptWallet(passphrase);
        BOOST_CHECK(wallet->IsLocked());

        const uint256 ticketHash = chainActive.Tip()->pstakeNode->Winners()[0];
        BOOST_CHECK(ticketHash != uint256());

        std::tie(voteHashStr, we) = wallet->Vote(ticketHash, chainActive.Tip()->GetBlockHash(), chainActive.Tip()->nHeight, 1U, defaultEvb);
        BOOST_CHECK_EQUAL(we.code, CWalletError::WALLET_UNLOCK_NEEDED);
    }

    // TODO: add tests for voteBits and extendedVoteBits
}

// test the transaction for votes with VSP support
BOOST_FIXTURE_TEST_CASE(vote_transaction_vsp, VoteTestingSetup)
{
    std::string voteHashStr;
    CWalletError we;

    LOCK2(cs_main, wallet->cs_wallet);

    ExtendChain(consensus.nStakeValidationHeight - chainActive.Height(), true, true);

    BOOST_CHECK(chainActive.Tip()->pstakeNode->Winners().size() > 0);

    std::vector<uint256> voteHashes;

    for (const uint256& ticketHash : chainActive.Tip()->pstakeNode->Winners()) {
        BOOST_CHECK(ticketHash != uint256());

        std::tie(voteHashStr, we) = wallet->Vote(ticketHash, chainActive.Tip()->GetBlockHash(), chainActive.Tip()->nHeight, 1U, defaultEvb);
        BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);
        BOOST_CHECK(voteHashStr.length() > 0);

        const uint256 voteHash = uint256S(voteHashStr);

        BOOST_CHECK(MempoolHasVoteForTicket(ticketHash));

        const CWalletTx* ticket = wallet->GetWalletTx(ticketHash);
        BOOST_CHECK(ticket != nullptr);
        BOOST_CHECK(ticket->tx != nullptr);

        const CWalletTx* vote = wallet->GetWalletTx(voteHash);
        BOOST_CHECK(vote != nullptr);
        BOOST_CHECK(vote->tx != nullptr);

        CheckVote(*vote->tx, *ticket->tx);

        voteHashes.push_back(voteHash);
    }

    ExtendChain(1);

    for (const uint256& voteHash : voteHashes)
        BOOST_CHECK(std::find_if(latestTestVotes.begin(), latestTestVotes.end(), [&voteHash](const CTransactionRef tx) { return tx->GetHash() == voteHash; }) != latestTestVotes.end());
}

// test the failing transaction for votes that spend ticket not belonging to the wallet
BOOST_FIXTURE_TEST_CASE(foreign_ticket_vote_transaction, VoteTestingSetup)
{
    std::string txHash;
    CWalletError we;

    LOCK2(cs_main, wallet->cs_wallet);

    ExtendChain(consensus.nStakeValidationHeight - 1 - chainActive.Tip()->nHeight, true, false, true);

    BOOST_CHECK_GT(chainActive.Tip()->pstakeNode->Winners().size(), 0U);

    for (auto& hash : chainActive.Tip()->pstakeNode->Winners()) {
        std::tie(txHash, we) = wallet->Vote(hash, chainActive.Tip()->GetBlockHash(), chainActive.Tip()->nHeight, 0U, defaultEvb);
        BOOST_CHECK_EQUAL(we.code, CWalletError::INVALID_ADDRESS_OR_KEY);
    }
}

// test the failing transaction for votes that spend ticket not belonging to the wallet and using VSP
BOOST_FIXTURE_TEST_CASE(foreign_vsp_ticket_vote_transaction, VoteTestingSetup)
{
    std::string txHash;
    CWalletError we;

    LOCK2(cs_main, wallet->cs_wallet);

    ExtendChain(consensus.nStakeValidationHeight - 1 - chainActive.Tip()->nHeight, true, true, true);

    BOOST_CHECK_GT(chainActive.Tip()->pstakeNode->Winners().size(), 0U);

    for (auto& hash : chainActive.Tip()->pstakeNode->Winners()) {
        std::tie(txHash, we) = wallet->Vote(hash, chainActive.Tip()->GetBlockHash(), chainActive.Tip()->nHeight, 0U, defaultEvb);
        BOOST_CHECK_EQUAL(we.code, CWalletError::INVALID_ADDRESS_OR_KEY);
    }
}

// test the automatic voter
BOOST_FIXTURE_TEST_CASE(auto_voter, VoteTestingSetup)
{
    LOCK2(cs_main, wallet->cs_wallet);

    CAutoVoter* av = wallet->GetAutoVoter();
    BOOST_CHECK(av != nullptr);

    BOOST_CHECK(!av->isStarted());

    CAutoVoterConfig& cfg = av->GetConfig();

    // Premature votes and preconditions

    av->start();
    ExtendChain(1);
    BOOST_CHECK(latestTestVotes.size() == 0U);
    ExtendChain(1);
    BOOST_CHECK(latestTestVotes.size() == 0U);
    av->stop();

    ExtendChain(consensus.nStakeValidationHeight - chainActive.Height());

    av->start();

    // All tickets (in mempool)

    ExtendChain(1);
    CheckLatestVotes(true);

    // All tickets (in blockchain) (from previous extension)

    ExtendChain(1, false);
    CheckLatestVotes();

    // Encrypted wallet (no passphrase)

    wallet->EncryptWallet(passphrase);
    BOOST_CHECK(wallet->IsLocked());

    ExtendChain(1, false); // (from previous extension)
    BOOST_CHECK(ExtendChain(1, false) == false);
    BOOST_CHECK(latestTestVotes.size() == 0U);

    // Encrypted wallet (wrong passphrase)

    cfg.passphrase = "aWr0ngP4$$word";

    BOOST_CHECK(ExtendChain(1, false) == false);
    BOOST_CHECK(latestTestVotes.size() == 0U);

    // Encrypted wallet (good passphrase)

    cfg.passphrase = passphrase;

    ExtendChain(1);
    BOOST_CHECK(latestTestVotes.size() > 0U);
    CheckLatestVotes(true);

    BOOST_CHECK(ExtendChain(1, false));
    BOOST_CHECK(latestTestVotes.size() > 0U);
    CheckLatestVotes();

    av->stop();
}

// test the automatic voter with VSP support
BOOST_FIXTURE_TEST_CASE(auto_voter_vsp, VoteTestingSetup)
{
    LOCK2(cs_main, wallet->cs_wallet);

    CAutoVoter* av = wallet->GetAutoVoter();
    BOOST_CHECK(av != nullptr);

    BOOST_CHECK(!av->isStarted());

    CAutoVoterConfig& cfg = av->GetConfig();

    // Premature votes and preconditions

    ExtendChain(consensus.nStakeValidationHeight - chainActive.Height(), true, true);

    av->start();

    // All tickets (in mempool)

    ExtendChain(1, true, true);
    CheckLatestVotes(true);

    // All tickets (in blockchain) (from previous extension)

    ExtendChain(1, false, true);
    CheckLatestVotes();

    // Encrypted wallet (no passphrase)

    wallet->EncryptWallet(passphrase);
    BOOST_CHECK(wallet->IsLocked());

    ExtendChain(1, false); // (from previous extension)
    BOOST_CHECK(ExtendChain(1, false, true) == false);
    BOOST_CHECK(latestTestVotes.size() == 0U);

    // Encrypted wallet (wrong passphrase)

    cfg.passphrase = "aWr0ngP4$$word";

    BOOST_CHECK(ExtendChain(1, false, true) == false);
    BOOST_CHECK(latestTestVotes.size() == 0U);

    // Encrypted wallet (good passphrase)

    cfg.passphrase = passphrase;

    ExtendChain(1, true, true);
    BOOST_CHECK(latestTestVotes.size() > 0U);
    CheckLatestVotes(true);

    BOOST_CHECK(ExtendChain(1, false, true));
    BOOST_CHECK(latestTestVotes.size() > 0U);
    CheckLatestVotes();

    av->stop();
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

// test the vote generating RPC
BOOST_FIXTURE_TEST_CASE(generate_vote_rpc, VoteTestingSetup)
{
    vpwallets.insert(vpwallets.begin(), wallet.get());

    RegisterWalletRPCCommands(tableRPC);

    ExtendChain(consensus.nStakeValidationHeight - 1 - chainActive.Tip()->nHeight);

    uint256 blockHash = chainActive.Tip()->GetBlockHash();
    std::string blockHashStr = blockHash.GetHex();
    int blockHeight = chainActive.Tip()->nHeight;
    std::string blockHeightStr = std::to_string(blockHeight);
    BOOST_CHECK_EQUAL(chainActive.Tip()->pstakeNode->Winners().size(), consensus.nTicketsPerBlock);
    uint256 ticketHash = chainActive.Tip()->pstakeNode->Winners()[0];
    std::string ticketHashStr = ticketHash.GetHex();

    // missing required parameters

    BOOST_CHECK_THROW(CallRPC(std::string("generatevote")), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC(std::string("generatevote ") + blockHashStr), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC(std::string("generatevote ") + blockHashStr + " " + blockHeightStr), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr), std::runtime_error);

    // block hash

    BOOST_CHECK_THROW(CallRPC(std::string("generatevote ") + "0" + " " + blockHeightStr + " " + ticketHashStr + " " + "1"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC(std::string("generatevote ") + "test" + " " + blockHeightStr + " " + ticketHashStr + " " + "1"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC(std::string("generatevote ") + "abc!" + " " + blockHeightStr + " " + ticketHashStr + " " + "1"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC(std::string("generatevote ") + "123" + " " + blockHeightStr + " " + ticketHashStr + " " + "1"), std::runtime_error);

    // block height

    BOOST_CHECK_THROW(CallRPC(std::string("generatevote ") + blockHashStr + " " + "0" + " " + ticketHashStr + " " + "1"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC(std::string("generatevote ") + blockHashStr + " " + "test" + " " + ticketHashStr + " " + "1"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC(std::string("generatevote ") + blockHashStr + " " + "abc!" + " " + ticketHashStr + " " + "1"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC(std::string("generatevote ") + blockHashStr + " " + "123" + " " + ticketHashStr + " " + "1"), std::runtime_error);

    // ticket hash

    BOOST_CHECK_THROW(CallRPC(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + "0" + " " + "1"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + "test" + " " + "1"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + "abc!" + " " + "1"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + "123" + " " + "1"), std::runtime_error);

    // vote bits

    BOOST_CHECK_THROW(CallRPC(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "-1"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "65536"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "test"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "abc!"), std::runtime_error);

    // vote yes

    UniValue r;

    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "1"));
    BOOST_CHECK(find_value(r.get_obj(), "hex").isStr());

    uint256 voteHash = uint256S(find_value(r.get_obj(), "hex").get_str());
    CheckVote(voteHash, ticketHash, VoteData{1, blockHash, static_cast<uint32_t>(blockHeight), VoteBits(VoteBits::Rtt, true), 0, defaultEvb});

    // vote no

    ticketHash = chainActive.Tip()->pstakeNode->Winners()[1];
    ticketHashStr = ticketHash.GetHex();

    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "0"));

    voteHash = uint256S(find_value(r.get_obj(), "hex").get_str());
    CheckVote(voteHash, ticketHash, VoteData{1, blockHash, static_cast<uint32_t>(blockHeight), VoteBits(VoteBits::Rtt, false), 0, defaultEvb});

    // vote exhaustive

    ticketHash = chainActive.Tip()->pstakeNode->Winners()[2];
    ticketHashStr = ticketHash.GetHex();

    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "65535"));

    voteHash = uint256S(find_value(r.get_obj(), "hex").get_str());
    CheckVote(voteHash, ticketHash, VoteData{1, blockHash, static_cast<uint32_t>(blockHeight), 65535, 0, defaultEvb});

    // extended vote bits

    ticketHash = chainActive.Tip()->pstakeNode->Winners()[3];
    ticketHashStr = ticketHash.GetHex();

    BOOST_CHECK_THROW(CallRPC(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "1" + " " + "0"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "1" + " " + "1"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "1" + " " + "123"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "1" + " " + "abcde"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "1" + " " + "test"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "1" + " " + "abc!"), std::runtime_error);

    BOOST_CHECK_THROW(CallRPC(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "1" + " " + invalidLargeEvbString), std::runtime_error);

    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "1" + " " + "1234567890abcdef"));

    voteHash = uint256S(find_value(r.get_obj(), "hex").get_str());
    CheckVote(voteHash, ticketHash, VoteData{1, blockHash, static_cast<uint32_t>(blockHeight), VoteBits(VoteBits::Rtt, true), 0, ExtendedVoteBits("1234567890abcdef")});

    ticketHash = chainActive.Tip()->pstakeNode->Winners()[4];
    ticketHashStr = ticketHash.GetHex();

    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "1" + " " + validLargeEvbString));

    voteHash = uint256S(find_value(r.get_obj(), "hex").get_str());
    CheckVote(voteHash, ticketHash, VoteData{1, blockHash, static_cast<uint32_t>(blockHeight), VoteBits(VoteBits::Rtt, true), 0, ExtendedVoteBits(validLargeEvbString)});

    // encrypted wallet

    wallet->EncryptWallet(passphrase);
    BOOST_CHECK(wallet->IsLocked());

    ExtendChain(1);

    blockHash = chainActive.Tip()->GetBlockHash();
    blockHashStr = blockHash.GetHex();
    blockHeight = chainActive.Tip()->nHeight;
    blockHeightStr = std::to_string(blockHeight);
    BOOST_CHECK_EQUAL(chainActive.Tip()->pstakeNode->Winners().size(), consensus.nTicketsPerBlock);
    ticketHash = chainActive.Tip()->pstakeNode->Winners()[0];
    ticketHashStr = ticketHash.GetHex();

    BOOST_CHECK_THROW(CallRPC(std::string("generatevote ") + blockHashStr + " " + blockHeightStr + " " + ticketHashStr + " " + "1" + " " + defaultEvb.getHex()), std::runtime_error);

    vpwallets.erase(std::remove(vpwallets.begin(), vpwallets.end(), wallet.get()), vpwallets.end());
}

// test the automatic voter RPCs
BOOST_FIXTURE_TEST_CASE(auto_voter_rpc, VoteTestingSetup)
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

    CAutoVoter* av = wallet->GetAutoVoter();
    BOOST_CHECK(av != nullptr);

    BOOST_CHECK(!av->isStarted());

    CAutoVoterConfig& cfg = av->GetConfig();

    // Settings (write)

    BOOST_CHECK_THROW(CallRPC("setautovotervotebits"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("setautovotervotebits abc"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("setautovotervotebits -1"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("setautovotervotebits 65536"), std::runtime_error);

    BOOST_CHECK_NO_THROW(CallRPC("setautovotervotebits 0"));
    BOOST_CHECK(cfg.voteBits == VoteBits(0U));
    BOOST_CHECK_NO_THROW(CallRPC("setautovotervotebits 1234"));
    BOOST_CHECK(cfg.voteBits == VoteBits(1234U));
    BOOST_CHECK_NO_THROW(CallRPC("setautovotervotebits 65535"));
    BOOST_CHECK(cfg.voteBits == VoteBits(65535U));
    BOOST_CHECK_NO_THROW(CallRPC("setautovotervotebits 1"));
    BOOST_CHECK(cfg.voteBits == VoteBits(1U));

    BOOST_CHECK_THROW(CallRPC("setautovotervotebitsext"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("setautovotervotebitsext "), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("setautovotervotebitsext  "), std::runtime_error);

    std::string largeExtendedVoteBits;
    for (unsigned i = 0; i <= ExtendedVoteBits::maxSize(); ++i)
        largeExtendedVoteBits += "00";
    BOOST_CHECK_THROW(CallRPC("setautovotervotebitsext " + largeExtendedVoteBits), std::runtime_error);

    BOOST_CHECK(cfg.extendedVoteBits == defaultEvb);

    BOOST_CHECK_NO_THROW(CallRPC("setautovotervotebitsext 00"));
    BOOST_CHECK(cfg.extendedVoteBits == ExtendedVoteBits("00"));
    BOOST_CHECK_NO_THROW(CallRPC("setautovotervotebitsext 1234"));
    BOOST_CHECK(cfg.extendedVoteBits == ExtendedVoteBits("1234"));
    BOOST_CHECK_NO_THROW(CallRPC("setautovotervotebitsext 12345678900abcdef0"));
    BOOST_CHECK(cfg.extendedVoteBits == ExtendedVoteBits("12345678900abcdef0"));

    BOOST_CHECK(cfg.voteBits == VoteBits(1U));
    BOOST_CHECK(cfg.extendedVoteBits == ExtendedVoteBits("12345678900abcdef0"));
    BOOST_CHECK_EQUAL(cfg.passphrase, "");

    // Settings (read)

    UniValue r;
    BOOST_CHECK_NO_THROW(r = CallRPC("autovoterconfig"));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "autovote").get_bool(), false);
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "votebits").get_int(), 1);
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "votebitsext").get_str(), "12345678900abcdef0");

    // Start (with minimal settings)

    BOOST_CHECK_THROW(CallRPC("startautovoter"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("startautovoter \"\""), std::runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("startautovoter 65535"));

    BOOST_CHECK_EQUAL(cfg.autoVote, true);
    BOOST_CHECK(cfg.voteBits == VoteBits(65535U));
    BOOST_CHECK(cfg.extendedVoteBits == defaultEvb);
    BOOST_CHECK_EQUAL(cfg.passphrase, "");

    // Start (with full settings)

    av->stop();

    BOOST_CHECK_EQUAL(cfg.autoVote, false);

    BOOST_CHECK_NO_THROW(CallRPC("startautovoter 1234 123456789abcdef0"));

    BOOST_CHECK_EQUAL(cfg.autoVote, true);
    BOOST_CHECK(cfg.voteBits == VoteBits(1234U));
    BOOST_CHECK(cfg.extendedVoteBits == ExtendedVoteBits("123456789abcdef0"));
    BOOST_CHECK_EQUAL(cfg.passphrase, "");

    // Stop

    BOOST_CHECK_NO_THROW(CallRPC("stopautovoter"));

    BOOST_CHECK_EQUAL(cfg.autoVote, false);

    // Start with passphrase

    wallet->EncryptWallet(passphrase);
    BOOST_CHECK(wallet->IsLocked());

    BOOST_CHECK_THROW(CallRPC("startautovoter 1234 123456789abcdef0"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("startautovoter 1234 123456789abcdef0 wrongP4ssword!"), std::runtime_error);

    BOOST_CHECK_NO_THROW(CallRPC(std::string("startautovoter 1 123456789abcdef0 ") + std::string(passphrase.c_str())));

    BOOST_CHECK_EQUAL(cfg.autoVote, true);
    BOOST_CHECK(cfg.voteBits == VoteBits(1U));
    BOOST_CHECK(cfg.extendedVoteBits == ExtendedVoteBits("123456789abcdef0"));
    BOOST_CHECK_EQUAL(cfg.passphrase, passphrase);

    av->stop();

    BOOST_CHECK_EQUAL(cfg.autoVote, false);

    BOOST_CHECK_NO_THROW(CallRPC(std::string("startautovoter 1  ") + std::string(passphrase.c_str())));

    BOOST_CHECK_EQUAL(cfg.autoVote, true);
    BOOST_CHECK(cfg.voteBits == VoteBits(1U));
    BOOST_CHECK(cfg.extendedVoteBits == defaultEvb);
    BOOST_CHECK_EQUAL(cfg.passphrase, passphrase);

    av->stop();

    vpwallets.erase(std::remove(vpwallets.begin(), vpwallets.end(), wallet.get()), vpwallets.end());
}

BOOST_AUTO_TEST_SUITE_END()
