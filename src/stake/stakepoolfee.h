// Copyright (c) 2020 The Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAICOIN_STAKE_STAKEPOOLFEE_H
#define PAICOIN_STAKE_STAKEPOOLFEE_H

#include "amount.h"

/* ValidPoolFeeRate tests to see if a pool fee is a valid percentage from 0.01% to 100.00%. */
bool IsValidPoolFeePercent(double feePercent);

/* StakePoolTicketFee determines the stake pool ticket fee for a given ticket
   from the passed percentage. Pool fee as a percentage is truncated from 0.01%
   to 100.00%. This all must be done with integers.
   - stakeDiff: The current ticket price (stake difficulty)
   - relayFee: the relay fee
   - height: block height for the current calculation
   - poolFeePercent: the percentage of fees to send to the pool (between 0 and 100) */
CAmount StakePoolTicketFee(CAmount stakeDiff, CAmount relayFee, int64_t height, double poolFeePercent);

#endif //PAICOIN_STAKE_STAKEPOOLFEE_H
