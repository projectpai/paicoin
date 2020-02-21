// Copyright (c) 2017-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "tx_verify.h"

#include "consensus.h"
#include "pubkey.h"
#include "primitives/transaction.h"
#include "script/interpreter.h"
#include "script/standard.h"
#include "stake/staketx.h"
#include "validation.h"
#include "../validation.h"
#include "chainparams.h"

// TODO remove the following dependencies
#include "chain.h"
#include "coins.h"
#include "utilmoneystr.h"

bool IsExpiredTx(const CTransaction &tx, int nBlockHeight)
{
    return tx.nExpiry != 0 && (uint32_t)nBlockHeight >= tx.nExpiry;
}

bool IsFinalTx(const CTransaction &tx, int nBlockHeight, int64_t nBlockTime)
{
    if (tx.nLockTime == 0)
        return true;
    if ((int64_t)tx.nLockTime < ((int64_t)tx.nLockTime < LOCKTIME_THRESHOLD ? (int64_t)nBlockHeight : nBlockTime))
        return true;
    for (const auto& txin : tx.vin) {
        if (!(txin.nSequence == CTxIn::SEQUENCE_FINAL))
            return false;
    }
    return true;
}

std::pair<int, int64_t> CalculateSequenceLocks(const CTransaction &tx, int flags, std::vector<int>* prevHeights, const CBlockIndex& block)
{
    assert(prevHeights->size() == tx.vin.size());

    // Will be set to the equivalent height- and time-based nLockTime
    // values that would be necessary to satisfy all relative lock-
    // time constraints given our view of block chain history.
    // The semantics of nLockTime are the last invalid height/time, so
    // use -1 to have the effect of any height or time being valid.
    int nMinHeight = -1;
    int64_t nMinTime = -1;

    // tx.nVersion is signed integer so requires cast to unsigned otherwise
    // we would be doing a signed comparison and half the range of nVersion
    // wouldn't support BIP 68.
    bool fEnforceBIP68 = static_cast<uint32_t>(tx.nVersion) >= 2
                      && flags & LOCKTIME_VERIFY_SEQUENCE;

    // Do not enforce sequence numbers as a relative lock time
    // unless we have been instructed to
    if (!fEnforceBIP68) {
        return std::make_pair(nMinHeight, nMinTime);
    }

    for (size_t txinIndex = 0; txinIndex < tx.vin.size(); txinIndex++) {
        const CTxIn& txin = tx.vin[txinIndex];

        // Sequence numbers with the most significant bit set are not
        // treated as relative lock-times, nor are they given any
        // consensus-enforced meaning at this point.
        if (txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_DISABLE_FLAG) {
            // The height of this input is not relevant for sequence locks
            (*prevHeights)[txinIndex] = 0;
            continue;
        }

        int nCoinHeight = (*prevHeights)[txinIndex];

        if (txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG) {
            int64_t nCoinTime = block.GetAncestor(std::max(nCoinHeight-1, 0))->GetMedianTimePast();
            // NOTE: Subtract 1 to maintain nLockTime semantics
            // BIP 68 relative lock times have the semantics of calculating
            // the first block or time at which the transaction would be
            // valid. When calculating the effective block time or height
            // for the entire transaction, we switch to using the
            // semantics of nLockTime which is the last invalid block
            // time or height.  Thus we subtract 1 from the calculated
            // time or height.

            // Time-based relative lock-times are measured from the
            // smallest allowed timestamp of the block containing the
            // txout being spent, which is the median time past of the
            // block prior.
            nMinTime = std::max(nMinTime, nCoinTime + (int64_t)((txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_MASK) << CTxIn::SEQUENCE_LOCKTIME_GRANULARITY) - 1);
        } else {
            nMinHeight = std::max(nMinHeight, nCoinHeight + (int)(txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_MASK) - 1);
        }
    }

    return std::make_pair(nMinHeight, nMinTime);
}

bool EvaluateSequenceLocks(const CBlockIndex& block, std::pair<int, int64_t> lockPair)
{
    assert(block.pprev);
    int64_t nBlockTime = block.pprev->GetMedianTimePast();
    if (lockPair.first >= block.nHeight || lockPair.second >= nBlockTime)
        return false;

    return true;
}

bool SequenceLocks(const CTransaction &tx, int flags, std::vector<int>* prevHeights, const CBlockIndex& block)
{
    return EvaluateSequenceLocks(block, CalculateSequenceLocks(tx, flags, prevHeights, block));
}

