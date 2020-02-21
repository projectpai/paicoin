// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpc/blockchain.h"

#include "chain.h"
#include "chainparams.h"
#include "checkpoints.h"
#include "clientversion.h"
#include "coins.h"
#include "config/paicoin-config.h"
#include "consensus/validation.h"
#include "validation.h"
#include "core_io.h"
#include "net.h"
#include "netbase.h"
#include "policy/feerate.h"
#include "policy/policy.h"
#include "primitives/transaction.h"
#include "rpc/server.h"
#include "streams.h"
#include "sync.h"
#include "timedata.h"
#include "txdb.h"
#include "txmempool.h"
#include "util.h"
#include "utilstrencodings.h"
#include "hash.h"
#include "warnings.h"

#include <numeric>
#include <stdint.h>

#include <univalue.h>

#include <boost/thread/thread.hpp> // boost::thread::interrupt

#include <mutex>
#include <condition_variable>

struct CUpdatedBlock
{
    uint256 hash;
    int height;
};

static std::mutex cs_blockchange;
static std::condition_variable cond_blockchange;
static CUpdatedBlock latestblock;

extern void TxToJSON(const CTransaction& tx, const uint256 hashBlock, UniValue& entry);

double GetDifficulty(const CBlockIndex* blockindex)
{
    if (blockindex == nullptr)
    {
        if (chainActive.Tip() == nullptr)
            return 1.0;
        else
            blockindex = chainActive.Tip();
    }

    auto nShift = (blockindex->nBits >> 24) & 0xff;

    auto dDiff =
        static_cast<double>(0x0000ffff) / static_cast<double>(blockindex->nBits & 0x00ffffff);

    while (nShift < 29)
    {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 29)
    {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
}

CAmount ComputeMeanAmount(std::vector<CAmount> const& txFees)
{
    if (txFees.empty()) {
        return CAmount(0);
    }

    CAmount sumFees = std::accumulate(txFees.cbegin(), txFees.cend(), CAmount(0));
    return sumFees / CAmount(txFees.size());
}

CAmount ComputeMedianAmount(std::vector<CAmount> txFees)
{
    if (txFees.empty()) {
        return CAmount(0);
    }

    auto numFees = txFees.size();

    std::sort(txFees.begin(), txFees.end(), [](const CAmount& c1, const CAmount& c2) -> bool {
        return (c1 < c2);
    });

    size_t middleIndex = numFees / 2;
    if ((numFees % 2) != 0) {
        return txFees[middleIndex];
    }

    return (txFees[middleIndex] + txFees[middleIndex - 1]) / 2;
}

CAmount ComputeStdDevAmount(const std::vector<CAmount>& txFees)
{
    auto numFees = txFees.size();
    if (numFees < 2) {
        return CAmount(0);
    }

    CAmount meanValue = ComputeMeanAmount(txFees);
    double total = 0.0;
    for (auto const& currFee : txFees) {
        total += pow(currFee - meanValue, 2);
    }

    double v = total / static_cast<double>((numFees - 1));
    return static_cast<CAmount>(v);
}

UniValue FormatTxFeesInfo(const std::vector<CAmount>& txFees)
{
    auto result = UniValue{UniValue::VOBJ};
    result.pushKV("number", txFees.size());
    if (!txFees.empty()){
        result.pushKV("min", *std::min_element(txFees.cbegin(), txFees.cend()));
        result.pushKV("max", *std::max_element(txFees.cbegin(), txFees.cend()));
        result.pushKV("mean", ComputeMeanAmount(txFees));
        result.pushKV("median", ComputeMedianAmount(txFees));
        result.pushKV("stddev", ComputeStdDevAmount(txFees));
    }

    return result;
}

UniValue blockheaderToJSON(const CBlockIndex* blockindex)
{
    UniValue result{UniValue::VOBJ};
    result.push_back(Pair("hash", blockindex->GetBlockHash().GetHex()));
    int confirmations{-1};
    // Only report confirmations if the block is on the main chain
    if (chainActive.Contains(blockindex))
        confirmations = chainActive.Height() - blockindex->nHeight + 1;
    result.push_back(Pair("confirmations", confirmations));
    result.push_back(Pair("height", blockindex->nHeight));
    result.push_back(Pair("version", blockindex->nVersion));
    result.push_back(Pair("versionHex", strprintf("%08x", blockindex->nVersion)));
    result.push_back(Pair("merkleroot", blockindex->hashMerkleRoot.GetHex()));
    result.push_back(Pair("time", static_cast<int64_t>(blockindex->nTime)));
    result.push_back(Pair("mediantime", static_cast<int64_t>(blockindex->GetMedianTimePast())));
    result.push_back(Pair("nonce", static_cast<uint64_t>(blockindex->nNonce)));
    result.push_back(Pair("bits", strprintf("%08x", blockindex->nBits)));
    result.push_back(Pair("difficulty", GetDifficulty(blockindex)));
    result.push_back(Pair("chainwork", blockindex->nChainWork.GetHex()));
    result.push_back(Pair("stakedifficulty", std::to_string(blockindex->nStakeDifficulty)));
    result.push_back(Pair("votebits", strprintf("%08x", blockindex->nVoteBits)));
    result.push_back(Pair("ticketpoolsize", strprintf("%08x", blockindex->nTicketPoolSize)));
    if (blockindex->pstakeNode != nullptr)
        result.push_back(Pair("ticketlotterystate", StakeStateToString(blockindex->pstakeNode->FinalState())));

    if (blockindex->pprev)
        result.push_back(Pair("previousblockhash", blockindex->pprev->GetBlockHash().GetHex()));
    CBlockIndex *pnext = chainActive.Next(blockindex);
    if (pnext)
        result.push_back(Pair("nextblockhash", pnext->GetBlockHash().GetHex()));
    return result;
}

UniValue blockToJSON(const CBlock& block, const CBlockIndex* blockindex, bool txDetails)
{
    UniValue result{UniValue::VOBJ};
    result.push_back(Pair("hash", blockindex->GetBlockHash().GetHex()));
    int confirmations{-1};
    // Only report confirmations if the block is on the main chain
    if (chainActive.Contains(blockindex))
        confirmations = chainActive.Height() - blockindex->nHeight + 1;
    result.push_back(Pair("confirmations", confirmations));
    result.push_back(Pair("strippedsize", static_cast<int>(::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS))));
    result.push_back(Pair("size", static_cast<int>(::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION))));
    result.push_back(Pair("weight", static_cast<int>(::GetBlockWeight(block))));
    result.push_back(Pair("height", blockindex->nHeight));
    result.push_back(Pair("version", block.nVersion));
    result.push_back(Pair("versionHex", strprintf("%08x", block.nVersion)));
    result.push_back(Pair("merkleroot", block.hashMerkleRoot.GetHex()));
    UniValue txs{UniValue::VARR};
    for(const auto& tx : block.vtx)
    {
        if(txDetails)
        {
            UniValue objTx{UniValue::VOBJ};
            TxToUniv(*tx, uint256(), objTx, true, RPCSerializationFlags());
            txs.push_back(objTx);
        }
        else
            txs.push_back(tx->GetHash().GetHex());
    }
    result.push_back(Pair("tx", txs));
    result.push_back(Pair("time", block.GetBlockTime()));
    result.push_back(Pair("mediantime", static_cast<int64_t>(blockindex->GetMedianTimePast())));
    result.push_back(Pair("nonce", static_cast<uint64_t>(block.nNonce)));
    result.push_back(Pair("bits", strprintf("%08x", block.nBits)));
    result.push_back(Pair("difficulty", GetDifficulty(blockindex)));
    result.push_back(Pair("chainwork", blockindex->nChainWork.GetHex()));
    result.push_back(Pair("stakedifficulty", std::to_string(blockindex->nStakeDifficulty)));
    result.push_back(Pair("votebits", strprintf("%08x", blockindex->nVoteBits)));
    result.push_back(Pair("ticketpoolsize", strprintf("%08x", blockindex->nTicketPoolSize)));
    if (blockindex->pstakeNode != nullptr)
        result.push_back(Pair("ticketlotterystate", StakeStateToString(blockindex->pstakeNode->FinalState())));

    if (blockindex->pprev)
        result.push_back(Pair("previousblockhash", blockindex->pprev->GetBlockHash().GetHex()));
    CBlockIndex *pnext = chainActive.Next(blockindex);
    if (pnext)
        result.push_back(Pair("nextblockhash", pnext->GetBlockHash().GetHex()));
    return result;
}

UniValue getblockcount(const JSONRPCRequest& request)
{
    if (request.fHelp || !request.params.empty())
        throw std::runtime_error{
            "getblockcount\n"
            "\nReturns the number of blocks in the longest blockchain.\n"
            "\nResult:\n"
            "n    (numeric) The current block count\n"
            "\nExamples:\n"
            + HelpExampleCli("getblockcount", "")
            + HelpExampleRpc("getblockcount", "")
        };

    LOCK(cs_main);
    return chainActive.Height();
}

UniValue getbestblockhash(const JSONRPCRequest& request)
{
    if (request.fHelp || !request.params.empty())
        throw std::runtime_error{
            "getbestblockhash\n"
            "\nReturns the hash of the best (tip) block in the longest blockchain.\n"
            "\nResult:\n"
            "\"hex\"      (string) the block hash hex encoded\n"
            "\nExamples:\n"
            + HelpExampleCli("getbestblockhash", "")
            + HelpExampleRpc("getbestblockhash", "")
        };

    LOCK(cs_main);
    return chainActive.Tip()->GetBlockHash().GetHex();
}

void RPCNotifyBlockChange(bool ibd, const CBlockIndex * pindex)
{
    if(pindex) {
        std::lock_guard<std::mutex> lock{cs_blockchange};
        latestblock.hash = pindex->GetBlockHash();
        latestblock.height = pindex->nHeight;
    }
    cond_blockchange.notify_all();
}

UniValue waitfornewblock(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error{
            "waitfornewblock (timeout)\n"
            "\nWaits for a specific new block and returns useful info about it.\n"
            "\nReturns the current block on timeout or exit.\n"
            "\nArguments:\n"
            "1. timeout (int, optional, default=0) Time in milliseconds to wait for a response. 0 indicates no timeout.\n"
            "\nResult:\n"
            "{                           (json object)\n"
            "  \"hash\" : {       (string) The blockhash\n"
            "  \"height\" : {     (int) Block height\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("waitfornewblock", "1000")
            + HelpExampleRpc("waitfornewblock", "1000")
        };

    int timeout{0};
    if (!request.params[0].isNull())
        timeout = request.params[0].get_int();

    CUpdatedBlock block;
    {
        std::unique_lock<std::mutex> lock{cs_blockchange};
        block = latestblock;
        if(timeout)
            cond_blockchange.wait_for(lock, std::chrono::milliseconds{timeout}, [&block]{return latestblock.height != block.height || latestblock.hash != block.hash || !IsRPCRunning(); });
        else
            cond_blockchange.wait(lock, [&block]{return latestblock.height != block.height || latestblock.hash != block.hash || !IsRPCRunning(); });
        block = latestblock;
    }
    UniValue ret{UniValue::VOBJ};
    ret.push_back(Pair("hash", block.hash.GetHex()));
    ret.push_back(Pair("height", block.height));
    return ret;
}

