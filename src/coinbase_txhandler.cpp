#include "base58.h"
#include "chain.h"
#include "chainparams.h"
#include "clientversion.h"
#include "coinbase_address.h"
#include "coinbase_addresses.h"
#include "coinbase_index.h"
#include "coinbase_keyhandler.h"
#include "coinbase_txhandler.h"
#include "coinbase_utils.h"
#include "key.h"
#include "pubkey.h"
#include "serialize.h"
#include "streams.h"
#include "txmempool.h"
#include "uint256.h"
#include "util.h"
#include "validation.h"
#include "wallet/wallet.h"

#include <boost/assert.hpp>
#include <boost/crc.hpp>
#include <map>
#include <memory>
#include <vector>

CAmount DEFAULT_COINBASE_TX_FEE(CENT);

uint16_t DataToCRC16(std::vector<unsigned char> const& data)
{
    boost::crc_16_type result;
    result.process_bytes(data.data(), data.size());
    return result.checksum();
}

bool IsValidCoinbaseOperationType(unsigned char operationType)
{
    return (operationType > COT_INVALID && operationType < COT_LAST);
}

CoinbaseOprPayload CoinbaseTxHandler::BuildNewAddressPayload(uint16_t newAddressIndex, uint160 const& targetAddress, int maxBlockHeight)
{
    CDataStream dataStream(SER_DISK, CLIENT_VERSION);
    dataStream << newAddressIndex;
    dataStream << targetAddress;
    dataStream << maxBlockHeight;

    return CoinbaseOprPayload(dataStream.begin(), dataStream.end());
}

CoinbaseOprPayload CoinbaseTxHandler::BuildSignedPayload(CKey const& signPrivKey, uint16_t newAddressIndex, CoinbaseOprPayload const& clearPayload)
{
    uint256 payloadToSignHash = Hash(clearPayload.begin(), clearPayload.end());
    CoinbaseOprPayload signedPayloadHash;
    signPrivKey.Sign(payloadToSignHash, signedPayloadHash);

    CDataStream dataStream(SER_DISK, CLIENT_VERSION);
    dataStream << newAddressIndex;
    dataStream.insert(dataStream.end(),
        reinterpret_cast<char*>(const_cast<unsigned char*>(signedPayloadHash.data())),
        reinterpret_cast<char*>(const_cast<unsigned char*>(signedPayloadHash.data())) + signedPayloadHash.size());

    return CoinbaseOprPayload(dataStream.begin(), dataStream.end());
}

bool CoinbaseTxHandler::ExtractPayloadFields(CoinbaseOprPayload const& payload,
    CoinbaseOperationType& operationType,
    uint16_t& newAddressIndex,
    uint160& targetAddress,
    int& maxBlockHeight,
    std::vector<unsigned char>& dataSignature)
{
    if (payload.size() < 27) {
        operationType = COT_ADD;
    } else {
        operationType = COT_SIGN;
    }

    try {
        CDataStream dataStream(payload, SER_DISK, CLIENT_VERSION);

        if (operationType == COT_ADD) {
            dataStream >> newAddressIndex;
            dataStream >> targetAddress;
            dataStream >> maxBlockHeight;
        } else if (operationType == COT_SIGN) {
            dataStream >> newAddressIndex;
            dataStream >> dataSignature;
            BOOST_ASSERT(dataSignature.size() == 72);
        }
    } catch (const std::exception& e) {
        LogPrintf("%s: could not extract payload fields; %s", __FUNCTION__, e.what());
        return false;
    }

    return true;
}

bool CoinbaseTxHandler::VerifyPayloadFieldsSignature(
    uint16_t const& newAddressIndex,
    uint160 const& targetAddress,
    std::vector<unsigned char> const& dataSignature,
    int const& maxBlockHeight,
    std::vector<CPubKey> candidateKeys)
{
    CDataStream signStream(SER_DISK, CLIENT_VERSION);
    signStream << newAddressIndex;
    signStream << targetAddress;
    signStream << maxBlockHeight;
    uint256 payloadToSignHash = Hash(signStream.begin(), signStream.end());

    for (auto const& pubKey : candidateKeys) {
        if (pubKey.Verify(payloadToSignHash, dataSignature)) {
            return true;
        }
    }

    return false;
}

