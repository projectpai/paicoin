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

UnspentInputs SelectInputs(const CWallet* wallet, CAmount desiredAmount);
CMutableTransaction CreateCoinbaseTransaction(const UnspentInputs& unspentInputs, CAmount txAmount, const CWallet* wallet);
bool SignCoinbaseTransaction(CMutableTransaction& rawTx, const CWallet* wallet);
bool SendCoinbaseTransactionToMempool(CMutableTransaction rawTx);

template <typename... Args>
void CoinbaseIndexLog(Args... params)
{
    LogPrint(BCLog::CBINDEX, params...);
}

#endif // PAICOIN_COINBASE_UTILS_H  
