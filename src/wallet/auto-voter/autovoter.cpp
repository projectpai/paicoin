//
// Copyright (c) 2017-2020 Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//


#include "autovoter.h"
#include "wallet/wallet.h"
#include "validation.h"
#include <numeric>

CAutoVoter::CAutoVoter(CWallet* wallet) :
    configured(false)
{
    pwallet = wallet;
}

CAutoVoter::~CAutoVoter()
{
    stop();
}

void CAutoVoter::NewPoWValidBlock(const CBlockIndex *pindex, const std::shared_ptr<const CBlock>&)
{
    // we have to wait until the entire blockchain is downloaded
    if (IsInitialBlockDownload())
        return;

    LogPrintf("CAutoVoter: received NewPoWValidBlock, height=%d, pstake=%p\n", pindex->nHeight, pindex->pstakeNode);

    DoVote(pindex);
}

void CAutoVoter::UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *, bool fInitialDownload)
{
    // we have to wait until the entire blockchain is downloaded
    if (fInitialDownload)
        return;

    DoVote(pindexNew);
}

void CAutoVoter::DoVote(const CBlockIndex *pindexNew)
{
    if (pwallet == nullptr)
        return;

    // if not configured yet, do nothing
    if (!configured.load())
        return;

    if (!config.autoVote)
        return;

    if (pindexNew == nullptr)
        return;

    LOCK2(cs_main, pwallet->cs_wallet);

    const CBlockIndex* activeTip = chainActive.Tip();

    const int& tipHeight = activeTip->nHeight;
    if (tipHeight < Params().GetConsensus().nStakeValidationHeight - 1)
        return;

    // do not try to vote when votes are not required anymore
    if (tipHeight >= Params().GetConsensus().nVotesNotRequiredHeight)
        return;

    std::vector<std::string> voteHashes;
    std::string voteHash;
    CWalletError we;

    // unlock wallet
    bool shouldRelock = pwallet->IsLocked();
    if (shouldRelock && ! pwallet->Unlock(config.passphrase)) {
        LogPrintf("CAutoVoter: Unlocking wallet: Wrong passphrase\n");
        return;
    }

    // send all possible votes for the block if this is at least at the
    // same height with the chain tip.
    auto voteForBlock = [&] (const CBlockIndex* block) {
        if (block->nStatus & BLOCK_FAILED_MASK)
            return;

        if (block->pstakeNode == nullptr)
            return;

        const int& blockHeight = block->nHeight;

        if (blockHeight < tipHeight)
            return;

        if (blockHeight < Params().GetConsensus().nStakeValidationHeight - 1)
            return;

        const uint256& blockHash = block->GetBlockHash();
        if (blockHash == uint256())
            return;

        // verify each winning ticket in the previous block and
        // if it belongs to the wallet, cast a vote according to the
        // current settings

        for (const uint256& ticketHash : block->pstakeNode->Winners()) {
            if (!pwallet->IsMyTicket(ticketHash))
                continue;

            std::tie(voteHash, we) = pwallet->Vote(ticketHash, blockHash, blockHeight, config.voteBits, config.extendedVoteBits);

            if (we.code == CWalletError::SUCCESSFUL && !voteHash.empty())
                voteHashes.push_back(voteHash);
            else
                LogPrintf("CAutoVoter: Failed to vote: (%d) %s - (%s)\n", we.code, we.message.c_str(), voteHash.c_str());
        }
    };

    // according to the preference, send votes for each chain tip or
    // only for the specified block
    if (config.voteAllTips)
        for (const auto block : GetChainTips())
            voteForBlock(block);
    else
        voteForBlock(pindexNew);

    if (shouldRelock) pwallet->Lock();

    if (voteHashes.size() > 0) {
        std::string hashes;
        for (const auto& h : voteHashes) {
            if (hashes.length() > 0)
                hashes += ", ";
            hashes += h;
        }
        LogPrintf("CAutoVoter: Voted: %s\n", hashes.c_str());
    }
}

void CAutoVoter::start()
{
    config.autoVote = true;

    // late initialization, just to ensure that the global are initialized and configured
    if (!configured.load()) {
        config.ParseCommandline();
        ::RegisterValidationInterface(this);
        configured.store(true);
    }
}

void CAutoVoter::stop()
{
    config.autoVote = false;
}

bool CAutoVoter::isStarted() const
{
    return config.autoVote;
}
