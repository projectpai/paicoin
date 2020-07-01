// Copyright (c) 2020 The Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/test/wallet_stake_test_fixture.h"
#include "script/script.h"
#include "validation.h"
#include "wallet/coincontrol.h"
#include "wallet/fees.h"
#include "net.h"
#include "rpc/server.h"
#include "rpc/client.h"
#include "miner.h"

#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string.hpp>
#include <univalue.h>

WalletStakeTestingSetup::WalletStakeTestingSetup()
    : TestingSetup(CBaseChainParams::REGTEST)
{
    CKey coinbaseKey;
    coinbaseKey.MakeNewKey(true);
    CPubKey coinbasePubKey{coinbaseKey.GetPubKey()};

    coinbaseScriptPubKey = CScript() << OP_DUP << OP_HASH160 << ToByteVector(coinbasePubKey.GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;

    ::bitdb.MakeMock();

    wallet.reset(new CWallet(std::unique_ptr<CWalletDBWrapper>(new CWalletDBWrapper(&bitdb, "wallet_stake_test.dat"))));

    bool firstRun;
    wallet->LoadWallet(firstRun);

    {
        LOCK(wallet->cs_wallet);
        wallet->AddKeyPubKey(coinbaseKey, coinbasePubKey);
    }

    wallet->SetBroadcastTransactions(true);
}

WalletStakeTestingSetup::~WalletStakeTestingSetup()
{
    wallet.reset();

    ::bitdb.Flush(true);
    ::bitdb.Reset();
}

uint256 WalletStakeTestingSetup::SendMoney(const std::vector<std::pair<CTxDestination, CAmount>> recipients)
{
    BOOST_CHECK(recipients.size() > 0);

    CWalletTx wtx;
    CValidationState state;
    CAmount feeRequired;
    std::string error;
    std::vector<CRecipient> rs;
    int changePos{0};

    for (auto& recipient : recipients) {
        BOOST_CHECK_NE(recipient.first.which(), 0);
        BOOST_CHECK(MoneyRange(recipient.second));

        CRecipient r{GetScriptForDestination(recipient.first), recipient.second, false};
        rs.push_back(r);
        ++changePos;
    }

    CReserveKey reservekey{wallet.get()};
    BOOST_CHECK(wallet->CreateTransaction(rs, wtx, reservekey, feeRequired, changePos, error, CCoinControl()));
    BOOST_CHECK(wallet->CommitTransaction(wtx, reservekey, g_connman.get(), state));
    BOOST_CHECK(state.IsValid());

    return wtx.GetHash();
}

uint256 WalletStakeTestingSetup::AddTicketTx(bool useVsp, bool foreign)
{
    const CBlockIndex* chainTip = chainActive.Tip();
    BOOST_CHECK_NE(chainTip, nullptr);

    // ticket settings

    CKey ticketKey;
    CPubKey ticketPubKey;
    CTxDestination ticketKeyId = CNoDestination();
    if (foreign) {
        ticketKey = foreignData.NewTicketKey();
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
        vspKey = foreignData.NewVspKey();
        vspPubKey = vspKey.GetPubKey();
        vspKeyId = vspPubKey.GetID();
        BOOST_CHECK_NE(vspKeyId.which(), 0);
    }

    const CAmount ticketPrice = CalculateNextRequiredStakeDifficulty(chainTip, consensus);
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
            splitTxHash = SendMoney({std::make_pair(vspKeyId, vspFee), std::make_pair(ticketKeyId, contributedAmount - vspFee)});
        else
            splitTxHash = SendMoney({std::make_pair(ticketKeyId, contributedAmount)});
    } else {
        CWalletError we;
        std::tie(splitTxHash, we) = wallet->CreateTicketPurchaseSplitTx("", ticketPrice, ticketFee, vspFee);
        BOOST_CHECK_EQUAL(we.code, CWalletError::SUCCESSFUL);
    }
    BOOST_CHECK(splitTxHash != emptyData.hash);

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

    BuyTicketData buyTicketData{1};
    CScript declScript = GetScriptForBuyTicketDecl(buyTicketData);
    mtx.vout.push_back(CTxOut(0, declScript));

    CScript redeemScript; // only for VSP

    if (useVsp) {
        redeemScript = GetScriptForMultisig(1, {vspPubKey, ticketPubKey});

        if (!foreign)
            wallet->AddCScript(redeemScript);

        const CScript& paymentScript = GetScriptForDestination(CScriptID{redeemScript});
        CScriptID paymentScriptId{paymentScript};

        mtx.vout.push_back(CTxOut(ticketPrice, paymentScript));

        TicketContribData ticketContribData{1, paymentScriptId, vspFee, 0, TicketContribData::DefaultFeeLimit};
        CScript contributorInfoScript = GetScriptForTicketContrib(ticketContribData);
        mtx.vout.push_back(CTxOut(0, contributorInfoScript));

        CScript changeScript = GetScriptForDestination(paymentScriptId);
        mtx.vout.push_back(CTxOut(0, changeScript));
    } else {
        CScript ticketScript = GetScriptForDestination(ticketKeyId);
        mtx.vout.push_back(CTxOut(ticketPrice, ticketScript));
    }

    TicketContribData ticketContribData{1, ticketKeyId, contributedAmount - vspFee, 0, TicketContribData::DefaultFeeLimit};
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

        const uint256 hash = mtx.GetHash();
        foreignData.AddTicketKey(ticketKey, hash);
        if (useVsp) {
            foreignData.AddRedeemScript(redeemScript, hash);
            foreignData.AddVspKey(vspKey, hash);
        }

        foreignData.AddTicket(MakeTransactionRef(std::move(mtx)));

        return hash;
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

    return emptyData.hash;
}

