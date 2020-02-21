// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAICOIN_RPC_BLOCKCHAIN_H
#define PAICOIN_RPC_BLOCKCHAIN_H

#include "amount.h"
#include "stake/staketx.h"
#include <vector>

class CBlock;
class CBlockIndex;
class UniValue;

/**
 * Get the difficulty of the net wrt to the given block index, or the chain tip if
 * not provided.
 *
 * @return A floating point number that is a multiple of the main net minimum
 * difficulty (4295032833 hashes).
 */
double GetDifficulty(const CBlockIndex* blockindex = nullptr);

/** Callback for when block tip changed. */
void RPCNotifyBlockChange(bool ibd, const CBlockIndex *);

/** Block description to JSON */
UniValue blockToJSON(const CBlock& block, const CBlockIndex* blockindex, bool txDetails = false);

/** Mempool information to JSON */
UniValue mempoolInfoToJSON();

/** Mempool to JSON */
UniValue mempoolToJSON(bool fVerbose = false);

/** Block header to JSON */
UniValue blockheaderToJSON(const CBlockIndex* blockindex);

CAmount ComputeMeanAmount(const std::vector<CAmount>& txFees);
CAmount ComputeMedianAmount(std::vector<CAmount> txFees);
CAmount ComputeStdDevAmount(const std::vector<CAmount>& txFees);
UniValue FormatTxFeesInfo(const std::vector<CAmount>& txFees);
UniValue ComputeBlocksTxFees(uint32_t startBlockHeight, uint32_t endBlockHeight, ETxClass txClass);

#endif

