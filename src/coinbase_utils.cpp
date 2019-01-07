// Copyright (c) 2018 The PAIcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "coinbase_utils.h"
#include "coinbase_txhandler.h"
#include "amount.h"
#include "coins.h"
#include "consensus/validation.h"
#include "net.h"
#include "policy/policy.h"
#include "txmempool.h"
#include "validation.h"
#include "wallet/wallet.h"

#include <numeric>
#include <vector>

UnspentInputs SelectUnspentInputsFromWallet(const CWallet* wallet, CAmount desiredAmount)
{
    if ((wallet == nullptr) || (desiredAmount <= 0)) {
        LogPrintf("%s: wallet not available or the desired amount is invalid", __FUNCTION__);
        return {};
    }

    int nMinDepth = 1;
    int nMaxDepth = 9999999;
    CAmount nMinimumAmount = 0;
    CAmount nMaximumAmount = MAX_MONEY;
    CAmount nMinimumSumAmount = MAX_MONEY;
    uint64_t nMaximumCount = 0;
    std::vector<COutput> vecOutputs;
    UnspentInputs unspentInputs;

    {
        assert(wallet != nullptr);
        LOCK2(cs_main, wallet->cs_wallet);

        wallet->AvailableCoins(vecOutputs, false, nullptr, nMinimumAmount, nMaximumAmount, nMinimumSumAmount, nMaximumCount, nMinDepth, nMaxDepth);
        for (const COutput& out : vecOutputs) {
            unspentInputs.emplace_back(UnspentInput{out.tx, out.i, out.tx->tx->vout[out.i].nValue, out.nDepth});
        }
    }

    std::sort(unspentInputs.begin(), unspentInputs.end(), [](const UnspentInput& u1, const UnspentInput& u2) -> bool {
        return (u1.amount * u1.confirmations) > (u2.amount * u2.confirmations);
    });

    bool foundAmount = false;
    CAmount totalAmount = 0;
    size_t inputIdx = 0;
    for (inputIdx = 0; inputIdx < unspentInputs.size(); ++inputIdx) {
        totalAmount += unspentInputs[inputIdx].amount;
        if (totalAmount >= desiredAmount) {
            foundAmount = true;
            break;
        }
    }

    if (!foundAmount) {
        CoinbaseIndexLog("%s: unable to find the needed amount for coinbase transaction inputs", __FUNCTION__);
        return {};
    }

    return UnspentInputs(unspentInputs.begin(), unspentInputs.begin() + inputIdx + 1);
}

CMutableTransaction CreateNewCoinbaseAddressTransaction(
    const UnspentInputs& unspentInputs,
    const CWallet* wallet)
{
    if (!wallet || unspentInputs.empty()) {
        CoinbaseIndexLog("%s: wallet not available or no available funds", __FUNCTION__);
        return {};
    }

    CMutableTransaction rawTx;
    for (auto const& unspentInput: unspentInputs) {
        uint32_t nSequence = std::numeric_limits<uint32_t>::max();
        CTxIn txIn(COutPoint(unspentInput.out->tx->GetHash(), unspentInput.outputNumber), CScript(), nSequence);

        rawTx.vin.push_back(txIn);
    }

    auto keys = wallet->GetKeys();
    if (keys.empty()) {
        CoinbaseIndexLog("%s: cannot find any wallet keys to accept the remaining output", __FUNCTION__);
        return {}; // otherwise the entire amount is lost in the OP_RETURN tx
    }

    auto firstKey = *(keys.begin());
    CScript ownDestinationScript = GetScriptForDestination(firstKey);

    CTxOut oprOut(0, CScript());
    CTxOut restOut(0, ownDestinationScript);
    std::vector<CTxOut> fakeOuts{
        CTxOut(0, CScript()),
        CTxOut(0, CScript())
    };

    rawTx.vout.push_back(oprOut);
    rawTx.vout.push_back(restOut);
    rawTx.vout.insert(rawTx.vout.end(), fakeOuts.cbegin(), fakeOuts.cend());

    return rawTx;
}