unsigned int GetLegacySigOpCount(const CTransaction& tx)
{
    unsigned startInput = ParseTxClass(tx) == TX_Vote ? voteStakeInputIndex : 0;    // first input in a vote is subsidy generation; skip it

    unsigned int nSigOps = 0;
    for (unsigned i=startInput; i<tx.vin.size(); i++)
    {
        auto& txin = tx.vin[i];
        nSigOps += txin.scriptSig.GetSigOpCount(false);
    }
    for (const auto& txout : tx.vout)
    {
        nSigOps += txout.scriptPubKey.GetSigOpCount(false);
    }
    return nSigOps;
}

unsigned int GetP2SHSigOpCount(const CTransaction& tx, const CCoinsViewCache& inputs)
{
    if (tx.IsCoinBase())
        return 0;

    unsigned int nSigOps = 0;
    unsigned startInput = ParseTxClass(tx) == TX_Vote ? voteStakeInputIndex : 0;    // first input in a vote is subsidy generation; skip it
    for (unsigned int i = startInput; i < tx.vin.size(); i++)
    {
        const Coin& coin = inputs.AccessCoin(tx.vin[i].prevout);
        assert(!coin.IsSpent());
        const CTxOut &prevout = coin.out;
        if (prevout.scriptPubKey.IsPayToScriptHash())
            nSigOps += prevout.scriptPubKey.GetSigOpCount(tx.vin[i].scriptSig);
    }
    return nSigOps;
}

int64_t GetTransactionSigOpCost(const CTransaction& tx, const CCoinsViewCache& inputs, int flags)
{
    int64_t nSigOps = GetLegacySigOpCount(tx) * WITNESS_SCALE_FACTOR;

    if (tx.IsCoinBase())
        return nSigOps;

    if (flags & SCRIPT_VERIFY_P2SH) {
        nSigOps += GetP2SHSigOpCount(tx, inputs) * WITNESS_SCALE_FACTOR;
    }

    unsigned startInput = ParseTxClass(tx) == TX_Vote ? voteStakeInputIndex : 0;    // first input in a vote is subsidy generation; skip it
    for (unsigned int i = startInput; i < tx.vin.size(); i++)
    {
        const Coin& coin = inputs.AccessCoin(tx.vin[i].prevout);
        assert(!coin.IsSpent());
        const CTxOut &prevout = coin.out;
        nSigOps += CountWitnessSigOps(tx.vin[i].scriptSig, prevout.scriptPubKey, &tx.vin[i].scriptWitness, flags);
    }
    return nSigOps;
}

bool CheckTransaction(const CTransaction& tx, CValidationState &state, bool fCheckDuplicateInputs)
{
    // Basic checks that don't depend on any context
    if (tx.vin.empty())
        return state.DoS(10, false, REJECT_INVALID, "bad-txns-vin-empty");
    if (tx.vout.empty())
        return state.DoS(10, false, REJECT_INVALID, "bad-txns-vout-empty");
    // Size limits (this doesn't take the witness into account, as that hasn't been checked for malleability)
    if (::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) * WITNESS_SCALE_FACTOR > MAX_BLOCK_WEIGHT)
        return state.DoS(100, false, REJECT_INVALID, "bad-txns-oversize");

    // Check for negative or overflow output values
    CAmount nValueOut = 0;
    for (const auto& txout : tx.vout)
    {
        if (txout.nValue < 0)
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-vout-negative");
        if (txout.nValue > MAX_MONEY)
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-vout-toolarge");
        nValueOut += txout.nValue;
        if (!MoneyRange(nValueOut))
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-txouttotal-toolarge");
    }

    // Check for duplicate inputs - note that this check is slow so we skip it in CheckBlock
    if (fCheckDuplicateInputs) {
        std::set<COutPoint> vInOutPoints;
        for (const auto& txin : tx.vin)
        {
            if (!vInOutPoints.insert(txin.prevout).second)
                return state.DoS(100, false, REJECT_INVALID, "bad-txns-inputs-duplicate");
        }
    }

    ETxClass txClass = ParseTxClass(tx);
    if (txClass == TX_Regular && tx.IsCoinBase())
    {
        // Check length of coinbase scriptSig.
        if (tx.vin[0].scriptSig.size() < 2 || tx.vin[0].scriptSig.size() > 100)
            return state.DoS(100, false, REJECT_INVALID, "bad-cb-length");
    }
    else if (txClass == TX_Vote)
    {
        // Check length of subsidy scriptSig.
        if (tx.vin[voteSubsidyInputIndex].scriptSig.size() < 2 || tx.vin[voteSubsidyInputIndex].scriptSig.size() > 100)
            return state.DoS(100, false, REJECT_INVALID, "bad-stakereward-length");

        // Reward scriptSig must be set to the one specified by the network.
        if (tx.vin[voteSubsidyInputIndex].scriptSig != Params().GetConsensus().stakeBaseSigScript)
            return state.DoS(100, false, REJECT_INVALID, "bad-stakereward-scriptsig");

        // The ticket reference must not be null.
        if (tx.vin[voteStakeInputIndex].prevout.IsNull())
            return state.DoS(100, false, REJECT_INVALID, "bad-ticket-ref");
    }
    else
    {
        // Previous transaction outputs referenced by the inputs to this transaction must not be null.
        for (const auto& txin : tx.vin)
            if (txin.prevout.IsNull())
                return state.DoS(10, false, REJECT_INVALID, "bad-txns-prevout-null");
    }

    return true;
}