UniValue waitforblock(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error{
            "waitforblock <blockhash> (timeout)\n"
            "\nWaits for a specific new block and returns useful info about it.\n"
            "\nReturns the current block on timeout or exit.\n"
            "\nArguments:\n"
            "1. \"blockhash\" (required, string) Block hash to wait for.\n"
            "2. timeout       (int, optional, default=0) Time in milliseconds to wait for a response. 0 indicates no timeout.\n"
            "\nResult:\n"
            "{                           (json object)\n"
            "  \"hash\" : {       (string) The blockhash\n"
            "  \"height\" : {     (int) Block height\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("waitforblock", "\"0000000000079f8ef3d2c688c244eb7a4570b24c9ed7b4a8c619eb02596f8862\", 1000")
            + HelpExampleRpc("waitforblock", "\"0000000000079f8ef3d2c688c244eb7a4570b24c9ed7b4a8c619eb02596f8862\", 1000")
        };
    int timeout{0};

    const auto hash = uint256S(request.params[0].get_str());

    if (!request.params[1].isNull())
        timeout = request.params[1].get_int();

    CUpdatedBlock block;
    {
        std::unique_lock<std::mutex> lock{cs_blockchange};
        if(timeout)
            cond_blockchange.wait_for(lock, std::chrono::milliseconds{timeout}, [&hash]{return latestblock.hash == hash || !IsRPCRunning();});
        else
            cond_blockchange.wait(lock, [&hash]{return latestblock.hash == hash || !IsRPCRunning(); });
        block = latestblock;
    }

    UniValue ret{UniValue::VOBJ};
    ret.push_back(Pair("hash", block.hash.GetHex()));
    ret.push_back(Pair("height", block.height));
    return ret;
}

UniValue waitforblockheight(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error{
            "waitforblockheight <height> (timeout)\n"
            "\nWaits for (at least) block height and returns the height and hash\n"
            "of the current tip.\n"
            "\nReturns the current block on timeout or exit.\n"
            "\nArguments:\n"
            "1. height  (required, int) Block height to wait for (int)\n"
            "2. timeout (int, optional, default=0) Time in milliseconds to wait for a response. 0 indicates no timeout.\n"
            "\nResult:\n"
            "{                           (json object)\n"
            "  \"hash\" : {       (string) The blockhash\n"
            "  \"height\" : {     (int) Block height\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("waitforblockheight", "\"100\", 1000")
            + HelpExampleRpc("waitforblockheight", "\"100\", 1000")
        };
    int timeout{0};

    const auto height = request.params[0].get_int();

    if (!request.params[1].isNull())
        timeout = request.params[1].get_int();

    CUpdatedBlock block;
    {
        std::unique_lock<std::mutex> lock{cs_blockchange};
        if(timeout)
            cond_blockchange.wait_for(lock, std::chrono::milliseconds{timeout}, [&height]{return latestblock.height >= height || !IsRPCRunning();});
        else
            cond_blockchange.wait(lock, [&height]{return latestblock.height >= height || !IsRPCRunning(); });
        block = latestblock;
    }
    UniValue ret{UniValue::VOBJ};
    ret.push_back(Pair("hash", block.hash.GetHex()));
    ret.push_back(Pair("height", block.height));
    return ret;
}

UniValue getdifficulty(const JSONRPCRequest& request)
{
    if (request.fHelp || !request.params.empty())
        throw std::runtime_error{
            "getdifficulty\n"
            "\nReturns the proof-of-work difficulty as a multiple of the minimum difficulty.\n"
            "\nResult:\n"
            "n.nnn       (numeric) the proof-of-work difficulty as a multiple of the minimum difficulty.\n"
            "\nExamples:\n"
            + HelpExampleCli("getdifficulty", "")
            + HelpExampleRpc("getdifficulty", "")
        };

    LOCK(cs_main);
    return GetDifficulty();
}

static std::string EntryDescriptionString()
{
    return "    \"size\" : n,             (numeric) virtual transaction size as defined in BIP 141. This is different from actual serialized size for witness transactions as witness data is discounted.\n"
           "    \"fee\" : n,              (numeric) transaction fee in " + CURRENCY_UNIT + "\n"
           "    \"modifiedfee\" : n,      (numeric) transaction fee with fee deltas used for mining priority\n"
           "    \"time\" : n,             (numeric) local time transaction entered pool in seconds since 1 Jan 1970 GMT\n"
           "    \"height\" : n,           (numeric) block height when transaction entered pool\n"
           "    \"descendantcount\" : n,  (numeric) number of in-mempool descendant transactions (including this one)\n"
           "    \"descendantsize\" : n,   (numeric) virtual transaction size of in-mempool descendants (including this one)\n"
           "    \"descendantfees\" : n,   (numeric) modified fees (see above) of in-mempool descendants (including this one)\n"
           "    \"ancestorcount\" : n,    (numeric) number of in-mempool ancestor transactions (including this one)\n"
           "    \"ancestorsize\" : n,     (numeric) virtual transaction size of in-mempool ancestors (including this one)\n"
           "    \"ancestorfees\" : n,     (numeric) modified fees (see above) of in-mempool ancestors (including this one)\n"
           "    \"wtxid\" : hash,         (string) hash of serialized transaction, including witness data\n"
           "    \"depends\" : [           (array) unconfirmed transactions used as inputs for this transaction\n"
           "        \"transactionid\",    (string) parent transaction id\n"
           "       ... ]\n";
}

static void entryToJSON(UniValue &info, const CTxMemPoolEntry &e)
{
    AssertLockHeld(mempool.cs);

    info.push_back(Pair("size", (int)e.GetTxSize()));
    info.push_back(Pair("fee", ValueFromAmount(e.GetFee())));
    info.push_back(Pair("modifiedfee", ValueFromAmount(e.GetModifiedFee())));
    info.push_back(Pair("time", e.GetTime()));
    info.push_back(Pair("height", (int)e.GetHeight()));
    info.push_back(Pair("descendantcount", e.GetCountWithDescendants()));
    info.push_back(Pair("descendantsize", e.GetSizeWithDescendants()));
    info.push_back(Pair("descendantfees", e.GetModFeesWithDescendants()));
    info.push_back(Pair("ancestorcount", e.GetCountWithAncestors()));
    info.push_back(Pair("ancestorsize", e.GetSizeWithAncestors()));
    info.push_back(Pair("ancestorfees", e.GetModFeesWithAncestors()));
    info.push_back(Pair("wtxid", mempool.vTxHashes[e.vTxHashesIdx].first.ToString()));
    const auto& tx = e.GetTx();
    std::set<std::string> setDepends;
    for (const auto& txin : tx.vin)
    {
        if (mempool.exists(txin.prevout.hash))
            setDepends.insert(txin.prevout.hash.ToString());
    }

    UniValue depends{UniValue::VARR};
    for (const auto& dep : setDepends)
    {
        depends.push_back(dep);
    }

    info.push_back(Pair("depends", depends));
}

UniValue mempoolToJSON(bool fVerbose)
{
    if (fVerbose)
    {
        LOCK(mempool.cs);
        UniValue o{UniValue::VOBJ};
        for (const CTxMemPoolEntry& e : mempool.mapTx)
        {
            const auto& hash = e.GetTx().GetHash();
            UniValue info{UniValue::VOBJ};
            entryToJSON(info, e);
            o.push_back(Pair(hash.ToString(), info));
        }
        return o;
    }
    else
    {
        std::vector<uint256> vtxid;
        mempool.queryHashes(vtxid);

        UniValue a{UniValue::VARR};
        for (const auto& hash : vtxid)
            a.push_back(hash.ToString());

        return a;
    }
}

UniValue getrawmempool(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error{
            "getrawmempool ( verbose )\n"
            "\nReturns all transaction ids in memory pool as a json array of string transaction ids.\n"
            "\nHint: use getmempoolentry to fetch a specific transaction from the mempool.\n"
            "\nArguments:\n"
            "1. verbose (boolean, optional, default=false) True for a json object, false for array of transaction ids\n"
            "\nResult: (for verbose = false):\n"
            "[                     (json array of string)\n"
            "  \"transactionid\"     (string) The transaction id\n"
            "  ,...\n"
            "]\n"
            "\nResult: (for verbose = true):\n"
            "{                           (json object)\n"
            "  \"transactionid\" : {       (json object)\n"
            + EntryDescriptionString()
            + "  }, ...\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getrawmempool", "true")
            + HelpExampleRpc("getrawmempool", "true")
        };

    auto fVerbose = false;
    if (!request.params[0].isNull())
        fVerbose = request.params[0].get_bool();

    return mempoolToJSON(fVerbose);
}

UniValue getmempoolancestors(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2) {
        throw std::runtime_error{
            "getmempoolancestors txid (verbose)\n"
            "\nIf txid is in the mempool, returns all in-mempool ancestors.\n"
            "\nArguments:\n"
            "1. \"txid\"                 (string, required) The transaction id (must be in mempool)\n"
            "2. verbose                  (boolean, optional, default=false) True for a json object, false for array of transaction ids\n"
            "\nResult (for verbose=false):\n"
            "[                       (json array of strings)\n"
            "  \"transactionid\"           (string) The transaction id of an in-mempool ancestor transaction\n"
            "  ,...\n"
            "]\n"
            "\nResult (for verbose=true):\n"
            "{                           (json object)\n"
            "  \"transactionid\" : {       (json object)\n"
            + EntryDescriptionString()
            + "  }, ...\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getmempoolancestors", "\"mytxid\"")
            + HelpExampleRpc("getmempoolancestors", "\"mytxid\"")
        };
    }

    auto fVerbose = false;
    if (!request.params[1].isNull())
        fVerbose = request.params[1].get_bool();

    const auto hash = ParseHashV(request.params[0], "parameter 1");

    LOCK(mempool.cs);

    CTxMemPool::txiter it{mempool.mapTx.find(hash)};
    if (it == mempool.mapTx.end()) {
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Transaction not in mempool");
    }

    CTxMemPool::setEntries setAncestors;
    const auto noLimit = std::numeric_limits<uint64_t>::max();
    std::string dummy;
    mempool.CalculateMemPoolAncestors(*it, setAncestors, noLimit, noLimit, noLimit, noLimit, dummy, false);

    if (!fVerbose) {
        UniValue o{UniValue::VARR};
        for (auto ancestorIt : setAncestors) {
            o.push_back(ancestorIt->GetTx().GetHash().ToString());
        }

        return o;
    } else {
        UniValue o{UniValue::VOBJ};
        for (auto ancestorIt : setAncestors) {
            const CTxMemPoolEntry &e = *ancestorIt;
            const auto& _hash = e.GetTx().GetHash();
            UniValue info{UniValue::VOBJ};
            entryToJSON(info, e);
            o.push_back(Pair(_hash.ToString(), info));
        }
        return o;
    }
}