bool UpdateNewCoinbaseAddressTransactionFees(
    CMutableTransaction& rawTx,
    const UnspentInputs& unspentInputs)
{
    if (unspentInputs.empty()) {
        CoinbaseIndexLog("%s: no available funds", __FUNCTION__);
        return false;
    }

    CAmount totalAvailable(0);
    for (auto const& unspentInput: unspentInputs) {
        totalAvailable += unspentInput.amount;
    }

    if (totalAvailable < DEFAULT_COINBASE_TX_FEE) {
        CoinbaseIndexLog("%s: insufficient funds", __FUNCTION__);
        return false;
    }

    auto& vout = rawTx.vout;
    if (vout.size() != NUMBER_OF_CBINDEX_VOUTS) {
        CoinbaseIndexLog("%s: incorrect number of outputs", __FUNCTION__);
        return false;
    }

    // first, compute the fees for the fake outputs
    CAmount sumFakeFees(0);
    std::vector<CAmount> fakeFees;
    for (auto outIdx = 2u; outIdx < vout.size(); ++outIdx) {
        auto outDustFee = GetDustThreshold(vout[outIdx], ::dustRelayFee);

        // make non-dust and update
        outDustFee += 1;
        fakeFees.push_back(outDustFee);

        sumFakeFees += outDustFee;
    }

    CAmount oprFee(DEFAULT_COINBASE_TX_FEE - sumFakeFees);
    CAmount restFee(totalAvailable - DEFAULT_COINBASE_TX_FEE - DEFAULT_MIN_RELAY_TX_FEE);

    // update all outputs with fees
    vout[0].nValue = oprFee;
    vout[1].nValue = restFee;
    for (auto outIdx = 2u, feeIdx = 0u; outIdx < vout.size(); ++outIdx, ++feeIdx) {
        vout[outIdx].nValue = fakeFees[feeIdx];
    }

    return true;
}

bool SignNewCoinbaseAddressTransaction(CMutableTransaction& rawTx, const CWallet* wallet)
{
    if (!wallet) {
        CoinbaseIndexLog("%s: wallet must be available in order to sign the coinbase transaction", __FUNCTION__);
        return false;
    }

    CCoinsView viewDummy;
    CCoinsViewCache view(&viewDummy);
    {
        LOCK(mempool.cs);
        CCoinsViewCache &viewChain = *pcoinsTip;
        CCoinsViewMemPool viewMempool(&viewChain, mempool);
        view.SetBackend(viewMempool);

        for (const CTxIn& txin : rawTx.vin) {
            view.AccessCoin(txin.prevout);
        }

        view.SetBackend(viewDummy);
    }

    const CTransaction txConst(rawTx);
    for (unsigned int i = 0; i < rawTx.vin.size(); i++) {
        CTxIn& txin = rawTx.vin[i];
        const Coin& coin = view.AccessCoin(txin.prevout);
        if (coin.IsSpent()) {
            CoinbaseIndexLog("%s: coin is spent, cancelling the operation", __FUNCTION__);
            return false;
        }
        const CScript& prevPubKey = coin.out.scriptPubKey;
        const CAmount& amount = coin.out.nValue;

        SignatureData sigdata;
        ProduceSignature(MutableTransactionSignatureCreator(wallet, &rawTx, i, amount, SIGHASH_ALL), prevPubKey, sigdata);
        sigdata = CombineSignatures(prevPubKey, TransactionSignatureChecker(&txConst, i, amount), sigdata, DataFromTransaction(rawTx, i));

        UpdateTransaction(rawTx, i, sigdata);

        ScriptError serror = SCRIPT_ERR_OK;
        if (!VerifyScript(txin.scriptSig, prevPubKey, &txin.scriptWitness, STANDARD_SCRIPT_VERIFY_FLAGS, TransactionSignatureChecker(&txConst, i, amount), &serror)) {
            CoinbaseIndexLog("%s: could not verify the destination script", __FUNCTION__);
            return false;
        }
    }

    return true;
}

