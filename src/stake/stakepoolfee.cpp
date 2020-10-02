//
// Copyright (c) 2017-2020 Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//


#include "stakepoolfee.h"
#include "arith_uint256.h"
#include "chainparams.h"
#include "validation.h"

#include <cmath>
#include <algorithm>

bool IsValidPoolFeePercent(double feePercent)
{
    double multiplied = feePercent * 100.0;

    if (multiplied > 10000.0)
        return false;
    if (multiplied < 1.0)
        return false;

    double normalized = floor(multiplied);

    return normalized >= 1.0 && normalized <= 10000.0;
}

CAmount StakePoolTicketFee(CAmount stakeDiff, CAmount relayFee, int64_t height, double poolFeePercent)
{
    if (!IsValidPoolFeePercent(poolFeePercent))
        return 0;

    // Shift the decimal two places, e.g. 1.00%
    // to 100. This assumes that the proportion
    // is already multiplied by 100 to give a
    // percentage, thus making the entirety
    // be a multiplication by 10000.
    uint64_t poolfeePercentInt = static_cast<uint64_t>(floor(poolFeePercent * 100.0));
    poolfeePercentInt = std::min(std::max(poolfeePercentInt, static_cast<uint64_t>(1)), static_cast<uint64_t>(10000));

    // Calculate voter subsidy
    CAmount subsidy = GetVoterSubsidy(static_cast<int>(height), Params().GetConsensus());

    // The numerator is (p*10000*s*(v+z)) << 64.
    unsigned int shift{64};
    auto s = arith_uint256(static_cast<uint64_t>(subsidy));
    auto v = arith_uint256(static_cast<uint64_t>(stakeDiff));
    auto z = arith_uint256(static_cast<uint64_t>(relayFee));
    auto num = arith_uint256(poolfeePercentInt);
    num *= s;
    auto vPlusZ = v + z;
    num *= vPlusZ;
    num <<= shift;

    // The denominator is 10000*(s+v).
    // The extra 10000 above cancels out.
    arith_uint256 den{s};
    den += v;
    den *= arith_uint256(10000);

    // Divide and shift back.
    num /= den;
    num >>= shift;

    return static_cast<CAmount>(num.GetLow64());
}