bool isBuyTicketStake(const Coin& coin, uint32_t txoutIndex)
{
    return coin.txClass == TX_BuyTicket && txoutIndex == ticketStakeOutputIndex;
}

bool isVoteReward(const Coin& coin, uint32_t txoutIndex)
{
    return coin.txClass == TX_Vote && txoutIndex >= voteRewardOutputIndex;
}

bool isRevokeTicketRefund(const Coin& coin, uint32_t txoutIndex)
{
    return coin.txClass == TX_RevokeTicket && txoutIndex >= revocationRefundOutputIndex;
}

bool isLegalScriptTypeForStake(const CScript& script)
{
    txnouttype whichType;
    std::vector<std::vector<unsigned char> > vSolutions;
    if (!Solver(script, whichType, vSolutions))
        return false;
    return whichType == TX_PUBKEYHASH || whichType == TX_SCRIPTHASH;
}

bool isLegalInputForBuyTicket(const Coin& coin, int txoutIndex)
{
    // TODO: we should not normally allow spending coinbases on buying tickets
    // but as we test this on REGTEST we have no other way at the moment, because we wanted to avoid 
    // adding another regular transaction before doing the actual ticket purchase
    if (coin.IsCoinBase())
        return true;

    // check class of containing transaction
    bool containedInLegalTxClass =
        coin.txClass == TX_Regular                          // a regular tx output, including coinbase, is a valid input
        || (coin.txClass == TX_BuyTicket && txoutIndex != ticketStakeOutputIndex)   // BuyTicket change is a valid input; BuyTicket stake is not!
        || coin.txClass == TX_Vote                             // Vote reward is a valid input
        || coin.txClass == TX_RevokeTicket;                    // RevokeTicket refund is a valid input
    if (!containedInLegalTxClass)
        return false;


    // check if stake coin's scriptPubKey is P2PKH or P2SH
    return isLegalScriptTypeForStake(coin.out.scriptPubKey);
}

bool isLegalInputForVoteOrRevocation(const Coin& coin, int txoutIndex)
{
    // check that the containing transaction is a ticket stake
    if (!isBuyTicketStake(coin, txoutIndex))
        return false;

    // check if stake coin's scriptPubKey is P2PKH or P2SH
    return isLegalScriptTypeForStake(coin.out.scriptPubKey);
}

bool checkBuyTicketInputs(const CTransaction& tx, CValidationState& state, const CCoinsViewCache& inputs, int nSpendHeight, const CChainParams& chainparams)
{
    // check transaction structure
    std::string reason;
    if (!ValidateBuyTicketStructure(tx, reason))
        return state.DoS(100, false, REJECT_INVALID, "bad-buyticket-structure", false, reason);

    // validate individual inputs
    for (unsigned i = 0; i < tx.vin.size(); i++)
    {
        auto& txIn = tx.vin[i];
        const Coin& coin = inputs.AccessCoin(txIn.prevout);

        // check that the coin exists and is unspent
        if (coin.IsSpent())
            return state.DoS(100, false, REJECT_INVALID, "bad-txin-missingorspent");

        // legal inputs are: outputs of regular or coinbase transactions, BuyTicket change outputs, Vote reward and RevokeTicket refund outputs
        // illegal inputs are: BuyTicket stake outputs, data outputs
        if (!isLegalInputForBuyTicket(coin, txIn.prevout.n))
            return state.DoS(100, false, REJECT_INVALID, "bad-txin-illegal-input");

        // check that the contribution plus change equals input amount
        TicketContribData contrib;
        ParseTicketContrib(tx, ticketContribOutputIndex + 2*i, contrib);
        CAmount change = tx.vout[ticketChangeOutputIndex + 2*i].nValue;
        CAmount funding = coin.out.nValue;
        if (contrib.contributedAmount + change != funding)
            return state.DoS(100, false, REJECT_INVALID, "bad-txin-amount-mismatch");
    }

    return true;
}