UniValue getmempooldescendants(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2) {
        throw std::runtime_error{
            "getmempooldescendants txid (verbose)\n"
            "\nIf txid is in the mempool, returns all in-mempool descendants.\n"
            "\nArguments:\n"
            "1. \"txid\"                 (string, required) The transaction id (must be in mempool)\n"
            "2. verbose                  (boolean, optional, default=false) True for a json object, false for array of transaction ids\n"
            "\nResult (for verbose=false):\n"
            "[                       (json array of strings)\n"
            "  \"transactionid\"           (string) The transaction id of an in-mempool descendant transaction\n"
            "  ,...\n"
            "]\n"
            "\nResult (for verbose=true):\n"
            "{                           (json object)\n"
            "  \"transactionid\" : {       (json object)\n"
            + EntryDescriptionString()
            + "  }, ...\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getmempooldescendants", "\"mytxid\"")
            + HelpExampleRpc("getmempooldescendants", "\"mytxid\"")
        };
    }

    auto fVerbose = false;
    if (!request.params[1].isNull())
        fVerbose = request.params[1].get_bool();

    const auto hash = ParseHashV(request.params[0], "parameter 1");

    LOCK(mempool.cs);

    CTxMemPool::txiter it{mempool.mapTx.find(hash)};
    if (it == mempool.mapTx.end()) {
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Transaction not in mempool");
    }

    CTxMemPool::setEntries setDescendants;
    mempool.CalculateDescendants(it, setDescendants);
    // CTxMemPool::CalculateDescendants will include the given tx
    setDescendants.erase(it);

    if (!fVerbose) {
        UniValue o{UniValue::VARR};
        for (CTxMemPool::txiter descendantIt : setDescendants) {
            o.push_back(descendantIt->GetTx().GetHash().ToString());
        }

        return o;
    } else {
        UniValue o(UniValue::VOBJ);
        for (auto descendantIt : setDescendants) {
            const CTxMemPoolEntry &e = *descendantIt;
            const auto& _hash = e.GetTx().GetHash();
            UniValue info{UniValue::VOBJ};
            entryToJSON(info, e);
            o.push_back(Pair(_hash.ToString(), info));
        }
        return o;
    }
}

UniValue getmempoolentry(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error{
            "getmempoolentry txid\n"
            "\nReturns mempool data for given transaction\n"
            "\nArguments:\n"
            "1. \"txid\"                   (string, required) The transaction id (must be in mempool)\n"
            "\nResult:\n"
            "{                           (json object)\n"
            + EntryDescriptionString()
            + "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getmempoolentry", "\"mytxid\"")
            + HelpExampleRpc("getmempoolentry", "\"mytxid\"")
        };
    }

    const auto hash = ParseHashV(request.params[0], "parameter 1");

    LOCK(mempool.cs);

    CTxMemPool::txiter it{mempool.mapTx.find(hash)};
    if (it == mempool.mapTx.end()) {
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Transaction not in mempool");
    }

    const CTxMemPoolEntry &e{*it};
    UniValue info{UniValue::VOBJ};
    entryToJSON(info, e);
    return info;
}

UniValue getblockhash(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error{
            "getblockhash height\n"
            "\nReturns hash of block in best-block-chain at height provided.\n"
            "\nArguments:\n"
            "1. height         (numeric, required) The height index\n"
            "\nResult:\n"
            "\"hash\"         (string) The block hash\n"
            "\nExamples:\n"
            + HelpExampleCli("getblockhash", "1000")
            + HelpExampleRpc("getblockhash", "1000")
        };

    LOCK(cs_main);

    const auto nHeight = request.params[0].get_int();
    if (nHeight < 0 || nHeight > chainActive.Height())
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Block height out of range");

    const auto * const pblockindex = chainActive[nHeight];
    return pblockindex->GetBlockHash().GetHex();
}

UniValue getblockheader(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error{
            "getblockheader \"hash\" ( verbose )\n"
            "\nIf verbose is false, returns a string that is serialized, hex-encoded data for blockheader 'hash'.\n"
            "If verbose is true, returns an Object with information about blockheader <hash>.\n"
            "\nArguments:\n"
            "1. \"hash\"          (string, required) The block hash\n"
            "2. verbose           (boolean, optional, default=true) true for a json object, false for the hex encoded data\n"
            "\nResult (for verbose = true):\n"
            "{\n"
            "  \"hash\" : \"hash\",     (string) the block hash (same as provided)\n"
            "  \"confirmations\" : n,   (numeric) The number of confirmations, or -1 if the block is not on the main chain\n"
            "  \"height\" : n,          (numeric) The block height or index\n"
            "  \"version\" : n,         (numeric) The block version\n"
            "  \"versionHex\" : \"00000000\", (string) The block version formatted in hexadecimal\n"
            "  \"merkleroot\" : \"xxxx\", (string) The merkle root\n"
            "  \"time\" : ttt,          (numeric) The block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"mediantime\" : ttt,    (numeric) The median block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"nonce\" : n,           (numeric) The nonce\n"
            "  \"bits\" : \"1d00ffff\", (string) The bits\n"
            "  \"difficulty\" : x.xxx,  (numeric) The difficulty\n"
            "  \"chainwork\" : \"0000...1f3\"     (string) Expected number of hashes required to produce the current chain (in hex)\n"
            "  \"previousblockhash\" : \"hash\",  (string) The hash of the previous block\n"
            "  \"nextblockhash\" : \"hash\",      (string) The hash of the next block\n"
            "}\n"
            "\nResult (for verbose=false):\n"
            "\"data\"             (string) A string that is serialized, hex-encoded data for block 'hash'.\n"
            "\nExamples:\n"
            + HelpExampleCli("getblockheader", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"")
            + HelpExampleRpc("getblockheader", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"")
        };

    LOCK(cs_main);

    const auto& strHash = request.params[0].get_str();
    const auto hash = uint256S(strHash);

    auto fVerbose = true;
    if (!request.params[1].isNull())
        fVerbose = request.params[1].get_bool();

    if (mapBlockIndex.count(hash) == 0)
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Block not found");

    const auto * const pblockindex = mapBlockIndex[hash];

    if (!fVerbose)
    {
        CDataStream ssBlock{SER_NETWORK, PROTOCOL_VERSION};
        ssBlock << pblockindex->GetBlockHeader();
        return HexStr(ssBlock);
    }

    return blockheaderToJSON(pblockindex);
}

UniValue getblock(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error{
            "getblock \"blockhash\" ( verbosity ) \n"
            "\nIf verbosity is 0, returns a string that is serialized, hex-encoded data for block 'hash'.\n"
            "If verbosity is 1, returns an Object with information about block <hash>.\n"
            "If verbosity is 2, returns an Object with information about block <hash> and information about each transaction. \n"
            "\nArguments:\n"
            "1. \"blockhash\"          (string, required) The block hash\n"
            "2. verbosity              (numeric, optional, default=1) 0 for hex encoded data, 1 for a json object, and 2 for json object with transaction data\n"
            "\nResult (for verbosity = 0):\n"
            "\"data\"             (string) A string that is serialized, hex-encoded data for block 'hash'.\n"
            "\nResult (for verbosity = 1):\n"
            "{\n"
            "  \"hash\" : \"hash\",     (string) the block hash (same as provided)\n"
            "  \"confirmations\" : n,   (numeric) The number of confirmations, or -1 if the block is not on the main chain\n"
            "  \"size\" : n,            (numeric) The block size\n"
            "  \"strippedsize\" : n,    (numeric) The block size excluding witness data\n"
            "  \"weight\" : n           (numeric) The block weight as defined in BIP 141\n"
            "  \"height\" : n,          (numeric) The block height or index\n"
            "  \"version\" : n,         (numeric) The block version\n"
            "  \"versionHex\" : \"00000000\", (string) The block version formatted in hexadecimal\n"
            "  \"merkleroot\" : \"xxxx\", (string) The merkle root\n"
            "  \"tx\" : [               (array of string) The transaction ids\n"
            "     \"transactionid\"     (string) The transaction id\n"
            "     ,...\n"
            "  ],\n"
            "  \"time\" : ttt,          (numeric) The block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"mediantime\" : ttt,    (numeric) The median block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"nonce\" : n,           (numeric) The nonce\n"
            "  \"bits\" : \"1d00ffff\", (string) The bits\n"
            "  \"difficulty\" : x.xxx,  (numeric) The difficulty\n"
            "  \"chainwork\" : \"xxxx\",  (string) Expected number of hashes required to produce the chain up to this block (in hex)\n"
            "  \"previousblockhash\" : \"hash\",  (string) The hash of the previous block\n"
            "  \"nextblockhash\" : \"hash\"       (string) The hash of the next block\n"
            "}\n"
            "\nResult (for verbosity = 2):\n"
            "{\n"
            "  ...,                     Same output as verbosity = 1.\n"
            "  \"tx\" : [               (array of Objects) The transactions in the format of the getrawtransaction RPC. Different from verbosity = 1 \"tx\" result.\n"
            "         ,...\n"
            "  ],\n"
            "  ,...                     Same output as verbosity = 1.\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getblock", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"")
            + HelpExampleRpc("getblock", "\"00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09\"")
        };

    LOCK(cs_main);

    const auto& strHash = request.params[0].get_str();
    const auto hash = uint256S(strHash);

    int verbosity{1};
    if (!request.params[1].isNull()) {
        if(request.params[1].isNum())
            verbosity = request.params[1].get_int();
        else
            verbosity = request.params[1].get_bool() ? 1 : 0;
    }

    if (mapBlockIndex.count(hash) == 0)
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Block not found");

    const auto * const pblockindex = mapBlockIndex[hash];

    if (fHavePruned && !(pblockindex->nStatus & BLOCK_HAVE_DATA) && pblockindex->nTx > 0)
        throw JSONRPCError(RPCErrorCode::MISC_ERROR, "Block not available (pruned data)");

    CBlock block;
    if (!ReadBlockFromDisk(block, pblockindex, Params().GetConsensus()))
        // Block not found on disk. This could be because we have the block
        // header in our index but don't have the block (for example if a
        // non-whitelisted node sends us an unrequested long chain of valid
        // blocks, we add the headers to our index, but don't accept the
        // block).
        throw JSONRPCError(RPCErrorCode::MISC_ERROR, "Block not found on disk");

    if (verbosity <= 0)
    {
        CDataStream ssBlock{SER_NETWORK, PROTOCOL_VERSION | RPCSerializationFlags()};
        ssBlock << block;
        return HexStr(ssBlock);
    }

    return blockToJSON(block, pblockindex, verbosity >= 2);
}

struct CCoinsStats
{
    int nHeight{0};
    uint256 hashBlock;
    uint64_t nTransactions{0};
    uint64_t nTransactionOutputs{0};
    uint64_t nBogoSize{0};
    uint256 hashSerialized;
    uint64_t nDiskSize{0};
    CAmount nTotalAmount{0};
};

