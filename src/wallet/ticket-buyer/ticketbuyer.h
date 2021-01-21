/* * Copyright (c) 2017-2020 Project PAI Foundation
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */


#ifndef PAICOIN_WALLET_TICKETBUYER_TICKETBUYER_H
#define PAICOIN_WALLET_TICKETBUYER_TICKETBUYER_H

#include "ticketbuyerconfig.h"
#include "validationinterface.h"

#include <atomic>

class CWallet;

// The Automatic Ticket Buyer (TB)
// This is responsible with monitoring the blockchain advance
// and automatically generate and publish any ticket purchase transaction
// on behalf of the user.

class CTicketBuyer : public CValidationInterface {
public:
    CTicketBuyer() = delete;
    CTicketBuyer(CWallet* wallet);
    virtual ~CTicketBuyer();

    virtual void UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload) override;

    CTicketBuyerConfig& GetConfig() { return config; }

    void start();
    void stop();    // does not stop immediately, but only after the current iteration
    bool isStarted() const;

private:
    CTicketBuyerConfig config;

    CWallet* pwallet;

    std::atomic<bool> configured;

    int64_t nextIntervalStart, currentInterval, intervalSize;
};

#endif // PAICOIN_WALLET_TICKETBUYER_TICKETBUYER_H
