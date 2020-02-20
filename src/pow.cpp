// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pow.h"

#include "arith_uint256.h"
#include "chain.h"
#include "primitives/block.h"
#include "uint256.h"

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Only change once per difficulty adjustment interval
    if ((pindexLast->nHeight+1) % params.DifficultyAdjustmentInterval() != 0)
    {
        if (params.fPowAllowMinDifficultyBlocks)
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Go back by what we want to be 14 days worth of blocks
    int nHeightFirst = pindexLast->nHeight - (params.DifficultyAdjustmentInterval()-1);
    assert(nHeightFirst >= 0);
    const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
    assert(pindexFirst);

    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespan/4)
        nActualTimespan = params.nPowTargetTimespan/4;
    if (nActualTimespan > params.nPowTargetTimespan*4)
        nActualTimespan = params.nPowTargetTimespan*4;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}

int64_t CalculateNextRequiredStakeDifficulty(const CBlockIndex* pindexLast, const Consensus::Params& params)
{
    // Stake difficulty before any tickets could possibly be purchased is
    // the minimum value.
    auto nextHeight = int64_t{0};
    if (pindexLast != nullptr) {
        nextHeight = pindexLast->nHeight + 1;
    }

    const auto& stakeDiffStartHeight = params.nCoinbaseMaturity + 1;
    if (nextHeight < stakeDiffStartHeight) {
        return params.nMinimumStakeDiff;
    }

    // Return the previous block's difficulty requirements if the next block
    // is not at a difficulty retarget interval.
    const auto& intervalSize = params.nStakeDiffWindowSize;
    const auto& curDiff = pindexLast->nStakeDifficulty;
    if (nextHeight % intervalSize != 0) {
        return curDiff;
    }

    // Get the pool size and number of tickets that were immature at the
    // previous retarget interval.
    //
    // NOTE: Since the stake difficulty must be calculated based on existing
    // blocks, it is always calculated for the block after a given block, so
    // the information for the previous retarget interval must be retrieved
    // relative to the block just before it to coincide with how it was
    // originally calculated.

    auto prevPoolSize = int64_t{0};
    const auto& prevRetargetHeight = nextHeight - intervalSize - 1;
    const auto& prevRetargetNode   = pindexLast->GetAncestor(prevRetargetHeight);
    if (prevRetargetNode != nullptr) {
        prevPoolSize = prevRetargetNode->nTicketPoolSize;
    }
    const auto& ticketMaturity = int64_t{params.nTicketMaturity};
    const auto& prevImmatureTickets = SumPurchasedTickets(prevRetargetNode, ticketMaturity);

    // Return the existing ticket price for the first few intervals to avoid
    // division by zero and encourage initial pool population.
    const auto& prevPoolSizeAll = prevPoolSize + prevImmatureTickets;
    if (prevPoolSizeAll == 0) {
        return curDiff;
    }

    // Count the number of currently immature tickets.
    const auto& immatureTickets = SumPurchasedTickets(pindexLast, ticketMaturity);

    // Calculate and return the final next required difficulty.
    const auto& curPoolSizeAll = int64_t{pindexLast->nTicketPoolSize} + immatureTickets;
    return CalcNextStakeDiffV2(params, nextHeight, curDiff, prevPoolSizeAll, curPoolSizeAll);
}

int64_t SumPurchasedTickets(const CBlockIndex *pindexStart, int64_t numToSum)
{
    auto numPurchased = int64_t{0};
    auto numTraversed = int64_t{0};
    for (auto node = pindexStart; node != nullptr && numTraversed < numToSum; ++numTraversed)
    {
        numPurchased += node->nFreshStake;
        node = node->pprev;
    }

    return numPurchased;
}

