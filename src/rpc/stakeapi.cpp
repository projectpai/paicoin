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

#include <stdint.h>
#include <univalue.h>

UniValue estimatestakediff(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            "estimatestakediff \"numtickets\"\n"
            "\nEstimate the stake difficulty.\n"
            "\nArguments:\n"
            "1. \"numtickets\"   (integer, optional) The custom number of tickets\n"
            "\nResult:\n"
            "\"min\"             (numeric) Minimum estimate for stake difficulty\n"
            "\"max\"             (numeric) Maximum estimate for stake difficulty\n"
            "\"expected\"        (numeric) Expected estimate for stake difficulty\n"
            "\"user\"            (numeric, optional) Estimate for stake difficulty with the passed user amount of tickets\n"
            "\nExample:\n"
            "\nEstimate stake difficulty\n"
            + HelpExampleCli("estimatestakediff", "1")
        );

    auto userTickets = false;
    if (!request.params.empty() && !request.params[0].isNull()) {
        RPCTypeCheck(request.params, {UniValue::VNUM});

        auto ticketsValue = request.params[0].get_int();
        if (ticketsValue < 0) {
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid number of tickets");
        }
        userTickets = true;
    }

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("min", ValueFromAmount(0)));
    result.push_back(Pair("max", ValueFromAmount(0)));
    result.push_back(Pair("expected", ValueFromAmount(0)));

    if (userTickets) {
        result.push_back(Pair("user", ValueFromAmount(0)));
    }

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

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("current", ValueFromAmount(0)));
    result.push_back(Pair("next", ValueFromAmount(0)));

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

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("currentheight", chainActive.Tip()->nHeight));
    result.push_back(Pair("hash", chainActive.Tip()->GetBlockHash().ToString()));

    uint8_t numVersions = 1;
    UniValue resultIntervals(UniValue::VARR);
    for (auto intervalIdx = 0u; intervalIdx < numIntervals; ++intervalIdx) {
        UniValue resultInterval(UniValue::VOBJ);

        resultInterval.push_back(Pair("startheight", chainActive.Tip()->nHeight));
        resultInterval.push_back(Pair("endheight", chainActive.Tip()->nHeight));

        UniValue posVersions(UniValue::VARR);
        UniValue voteVersions(UniValue::VARR);
        for (auto versionIdx = 0u; versionIdx < numVersions; ++versionIdx) {
            UniValue posVersion(UniValue::VOBJ);
            posVersion.push_back(Pair("version", 0));
            posVersion.push_back(Pair("count", 0));

            UniValue voteVersion(UniValue::VOBJ);
            voteVersion.push_back(Pair("version", 0));
            voteVersion.push_back(Pair("count", 0));

            posVersions.push_back(posVersion);
            voteVersions.push_back(voteVersion);
        }

        resultInterval.push_back(Pair("posversions", posVersions));
        resultInterval.push_back(Pair("voteversions", voteVersions));

        resultIntervals.push_back(resultInterval);
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

    auto intCountBlocks = request.params[1].get_int();
    if (intCountBlocks <= 0) {
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid count of blocks, must be greater than zero");
    }
    auto countBlocks = static_cast<uint32_t>(intCountBlocks);

    size_t numVotes = 1;
    UniValue result(UniValue::VARR);

    for (auto countIdx = 0u; countIdx < countBlocks; ++countIdx) {
        UniValue stakeVersion(UniValue::VOBJ);

        stakeVersion.push_back(Pair("hash", chainActive.Tip()->GetBlockHeader().GetHash().ToString()));
        stakeVersion.push_back(Pair("height", chainActive.Tip()->nHeight));
        stakeVersion.push_back(Pair("blockversion", 0));
        stakeVersion.push_back(Pair("stakeversion", 0));

        UniValue votes(UniValue::VARR);
        for (auto voteIdx = 0u; voteIdx < numVotes; ++voteIdx) {
            UniValue vote(UniValue::VOBJ);
            vote.push_back(Pair("version", 0));
            vote.push_back(Pair("bits", 0));

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
