/* * Copyright (c) 2009-2010 Satoshi Nakamoto
 * Copyright (c) 2009-2016 The Bitcoin Core developers
 * Copyright (c) 2017-2020 Project PAI Foundation
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */


#ifndef PAICOIN_CONSENSUS_PARAMS_H
#define PAICOIN_CONSENSUS_PARAMS_H

#include "uint256.h"
#include <map>
#include <string>
#include "script/script.h"

namespace Consensus {

enum DeploymentPos
{
    DEPLOYMENT_TESTDUMMY,
    DEPLOYMENT_CSV, // Deployment of BIP68, BIP112, and BIP113.
    DEPLOYMENT_SEGWIT, // Deployment of BIP141, BIP143, and BIP147.
    // NOTE: Also add new deployments to VersionBitsDeploymentInfo in versionbits.cpp
    MAX_VERSION_BITS_DEPLOYMENTS
};

/**
 * Struct for each individual consensus rule change using BIP9.
 */
struct BIP9Deployment {
    /** Bit position to select the particular bit in nVersion. */
    int bit;
    /** Start MedianTime for version bits miner confirmation. Can be a date in the past */
    int64_t nStartTime;
    /** Timeout/expiry MedianTime for the deployment attempt. */
    int64_t nTimeout;
};

typedef std::vector<char> ByteVector;

struct TokenPayout {
    std::string sAddress;
    int64_t     nAmount;
};
typedef std::shared_ptr<TokenPayout> TokenPayoutPtr;
typedef std::vector<TokenPayoutPtr>  TokenPayoutVector;

/**
 * Parameters that influence chain consensus.
 */
struct Params {
    uint256 hashGenesisBlock;

    // Subsidy parameters
    int nSubsidyHalvingInterval;
    int nTotalBlockSubsidy;
    int nWorkSubsidyProportion;
    int nStakeSubsidyProportion;

    /** Block height and hash at which BIP34 becomes active */
    int BIP34Height;
    uint256 BIP34Hash;
    /** Block height at which BIP65 becomes active */
    int BIP65Height;
    /** Block height at which BIP66 becomes active */
    int BIP66Height;
    /**
     * Minimum blocks including miner confirmation of the total of 2016 blocks in a retargeting period,
     * (nPowTargetTimespan / nPowTargetSpacing) which is also used for BIP9 deployments.
     * Examples: 1916 for 95%, 1512 for testchains.
     */
    uint32_t nRuleChangeActivationThreshold;
    uint32_t nMinerConfirmationWindow;
    BIP9Deployment vDeployments[MAX_VERSION_BITS_DEPLOYMENTS];
    /** Proof of work parameters */
    uint256 powLimit;
    bool fPowAllowMinDifficultyBlocks;
    bool fPowNoRetargeting;
    int64_t nPowTargetSpacing;
    int64_t nPowTargetTimespan;
    int64_t DifficultyAdjustmentInterval() const { return nPowTargetTimespan / nPowTargetSpacing; }
    uint256 nMinimumChainWork;
    uint256 defaultAssumeValid;

    /** Coinbase address whitelist parameters */
    // Number of blocks after the stake validation height when to disable the coinbase address whitelist
    int nCoinbaseWhitelistExpiration;

    /** Hybrid Consensus fork paramters */
    // Block height at which Hybrid Consensus becomes active
    int nHybridConsensusHeight;
    // Proof of work limit to be used after the Hybrid Consensus fork activates
    uint256 hybridConsensusPowLimit;
    // Difficulty of the first blocks after the hybrid consensus fork
    uint32_t nHybridConsensusInitialDifficulty;
    // Number of blocks to apply the initial Hybrid Consensus difficulty to
    int nHybridConsensusInitialDifficultyBlockCount;

    /** Paicoin Hash proof of work parameters */
    // Block timestamp from which the PoW hashing algorithm MUST be Paicoin Hash
    int64_t nPaicoinHashTimestamp;
    // Any block after this height MUST use Paicoin Hash as the PoW hashing algorithm, i.e. must be after the nPaicoinHashTimestamp.
    int nPaicoinHashMaximumActivationHeight;
    // Difficulty of the first blocks after the Paicoin Hash fork
    uint32_t nPaicoinHashInitialDifficulty;
    // Number of blocks to apply the initial Paicoin Hash difficulty to
    int nPaicoinHashInitialDifficultyBlockCount;

    /** Proof of stake parameters */

