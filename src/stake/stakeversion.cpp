#include "stakeversion.h"

#include "primitives/block.h"

#include <map>

enum {
    StakeIntervalError_BadNode = -1,
    StakeIntervalError_MajorityNotFound = -2
};

struct StakeMajorityKey {
    uint32_t version;
    uint256 hash;

    bool operator<(const StakeMajorityKey& key) const
    {
       return version < key.version && hash < key.hash;
    }
};

// cached state of stake versions related to intervals;
// to protect access, calls of functions below must be guarded by locks.
std::map<StakeMajorityKey,bool> isStakeMajorityVersionCache;
std::map<uint256,uint32_t> priorStakeVersionCache;
std::map<uint256,uint32_t> calcVoterVersionIntervalCache;
std::map<uint256,uint32_t> stakeVersionCache;

// stakeMajorityCacheVersionKey creates a map key that is comprised of a stake
// version and a hash.  This is used for caches that require a version in
// addition to a simple hash.
StakeMajorityKey stakeMajorityCacheVersionKey(uint32_t version, const uint256* phash)
{
    StakeMajorityKey key = { version, *phash };
    return key;
}

// calcWantHeight calculates the height of the final block of the previous interval
// given a stake validation height, stake validation interval, and block height.
int64_t calcWantHeight(int64_t stakeValidationHeight, int64_t interval, int64_t height)
{
    // The adjusted height accounts for the fact the starting validation height does not necessarily start on an interval
    // and thus the intervals might not be zero-based.
    const auto& intervalOffset = stakeValidationHeight % interval;
    const auto& adjustedHeight = height - intervalOffset - 1;
    return (adjustedHeight - ((adjustedHeight + 1) % interval)) + intervalOffset;
}

// findStakeVersionPriorNode walks the chain backwards from prevNode until it
// reaches the final block of the previous stake version interval and returns
// that node.  The returned node will be nil when the provided prevNode is too
// low such that there is no previous stake version interval due to falling
// prior to the stake validation interval.
//
// This function MUST be called with the chain state lock held (for writes).
const CBlockIndex* findStakeVersionPriorNode(const CBlockIndex *pprevIndex, const Consensus::Params& params)
{
    // Check to see if the blockchain is high enough to begin accounting stake versions.
    uint32_t nextHeight = pprevIndex->nHeight + 1;
    if (nextHeight < params.nStakeValidationHeight + params.nStakeVersionInterval)
        return nullptr;

    uint32_t wantHeight = calcWantHeight(params.nStakeValidationHeight, params.nStakeVersionInterval, nextHeight);
    return pprevIndex->GetAncestor(wantHeight);
}

// isStakeMajorityVersion determines if minVer requirement is met based on
// prevNode.  The function always uses the stake versions of the prior window.
// For example, if StakeVersionInterval = 11 and StakeValidationHeight = 13 the
// windows start at 13 + (11 * 2) 25 and are as follows: 24-34, 35-45, 46-56 ...
// If height comes in at 35 we use the 24-34 window, up to height 45.
// If height comes in at 46 we use the 35-45 window, up to height 56 etc.
//
// This function MUST be called with the chain state lock held (for writes).
bool isStakeMajorityVersion(uint32_t minVer, const CBlockIndex* pprevIndex, const Consensus::Params& params)
{
    // Walk blockchain backwards to calculate version.
    const CBlockIndex *pIndex = findStakeVersionPriorNode(pprevIndex, params);
    if (pIndex == nullptr)
        return 0 >= minVer;

    // Generate map key and look up cached result.
    auto key = stakeMajorityCacheVersionKey(minVer, pIndex->phashBlock);
    if (isStakeMajorityVersionCache.find(key) != isStakeMajorityVersionCache.end())
        return isStakeMajorityVersionCache[key];

    // Tally how many of the block headers in the previous stake version validation interval
    // have their stake version set to at least the requested minimum version.
    int versionCount = 0;
    const CBlockIndex *pIterIndex = pIndex;
    for (int i = 0; i < params.nStakeVersionInterval && pIterIndex != nullptr; i++) {
        if (pIterIndex->nStakeVersion >= minVer)
            versionCount++;

        pIterIndex = pIterIndex->pprev;
    }

    // Determine the required amount of votes to reach supermajority.
    auto numRequired = params.nStakeVersionInterval * params.nStakeMajorityMultiplier / params.nStakeMajorityDivisor;

    // Cache result.
    bool result = versionCount >= numRequired;
    isStakeMajorityVersionCache[key] = result;

    return result;
}