static void ApplyStats(CCoinsStats &stats, CHashWriter& ss, const uint256& hash, const std::map<uint32_t, Coin>& outputs)
{
    assert(!outputs.empty());
    ss << hash;
    ss << VARINT(outputs.begin()->second.nHeight * 2 + outputs.begin()->second.fCoinBase);
    stats.nTransactions++;
    for (const auto& output : outputs) {
        ss << VARINT(output.first + 1);
        ss << output.second.out.scriptPubKey;
        ss << VARINT(output.second.out.nValue);
        stats.nTransactionOutputs++;
        stats.nTotalAmount += output.second.out.nValue;
        stats.nBogoSize += 32 /* txid */ + 4 /* vout index */ + 4 /* height + coinbase */ + 8 /* amount */ +
                           2 /* scriptPubKey len */ + output.second.out.scriptPubKey.size() /* scriptPubKey */;
    }
    ss << VARINT(0);
}

//! Calculate statistics about the unspent transaction output set
static bool GetUTXOStats(CCoinsView *view, CCoinsStats &stats)
{
    std::unique_ptr<CCoinsViewCursor> pcursor{view->Cursor()};
    assert(pcursor);

    CHashWriter ss{SER_GETHASH, PROTOCOL_VERSION};
    stats.hashBlock = pcursor->GetBestBlock();
    {
        LOCK(cs_main);
        stats.nHeight = mapBlockIndex.find(stats.hashBlock)->second->nHeight;
    }
    ss << stats.hashBlock;
    uint256 prevkey;
    std::map<uint32_t, Coin> outputs;
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        COutPoint key;
        Coin coin;
        if (pcursor->GetKey(key) && pcursor->GetValue(coin)) {
            if (!outputs.empty() && key.hash != prevkey) {
                ApplyStats(stats, ss, prevkey, outputs);
                outputs.clear();
            }
            prevkey = key.hash;
            outputs[key.n] = std::move(coin);
        } else {
            return error("%s: unable to read value", __func__);
        }
        pcursor->Next();
    }
    if (!outputs.empty()) {
        ApplyStats(stats, ss, prevkey, outputs);
    }
    stats.hashSerialized = ss.GetHash();
    stats.nDiskSize = view->EstimateSize();
    return true;
}

UniValue pruneblockchain(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error{
            "pruneblockchain\n"
            "\nArguments:\n"
            "1. \"height\"       (numeric, required) The block height to prune up to. May be set to a discrete height, or a unix timestamp\n"
            "                  to prune blocks whose block time is at least 2 hours older than the provided timestamp.\n"
            "\nResult:\n"
            "n    (numeric) Height of the last block pruned.\n"
            "\nExamples:\n"
            + HelpExampleCli("pruneblockchain", "1000")
            + HelpExampleRpc("pruneblockchain", "1000")
        };

    if (!fPruneMode)
        throw JSONRPCError(RPCErrorCode::MISC_ERROR, "Cannot prune blocks because node is not in prune mode.");

    LOCK(cs_main);

    auto heightParam = request.params[0].get_int();
    if (heightParam < 0)
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Negative block height.");

    // Height value more than a billion is too high to be a block height, and
    // too low to be a block time (corresponds to timestamp from Sep 2001).
    if (heightParam > 1000000000) {
        // Add a 2 hour buffer to include blocks which might have had old timestamps
        auto* const pindex = chainActive.FindEarliestAtLeast(heightParam - TIMESTAMP_WINDOW);
        if (!pindex) {
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Could not find block with at least the specified timestamp.");
        }
        heightParam = pindex->nHeight;
    }

    auto height = static_cast<unsigned int>(heightParam);
    auto chainHeight = static_cast<unsigned int>(chainActive.Height());
    if (chainHeight < Params().PruneAfterHeight())
        throw JSONRPCError(RPCErrorCode::MISC_ERROR, "Blockchain is too short for pruning.");
    else if (height > chainHeight)
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Blockchain is shorter than the attempted prune height.");
    else if (height > chainHeight - MIN_BLOCKS_TO_KEEP) {
        LogPrint(BCLog::RPC, "Attempt to prune blocks close to the tip.  Retaining the minimum number of blocks.");
        height = chainHeight - MIN_BLOCKS_TO_KEEP;
    }

    PruneBlockFilesManual(height);
    return uint64_t(height);
}

UniValue gettxoutsetinfo(const JSONRPCRequest& request)
{
    if (request.fHelp || !request.params.empty())
        throw std::runtime_error{
            "gettxoutsetinfo\n"
            "\nReturns statistics about the unspent transaction output set.\n"
            "Note this call may take some time.\n"
            "\nResult:\n"
            "{\n"
            "  \"height\":n,     (numeric) The current block height (index)\n"
            "  \"bestblock\": \"hex\",   (string) the best block hash hex\n"
            "  \"transactions\": n,      (numeric) The number of transactions\n"
            "  \"txouts\": n,            (numeric) The number of output transactions\n"
            "  \"bogosize\": n,          (numeric) A meaningless metric for UTXO set size\n"
            "  \"hash_serialized_2\": \"hash\", (string) The serialized hash\n"
            "  \"disk_size\": n,         (numeric) The estimated size of the chainstate on disk\n"
            "  \"total_amount\": x.xxx          (numeric) The total amount\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("gettxoutsetinfo", "")
            + HelpExampleRpc("gettxoutsetinfo", "")
        };

    UniValue ret{UniValue::VOBJ};

    CCoinsStats stats;
    FlushStateToDisk();
    if (GetUTXOStats(pcoinsdbview, stats)) {
        ret.push_back(Pair("height", static_cast<int64_t>(stats.nHeight)));
        ret.push_back(Pair("bestblock", stats.hashBlock.GetHex()));
        ret.push_back(Pair("transactions", static_cast<int64_t>(stats.nTransactions)));
        ret.push_back(Pair("txouts", static_cast<int64_t>(stats.nTransactionOutputs)));
        ret.push_back(Pair("bogosize", static_cast<int64_t>(stats.nBogoSize)));
        ret.push_back(Pair("hash_serialized_2", stats.hashSerialized.GetHex()));
        ret.push_back(Pair("disk_size", stats.nDiskSize));
        ret.push_back(Pair("total_amount", ValueFromAmount(stats.nTotalAmount)));
    } else {
        throw JSONRPCError(RPCErrorCode::INTERNAL_ERROR, "Unable to read UTXO set");
    }
    return ret;
}

UniValue gettxout(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 3)
        throw std::runtime_error{
            "gettxout \"txid\" n ( include_mempool )\n"
            "\nReturns details about an unspent transaction output.\n"
            "\nArguments:\n"
            "1. \"txid\"             (string, required) The transaction id\n"
            "2. \"n\"                (numeric, required) vout number\n"
            "3. \"include_mempool\"  (boolean, optional) Whether to include the mempool. Default: true."
            "     Note that an unspent output that is spent in the mempool won't appear.\n"
            "\nResult:\n"
            "{\n"
            "  \"bestblock\" : \"hash\",    (string) the block hash\n"
            "  \"confirmations\" : n,       (numeric) The number of confirmations\n"
            "  \"value\" : x.xxx,           (numeric) The transaction value in " + CURRENCY_UNIT + "\n"
            "  \"scriptPubKey\" : {         (json object)\n"
            "     \"asm\" : \"code\",       (string) \n"
            "     \"hex\" : \"hex\",        (string) \n"
            "     \"reqSigs\" : n,          (numeric) Number of required signatures\n"
            "     \"type\" : \"pubkeyhash\", (string) The type, eg pubkeyhash\n"
            "     \"addresses\" : [          (array of string) array of paicoin addresses\n"
            "        \"address\"     (string) paicoin address\n"
            "        ,...\n"
            "     ]\n"
            "  },\n"
            "  \"coinbase\" : true|false   (boolean) Coinbase or not\n"
            "}\n"

            "\nExamples:\n"
            "\nGet unspent transactions\n"
            + HelpExampleCli("listunspent", "") +
            "\nView the details\n"
            + HelpExampleCli("gettxout", "\"txid\" 1") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("gettxout", "\"txid\", 1")
        };

    LOCK(cs_main);

    UniValue ret{UniValue::VOBJ};

    const auto hash = uint256S(request.params[0].get_str());
    const auto n = request.params[1].get_int();
    const COutPoint out{hash, static_cast<uint32_t>(n)};
    auto fMempool = true;
    if (!request.params[2].isNull())
        fMempool = request.params[2].get_bool();

    Coin coin;
    if (fMempool) {
        LOCK(mempool.cs);
        CCoinsViewMemPool view(pcoinsTip, mempool);
        if (!view.GetCoin(out, coin) || mempool.isSpent(out)) {
            return NullUniValue;
        }
    } else {
        if (!pcoinsTip->GetCoin(out, coin)) {
            return NullUniValue;
        }
    }

    auto it = mapBlockIndex.find(pcoinsTip->GetBestBlock());
    auto * const pindex = it->second;
    ret.push_back(Pair("bestblock", pindex->GetBlockHash().GetHex()));
    if (coin.nHeight == MEMPOOL_HEIGHT) {
        ret.push_back(Pair("confirmations", 0));
    } else {
        ret.push_back(Pair("confirmations", static_cast<int64_t>((pindex->nHeight - coin.nHeight + 1))));
    }
    ret.push_back(Pair("value", ValueFromAmount(coin.out.nValue)));
    UniValue o{UniValue::VOBJ};
    ScriptPubKeyToUniv(coin.out.scriptPubKey, o, true);
    ret.push_back(Pair("scriptPubKey", o));
    ret.push_back(Pair("coinbase", static_cast<bool>(coin.fCoinBase)));

    return ret;
}

UniValue verifychain(const JSONRPCRequest& request)
{
    auto nCheckLevel = static_cast<int>(gArgs.GetArg("-checklevel", DEFAULT_CHECKLEVEL));
    auto nCheckDepth = static_cast<int>(gArgs.GetArg("-checkblocks", DEFAULT_CHECKBLOCKS));
    if (request.fHelp || request.params.size() > 2)
        throw std::runtime_error{
            "verifychain ( checklevel nblocks )\n"
            "\nVerifies blockchain database.\n"
            "\nArguments:\n"
            "1. checklevel   (numeric, optional, 0-4, default=" + strprintf("%d", nCheckLevel) + ") How thorough the block verification is.\n"
            "2. nblocks      (numeric, optional, default=" + strprintf("%d", nCheckDepth) + ", 0=all) The number of blocks to check.\n"
            "\nResult:\n"
            "true|false       (boolean) Verified or not\n"
            "\nExamples:\n"
            + HelpExampleCli("verifychain", "")
            + HelpExampleRpc("verifychain", "")
        };

    LOCK(cs_main);

    if (!request.params[0].isNull())
        nCheckLevel = request.params[0].get_int();
    if (!request.params[1].isNull())
        nCheckDepth = request.params[1].get_int();

    return CVerifyDB().VerifyDB(Params(), pcoinsTip, nCheckLevel, nCheckDepth);
}

/** Implementation of IsSuperMajority with better feedback */
static UniValue SoftForkMajorityDesc(int version, CBlockIndex* pindex, const Consensus::Params& consensusParams)
{
    UniValue rv{UniValue::VOBJ};
    auto activated = false;
    switch(version)
    {
        case 2:
            activated = pindex->nHeight >= consensusParams.BIP34Height;
            break;
        case 3:
            activated = pindex->nHeight >= consensusParams.BIP66Height;
            break;
        case 4:
            activated = pindex->nHeight >= consensusParams.BIP65Height;
            break;
    }
    rv.push_back(Pair("status", activated));
    return rv;
}