    // MinimumStakeDiff if the minimum amount of Atoms required to purchase a
    // take ticket.
    int64_t nMinimumStakeDiff;
    // Ticket pool sizes for PAICoin PoS. This denotes the number of possible
    // buckets/number of different ticket numbers. It is also the number of
    // possible winner numbers there are.
    uint16_t nTicketPoolSize;
    // Average number of tickets per block for Decred PoS.
    uint16_t nTicketsPerBlock;
    // Number of blocks for tickets to mature (spendable at TicketMaturity+1).
    uint16_t nTicketMaturity;
    // Number of blocks for tickets to expire after they have matured. This MUST
    // be >= (StakeEnabledHeight + StakeValidationHeight).
    uint32_t nTicketExpiry;
    // Number of blocks after which a vote that has not been included in the chain and still
    // lingers in the mempool will be discarded. Must be less than nTicketExpiry.
    uint32_t nMempoolVoteExpiry;
    // CoinbaseMaturity is the number of blocks required before newly mined
    // coins (coinbase transactions) can be spent.
    // uint16_t nCoinbaseMaturity; We have a constant for this purpose in consensus.h, COINBASE_MATURITY

    // Maturity for spending SStx change outputs.
    uint16_t nSStxChangeMaturity;
    // TicketPoolSizeWeight is the multiplicative weight applied to the
    // ticket pool size difference between a window period and its target
    // when determining the stake system.
    uint16_t nTicketPoolSizeWeight;
    // StakeDiffAlpha is the stake difficulty EMA calculation alpha (smoothing)
    // value. It is different from a normal EMA alpha. Closer to 1 --> smoother.
    int64_t nStakeDiffAlpha;
    // StakeDiffWindowSize is the number of blocks used for each interval in
    // exponentially weighted average.
    int64_t nStakeDiffWindowSize;
    // StakeDiffWindows is the number of windows (intervals) used for calculation
    // of the exponentially weighted average.
    int64_t nStakeDiffWindows;
    // StakeVersionInterval determines the interval where the stake version
    // is calculated.
    int64_t nStakeVersionInterval;
    // MaxFreshStakePerBlock is the maximum number of new tickets that may be
    // submitted per block.
    uint8_t nMaxFreshStakePerBlock;
    // StakeEnabledHeight is the height in which the first ticket could possibly
    // mature.
    int32_t nStakeEnabledHeight;
    // StakeValidationHeight is the height at which votes (SSGen) are required
    // to add a new block to the top of the blockchain. This height is the
    // first block that will be voted on, but will include in itself no votes.
    int32_t nStakeValidationHeight;
    // StakeBaseSigScript is the consensus stakebase signature script for all
    // votes on the network. This isn't signed in any way, so without forcing
    // it to be this value miners/daemons could freely change it.
    CScript stakeBaseSigScript;
    // StakeMajorityMultiplier and StakeMajorityDivisor are used
    // to calculate the super majority of stake votes using integer math as
    // such: X*StakeMajorityMultiplier/StakeMajorityDivisor
    int32_t nStakeMajorityMultiplier;
    int32_t nStakeMajorityDivisor;
    // A ticket contributor may limit the amount of fee the corresponding vote or
    // revocation may charge from his/her reward or refund by specifying a fee limit
    // when the ticket is purchased. If all the contributor specify a zero or very low
    // limit, it might be possible that the ticket cannot vote or be revoked, because the
    // corresponding transaction fees might exceed the total allowed fee. Therefore,
    // the following two minimums have been enforced for the total ticket fee limits. A
    // ticket that does not allow at least that much fee are not going to be accepted by
    // the network. However, since votes are essential for the functioning of the
    // blockchain, they are allowed to have zero fee.
    int64_t nMinimumTotalVoteFeeLimit;
    int64_t nMinimumTotalRevocationFeeLimit;
    // OrganizationPkScript is the output script for block taxes to be
    // distributed to in every block's coinbase. It should ideally be a P2SH
    // multisignature address.  OrganizationPkScriptVersion is the version
    // of the output script.  Until PoS hardforking is implemented, this
    // version must always match for a block to validate.
    CScript organizationPkScript;
    uint16_t nOrganizationPkScriptVersion;
    // BlockOneLedger specifies the list of payouts in the coinbase of
    // block height 1. If there are no payouts to be given, set this
    // to an empty slice.
    TokenPayoutVector vBlockOneLedger;

    int TotalSubsidyProportions() const { return nWorkSubsidyProportion + nStakeSubsidyProportion; }
};
} // namespace Consensus

#endif // PAICOIN_CONSENSUS_PARAMS_H
