//
// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017-2020 Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//


#include "pow.h"

#include "arith_uint256.h"
#include "chain.h"
#include "util.h"
#include "primitives/block.h"
#include "uint256.h"
#include "verification_client.h"
#include "consensus/consensus.h"
#include "validation.h"

/**
 * Compute the next required proof of work using the legacy Bitcoin difficulty
 * adjustment
 */
static unsigned int GetNextLegacyWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // assert(params.nHybridConsensusHeight < 0 || params.nHybridConsensusHeight % params.DifficultyAdjustmentInterval() == 0);

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

    // if (pindexLast->nHeight + 1 == params.nHybridConsensusHeight)
    //     return 0x207ffffe;

    // Go back by what we want to be 14 days worth of blocks
    int nHeightFirst = pindexLast->nHeight - (params.DifficultyAdjustmentInterval()-1);
    assert(nHeightFirst >= 0);
    const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
    assert(pindexFirst);

    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
}

/**
 * Compute a target based on the work done between 2 blocks and the time
 * required to produce that work.
 */
static arith_uint256 ComputeTarget(const CBlockIndex *pindexFirst,
                                   const CBlockIndex *pindexLast,
                                   const Consensus::Params &params) {
    assert(pindexLast->nHeight > pindexFirst->nHeight);

    /**
     * From the total work done and the time it took to produce that much work,
     * we can deduce how much work we expect to be produced in the targeted time
     * between blocks.
     */
    arith_uint256 work = pindexLast->nChainWork - pindexFirst->nChainWork;
    work *= params.nPowTargetSpacing;

    // In order to avoid difficulty cliffs, we bound the amplitude of the
    // adjustment we are going to do to a factor in [0.5, 2].
    int64_t nActualTimespan =
        int64_t(pindexLast->nTime) - int64_t(pindexFirst->nTime);
    if (nActualTimespan > 288 * params.nPowTargetSpacing) {
        nActualTimespan = 288 * params.nPowTargetSpacing;
    } else if (nActualTimespan < 72 * params.nPowTargetSpacing) {
        nActualTimespan = 72 * params.nPowTargetSpacing;
    }

    work /= nActualTimespan;

    /**
     * We need to compute T = (2^256 / W) - 1 but 2^256 doesn't fit in 256 bits.
     * By expressing 1 as W / W, we get (2^256 - W) / W, and we can compute
     * 2^256 - W as the complement of W.
     */
    return (-work) / work;
}

/**
 * To reduce the impact of timestamp manipulation, we select the block we are
 * basing our computation on via a median of 3.
 */
static const CBlockIndex *GetSuitableBlock(const CBlockIndex *pindex) {
    assert(pindex->nHeight >= 3);

    /**
     * In order to avoid a block with a very skewed timestamp having too much
     * influence, we select the median of the 3 top most blocks as a starting
     * point.
     */
    const CBlockIndex *blocks[3];
    blocks[2] = pindex;
    blocks[1] = pindex->pprev;
    blocks[0] = blocks[1]->pprev;

    // Sorting network.
    if (blocks[0]->nTime > blocks[2]->nTime) {
        std::swap(blocks[0], blocks[2]);
    }

    if (blocks[0]->nTime > blocks[1]->nTime) {
        std::swap(blocks[0], blocks[1]);
    }

    if (blocks[1]->nTime > blocks[2]->nTime) {
        std::swap(blocks[1], blocks[2]);
    }

    // We should have our candidate in the middle now.
    return blocks[1];
}

/**
 * Find the first block using Paicoin Hash by reverse searching the chain.
 * This function should only be called once Paicoin Hash switching has been reached,
 * since it assumes that at least the successor of pindexPrev block uses Paicoin Hash.
 */
static inline int GetFirstPaicoinHashHeight(const CBlockIndex *pindexPrev) {
    assert(pindexPrev);

    int height = pindexPrev->nHeight + 1;
    while (height > 0 && pindexPrev != nullptr && pindexPrev->GetBlockHeader().isPaicoinHashBlock()) {
        --height;
        pindexPrev = pindexPrev->pprev;
    }

    return height;
}