// calcPriorStakeVersion calculates the header stake version of the prior
// interval. The function walks the chain backwards by one interval and then
// it performs a standard majority calculation.
//
// This function MUST be called with the chain state lock held (for writes).
int calcPriorStakeVersion(const CBlockIndex *pprevIndex, const Consensus::Params& params)
{
    // Walk blockchain backwards to calculate version.
    const CBlockIndex* pIndex = findStakeVersionPriorNode(pprevIndex, params);
    if (pIndex == nullptr)
        return -1;

    // Check cache.
    if (priorStakeVersionCache.find(pIndex->GetBlockHash()) != priorStakeVersionCache.end())
        return priorStakeVersionCache[pIndex->GetBlockHash()];

    // Tally how many of each stake version the block headers in the previous stake
    // version validation interval have.
    std::map<uint32_t,uint32_t> versions;
    const CBlockIndex *pIterNode = pIndex;
    for (int i = 0; i < params.nStakeVersionInterval && pIterNode != nullptr; i++) {
        if (versions.find(pIterNode->nStakeVersion) == versions.end())
            versions[pIterNode->nStakeVersion] = 1;
        else
            versions[pIterNode->nStakeVersion]++;

        pIterNode = pIterNode->pprev;
    }

    // Determine the required amount of votes to reach supermajority.
    auto numRequired = params.nStakeVersionInterval * params.nStakeMajorityMultiplier / params.nStakeMajorityDivisor;

    for (auto elem : versions) {
        auto count = elem.second;
        if (count >= numRequired) {
            auto version = elem.first;
            priorStakeVersionCache[pIndex->GetBlockHash()] = version;
            return version;
        }
    }

    return StakeIntervalError_MajorityNotFound;
}

// calcVoterVersionInterval tallies all voter versions in an interval and
// returns a version that has reached 75% majority.  This function MUST be
// called with a node that is the final node in a valid stake version interval
// and greater than or equal to the stake validation height or it will result in
// an assertion error.
//
// This function is really meant to be called internally only from this file.
//
// This function MUST be called with the chain state lock held (for writes).
int calcVoterVersionInterval(const CBlockIndex *pprevIndex, const Consensus::Params& params)
{
    // Ensure the provided node is the final node in a valid stake version
    // interval and is greater than or equal to the stake validation height
    // since the logic below relies on these assumptions.
    int expectedHeight = calcWantHeight(params.nStakeValidationHeight, params.nStakeVersionInterval, pprevIndex->nHeight + 1);
    if (pprevIndex->nHeight != expectedHeight || expectedHeight < params.nStakeValidationHeight) {
        assert(!"calcVoterVersionInterval must be called with a node that is the final node in a stake version interval");
        return StakeIntervalError_BadNode;
    }

    // See if we have cached results.
    if (calcVoterVersionIntervalCache.find(pprevIndex->GetBlockHash()) != calcVoterVersionIntervalCache.end())
        return (int)calcVoterVersionIntervalCache[pprevIndex->GetBlockHash()];

    // Tally both the total number of votes in the previous stake version validation
    // interval and how many of each version those votes have.
    std::map<uint32_t, uint32_t> versions; // version -> count
    uint32_t totalVotesFound = 0;
    const CBlockIndex *pIterIndex = pprevIndex;
    for (int i = 0; i < params.nStakeVersionInterval && pIterIndex != nullptr; i++) {
        totalVotesFound += pIterIndex->votes.size();
        for (auto &v : pIterIndex->votes) {
            if (versions.find(v.Version) == versions.end())
                versions[v.Version] = 1;
            else
                versions[v.Version]++;
        }
        pIterIndex = pIterIndex->pprev;
    }

    // Determine the required amount of votes to reach supermajority.
    auto numRequired = totalVotesFound * params.nStakeMajorityMultiplier / params.nStakeMajorityDivisor;

    for (auto elem : versions) {
        auto count = elem.second;
        if (count >= numRequired) {
            auto version = elem.first;
            calcVoterVersionIntervalCache[pprevIndex->GetBlockHash()] = version;
            return version;
        }
    }

    return StakeIntervalError_MajorityNotFound;
}

