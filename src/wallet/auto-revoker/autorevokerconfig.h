// Copyright (c) 2020 The Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAICOIN_WALLET_AUTOREVOKER_AUTOREVOKERCFG_H
#define PAICOIN_WALLET_AUTOREVOKER_AUTOREVOKERCFG_H

#include "support/allocators/secure.h"

// The Automatic Revoker (AR) configuration
class CAutoRevokerConfig {

public:
    // Enables the automatic revoking
    bool autoRevoke;

    // Wallet passphrase
    SecureString passphrase;

    CAutoRevokerConfig();

    void ParseCommandline();
};

#endif // PAICOIN_WALLET_AUTOREVOKER_AUTOREVOKERCFG_H
