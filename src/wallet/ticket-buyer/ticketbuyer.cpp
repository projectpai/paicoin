//
// Copyright (c) 2017-2020 Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//


#include "ticketbuyer.h"
#include "wallet/wallet.h"
#include "validation.h"
#include <numeric>

CTicketBuyer::CTicketBuyer(CWallet* wallet) :
    configured(false),
    nextIntervalStart{0}, currentInterval{0}, intervalSize{-1}
{
    pwallet = wallet;
}

CTicketBuyer::~CTicketBuyer()
{
    stop();
}

void CTicketBuyer::UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *, bool fInitialDownload)
{
    // we have to wait until the entire blockchain is downloaded
    if (fInitialDownload)
        return;

    if (pwallet == nullptr)
        return;

    // if not configured yet, do nothing
    if (!configured.load())
        return;

    if (!config.buyTickets)
        return;

    LOCK(cs_main);

    const CBlockIndex* tip = chainActive.Tip();

    // check if the new tip is indeed on chain active
    if (pindexNew == nullptr || tip == nullptr || pindexNew->GetBlockHash() != tip->GetBlockHash())
        return;

    int64_t height = chainActive.Height();

    // do not try to purchase tickets before the stake enabling height - ticket maturity,
    // so they are mature at stake enabling height
    if (height < Params().GetConsensus().nStakeEnabledHeight - Params().GetConsensus().nTicketMaturity)
        return;

    // unlock wallet
    bool shouldRelock = pwallet->IsLocked();
    if (shouldRelock && ! pwallet->Unlock(config.passphrase)) {
        LogPrintf("CTicketBuyer: Purchased tickets: Wrong passphrase\n");
        return;
    }

    int64_t expiry{0};

    // calculate the height of the first block in the
    // next stake difficulty interval:
    if (height + 2 >= nextIntervalStart) {
        currentInterval = height / intervalSize + 1;
        nextIntervalStart = currentInterval * intervalSize;

        // Skip this purchase when no more tickets may be purchased in the interval and
        // the next sdiff is unknown. The earliest any ticket may be mined is two
        // blocks from now, with the next block containing the split transaction
        // that the ticket purchase spends.
        if (height + 2 == nextIntervalStart) {
            LogPrintf("CTicketBuyer: Skipping purchase: next sdiff interval starts soon\n");
            if (shouldRelock) pwallet->Lock();
            return;
        }

        // Set expiry to prevent tickets from being mined in the next
        // sdiff interval. When the next block begins the new interval,
        // the ticket is being purchased for the next interval; therefore
        // increment expiry by a full sdiff window size to prevent it
        // being mined in the interval after the next.
        expiry = nextIntervalStart;
        if (height + 1 == nextIntervalStart) {
            expiry += intervalSize;
        }

        // Make sure to use the specified value if that is within the limits
        // of the stake difficulty window.
        if (height + config.txExpiry < expiry)
            expiry = height + config.txExpiry;
    }

    // TODO check rescan point
    // Don't buy tickets for this attached block when transactions are not
    // synced through the tip block.

    // Cannot purchase tickets if not broadcasting
    if (! pwallet->GetBroadcastTransactions()) {
        if (shouldRelock) pwallet->Lock();
        return;
    }

    // Determine how many tickets to buy
    CAmount spendable = pwallet->GetAvailableBalance();
    if (spendable < config.maintain) {
        LogPrintf("CTicketBuyer: Skipping purchase: low available balance (less than amount to maintain)\n");
        if (shouldRelock) pwallet->Lock();
        return;
    }
    spendable -= config.maintain;

    CAmount sdiff = CalculateNextRequiredStakeDifficulty(chainActive.Tip(), Params().GetConsensus());

    int64_t buy = spendable / sdiff;
    if (buy <= 0) {
        LogPrintf("CTicketBuyer: Skipping purchase: low available balance\n");
        if (shouldRelock) pwallet->Lock();
        return;
    }

    int max = Params().GetConsensus().nMaxFreshStakePerBlock;
    if (buy > max)
        buy = max;

    if (config.limit > 0 && buy > config.limit)
        buy = config.limit;

    const auto&& r = pwallet->PurchaseTicket(config.account, spendable, config.minConf, config.votingAddress, config.rewardAddress, static_cast<unsigned int>(buy), config.poolFeeAddress, config.poolFees, expiry, 0 /*TODO Make sure this is handled correctly*/);

    if (r.second.code != CWalletError::SUCCESSFUL)
        LogPrintf("CTicketBuyer: Failed to purchase tickets: (%d) %s\n", r.second.code, r.second.message.c_str());

    if (r.first.size() > 0) {
        std::string hashes;
        for (const auto& h : r.first) {
            if (hashes.length() > 0)
                hashes += ", ";
            hashes += h;
        }
        LogPrintf("CTicketBuyer: Purchased tickets: %s\n", hashes.c_str());
    } else
        LogPrintf("CTicketBuyer: Successful, but not ticket hashes are available\n");

    if (shouldRelock) pwallet->Lock();
}

void CTicketBuyer::start()
{
    config.buyTickets = true;

    // late initialization, just to ensure that the global are initialized and configured
    if (!configured.load()) {
        intervalSize = Params().GetConsensus().nStakeDiffWindowSize;
        config.ParseCommandline();
        ::RegisterValidationInterface(this);
        configured.store(true);
    }
}

void CTicketBuyer::stop()
{
    config.buyTickets = false;
}

bool CTicketBuyer::isStarted() const
{
    return config.buyTickets;
}