bool checkVoteOrRevokeTicketInputs(const CTransaction& tx, bool vote, CValidationState& state, const CCoinsViewCache& inputs, int nSpendHeight, const CChainParams& chainparams)
{
    std::string what = vote ? "vote" : "revocation";
    std::string badTxin = "bad-txin-";

    // check transaction structure
    std::string reason;
    bool structureValid = vote ? ValidateVoteStructure(tx, reason) : ValidateRevokeTicketStructure(tx, reason);
    if (!structureValid)
        return state.DoS(100, false, REJECT_INVALID, std::string("bad-") + what + "-structure", false, reason);

    // Check that the ticket stake input is the second output (index 1) of BuyTicket
    int stakeInputIndex = vote ? voteStakeInputIndex : revocationStakeInputIndex;
    const COutPoint& ticketStakeOutpoint = tx.vin[stakeInputIndex].prevout;
    if (ticketStakeOutpoint.n != ticketStakeOutputIndex)
        return state.DoS(100, false, REJECT_INVALID, badTxin + what + "-spends-nonstake");

    // Find BuyTicket transaction
    CTransactionRef ticketTxPtr = GetTicket(ticketStakeOutpoint.hash);
    if (ticketTxPtr == nullptr)
        return state.DoS(100, false, REJECT_INVALID, badTxin + what + "-bad-ticket-reference");

    // Ensure the referenced ticket stake coins are available.
    const Coin& coin = inputs.AccessCoin(ticketStakeOutpoint);
    if (coin.IsSpent())
        return state.DoS(100, false, REJECT_INVALID, badTxin + what + "-ticketstake-missingorspent");

    // legal input is only a BuyTicket stake output
    if (!isLegalInputForVoteOrRevocation(coin, ticketStakeOutpoint.n))
        return state.DoS(100, false, REJECT_INVALID, "bad-txin-illegal-input");

    // Ensure that the ticket being spent is mature.
    // NOTE: A ticket stake can only be spent in the block AFTER the entire ticket maturity has passed, hence the +1.
    // In case of revocations, the ticket must have been missed which can't possibly
    // happen for another block after that, hence the +2.
    int maturityAdd = vote ? 1 : 2;
    int ticketMaturity = chainparams.GetConsensus().nTicketMaturity + maturityAdd;
    if (nSpendHeight - coin.nHeight < ticketMaturity)
        return state.DoS(100, false, REJECT_INVALID, badTxin + what + "-ticketstake-immature");

    // Ensure that the number of payment outputs matches the number of ticket stake contributions.
    auto numVotePayments = tx.vout.size() - 1;
    auto numTicketStakeContributions = ticketTxPtr->vout.size() - 2;
    if (numVotePayments * 2 != numTicketStakeContributions)
        return state.DoS(100, false, REJECT_INVALID, what + "-payments-contributions-mismatch");

    // Calculate contribution sum
    CAmount contributionSum = 0;
    for (unsigned i = ticketContribOutputIndex; i < ticketTxPtr->vout.size(); i += 2) {
        TicketContribData contrib;
        ParseTicketContrib(*ticketTxPtr, i, contrib);
        contributionSum += contrib.contributedAmount;
    }

    // Ensure the payment outputs correspond with the ticket contributions.
    CAmount stakedAmount = ticketTxPtr->vout[ticketStakeOutputIndex].nValue;
    CAmount subsidy = vote ? GetVoterSubsidy(nSpendHeight, Params().GetConsensus()) : 0;
    unsigned paymentOutputIndex = vote ? voteRewardOutputIndex : revocationRefundOutputIndex;
    for (unsigned i = paymentOutputIndex; i < tx.vout.size(); i++)
    {
        // Extract contribution info corresponding to this payment
        unsigned contribIndex = 2 * i;
        TicketContribData contrib;
        ParseTicketContrib(*ticketTxPtr, contribIndex, contrib);

        // Check if the payment uses P2PKH or P2SH
        if (!isLegalScriptTypeForStake(tx.vout[i].scriptPubKey))
            return state.DoS(100, false, REJECT_INVALID, what + "-invalid-payment-type");

        // Check if the payment goes to the address specified in the contribution info
        CTxDestination dest;
        if (!ExtractDestination(tx.vout[i].scriptPubKey, dest) || !IsValidDestination(dest))
            return state.DoS(100, false, REJECT_INVALID, what + "-invalid-payment-address");
        const uint160& addr = dest.which() == 1 ? boost::get<CKeyID>(dest) : boost::get<CScriptID>(dest);
        if (addr != contrib.rewardAddr)
            return state.DoS(100, false, REJECT_INVALID, what + "-incorrect-payment-address");

        // Check if the payment amount is as expected
        CAmount paymentAmount = CalcContributorRemuneration(contrib.contributedAmount, stakedAmount, subsidy, contributionSum);
        if (tx.vout[i].nValue != paymentAmount)
            return state.DoS(100, false, REJECT_INVALID, what + "-bad-payment-amount");
    }

    return true;
}