unsigned int GetNextWorkRequired(const CBlockIndex *pindexPrev, const CBlockHeader *pblock, const Consensus::Params &params) {
    // GetNextWorkRequired should never be called on the genesis block
    assert(pindexPrev != nullptr);

    // Special rule for regtest: we never retarget.
    if (params.fPowNoRetargeting) {
        return pindexPrev->nBits;
    }

    if (IsHybridConsensusForkEnabled(pindexPrev, params)) {
        return GetNextCashWorkRequired(pindexPrev, pblock, params);
    }

    return GetNextLegacyWorkRequired(pindexPrev, pblock, params);
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

bool CheckProofOfWork(const CBlockHeader& block, const Consensus::Params& params, bool checkMLproof)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(block.nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > ((nVersion & HARDFORK_VERSION_BIT) ? UintToArith256(params.hybridConsensusPowLimit) : UintToArith256(params.powLimit)))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(block.GetHash()) > bnTarget)
        return false;

    // check that the nonce is derived from ML data
    if (block.nNonce != block.DeriveNonceFromML())
        return false;

    // if we are asked only to check block hash against difficulty, we are done
    if (!checkMLproof)
        return true;

    // for genesis block we don't verify ML proof because it's just random data rather than a product of ML training
    if (block.GetHash() == params.hashGenesisBlock)
        return true;

    // in regtest mode, we don't verify ML proof because it's just random data rather than a product of ML training
    if (gArgs.GetBoolArg("-regtest", false))
        return true;

    // if ML proof verification is switched off with a flag, we skip it
    if (gArgs.GetBoolArg("-skipmlcheck", false))
        return true;

    // check ML proof
    VerificationClient client(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));
    auto result = client.Verify(std::string(block.powMsgID), block.powModelHash.ToString(), std::string(block.powNextMsgID));
    int resultCode = int(result.first);
    return resultCode == pai::pouw::verification::Response::OK;
}

/**
 * Compute the next required proof of work using a weighted average of the
 * estimated hashrate per block.
 *
 * Using a weighted average ensure that the timestamp parameter cancels out in
 * most of the calculation - except for the timestamp of the first and last
 * block. Because timestamps are the least trustworthy information we have as
 * input, this ensures the algorithm is more resistant to malicious inputs.
 */
uint32_t GetNextCashWorkRequired(const CBlockIndex *pindexPrev,
                                 const CBlockHeader *pblock,
                                 const Consensus::Params &params) {
    // This cannot handle the genesis block and early blocks in general.
    assert(pindexPrev);

    // if the block is just after the Hybrid Consensus fork,
    // enforce the initial difficulty
    if (pindexPrev->nHeight < params.nHybridConsensusHeight + params.nHybridConsensusInitialDifficultyBlockCount) {
        return params.nHybridConsensusInitialDifficulty;
    }

    // if the block is just after the Paicoin Hash fork,
    // enforce the initial difficulty for this situation
    int nPaicoinHashHeight = GetFirstPaicoinHashHeight(pindexPrev);
    if (pblock->isPaicoinHashBlock() && pindexPrev->nHeight < nPaicoinHashHeight + params.nPaicoinHashInitialDifficultyBlockCount) {
        return params.nPaicoinHashInitialDifficulty;
    }

    // Special difficulty rule for testnet:
    // If the new block's timestamp is more than 2* 10 minutes then allow
    // mining of a min-difficulty block.
    if (params.fPowAllowMinDifficultyBlocks &&
        (pblock->GetBlockTime() >
         pindexPrev->GetBlockTime() + 2 * params.nPowTargetSpacing)) {
        return UintToArith256(params.hybridConsensusPowLimit).GetCompact();
    }

    // Compute the difficulty based on the full adjustment interval.
    const int nHeight = pindexPrev->nHeight;
    assert(nHeight >= params.DifficultyAdjustmentInterval());

    // Get the last suitable block of the difficulty interval.
    const CBlockIndex *pindexLast = GetSuitableBlock(pindexPrev);
    assert(pindexLast);

    // Get the first suitable block of the difficulty interval.
    // Do not consider blocks before hybrid consensus takes effect.
    int nHeightFirst = 0;
    if (pblock->isPaicoinHashBlock())
        nHeightFirst = (nHeight - 144) < (nPaicoinHashHeight + 1) ? (nPaicoinHashHeight + 1) : (nHeight - 144);
    else
        nHeightFirst = (nHeight - 144) < (params.nHybridConsensusHeight + 1) ? (params.nHybridConsensusHeight + 1) : (nHeight - 144);

    const CBlockIndex *pindexFirst = GetSuitableBlock(pindexPrev->GetAncestor(nHeightFirst));
    assert(pindexFirst);
    // Compute the target based on time and work done during the interval.
    const arith_uint256 nextTarget =
        ComputeTarget(pindexFirst, pindexLast, params);

    const arith_uint256 powLimit = UintToArith256(params.hybridConsensusPowLimit);
    if (nextTarget > powLimit) {
        return powLimit.GetCompact();
    }

    return nextTarget.GetCompact();
}

int64_t CalculateNextRequiredStakeDifficulty(const CBlockIndex* pindexLast, const Consensus::Params& params)
{
    // Stake difficulty before any tickets could possibly be purchased is
    // the minimum value.
    auto nextHeight = int64_t{0};
    if (pindexLast != nullptr) {
        nextHeight = pindexLast->nHeight + 1;
    }

    const auto& stakeDiffStartHeight = COINBASE_MATURITY + 1;
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
    const auto& stakeDiffStartHeight = int64_t(COINBASE_MATURITY) + 1;
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