int64_t CalcNextStakeDiffV2( const Consensus::Params& params, int64_t nextHeight, int64_t curDiff, int64_t prevPoolSizeAll, int64_t curPoolSizeAll)
{
    // Shorter version of various parameter for convenience.
    const auto& votesPerBlock  = int64_t{params.nTicketsPerBlock};
    const auto& ticketPoolSize = int64_t{params.nTicketPoolSize};
    const auto& ticketMaturity = int64_t{params.nTicketMaturity};

    // Calculate the difficulty by multiplying the old stake difficulty
    // with two ratios that represent a force to counteract the relative
    // change in the pool size (Fc) and a restorative force to push the pool
    // size towards the target value (Fr).
    //
    // Per DCP0001, the generalized equation is:
    //
    //   nextDiff = min(max(curDiff * Fc * Fr, Slb), Sub)
    //
    // The detailed form expands to:
    //
    //                        curPoolSizeAll      curPoolSizeAll
    //   nextDiff = curDiff * ---------------  * -----------------
    //                        prevPoolSizeAll    targetPoolSizeAll
    //
    //   Slb = b.chainParams.MinimumStakeDiff
    //
    //               estimatedTotalSupply
    //   Sub = -------------------------------
    //          targetPoolSize / votesPerBlock
    //
    // In order to avoid the need to perform floating point math which could
    // be problematic across languages due to uncertainty in floating point
    // math libs, this is further simplified to integer math as follows:
    //
    //                   curDiff * curPoolSizeAll^2
    //   nextDiff = -----------------------------------
    //              prevPoolSizeAll * targetPoolSizeAll
    //
    // Further, the Sub parameter must calculate the denominator first using
    // integer math.
    const auto& targetPoolSizeAll = votesPerBlock * (ticketPoolSize + ticketMaturity);

    const auto& curPoolSizeAllBig = arith_uint256(curPoolSizeAll);
    auto nextDiffBig = arith_uint256(curDiff);
    nextDiffBig *= curPoolSizeAllBig;
    nextDiffBig *= curPoolSizeAllBig;
    nextDiffBig /= arith_uint256(prevPoolSizeAll);
    nextDiffBig /= arith_uint256(targetPoolSizeAll);

    // Limit the new stake difficulty between the minimum allowed stake
    // difficulty and a maximum value that is relative to the total supply.
    //
    // NOTE: This is intentionally using integer math to prevent any
    // potential issues due to uncertainty in floating point math libs.  The
    // ticketPoolSize parameter already contains the result of
    // (targetPoolSize / votesPerBlock).
    auto nextDiff = int64_t(nextDiffBig.GetLow64());
    const auto& estimatedSupply = EstimateSupply(params, nextHeight);
    const auto& maximumStakeDiff = estimatedSupply / ticketPoolSize;
    if (nextDiff > maximumStakeDiff) {
        nextDiff = maximumStakeDiff;
    }
    if (nextDiff < params.nMinimumStakeDiff) {
        nextDiff = params.nMinimumStakeDiff;
    }
    return nextDiff;
}

int64_t EstimateSupply(const Consensus::Params& params, int64_t height) 
{
    if (height <= 0) {
        return 0;
    }

    // Estimate the supply by calculating the full block subsidy for each
    // reduction interval and multiplying it the number of blocks in the
    // interval then adding the subsidy produced by number of blocks in the
    // current interval.
    // const auto& supply = 0;//params.BlockOneSubsidy()
    // const reductions := height / params.SubsidyReductionInterval
    // subsidy := params.BaseSubsidy
    // for i := int64(0); i < reductions; i++ {
    //     supply += params.SubsidyReductionInterval * subsidy

    //     subsidy *= params.MulSubsidy
    //     subsidy /= params.DivSubsidy
    // }
    // supply += (1 + height%params.SubsidyReductionInterval) * subsidy

    // Blocks 0 and 1 have special subsidy amounts that have already been
    // added above, so remove what their subsidies would have normally been
    // which were also added above.
    // supply -= params.BaseSubsidy * 2
    auto supply = int64_t{0};
    const int halvings = height / params.nSubsidyHalvingInterval;
    auto subsidy = GetTotalBlockSubsidy(0, params);
    for (int i = 0; i < halvings; ++i ) {
        supply += params.nSubsidyHalvingInterval * subsidy;
        subsidy >>= 1;
    }
    supply += (1 + height % params.nSubsidyHalvingInterval) * subsidy;

    return supply;
}

CAmount GetTotalBlockSubsidy(int nHeight, const Consensus::Params& consensusParams)
{
    int halvings = nHeight / consensusParams.nSubsidyHalvingInterval;
    // Force block reward to zero when right shift is undefined.
    if (halvings >= 64)
        return 0;

    CAmount nSubsidy = consensusParams.nTotalBlockSubsidy * COIN;
    // Subsidy is cut in half every 210,000 blocks which will occur approximately every 4 years.
    nSubsidy >>= halvings;
    return nSubsidy;
}