static UniValue SoftForkDesc(const std::string &name, int version, CBlockIndex* pindex, const Consensus::Params& consensusParams)
{
    UniValue rv{UniValue::VOBJ};
    rv.push_back(Pair("id", name));
    rv.push_back(Pair("version", version));
    rv.push_back(Pair("reject", SoftForkMajorityDesc(version, pindex, consensusParams)));
    return rv;
}

static UniValue BIP9SoftForkDesc(const Consensus::Params& consensusParams, Consensus::DeploymentPos id)
{
    UniValue rv{UniValue::VOBJ};
    const auto thresholdState = VersionBitsTipState(consensusParams, id);
    switch (thresholdState) {
    case THRESHOLD_DEFINED: rv.push_back(Pair("status", "defined")); break;
    case THRESHOLD_STARTED: rv.push_back(Pair("status", "started")); break;
    case THRESHOLD_LOCKED_IN: rv.push_back(Pair("status", "locked_in")); break;
    case THRESHOLD_ACTIVE: rv.push_back(Pair("status", "active")); break;
    case THRESHOLD_FAILED: rv.push_back(Pair("status", "failed")); break;
    }
    if (THRESHOLD_STARTED == thresholdState)
    {
        rv.push_back(Pair("bit", consensusParams.vDeployments[id].bit));
    }
    rv.push_back(Pair("startTime", consensusParams.vDeployments[id].nStartTime));
    rv.push_back(Pair("timeout", consensusParams.vDeployments[id].nTimeout));
    rv.push_back(Pair("since", VersionBitsTipStateSinceHeight(consensusParams, id)));
    if (THRESHOLD_STARTED == thresholdState)
    {
        UniValue statsUV{UniValue::VOBJ};
        BIP9Stats statsStruct = VersionBitsTipStatistics(consensusParams, id);
        statsUV.push_back(Pair("period", statsStruct.period));
        statsUV.push_back(Pair("threshold", statsStruct.threshold));
        statsUV.push_back(Pair("elapsed", statsStruct.elapsed));
        statsUV.push_back(Pair("count", statsStruct.count));
        statsUV.push_back(Pair("possible", statsStruct.possible));
        rv.push_back(Pair("statistics", statsUV));
    }
    return rv;
}

static void BIP9SoftForkDescPushBack(UniValue& bip9_softforks, const std::string &name, const Consensus::Params& consensusParams, Consensus::DeploymentPos id)
{
    // Deployments with timeout value of 0 are hidden.
    // A timeout value of 0 guarantees a softfork will never be activated.
    // This is used when softfork codes are merged without specifying the deployment schedule.
    if (consensusParams.vDeployments[id].nTimeout > 0)
        bip9_softforks.push_back(Pair(name, BIP9SoftForkDesc(consensusParams, id)));
}

UniValue getblockchaininfo(const JSONRPCRequest& request)
{
    if (request.fHelp || !request.params.empty())
        throw std::runtime_error{
            "getblockchaininfo\n"
            "Returns an object containing various state info regarding blockchain processing.\n"
            "\nResult:\n"
            "{\n"
            "  \"chain\": \"xxxx\",        (string) current network name as defined in BIP70 (main, test, regtest)\n"
            "  \"blocks\": xxxxxx,         (numeric) the current number of blocks processed in the server\n"
            "  \"headers\": xxxxxx,        (numeric) the current number of headers we have validated\n"
            "  \"bestblockhash\": \"...\", (string) the hash of the currently best block\n"
            "  \"difficulty\": xxxxxx,     (numeric) the current difficulty\n"
            "  \"mediantime\": xxxxxx,     (numeric) median time for the current best block\n"
            "  \"verificationprogress\": xxxx, (numeric) estimate of verification progress [0..1]\n"
            "  \"chainwork\": \"xxxx\"     (string) total amount of work in active chain, in hexadecimal\n"
            "  \"size_on_disk\": xxxxxx,   (numeric) the estimated size of the block and undo files on disk\n"
            "  \"pruned\": xx,             (boolean) if the blocks are subject to pruning\n"
            "  \"pruneheight\": xxxxxx,    (numeric) lowest-height complete block stored (only present if pruning is enabled)\n"
            "  \"automatic_pruning\": xx,  (boolean) whether automatic pruning is enabled (only present if pruning is enabled)\n"
            "  \"prune_target_size\": xxxxxx,  (numeric) the target size used by pruning (only present if automatic pruning is enabled)\n"
            "  \"softforks\": [            (array) status of softforks in progress\n"
            "     {\n"
            "        \"id\": \"xxxx\",        (string) name of softfork\n"
            "        \"version\": xx,         (numeric) block version\n"
            "        \"reject\": {            (object) progress toward rejecting pre-softfork blocks\n"
            "           \"status\": xx,       (boolean) true if threshold reached\n"
            "        },\n"
            "     }, ...\n"
            "  ],\n"
            "  \"bip9_softforks\": {          (object) status of BIP9 softforks in progress\n"
            "     \"xxxx\" : {                (string) name of the softfork\n"
            "        \"status\": \"xxxx\",    (string) one of \"defined\", \"started\", \"locked_in\", \"active\", \"failed\"\n"
            "        \"bit\": xx,             (numeric) the bit (0-28) in the block version field used to signal this softfork (only for \"started\" status)\n"
            "        \"startTime\": xx,       (numeric) the minimum median time past of a block at which the bit gains its meaning\n"
            "        \"timeout\": xx,         (numeric) the median time past of a block at which the deployment is considered failed if not yet locked in\n"
            "        \"since\": xx,           (numeric) height of the first block to which the status applies\n"
            "        \"statistics\": {        (object) numeric statistics about BIP9 signalling for a softfork (only for \"started\" status)\n"
            "           \"period\": xx,       (numeric) the length in blocks of the BIP9 signalling period \n"
            "           \"threshold\": xx,    (numeric) the number of blocks with the version bit set required to activate the feature \n"
            "           \"elapsed\": xx,      (numeric) the number of blocks elapsed since the beginning of the current period \n"
            "           \"count\": xx,        (numeric) the number of blocks with the version bit set in the current period \n"
            "           \"possible\": xx      (boolean) returns false if there are not enough blocks left in this period to pass activation threshold \n"
            "        }\n"
            "     }\n"
            "  }\n"
            "  \"warnings\" : \"...\",         (string) any network and blockchain warnings.\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getblockchaininfo", "")
            + HelpExampleRpc("getblockchaininfo", "")
        };

    LOCK(cs_main);

    UniValue obj{UniValue::VOBJ};
    obj.push_back(Pair("chain",                 Params().NetworkIDString()));
    obj.push_back(Pair("blocks",                static_cast<int>(chainActive.Height())));
    obj.push_back(Pair("headers",               pindexBestHeader ? pindexBestHeader->nHeight : -1));
    obj.push_back(Pair("bestblockhash",         chainActive.Tip()->GetBlockHash().GetHex()));
    obj.push_back(Pair("difficulty",            static_cast<double>(GetDifficulty())));
    obj.push_back(Pair("mediantime",            static_cast<int64_t>(chainActive.Tip()->GetMedianTimePast())));
    obj.push_back(Pair("verificationprogress",  GuessVerificationProgress(Params().TxData(), chainActive.Tip())));
    obj.push_back(Pair("chainwork",             chainActive.Tip()->nChainWork.GetHex()));
    obj.push_back(Pair("size_on_disk",          CalculateCurrentUsage()));
    obj.push_back(Pair("pruned",                fPruneMode));
    if (fPruneMode) {
        CBlockIndex* block = chainActive.Tip();
        assert(block);
        while (block->pprev && (block->pprev->nStatus & BLOCK_HAVE_DATA)) {
            block = block->pprev;
        }

        obj.push_back(Pair("pruneheight",        block->nHeight));

        // if 0, execution bypasses the whole if block.
        bool automatic_pruning = (gArgs.GetArg("-prune", 0) != 1);
        obj.push_back(Pair("automatic_pruning",  automatic_pruning));
        if (automatic_pruning) {
            obj.push_back(Pair("prune_target_size",  nPruneTarget));
        }
    }

    const auto& consensusParams = Params().GetConsensus();
    const auto tip = chainActive.Tip();
    UniValue softforks{UniValue::VARR};
    UniValue bip9_softforks{UniValue::VOBJ};
    softforks.push_back(SoftForkDesc("bip34", 2, tip, consensusParams));
    softforks.push_back(SoftForkDesc("bip66", 3, tip, consensusParams));
    softforks.push_back(SoftForkDesc("bip65", 4, tip, consensusParams));
    BIP9SoftForkDescPushBack(bip9_softforks, "csv", consensusParams, Consensus::DEPLOYMENT_CSV);
    BIP9SoftForkDescPushBack(bip9_softforks, "segwit", consensusParams, Consensus::DEPLOYMENT_SEGWIT);
    obj.push_back(Pair("softforks",             softforks));
    obj.push_back(Pair("bip9_softforks", bip9_softforks));

    obj.push_back(Pair("warnings", GetWarnings("statusbar")));
    return obj;
}

/** Comparison function for sorting the getchaintips heads.  */
struct CompareBlocksByHeight
{
    bool operator()(const CBlockIndex* a, const CBlockIndex* b) const
    {
        /* Make sure that unequal blocks with the same height do not compare
           equal. Use the pointers themselves to make a distinction. */

        if (a->nHeight != b->nHeight)
          return (a->nHeight > b->nHeight);

        return a < b;
    }
};

