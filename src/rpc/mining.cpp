//
// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017-2020 Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//


#include <amount.h>
#include <chain.h>
#include <chainparams.h>
#include <consensus/consensus.h>
#include <consensus/params.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <init.h>
#include <validation.h>
#include <key_io.h>
#include <miner.h>
#include <net.h>
#include <policy/fees.h>
#include <policy/policy.h>
#include <pow.h>
#include <rpc/blockchain.h>
#include <rpc/mining.h>
#include <rpc/server.h>
#include <txmempool.h>
#include <util.h>
#include <utilstrencodings.h>
#include <validationinterface.h>
#include <warnings.h>

#include <memory>
#include <stdint.h>

#include <univalue.h>

unsigned int ParseConfirmTarget(const UniValue& value)
{
    const auto target = value.get_int();
    const unsigned int max_target{::feeEstimator.HighestTargetTracked(FeeEstimateHorizon::LONG_HALFLIFE)};
    if (target < 1 || static_cast<unsigned int>(target) > max_target) {
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, strprintf("Invalid conf_target, must be between %u - %u", 1, max_target));
    }
    return static_cast<unsigned int>(target);
}

/**
 * Return average network hashes per second based on the last 'lookup' blocks,
 * or from the last difficulty change if 'lookup' is nonpositive.
 * If 'height' is nonnegative, compute the estimate at the time when a given block was found.
 */
UniValue GetNetworkHashPS(int lookup, int height) {
    const auto* pb = chainActive.Tip();

    if (height >= 0 && height < chainActive.Height())
        pb = chainActive[height];

    if (pb == nullptr || !pb->nHeight)
        return 0;

    // If lookup is -1, then use blocks since last difficulty change.
    if (lookup <= 0)
        lookup = pb->nHeight % Params().GetConsensus().DifficultyAdjustmentInterval() + 1;

    // If lookup is larger than chain, then set it to chain length.
    if (lookup > pb->nHeight)
        lookup = pb->nHeight;

    auto pb0 = pb;
    auto minTime = pb0->GetBlockTime();
    auto maxTime = minTime;
    for (int i{0}; i < lookup; ++i) {
        pb0 = pb0->pprev;
        const auto time = pb0->GetBlockTime();
        minTime = std::min(time, minTime);
        maxTime = std::max(time, maxTime);
    }

    // In case there's a situation where minTime == maxTime, we don't want a divide by zero exception.
    if (minTime == maxTime)
        return 0;

    const arith_uint256 workDiff{pb->nChainWork - pb0->nChainWork};
    const int64_t timeDiff{maxTime - minTime};

    return workDiff.getdouble() / timeDiff;
}

UniValue getnetworkhashps(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 2)
        throw std::runtime_error{
            "getnetworkhashps ( nblocks height )\n"
            "\nReturns the estimated network hashes per second based on the last n blocks.\n"
            "Pass in [blocks] to override # of blocks, -1 specifies since last difficulty change.\n"
            "Pass in [height] to estimate the network speed at the time when a certain block was found.\n"
            "\nArguments:\n"
            "1. nblocks     (numeric, optional, default=120) The number of blocks, or -1 for blocks since last difficulty change.\n"
            "2. height      (numeric, optional, default=-1) To estimate at the time of the given height.\n"
            "\nResult:\n"
            "x             (numeric) Hashes per second estimated\n"
            "\nExamples:\n"
            + HelpExampleCli("getnetworkhashps", "")
            + HelpExampleRpc("getnetworkhashps", "")
        };

    LOCK(cs_main);
    return GetNetworkHashPS(!request.params[0].isNull() ? request.params[0].get_int() : 120, !request.params[1].isNull() ? request.params[1].get_int() : -1);
}

UniValue generateBlocks(std::shared_ptr<CReserveScript> coinbaseScript, int nGenerate, uint64_t nMaxTries, bool keepScript)
{
    const int nInnerLoopCount{0x10000};
    int nHeightEnd{0};
    int nHeight{0};

    {   // Don't keep cs_main locked
        LOCK(cs_main);
        nHeight = chainActive.Height();
        nHeightEnd = nHeight+nGenerate;
    }
    unsigned int nExtraNonce{0};
    UniValue blockHashes{UniValue::VARR};
    while (nHeight < nHeightEnd)
    {
        auto pblocktemplate = BlockAssembler(Params()).CreateNewBlock(coinbaseScript->reserveScript);
        if (!pblocktemplate.get())
            throw JSONRPCError(RPCErrorCode::INTERNAL_ERROR, "Couldn't create new block");

        auto * const pblock = &pblocktemplate->block;
        {
            LOCK(cs_main);
            IncrementExtraNonce(pblock, chainActive.Tip(), nExtraNonce);
        }
        while (nMaxTries > 0 && pblock->nNonce < nInnerLoopCount && !CheckProofOfWork(pblock->GetHash(), pblock->nBits, pblock->nVersion, Params().GetConsensus())) {
            ++pblock->nNonce;
            --nMaxTries;
        }
        if (nMaxTries == 0) {
            break;
        }
        if (pblock->nNonce == nInnerLoopCount) {
            continue;
        }
        const auto shared_pblock = std::make_shared<const CBlock>(*pblock);
        if (!ProcessNewBlock(Params(), shared_pblock, true, nullptr))
            throw JSONRPCError(RPCErrorCode::INTERNAL_ERROR, "ProcessNewBlock, block not accepted");
        ++nHeight;
        blockHashes.push_back(pblock->GetHash().GetHex());

        //mark script as important because it was used at least for one coinbase output if the script came from the wallet
        if (keepScript)
        {
            coinbaseScript->KeepScript();
        }
    }
    return blockHashes;
}

UniValue generatetoaddress(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 3)
        throw std::runtime_error{
            "generatetoaddress nblocks address (maxtries)\n"
            "\nMine blocks immediately to a specified address (before the RPC call returns)\n"
            "\nArguments:\n"
            "1. nblocks      (numeric, required) How many blocks are generated immediately.\n"
            "2. address      (string, required) The address to send the newly generated paicoin to.\n"
            "3. maxtries     (numeric, optional) How many iterations to try (default = 1000000).\n"
            "\nResult:\n"
            "[ blockhashes ]     (array) hashes of blocks generated\n"
            "\nExamples:\n"
            "\nGenerate 11 blocks to myaddress\n"
            + HelpExampleCli("generatetoaddress", "11 \"myaddress\"")
        };

    const auto nGenerate = request.params[0].get_int();
    uint64_t nMaxTries{1000000};
    if (!request.params[2].isNull()) {
        nMaxTries = request.params[2].get_int();
    }

    const auto destination = DecodeDestination(request.params[1].get_str());
    if (!IsValidDestination(destination)) {
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Error: Invalid address");
    }

    const auto coinbaseScript = std::make_shared<CReserveScript>();
    coinbaseScript->reserveScript = GetScriptForDestination(destination);

    return generateBlocks(coinbaseScript, nGenerate, nMaxTries, false);
}

UniValue getmininginfo(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error{
            "getmininginfo\n"
            "\nReturns a json object containing mining-related information."
            "\nResult:\n"
            "{\n"
            "  \"blocks\": nnn,             (numeric) The current block\n"
            "  \"currentblockweight\": nnn, (numeric) The last block weight\n"
            "  \"currentblocktx\": nnn,     (numeric) The last block transaction\n"
            "  \"difficulty\": xxx.xxxxx    (numeric) The current difficulty\n"
            "  \"networkhashps\": nnn,      (numeric) The network hashes per second\n"
            "  \"pooledtx\": n              (numeric) The size of the mempool\n"
            "  \"chain\": \"xxxx\",           (string) current network name as defined in BIP70 (main, test, regtest)\n"
            "  \"warnings\": \"...\"          (string) any network and blockchain warnings\n"
            "  \"errors\": \"...\"            (string) DEPRECATED. Same as warnings. Only shown when paicoind is started with -deprecatedrpc=getmininginfo\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getmininginfo", "")
            + HelpExampleRpc("getmininginfo", "")
        };

    LOCK(cs_main);

    UniValue obj{UniValue::VOBJ};
    obj.push_back(Pair("blocks",           static_cast<int>(chainActive.Height())));
    obj.push_back(Pair("currentblockweight", static_cast<uint64_t>(nLastBlockWeight)));
    obj.push_back(Pair("currentblocktx",   static_cast<uint64_t>(nLastBlockTx)));
    obj.push_back(Pair("difficulty",       static_cast<double>(GetDifficulty())));
    obj.push_back(Pair("networkhashps",    getnetworkhashps(request)));
    obj.push_back(Pair("pooledtx",         static_cast<uint64_t>(mempool.size())));
    obj.push_back(Pair("chain",            Params().NetworkIDString()));
    if (IsDeprecatedRPCEnabled("getmininginfo")) {
        obj.push_back(Pair("errors",       GetWarnings("statusbar")));
    } else {
        obj.push_back(Pair("warnings",     GetWarnings("statusbar")));
    }
    return obj;
}


// NOTE: Unlike wallet RPC (which use PAI values), mining RPCs follow GBT (BIP 22) in using satoshi amounts
UniValue prioritisetransaction(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 3)
        throw std::runtime_error{
            "prioritisetransaction <txid> <dummy value> <fee delta>\n"
            "Accepts the transaction into mined blocks at a higher (or lower) priority\n"
            "\nArguments:\n"
            "1. \"txid\"       (string, required) The transaction id.\n"
            "2. dummy          (numeric, optional) API-Compatibility for previous API. Must be zero or null.\n"
            "                  DEPRECATED. For forward compatibility use named arguments and omit this parameter.\n"
            "3. fee_delta      (numeric, required) The fee value (in satoshis) to add (or subtract, if negative).\n"
            "                  The fee is not actually paid, only the algorithm for selecting transactions into a block\n"
            "                  considers the transaction as it would have paid a higher (or lower) fee.\n"
            "\nResult:\n"
            "true              (boolean) Returns true\n"
            "\nExamples:\n"
            + HelpExampleCli("prioritisetransaction", "\"txid\" 0.0 10000")
            + HelpExampleRpc("prioritisetransaction", "\"txid\", 0.0, 10000")
        };

    LOCK(cs_main);

    const auto hash = ParseHashStr(request.params[0].get_str(), "txid");
    const CAmount nAmount{request.params[2].get_int64()};

    if (!(request.params[1].isNull() || request.params[1].get_real() == 0)) {
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Priority is no longer supported, dummy argument to prioritisetransaction must be 0.");
    }

    mempool.PrioritiseTransaction(hash, nAmount);
    return true;
}