void CoinbaseTxHandler::GetAssembledHeader(std::vector<unsigned char> const& payload,
    unsigned char operationType,
    std::vector<unsigned char>& preHeader,
    std::vector<unsigned char>& postHeader)
{
    preHeader = _preHeader;
    if (operationType == COT_ADD) {
        preHeader.push_back(0x00);
        preHeader.push_back(0x00);
    }

    std::vector<unsigned char> dataToCRC(preHeader.begin(), preHeader.end());
    dataToCRC.insert(dataToCRC.end(), payload.begin(), payload.end());

    CDataStream dataStream(SER_DISK, CLIENT_VERSION);
    dataStream << DataToCRC16(dataToCRC);

    postHeader.assign(dataStream.begin(), dataStream.end());
}

bool CoinbaseTxHandler::GetPayloadFromTrimmedHeader(std::vector<unsigned char> const& payloadWithHeader,
    std::vector<unsigned char>& payload,
    unsigned char& operationType)
{
    if (payloadWithHeader.size() < 31) {
        return false;
    }

    try {
        size_t preheaderSkipBytes = 0;
        if (payloadWithHeader.size() > 31) {
            operationType = COT_SIGN;
        } else {
            operationType = COT_ADD;
            preheaderSkipBytes = 2;
        }

        for (size_t idx = 0; idx < _preHeader.size(); ++idx) {
            if (payloadWithHeader[idx] != _preHeader[idx]) {
                // TODO: invalid payload header
                LogPrintf("%s: invalid payload header", __FUNCTION__);
                return false;
            }
        }

        if (operationType == COT_ADD) {
            BOOST_ASSERT(payloadWithHeader[_preHeader.size()] == 0x00);
            BOOST_ASSERT(payloadWithHeader[_preHeader.size() + 1] == 0x00);
        }

        CDataStream dataStream(SER_DISK, CLIENT_VERSION);
        const char* checksumBegin = reinterpret_cast<const char*>(payloadWithHeader.data() + payloadWithHeader.size() - sizeof(uint16_t));
        const char* checksumEnd = checksumBegin + sizeof(uint16_t);
        dataStream.insert(dataStream.begin(), checksumBegin, checksumEnd);

        uint16_t checksum = 0;
        dataStream >> checksum;

        std::vector<unsigned char> payloadWithPreHeader(payloadWithHeader.begin(), payloadWithHeader.end() - sizeof(uint16_t));
        uint16_t computedChecksum = DataToCRC16(payloadWithPreHeader);

        if (checksum != computedChecksum) {
            // TODO: payload fails checksum check
            LogPrintf("%s: payload fails checksum", __FUNCTION__);
            return false;
        }

        payload.assign(payloadWithPreHeader.begin() + _preHeader.size() + preheaderSkipBytes, payloadWithPreHeader.end());
    } catch (const std::exception& e) {
        LogPrintf("%s: could not extract payload and header; %s", __FUNCTION__, e.what());
        return false;
    }

    return true;
}

CoinbaseOprPayload CoinbaseTxHandler::FillTransactionWithCoinbaseNewAddress(CMutableTransaction& tx,
    uint16_t newAddressIndex,
    uint160 const& targetAddress,
    int maxBlockHeight)
{
    auto payload = BuildNewAddressPayload(newAddressIndex, targetAddress, maxBlockHeight);

    CoinbaseOprHeader preHeader;
    CoinbaseOprHeader postHeader;
    GetAssembledHeader(payload, COT_ADD, preHeader, postHeader);

    tx.vout[0].scriptPubKey << OP_RETURN << preHeader << payload << postHeader;
    return payload;
}

bool CoinbaseTxHandler::FillTransactionWithCoinbaseSignature(CMutableTransaction& tx,
    const CKey& signKey,
    uint16_t newAddressIndex,
    CoinbaseOprPayload const& payload)
{
    auto signedPayload = BuildSignedPayload(signKey, newAddressIndex, payload);

    CoinbaseOprHeader preHeader;
    CoinbaseOprHeader postHeader;
    GetAssembledHeader(signedPayload, COT_SIGN, preHeader, postHeader);

    tx.vout[0].scriptPubKey << OP_RETURN << preHeader << signedPayload << postHeader;
    return true;
}

struct CoinbaseMempoolTxDeleter
{
    std::unique_ptr<CMutableTransaction> tx;

    explicit CoinbaseMempoolTxDeleter(CMutableTransaction* _tx): tx(_tx) {}
    ~CoinbaseMempoolTxDeleter()
    {
        if (tx != nullptr)
        {
            mempool.removeRecursive(CTransaction(*tx));
        }
    }

