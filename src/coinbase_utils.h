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
CMutableTransaction CreateNewCoinbaseAddressTransaction(const UnspentInputs& unspentInputs, CAmount txAmount, const CWallet* wallet);

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

#endif // PAICOIN_COINBASE_UTILS_H  
