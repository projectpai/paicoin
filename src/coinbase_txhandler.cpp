// Copyright (c) 2018 The PAIcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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
#include "key_io.h"
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

/**
 * 1 rest output
 * 1 OP_RETURN
 * 2 fake addresses
 */
const unsigned NUMBER_OF_CBINDEX_VOUTS = 4;

uint32_t DataToCRC32(std::vector<unsigned char> const& data)
{
    boost::crc_32_type result;
    result.process_bytes(data.data(), data.size());
    return result.checksum();
}

bool CoinbaseIndexTxHandler::HasTransactionNewCoinbaseAddress(CTransactionRef const& tx)
{
    auto payload = GetCoinbaseAddressTransactionPayload(tx);
    return !payload.empty();
}

std::vector<unsigned char> CoinbaseIndexTxHandler::BuildNewCoinbaseAddressPayload(
    uint160 const& newTargetAddress,
    int maxBlockHeight,
    CKey const& signPrivKey)
{
    CDataStream signatureStream(SER_DISK, CLIENT_VERSION);
    signatureStream << newTargetAddress << maxBlockHeight;
    uint256 hashForSigning = Hash(signatureStream.begin(), signatureStream.end());

    std::vector<unsigned char> dataSignature;
    signPrivKey.Sign(hashForSigning, dataSignature);

    CDataStream payloadDataStream(SER_DISK, CLIENT_VERSION);
    payloadDataStream << newTargetAddress;
    payloadDataStream << maxBlockHeight;
    payloadDataStream << dataSignature;
    auto payloadCrc32 = DataToCRC32(std::vector<unsigned char>(payloadDataStream.begin(), payloadDataStream.end()));
    payloadDataStream << payloadCrc32;

    std::vector<unsigned char> assembledData(_preHeader.cbegin(), _preHeader.cend());
    assembledData.insert(assembledData.end(), payloadDataStream.begin(), payloadDataStream.end());

    return assembledData;
}

bool CoinbaseIndexTxHandler::ExtractPayloadFields(std::vector<unsigned char> const& payload,
    uint160& targetAddress,
    int& maxBlockHeight,
    std::vector<unsigned char>& dataSignature)
{
    if (payload.empty()) {
        return false;
    }

    std::decay<decltype(payload)>::type intendedHeader(payload.cbegin(), payload.cbegin() + _preHeader.size());
    if (intendedHeader != _preHeader) {
        return false;
    }

    try {
        uint160 xTargetAddress;
        int xMaxBlockHeight = -1;
        std::vector<unsigned char> xDataSignature;
        uint32_t xCrc = 0;

        std::decay<decltype(payload)>::type payloadNoHeader(payload.cbegin() + _preHeader.size(), payload.cend());
        CDataStream dataStream(payloadNoHeader, SER_DISK, CLIENT_VERSION);
        dataStream >> xTargetAddress >> xMaxBlockHeight >> xDataSignature >> xCrc;

        CDataStream verifyingStream(SER_DISK, CLIENT_VERSION);
        verifyingStream << xTargetAddress << xMaxBlockHeight << xDataSignature;
        uint32_t verifyingCrc = DataToCRC32(std::vector<unsigned char>(verifyingStream.begin(), verifyingStream.end()));
        if (verifyingCrc != xCrc) {
            CoinbaseIndexLog("%s: CRC32 failed while trying to extract payload fields", __FUNCTION__);
            return false;
        }

        targetAddress = std::move(xTargetAddress);
        maxBlockHeight = xMaxBlockHeight;
        dataSignature = std::move(xDataSignature);
    } catch (const std::exception& e) {
        CoinbaseIndexLog("%s: could not extract payload fields; %s", __FUNCTION__, e.what());
        return false;
    }

    return true;
}