UniValue getchaintips(const JSONRPCRequest& request)
{
    if (request.fHelp || !request.params.empty())
        throw std::runtime_error{
            "getchaintips\n"
            "Return information about all known tips in the block tree,"
            " including the main chain as well as orphaned branches.\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"height\": xxxx,         (numeric) height of the chain tip\n"
            "    \"hash\": \"xxxx\",         (string) block hash of the tip\n"
            "    \"branchlen\": 0          (numeric) zero for main chain\n"
            "    \"status\": \"active\"      (string) \"active\" for the main chain\n"
            "  },\n"
            "  {\n"
            "    \"height\": xxxx,\n"
            "    \"hash\": \"xxxx\",\n"
            "    \"branchlen\": 1          (numeric) length of branch connecting the tip to the main chain\n"
            "    \"status\": \"xxxx\"        (string) status of the chain (active, valid-fork, valid-headers, headers-only, invalid)\n"
            "  }\n"
            "]\n"
            "Possible values for status:\n"
            "1.  \"invalid\"               This branch contains at least one invalid block\n"
            "2.  \"headers-only\"          Not all blocks for this branch are available, but the headers are valid\n"
            "3.  \"valid-headers\"         All blocks are available for this branch, but they were never fully validated\n"
            "4.  \"valid-fork\"            This branch is not part of the active chain, but is fully validated\n"
            "5.  \"active\"                This is the tip of the active main chain, which is certainly valid\n"
            "\nExamples:\n"
            + HelpExampleCli("getchaintips", "")
            + HelpExampleRpc("getchaintips", "")
        };

    LOCK(cs_main);

    /*
     * Idea:  the set of chain tips is chainActive.tip, plus orphan blocks which do not have another orphan building off of them.
     * Algorithm:
     *  - Make one pass through mapBlockIndex, picking out the orphan blocks, and also storing a set of the orphan block's pprev pointers.
     *  - Iterate through the orphan blocks. If the block isn't pointed to by another orphan, it is a chain tip.
     *  - add chainActive.Tip()
     */
    std::set<const CBlockIndex*, CompareBlocksByHeight> setTips;
    std::set<const CBlockIndex*> setOrphans;
    std::set<const CBlockIndex*> setPrevs;

    for (const auto& item : mapBlockIndex)
    {
        if (!chainActive.Contains(item.second)) {
            setOrphans.insert(item.second);
            setPrevs.insert(item.second->pprev);
        }
    }

    for (auto it = setOrphans.begin(); it != setOrphans.end(); ++it)
    {
        if (setPrevs.erase(*it) == 0) {
            setTips.insert(*it);
        }
    }

    // Always report the currently active tip.
    setTips.insert(chainActive.Tip());

    /* Construct the output array.  */
    UniValue res{UniValue::VARR};
    for (const auto block : setTips)
    {
        UniValue obj{UniValue::VOBJ};
        obj.push_back(Pair("height", block->nHeight));
        obj.push_back(Pair("hash", block->phashBlock->GetHex()));

        const int branchLen{block->nHeight - chainActive.FindFork(block)->nHeight};
        obj.push_back(Pair("branchlen", branchLen));

        std::string status;
        if (chainActive.Contains(block)) {
            // This block is part of the currently active chain.
            status = "active";
        } else if (block->nStatus & BLOCK_FAILED_MASK) {
            // This block or one of its ancestors is invalid.
            status = "invalid";
        } else if (block->nChainTx == 0) {
            // This block cannot be connected because full block data for it or one of its parents is missing.
            status = "headers-only";
        } else if (block->IsValid(BLOCK_VALID_SCRIPTS)) {
            // This block is fully validated, but no longer part of the active chain. It was probably the active block once, but was reorganized.
            status = "valid-fork";
        } else if (block->IsValid(BLOCK_VALID_TREE)) {
            // The headers for this block are valid, but it has not been validated. It was probably never part of the most-work chain.
            status = "valid-headers";
        } else {
            // No clue.
            status = "unknown";
        }
        obj.push_back(Pair("status", status));

        res.push_back(obj);
    }

    return res;
}

UniValue mempoolInfoToJSON()
{
    UniValue ret{UniValue::VOBJ};
    ret.push_back(Pair("size", static_cast<int64_t>(mempool.size())));
    ret.push_back(Pair("bytes", static_cast<int64_t>(mempool.GetTotalTxSize())));
    ret.push_back(Pair("usage", static_cast<int64_t>(mempool.DynamicMemoryUsage())));
    const auto maxmempool = gArgs.GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000;
    ret.push_back(Pair("maxmempool", static_cast<int64_t>(maxmempool)));
    ret.push_back(Pair("mempoolminfee", ValueFromAmount(mempool.GetMinFee(maxmempool).GetFeePerK())));

    return ret;
}

UniValue getmempoolinfo(const JSONRPCRequest& request)
{
    if (request.fHelp || !request.params.empty())
        throw std::runtime_error{
            "getmempoolinfo\n"
            "\nReturns details on the active state of the TX memory pool.\n"
            "\nResult:\n"
            "{\n"
            "  \"size\": xxxxx,               (numeric) Current tx count\n"
            "  \"bytes\": xxxxx,              (numeric) Sum of all virtual transaction sizes as defined in BIP 141. Differs from actual serialized size because witness data is discounted\n"
            "  \"usage\": xxxxx,              (numeric) Total memory usage for the mempool\n"
            "  \"maxmempool\": xxxxx,         (numeric) Maximum memory usage for the mempool\n"
            "  \"mempoolminfee\": xxxxx       (numeric) Minimum fee rate in " + CURRENCY_UNIT + "/kB for tx to be accepted\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getmempoolinfo", "")
            + HelpExampleRpc("getmempoolinfo", "")
        };

    return mempoolInfoToJSON();
}

UniValue preciousblock(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error{
            "preciousblock \"blockhash\"\n"
            "\nTreats a block as if it were received before others with the same work.\n"
            "\nA later preciousblock call can override the effect of an earlier one.\n"
            "\nThe effects of preciousblock are not retained across restarts.\n"
            "\nArguments:\n"
            "1. \"blockhash\"   (string, required) the hash of the block to mark as precious\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("preciousblock", "\"blockhash\"")
            + HelpExampleRpc("preciousblock", "\"blockhash\"")
        };

    const auto hash = uint256S(request.params[0].get_str());

    CBlockIndex* pblockindex{nullptr};
    {
        LOCK(cs_main);
        if (mapBlockIndex.count(hash) == 0)
            throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Block not found");

        pblockindex = mapBlockIndex[hash];
    }

    CValidationState state;
    PreciousBlock(state, Params(), pblockindex);

    if (!state.IsValid()) {
        throw JSONRPCError(RPCErrorCode::DATABASE_ERROR, state.GetRejectReason());
    }

    return NullUniValue;
}

UniValue invalidateblock(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error{
            "invalidateblock \"blockhash\"\n"
            "\nPermanently marks a block as invalid, as if it violated a consensus rule.\n"
            "\nArguments:\n"
            "1. \"blockhash\"   (string, required) the hash of the block to mark as invalid\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("invalidateblock", "\"blockhash\"")
            + HelpExampleRpc("invalidateblock", "\"blockhash\"")
        };

    const auto hash = uint256S(request.params[0].get_str());

    CValidationState state;
    {
        LOCK(cs_main);
        if (mapBlockIndex.count(hash) == 0)
            throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Block not found");

        InvalidateBlock(state, Params(), mapBlockIndex[hash]);
    }

    if (state.IsValid()) {
        ActivateBestChain(state, Params());
    }

    if (!state.IsValid()) {
        throw JSONRPCError(RPCErrorCode::DATABASE_ERROR, state.GetRejectReason());
    }

    return NullUniValue;
}

UniValue reconsiderblock(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error{
            "reconsiderblock \"blockhash\"\n"
            "\nRemoves invalidity status of a block and its descendants, reconsider them for activation.\n"
            "This can be used to undo the effects of invalidateblock.\n"
            "\nArguments:\n"
            "1. \"blockhash\"   (string, required) the hash of the block to reconsider\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("reconsiderblock", "\"blockhash\"")
            + HelpExampleRpc("reconsiderblock", "\"blockhash\"")
        };

    const auto hash = uint256S(request.params[0].get_str());

    {
        LOCK(cs_main);
        if (mapBlockIndex.count(hash) == 0)
            throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Block not found");

        ResetBlockFailureFlags(mapBlockIndex[hash]);
    }

    CValidationState state;
    ActivateBestChain(state, Params());

    if (!state.IsValid()) {
        throw JSONRPCError(RPCErrorCode::DATABASE_ERROR, state.GetRejectReason());
    }

    return NullUniValue;
}

UniValue getchaintxstats(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 2)
        throw std::runtime_error{
            "getchaintxstats ( nblocks blockhash )\n"
            "\nCompute statistics about the total number and rate of transactions in the chain.\n"
            "\nArguments:\n"
            "1. nblocks      (numeric, optional) Size of the window in number of blocks (default: one month).\n"
            "2. \"blockhash\"  (string, optional) The hash of the block that ends the window.\n"
            "\nResult:\n"
            "{\n"
            "  \"time\": xxxxx,                (numeric) The timestamp for the final block in the window in UNIX format.\n"
            "  \"txcount\": xxxxx,             (numeric) The total number of transactions in the chain up to that point.\n"
            "  \"window_block_count\": xxxxx,  (numeric) Size of the window in number of blocks.\n"
            "  \"window_tx_count\": xxxxx,     (numeric) The number of transactions in the window. Only returned if \"window_block_count\" is > 0.\n"
            "  \"window_interval\": xxxxx,     (numeric) The elapsed time in the window in seconds. Only returned if \"window_block_count\" is > 0.\n"
            "  \"txrate\": x.xx,               (numeric) The average rate of transactions per second in the window. Only returned if \"window_interval\" is > 0.\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getchaintxstats", "")
            + HelpExampleRpc("getchaintxstats", "2016")
        };

    auto blockcount = static_cast<int>(30 * 24 * 60 * 60 / Params().GetConsensus().nPowTargetSpacing); // By default: 1 month

    const auto havehash = !request.params[1].isNull();
    uint256 hash;
    if (havehash) {
        hash = uint256S(request.params[1].get_str());
    }

    const CBlockIndex* pindex{nullptr};
    {
        LOCK(cs_main);
        if (havehash) {
            auto it = mapBlockIndex.find(hash);
            if (it == mapBlockIndex.end()) {
                throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Block not found");
            }
            pindex = it->second;
            if (!chainActive.Contains(pindex)) {
                throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Block is not in main chain");
            }
        } else {
            pindex = chainActive.Tip();
        }
    }
    
    assert(pindex != nullptr);

    if (request.params[0].isNull()) {
        blockcount = std::max(0, std::min(blockcount, pindex->nHeight - 1));
    } else {
        blockcount = request.params[0].get_int();

        if (blockcount < 0 || (blockcount > 0 && blockcount >= pindex->nHeight)) {
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid block count: should be between 0 and the block's height - 1");
        }
    }

    const auto pindexPast = pindex->GetAncestor(pindex->nHeight - blockcount);
    const auto nTimeDiff = static_cast<int>(pindex->GetMedianTimePast() - pindexPast->GetMedianTimePast());
    const auto nTxDiff = static_cast<int>(pindex->nChainTx - pindexPast->nChainTx);

    UniValue ret{UniValue::VOBJ};
    ret.push_back(Pair("time", static_cast<int64_t>(pindex->nTime)));
    ret.push_back(Pair("txcount", static_cast<int64_t>(pindex->nChainTx)));
    ret.push_back(Pair("window_block_count", blockcount));
    if (blockcount > 0) {
        ret.push_back(Pair("window_tx_count", nTxDiff));
        ret.push_back(Pair("window_interval", nTimeDiff));
        if (nTimeDiff > 0) {
            ret.push_back(Pair("txrate", static_cast<double>(nTxDiff) / nTimeDiff));
        }
    }

    return ret;
}

UniValue savemempool(const JSONRPCRequest& request)
{
    if (request.fHelp || !request.params.empty()) {
        throw std::runtime_error{
            "savemempool\n"
            "\nDumps the mempool to disk.\n"
            "\nExamples:\n"
            + HelpExampleCli("savemempool", "")
            + HelpExampleRpc("savemempool", "")
        };
    }

    if (!DumpMempool()) {
        throw JSONRPCError(RPCErrorCode::MISC_ERROR, "Unable to dump mempool to disk");
    }

    return NullUniValue;
}