// NOTE: Assumes a conclusive result; if result is inconclusive, it must be handled by caller
static UniValue BIP22ValidationResult(const CValidationState& state)
{
    if (state.IsValid())
        return NullUniValue;

    const auto strRejectReason = state.GetRejectReason();
    if (state.IsError())
        throw JSONRPCError(RPCErrorCode::VERIFY_ERROR, strRejectReason);
    if (state.IsInvalid())
    {
        if (strRejectReason.empty())
            return "rejected";
        return strRejectReason;
    }
    // Should be impossible
    return "valid?";
}

static std::string gbt_vb_name(const Consensus::DeploymentPos pos) {
    const auto& vbinfo = VersionBitsDeploymentInfo[pos];
    std::string s{vbinfo.name};
    if (!vbinfo.gbt_force) {
        s.insert(s.begin(), '!');
    }
    return s;
}

UniValue getblocktemplate(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error{
            "getblocktemplate ( TemplateRequest )\n"
            "\nIf the request parameters include a 'mode' key, that is used to explicitly select between the default 'template' request or a 'proposal'.\n"
            "It returns data needed to construct a block to work on.\n"
            "For full specification, see BIPs 22, 23, 9, and 145:\n"
            "    https://github.com/bitcoin/bips/blob/master/bip-0022.mediawiki\n"
            "    https://github.com/bitcoin/bips/blob/master/bip-0023.mediawiki\n"
            "    https://github.com/bitcoin/bips/blob/master/bip-0009.mediawiki#getblocktemplate_changes\n"
            "    https://github.com/bitcoin/bips/blob/master/bip-0145.mediawiki\n"

            "\nArguments:\n"
            "1. template_request         (json object, optional) A json object in the following spec\n"
            "     {\n"
            "       \"mode\":\"template\"    (string, optional) This must be set to \"template\", \"proposal\" (see BIP 23), or omitted\n"
            "       \"capabilities\":[     (array, optional) A list of strings\n"
            "           \"support\"          (string) client side supported feature, 'longpoll', 'coinbasetxn', 'coinbasevalue', 'proposal', 'serverlist', 'workid'\n"
            "           ,...\n"
            "       ],\n"
            "       \"rules\":[            (array, optional) A list of strings\n"
            "           \"support\"          (string) client side supported softfork deployment\n"
            "           ,...\n"
            "       ]\n"
            "     }\n"
            "\n"

            "\nResult:\n"
            "{\n"
            "  \"version\" : n,                    (numeric) The preferred block version\n"
            "  \"rules\" : [ \"rulename\", ... ],    (array of strings) specific block rules that are to be enforced\n"
            "  \"vbavailable\" : {                 (json object) set of pending, supported versionbit (BIP 9) softfork deployments\n"
            "      \"rulename\" : bitnumber          (numeric) identifies the bit number as indicating acceptance and readiness for the named softfork rule\n"
            "      ,...\n"
            "  },\n"
            "  \"vbrequired\" : n,                 (numeric) bit mask of versionbits the server requires set in submissions\n"
            "  \"previousblockhash\" : \"xxxx\",     (string) The hash of current highest block\n"
            "  \"transactions\" : [                (array) contents of non-coinbase transactions that should be included in the next block\n"
            "      {\n"
            "         \"data\" : \"xxxx\",             (string) transaction data encoded in hexadecimal (byte-for-byte)\n"
            "         \"txid\" : \"xxxx\",             (string) transaction id encoded in little-endian hexadecimal\n"
            "         \"hash\" : \"xxxx\",             (string) hash encoded in little-endian hexadecimal (including witness data)\n"
            "         \"depends\" : [                (array) array of numbers \n"
            "             n                          (numeric) transactions before this one (by 1-based index in 'transactions' list) that must be present in the final block if this one is\n"
            "             ,...\n"
            "         ],\n"
            "         \"fee\": n,                    (numeric) difference in value between transaction inputs and outputs (in satoshis); for coinbase transactions, this is a negative Number of the total collected block fees (ie, not including the block subsidy); if key is not present, fee is unknown and clients MUST NOT assume there isn't one\n"
            "         \"sigops\" : n,                (numeric) total SigOps cost, as counted for purposes of block limits; if key is not present, sigop cost is unknown and clients MUST NOT assume it is zero\n"
            "         \"weight\" : n,                (numeric) total transaction weight, as counted for purposes of block limits\n"
            "         \"required\" : true|false      (boolean) if provided and true, this transaction must be in the final block\n"
            "      }\n"
            "      ,...\n"
            "  ],\n"
            "  \"coinbaseaux\" : {                 (json object) data that should be included in the coinbase's scriptSig content\n"
            "      \"flags\" : \"xx\"                  (string) key name is to be ignored, and value included in scriptSig\n"
            "  },\n"
            "  \"coinbasevalue\" : n,              (numeric) maximum allowable input to coinbase transaction, including the generation award and transaction fees (in satoshis)\n"
            "  \"coinbasetxn\" : { ... },          (json object) information for coinbase transaction\n"
            "  \"target\" : \"xxxx\",                (string) The hash target\n"
            "  \"mintime\" : xxx,                  (numeric) The minimum timestamp appropriate for next block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"mutable\" : [                     (array of string) list of ways the block template may be changed \n"
            "     \"value\"                          (string) A way the block template may be changed, e.g. 'time', 'transactions', 'prevblock'\n"
            "     ,...\n"
            "  ],\n"
            "  \"noncerange\" : \"00000000ffffffff\",(string) A range of valid nonces\n"
            "  \"sigoplimit\" : n,                 (numeric) limit of sigops in blocks\n"
            "  \"sizelimit\" : n,                  (numeric) limit of block size\n"
            "  \"weightlimit\" : n,                (numeric) limit of block weight\n"
            "  \"curtime\" : ttt,                  (numeric) current timestamp in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"bits\" : \"xxxxxxxx\",              (string) compressed target of next block\n"
            "  \"height\" : n                      (numeric) The height of the next block\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("getblocktemplate", "")
            + HelpExampleRpc("getblocktemplate", "")
        };

    LOCK(cs_main);

    std::string strMode{"template"};
    UniValue lpval{NullUniValue};
    std::set<std::string> setClientRules;
    int64_t nMaxVersionPreVB{-1};
    if (!request.params[0].isNull())
    {
        const auto& oparam = request.params[0].get_obj();
        const auto& modeval = find_value(oparam, "mode");
        if (modeval.isStr())
            strMode = modeval.get_str();
        else if (modeval.isNull())
        {
            /* Do nothing */
        }
        else
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid mode");
        lpval = find_value(oparam, "longpollid");

        if (strMode == "proposal")
        {
            const auto& dataval = find_value(oparam, "data");
            if (!dataval.isStr())
                throw JSONRPCError(RPCErrorCode::TYPE_ERROR, "Missing data String key for proposal");

            CBlock block;
            if (!DecodeHexBlk(block, dataval.get_str()))
                throw JSONRPCError(RPCErrorCode::DESERIALIZATION_ERROR, "Block decode failed");

            const auto hash = block.GetHash();
            auto mi = mapBlockIndex.find(hash);
            if (mi != mapBlockIndex.end()) {
                auto pindex = mi->second;
                if (pindex->IsValid(BLOCK_VALID_SCRIPTS))
                    return "duplicate";
                if (pindex->nStatus & BLOCK_FAILED_MASK)
                    return "duplicate-invalid";
                return "duplicate-inconclusive";
            }

            auto* const pindexPrev = chainActive.Tip();
            // TestBlockValidity only supports blocks built on the current Tip
            if (block.hashPrevBlock != pindexPrev->GetBlockHash())
                return "inconclusive-not-best-prevblk";
            CValidationState state;
            TestBlockValidity(state, Params(), block, pindexPrev, false, true, false);
            return BIP22ValidationResult(state);
        }

        const auto& aClientRules = find_value(oparam, "rules");
        if (aClientRules.isArray()) {
            for (size_t i{0}; i < aClientRules.size(); ++i) {
                const auto& v = aClientRules[i];
                setClientRules.insert(v.get_str());
            }
        } else {
            // NOTE: It is important that this NOT be read if versionbits is supported
            const auto& uvMaxVersion = find_value(oparam, "maxversion");
            if (uvMaxVersion.isNum()) {
                nMaxVersionPreVB = uvMaxVersion.get_int64();
            }
        }
    }

    if (strMode != "template")
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid mode");

    if(!g_connman)
        throw JSONRPCError(RPCErrorCode::CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");

    if (g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL) == 0)
        throw JSONRPCError(RPCErrorCode::CLIENT_NOT_CONNECTED, "PAI Coin is not connected!");

    if (IsInitialBlockDownload())
        throw JSONRPCError(RPCErrorCode::CLIENT_IN_INITIAL_DOWNLOAD, "PAI Coin is downloading blocks...");

    static unsigned int nTransactionsUpdatedLast;

    if (!lpval.isNull())
    {
        // Wait to respond until either the best block changes, OR a minute has passed and there are more transactions
        uint256 hashWatchedChain;
        boost::system_time checktxtime;
        unsigned int nTransactionsUpdatedLastLP;

        if (lpval.isStr())
        {
            // Format: <hashBestChain><nTransactionsUpdatedLast>
            const auto& lpstr = lpval.get_str();

            hashWatchedChain.SetHex(lpstr.substr(0, 64));
            nTransactionsUpdatedLastLP = atoi64(lpstr.substr(64));
        }
        else
        {
            // NOTE: Spec does not specify behaviour for non-string longpollid, but this makes testing easier
            hashWatchedChain = chainActive.Tip()->GetBlockHash();
            nTransactionsUpdatedLastLP = nTransactionsUpdatedLast;
        }

        // Release the wallet and main lock while waiting
        LEAVE_CRITICAL_SECTION(cs_main);
        {
            checktxtime = boost::get_system_time() + boost::posix_time::minutes{1};

            boost::unique_lock<boost::mutex> lock{csBestBlock};
            while (chainActive.Tip()->GetBlockHash() == hashWatchedChain && IsRPCRunning())
            {
                if (!cvBlockChange.timed_wait(lock, checktxtime))
                {
                    // Timeout: Check transactions for update
                    if (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLastLP)
                        break;
                    checktxtime += boost::posix_time::seconds{10};
                }
            }
        }
        ENTER_CRITICAL_SECTION(cs_main, true);

        if (!IsRPCRunning())
            throw JSONRPCError(RPCErrorCode::CLIENT_NOT_CONNECTED, "Shutting down");
        // TODO: Maybe recheck connections/IBD and (if something wrong) send an expires-immediately template to stop miners?
    }

    const auto& segwit_info = VersionBitsDeploymentInfo[Consensus::DEPLOYMENT_SEGWIT];
    // If the caller is indicating segwit support, then allow CreateNewBlock()
    // to select witness transactions, after segwit activates (otherwise
    // don't).
    const auto fSupportsSegwit = setClientRules.find(segwit_info.name) != setClientRules.end();

    const auto nBlockAllVotesWaitTime = gArgs.GetArg("-blockallvoteswaittime", DEFAULT_BLOCK_ALL_VOTES_WAIT_TIME);

    // Update block
    static CBlockIndex* pindexPrev;
    static int64_t nStart;
    static std::unique_ptr<CBlockTemplate> pblocktemplate;
    // Cache whether the last invocation was with segwit support, to avoid returning
    // a segwit-block to a non-segwit caller.
    static bool fLastTemplateSupportsSegwit = true;
    const auto nTimeElapsed = GetTime() - nStart;
    const Consensus::Params& consensusParams = Params().GetConsensus();
    auto bTooFewVotes = false;

    if (pindexPrev != chainActive.Tip() ||
        (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && nTimeElapsed > 5) ||
        fLastTemplateSupportsSegwit != fSupportsSegwit)
    {
        // Clear pindexPrev so future calls make a new block, despite any failures from here on
        pindexPrev = nullptr;

        // Store the pindexBest used before CreateNewBlock, to avoid races
        nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();
        CBlockIndex* pindexPrevNew = nullptr;
        fLastTemplateSupportsSegwit = fSupportsSegwit;

        const int& tipHeight = chainActive.Tip()->nHeight;
        auto maxVotes = 0;
        const auto& majority = (consensusParams.nTicketsPerBlock / 2) + 1;
        if (tipHeight >= Params().GetConsensus().nStakeValidationHeight - 1) {
            CBlockIndex* selectedIndex = nullptr;
            const auto& setTips = GetChainTips();
            if (setTips.size() == 0)
                throw JSONRPCError(RPCErrorCode::DATABASE_ERROR, "Blockchain has no tips!");

            LOCK(mempool.cs);
            for (auto pBlockIndex : setTips) {
                if (pBlockIndex->nStatus & BLOCK_FAILED_MASK)
                    continue;

                if (!(pBlockIndex->nStatus & BLOCK_HAVE_DATA))
                    continue;

                if (pBlockIndex->nHeight < tipHeight)
                    continue;

                const uint256& blockHash = pBlockIndex->GetBlockHash();
                if (blockHash == uint256())
                    continue;

                auto& voted_hash_index = mempool.mapTx.get<voted_block_hash>();
                auto votesForBlockHash = voted_hash_index.equal_range(pBlockIndex->GetBlockHash());

                //assert (pBlockIndex->pstakeNode != nullptr);
                if (pBlockIndex->pstakeNode == nullptr)
                    throw JSONRPCError(RPCErrorCode::DATABASE_ERROR, "Tip doesn't have a stake node!");

                auto winningHashes = pBlockIndex->pstakeNode->Winners();

                int nNewVotes = 0;
                for (auto votetxiter = votesForBlockHash.first; votetxiter != votesForBlockHash.second; ++votetxiter) {
                    const auto& spentTicketHash = votetxiter->GetTx().vin[voteStakeInputIndex].prevout.hash;
                    if (std::find(winningHashes.begin(), winningHashes.end(), spentTicketHash) == winningHashes.end())
                        continue; //not a winner

                    ++nNewVotes;
                }

                // always select the highest tip with enought votes to make majority,
                // regardless if it doesn't really have to most votes
                if (selectedIndex != nullptr && maxVotes >= majority && pBlockIndex->nHeight < selectedIndex->nHeight)
                    continue;

                if (nNewVotes > maxVotes) {
                    selectedIndex = pBlockIndex;
                    maxVotes = nNewVotes;
                }
            }
            if (maxVotes >= majority) {
                // Create new block based on selected tip
                pindexPrevNew = selectedIndex;
            } else {
                // too-few-votes: none of the tips have enough to be mined, we will remine on top of previous
                bTooFewVotes = true;
            }
        } else {
            pindexPrevNew = chainActive.Tip();
        }

        if (bTooFewVotes) {
            // nothing to do, we'll use the cached template, but update its time, otherwise we'll mine the exact same block
            // which leads to 'duplicate' status in submitblock
            if (!pblocktemplate) {
                CValidationState state;
                // we have no cached template so we try to build one
                // disconnect the tip briefly to return its content txs to mempool, so they will be included in the template
                // then activate the tip back
                if (!DisconnectTipForRemine(state, Params(), chainActive.Tip()))
                    throw JSONRPCError(RPCErrorCode::OUT_OF_MEMORY, "Unable to disconnect current tip");

                pindexPrevNew = chainActive.Tip();

                //assert(pindexPrevNew != nullptr && (pindexPrevNew->nHeight == tipHeight - 1));
                if (pindexPrevNew == nullptr)
                    throw JSONRPCError(RPCErrorCode::OUT_OF_MEMORY, "Missing active chain tip!");
                if (pindexPrevNew->nHeight != tipHeight - 1)
                    throw JSONRPCError(RPCErrorCode::DATABASE_ERROR, "According to its height, the next block is not the successor of the active chain tip!");

                nStart = GetTime(); // reinitialize Start
                CScript scriptDummy = CScript() << OP_TRUE;
                pblocktemplate = BlockAssembler(Params()).CreateNewBlock(scriptDummy, fSupportsSegwit, pindexPrevNew);

                ActivateBestChain(state, Params());
                if (!state.IsValid()) {
                    throw JSONRPCError(RPCErrorCode::DATABASE_ERROR, state.GetRejectReason());
                }
            } else {
                // find the index of the current template
                auto mi = mapBlockIndex.find(pblocktemplate->block.hashPrevBlock);
                if (mi == mapBlockIndex.end())
                    throw JSONRPCError(RPCErrorCode::DATABASE_ERROR, "Template block index not found!");

                pindexPrevNew = mi->second;

                pblocktemplate->block.nTime++;
                LogPrintf("returning cached template with adjusted time %d!\n", pblocktemplate->block.nTime);
            }
        } else {
            // Create new block

            //assert(pindexPrevNew != nullptr && (pindexPrevNew->nHeight == tipHeight));
            if (pindexPrevNew == nullptr)
                throw JSONRPCError(RPCErrorCode::OUT_OF_MEMORY, "Previous block index is not specified!");

            nStart = GetTime(); // reinitialize Start
            CScript scriptDummy = CScript() << OP_TRUE;
            pblocktemplate = BlockAssembler(Params()).CreateNewBlock(scriptDummy, fSupportsSegwit, pindexPrevNew);
        }

        if (!pblocktemplate)
            throw JSONRPCError(RPCErrorCode::OUT_OF_MEMORY, "Out of memory");

        // we have enough votes, but some vote transactions may still be in transit,
        // return error while waiting for more votes in the specified time
        if ( !bTooFewVotes
           && pindexPrevNew->nHeight + 1 >= Params().GetConsensus().nStakeValidationHeight
           && pblocktemplate->block.nVoters < Params().GetConsensus().nTicketsPerBlock
           && nTimeElapsed < nBlockAllVotesWaitTime) {
            throw JSONRPCError(RPCErrorCode::VERIFY_ERROR, "Some vote transactions are not yet available, waiting <blockallvoteswaittime>");
        }

        // Need to update only after we know CreateNewBlock succeeded
        pindexPrev = pindexPrevNew;
    }

    auto pblock = &pblocktemplate->block; // pointer for convenience

    // Update nTime
    UpdateTime(pblock, consensusParams, pindexPrev);
    LogPrintf("UpdateTime %d!\n", pblock->nTime);
    pblock->nNonce = 0;

    // NOTE: If at some point we support pre-segwit miners post-segwit-activation, this needs to take segwit support into consideration
    const auto fPreSegWit = (THRESHOLD_ACTIVE != VersionBitsState(pindexPrev, consensusParams, Consensus::DEPLOYMENT_SEGWIT, versionbitscache));

    UniValue aCaps{UniValue::VARR};
    aCaps.push_back("proposal");

    UniValue transactions{UniValue::VARR};
    std::map<uint256, int64_t> setTxIndex;
    int i = 0;
    for (const auto& it : pblock->vtx) {
        const auto& tx = *it;
        const auto txHash = tx.GetHash();
        setTxIndex[txHash] = i++;

        if (tx.IsCoinBase())
            continue;

        UniValue entry{UniValue::VOBJ};

        entry.push_back(Pair("data", EncodeHexTx(tx)));
        entry.push_back(Pair("txid", txHash.GetHex()));
        entry.push_back(Pair("hash", tx.GetWitnessHash().GetHex()));

        UniValue deps{UniValue::VARR};
        for (const auto &in : tx.vin)
        {
            if (setTxIndex.count(in.prevout.hash))
                deps.push_back(setTxIndex[in.prevout.hash]);
        }
        entry.push_back(Pair("depends", deps));

        const int index_in_template{i - 1};
        entry.push_back(Pair("fee", pblocktemplate->vTxFees[index_in_template]));
        int64_t nTxSigOps{pblocktemplate->vTxSigOpsCost[index_in_template]};
        if (fPreSegWit) {
            assert(nTxSigOps % WITNESS_SCALE_FACTOR == 0);
            nTxSigOps /= WITNESS_SCALE_FACTOR;
        }
        entry.push_back(Pair("sigops", nTxSigOps));
        entry.push_back(Pair("weight", GetTransactionWeight(tx)));

        transactions.push_back(entry);
    }

    UniValue aux{UniValue::VOBJ};
    aux.push_back(Pair("flags", HexStr(COINBASE_FLAGS.begin(), COINBASE_FLAGS.end())));

    const auto hashTarget = arith_uint256{}.SetCompact(pblock->nBits);

    UniValue aMutable{UniValue::VARR};
    aMutable.push_back("time");
    aMutable.push_back("transactions");
    aMutable.push_back("prevblock");

    UniValue result{UniValue::VOBJ};
    result.push_back(Pair("capabilities", aCaps));

    UniValue aRules{UniValue::VARR};
    UniValue vbavailable{UniValue::VOBJ};
    for (int j{0}; j < static_cast<int>(Consensus::MAX_VERSION_BITS_DEPLOYMENTS); ++j) {
        const auto pos = static_cast<Consensus::DeploymentPos>(j);
        const auto state = VersionBitsState(pindexPrev, consensusParams, pos, versionbitscache);
        switch (state) {
            case THRESHOLD_DEFINED:
            case THRESHOLD_FAILED:
                // Not exposed to GBT at all
                break;
            case THRESHOLD_LOCKED_IN:
                // Ensure bit is set in block version
                pblock->nVersion |= VersionBitsMask(consensusParams, pos);
                // FALL THROUGH to get vbavailable set...
            case THRESHOLD_STARTED:
            {
                const auto& vbinfo = VersionBitsDeploymentInfo[pos];
                vbavailable.push_back(Pair(gbt_vb_name(pos), consensusParams.vDeployments[pos].bit));
                if (setClientRules.find(vbinfo.name) == setClientRules.end()) {
                    if (!vbinfo.gbt_force) {
                        // If the client doesn't support this, don't indicate it in the [default] version
                        pblock->nVersion &= ~VersionBitsMask(consensusParams, pos);
                    }
                }
                break;
            }
            case THRESHOLD_ACTIVE:
            {
                // Add to rules only
                const auto& vbinfo = VersionBitsDeploymentInfo[pos];
                aRules.push_back(gbt_vb_name(pos));
                if (setClientRules.find(vbinfo.name) == setClientRules.end()) {
                    // Not supported by the client; make sure it's safe to proceed
                    if (!vbinfo.gbt_force) {
                        // If we do anything other than throw an exception here, be sure version/force isn't sent to old clients
                        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, strprintf("Support for '%s' rule requires explicit client support", vbinfo.name));
                    }
                }
                break;
            }
        }
    }
    result.push_back(Pair("version", pblock->nVersion));
    result.push_back(Pair("rules", aRules));
    result.push_back(Pair("vbavailable", vbavailable));
    result.push_back(Pair("vbrequired", 0));

    if (nMaxVersionPreVB >= 2) {
        // If VB is supported by the client, nMaxVersionPreVB is -1, so we won't get here
        // Because BIP 34 changed how the generation transaction is serialized, we can only use version/force back to v2 blocks
        // This is safe to do [otherwise-]unconditionally only because we are throwing an exception above if a non-force deployment gets activated
        // Note that this can probably also be removed entirely after the first BIP9 non-force deployment (ie, probably segwit) gets activated
        aMutable.push_back("version/force");
    }

    result.push_back(Pair("previousblockhash", pblock->hashPrevBlock.GetHex()));
    result.push_back(Pair("transactions", transactions));
    result.push_back(Pair("coinbaseaux", aux));
    result.push_back(Pair("coinbasevalue", static_cast<int64_t>(pblock->vtx[0]->vout[0].nValue)));
    result.push_back(Pair("longpollid", chainActive.Tip()->GetBlockHash().GetHex() + i64tostr(nTransactionsUpdatedLast)));
    result.push_back(Pair("target", hashTarget.GetHex()));
    result.push_back(Pair("mintime", static_cast<int64_t>(pindexPrev->GetMedianTimePast()+1)));
    result.push_back(Pair("mutable", aMutable));
    result.push_back(Pair("noncerange", "00000000ffffffff"));
    int64_t nSigOpLimit{MAX_BLOCK_SIGOPS_COST};
    int64_t nSizeLimit{MAX_BLOCK_SERIALIZED_SIZE};
    if (fPreSegWit) {
        assert(nSigOpLimit % WITNESS_SCALE_FACTOR == 0);
        nSigOpLimit /= WITNESS_SCALE_FACTOR;
        assert(nSizeLimit % WITNESS_SCALE_FACTOR == 0);
        nSizeLimit /= WITNESS_SCALE_FACTOR;
    }
    result.push_back(Pair("sigoplimit", nSigOpLimit));
    result.push_back(Pair("sizelimit", nSizeLimit));
    if (!fPreSegWit) {
        result.push_back(Pair("weightlimit", static_cast<int64_t>(MAX_BLOCK_WEIGHT)));
    }
    result.push_back(Pair("curtime", pblock->GetBlockTime()));
    result.push_back(Pair("bits", strprintf("%08x", pblock->nBits)));
    result.push_back(Pair("height", static_cast<int64_t>((pindexPrev->nHeight+1))));

    if (IsHybridConsensusForkEnabled(pindexPrev, Params().GetConsensus())) {
        // large values are not correctly interpreted by the miner, thus we use hex strings where needed
        result.push_back(Pair("stakedifficulty", strprintf("%016x", pblock->nStakeDifficulty)));
        result.push_back(Pair("votebits", pblock->nVoteBits.getBits()));
        result.push_back(Pair("ticketpoolsize", static_cast<int64_t>(pblock->nTicketPoolSize)));
        result.push_back(Pair("ticketlotterystate", StakeStateToString(pblock->ticketLotteryState)));
        result.push_back(Pair("voters", pblock->nVoters));
        result.push_back(Pair("freshstake", pblock->nFreshStake));
        result.push_back(Pair("revocations", pblock->nRevocations));

        std::string extraData;
        for (auto& byte: pblock->extraData)
            extraData += strprintf("%02x", byte);
        result.push_back(Pair("extradata", extraData));

        result.push_back(Pair("stakeversion", static_cast<int64_t>(pblock->nStakeVersion)));
    }

    if (!pblocktemplate->vchCoinbaseCommitment.empty() && fSupportsSegwit) {
        result.push_back(Pair("default_witness_commitment", HexStr(pblocktemplate->vchCoinbaseCommitment)));
    }

    return result;
}