bool CoinbaseIndexTxHandler::VerifyPayloadFieldsSignature(
    uint160 const& targetAddress,
    int const& maxBlockHeight,
    std::vector<unsigned char> const& dataSignature,
    std::vector<CPubKey> candidateKeys)
{
    CDataStream signStream(SER_DISK, CLIENT_VERSION);
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

bool CoinbaseIndexTxHandler::FillTransactionWithCoinbaseNewAddress(CMutableTransaction& tx,
    uint160 const& targetAddress,
    int maxBlockHeight,
    const CKey& signKey)
{
    auto payload = BuildNewCoinbaseAddressPayload(targetAddress, maxBlockHeight, signKey);
    if (payload.empty()) {
        CoinbaseIndexLog("%s: payload empty.", __FUNCTION__);
        return false;
    }
    auto payloadBlockSizes = ComputeCoinbaseAddressPayloadBlockSizes(payload);
    if (payloadBlockSizes.empty()) {
        CoinbaseIndexLog("%s: payload block sizes empty.", __FUNCTION__);
        return false;
    }
    auto dataBlocks = PartitionDataBlock(payload, payloadBlockSizes);
    if (dataBlocks.empty()) {
        CoinbaseIndexLog("%s: no data blocks partitioned.", __FUNCTION__);
        return false;
    }

    tx.vout[0].scriptPubKey << OP_RETURN << dataBlocks[0];
    for (auto i = 1u; i < dataBlocks.size(); ++i) {

        // pad data, otherwise uint160 will assert
        if (dataBlocks[i].size() < 20) {
            dataBlocks[i].insert(dataBlocks[i].end(), 20 - dataBlocks[i].size(), 0x00);
        }

        uint160 dataBlockAsBlob(dataBlocks[i]);
        CKeyID fakeKey(dataBlockAsBlob);
        CScript fakeScript = GetScriptForDestination(fakeKey);

        tx.vout[i + 1].scriptPubKey = fakeScript;
    }

    return true;
}

CTransactionRef CoinbaseIndexTxHandler::CreateNewCoinbaseAddrTransaction(
    const CWallet* wallet,
    uint160 const& targetAddress,
    int maxBlockHeight)
{
    if (!wallet) {
        CoinbaseIndexLog("%s: wallet should be available for this method", __FUNCTION__);
        return {};
    }

    if (targetAddress.IsNull()) {
        CoinbaseIndexLog("%s: target address is invalid", __FUNCTION__);
        return {};
    }

    if ((maxBlockHeight < -1) ||
        ((maxBlockHeight > -1) && (maxBlockHeight <= static_cast<int>(mapBlockIndex.size())))) {
        CoinbaseIndexLog("%s: invalid maximum block height", __FUNCTION__);
        return {};
    }

    auto unspentInputsDataTx = SelectUnspentInputsFromWallet(wallet, DEFAULT_COINBASE_TX_FEE);
    if (unspentInputsDataTx.empty()) {
        CoinbaseIndexLog("%s: could not select inputs for coinbase transaction", __FUNCTION__);
        return {};
    }
    auto dataTx = CreateNewCoinbaseAddressTransaction(unspentInputsDataTx, wallet);

    CoinbaseIndexKeyHandler cbKeyHandler(GetDataDir());
    auto signKey = cbKeyHandler.GetSigningKey();
    if (!signKey.IsValid()) {
        CoinbaseIndexLog("%s: could not get the private key to generate the coinbase signature transaction", __FUNCTION__);
        return {};
    }

    auto result = FillTransactionWithCoinbaseNewAddress(dataTx, targetAddress, maxBlockHeight, signKey);
    if (!result) {
        CoinbaseIndexLog("%s: could not fill transaction with new coinbase address data", __FUNCTION__);
        return {};
    }

    result = UpdateNewCoinbaseAddressTransactionFees(dataTx, unspentInputsDataTx);
    if (!result) {
        CoinbaseIndexLog("%s: could not update transaction fees", __FUNCTION__);
        return {};
    }

    if (!SignNewCoinbaseAddressTransaction(dataTx, wallet)) {
        CoinbaseIndexLog("%s: could not sign the coinbase data transaction", __FUNCTION__);
        return {};
    }
    if (!SendNewCoinbaseAddressTransactionToMempool(dataTx)) {
        CoinbaseIndexLog("%s: could not send the coinbase data transaction to the mempool", __FUNCTION__);
        return {};
    }

    return MakeTransactionRef(dataTx);
}

bool CoinbaseIndexTxHandler::HasTransactionNewCoinbaseAddressBasic(CTransactionRef const& tx)
{
    if (tx->vout.size() != NUMBER_OF_CBINDEX_VOUTS) {
        return false;
    }

    if (tx->vout[0].scriptPubKey.front() != OP_RETURN) {
        return false;
    }

    std::vector<unsigned char> scriptHeader(tx->vout[0].scriptPubKey.begin() + 2,
        tx->vout[0].scriptPubKey.begin() + 2 + _preHeader.size());
    if (scriptHeader != _preHeader) {
        return false;
    }

    return true;
}


namespace
{
    class FakeAddressDestinationDecoder : public boost::static_visitor<std::vector<unsigned char>>
    {
    public:
        FakeAddressDestinationDecoder() {}
        std::vector<unsigned char> operator()(const CKeyID& id) const
        {
            return std::vector<unsigned char>(id.begin(), id.end());
        }

        std::vector<unsigned char> operator()(const CScriptID& id) const { return {}; }
        std::vector<unsigned char> operator()(const CNoDestination& no) const { return {}; }
    };
}

std::vector<unsigned char> CoinbaseIndexTxHandler::GetCoinbaseAddressTransactionPayload(CTransactionRef const& tx)
{
    if (!HasTransactionNewCoinbaseAddressBasic(tx)) {
        return {};
    }

    std::vector<unsigned char> payload(tx->vout[0].scriptPubKey.begin() + 2, tx->vout[0].scriptPubKey.end());

    auto numVouts = tx->vout.size();
    for (auto outIdx = 2u; outIdx < numVouts; ++outIdx) {
        const auto& out = tx->vout[outIdx];

        auto const& scriptPubKey = out.scriptPubKey;
        if (scriptPubKey.empty()) {
            continue;
        }

        CTxDestination outFakeDest;
        auto destValid = ExtractDestination(scriptPubKey, outFakeDest);
        if (!destValid) {
            continue;
        }

        std::vector<unsigned char> fakeDestData;
        fakeDestData = boost::apply_visitor(FakeAddressDestinationDecoder(), outFakeDest);
        if (fakeDestData.empty()) {
            continue;
        }
        // sanity check, we should have exactly 20 bytes
        if (fakeDestData.size() != 20) {
            continue;
        }

        payload.insert(payload.end(), fakeDestData.cbegin(), fakeDestData.cend());
    }

    return payload;
}

std::unique_ptr<CoinbaseAddress> CoinbaseIndexTxHandler::ExtractNewCoinbaseAddrFromTransaction(
    CTransactionRef const& tx,
    CoinbaseIndex const& coinbaseIndex)
{
    if (!tx) {
        CoinbaseIndexLog("%s: invalid input transaction", __FUNCTION__);
        return nullptr;
    }

    auto txPayload = GetCoinbaseAddressTransactionPayload(tx);
    if (txPayload.empty()) {
        CoinbaseIndexLog("%s: invalid payload for transaction", __FUNCTION__);
        return nullptr;
    }

    uint160 txTargetAddress;
    int txMaxBlockHeight = -1;
    std::vector<unsigned char> txDataSignature;
    auto result = ExtractPayloadFields(txPayload, txTargetAddress, txMaxBlockHeight, txDataSignature);
    if (!result) {
        CoinbaseIndexLog("%s: could not extract payload data from transaction", __FUNCTION__);
        return nullptr;
    }

    if ((txMaxBlockHeight > -1) && (static_cast<size_t>(txMaxBlockHeight) <= mapBlockIndex.size())) {
        CoinbaseIndexLog("%s: the block height is invalid", __FUNCTION__);
        return nullptr;
    }

    auto defaultCoinbaseKeys = coinbaseIndex.GetDefaultCoinbaseKeys();
    if (defaultCoinbaseKeys.empty()) {
        CoinbaseIndexLog("%s: could not retrieve the default (hard coded) keys", __FUNCTION__);
        return nullptr;
    }

    auto verifiedSignature = VerifyPayloadFieldsSignature(txTargetAddress,
        txMaxBlockHeight, txDataSignature, defaultCoinbaseKeys);
    if (!verifiedSignature) {
        CoinbaseIndexLog("%s: the transaction data signature is invalid", __FUNCTION__);
        return nullptr;
    }

    auto newAddressHash = EncodeDestination(CKeyID(txTargetAddress));
    return std::unique_ptr<CoinbaseAddress>(new CoinbaseAddress{newAddressHash, CPubKey(), false, txMaxBlockHeight});
}
