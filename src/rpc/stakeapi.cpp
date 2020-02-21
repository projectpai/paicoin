// Copyright (c) 2019 The PAIcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chain.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "rpc/safemode.h"
#include "rpc/server.h"
#include "sync.h"
#include "utilstrencodings.h"
#include "validation.h"
#include "pow.h"
#include "stake/stakeversion.h"

#include <stdint.h>
#include <univalue.h>

// StakeVersions is a condensed form of a dcrutil.Block that is used to prevent
// using gigabytes of memory.
struct StakeVersions {
    uint256 hash;
    int height;
    int32_t blockVersion;
    uint32_t stakeVersion;
    VoteVersionVector votes;
};


std::vector<StakeVersions> GetStakeVersions(const CBlockIndex* const startIndex, int count)
{
    if (startIndex == nullptr){
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid block index.");
    }
    auto result = std::vector<StakeVersions>{};
    // Nothing to do if no count requested.
    if (count == 0) {
        return result;
    }
    if (count < 0) {
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid count, must be greater than zero");
    }
    // Limit the requested count to the max possible for the requested block.
    if (count > startIndex->nHeight + 1) {
        count = startIndex->nHeight + 1;
    }

    auto prevIndex = startIndex;
    for (auto i = 0; prevIndex != nullptr && i < count; i++) {
        result.push_back(StakeVersions{
            prevIndex->GetBlockHash(),
            prevIndex->nHeight,
            prevIndex->nVersion,
            prevIndex->nStakeVersion,
            prevIndex->votes});
        prevIndex = prevIndex->pprev;
    }

    return result;
};

UniValue estimatestakediff(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            "estimatestakediff \"numtickets\"\n"
            "\nEstimate the next minimum, maximum, expected, and user-specified stake difficulty.\n"
            "\nArguments:\n"
            "1. \"numtickets\"   (integer, optional) Use this number of new tickets in blocks to estimate the next difficulty\n"
            "\nResult:\n"
            "\"min\"             (numeric) Minimum estimate for stake difficulty\n"
            "\"max\"             (numeric) Maximum estimate for stake difficulty\n"
            "\"expected\"        (numeric) Expected estimate for stake difficulty\n"
            "\"user\"            (numeric) Estimate for stake difficulty with the passed user amount of tickets\n"
            "\nExample:\n"
            "\nEstimate stake difficulty\n"
            + HelpExampleCli("estimatestakediff", "1")
        );

    const auto& params = Params().GetConsensus();
    auto userEstimate = int64_t{0};
    if (!request.params.empty() && !request.params[0].isNull()) {
        RPCTypeCheck(request.params, {UniValue::VNUM});

        auto ticketsValue = request.params[0].get_int();
        if (ticketsValue < 0) {
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid number of tickets");
        }
        // User-specified stake difficulty, if they asked for one.
        LOCK(cs_main);
        userEstimate = EstimateNextStakeDifficulty(chainActive.Tip(), ticketsValue, false, params);
    }

    // Minimum possible stake difficulty.
    LOCK(cs_main);
    const auto& min = EstimateNextStakeDifficulty(chainActive.Tip(), 0, false, params);

    // Maximum possible stake difficulty.
    const auto& max = EstimateNextStakeDifficulty(chainActive.Tip(), 0, true, params);

    // The expected stake difficulty. Average the number of fresh stake
    // since the last retarget to get the number of tickets per block,
    // then use that to estimate the next stake difficulty.
    const auto& bestHeight = chainActive.Tip()->nHeight;
    const auto& lastAdjustment = (bestHeight / params.nStakeDiffWindowSize) * params.nStakeDiffWindowSize;
    const auto& nextAdjustment = ((bestHeight / params.nStakeDiffWindowSize) + 1) * params.nStakeDiffWindowSize;
    auto totalTickets = 0;
    for (auto i = lastAdjustment; i <= bestHeight; i++) {
        const auto& bh = chainActive.Tip()->GetAncestor(i)->GetBlockHeader();
        totalTickets += bh.nFreshStake;
    }
    const auto& blocksSince = bestHeight - lastAdjustment + 1;
    const auto& remaining = nextAdjustment - bestHeight - 1;
    const auto& averagePerBlock = double(totalTickets) / blocksSince;
    const auto& expectedTickets = floor(averagePerBlock * remaining);
    const auto& expected = EstimateNextStakeDifficulty(chainActive.Tip(), expectedTickets, false, params);

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("min", ValueFromAmount(min)));
    result.push_back(Pair("max", ValueFromAmount(max)));
    result.push_back(Pair("expected", ValueFromAmount(expected)));
    result.push_back(Pair("user", ValueFromAmount(userEstimate)));

    return result;
}