    void reset()
    {
        tx.reset();
    }
};

CTransactionRef CoinbaseTxHandler::CreateCompleteCoinbaseTransaction(
    const CWallet* wallet,
    uint160 const& targetAddress,
    int maxBlockHeight)
{
    if (targetAddress.IsNull()) {
        LogPrintf("%s: target address is invalid", __FUNCTION__);
        return {};
    }

    if ((maxBlockHeight < -1) ||
        ((maxBlockHeight > -1) && (maxBlockHeight <= static_cast<int>(mapBlockIndex.size())))) {
        LogPrintf("%s: invalid maximum block height", __FUNCTION__);
        return {};
    }

    uint16_t newAddressIndex = static_cast<uint16_t>(gCoinbaseIndex.GetNumCoinbaseAddrs()) + 1;
    if (newAddressIndex < 1) {
        LogPrintf("%s: invalid new coinbase address index", __FUNCTION__);
        return {};
    }

    auto unspentInputsDataTx = SelectInputs(wallet, DEFAULT_COINBASE_TX_FEE);
    if (unspentInputsDataTx.empty()) {
        LogPrintf("%s: could not select inputs for coinbase transaction", __FUNCTION__);
        return {};
    }
    auto dataTx = CreateCoinbaseTransaction(unspentInputsDataTx, DEFAULT_COINBASE_TX_FEE, wallet);

    auto payload = FillTransactionWithCoinbaseNewAddress(dataTx, newAddressIndex, targetAddress, maxBlockHeight);
    if (payload.empty()) {
        LogPrintf("%s: invalid empty payload", __FUNCTION__);
        return {};
    }
    if (!SignCoinbaseTransaction(dataTx, wallet)) {
        LogPrintf("%s: could not sign the coinbase data transaction", __FUNCTION__);
        return {};
    }
    if (!SendCoinbaseTransactionToMempool(dataTx)) {
        LogPrintf("%s: could not send the coinbase data transaction to the mempool", __FUNCTION__);
        return {};
    }

    // Make sure if we fail to add the second transaction, we undo the first transaction as well
    CoinbaseMempoolTxDeleter dataTxMempoolDeleter(new CMutableTransaction(dataTx));

    CAmount totalBefore = -DEFAULT_COINBASE_TX_FEE;
    for (auto const& u : unspentInputsDataTx) {
        totalBefore += u.amount;
    }

    std::unique_ptr<CWalletTx> ptrGuard(new CWalletTx(wallet, MakeTransactionRef(dataTx)));
    UnspentInput unspentInputSigTx{ptrGuard.get(), 1, totalBefore, 1};

    auto sigTx = CreateCoinbaseTransaction({unspentInputSigTx}, DEFAULT_COINBASE_TX_FEE, wallet);
    
    CoinbaseKeyHandler cbKeyHandler(GetDataDir());
    auto signKey = cbKeyHandler.GetCoinbaseSigningKey();
    if (!signKey.IsValid()) {
        LogPrintf("%s: could not get the private key to generate the coinbase signature transaction", __FUNCTION__);
        return {};
    }

    if (!FillTransactionWithCoinbaseSignature(sigTx, signKey, newAddressIndex, payload)) {
        LogPrintf("%s: could not generate the coinbase signature transaction", __FUNCTION__);
        return {};
    }
    if (!SignCoinbaseTransaction(sigTx, wallet)) {
        LogPrintf("%s: could not sign the coinbase signature transaction", __FUNCTION__);
        return {};
    }
    if (!SendCoinbaseTransactionToMempool(sigTx)) {
        LogPrintf("%s: could not send the coinbase signature transaction to the mempool", __FUNCTION__);
        return {};
    }

    // Safe to release the first transaction deleter here, we submitted successfully
    // both transactions
    dataTxMempoolDeleter.reset();

    return MakeTransactionRef(dataTx);
}

bool CoinbaseTxHandler::IsTransactionCoinbaseAddress(CTransactionRef const& tx)
{
    auto payload = GetCoinbaseAddressTransactionPayload(tx);
    return !payload.empty();
}