// calcVoterVersion calculates the last prior valid majority stake version.  If
// the current interval does not have a majority stake version it'll go back to
// the prior interval.  It'll keep going back up to the minimum height at which
// point we know the version was 0.
//
// This function MUST be called with the chain state lock held (for writes).
uint32_t calcVoterVersion(const CBlockIndex* pprevIndex, CBlockIndex **pIntervalNodePtr, const Consensus::Params& params)
{
    // Walk blockchain backwards to find interval.
    const CBlockIndex *pIndex = findStakeVersionPriorNode(pprevIndex, params);

    // Iterate over versions until a majority is found.
    // Don't try to count votes before the stake validation height since there could not possibly have been any.
    while (pIndex != nullptr && pIndex->nHeight >= params.nStakeValidationHeight) {
        int version = calcVoterVersionInterval(pIndex, params);
        if (version >= 0) {
            *pIntervalNodePtr = (CBlockIndex*)pIndex;
            return version;
        }
        if (version != StakeIntervalError_MajorityNotFound)
            break;

        pIndex = pIndex->GetRelativeAncestor(params.nStakeVersionInterval);
    }

    // No majority version found.
    return 0;
}

// calcStakeVersion calculates the header stake version based on voter versions.
// If there is a majority of voter versions it uses the header stake version to
// prevent reverting to a prior version.
//
// This function MUST be called with the chain state lock held (for writes).
uint32_t calcStakeVersion(const CBlockIndex *pprevIndex, const Consensus::Params& params)
{
    CBlockIndex *pIndex = nullptr;
    uint32_t version = calcVoterVersion(pprevIndex, &pIndex, params);
    if (version == 0 || pIndex == nullptr)
        return 0;

    // Check cache.
    if (stakeVersionCache.find(pIndex->GetBlockHash()) != stakeVersionCache.end())
        return stakeVersionCache[pIndex->GetBlockHash()];

    // Walk chain backwards to start of node interval (start of current
    // period) Note that calcWantHeight returns the LAST height of the
    // prior interval; hence the + 1.
    uint32_t startIntervalHeight = calcWantHeight(params.nStakeValidationHeight, params.nStakeVersionInterval, pIndex->nHeight) + 1;
    CBlockIndex* pStartNode = pIndex->GetAncestor(startIntervalHeight);
    if (pStartNode == nullptr) {
        // Note that should this not be possible to hit because a
        // majority voter version was obtained above, which means there
        // is at least an interval of nodes.  However, be paranoid.
        stakeVersionCache[pIndex->GetBlockHash()] = 0;
    }

    // Don't allow the stake version to go backwards once it has been locked in by a previous majority,
    // even if the majority of votes are now a lower version.
    if (isStakeMajorityVersion(version, pIndex, params)) {
        int priorVersion = calcPriorStakeVersion(pIndex, params);
        if (priorVersion >= 0 && (uint32_t)priorVersion > version)
            version = (uint32_t) priorVersion;
    }

    stakeVersionCache[pIndex->GetBlockHash()] = version;
    return version;
}