bool SendNewCoinbaseAddressTransactionToMempool(CMutableTransaction rawTx)
{
    bool txAccepted = false;
    
    LOCK(cs_main);

    CTransactionRef tx(MakeTransactionRef(std::move(rawTx)));
    const uint256& hashTx = tx->GetHash();

    CAmount nMaxRawTxFee = maxTxFee;

    CCoinsViewCache &view = *pcoinsTip;
    bool isTxInChain = false;
    for (size_t outputIdx = 0; !isTxInChain && outputIdx < tx->vout.size(); ++outputIdx) {
        const Coin& existingCoin = view.AccessCoin(COutPoint(hashTx, outputIdx));
        isTxInChain = !existingCoin.IsSpent();
    }

    bool isTxInMempool = mempool.exists(hashTx);

    if (!isTxInMempool && !isTxInChain) {
        // push to local node and sync with wallets
        CValidationState state;
        bool areMissingInputs = false;

        txAccepted = AcceptToMemoryPool(mempool, state, std::move(tx), &areMissingInputs, nullptr, false, nMaxRawTxFee);
    }

    if (!txAccepted) {
        CoinbaseIndexLog("%s: transaction mempool could not accept the tentative coinbase transaction", __FUNCTION__);
    }

    if(!g_connman)
        return txAccepted;

    CInv inv(MSG_TX, hashTx);
    g_connman->ForEachNode([&inv](CNode* pnode)
    {
        pnode->PushInventory(inv);
    });

    return txAccepted;
}

std::vector<std::vector<unsigned char>> PartitionDataBlock(
    std::vector<unsigned char> const& inputBlock,
    std::vector<size_t> const& sizes
)
{
    if (inputBlock.empty()) {
        CoinbaseIndexLog("%s: input block empty, returning empty output", __FUNCTION__);
        return {};
    }
    if (sizes.empty()) {
        CoinbaseIndexLog("%s: no sizes provided, returning input", __FUNCTION__);
        return {inputBlock};
    }
    if (sizes.size() > inputBlock.size()) {
        CoinbaseIndexLog("%s: more blocks requested than the input block could provide", __FUNCTION__);
        return {inputBlock};
    }

    auto numRequestedData = std::accumulate(sizes.cbegin(), sizes.cend(), 0u);
    if (numRequestedData > inputBlock.size()) {
        CoinbaseIndexLog("%s: too much data requested and not existing in input block", __FUNCTION__);
        return {};
    }

    std::vector<std::vector<unsigned char>> partitionedData;
    auto currDataCursor = inputBlock.cbegin();
    for (auto const& currSize : sizes) {
        if (currSize < 1) {
            partitionedData.emplace_back(std::vector<unsigned char>{});
            continue;
        }

        partitionedData.emplace_back(std::vector<unsigned char>(currDataCursor, currDataCursor + currSize));
        std::advance(currDataCursor, currSize);
    }

    if (currDataCursor != inputBlock.cend()) {
        partitionedData.emplace_back(std::vector<unsigned char>(currDataCursor, inputBlock.cend()));
    }

    return partitionedData;
}

std::vector<size_t> ComputeCoinbaseAddressPayloadBlockSizes(std::vector<unsigned char> const& payload)
{
    if (payload.size() <= 75) {
        return {payload.size()};
    }

    std::vector<size_t> blockSizes;
    auto payloadSize = payload.size();

    // first block will be stored in OP_RETURN vout, we can safely store 75 bytes here
    blockSizes.push_back(75);
    payloadSize -= 75;

    // compute how many fake addresses (blocks of 20 bytes) we need
    // add a residual, even if we will not use the full 20 bytes of them
    uint16_t numFakeAddresses = static_cast<uint16_t>(payloadSize / 20);
    uint16_t paddingFakeAddressesSize = payloadSize % 20;

    blockSizes.insert(blockSizes.end(), numFakeAddresses, 20);
    if (paddingFakeAddressesSize > 0) {
        blockSizes.push_back(paddingFakeAddressesSize);
    }

    return blockSizes;
}