class submitblock_StateCatcher : public CValidationInterface
{
public:
    uint256 hash;
    bool found;
    CValidationState state;

    explicit submitblock_StateCatcher(const uint256 &hashIn) : hash{hashIn}, found{false} {}

protected:
    void BlockChecked(const CBlock& block, const CValidationState& stateIn) override {
        if (block.GetHash() != hash)
            return;
        found = true;
        state = stateIn;
    }
};

UniValue submitblock(const JSONRPCRequest& request)
{
    // We allow 2 arguments for compliance with BIP22. Argument 2 is ignored.
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2) {
        throw std::runtime_error{
            "submitblock \"hexdata\"  ( \"dummy\" )\n"
            "\nAttempts to submit new block to network.\n"
            "See https://en.paicoin.it/wiki/BIP_0022 for full specification.\n"

            "\nArguments:\n"
            "1. \"hexdata\"        (string, required) the hex-encoded block data to submit\n"
            "2. \"dummy\"          (optional) dummy value, for compatibility with BIP22. This value is ignored.\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("submitblock", "\"mydata\"")
            + HelpExampleRpc("submitblock", "\"mydata\"")
        };
    }

    const auto blockptr = std::make_shared<CBlock>();
    if (!DecodeHexBlk(*blockptr, request.params[0].get_str())) {
        throw JSONRPCError(RPCErrorCode::DESERIALIZATION_ERROR, "Block decode failed");
    }

    if (blockptr->vtx.empty() || !blockptr->vtx[0]->IsCoinBase()) {
        throw JSONRPCError(RPCErrorCode::DESERIALIZATION_ERROR, "Block does not start with a coinbase");
    }

    const auto hash = blockptr->GetHash();
    auto fBlockPresent = false;
    {
        LOCK(cs_main);
        auto mi = mapBlockIndex.find(hash);
        LogPrintf("submitted block hash %s\n", hash.GetHex());
        if (mi != mapBlockIndex.end()) {
            const auto* const pindex = mi->second;
            if (pindex->IsValid(BLOCK_VALID_SCRIPTS)) {
                return "duplicate";
            }
            if (pindex->nStatus & BLOCK_FAILED_MASK) {
                return "duplicate-invalid";
            }
            // Otherwise, we might only have the header - process the block before returning
            fBlockPresent = true;
        }
    }

    {
        LOCK(cs_main);
        auto mi = mapBlockIndex.find(blockptr->hashPrevBlock);
        if (mi != mapBlockIndex.end()) {
            UpdateUncommittedBlockStructures(*blockptr, mi->second, Params().GetConsensus());
        }
    }

    submitblock_StateCatcher sc(blockptr->GetHash());
    RegisterValidationInterface(&sc);
    const auto fAccepted = ProcessNewBlock(Params(), blockptr, true, nullptr);
    UnregisterValidationInterface(&sc);
    if (fBlockPresent) {
        if (fAccepted && !sc.found) {
            return "duplicate-inconclusive";
        }
        return "duplicate";
    }
    if (!sc.found) {
        return "inconclusive";
    }
    return BIP22ValidationResult(sc.state);
}

