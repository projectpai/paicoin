/* * Copyright (c) 2017-2020 Project PAI Foundation
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#ifndef STAKEVERSION_H
#define STAKEVERSION_H

#include "consensus/params.h"
#include "chain.h"

#include <stdint.h>

uint32_t calcStakeVersion(const CBlockIndex *pprevIndex, const Consensus::Params& params);
int64_t calcWantHeight(int64_t stakeValidationHeight, int64_t interval, int64_t height);

#endif // STAKEVERSION_H
