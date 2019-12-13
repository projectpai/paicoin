#ifndef STAKEVERSION_H
#define STAKEVERSION_H

#include "consensus/params.h"
#include "chain.h"

#include <stdint.h>

uint32_t calcStakeVersion(const CBlockIndex *pprevIndex, const Consensus::Params& params);

#endif // STAKEVERSION_H
