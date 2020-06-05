// Copyright (c) 2020 The Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAICOIN_WALLET_AUTOVOTER_AUTOVOTER_H
#define PAICOIN_WALLET_AUTOVOTER_AUTOVOTER_H

#include "autovoterconfig.h"
#include "validationinterface.h"

#include <atomic>
//#include <mutex>

class CWallet;

// The Automatic Voter (AV)
class CAutoVoter : public CValidationInterface {
public:
    CAutoVoter() = delete;
    CAutoVoter(CWallet* wallet);
    virtual ~CAutoVoter();

    virtual void UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload) override;
    //virtual void BlockConnected(const std::shared_ptr<const CBlock> &block, const CBlockIndex *pindex, const std::vector<CTransactionRef> &txnConflicted) override;

    CAutoVoterConfig& GetConfig() { return config; }

    void start();
    void stop();
    bool isStarted() const;

private:
    CAutoVoterConfig config;

    CWallet* pwallet;

    std::atomic<bool> configured;

    //std::mutex mtx;
};

#endif // PAICOIN_WALLET_AUTOVOTER_AUTOVOTER_H