UniValue getcoinsupply(const JSONRPCRequest& request)
{
    if (request.fHelp || !request.params.empty())
        throw std::runtime_error{
            "getcoinsupply\n"
            "\nReturns current total coin supply.\n"
            "Note this call may take some time.\n"
            "\nResult:\n"
            "\n"
            "  \"supply\":n,     (numeric) Current coin supply\n"
            "\nExamples:\n"
            + HelpExampleCli("getcoinsupply", "")
            + HelpExampleRpc("getcoinsupply", "")
        };

    UniValue ret{UniValue::VNUM};

    CCoinsStats stats;
    FlushStateToDisk();
    if (GetUTXOStats(pcoinsdbview, stats)) {
        ret = ValueFromAmount(stats.nTotalAmount);
    } else {
        throw JSONRPCError(RPCErrorCode::INTERNAL_ERROR, "Unable to read UTXO set");
    }
    return ret;
}

UniValue getbestblock(const JSONRPCRequest& request)
{
    if (request.fHelp || !request.params.empty())
        throw std::runtime_error{
            "getbestblock\n"
            "\nReturns the best (tip) block in the longest blockchain.\n"
            "\nResult:\n"
            "{\n"
            "  \"hash\"      (string)  the best block hash hex encoded\n"
            "  \"height\"    (numeric) the best block height\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getbestblock", "")
            + HelpExampleRpc("getbestblock", "")
        };

    LOCK(cs_main);

    auto tip = chainActive.Tip();
    auto height = tip->nHeight;
    auto hash = tip->GetBlockHash().GetHex();

    UniValue res{UniValue::VOBJ};
    res.push_back(Pair("hash", hash));
    res.push_back(Pair("height", height));

    return res;
}

UniValue getcurrentnet(const JSONRPCRequest& request)
{
    if (request.fHelp || !request.params.empty())
        throw std::runtime_error{
            "getcurrentnet\n"
            "\nReturns the current network ID.\n"
            "\nResult:\n"
            "\"networkID\"      (string)  the current network ID\n"
            "\nExamples:\n"
            + HelpExampleCli("getcurrentnet", "")
            + HelpExampleRpc("getcurrentnet", "")
        };

    LOCK(cs_main);

    return Params().NetworkIDString();
}

UniValue version(const JSONRPCRequest& request)
{
    if (request.fHelp || !request.params.empty())
        throw std::runtime_error{
            "version\n"
            "\nReturns the JSON-RPC version.\n"
            "\nResult:\n"
            "{\n"
            "  \"versionstring\"    (string)  The full version string\n"
            "  \"major\"            (numeric) The major version\n"
            "  \"minor\"            (numeric) The minor version\n"
            "  \"patch\"            (numeric) The patch version\n"
            "  \"prerelease\"       (string)  The prerelease version\n"
            "  \"buildmetadata\"    (string)  The build metadata\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("version", "")
            + HelpExampleRpc("version", "")
        };
    
    UniValue versionResult{UniValue::VOBJ};

    versionResult.push_back(Pair("versionstring", FormatFullVersion()));
    versionResult.push_back(Pair("major", CLIENT_VERSION_MAJOR));
    versionResult.push_back(Pair("minor", CLIENT_VERSION_MINOR));
    versionResult.push_back(Pair("patch", CLIENT_VERSION_REVISION));
    versionResult.push_back(Pair("prerelease", ""));
    versionResult.push_back(Pair("buildmetadata", FormatFullVersion()));

    return versionResult;
}

static std::string GetProxyInfo()
{
    UniValue networks{UniValue::VARR};
    for(int n=0; n<NET_MAX; ++n)
    {
        const auto network = static_cast<enum Network>(n);
        if(network == NET_UNROUTABLE || network == NET_INTERNAL)
            continue;

        proxyType proxy;
        UniValue obj{UniValue::VOBJ};
        GetProxy(network, proxy);
        if (proxy.IsValid()) {
            auto proxyStr = proxy.proxy.ToStringIPPort();
            if (!proxyStr.empty()) {
                return proxyStr;
            }
        }
    }

    return "";
}

UniValue getinfo(const JSONRPCRequest& request)
{
    if (request.fHelp || !request.params.empty())
        throw std::runtime_error{
            "getinfo\n"
            "\nReturns a JSON object containing various state info.\n"
            "\nResult:\n"
            "{\n"
            "  \"version\"          (numeric)  The version of the server\n"
            "  \"protocolversion\"  (numeric)  The latest supported protocol version\n"
            "  \"blocks\"           (numeric)  The number of blocks processed\n"
            "  \"timeoffset\"       (numeric)  The time offset\n"
            "  \"connections\"      (numeric)  The number of connected peers\n"
            "  \"proxy\"            (string)   The proxy used by the server\n"
            "  \"difficulty\"       (float)    The proxy used by the server\n"
            "  \"testnet\"          (bool)     Whether or not server is using testnet\n"
            "  \"relayfee\"         (float)    The minimum relay fee for non-free transactions in Satoshis/KB\n"
            "  \"errors\"           (string)   Any current errors\n"
            "}"
            "\nExamples:\n"
            + HelpExampleCli("getinfo", "")
            + HelpExampleRpc("getinfo", "")
        };
    
    auto numConnections = 0;
    if (g_connman) {
        numConnections = static_cast<int>(g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL));
    }
    bool isTestNet = (Params().NetworkIDString() == BaseParams().TESTNET);

    LOCK(cs_main);
    UniValue stateResult{UniValue::VOBJ};
    stateResult.push_back(Pair("version", CLIENT_VERSION));
    stateResult.push_back(Pair("protocolversion", PROTOCOL_VERSION));
    stateResult.push_back(Pair("blocks", chainActive.Height()));
    stateResult.push_back(Pair("timeoffset", GetTimeOffset()));
    stateResult.push_back(Pair("connections", numConnections));
    stateResult.push_back(Pair("proxy", GetProxyInfo()));
    stateResult.push_back(Pair("difficulty", GetDifficulty()));
    stateResult.push_back(Pair("testnet", isTestNet));
    stateResult.push_back(Pair("relayfee", ValueFromAmount(::minRelayTxFee.GetFeePerK())));
    stateResult.push_back(Pair("errors", ""));

    return stateResult;
}

CAmount computeTransactionFee(CTransaction const& tx)
{
    auto valueIn = CAmount{0};
    for (const auto& input : tx.vin){
        CTransactionRef input_tx;
        uint256 hashBlock;
        if (!GetTransaction(input.prevout.hash, input_tx, Params().GetConsensus(), hashBlock, false, false))
            throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, std::string("No such blockchain transaction. Use -txindex to enable blockchain transaction queries."));
        valueIn += input_tx->vout[input.prevout.n].nValue;
    }
    const auto& valueOut = tx.GetValueOut();

    const auto& fee_rate = CFeeRate(valueIn - valueOut, tx.GetTotalSize());
    return fee_rate.GetFeePerK();
}

UniValue ComputeBlocksTxFees(uint32_t startBlockHeight, uint32_t endBlockHeight, ETxClass txClass)
{
    uint32_t currHeightU = static_cast<uint32_t>(chainActive.Tip()->nHeight);
    if (startBlockHeight > currHeightU) {
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid starting block height");
    }
    if (endBlockHeight <= startBlockHeight || endBlockHeight > (currHeightU + 1)) {
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid ending block height");
    }

    std::vector<CAmount> txFees;

    for (auto currHeight = startBlockHeight; currHeight < endBlockHeight; ++currHeight) {
        auto blockIndex = chainActive[currHeight];
        if (!blockIndex) {
            continue;
        }

        CBlock block;
        if (!ReadBlockFromDisk(block, blockIndex, Params().GetConsensus())) {
            continue;
        }

        auto vtx = StakeSlice(block.vtx, txClass);
        for (const auto& tx : vtx) {
            txFees.push_back(computeTransactionFee(*tx));
        }
    }

    return FormatTxFeesInfo(txFees);
}

UniValue computeMempoolTxFees()
{
    std::vector<CAmount> txFees;
    auto& tx_class_index = mempool.mapTx.get<tx_class>();
    auto regularTxs = tx_class_index.equal_range(ETxClass::TX_Regular);
    for (auto regular_tx_iter = regularTxs.first; regular_tx_iter != regularTxs.second; ++regular_tx_iter) {
        auto txiter = mempool.mapTx.project<0>(regular_tx_iter);
        const auto& fee_rate = CFeeRate(txiter->GetModifiedFee(), txiter->GetTxSize());
        txFees.push_back(fee_rate.GetFeePerK());
    }

    return FormatTxFeesInfo(txFees);
}

UniValue txfeeinfo(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 3)
        throw std::runtime_error{
            "txfeeinfo ( blocks rangestart rangeend )\n"
            "\nGet various information about regular transaction fees from the mempool, blocks, and difficulty windows.\n"
            "\nArguments:\n"
            "1. blocks      (numeric, optional) The number of blocks to calculate transaction fees for, starting from the end of the tip moving backwards.\n"
            "2. rangestart  (numeric, optional) The start height of the block range to calculate transaction fees for.\n"
            "3. rangeend    (numeric, optional) The end height of the block range to calculate transaction fees for.\n"
            "\nResult:\n"
            "{\n"
            "  \"feeinfomempool\":  Transaction fee information for all regular transactions in the mempool\n"
            "  {\n"
            "       \"number\": (numeric)   Number of transactions in the mempool\n"
            "       \"min\":    (numeric)   Minimum transaction fee in the mempool\n"
            "       \"max\":    (numeric)   Maximum transaction fee in the mempool\n"
            "       \"mean\":   (numeric)   Mean of transaction fees in the mempool\n"
            "       \"median\": (numeric)   Median of transaction fees in the mempool\n"
            "       \"stddev\": (numeric)   Standard deviation of transaction fees in the mempool\n"
            "  },\n"
            "  \"feeinfoblocks\":  Ticket fee information for a given list of blocks descending from the chain tip (units: PAIcoin/kB)\n"
            "  [\n"
            "       {\n"
            "            \"height\": (numeric)   Height of the block\n"
            "            \"number\": (numeric)   Number of transactions in the block\n"
            "            \"min\":    (numeric)   Minimum transaction fee in the block\n"
            "            \"max\":    (numeric)   Maximum transaction fee in the block\n"
            "            \"mean\":   (numeric)   Mean of transaction fees in the block\n"
            "            \"median\": (numeric)   Median of transaction fees in the block\n"
            "            \"stddev\": (numeric)   Standard deviation of transaction fees in the block\n"
            "       },\n"
            "  ],\n"
            "  \"feeinforange\":  Ticket fee information for a window period where the stake difficulty was the same (units: PAIcoin/kB)\n"
            "  {\n"
            "       \"number\": (numeric)   Number of transactions in the window\n"
            "       \"min\":    (numeric)   Minimum transaction fee in the window\n"
            "       \"max\":    (numeric)   Maximum transaction fee in the window\n"
            "       \"mean\":   (numeric)   Mean of transaction fees in the window\n"
            "       \"median\": (numeric)   Median of transaction fees in the window\n"
            "       \"stddev\": (numeric)   Standard deviation of transaction fees in the window\n"
            "  }\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("txfeeinfo", "")
            + HelpExampleRpc("txfeeinfo", "")
            + HelpExampleCli("txfeeinfo", "3 5 7")
            + HelpExampleRpc("txfeeinfo", "3 5 7")
        };
    
    auto blocksTip = chainActive.Tip();
    auto currHeight = static_cast<uint32_t>(blocksTip->nHeight);

    uint32_t blocks = 0;
    uint32_t rangeStart = 0;
    uint32_t rangeEnd = 0;
    if (!request.params[0].isNull()) {
        blocks = static_cast<uint32_t>(request.params[0].get_int());
    }
    if (!request.params[1].isNull()) {
        rangeStart = static_cast<uint32_t>(request.params[1].get_int());
    } else {
        rangeStart = currHeight - 1; // TODO: -1 here is a placeholder for windowdiffsize
    }
    if (!request.params[2].isNull()) {
        rangeEnd = static_cast<uint32_t>(request.params[2].get_int());
    } else {
        rangeEnd = currHeight;
    }

    // Validations
    if (blocks > currHeight) {
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid parameter for blocks");
    }
    if (rangeStart > rangeEnd) {
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Range start is larger than range end");
    }
    if (rangeEnd > currHeight) {
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Range end is out of bounds");
    }

    UniValue mempoolFees = computeMempoolTxFees();

    UniValue blockFees{UniValue::VARR};
    if (blocks > 0) {
        auto start = currHeight;
        auto end = currHeight - blocks;

        for (auto i = start; i > end; --i) {
            auto blockFee = ComputeBlocksTxFees(i, i + 1, ETxClass::TX_Regular);
            blockFee.push_back(Pair("height", static_cast<int32_t>(i)));

            blockFees.push_back(blockFee);
        }
    }

    UniValue rangeFees = ComputeBlocksTxFees(rangeStart, rangeEnd, ETxClass::TX_Regular);

    UniValue txFeesResult{UniValue::VOBJ};
    txFeesResult.push_back(Pair("feeinfomempool", mempoolFees));
    txFeesResult.push_back(Pair("feeinfoblocks", blockFees));
    txFeesResult.push_back(Pair("feeinforange", rangeFees));
    return txFeesResult;
}