std::vector<unsigned char> CoinbaseTxHandler::GetCoinbaseAddressTransactionPayload(CTransactionRef const& tx)
{
    std::vector<unsigned char> payload;
    unsigned char operationType;

    for (auto const& out : tx->vout) {
        auto scriptPubKey = out.scriptPubKey;
        if (scriptPubKey.empty()) {
            continue;
        }

        std::vector<unsigned char> pubKeyData(scriptPubKey.begin() + 1, scriptPubKey.end());
        auto payloadExtracted = GetPayloadFromTrimmedHeader(pubKeyData, payload, operationType);
        if (!payloadExtracted) {
            continue;
        }

        if (IsValidCoinbaseOperationType(operationType) && !payload.empty()) {
            return payload;
        }
    }

    return {};
}

std::unique_ptr<CoinbaseAddress> CoinbaseTxHandler::GetCoinbaseAddrFromTransactions(
    CTransactionRef const& tx1,
    CTransactionRef const& tx2,
    CoinbaseIndex const& coinbaseIndex,
    CoinbaseOperationType& tx1OpType,
    CoinbaseOperationType& tx2OpType)
{
    if (!tx1 || !tx2) {
        LogPrintf("%s: invalid input transactions", __FUNCTION__);
        return nullptr;
    }

    auto tx1Payload = GetCoinbaseAddressTransactionPayload(tx1);
    if (tx1Payload.empty()) {
        LogPrintf("%s: invalid payload for first transaction", __FUNCTION__);
        return nullptr;
    }

    auto tx2Payload = GetCoinbaseAddressTransactionPayload(tx2);
    if (tx2Payload.empty()) {
        LogPrintf("%s: invalid payload for second transaction", __FUNCTION__);
        return nullptr;
    }

    tx1OpType = COT_INVALID;
    uint16_t txNewAddressIndex = 0;
    uint160 txTargetAddress;
    int txMaxBlockHeight = -1;
    std::vector<unsigned char> txDataSignature;
    auto result = ExtractPayloadFields(tx1Payload, tx1OpType, txNewAddressIndex,
        txTargetAddress, txMaxBlockHeight, txDataSignature);
    if (!result) {
        LogPrintf("%s: could not extract payload data from first transaction", __FUNCTION__);
        return nullptr;
    }
    if (!IsValidCoinbaseOperationType(tx1OpType)) {
        LogPrintf("%s: invalid coinbase operation type for first transaction", __FUNCTION__);
        return nullptr;
    }

    tx2OpType = COT_INVALID;
    uint16_t txNewAddressIndex2 = 0;
    result = ExtractPayloadFields(tx2Payload, tx2OpType, txNewAddressIndex2,
        txTargetAddress, txMaxBlockHeight, txDataSignature);
    if (!result) {
        LogPrintf("%s: could not extract payload data from second transaction", __FUNCTION__);
        return nullptr;
    }

    bool areTxsPaired = (tx1OpType == COT_ADD && tx2OpType == COT_SIGN) ||
                        (tx1OpType == COT_SIGN && tx2OpType == COT_ADD);
    if (!areTxsPaired) {
        LogPrintf("%s: the input transactions are not complementary", __FUNCTION__);
        return nullptr;
    }

    if (txNewAddressIndex != (coinbaseIndex.GetNumCoinbaseAddrs() + 1)) {
        LogPrintf("%s: the intended coinbase transaction index value is invalid", __FUNCTION__);
        return nullptr;
    }
    if (txNewAddressIndex != txNewAddressIndex2) {
        LogPrintf("%s: the input transactions have different coinbase transaction index value", __FUNCTION__);
        return nullptr;
    }

    if ((txMaxBlockHeight > -1) && (static_cast<size_t>(txMaxBlockHeight) <= mapBlockIndex.size())) {
        LogPrintf("%s: the block height is invalid", __FUNCTION__);
        return nullptr;
    }

    auto defaultCoinbaseKeys = coinbaseIndex.GetDefaultCoinbaseKeys();
    if (defaultCoinbaseKeys.empty()) {
        LogPrintf("%s: could not retrieve the default (hard coded) keys", __FUNCTION__);
        return nullptr;
    }

    auto verifiedSignature = VerifyPayloadFieldsSignature(txNewAddressIndex, txTargetAddress,
        txDataSignature, txMaxBlockHeight, defaultCoinbaseKeys);
    if (!verifiedSignature) {
        LogPrintf("%s: the signature is invalid", __FUNCTION__);
        return nullptr;
    }

    auto newAddressHash = EncodeDestination(CKeyID(txTargetAddress));
    return std::unique_ptr<CoinbaseAddress>(new CoinbaseAddress{newAddressHash, CPubKey(), false, txMaxBlockHeight});
}
