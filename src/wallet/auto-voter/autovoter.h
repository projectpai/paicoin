/* * Copyright (c) 2017-2020 Project PAI Foundation
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */


#ifndef PAICOIN_WALLET_AUTOVOTER_AUTOVOTER_H
#define PAICOIN_WALLET_AUTOVOTER_AUTOVOTER_H

#include "autovoterconfig.h"
#include "validationinterface.h"

#include <atomic>

class CWallet;

// The Automatic Voter (AV)
// This is responsible with monitoring the blockchain advance
// and automatically generate and publish a vote transaction
// for a ticket that has been selected as winner.

class CAutoVoter : public CValidationInterface {
public:
    CAutoVoter() = delete;
    CAutoVoter(CWallet* wallet);
    virtual ~CAutoVoter();

    virtual void UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload) override;
    virtual void BlockConnected(const std::shared_ptr<const CBlock> &block, const CBlockIndex *pindex, const std::vector<CTransactionRef> &txnConflicted) override;
    virtual void NewPoWValidBlock(const CBlockIndex *pindex, const std::shared_ptr<const CBlock>& block) override;

    CAutoVoterConfig& GetConfig() { return config; }

    void start();
    void stop();
    bool isStarted() const;

private:
    CAutoVoterConfig config;

    CWallet* pwallet;

    std::atomic<bool> configured;
    void DoVote(const CBlockIndex *pindexNew);
};

#endif // PAICOIN_WALLET_AUTOVOTER_AUTOVOTER_H
