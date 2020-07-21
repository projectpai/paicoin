// Copyright (c) 2020 The Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAICOIN_WALLET_AUTOVOTER_AUTOVOTERCFG_H
#define PAICOIN_WALLET_AUTOVOTER_AUTOVOTERCFG_H

#include "support/allocators/secure.h"
#include "stake/votebits.h"
#include "stake/extendedvotebits.h"

// The Automatic Voter (AV) configuration
class CAutoVoterConfig {

public:
    // Enables the automatic voting
    bool autoVote;

    // The bits that must be included in the vote
    // Default, RTT accepted
    VoteBits voteBits;

    // The extended vote bits
    ExtendedVoteBits extendedVoteBits;

    // Wallet passphrase
    SecureString passphrase;

    CAutoVoterConfig();

    void ParseCommandline();
};

#endif // PAICOIN_WALLET_AUTOVOTER_AUTOVOTERCFG_H
