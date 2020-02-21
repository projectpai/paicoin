// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAICOIN_POW_H
#define PAICOIN_POW_H

#include "consensus/params.h"
#include "amount.h"

#include <stdint.h>

class CBlockHeader;
class CBlockIndex;
class uint256;

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params&);
unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params&);

/** Check whether a block hash satisfies the proof-of-work requirement specified by nBits */
bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params&);

/* Stake difficulty*/
// calcNextRequiredStakeDifficulty calculates the required stake difficulty for
// the block after the passed previous block node based on the active stake
// difficulty retarget rules.
//
// This function differs from the exported CalcNextRequiredDifficulty in that
// the exported version uses the current best chain as the previous block node
// while this function accepts any block node.
//
// Note: only the newer algorithm calcNextRequiredStakeDifficultyV2 is ported
int64_t CalculateNextRequiredStakeDifficulty(const CBlockIndex* pindexLast, const Consensus::Params&);

// sumPurchasedTickets returns the sum of the number of tickets purchased in the
// most recent specified number of blocks from the point of view of the passed
// node.
int64_t SumPurchasedTickets(const CBlockIndex *pindexStart, int64_t numToSum);

// calcNextStakeDiffV2 calculates the next stake difficulty for the given set
// of parameters using the algorithm defined in DCP0001.
//
// This function contains the heart of the algorithm and thus is separated for
// use in both the actual stake difficulty calculation as well as estimation.
//
// The caller must perform all of the necessary chain traversal in order to
// get the current difficulty, previous retarget interval's pool size plus
// its immature tickets, as well as the current pool size plus immature tickets.
int64_t CalcNextStakeDiffV2(const Consensus::Params&, int64_t nextHeight, int64_t curDiff, int64_t prevPoolSizeAll, int64_t curPoolSizeAll);

// estimateSupply returns an estimate of the coin supply for the provided block
// height.  This is primarily used in the stake difficulty algorithm and relies
// on an estimate to simplify the necessary calculations.  The actual total
// coin supply as of a given block height depends on many factors such as the
// number of votes included in every prior block (not including all votes
// reduces the subsidy) and whether or not any of the prior blocks have been
// invalidated by stakeholders thereby removing the PoW subsidy for them.
int64_t EstimateSupply(const Consensus::Params&, int64_t height);

CAmount GetTotalBlockSubsidy(int nHeight, const Consensus::Params& consensusParams);

// estimateNextStakeDifficulty estimates the next stake difficulty by pretending
// the provided number of tickets will be purchased in the remainder of the
// interval unless the flag to use max tickets is set in which case it will use
// the max possible number of tickets that can be purchased in the remainder of
// the interval.
//
// The stake difficulty algorithm is selected based on the active rules.
//
// This function differs from the exported EstimateNextStakeDifficulty in that
// the exported version uses the current best chain as the block node while this
// function accepts any block node.
//
// This function MUST be called with the chain state lock held (for writes).

// Note: only the newer algorithm estimateNextStakeDifficultyV2 is ported
int64_t EstimateNextStakeDifficulty(const CBlockIndex* pindexLast, int newTickets, bool useMaxTickets, const Consensus::Params& params);

#endif // PAICOIN_POW_H