UniValue existsexpiredtickets(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1) 
        throw std::runtime_error{
            "existsexpiredtickets \"txhashes\"\n"
            "\nTest for the existence of the provided tickets in the expired ticket map.\n"
            "\nArguments:\n"
            "1. \"txhashes\"       (array, required)   Array of hashes to check\n"
            "\nResult:\n"
            "{                   (dictionary)        Dictionary having txhashes as keys, showing if ticket exists in the expired ticket database or not\n"
            "  \"txhash\": true|false,\n"
            "  ...\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("existsexpiredtickets", "[\"txhash1\", \"txhash2\"]")
            + HelpExampleRpc("existsexpiredtickets", "[\"txhash1\", \"txhash2\"]")
        };

    std::vector<uint256> vTxids;
    UniValue txids = request.params[0].get_array();
    for (unsigned int idx = 0; idx < txids.size(); idx++) {
        const UniValue& txid = txids[idx];
        const auto& txhash = ParseHashStr(txid.get_str(), "txhash");
        vTxids.push_back(txhash);
    }

    auto result = UniValue{UniValue::VOBJ};

    LOCK(cs_main);

    CBlockIndex* pblockindex = chainActive.Tip();

    for (unsigned int idx = 0; idx < vTxids.size(); idx++) {
        const auto& tx = vTxids[idx];
        const auto& exists = pblockindex->pstakeNode->ExistsExpiredTicket(tx);
        result.push_back(Pair(tx.GetHex(), exists));
    }

    return result;
}

