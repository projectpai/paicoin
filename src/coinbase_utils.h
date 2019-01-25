// Copyright (c) 2018 The PAIcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAICOIN_COINBASE_UTILS_H
#define PAICOIN_COINBASE_UTILS_H

#include "amount.h"
#include "primitives/transaction.h"
#include "util.h"
#include <vector>

class CWallet;
class CWalletTx;

struct UnspentInput
{
    const CWalletTx* out;
    int outputNumber;
    CAmount amount;
    int confirmations;
};
typedef std::vector<UnspentInput> UnspentInputs;

/**
 * Selects unspent inputs from the wallet which together satisfy
 * the desired amount
 */
UnspentInputs SelectUnspentInputsFromWallet(const CWallet* wallet, CAmount desiredAmount);

/**
 * Create a new transaction which will advertise to the network
 * that nodes could create coinbase transactions against a new address.
 */
CMutableTransaction CreateNewCoinbaseAddressTransaction(const UnspentInputs& unspentInputs, const CWallet* wallet);

/**
 * Update a new coinbase transaction output amounts and fees, based on data usage.
 */
bool UpdateNewCoinbaseAddressTransactionFees(
    CMutableTransaction& rawTx,
    const UnspentInputs& unspentInputs);

/**
 * Signs a new coinbase address transaction to be introduced to the network
 */
bool SignNewCoinbaseAddressTransaction(CMutableTransaction& rawTx, const CWallet* wallet);

/**
 * Send a new coinbase address transaction to the mempool. The transaction
 * is expected to be signed.
 */
bool SendNewCoinbaseAddressTransactionToMempool(CMutableTransaction rawTx);

template <typename... Args>
void CoinbaseIndexLog(Args... params)
{
    LogPrint(BCLog::CBINDEX, params...);
}

/**
 * Split a block/vector of byte data into multiple blocks/vectors, sizes given as arguments.
 * Total requested size should be less or equal to the input block size.
 * If total requested size is less than the input block size, the remaining bytes are returned as is.
 * If any of the sizes argument is zero, there will be an empty block returned.
 * If the number of blocks is greater than the number of bytes in the input blocks, the input block will be returned as is.
 */
std::vector<std::vector<unsigned char>> PartitionDataBlock(
    std::vector<unsigned char> const& inputBlock,
    std::vector<size_t> const& sizes
);

/**
 * For a generic data block, compute the block sizes needed to fit the data
 * into transaction vouts. The first block size is fixed at 75 bytes.
 * The next blocks are fixed at 20 bytes, as the fake addresses (actually containing data)
 * could contain a uint160.
 */
std::vector<size_t> ComputeCoinbaseAddressPayloadBlockSizes(std::vector<unsigned char> const& payload);

#endif // PAICOIN_COINBASE_UTILS_H  
