// Copyright (c) 2020 The Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAICOIN_WALLET_TICKETBUYER_TICKETBUYERCFG_H
#define PAICOIN_WALLET_TICKETBUYER_TICKETBUYERCFG_H

#include "amount.h"
#include "support/allocators/secure.h"

// The ticket buyer (TB) configuration
class CTicketBuyerConfig {

public:
    // Enables the automatic ticket purchasing
    bool buyTickets;

    // Account to buy tickets from
    std::string account;

    // Minimum amount to maintain in purchasing account
    CAmount maintain;

    // Account to derive voting addresses from; overridden by VotingAddr
    std::string votingAccount;

    // Address to assign voting rights; overrides VotingAccount
    std::string votingAddress;

    // Commitment address for stakepool fees
    std::string poolFeeAddress;

    // Stakepool fee percentage (between 0-100)
    double poolFees;

    // Limit maximum number of purchased tickets per block
    int limit;

    // Minimum number of block confirmations required
    const int minConf = 1;

    // Wallet passphrase
    SecureString passphrase;

    CTicketBuyerConfig();

    void ParseCommandline();
};

#endif // PAICOIN_WALLET_TICKETBUYER_TICKETBUYERCFG_H