UniValue getstakedifficulty(const JSONRPCRequest& request)
{
    if (request.fHelp || !request.params.empty())
        throw std::runtime_error(
            "getstakedifficulty\n"
            "\nReturns the proof-of-stake difficulty.\n"
            "\nResult:\n"
            "\"current\"         (numeric) The current top block's stake difficulty\n"
            "\"next\"            (numeric) The calculated stake difficulty of the next block\n"
            "\nExample:\n"
            "\nGet stake difficulty\n"
            + HelpExampleCli("getstakedifficulty", "")
        );


    LOCK(cs_main);
    const auto& blockHeader = chainActive.Tip()->GetBlockHeader();
    const auto& currentSdiff = blockHeader.nStakeDifficulty;
    const auto& nextSdiff = CalculateNextRequiredStakeDifficulty(chainActive.Tip(), Params().GetConsensus());

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("current", ValueFromAmount(currentSdiff)));
    result.push_back(Pair("next", ValueFromAmount(nextSdiff)));

    return result;
}

UniValue getstakeversioninfo(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            "getstakeversioninfo \"count\"\n"
            "\nReturns stake version statistics for one or more stake version intervals.\n"
            "\nArguments:\n"
            "1. \"count\"               (integer, optional) Number of intervals to return.\n"
            "\nResult:\n"
            "\"currentheight\"          (numeric) Top of the chain height.\n"
            "\"hash\"                   (string)  Top of the chain hash.\n"
            "\"intervals\"              (array)   Array of total stake and vote counts.\n"
            "  [\n"
            "     {\n"
            "       \"startheight\":    (numeric) Start of the interval.\n"
            "       \"endheight\":      (numeric) End of the interval.\n"
            "       \"posversions\":    (array)   Tally of the stake versions.\n"
            "         [\n"
            "           {\n"
            "             \"version\":  (numeric) Version of the vote.\n"
            "             \"count\":    (numeric) Number of votes.\n"
            "           },\n"
            "           ...\n"
            "         ]\n"
            "       \"voteversions\":   (array)   Tally of all vote versions.\n"
            "         [\n"
            "           {\n"
            "             \"version\":  (numeric) Version of the vote.\n"
            "             \"count\":    (numeric) Number of votes.\n"
            "           },\n"
            "           ...\n"
            "         ]\n"
            "     },\n"
            "     ...\n"
            "  ]\n"
            "\nExample:\n"
            "\nGet stake version info with default count of one interval\n"
            + HelpExampleCli("getstakeversioninfo", "1") +
            "\nGet stake version info with custom count of intervals\n"
            + HelpExampleCli("getstakeversioninfo", "5")
        );

    auto numIntervals = 1u;
    if (!request.params.empty() && !request.params[0].isNull()) {
        RPCTypeCheck(request.params, {UniValue::VNUM});

        auto intNumIntervals = request.params[0].get_int64();
        if (intNumIntervals <= 0) {
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid number of intervals, must be greater than zero");
        }

        numIntervals = static_cast<decltype(numIntervals)>(intNumIntervals);
    }

    LOCK(cs_main);
    auto pCurrentIndex = chainActive.Tip();
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("currentheight", pCurrentIndex->nHeight));
    result.push_back(Pair("hash", pCurrentIndex->GetBlockHash().ToString()));

    const auto& stakeVersionInterval = Params().GetConsensus().nStakeVersionInterval;
    const auto& stakeValidationHeight = Params().GetConsensus().nStakeValidationHeight;

    auto startHeight = pCurrentIndex->nHeight;
    auto endHeight = calcWantHeight(stakeValidationHeight, stakeVersionInterval, startHeight) + 1;
    UniValue resultIntervals(UniValue::VARR);
    auto adjust = 1;
    for (auto intervalIdx = 0u; intervalIdx < numIntervals; ++intervalIdx)
    {
        const auto& numBlocks = startHeight - endHeight;
        if (numBlocks <= 0) {
            // Just return what we got.
            break;
        }

        const auto& sv = GetStakeVersions(pCurrentIndex, numBlocks+adjust);
        auto posVersions = std::map<int,int>{};
        auto voteVersions = std::map<int,int>{};
        for (const auto& v : sv)
        {
            posVersions[int(v.stakeVersion)]++;
            for (const auto& vote : v.votes)
            {
                voteVersions[int(vote.Version)]++;
            }
        }

        UniValue versionInterval(UniValue::VOBJ);

        versionInterval.push_back(Pair("startheight", startHeight));
        versionInterval.push_back(Pair("endheight", endHeight));

        auto ConvertVersionMap = [](const std::map<int, int>& versionsMap) {
            UniValue versions(UniValue::VARR);
            for (const auto& kv : versionsMap) {
                UniValue version(UniValue::VOBJ);
                version.push_back(Pair("version", kv.first));
                version.push_back(Pair("count", kv.second));
                versions.push_back(version);
            }
            return versions;
        };


        versionInterval.push_back(Pair("posversions", ConvertVersionMap(posVersions)));
        versionInterval.push_back(Pair("voteversions", ConvertVersionMap(voteVersions)));

        resultIntervals.push_back(versionInterval);

        // Adjust interval.
        endHeight -= stakeVersionInterval;
        startHeight = endHeight + stakeVersionInterval;
        adjust = 0;

        // Get prior block index.
        pCurrentIndex = pCurrentIndex->pprev;
    }

    result.push_back(Pair("intervals", resultIntervals));

    return result;
}