UniValue existsliveticket(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1) 
        throw std::runtime_error{
            "existsliveticket \"txhash\"\n"
            "\nTest for the existence of the provided ticket.\n"
            "\nArguments:\n"
            "1. \"txhash\"  (string, required)  The ticket hash to check\n"
            "\nResult:\n"
            "   true|false  (boolean)           Bool showing if address exists in the live ticket database or not\n"
            "\nExamples:\n"
            + HelpExampleCli("existsliveticket", "\"txhash\"")
            + HelpExampleRpc("existsliveticket", "\"txhash\"")
        };

    const auto& txhash = ParseHashStr(request.params[0].get_str(), "txhash");

    LOCK(cs_main);
    CBlockIndex* pblockindex = chainActive.Tip();
    const auto& exists = pblockindex->pstakeNode->ExistsLiveTicket(txhash);
    return UniValue(exists);
}

UniValue existslivetickets(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1) 
        throw std::runtime_error{
            "existslivetickets \"txhashes\"\n"
            "\nTest for the existence of the provided tickets in the live ticket map.\n"
            "\nArguments:\n"
            "1. \"txhashes\"       (array, required)   Array of hashes to check\n"
            "\nResult:\n"
            "{                   (dictionary)        Dictionary having txhashes as keys, showing if ticket exists in the live ticket database or not\n"
            "  \"txhash\": true|false,\n"
            "  ...\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("existslivetickets", "[\"txhash1\", \"txhash2\"]")
            + HelpExampleRpc("existslivetickets", "[\"txhash1\", \"txhash2\"]")
        };

    std::vector<uint256> vTxids;
    UniValue txids = request.params[0].get_array();
    for (unsigned int idx = 0; idx < txids.size(); idx++) {
        const UniValue& txid = txids[idx];
        const auto& txhash = ParseHashStr(txid.get_str(), "txhash");
        vTxids.push_back(txhash);
    }

    auto result = UniValue{UniValue::VOBJ};

    LOCK(cs_main);

    CBlockIndex* pblockindex = chainActive.Tip();

    for (unsigned int idx = 0; idx < vTxids.size(); idx++) {
        const auto& tx = vTxids[idx];
        const auto& exists = pblockindex->pstakeNode->ExistsLiveTicket(tx);
        result.push_back(Pair(tx.GetHex(), exists));
    }

    return result;
}

UniValue existsmissedtickets(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1) 
        throw std::runtime_error{
            "existsmissedtickets \"txhashes\"\n"
            "\nTest for the existence of the provided tickets in the missed ticket map.\n"
            "\nArguments:\n"
            "1. \"txhashes\"       (array, required)   Array of hashes to check\n"
            "\nResult:\n"
            "{                   (dictionary)        Dictionary having txhashes as keys, showing if ticket exists in the missed ticket database or not\n"
            "  \"txhash\": true|false,\n"
            "  ...\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("existsmissedtickets", "[\"txhash1\", \"txhash2\"]")
            + HelpExampleRpc("existsmissedtickets", "[\"txhash1\", \"txhash2\"]")
        };

    std::vector<uint256> vTxids;
    UniValue txids = request.params[0].get_array();
    for (unsigned int idx = 0; idx < txids.size(); idx++) {
        const UniValue& txid = txids[idx];
        const auto& txhash = ParseHashStr(txid.get_str(), "txhash");
        vTxids.push_back(txhash);
    }

    auto result = UniValue{UniValue::VOBJ};

    LOCK(cs_main);

    CBlockIndex* pblockindex = chainActive.Tip();

    for (unsigned int idx = 0; idx < vTxids.size(); idx++) {
        const auto& tx = vTxids[idx];
        const auto& exists = pblockindex->pstakeNode->ExistsMissedTicket(tx);
        result.push_back(Pair(tx.GetHex(), exists));
    }

    return result;
}

UniValue getticketpoolvalue(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0) 
        throw std::runtime_error{
            "getticketpoolvalue\n"
            "\nReturn the current value of all locked funds in the ticket pool.\n"
            "\nResult:\n"
            "   n.nnn (numeric) Total value of ticket pool\n"
            "\nExamples:\n"
            + HelpExampleCli("getticketpoolvalue", "")
            + HelpExampleRpc("getticketpoolvalue", "")
        };

    LOCK(cs_main);
    CBlockIndex* pblockindex = chainActive.Tip();
    const auto& liveTickets = pblockindex->pstakeNode->LiveTickets();
    auto sum = CAmount{};
    for (const auto& tx : liveTickets){
        const COutPoint out{tx, static_cast<uint32_t>(ticketStakeOutputIndex)};
        Coin coin;
        if (!pcoinsTip->GetCoin(out, coin)) {
            return NullUniValue;
        }
        sum += coin.out.nValue;
    }
    return ValueFromAmount(sum);
}

static int getTicketPurchaseHeight(const uint256& hashBlock)
{
    auto purchaseHeight = -1;
    BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
    if (mi != mapBlockIndex.end() && (*mi).second) {
        CBlockIndex* pindex = (*mi).second;
        if (chainActive.Contains(pindex))
            purchaseHeight = pindex->nHeight;
    }
    if (purchaseHeight < Params().GetConsensus().nHybridConsensusHeight)
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Error getting the block including the ticket purchase transaction");
    return purchaseHeight;
}

UniValue livetickets(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 2) 
        throw std::runtime_error{
            "livetickets ( verbose blockheight )\n"
            "\nReturns live ticket hashes from the ticket database\n"
            "\nArguments:\n"
            "1. verbose        (boolean, optional, default=false) Set true for additional information about tickets\n"
            "2. blockheight    (numeric, optional)                The height index, if not given the tip height is used\n"
            "\nResult:\n"
            "{\n"
            "   \"tickets\": [\"value\",...], (array of string) List of live tickets\n"
            "}\n" 
            "\nExamples:\n"
            + HelpExampleCli("livetickets", "")
            + HelpExampleRpc("livetickets", "")
        };

    auto fVerbose = false;
    if (!request.params[0].isNull())
        fVerbose = request.params[0].get_bool();

    auto nHeight = chainActive.Height();
    if (!request.params[1].isNull()) {
        nHeight = request.params[1].get_int();
        if (nHeight < 0 || nHeight > chainActive.Height())
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Block height out of range");
    }

    LOCK(cs_main);
    CBlockIndex* pblockindex = chainActive[nHeight];
    const auto& liveTickets = pblockindex->pstakeNode->LiveTickets();
    auto result = UniValue{UniValue::VOBJ};
    auto array = UniValue{UniValue::VARR};
    for (const auto& txhash : liveTickets){
        if (fVerbose) {
            CTransactionRef tx;
            uint256 hashBlock;
            if (!GetTransaction(txhash, tx, Params().GetConsensus(), hashBlock, true, false))
                throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "No such blockchain transaction");

            auto info = UniValue{UniValue::VOBJ};
            info.pushKV("txid", txhash.GetHex());
            StakingToUniv(*tx, info, false);

            const auto& purchaseHeight =  getTicketPurchaseHeight(hashBlock);
            info.push_back(Pair("purchase_height", purchaseHeight));

            array.push_back(info);
        } else {
            array.push_back(txhash.GetHex());
        }
    }
    result.push_back(Pair("tickets",array));
    return result;
}

