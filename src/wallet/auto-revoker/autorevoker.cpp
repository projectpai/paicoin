// Copyright (c) 2020 The Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "autorevoker.h"
#include "wallet/wallet.h"
#include "validation.h"
#include <numeric>

CAutoRevoker::CAutoRevoker(CWallet* wallet) :
    configured(false)
{
    pwallet = wallet;
}

CAutoRevoker::~CAutoRevoker()
{
    stop();
}

void CAutoRevoker::UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *, bool fInitialDownload)
{
    // we have to wait until the entire blockchain is downloaded
    if (fInitialDownload)
        return;

    if (pwallet == nullptr)
        return;

    // if not configured yet, do nothing
    if (!configured.load())
        return;

    if (!config.autoRevoke)
        return;

    LOCK2(cs_main, pwallet->cs_wallet);

    // unlock wallet
    bool shouldRelock = pwallet->IsLocked();
    if (shouldRelock && ! pwallet->Unlock(config.passphrase)) {
        LogPrintf("CAutoRevoker: Unlocking wallet: Wrong passphrase");
        return;
    }

    const CBlockIndex* tip = chainActive.Tip();

    // check if the new tip is indeed on chain active
    if (pindexNew == nullptr || tip == nullptr || pindexNew->GetBlockHash() != tip->GetBlockHash()) {
        if (shouldRelock) pwallet->Lock();
        return;
    }

    // search for all missed tickets that belong to the wallet and
    // send a revocation transaction for each of them; missed tickets
    // also include the expired ones

    std::vector<std::string> revocationHashes;
    CWalletError we;

    std::tie(revocationHashes, we) = pwallet->RevokeAll();

    if (shouldRelock) pwallet->Lock();

    if (we.code != CWalletError::SUCCESSFUL)
        LogPrintf("CAutoRevoker: Failed to revoke: (%d) %s - (%s)", we.code, we.message.c_str());

    if (revocationHashes.size() > 0) {
        std::string hashes;
        for (const auto& h : revocationHashes) {
            if (hashes.length() > 0)
                hashes += ", ";
            hashes += h;
        }
        LogPrintf("CAutoRevoker: Revoked: %s", hashes.c_str());
    }
}

void CAutoRevoker::start()
{
    config.autoRevoke = true;

    // late initialization, just to ensure that the global are initialized and configured
    if (!configured.load()) {
        config.ParseCommandline();
        ::RegisterValidationInterface(this);
        configured.store(true);
    }
}

void CAutoRevoker::stop()
{
    config.autoRevoke = false;
}

bool CAutoRevoker::isStarted() const
{
    return config.autoRevoke;
}