uint256 WalletStakeTestingSetup::AddVoteTx(const uint256& ticketHash, bool foreignSignature)
{
    // block being voted

    const CBlockIndex *chainTip = chainActive.Tip();
    BOOST_CHECK_NE(chainTip, nullptr);

    const uint256& hash{chainTip->GetBlockHash()};
    BOOST_CHECK(hash != emptyData.hash);

    const int height = chainTip->nHeight;
    BOOST_CHECK_GT(height, 0);

    std::string reason;

    // ticket

    BOOST_CHECK(ticketHash != emptyData.hash);

    CTransactionRef ticket;

    if (foreignSignature) {
        ticket = foreignData.Ticket(ticketHash);
        BOOST_CHECK(!ticket->IsNull());
    } else {
        const CWalletTx* wticket = wallet->GetWalletTx(ticketHash);
        BOOST_CHECK_NE(wticket, nullptr);

        ticket = wticket->tx;
    }

    BOOST_CHECK_NE(ticket.get(), nullptr);

    BOOST_CHECK_MESSAGE(ValidateBuyTicketStructure(*ticket, reason), ((reason.size() > 0) ? reason : "ValidateBuyTicketStructure"));

    std::vector<TicketContribData> contributions;
    CAmount totalContribution{0};
    CAmount totalVoteFeeLimit{0};
    CAmount totalRevocationFeeLimit{0};
    BOOST_CHECK(ParseTicketContribs(*ticket, contributions, totalContribution, totalVoteFeeLimit, totalRevocationFeeLimit));

    const CAmount ticketPrice = ticket->vout[ticketStakeOutputIndex].nValue;
    BOOST_CHECK(MoneyRange(ticketPrice));

    const CAmount voteSubsidy = GetVoterSubsidy(height + 1, consensus);
    BOOST_CHECK_NE(voteSubsidy, 0);

    std::vector<CAmount> rewards = CalculateNetRemunerations(contributions, ticketPrice, voteSubsidy);
    BOOST_CHECK(rewards.size() == contributions.size());

    // vote

    CMutableTransaction mtx;

    // inputs

    mtx.vin.push_back(CTxIn(COutPoint(), consensus.stakeBaseSigScript));
    mtx.vin.push_back(CTxIn(COutPoint(ticketHash, ticketStakeOutputIndex)));

    // outputs

    VoteData voteData{1, hash, static_cast<uint32_t>(height), VoteBits::rttAccepted, 0, extendedVoteBitsData.empty};
    CScript declScript = GetScriptForVoteDecl(voteData);
    mtx.vout.push_back(CTxOut(0, declScript));

    for (unsigned i = 0; i < contributions.size(); ++i) {
        const TicketContribData& contrib = contributions[i];

        const CAmount& reward = rewards[i];
        BOOST_CHECK(MoneyRange(reward));

        const CScript& rewardDestination = contrib.whichAddr == 1 ? GetScriptForDestination(CKeyID(contrib.rewardAddr)) : GetScriptForDestination(CScriptID(contrib.rewardAddr));
        mtx.vout.push_back(CTxOut(reward, rewardDestination));
    }

    // signature

    CTransaction tx(mtx);
    const CScript& scriptPubKey = ticket->vout[ticketStakeOutputIndex].scriptPubKey;
    const CAmount& amount = ticket->vout[ticketStakeOutputIndex].nValue;

    SignatureData sigdata;

    if (foreignSignature) {
        CBasicKeyStore keyStore;

        CKey foreignKey = foreignData.KeyForTicket(ticketHash);
        CKey vspKey = foreignData.KeyForVsp(ticketHash);
        CScript redeemScript = foreignData.RedeemScript(ticketHash);

        if (foreignKey.IsValid())
            keyStore.AddKey(foreignKey);
        if (vspKey.IsValid())
            keyStore.AddKey(vspKey);
        if (!redeemScript.empty())
            keyStore.AddCScript(redeemScript);

        BOOST_CHECK(ProduceSignature(TransactionSignatureCreator(&keyStore, &tx, voteStakeInputIndex, amount, SIGHASH_ALL), scriptPubKey, sigdata));
    } else {
        BOOST_CHECK(ProduceSignature(TransactionSignatureCreator(wallet.get(), &tx, voteStakeInputIndex, amount, SIGHASH_ALL), scriptPubKey, sigdata));
    }

    UpdateTransaction(mtx, voteStakeInputIndex, sigdata);

    // commitment

    if (foreignSignature) {
        BOOST_CHECK(mempool.addUnchecked(mtx.GetHash(), CTxMemPoolEntry(MakeTransactionRef(mtx), 0, GetTime(), static_cast<unsigned int>(chainTip->nHeight+1), false, 0, LockPoints())));
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

    return emptyData.hash;
}

uint256 WalletStakeTestingSetup::AddRevokeTx(const uint256& ticketHash, bool foreignSignature)
{
    // ticket

    const CBlockIndex *chainTip = chainActive.Tip();
    BOOST_CHECK_NE(chainTip, nullptr);

    std::string reason;

    BOOST_CHECK(ticketHash != emptyData.hash);

    CTransactionRef ticket;

    if (foreignSignature) {
        ticket = foreignData.Ticket(ticketHash);
        BOOST_CHECK(!ticket->IsNull());
    } else {
        const CWalletTx* wticket = wallet->GetWalletTx(ticketHash);
        BOOST_CHECK_NE(wticket, nullptr);

        ticket = wticket->tx;
    }

    BOOST_CHECK_NE(ticket.get(), nullptr);

    BOOST_CHECK_MESSAGE(ValidateBuyTicketStructure(*ticket, reason), ((reason.size() > 0) ? reason : "ValidateBuyTicketStructure"));

    // stake, contributions, fee and refunds

    const CAmount ticketPrice = ticket->vout[ticketStakeOutputIndex].nValue;
    BOOST_CHECK(MoneyRange(ticketPrice));

    std::vector<TicketContribData> contributions;
    CAmount totalContribution{0};
    CAmount totalVoteFeeLimit{0};
    CAmount totalRevocationFeeLimit{0};
    BOOST_CHECK(ParseTicketContribs(*ticket, contributions, totalContribution, totalVoteFeeLimit, totalRevocationFeeLimit));

    FeeCalculation feeCalc;
    CAmount fee = GetMinimumFee(static_cast<unsigned int>(GetEstimatedSizeOfRevokeTicketTx(contributions.size(), false)), CCoinControl(), ::mempool, ::feeEstimator, &feeCalc);

    std::vector<CAmount> refunds = CalculateNetRemunerations(contributions, ticketPrice, 0, fee, FeeDistributionPolicy::ProportionalFee);
    BOOST_CHECK(refunds.size() == contributions.size());

    // vote

    CMutableTransaction mtx;

    // inputs

    mtx.vin.push_back(CTxIn(COutPoint(ticketHash, ticketStakeOutputIndex)));

    // outputs

    RevokeTicketData revokeData{1};
    CScript declScript = GetScriptForRevokeTicketDecl(revokeData);
    mtx.vout.push_back(CTxOut(0, declScript));

    for (unsigned i = 0; i < contributions.size(); ++i) {
        const TicketContribData& contrib = contributions[i];

        const CAmount& refund = refunds[i];
        BOOST_CHECK(MoneyRange(refund));

        const CScript& refundDestination = contrib.whichAddr == 1 ? GetScriptForDestination(CKeyID(contrib.rewardAddr)) : GetScriptForDestination(CScriptID(contrib.rewardAddr));
        mtx.vout.push_back(CTxOut(refund, refundDestination));
    }

    // signature

    std::map<uint256, CWalletTx>::const_iterator mi = wallet->mapWallet.find(mtx.vin[revocationStakeInputIndex].prevout.hash);
    BOOST_CHECK(mi != wallet->mapWallet.end());
    BOOST_CHECK_LT(mtx.vin[revocationStakeInputIndex].prevout.n, static_cast<uint32_t>(mi->second.tx->vout.size()));

    CTransaction tx(mtx);
    const CScript& scriptPubKey = mi->second.tx->vout[tx.vin[revocationStakeInputIndex].prevout.n].scriptPubKey;
    const CAmount& amount = mi->second.tx->vout[tx.vin[revocationStakeInputIndex].prevout.n].nValue;

    SignatureData sigdata;

    if (foreignSignature) {
        CBasicKeyStore keyStore;

        CKey foreignKey = foreignData.KeyForTicket(ticketHash);
        CKey vspKey = foreignData.KeyForVsp(ticketHash);
        CScript redeemScript = foreignData.RedeemScript(ticketHash);

        if (foreignKey.IsValid())
            keyStore.AddKey(foreignKey);
        if (vspKey.IsValid())
            keyStore.AddKey(vspKey);
        if (!redeemScript.empty())
            keyStore.AddCScript(redeemScript);

        BOOST_CHECK(ProduceSignature(TransactionSignatureCreator(&keyStore, &tx, revocationStakeInputIndex, amount, SIGHASH_ALL), scriptPubKey, sigdata));
    } else {
        BOOST_CHECK(ProduceSignature(TransactionSignatureCreator(wallet.get(), &tx, revocationStakeInputIndex, amount, SIGHASH_ALL), scriptPubKey, sigdata));
    }

    UpdateTransaction(mtx, revocationStakeInputIndex, sigdata);

    // commitment

    if (foreignSignature) {
        BOOST_CHECK(mempool.addUnchecked(mtx.GetHash(), CTxMemPoolEntry(MakeTransactionRef(mtx), 0, GetTime(), static_cast<unsigned int>(chainTip->nHeight+1), false, 0, LockPoints())));
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

    return emptyData.hash;
}

bool WalletStakeTestingSetup::MempoolHasTicket() {
    std::vector<TxMempoolInfo> txnInfos = mempool.infoAll();
    for (size_t j = 0; j < txnInfos.size(); ++j)
        if (ParseTxClass(*(txnInfos[j].tx)) == TX_BuyTicket)
            return true;

    return false;
}

bool WalletStakeTestingSetup::MempoolHasEnoughTicketsForBlock() {
    int count{0};
    std::vector<TxMempoolInfo> txnInfos = mempool.infoAll();
    for (size_t j = 0; j < txnInfos.size(); ++j)
        if (ParseTxClass(*(txnInfos[j].tx)) == TX_BuyTicket)
            if ((++count) >= consensus.nTicketsPerBlock)
                return true;

    return false;
}

bool WalletStakeTestingSetup::MempoolHasVoteForTicket(const uint256& ticketHash) {
    std::vector<TxMempoolInfo> txnInfos = mempool.infoAll();
    for (size_t j = 0; j < txnInfos.size(); ++j) {
        const CTransactionRef tx = txnInfos[j].tx;
        if (ParseTxClass(*(tx.get())) == TX_Vote && tx->vin.size() > voteStakeInputIndex && tx->vin[voteStakeInputIndex].prevout.hash == ticketHash)
            return true;
    }

    return false;
}

bool WalletStakeTestingSetup::MempoolHasEnoughVotesForBlock() {
    const int majority{(consensus.nTicketsPerBlock / 2) + 1};
    int count{0};
    std::vector<TxMempoolInfo> txnInfos = mempool.infoAll();
    for (size_t j = 0; j < txnInfos.size(); ++j)
        if (ParseTxClass(*(txnInfos[j].tx)) == TX_Vote)
            if ((++count) >= majority)
                return true;

    return false;
}

CBlock WalletStakeTestingSetup::CreateAndProcessBlock()
{
    std::unique_ptr<CBlockTemplate> pblocktemplate = BlockAssembler(params).CreateNewBlock(coinbaseScriptPubKey);
    CBlock& block = pblocktemplate->block;

    unsigned int extraNonce = 0;
    IncrementExtraNonce(&block, chainActive.Tip(), extraNonce);

    while (!CheckProofOfWork(block.GetHash(), block.nBits, consensus)) ++block.nNonce;

    std::shared_ptr<const CBlock> sharedBlock = std::make_shared<const CBlock>(block);
    ProcessNewBlock(params, sharedBlock, true, nullptr);

    CBlock result = block;
    return result;
}

bool WalletStakeTestingSetup::ExtendChain(int blockCount, bool withImplicitVotes, bool useVsp, bool foreign, bool onlyMajorityVotes, bool foreignSignature)
{
    if (blockCount <= 0)
        return false;

    LOCK2(cs_main, wallet->cs_wallet);

    txsInMempoolBeforeLastBlock.clear();
    if (blockCount > 0) {
        std::vector<TxMempoolInfo> txInfos = mempool.infoAll();
        for (size_t j = 0; j < txInfos.size(); ++j)
            txsInMempoolBeforeLastBlock.push_back(MakeTransactionRef(*(txInfos[j].tx)));
    }

    votesInLastBlock.clear();
    revocationsInLastBlock.clear();

    bool result = true;

    bool shouldRelock = wallet->IsLocked();

    for (int i = 0; i < blockCount; ++i) {
        if (shouldRelock)
            wallet->Unlock(passphrase);

        if (chainActive.Tip()->nHeight >= consensus.nStakeValidationHeight - 1 - consensus.nTicketMaturity - 1)
            while (!MempoolHasEnoughTicketsForBlock())
                BOOST_CHECK(AddTicketTx(useVsp, foreign) != emptyData.hash);

        if (withImplicitVotes && chainActive.Tip()->nHeight >= consensus.nStakeValidationHeight - 1)
            for (const uint256& ticketHash : chainActive.Tip()->pstakeNode->Winners()) {
                if (onlyMajorityVotes) {
                    if (!MempoolHasEnoughVotesForBlock() && !MempoolHasVoteForTicket(ticketHash))
                        BOOST_CHECK(AddVoteTx(ticketHash, foreignSignature) != emptyData.hash);
                } else if (!MempoolHasVoteForTicket(ticketHash))
                    BOOST_CHECK(AddVoteTx(ticketHash, foreignSignature) != emptyData.hash);
            }

        if (shouldRelock)
            wallet->Lock();

        try {
            CBlock b = CreateAndProcessBlock();

            wallet->ScanForWalletTransactions(chainActive.Tip(), nullptr);

            const StakeSlice votes(b.vtx, TX_Vote);
            votesInLastBlock.insert(votesInLastBlock.end(), votes.begin(), votes.end());

            const StakeSlice revocations(b.vtx, TX_RevokeTicket);
            revocationsInLastBlock.insert(revocationsInLastBlock.end(), revocations.begin(), revocations.end());
        } catch (...) {
            result = false;
        }
    }

    return result;
}

bool WalletStakeTestingSetup::GetSomeCoins()
{
    return ExtendChain(COINBASE_MATURITY + 100);
}

void WalletStakeTestingSetup::CheckTicket(const CTransaction& ticket, std::vector<TicketContribData> expectedContributions, bool validateAmounts)
{
    std::string reason;
    BOOST_CHECK_MESSAGE(ValidateBuyTicketStructure(ticket, reason), ((reason.size() > 0) ? reason : "ValidateBuyTicketStructure"));

    for (unsigned i = 2; i < expectedContributions.size(); ++i) {
        TicketContribData contribution;
        TicketContribData expectedContribution = expectedContributions[i];

        BOOST_CHECK(ParseTicketContrib(ticket, 2*i, contribution));

        BOOST_CHECK_EQUAL(contribution.nVersion, expectedContribution.nVersion);
        BOOST_CHECK(contribution.rewardAddr == expectedContribution.rewardAddr);
        BOOST_CHECK_EQUAL(contribution.whichAddr, expectedContribution.whichAddr);
        if (validateAmounts)
            BOOST_CHECK_EQUAL(contribution.contributedAmount, expectedContribution.contributedAmount);
        BOOST_CHECK_EQUAL(contribution.voteFeeLimit(), expectedContribution.voteFeeLimit());
        BOOST_CHECK_EQUAL(contribution.revocationFeeLimit(), expectedContribution.revocationFeeLimit());
    }
}

void WalletStakeTestingSetup::CheckVote(const CTransaction& vote, const CTransaction& ticket, bool inMempool)
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

    std::vector<TicketContribData> contributions;
    CAmount totalContribution{0};
    CAmount totalVoteFeeLimit{0};
    CAmount totalRevocationFeeLimit{0};
    BOOST_CHECK(ParseTicketContribs(ticket, contributions, totalContribution, totalVoteFeeLimit, totalRevocationFeeLimit));

    const CAmount& ticketPrice = ticket.vout[ticketStakeOutputIndex].nValue;
    const CAmount& voteSubsidy = GetVoterSubsidy(inMempool ? chainTip->nHeight + 1 : chainTip->nHeight, consensus);

    std::vector<CAmount> remunerations = CalculateNetRemunerations(contributions, ticketPrice, voteSubsidy /*zero fees*/);
    BOOST_CHECK(remunerations.size() == contributions.size());

    for (unsigned i = voteRewardOutputIndex; i < vote.vout.size(); ++i) {
        const TicketContribData& contrib = contributions[i-voteRewardOutputIndex];

        const CScript& rewardDestination = contrib.whichAddr == 1 ? GetScriptForDestination(CKeyID(contrib.rewardAddr)) : GetScriptForDestination(CScriptID(contrib.rewardAddr));
        BOOST_CHECK(vote.vout[i].scriptPubKey == rewardDestination);

        const CAmount& reward = remunerations[i-voteRewardOutputIndex];
        BOOST_CHECK_EQUAL(vote.vout[i].nValue, reward);
    }
}

void WalletStakeTestingSetup::CheckVote(const uint256& voteHash, const uint256& ticketHash, const VoteData& voteData)
{
    BOOST_CHECK(voteHash != emptyData.hash);
    BOOST_CHECK(ticketHash != emptyData.hash);

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

void WalletStakeTestingSetup::CheckLatestVotes(bool inMempool, bool ifAllMine)
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

        votes.insert(votes.end(), votesInLastBlock.begin(), votesInLastBlock.end());
    }

    BOOST_CHECK(votes.size() == winners.size());

    for (CTransactionRef vote : votes) {
        // vote transaction

        BOOST_CHECK_MESSAGE(ValidateVoteStructure(*vote, reason), ((reason.size() > 0) ? reason : "ValidateVoteStructure"));

        const uint256 ticketHash = vote->vin[voteStakeInputIndex].prevout.hash;
        BOOST_CHECK(std::find_if(winners.begin(), winners.end(), [&ticketHash](const uint256 hash) { return hash == ticketHash; }) != winners.end());

        BOOST_CHECK(vote->vin[voteStakeInputIndex].prevout.n == ticketStakeOutputIndex);

        // wallet transactions

        if (ifAllMine) {
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

void WalletStakeTestingSetup::CheckRevocation(const CTransaction& revocation, const CTransaction& ticket)
{
    // ticket

    CheckTicket(ticket);

    std::string reason;

    // structure

    BOOST_CHECK_MESSAGE(ValidateRevokeTicketStructure(revocation, reason), ((reason.size() > 0) ? reason : "ValidateRevokeTicketStructure"));

    // inputs

    BOOST_CHECK(revocation.vin[revocationStakeInputIndex].prevout.hash == ticket.GetHash());
    BOOST_CHECK_EQUAL(revocation.vin[revocationStakeInputIndex].prevout.n, ticketStakeOutputIndex);

    // reward outputs

    std::vector<TicketContribData> contributions;
    CAmount totalContribution{0};
    CAmount totalVoteFeeLimit{0};
    CAmount totalRevocationFeeLimit{0};
    BOOST_CHECK(ParseTicketContribs(ticket, contributions, totalContribution, totalVoteFeeLimit, totalRevocationFeeLimit));

    const CAmount& ticketPrice = ticket.vout[ticketStakeOutputIndex].nValue;

    std::vector<CAmount> remunerations = CalculateNetRemunerations(contributions, ticketPrice, 0 /*zero fees*/);
    BOOST_CHECK(remunerations.size() == contributions.size());

    for (unsigned i = revocationRefundOutputIndex; i < revocation.vout.size(); ++i) {
        const TicketContribData& contrib = contributions[i-revocationRefundOutputIndex];

        const CScript& refundDestination = contrib.whichAddr == 1 ? GetScriptForDestination(CKeyID(contrib.rewardAddr)) : GetScriptForDestination(CScriptID(contrib.rewardAddr));
        BOOST_CHECK(revocation.vout[i].scriptPubKey == refundDestination);

        const CAmount& refund = remunerations[i-revocationRefundOutputIndex];
        BOOST_CHECK_LE(revocation.vout[i].nValue, refund);
    }
}

void WalletStakeTestingSetup::CheckRevocation(const uint256& revocationHash, const uint256& ticketHash)
{
    BOOST_CHECK(revocationHash != emptyData.hash);
    BOOST_CHECK(ticketHash != emptyData.hash);

    const CWalletTx* voteWtx = wallet->GetWalletTx(revocationHash);
    BOOST_CHECK_NE(voteWtx, nullptr);

    const CTransactionRef vote = voteWtx->tx;
    BOOST_CHECK_NE(vote.get(), nullptr);

    const CWalletTx* ticketWtx = wallet->GetWalletTx(ticketHash);
    BOOST_CHECK_NE(ticketWtx, nullptr);

    const CTransactionRef ticket = ticketWtx->tx;
    BOOST_CHECK_NE(ticket.get(), nullptr);

    CheckRevocation(*vote, *ticket);
}

void WalletStakeTestingSetup::CheckLatestRevocations(bool inMempool, bool ifAllMine)
{
    std::string reason;

    HashVector missed;
    std::vector<CTransactionRef> revocations;

    if (inMempool) {
        missed = chainActive.Tip()->pstakeNode->MissedTickets();

        std::vector<TxMempoolInfo> txnInfos = mempool.infoAll();
        for (size_t j = 0; j < txnInfos.size(); ++j)
            if (ParseTxClass(*(txnInfos[j].tx)) == TX_RevokeTicket)
                revocations.push_back(txnInfos[j].tx);
    } else {
        missed = chainActive.Tip()->pprev->pstakeNode->MissedTickets();

        revocations.insert(revocations.end(), revocationsInLastBlock.begin(), revocationsInLastBlock.end());
    }

    BOOST_CHECK(revocations.size() == missed.size());

    for (CTransactionRef revocation : revocations) {
        // vote transaction

        BOOST_CHECK_MESSAGE(ValidateRevokeTicketStructure(*revocation, reason), ((reason.size() > 0) ? reason : "ValidateRevokeTicketStructure"));

        const uint256 ticketHash = revocation->vin[revocationStakeInputIndex].prevout.hash;
        BOOST_CHECK(std::find_if(missed.begin(), missed.end(), [&ticketHash](const uint256 hash) { return hash == ticketHash; }) != missed.end());

        BOOST_CHECK(revocation->vin[revocationStakeInputIndex].prevout.n == ticketStakeOutputIndex);

        // wallet transactions

        if (ifAllMine) {
            const CWalletTx* revocationWtx = wallet->GetWalletTx(revocation->GetHash());
            BOOST_CHECK(revocationWtx != nullptr);
            BOOST_CHECK(revocationWtx->tx != nullptr);

            const CWalletTx* ticketWtx = wallet->GetWalletTx(ticketHash);
            BOOST_CHECK(ticketWtx != nullptr);
            BOOST_CHECK(ticketWtx->tx != nullptr);

            CheckRevocation(*(revocationWtx->tx), *(ticketWtx->tx));
        }
    }
}

UniValue WalletStakeTestingSetup::CallRpc(std::string args)
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

WalletStakeTestingSetup::EmptyData::EmptyData()
    : hash{uint256()}, vector{std::vector<unsigned char>()}, string(std::string())
{
}

WalletStakeTestingSetup::ExtendedVoteBitsData::ExtendedVoteBitsData()
    : empty{ExtendedVoteBits()},
      validLargeVector{std::vector<unsigned char>(ExtendedVoteBits::maxSize(), 0xab)},
      invalidLargeVector(std::vector<unsigned char>(ExtendedVoteBits::maxSize() + 1, 0xab))
{
    for (unsigned i = 0; i < ExtendedVoteBits::maxSize(); ++i)
        validLargeString += "ab";

    invalidLargeString = validLargeString;
    invalidLargeString += "ab";
}

WalletStakeTestingSetup::ForeignData::ForeignData()
{
}

void WalletStakeTestingSetup::ForeignData::AddTicket(CTransactionRef ticket)
{
    tickets.push_back(ticket);
}

CTransactionRef WalletStakeTestingSetup::ForeignData::Ticket(const uint256& ticketHash)
{
    auto tx = std::find_if(tickets.begin(), tickets.end(), [&](const CTransactionRef& e) {
        return e->GetHash() == ticketHash;
    });

    if (tx != tickets.end())
        return *tx;

    return MakeTransactionRef();
}

void WalletStakeTestingSetup::ForeignData::AddRedeemScript(const CScript& script, const uint256& ticketHash)
{
    redeemScripts.push_back(std::make_pair(script, ticketHash));
}

CScript WalletStakeTestingSetup::ForeignData::RedeemScript(const uint256& ticketHash)
{
    auto script = std::find_if(redeemScripts.begin(), redeemScripts.end(), [&](const std::pair<CScript, uint256>& e) {
        return e.second == ticketHash;
    });

    if (script != redeemScripts.end())
        return script->first;

    return CScript();
}

void WalletStakeTestingSetup::ForeignData::AddTicketKey(const CKey& key, const uint256& ticketHash)
{
    if (!key.IsValid())
        return;

    const CPubKey& pubKey{key.GetPubKey()};
    ticketKeys.push_back(std::make_tuple(key, pubKey, pubKey.GetID(), ticketHash));
}

const CKey WalletStakeTestingSetup::ForeignData::NewTicketKey()
{
    CKey key;
    key.MakeNewKey(true);

    return key;
}

bool WalletStakeTestingSetup::ForeignData::HasTicketKey(const CKey& key)
{
    if (!key.IsValid())
        return false;

    return std::find_if(ticketKeys.begin(), ticketKeys.end(), [&](const std::tuple<CKey, CPubKey, CKeyID, uint256>& e) {
        return std::get<0>(e) == key;
    }) != ticketKeys.end();
}

bool WalletStakeTestingSetup::ForeignData::HasTicketPubKey(const CPubKey& pubKey)
{
    if (!pubKey.IsValid())
        return false;

    return std::find_if(ticketKeys.begin(), ticketKeys.end(), [&](const std::tuple<CKey, CPubKey, CKeyID, uint256>& e) {
        return std::get<1>(e) == pubKey;
    }) != ticketKeys.end();
}

bool WalletStakeTestingSetup::ForeignData::HasTicketKeyId(const CKeyID& keyId)
{
    return std::find_if(ticketKeys.begin(), ticketKeys.end(), [&](const std::tuple<CKey, CPubKey, CKeyID, uint256>& e) {
        return std::get<2>(e) == keyId;
    }) != ticketKeys.end();
}

CKey WalletStakeTestingSetup::ForeignData::KeyForTicket(const uint256& ticketHash)
{
    const auto it = std::find_if(ticketKeys.begin(), ticketKeys.end(), [&](const std::tuple<CKey, CPubKey, CKeyID, uint256>& e) {
        return std::get<3>(e) == ticketHash;
    });

    if (it != ticketKeys.end())
        return std::get<0>(*it);

    return CKey();
}

void WalletStakeTestingSetup::ForeignData::AddVspKey(const CKey& key, const uint256& ticketHash)
{
    if (!key.IsValid())
        return;

    const CPubKey& pubKey{key.GetPubKey()};
    vspKeys.push_back(std::make_tuple(key, pubKey, pubKey.GetID(), ticketHash));
}

const CKey WalletStakeTestingSetup::ForeignData::NewVspKey()
{
    CKey key;
    key.MakeNewKey(true);

    return key;
}

bool WalletStakeTestingSetup::ForeignData::HasVspKey(const CKey& key)
{
    if (!key.IsValid())
        return false;

    return std::find_if(vspKeys.begin(), vspKeys.end(), [&](const std::tuple<CKey, CPubKey, CKeyID, uint256>& e) {
        return std::get<0>(e) == key;
    }) != vspKeys.end();
}

bool WalletStakeTestingSetup::ForeignData::HasVspPubKey(const CPubKey& pubKey)
{
    if (!pubKey.IsValid())
        return false;

    return std::find_if(vspKeys.begin(), vspKeys.end(), [&](const std::tuple<CKey, CPubKey, CKeyID, uint256>& e) {
        return std::get<1>(e) == pubKey;
    }) != vspKeys.end();
}

bool WalletStakeTestingSetup::ForeignData::HasVspKeyId(const CKeyID& keyId)
{
    return std::find_if(vspKeys.begin(), vspKeys.end(), [&](const std::tuple<CKey, CPubKey, CKeyID, uint256>& e) {
        return std::get<2>(e) == keyId;
    }) != vspKeys.end();
}

CKey WalletStakeTestingSetup::ForeignData::KeyForVsp(const uint256& ticketHash)
{
    const auto it = std::find_if(vspKeys.begin(), vspKeys.end(), [&](const std::tuple<CKey, CPubKey, CKeyID, uint256>& e) {
        return std::get<3>(e) == ticketHash;
    });

    if (it != vspKeys.end())
        return std::get<0>(*it);

    return CKey();
}