UniValue winningtickets(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1) 
        throw std::runtime_error{
            "winningtickets ( blockheight )\n"
            "\nReturns winning ticket hashes from the chain tip's ticket database\n"
            "\nArguments:\n"
            "1. blockheight     (numeric, optional)     The height index, if not given the tip height is used\n"
            "\nResult:\n"
            "{\n"
            "   \"tickets\": [\"value\",...], (array of string) List of winning tickets\n"
            "}\n" 
            "\nExamples:\n"
            + HelpExampleCli("winningtickets", "")
            + HelpExampleRpc("winningtickets", "")
        };

    auto nHeight = chainActive.Height();
    if (!request.params[0].isNull()) {
        nHeight = request.params[0].get_int();
        if (nHeight < 0 || nHeight > chainActive.Height())
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Block height out of range");
    }

    LOCK(cs_main);

    const auto& setTips = GetChainTips();
    auto result = UniValue{UniValue::VARR};
    for (const auto& block : setTips) {
        if (block->pstakeNode == nullptr)
            continue;

        const int& blockHeight = block->nHeight;

        if (blockHeight < nHeight)
            continue;

        const uint256& blockHash = block->GetBlockHash();
        if (blockHash == uint256())
            continue;

        auto tip = UniValue{UniValue::VOBJ};
        tip.push_back(Pair("blockhash", blockHash.GetHex()));

        auto array = UniValue{UniValue::VARR};
        for (const uint256& ticketHash : block->pstakeNode->Winners()) {
            array.push_back(ticketHash.GetHex());
        }
        tip.push_back(Pair("tickets",array));
        result.push_back(tip);
    }

    return result;
}

UniValue missedtickets(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 2) 
        throw std::runtime_error{
            "missedtickets ( verbose blockheight )\n"
            "\nReturns missed ticket hashes from the ticket database\n"
            "\nArguments:\n"
            "1. verbose        (boolean, optional, default=false) Set true for additional information about tickets\n"
            "2. blockheight    (numeric, optional)                The height index, if not given the tip height is used\n"
            "\nResult:\n"
            "{\n"
            "   \"tickets\": [\"value\",...], (array of string) List of missed tickets\n"
            "}\n" 
            "\nExamples:\n"
            + HelpExampleCli("missedtickets", "")
            + HelpExampleRpc("missedtickets", "")
        };

    auto fVerbose = false;
    if (!request.params[0].isNull())
        fVerbose = request.params[0].get_bool();

    auto nHeight = chainActive.Height();
    if (!request.params[1].isNull()) {
        nHeight = request.params[1].get_int();
        if (nHeight < 0 || nHeight > chainActive.Height())
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Block height out of range");
    }

    LOCK(cs_main);
    CBlockIndex* pblockindex = chainActive[nHeight];
    const auto& missedTickets = pblockindex->pstakeNode->MissedTickets();
    auto result = UniValue{UniValue::VOBJ};
    auto array = UniValue{UniValue::VARR};
    for (const auto& txhash : missedTickets){
        if (fVerbose) {
            CTransactionRef tx;
            uint256 hashBlock;
            if (!GetTransaction(txhash, tx, Params().GetConsensus(), hashBlock, true, false))
                throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "No such blockchain transaction");

            auto info = UniValue{UniValue::VOBJ};
            info.pushKV("txid", txhash.GetHex());
            StakingToUniv(*tx, info, false);

            const auto& purchaseHeight =  getTicketPurchaseHeight(hashBlock);
            info.push_back(Pair("purchase_height", purchaseHeight));

            const auto& bExpired = pblockindex->pstakeNode->ExistsExpiredTicket(txhash);
            info.push_back(Pair("cause", bExpired ? "expiration" : "missed_vote"));
            if (!bExpired) {
                auto missedHeight = nHeight - 1;
                for (; missedHeight > purchaseHeight + Params().GetConsensus().nTicketMaturity 
                       && chainActive[missedHeight]->pstakeNode->ExistsMissedTicket(txhash); --missedHeight);
                info.push_back(Pair("missed_height", missedHeight + 1));
            } else {
                info.push_back(Pair("missed_height", nullptr));
            }

            array.push_back(info);
        } else {
            array.push_back(txhash.GetHex());
        }
    }
    result.push_back(Pair("tickets",array));
    return result;
}

UniValue ticketfeeinfo(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 2) 
        throw std::runtime_error{
            "ticketfeeinfo (blocks windows)\n"
            "\nGet various information about ticket fees from the mempool, blocks, and difficulty windows (units: PAI/kB)\n"
            "\nArguments:\n"
            "1. blocks  (numeric, optional) The number of blocks, starting from the chain tip and descending, to return fee information about\n"
            "2. windows (numeric, optional) The number of difficulty windows to return ticket fee information about\n"
            "\nResult:\n"
            "{\n"
            "   \"feeinfomempool\": {   (object)          Ticket fee information for all tickets in the mempool (units: PAI/kB)\n"
            "   \"number\": n,          (numeric)         Number of transactions in the mempool\n"
            "   \"min\": n.nnn,         (numeric)         Minimum transaction fee in the mempool\n"
            "   \"max\": n.nnn,         (numeric)         Maximum transaction fee in the mempool\n"
            "   \"mean\": n.nnn,        (numeric)         Mean of transaction fees in the mempool\n"
            "   \"median\": n.nnn,      (numeric)         Median of transaction fees in the mempool\n"
            "   \"stddev\": n.nnn,      (numeric)         Standard deviation of transaction fees in the mempool\n"
            "   },\n"
            "   \"feeinfoblocks\": [{   (array of object) Ticket fee information for a given list of blocks descending from the chain tip (units: PAI/kB)\n"
            "   \"height\": n,          (numeric)         Height of the block\n"
            "   \"number\": n,          (numeric)         Number of transactions in the block\n"
            "   \"min\": n.nnn,         (numeric)         Minimum transaction fee in the block\n"
            "   \"max\": n.nnn,         (numeric)         Maximum transaction fee in the block\n"
            "   \"mean\": n.nnn,        (numeric)         Mean of transaction fees in the block\n"
            "   \"median\": n.nnn,      (numeric)         Median of transaction fees in the block\n"
            "   \"stddev\": n.nnn,      (numeric)         Standard deviation of transaction fees in the block\n"
            "   },...],\n"
            "   \"feeinfowindows\": [{  (array of object) Ticket fee information for a window period where the stake difficulty was the same (units: PAI/kB)\n"
            "   \"startheight\": n,     (numeric)         First block in the window (inclusive)\n"
            "   \"endheight\": n,       (numeric)         Last block in the window (exclusive)\n"
            "   \"number\": n,          (numeric)         Number of transactions in the window\n"
            "   \"min\": n.nnn,         (numeric)         Minimum transaction fee in the window\n"
            "   \"max\": n.nnn,         (numeric)         Maximum transaction fee in the window\n"
            "   \"mean\": n.nnn,        (numeric)         Mean of transaction fees in the window\n"
            "   \"median\": n.nnn,      (numeric)         Median of transaction fees in the window\n"
            "   \"stddev\": n.nnn,      (numeric)         Standard deviation of transaction fees in the window\n"
            "   },...],\n"
            "}\n"  
            "\nExamples:\n"
            + HelpExampleCli("ticketfeeinfo", "5 3")
            + HelpExampleRpc("ticketfeeinfo", "5 3")
        };
    LOCK(cs_main);

    const auto& blocksTip = chainActive.Tip();
    const auto& currHeight = static_cast<uint32_t>(blocksTip->nHeight);

    auto blocks = uint32_t{0};
    auto windows = uint32_t{0};

    if (!request.params[0].isNull()) {
        blocks = request.params[0].get_int();
    }
    if (!request.params[1].isNull()) {
        windows = request.params[1].get_int();
    }

    if (blocks > currHeight) {
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid parameter for blocks");
    }

    // mempool fee rates
    auto mempool_feerates = std::vector<CAmount>{};

    {
        LOCK(mempool.cs);
        auto& tx_class_index = mempool.mapTx.get<tx_class>();
        auto existingTickets = tx_class_index.equal_range(ETxClass::TX_BuyTicket);
        for (auto tickettxiter = existingTickets.first; tickettxiter != existingTickets.second; ++tickettxiter) {
            auto txiter = mempool.mapTx.project<0>(tickettxiter);
            const auto& fee_rate = CFeeRate(txiter->GetModifiedFee(), txiter->GetTxSize());
            mempool_feerates.push_back(fee_rate.GetFeePerK());
        }
    }

    auto fee_info_mempool = FormatTxFeesInfo(mempool_feerates);

    // block fee rates
    auto fee_info_blocks = UniValue{UniValue::VARR};
    if (blocks > 0) {
        auto start = currHeight;
        auto end = currHeight - blocks;

        for (auto i = start; i > end; --i) {
            auto blockFee = ComputeBlocksTxFees(i, i + 1, ETxClass::TX_BuyTicket);
            blockFee.pushKV("height", static_cast<int32_t>(i));

            fee_info_blocks.push_back(blockFee);
        }
    }

    // window fee rates
    auto fee_info_windows = UniValue{UniValue::VARR};
    if (windows > 0) {
        // The first window is special because it may not be finished.
        // Perform this first and return if it's the only window the
        // user wants. Otherwise, append and continue.
        const auto& winLen = Params().GetConsensus().nStakeDiffWindowSize;
        const auto& lastChange = (currHeight / winLen) * winLen;

        auto windowFee = ComputeBlocksTxFees(lastChange, currHeight + 1, ETxClass::TX_BuyTicket);
        windowFee.pushKV("startheight", static_cast<int32_t>(lastChange));
        windowFee.pushKV("endheight", static_cast<int32_t>(currHeight + 1));
        fee_info_windows.push_back(windowFee);

        // We need data on windows from before this. Start from
        // the last adjustment and move backwards through window
        // lengths, calculating the fees data and appending it
        // each time.
        if (windows > 1) {
            // Go down to the last height requested, except
            // in the case that the user has specified to
            // many windows. In that case, just proceed to the
            // first block.
            auto end = int64_t{-1};
            if (lastChange - windows * winLen > end) {
                end = lastChange - windows*winLen;
            }
            for (auto i = lastChange; i > end+winLen; i -= winLen) {
                auto windowFee = ComputeBlocksTxFees(i-winLen, i, ETxClass::TX_BuyTicket);
                windowFee.pushKV("startheight", static_cast<int32_t>(i-winLen));
                windowFee.pushKV("endheight", static_cast<int32_t>(i));
                fee_info_windows.push_back(windowFee);
            }
        }
    }

    auto result = UniValue{UniValue::VOBJ};
    result.pushKV("feeinfomempool", fee_info_mempool);
    result.pushKV("feeinfoblocks", fee_info_blocks);
    result.pushKV("feeinfowindows", fee_info_windows);

    return result;
}

