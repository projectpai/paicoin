// Copyright (c) 2018 The PAIcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAICOIN_COINBASEINDEX_TXHANDLER_H
#define PAICOIN_COINBASEINDEX_TXHANDLER_H

#include "primitives/transaction.h"
#include "util.h"
#include <vector>

extern CAmount DEFAULT_COINBASE_TX_FEE;
extern const unsigned NUMBER_OF_CBINDEX_VOUTS;

class CKey;
class CPubKey;
class CWallet;
class uint160;
class CoinbaseAddress;
class CoinbaseIndex;

class CoinbaseIndexTxHandler
{
public:
    // We assume we receive a standard transaction here, i.e. a transaction having
    // at most one OP_RETURN output
    bool HasTransactionNewCoinbaseAddress(CTransactionRef const& tx);

    std::unique_ptr<CoinbaseAddress> ExtractNewCoinbaseAddrFromTransaction(
        CTransactionRef const& tx,
        CoinbaseIndex const& coinbaseIndex);

    CTransactionRef CreateNewCoinbaseAddrTransaction(
        const CWallet* wallet,
        uint160 const& targetAddress,
        int maxBlockHeight);

private:

    /**
     * Helper methods to create a new coinbase address payload and insert
     * it into an existing transaction.
     */
    bool FillTransactionWithCoinbaseNewAddress(CMutableTransaction& tx,
        uint160 const& targetAddress,
        int maxBlockHeight,
        const CKey& signKey);
    std::vector<unsigned char> BuildNewCoinbaseAddressPayload(
        uint160 const& newTargetAddress,
        int maxBlockHeight,
        CKey const& signPrivKey);

    /**
     * Helper methods to extract a new coinbase address payload from the enclosing transaction.
     * The signature is also verified against existing coinbase index public keys.
     */
    std::vector<unsigned char> GetCoinbaseAddressTransactionPayload(CTransactionRef const& tx);

    bool ExtractPayloadFields(std::vector<unsigned char> const& payload,
        uint160& targetAddress,
        int& maxBlockHeight,
        std::vector<unsigned char>& dataSignature);

    bool VerifyPayloadFieldsSignature(
        uint160 const& targetAddress,
        int const& maxBlockHeight,
        std::vector<unsigned char> const& dataSignature,
        std::vector<CPubKey> candidateKeys);

    /**
     * This only verifies basic preconditions, it is a non-comprehensive fast test.
     */
    bool HasTransactionNewCoinbaseAddressBasic(CTransactionRef const& tx);

private:
    std::vector<unsigned char> _preHeader{
        0x66,       // crc8('PAICB')
        0x10,       // version 1.0
        0x00        // reserved for future use
    };
};

#endif // PAICOIN_COINBASEINDEX_TXHANDLER_H