bool Consensus::CheckTxInputs(const CTransaction& tx, CValidationState& state, const CCoinsViewCache& inputs, int nSpendHeight, CAmount& txfee, const CChainParams& chainparams)
{
    // are the actual inputs available?
    if (!inputs.HaveInputs(tx)) {
        return state.DoS(100, false, REJECT_INVALID, "bad-txns-inputs-missingorspent", false,
                         strprintf("%s: inputs missing/spent", __func__));
    }

    // parse tx
    ETxClass txClass = ParseTxClass(tx);
    VoteData voteData;
    if (txClass == TX_Vote && !ParseVote(tx, voteData))
        return state.DoS(100, false, REJECT_INVALID, "could-not-parse-vote-tx", false);

    // check inputs of stake transactions
    if (txClass == TX_BuyTicket && !checkBuyTicketInputs(tx, state, inputs, nSpendHeight, chainparams))
        return false;
    if (txClass == TX_Vote && !checkVoteOrRevokeTicketInputs(tx, true, state, inputs, nSpendHeight, chainparams))
        return false;
    if (txClass == TX_RevokeTicket && !checkVoteOrRevokeTicketInputs(tx, false, state, inputs, nSpendHeight, chainparams))
        return false;

    // general inputs check and summation of input amounts
    CAmount nValueIn = 0;
    for (unsigned int i = 0; i < tx.vin.size(); ++i)
    {
        // vote reward input doesn't reference existing coins
        // (instead, like with coinbase, coins are generated out of thin air)
        // so we'll skip the checks
        if (txClass == TX_Vote && i == voteSubsidyInputIndex)
        {
            nValueIn += GetVoterSubsidy(nSpendHeight/*voteData.blockHeight*/, ::Params().GetConsensus());
            continue;
        }

        const COutPoint &prevout = tx.vin[i].prevout;
        const Coin& coin = inputs.AccessCoin(prevout);
        assert(!coin.IsSpent());

        // If prev is coinbase, check that it's matured
        const CChainParams& chainparams = ::Params();
        if (!chainparams.HasGenesisBlockTxOutPoint(prevout)) {
          if (coin.IsCoinBase() && nSpendHeight - coin.nHeight < COINBASE_MATURITY) {
              return state.Invalid(false,
                  REJECT_INVALID, "bad-txns-premature-spend-of-coinbase",
                  strprintf("tried to spend coinbase at depth %d", nSpendHeight - coin.nHeight));
          }
        }

        // Unless the tx is a vote or a revocation, it is forbidden to spend ticket stake
        if (txClass != TX_Vote && txClass != TX_RevokeTicket && isBuyTicketStake(coin, prevout.n))
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-illegal-spend-of-ticket-stake");

        // Vote reward and revocation refund payments can only be spent after coinbase maturity many blocks.
        if ((isVoteReward(coin, prevout.n) || isRevokeTicketRefund(coin, prevout.n))
            && nSpendHeight - coin.nHeight < COINBASE_MATURITY)
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-reward-or-refund-immature");

        // Check for negative or overflow input values
        nValueIn += coin.out.nValue;
        if (!MoneyRange(coin.out.nValue) || !MoneyRange(nValueIn)) {
            return state.DoS(100, false, REJECT_INVALID, "bad-txns-inputvalues-outofrange");
        }
    }

    // Output amount must not be greater than inputs
    const CAmount value_out = tx.GetValueOut();
    if (nValueIn < value_out) {
        return state.DoS(100, false, REJECT_INVALID, "bad-txns-in-belowout", false,
            strprintf("value in (%s) < value out (%s)", FormatMoney(nValueIn), FormatMoney(value_out)));
    }

    // Tally transaction fees
    const CAmount txfee_aux = nValueIn - value_out;
    if (!MoneyRange(txfee_aux)) {
        return state.DoS(100, false, REJECT_INVALID, "bad-txns-fee-outofrange");
    }

    txfee = txfee_aux;
    return true;
}