UniValue ticketsforaddress(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1) 
        throw std::runtime_error{
            "ticketsforaddress \"address\"\n"
            "\nRequest all the tickets for an address.\n"
            "\nArguments:\n"
            "1. address (string, required) Address to look for.\n"
            "\nResult:\n"
            "{\n"
            "   \"tickets\": [\"value\",...], (array of string) Tickets owned by the specified address.\n"
            "}\n" 
            "\nExamples:\n"
            + HelpExampleCli("ticketsforaddress", "\"address\"")
            + HelpExampleRpc("ticketsforaddress", "\"address\"")
        };

    const auto destination = DecodeDestination(request.params[0].get_str());
    if (!IsValidDestination(destination)) {
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Error: Invalid address");
    }

    auto result = UniValue{UniValue::VOBJ};
    auto array = UniValue{UniValue::VARR};

    LOCK(cs_main);

    CBlockIndex* pblockindex = chainActive.Tip();
    const auto& liveTickets = pblockindex->pstakeNode->LiveTickets();
    for (const auto& tx : liveTickets) {
        const auto& ticketTx = GetTicket(tx);
        const auto& script = ticketTx->vout[ticketStakeOutputIndex].scriptPubKey;
        CTxDestination addressRet;
        if (!ExtractDestination(script, addressRet)) {
            throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Error: Could not extract destination address");
        }

        if (destination == addressRet) {
            array.push_back(tx.GetHex());
        }
    }

    result.push_back(Pair("tickets",array));
    return result;
}

UniValue ticketvwap(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 2) 
        throw std::runtime_error{
            "ticketvwap (start end)\n"
            "\nCalculate the volume weighted average price of tickets for a range of blocks (default: full PoS difficulty adjustment depth)\n"
            "\nArguments:\n"
            "1. start (numeric, optional) The start height to begin calculating the VWAP from\n"
            "2. end   (numeric, optional) The end height to begin calculating the VWAP from\n"
            "\nResult:\n"
            "   n.nnn (numeric) The volume weighted average price\n"
            "\nExamples:\n"
            + HelpExampleCli("ticketvwap", "10 20")
            + HelpExampleRpc("ticketvwap", "10 20")
        };

    // The default VWAP is for the past WorkDiffWindows * WorkDiffWindowSize
    // many blocks.
    const auto& blocksTip  = chainActive.Tip();
    const auto& currHeight = static_cast<uint32_t>(blocksTip->nHeight);

    auto start = uint32_t{0};
    if (request.params[0].isNull()) {
        const auto& params =Params().GetConsensus();
        // In Decred they use params.WorkDiffWindows * params.WorkDiffWindowSize, we do not have these
        const auto& toEval = params.nStakeDiffWindows * params.nStakeDiffWindowSize;
        const auto& startI64 = currHeight - toEval;

        // Use 1 as the first block if there aren't enough blocks.
        if (startI64 <= 0) {
            start = 1;
        } else {
            start = startI64;
        }
    }
    else {
        start = request.params[0].get_int();
    }

    auto end = currHeight;
    if (!request.params[1].isNull()) {
        end = request.params[1].get_int();
    }

    if (start > end) {
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Error: Start height is beyond end height");
    }

    if (end > currHeight) {
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Error: End height is beyond blockchain tip height");
    }

    // Calculate the volume weighted average price of a ticket for the
    // given range.
    auto ticketNum  = int64_t{0};
    auto totalValue = int64_t{0};
    LOCK(cs_main);
    for (auto i = start; i <= end; ++i) {
        const auto& blockindex = chainActive[i];
        // Decred uses FreshStake = Number of new sstx in this block. We will use number of new tickets
        // Decred uses SBits = Stake difficulty target, we will use nStakeDifficulty
        ticketNum += blockindex->newTickets->size(); //int64(blockHeader.FreshStake)
        totalValue += blockindex->nStakeDifficulty * blockindex->newTickets->size(); //blockHeader.SBits * int64(blockHeader.FreshStake)
    }
    auto vwap = int64_t{0};
    if (ticketNum > 0) {
        vwap = totalValue / ticketNum;
    }

    return ValueFromAmount(CAmount(vwap));
}

UniValue removemempoolvotes(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error{
            "removemempoolvotes \"blockhash\"\n"
            "\nRemoves the votes for the specified block hash.\n"
            "\nArguments:\n"
            "1. \"blockhash\"        (string, required) the hex-encoded hash of block in votes to remove\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("removemempoolvotes", "\"00000000018151b673df2356e5e25bfcfecbcd7cf888717f2458530461512343\"")
            + HelpExampleRpc("removemempoolvotes", "\"00000000018151b673df2356e5e25bfcfecbcd7cf888717f2458530461512343\"")
        };
    }

    const auto hash = uint256S(request.params[0].get_str());

    mempool.removeVotesForBlock(hash, Params().GetConsensus());

    return NullUniValue;
}

UniValue removeallmempoolvotesexcept(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error{
            "removeallmempoolvotesexcept \"blockhash\"\n"
            "\nRemoves all the votes that are NOT for the specified block hash.\n"
            "\nWARNING! This might leave mempool without necessary votes for chain advance. Use with caution!\n"
            "\nArguments:\n"
            "1. \"blockhash\"        (string, required) the hex-encoded hash of block in votes to keep\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("removeallmempoolvotesexcept", "\"00000000018151b673df2356e5e25bfcfecbcd7cf888717f2458530461512343\"")
            + HelpExampleRpc("removeallmempoolvotesexcept", "\"00000000018151b673df2356e5e25bfcfecbcd7cf888717f2458530461512343\"")
        };
    }

    const auto hash = uint256S(request.params[0].get_str());

    mempool.removeAllVotesExceptForBlock(hash, Params().GetConsensus());

    return NullUniValue;
}

UniValue estimatefee(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error{
            "estimatefee nblocks\n"
            "\nDEPRECATED. Please use estimatesmartfee for more intelligent estimates."
            "\nEstimates the approximate fee per kilobyte needed for a transaction to begin\n"
            "confirmation within nblocks blocks. Uses virtual transaction size of transaction\n"
            "as defined in BIP 141 (witness data is discounted).\n"
            "\nArguments:\n"
            "1. nblocks     (numeric, required)\n"
            "\nResult:\n"
            "n              (numeric) estimated fee-per-kilobyte\n"
            "\n"
            "A negative value is returned if not enough transactions and blocks\n"
            "have been observed to make an estimate.\n"
            "-1 is always returned for nblocks == 1 as it is impossible to calculate\n"
            "a fee that is high enough to get reliably included in the next block.\n"
            "\nExample:\n"
            + HelpExampleCli("estimatefee", "6")
        };

    if (!IsDeprecatedRPCEnabled("estimatefee")) {
        throw JSONRPCError(RPCErrorCode::METHOD_DEPRECATED, "estimatefee is deprecated and will be fully removed in v0.17. "
            "To use estimatefee in v0.16, restart paicoind with -deprecatedrpc=estimatefee.\n"
            "Projects should transition to using estimatesmartfee before upgrading to v0.17");
    }

    RPCTypeCheck(request.params, {UniValue::VNUM});

    auto nBlocks = request.params[0].get_int();
    if (nBlocks < 1)
        nBlocks = 1;

    const auto feeRate = ::feeEstimator.estimateFee(nBlocks);
    if (feeRate == CFeeRate(0))
        return -1.0;

    return ValueFromAmount(feeRate.GetFeePerK());
}

UniValue estimatesmartfee(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error{
            "estimatesmartfee conf_target (\"estimate_mode\")\n"
            "\nEstimates the approximate fee per kilobyte needed for a transaction to begin\n"
            "confirmation within conf_target blocks if possible and return the number of blocks\n"
            "for which the estimate is valid. Uses virtual transaction size as defined\n"
            "in BIP 141 (witness data is discounted).\n"
            "\nArguments:\n"
            "1. conf_target     (numeric) Confirmation target in blocks (1 - 1008)\n"
            "2. \"estimate_mode\" (string, optional, default=CONSERVATIVE) The fee estimate mode.\n"
            "                   Whether to return a more conservative estimate which also satisfies\n"
            "                   a longer history. A conservative estimate potentially returns a\n"
            "                   higher feerate and is more likely to be sufficient for the desired\n"
            "                   target, but is not as responsive to short term drops in the\n"
            "                   prevailing fee market.  Must be one of:\n"
            "       \"UNSET\" (defaults to CONSERVATIVE)\n"
            "       \"ECONOMICAL\"\n"
            "       \"CONSERVATIVE\"\n"
            "\nResult:\n"
            "{\n"
            "  \"feerate\" : x.x,     (numeric, optional) estimate fee rate in " + CURRENCY_UNIT + "/kB\n"
            "  \"errors\": [ str... ] (json array of strings, optional) Errors encountered during processing\n"
            "  \"blocks\" : n         (numeric) block number where estimate was found\n"
            "}\n"
            "\n"
            "The request target will be clamped between 2 and the highest target\n"
            "fee estimation is able to return based on how long it has been running.\n"
            "An error is returned if not enough transactions and blocks\n"
            "have been observed to make an estimate for any number of blocks.\n"
            "\nExample:\n"
            + HelpExampleCli("estimatesmartfee", "6")
        };

    RPCTypeCheck(request.params, {UniValue::VNUM, UniValue::VSTR});
    RPCTypeCheckArgument(request.params[0], UniValue::VNUM);
    const auto conf_target = ParseConfirmTarget(request.params[0]);
    auto conservative = true;
    if (!request.params[1].isNull()) {
        FeeEstimateMode fee_mode;
        if (!FeeModeFromString(request.params[1].get_str(), fee_mode)) {
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid estimate_mode parameter");
        }
        if (fee_mode == FeeEstimateMode::ECONOMICAL) conservative = false;
    }

    UniValue result{UniValue::VOBJ};
    UniValue errors{UniValue::VARR};
    FeeCalculation feeCalc;
    const auto feeRate = ::feeEstimator.estimateSmartFee(conf_target, &feeCalc, conservative);
    if (feeRate != CFeeRate(0)) {
        result.push_back(Pair("feerate", ValueFromAmount(feeRate.GetFeePerK())));
    } else {
        errors.push_back("Insufficient data or no feerate found");
        result.push_back(Pair("errors", errors));
    }
    result.push_back(Pair("blocks", feeCalc.returnedTarget));
    return result;
}

