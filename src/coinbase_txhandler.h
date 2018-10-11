#ifndef PAICOIN_COINBASE_TXHANDLER_H
#define PAICOIN_COINBASE_TXHANDLER_H

#include "primitives/transaction.h"
#include "util.h"
#include <vector>

extern CAmount DEFAULT_COINBASE_TX_FEE;

class CKey;
class CPubKey;
class CWallet;
class uint160;
class CoinbaseAddress;
class CoinbaseIndex;

using CoinbaseOprPayload = std::vector<unsigned char>;
using CoinbaseOprHeader = std::vector<unsigned char>;

uint16_t DataToCRC16(std::vector<unsigned char> const& data);
bool IsValidCoinbaseOperationType(unsigned char operationType);

enum CoinbaseOperationType {
    COT_INVALID = 0,
    COT_ADD = 1,
    COT_SIGN = 2,
    COT_LAST
};

class CoinbaseTxHandler
{
public:
    // We assume we receive a standard transaction here, i.e. a transaction having
    // at most one OP_RETURN output
    bool IsTransactionCoinbaseAddress(CTransactionRef const& tx);

    std::unique_ptr<CoinbaseAddress> GetCoinbaseAddrFromTransactions(
        CTransactionRef const& tx1,
        CTransactionRef const& tx2,
        CoinbaseIndex const& coinbaseIndex,
        CoinbaseOperationType& tx1OpType,
        CoinbaseOperationType& tx2OpType);

    std::pair<CTransactionRef, CTransactionRef> CreateCompleteCoinbaseTransaction(
        const CWallet* wallet,
        uint160 const& targetAddress,
        int maxBlockHeight);

private:

    CoinbaseOprPayload BuildNewAddressPayload(uint16_t newAddressIndex, uint160 const& targetAddress, int maxBlockHeight);
    CoinbaseOprPayload BuildSignedPayload(CKey const& signPrivKey, uint16_t newAddressIndex, CoinbaseOprPayload const& clearPayload);

    bool ExtractPayloadFields(CoinbaseOprPayload const& payload,
        CoinbaseOperationType& operationType,
        uint16_t& newAddressIndex,
        uint160& targetAddress,
        int& maxBlockHeight,
        std::vector<unsigned char>& dataSignature);

    bool VerifyPayloadFieldsSignature(
        uint16_t const& newAddressIndex,
        uint160 const& targetAddress,
        std::vector<unsigned char> const& dataSignature,
        int const& maxBlockHeight,
        std::vector<CPubKey> candidateKeys);

    void GetAssembledHeader(std::vector<unsigned char> const& payload,
        unsigned char operationType,
        std::vector<unsigned char>& preHeader,
        std::vector<unsigned char>& postHeader);

    bool GetPayloadFromTrimmedHeader(std::vector<unsigned char> const& payloadWithHeader,
        std::vector<unsigned char>& payload,
        unsigned char& operationType);

    CoinbaseOprPayload FillTransactionWithCoinbaseNewAddress(CMutableTransaction& tx,
        uint16_t newAddressIndex,
        uint160 const& targetAddress,
        int maxBlockHeight);

    bool FillTransactionWithCoinbaseSignature(CMutableTransaction& tx,
        const CKey& signKey,
        uint16_t newAddressIndex,
        CoinbaseOprPayload const& payload);

    std::vector<unsigned char> GetCoinbaseAddressTransactionPayload(CTransactionRef const& tx);

private:
    std::vector<unsigned char> _preHeader{
        0x66,       // crc8('PAICB')
        0x10,       // version 1.0
        0x00, 0x00  // reserved for future use
    };
};

#endif // PAICOIN_COINBASE_TXHANDLER_H