UniValue getblocksubsidy(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error{
            "getblocksubsidy ( height voters )\n"
            "\nReturns information regarding subsidy amounts.\n"
            "\nArguments:\n"
            "1. height      (numeric) The block height.\n"
            "2. voters      (numeric) The number of voters.\n"
            "\nResult:\n"
            "{\n"
            "   \"developer\":  (numeric)   The developer subsidy\n"
            "   \"pos\":        (numeric)   The Proof-of-Stake subsidy\n"
            "   \"pow\":        (numeric)   The Proof-of-Work subsidy\n"
            "   \"total\":      (numeric)   The total subsidy\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getblocksubsidy", "100 5")
            + HelpExampleRpc("getblocksubsidy", "100 5")
        };
    
    UniValue result{UniValue::VOBJ};
    result.push_back(Pair("developer", 0));
    result.push_back(Pair("pos", 0));
    result.push_back(Pair("pow", 0));
    result.push_back(Pair("total", 0));
    return result;
}

UniValue getcfilter(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error{
            "getcfilter ( hash filtertype )\n"
            "\nReturns the committed filter for a block.\n"
            "\nArguments:\n"
            "1. hash        (hash)   The block hash of the filter being queried.\n"
            "2. filtertype  (string) The type of committed filter to return.\n"
            "\nResult:\n"
            "{\n"
            "   \"filterbytes\":  (serialized bytes) The committed filter serialized with the N value and encoded as a hex string\n"
            "}\n"
        };
    
    UniValue result{UniValue::VSTR};
    result.setStr("000000");
    return result;
}

UniValue getcfilterheader(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error{
            "getcfilterheader ( hash filtertype )\n"
            "\nReturns the filter header hash committing to all filters in the chain up through a block.\n"
            "\nArguments:\n"
            "1. hash        (hash)   The block hash of the filter header being queried.\n"
            "2. filtertype  (string) The type of committed filter to return the header commitment for.\n"
            "\nResult:\n"
            "{\n"
            "   \"filterbytes\":  (serialized bytes) The filter header commitment hash\n"
            "}\n"
        };
    
    UniValue result{UniValue::VSTR};
    result.setStr("000000");
    return result;
}

UniValue getvoteinfo(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error{
            "getvoteinfo ( version )\n"
            "\nReturns the vote info statistics.\n"
            "\nArguments:\n"
            "1. version      (numeric) The stake version.\n"
            "\nResult:\n"
            "{\n"
            "  \"currentheight\":   (numeric) Top of the chain height.\n"
            "  \"startheight\":     (numeric) The start height of this voting window.\n"
            "  \"endheight\":       (numeric) The end height of this voting window.\n"
            "  \"hash\":            (hash)    The hash of the current height block.\n"
            "  \"voteversion\":     (numeric) Selected vote version.\n"
            "  \"quorum\":          (numeric) Minimum amount of votes required.\n"
            "  \"totalvotes\":      (numeric) Total votes.\n"
            "  \"agendas\":         All agendas for this stake version.\n"
            "  [\n"
            "       {\n"
            "            \"id\":                (string)  Unique identifier of this agenda.\n"
            "            \"description\":       (string)  Description of this agenda.\n"
            "            \"mask\":              (numeric) Agenda mask.\n"
            "            \"starttime\":         (numeric) Time agenda becomes valid.\n"
            "            \"expiretime\":        (numeric) Time agenda becomes invalid.\n"
            "            \"status\":            (string)  Agenda status.\n"
            "            \"quorumprogress\":    (float)   Progress of quorum reached.\n"
            "            \"choices\":           All choices in this agenda.\n"
            "            [\n"
            "                 {\n"
            "                      \"id\":          (string)  Unique identifier of this choice.\n"
            "                      \"description\": (string)  Description of this choice.\n"
            "                      \"bits\":        (numeric) Bits that identify this choice.\n"
            "                      \"isabstain\":   (bool)    This choice is to abstain from change.\n"
            "                      \"isno\":        (bool)    Hard no choice (1 and only 1 per agenda).\n"
            "                      \"count\":       (numeric) How many votes received.\n"
            "                      \"progress\":    (float)   Progress of the overall count.\n"
            "                 },\n"
            "            ]\n"
            "       },\n"
            "  ],\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getvoteinfo", "2")
            + HelpExampleRpc("getvoteinfo", "2")
        };
    
    UniValue resultChoice{UniValue::VOBJ};
    resultChoice.push_back(Pair("id", "choice"));
    resultChoice.push_back(Pair("description", "choice description"));
    resultChoice.push_back(Pair("bits", 0));
    resultChoice.push_back(Pair("isabstain", true));
    resultChoice.push_back(Pair("isno", true));
    resultChoice.push_back(Pair("count", 1));
    resultChoice.push_back(Pair("progress", 0));
    UniValue arrChoices{UniValue::VARR};
    arrChoices.push_back(resultChoice);

    UniValue resultAgenda{UniValue::VOBJ};
    resultAgenda.push_back(Pair("id", "agenda"));
    resultAgenda.push_back(Pair("description", "agenda description"));
    resultAgenda.push_back(Pair("mask", 0));
    resultAgenda.push_back(Pair("starttime", 0));
    resultAgenda.push_back(Pair("expiretime", 0));
    resultAgenda.push_back(Pair("status", "agenda status"));
    resultAgenda.push_back(Pair("quorumprogress", 0));
    resultAgenda.push_back(Pair("choices", arrChoices));
    UniValue arrAgendas{UniValue::VARR};
    arrAgendas.push_back(resultAgenda);

    UniValue result{UniValue::VOBJ};
    result.push_back(Pair("currentheight", -1));
    result.push_back(Pair("startheight", -1));
    result.push_back(Pair("endheight", -1));
    result.push_back(Pair("hash", "AB"));
    result.push_back(Pair("voteversion", 0));
    result.push_back(Pair("quorum", 0));
    result.push_back(Pair("totalvotes", 1));
    result.push_back(Pair("agendas", arrAgendas));

    return result;
}

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
  //  --------------------- ------------------------  -----------------------  ----------
    { "blockchain",         "getblockchaininfo",      &getblockchaininfo,      {} },
    { "blockchain",         "getchaintxstats",        &getchaintxstats,        {"nblocks", "blockhash"} },
    { "blockchain",         "getbestblockhash",       &getbestblockhash,       {} },
    { "blockchain",         "getbestblock",           &getbestblock,           {} },
    { "blockchain",         "getblockcount",          &getblockcount,          {} },
    { "blockchain",         "getblock",               &getblock,               {"blockhash","verbosity|verbose"} },
    { "blockchain",         "getblockhash",           &getblockhash,           {"height"} },
    { "blockchain",         "getblockheader",         &getblockheader,         {"blockhash","verbose"} },
    { "blockchain",         "getblocksubsidy",        &getblocksubsidy,        {"height","voters"} },
    { "blockchain",         "getcfilter",             &getcfilter,             {"hash","filtertype"} },
    { "blockchain",         "getcfilterheader",       &getcfilterheader,       {} },
    { "blockchain",         "getchaintips",           &getchaintips,           {} },
    { "blockchain",         "getcoinsupply",          &getcoinsupply,          {} },
    { "blockchain",         "getcurrentnet",          &getcurrentnet,          {} },
    { "blockchain",         "getdifficulty",          &getdifficulty,          {} },
    { "blockchain",         "getinfo",                &getinfo,                {} },
    { "blockchain",         "getmempoolancestors",    &getmempoolancestors,    {"txid","verbose"} },
    { "blockchain",         "getmempooldescendants",  &getmempooldescendants,  {"txid","verbose"} },
    { "blockchain",         "getmempoolentry",        &getmempoolentry,        {"txid"} },
    { "blockchain",         "getmempoolinfo",         &getmempoolinfo,         {} },
    { "blockchain",         "getrawmempool",          &getrawmempool,          {"verbose"} },
    { "blockchain",         "gettxout",               &gettxout,               {"txid","n","include_mempool"} },
    { "blockchain",         "gettxoutsetinfo",        &gettxoutsetinfo,        {} },
    { "blockchain",         "getvoteinfo",            &getvoteinfo,            {} },
    { "blockchain",         "pruneblockchain",        &pruneblockchain,        {"height"} },
    { "blockchain",         "savemempool",            &savemempool,            {} },
    { "blockchain",         "txfeeinfo",              &txfeeinfo,              {"blocks", "rangestart", "rangeend"} },
    { "blockchain",         "verifychain",            &verifychain,            {"checklevel","nblocks"} },
    { "blockchain",         "version",                &version,                {} },

    { "blockchain",         "preciousblock",          &preciousblock,          {"blockhash"} },

    /* Not shown in help */
    { "hidden",             "invalidateblock",        &invalidateblock,        {"blockhash"} },
    { "hidden",             "reconsiderblock",        &reconsiderblock,        {"blockhash"} },
    { "hidden",             "waitfornewblock",        &waitfornewblock,        {"timeout"} },
    { "hidden",             "waitforblock",           &waitforblock,           {"blockhash","timeout"} },
    { "hidden",             "waitforblockheight",     &waitforblockheight,     {"height","timeout"} },
};

void RegisterBlockchainRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx{0}; vcidx < ARRAYLEN(commands); ++vcidx)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