UniValue estimaterawfee(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error{
            "estimaterawfee conf_target (threshold)\n"
            "\nWARNING: This interface is unstable and may disappear or change!\n"
            "\nWARNING: This is an advanced API call that is tightly coupled to the specific\n"
            "         implementation of fee estimation. The parameters it can be called with\n"
            "         and the results it returns will change if the internal implementation changes.\n"
            "\nEstimates the approximate fee per kilobyte needed for a transaction to begin\n"
            "confirmation within conf_target blocks if possible. Uses virtual transaction size as\n"
            "defined in BIP 141 (witness data is discounted).\n"
            "\nArguments:\n"
            "1. conf_target (numeric) Confirmation target in blocks (1 - 1008)\n"
            "2. threshold   (numeric, optional) The proportion of transactions in a given feerate range that must have been\n"
            "               confirmed within conf_target in order to consider those feerates as high enough and proceed to check\n"
            "               lower buckets.  Default: 0.95\n"
            "\nResult:\n"
            "{\n"
            "  \"short\" : {            (json object, optional) estimate for short time horizon\n"
            "      \"feerate\" : x.x,        (numeric, optional) estimate fee rate in " + CURRENCY_UNIT + "/kB\n"
            "      \"decay\" : x.x,          (numeric) exponential decay (per block) for historical moving average of confirmation data\n"
            "      \"scale\" : x,            (numeric) The resolution of confirmation targets at this time horizon\n"
            "      \"pass\" : {              (json object, optional) information about the lowest range of feerates to succeed in meeting the threshold\n"
            "          \"startrange\" : x.x,     (numeric) start of feerate range\n"
            "          \"endrange\" : x.x,       (numeric) end of feerate range\n"
            "          \"withintarget\" : x.x,   (numeric) number of txs over history horizon in the feerate range that were confirmed within target\n"
            "          \"totalconfirmed\" : x.x, (numeric) number of txs over history horizon in the feerate range that were confirmed at any point\n"
            "          \"inmempool\" : x.x,      (numeric) current number of txs in mempool in the feerate range unconfirmed for at least target blocks\n"
            "          \"leftmempool\" : x.x,    (numeric) number of txs over history horizon in the feerate range that left mempool unconfirmed after target\n"
            "      },\n"
            "      \"fail\" : { ... },       (json object, optional) information about the highest range of feerates to fail to meet the threshold\n"
            "      \"errors\":  [ str... ]   (json array of strings, optional) Errors encountered during processing\n"
            "  },\n"
            "  \"medium\" : { ... },    (json object, optional) estimate for medium time horizon\n"
            "  \"long\" : { ... }       (json object) estimate for long time horizon\n"
            "}\n"
            "\n"
            "Results are returned for any horizon which tracks blocks up to the confirmation target.\n"
            "\nExample:\n"
            + HelpExampleCli("estimaterawfee", "6 0.9")
        };

    RPCTypeCheck(request.params, {UniValue::VNUM, UniValue::VNUM}, true);
    RPCTypeCheckArgument(request.params[0], UniValue::VNUM);
    const auto conf_target = ParseConfirmTarget(request.params[0]);
    double threshold{0.95};
    if (!request.params[1].isNull()) {
        threshold = request.params[1].get_real();
    }
    if (threshold < 0 || threshold > 1) {
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid threshold");
    }

    UniValue result{UniValue::VOBJ};

    for (const auto horizon : {FeeEstimateHorizon::SHORT_HALFLIFE, FeeEstimateHorizon::MED_HALFLIFE, FeeEstimateHorizon::LONG_HALFLIFE}) {
        // Only output results for horizons which track the target
        if (conf_target > ::feeEstimator.HighestTargetTracked(horizon)) continue;

        EstimationResult buckets;
        const auto feeRate = ::feeEstimator.estimateRawFee(conf_target, threshold, horizon, &buckets);
        UniValue horizon_result{UniValue::VOBJ};
        UniValue errors{UniValue::VARR};
        UniValue passbucket{UniValue::VOBJ};
        passbucket.push_back(Pair("startrange", round(buckets.pass.start)));
        passbucket.push_back(Pair("endrange", round(buckets.pass.end)));
        passbucket.push_back(Pair("withintarget", round(buckets.pass.withinTarget * 100.0) / 100.0));
        passbucket.push_back(Pair("totalconfirmed", round(buckets.pass.totalConfirmed * 100.0) / 100.0));
        passbucket.push_back(Pair("inmempool", round(buckets.pass.inMempool * 100.0) / 100.0));
        passbucket.push_back(Pair("leftmempool", round(buckets.pass.leftMempool * 100.0) / 100.0));
        UniValue failbucket{UniValue::VOBJ};
        failbucket.push_back(Pair("startrange", round(buckets.fail.start)));
        failbucket.push_back(Pair("endrange", round(buckets.fail.end)));
        failbucket.push_back(Pair("withintarget", round(buckets.fail.withinTarget * 100.0) / 100.0));
        failbucket.push_back(Pair("totalconfirmed", round(buckets.fail.totalConfirmed * 100.0) / 100.0));
        failbucket.push_back(Pair("inmempool", round(buckets.fail.inMempool * 100.0) / 100.0));
        failbucket.push_back(Pair("leftmempool", round(buckets.fail.leftMempool * 100.0) / 100.0));

        // CFeeRate(0) is used to indicate error as a return value from estimateRawFee
        if (feeRate != CFeeRate{0}) {
            horizon_result.push_back(Pair("feerate", ValueFromAmount(feeRate.GetFeePerK())));
            horizon_result.push_back(Pair("decay", buckets.decay));
            horizon_result.push_back(Pair("scale", static_cast<int>(buckets.scale)));
            horizon_result.push_back(Pair("pass", passbucket));
            // buckets.fail.start == -1 indicates that all buckets passed, there is no fail bucket to output
            if (buckets.fail.start != -1) horizon_result.push_back(Pair("fail", failbucket));
        } else {
            // Output only information that is still meaningful in the event of error
            horizon_result.push_back(Pair("decay", buckets.decay));
            horizon_result.push_back(Pair("scale", static_cast<int>(buckets.scale)));
            horizon_result.push_back(Pair("fail", failbucket));
            errors.push_back("Insufficient data or no feerate found which meets threshold");
            horizon_result.push_back(Pair("errors", errors));
        }
        result.push_back(Pair(StringForFeeEstimateHorizon(horizon), horizon_result));
    }
    return result;
}

static const CRPCCommand commands[] =
{ //  category              name                            actor (function)                argNames
  //  --------------------- ------------------------------- ------------------------------- ----------
    { "mining",             "getnetworkhashps",             &getnetworkhashps,              {"nblocks","height"} },
    { "mining",             "getmininginfo",                &getmininginfo,                 {} },
    { "mining",             "prioritisetransaction",        &prioritisetransaction,         {"txid","dummy","fee_delta"} },
    { "mining",             "getblocktemplate",             &getblocktemplate,              {"template_request"} },
    { "mining",             "submitblock",                  &submitblock,                   {"hexdata","dummy"} },

    { "mining",             "existsexpiredtickets",         &existsexpiredtickets,          {"txhashes"} },
    { "mining",             "existsliveticket",             &existsliveticket,              {"txhash"} },
    { "mining",             "existsmissedtickets",          &existsmissedtickets,           {"txhashes"} },
    { "mining",             "existslivetickets",            &existslivetickets,             {"txhashes"} },
    { "mining",             "getticketpoolvalue",           &getticketpoolvalue,            {} },
    { "mining",             "livetickets",                  &livetickets,                   {"verbose", "blockheight"} },
    { "mining",             "winningtickets",               &winningtickets,                {"blockheight"} },
    { "mining",             "missedtickets",                &missedtickets,                 {"verbose", "blockheight"} },
    { "mining",             "ticketfeeinfo",                &ticketfeeinfo,                 {"blocks","windows"} },
    { "mining",             "ticketsforaddress",            &ticketsforaddress,             {"address"} },
    { "mining",             "ticketvwap",                   &ticketvwap,                    {"start","stop"} },
    { "mining",             "removemempoolvotes",           &removemempoolvotes,            {"blockhash"} },
    { "mining",             "removeallmempoolvotesexcept",  &removeallmempoolvotesexcept,   {"blockhash"} },

    { "generating",         "generatetoaddress",            &generatetoaddress,             {"nblocks","address","maxtries"} },

    { "util",               "estimatefee",                  &estimatefee,                   {"nblocks"} },
    { "util",               "estimatesmartfee",             &estimatesmartfee,              {"conf_target", "estimate_mode"} },

    { "hidden",             "estimaterawfee",               &estimaterawfee,                {"conf_target", "threshold"} },
};

void RegisterMiningRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx{0}; vcidx < ARRAYLEN(commands); ++vcidx)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
