// Copyright (c) 2020 The Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ticketbuyer.h"
#include "wallet/wallet.h"
#include "validation.h"
#include <numeric>

CTicketBuyer::CTicketBuyer(CWallet* wallet) :
    configured(false),
    shouldRun(true),
    thread(&CTicketBuyer::mainLoop, this)
{
    pwallet = wallet;
}

CTicketBuyer::~CTicketBuyer()
{
    stop();
    shouldRun.store(false);

    // make sure that the thead doesn't remain
    // stucked waiting the condition variable
    mtx.lock();
    cv.notify_one();
    mtx.unlock();

    if (thread.joinable())
        thread.join();
}

void CTicketBuyer::UpdatedBlockTip(const CBlockIndex *, const CBlockIndex *, bool fInitialDownload)
{
    // we have to wait until the entire blockchain is downloaded
    if (fInitialDownload)
        return;

    std::unique_lock<std::mutex> lck{mtx};
    cv.notify_one();
}

void CTicketBuyer::start()
{
    config.buyTickets = true;
}

void CTicketBuyer::stop()
{
    config.buyTickets = false;
}

bool CTicketBuyer::isStarted() const
{
    return config.buyTickets;
}

void CTicketBuyer::mainLoop()
{
    int64_t height{0};
    int64_t nextIntervalStart{0}, currentInterval{0}, intervalSize{-1};
    int64_t expiry{0};
    CAmount spendable{0};
    CAmount sdiff{0};
    int64_t buy{0};
    int max{0};
    bool shouldRelock{false};

    while (shouldRun.load()) {
        // wait until activated
        if (! config.buyTickets) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // late initialization, just to ensure that the global are initialized and configured
        if (!configured.load()) {
            intervalSize = Params().GetConsensus().nStakeDiffWindowSize;
            config.ParseCommandline();
            ::RegisterValidationInterface(this);
            configured.store(true);
        }

        // wait until new block is received
        std::unique_lock<std::mutex> lck{mtx};
        cv.wait(lck);
        lck.unlock();

        // check if the thread was instructed to stop while
        // waiting for the condition variable and stop
        if (! shouldRun.load())
            break;

        if (pwallet == nullptr)
            continue;

        LOCK2(cs_main, pwallet->cs_wallet);

        // unlock wallet
        shouldRelock = pwallet->IsLocked();
        if (shouldRelock && ! pwallet->Unlock(config.passphrase)) {
            LogPrintf("CTicketBuyer: Purchased tickets: Wrong passphrase");
            continue;
        }

        if (chainActive.Tip() == nullptr) {
            if (shouldRelock) pwallet->Lock();
            continue;
        }

        // calculate the height of the first block in the
        // next stake difficulty interval:
        height = chainActive.Height();
        if (height + 2 >= nextIntervalStart) {
            currentInterval = height / intervalSize + 1;
            nextIntervalStart = currentInterval * intervalSize;

            // Skip this purchase when no more tickets may be purchased in the interval and
            // the next sdiff is unknown. The earliest any ticket may be mined is two
            // blocks from now, with the next block containing the split transaction
            // that the ticket purchase spends.
            if (height + 2 == nextIntervalStart) {
                LogPrintf("CTicketBuyer: Skipping purchase: next sdiff interval starts soon");
                if (shouldRelock) pwallet->Lock();
                continue;
            }

            // Set expiry to prevent tickets from being mined in the next
            // sdiff interval.  When the next block begins the new interval,
            // the ticket is being purchased for the next interval; therefore
            // increment expiry by a full sdiff window size to prevent it
            // being mined in the interval after the next.
            expiry = nextIntervalStart;
            if (height + 1 == nextIntervalStart) {
                expiry += intervalSize;
            }
        }

        // TODO check rescan point
        // Don't buy tickets for this attached block when transactions are not
        // synced through the tip block.

        // Cannot purchase tickets if not broadcasting
        if (! pwallet->GetBroadcastTransactions()) {
            if (shouldRelock) pwallet->Lock();
            continue;
        }

        // Determine how many tickets to buy
        spendable = pwallet->GetAvailableBalance();
        if (spendable < config.maintain) {
            LogPrintf("CTicketBuyer: Skipping purchase: low available balance");
            if (shouldRelock) pwallet->Lock();
            continue;
        }
        spendable -= config.maintain;

        sdiff = CalculateNextRequiredStakeDifficulty(chainActive.Tip(), Params().GetConsensus());

        buy = spendable / sdiff;
        if (buy <= 0) {
            LogPrintf("CTicketBuyer: Skipping purchase: low available balance");
            if (shouldRelock) pwallet->Lock();
            continue;
        }

        max = Params().GetConsensus().nMaxFreshStakePerBlock;
        if (buy > max)
            buy = max;

        if (config.limit > 0 && buy > config.limit)
            buy = config.limit;

        const auto&& r = pwallet->PurchaseTicket(config.account, spendable, config.minConf, config.votingAddress, static_cast<unsigned int>(buy), config.poolFeeAddress, config.poolFees, expiry, 0 /*TODO Make sure this is handled correctly*/);

        if (r.second.code != CWalletError::SUCCESSFUL)
            LogPrintf("CTicketBuyer: Failed to purchase tickets: (%d) %s", r.second.code, r.second.message.c_str());

        if (r.first.size() > 0) {
            std::string hashes;
            for (const auto& h : r.first) {
                if (hashes.length() > 0)
                    hashes += ", ";
                hashes += h;
            }
            LogPrintf("CTicketBuyer: Purchased tickets: %s", hashes.c_str());
        } else
            LogPrintf("CTicketBuyer: Successful, but not ticket hashes are available");

        if (shouldRelock) pwallet->Lock();
    }
}
