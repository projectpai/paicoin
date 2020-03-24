#ifndef STAKEVERSION_H
#define STAKEVERSION_H

#include "consensus/params.h"
#include "chain.h"

#include <stdint.h>

uint32_t calcStakeVersion(const CBlockIndex *pprevIndex, const Consensus::Params& params);
int64_t calcWantHeight(int64_t stakeValidationHeight, int64_t interval, int64_t height);

#endif // STAKEVERSION_H