UniValue getstakeversions(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "getstakeversions \"count\"\n"
            "\nReturns the stake versions statistics.\n"
            "\nArguments:\n"
            "1. \"hash\"                (string)  The start block hash.\n"
            "1. \"count\"               (integer) The number of blocks that will be returned.\n"
            "\nResult:\n"
            "\"stakeversions\"          (array)   Array of stake versions per block.\n"
            "  [\n"
            "     {\n"
            "       \"hash\":           (string)  Hash of the block.\n"
            "       \"height\":         (numeric) Height of the block.\n"
            "       \"blockversion\":   (numeric) The block version\n"
            "       \"stakeversion\":   (numeric) The stake version of the block\n"
            "       \"votes\":          (array)   The version and bits of each vote in the block.\n"
            "         [\n"
            "           {\n"
            "             \"version\":  (numeric) The version of the vote.\n"
            "             \"bits\":     (numeric) The bits assigned by the vote.\n"
            "           },\n"
            "           ...\n"
            "         ]\n"
            "     },\n"
            "     ...\n"
            "  ]\n"
            "\nExample:\n"
            "\nGet stake versions info\n"
            + HelpExampleCli("getstakeversions", chainActive.Tip()->GetBlockHeader().GetHash().ToString() + " 1")
        );

    RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::VNUM});
    auto hashStr = request.params[0].get_str();
    if (!IsHex(hashStr)) {
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid hash format provided");
    }
    const auto hash = uint256S(hashStr);

    auto intCountBlocks = request.params[1].get_int();
    if (intCountBlocks <= 0) {
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid count of blocks, must be greater than zero");
    }

    LOCK(cs_main);

    if (mapBlockIndex.count(hash) == 0)
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Block not found");

    const auto * const pblockindex = mapBlockIndex[hash];

    const auto& sv = GetStakeVersions(pblockindex, intCountBlocks);

    UniValue result(UniValue::VARR);

    for (const auto& v : sv) {
        UniValue stakeVersion(UniValue::VOBJ);

        stakeVersion.push_back(Pair("hash", v.hash.ToString()));
        stakeVersion.push_back(Pair("height", v.height));
        stakeVersion.push_back(Pair("blockversion", v.blockVersion));
        stakeVersion.push_back(Pair("stakeversion", static_cast<int>(v.stakeVersion)));

        UniValue votes(UniValue::VARR);
        for (const auto& voteIt : v.votes) {
            UniValue vote(UniValue::VOBJ);
            vote.push_back(Pair("version", static_cast<int>(voteIt.Version)));
            vote.push_back(Pair("bits", voteIt.Bits));

            votes.push_back(vote);
        }

        stakeVersion.push_back(Pair("votes", votes));
        result.push_back(stakeVersion);
    }

    return result;
}

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
  //  --------------------- ------------------------  -----------------------  ----------
    { "stakeapi",           "estimatestakediff",      &estimatestakediff,      {"numtickets"}   },
    { "stakeapi",           "getstakedifficulty",     &getstakedifficulty,     {}               },
    { "stakeapi",           "getstakeversioninfo",    &getstakeversioninfo,    {"count"}        },
    { "stakeapi",           "getstakeversions",       &getstakeversions,       {"hash", "count"}},
};

void RegisterStakeAPIRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
