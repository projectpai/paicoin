#include "coinbase_utils.h"
#include "amount.h"
#include "coins.h"
#include "consensus/validation.h"
#include "net.h"
#include "policy/policy.h"
#include "txmempool.h"
#include "validation.h"
#include "wallet/wallet.h"

#include <vector>

UnspentInputs SelectInputs(const CWallet* wallet, CAmount desiredAmount)
{
    if ((wallet == nullptr) || (desiredAmount <= 0)) {
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
        return {};
    }

    return UnspentInputs(unspentInputs.begin(), unspentInputs.begin() + inputIdx + 1);
}

CMutableTransaction CreateCoinbaseTransaction(
    const UnspentInputs& unspentInputs,
    CAmount txAmount,
    const CWallet* wallet)
{
    if (!wallet || unspentInputs.empty()) {
        return {};
    }

    CMutableTransaction rawTx;
    CAmount totalAvailableAmount = 0;

    for (auto const& unspentInput: unspentInputs) {
        uint32_t nSequence = std::numeric_limits<uint32_t>::max();
        CTxIn txIn(COutPoint(unspentInput.out->tx->GetHash(), unspentInput.outputNumber), CScript(), nSequence);

        rawTx.vin.push_back(txIn);
        totalAvailableAmount += unspentInput.amount;
    }

    auto keys = wallet->GetKeys();
    if (keys.empty()) {
        return {}; // otherwise the entire amount is lost in the OP_RETURN tx
    }

    auto firstKey = *(keys.begin());
    CScript ownDestinationScript = GetScriptForDestination(firstKey);

    CTxOut oprOut(txAmount / 2, CScript());
    CTxOut restOut(totalAvailableAmount - txAmount, ownDestinationScript);

    rawTx.vout.push_back(oprOut);
    rawTx.vout.push_back(restOut);

    return rawTx;
}

bool SignCoinbaseTransaction(CMutableTransaction& rawTx, const CWallet* wallet)
{
    if (!wallet) {
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
            return false;
        }
    }

    return true;
}

bool SendCoinbaseTransactionToMempool(CMutableTransaction rawTx)
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
        bool isLimitFree = true;

        txAccepted = AcceptToMemoryPool(mempool, state, std::move(tx), isLimitFree, &areMissingInputs, nullptr, false, nMaxRawTxFee);
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
