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

    auto supply = int64_t{0};
    const int halvings = height / params.nSubsidyHalvingInterval;
    auto subsidy = GetTotalBlockSubsidy(0, params);
    for (int i = 0; i < halvings; ++i ) {
        supply += params.nSubsidyHalvingInterval * subsidy;
        if (i >= 64) {
            // Force block reward to zero when right shift is undefined.
            subsidy = 0;
        }
        else {
            subsidy >>= 1;
        }
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

// estimateNextStakeDifficultyV2 estimates the next stake difficulty using the
// algorithm defined in DCP0001 by pretending the provided number of tickets
// will be purchased in the remainder of the interval unless the flag to use max
// tickets is set in which case it will use the max possible number of tickets
// that can be purchased in the remainder of the interval.
//
// This function MUST be called with the chain state lock held (for writes).
int64_t EstimateNextStakeDifficulty(const CBlockIndex* pindexLast, int newTickets, bool useMaxTickets, const Consensus::Params& params)
{
    // Calculate the next retarget interval height.
    auto curHeight = int64_t{0};
    if (pindexLast != nullptr) {
        curHeight = pindexLast->nHeight;
    }

    const auto& ticketMaturity = int64_t{params.nTicketMaturity};
    const auto& intervalSize   = params.nStakeDiffWindowSize;
    const auto& blocksUntilRetarget = int64_t{intervalSize - curHeight % intervalSize};
    const auto& nextRetargetHeight  = curHeight + blocksUntilRetarget;

    // Calculate the maximum possible number of tickets that could be sold
    // in the remainder of the interval and potentially override the number
    // of new tickets to include in the estimate per the user-specified
    // flag.
    const auto& maxTicketsPerBlock = int64_t{params.nMaxFreshStakePerBlock};
    const auto& maxRemainingTickets = (blocksUntilRetarget - 1) * maxTicketsPerBlock;
    if (useMaxTickets) {
        newTickets = maxRemainingTickets;
    }

    // Ensure the specified number of tickets is not too high.
    if (newTickets > maxRemainingTickets) {
        throw std::logic_error("Unable to create an estimate stake difficulty with more tickets than the maximum remaining tickets");
    }

    // Stake difficulty before any tickets could possibly be purchased is
    // the minimum value.
    const auto& stakeDiffStartHeight = int64_t(params.nCoinbaseMaturity) + 1;
    if (nextRetargetHeight < stakeDiffStartHeight) {
        return params.nMinimumStakeDiff;
    }

    // Get the pool size and number of tickets that were immature at the
    // previous retarget interval
    //
    // NOTE: Since the stake difficulty must be calculated based on existing
    // blocks, it is always calculated for the block after a given block, so
    // the information for the previous retarget interval must be retrieved
    // relative to the block just before it to coincide with how it was
    // originally calculated.
    auto prevPoolSize  = int64_t{0};
    auto prevRetargetHeight = nextRetargetHeight - intervalSize - 1;
    auto prevRetargetNode = pindexLast->GetAncestor(prevRetargetHeight);
    if (prevRetargetNode != nullptr) {
        prevPoolSize = prevRetargetNode->nTicketPoolSize;
    }
    const auto& prevImmatureTickets = SumPurchasedTickets(prevRetargetNode, ticketMaturity);

    // Return the existing ticket price for the first few intervals to avoid
    // division by zero and encourage initial pool population.
    auto curDiff = pindexLast->nStakeDifficulty;
    const auto& prevPoolSizeAll = prevPoolSize + prevImmatureTickets;
    if (prevPoolSizeAll == 0) {
        return curDiff;
    }

    // Calculate the number of tickets that will still be immature at the
    // next retarget based on the known (non-estimated) data.
    //
    // Note that when the interval size is larger than the ticket maturity,
    // the current height might be before the maturity floor (the point
    // after which the remaining tickets will remain immature).  There are
    // therefore no possible remaining immature tickets from the blocks that
    // are not being estimated in that case.
    auto remainingImmatureTickets = int64_t{0};
    const auto& nextMaturityFloor = nextRetargetHeight - ticketMaturity - 1;
    if (curHeight > nextMaturityFloor) {
        remainingImmatureTickets = SumPurchasedTickets(pindexLast, curHeight-nextMaturityFloor);
    }

    // Add the number of tickets that will still be immature at the next
    // retarget based on the estimated data.
    const auto& maxImmatureTickets = ticketMaturity * maxTicketsPerBlock;
    if (newTickets > maxImmatureTickets) {
        remainingImmatureTickets += maxImmatureTickets;
    } else {
        remainingImmatureTickets += newTickets;
    }

    // Calculate the number of tickets that will mature in the remainder of
    // the interval based on the known (non-estimated) data.
    //
    // NOTE: The pool size in the block headers does not include the tickets
    // maturing at the height in which they mature since they are not
    // eligible for selection until the next block, so exclude them by
    // starting one block before the next maturity floor.
    auto finalMaturingHeight = nextMaturityFloor - 1;
    if (finalMaturingHeight > curHeight) {
        finalMaturingHeight = curHeight;
    }
    const auto& finalMaturingNode = pindexLast->GetAncestor(finalMaturingHeight);
    const auto& firstMaturingHeight = curHeight - ticketMaturity;
    auto maturingTickets = SumPurchasedTickets(finalMaturingNode, finalMaturingHeight-firstMaturingHeight+1);

    // Add the number of tickets that will mature based on the estimated data.
    //
    // Note that when the ticket maturity is greater than or equal to the
    // interval size, the current height will always be after the maturity
    // floor.  There are therefore no possible maturing estimated tickets
    // in that case.
    if (curHeight < nextMaturityFloor) {
        const auto& maturingEstimateNodes = nextMaturityFloor - curHeight - 1;
        auto maturingEstimatedTickets = maxTicketsPerBlock * maturingEstimateNodes;
        if (maturingEstimatedTickets > newTickets) {
            maturingEstimatedTickets = newTickets;
        }
        maturingTickets += maturingEstimatedTickets;
    }

    // Calculate the number of votes that will occur during the remainder of
    // the interval.
    const auto& stakeValidationHeight = params.nStakeValidationHeight;
    auto pendingVotes = int64_t{0};
    if (nextRetargetHeight > stakeValidationHeight) {
        auto votingBlocks = blocksUntilRetarget - 1;
        if (curHeight < stakeValidationHeight) {
            votingBlocks = nextRetargetHeight - stakeValidationHeight;
        }
        const auto& votesPerBlock = int64_t(params.nTicketsPerBlock);
        pendingVotes = votingBlocks * votesPerBlock;
    }

    // Calculate what the pool size would be as of the next interval.
    const auto& curPoolSize = int64_t(pindexLast->nTicketPoolSize);
    const auto& estimatedPoolSize = curPoolSize + maturingTickets - pendingVotes;
    const auto& estimatedPoolSizeAll = estimatedPoolSize + remainingImmatureTickets;

    // Calculate and return the final estimated difficulty.
    return CalcNextStakeDiffV2(params, nextRetargetHeight, curDiff, prevPoolSizeAll, estimatedPoolSizeAll);
}