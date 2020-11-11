//
// Copyright (c) 2017-2020 Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//

#include "primitives/transaction.h"
#include "stake/staketx.h"
#include "script/standard.h"
#include "chain.h"
#include "pubkey.h"

#include <sstream>

#define REASON(arg) do { std::ostringstream _reason; _reason << arg; reason = _reason.str(); } while(false)

bool IsPaymentTxout(const CTransaction& tx, uint32_t txoutIndex)
{
    if (txoutIndex >= tx.vout.size())
        return false;

    const CScript& scr = tx.vout[txoutIndex].scriptPubKey;
    return !scr.IsUnspendable();
}

bool IsDataTxout(const CTransaction& tx, uint32_t txoutIndex)
{
    if (txoutIndex >= tx.vout.size())
        return false;

    const CScript& scr = tx.vout[txoutIndex].scriptPubKey;
    return scr.size() > 0 && scr[0] == OP_RETURN;
}

bool IsStructuredDataTxout(const CTransaction& tx, uint32_t txoutIndex)
{
    if (txoutIndex >= tx.vout.size())
        return false;

    const CScript& scr = tx.vout[txoutIndex].scriptPubKey;
    return scr.size() >= 3 && scr[0] == OP_RETURN && scr[1] == OP_STRUCT;   // scr[2] (version) must be present
}

bool ValidateTxDeclStructure(const CTransaction& tx, ETxClass eTxClassExpected, unsigned numExpectedItems, unsigned expectedItemSizes[], std::string& reason)
{
    std::vector<std::vector<unsigned char> > items;
    if (!ValidateDataTxoutStructure(tx, txdeclOutputIndex, numExpectedItems, expectedItemSizes, &items, reason))
        return false;

    if (CScriptNum(items[0], false).getint() != eTxClassExpected)   // returned items begin with txClass, hence index 0
        return false;

    return true;
}

bool ValidateDataTxoutStructure(const CTransaction& tx, uint32_t txoutIndex,
                       unsigned numExpectedItems, unsigned expectedItemSizes[],
                       std::vector<std::vector<unsigned char> > *pRetItems, std::string& reason)
{
    bool isData = IsDataTxout(tx, txoutIndex);
    bool isStruct = IsStructuredDataTxout(tx, txoutIndex);
    if (!isData && !isStruct) {
        REASON("output " << txoutIndex << " not a data output");
        return false;
    }
    int offsetItems = isStruct ? txClassIndex : 0; // skip version, dataClass and stakeDataClass

    const CScript& script = tx.vout[txoutIndex].scriptPubKey;
    txnouttype scriptType;
    std::vector<std::vector<unsigned char> > items;
    if (!Solver(script, scriptType, items)) {
        REASON("couldn't parse output " << txoutIndex);
        return false;
    }

    std::vector<std::vector<unsigned char> > localItems;
    auto& resultItems = pRetItems ? *pRetItems : localItems;
    std::copy(items.begin() + offsetItems, items.end(), std::back_inserter(resultItems));

    if (resultItems.size() != numExpectedItems) {
        REASON("in output " << txoutIndex << " expected " << numExpectedItems << " data items, found " << resultItems.size());
        return false;
    }

    // following OP_RETURN header, there must exist pieces of data whose number and sizes correspond to the expected dataSizes
    for (unsigned i=0; i<numExpectedItems; i++) {
        if (expectedItemSizes[i] == 0) // zero indicates a variable item size, so we skip size comparison in that case
            continue;
        if (resultItems[i].size() != expectedItemSizes[i]) {
            REASON("in output " << txoutIndex << ", data item " << i
                         << " expected size was " << expectedItemSizes[i]
                         << " bytes, found " << resultItems[i].size() << " bytes");
            return false;
        }
    }

    return true;
}

// ======================================================================

const CAmount TicketContribData::DefaultFeeLimit = 1LL<<20;

CScript GetScriptForBuyTicketDecl(const BuyTicketData& data)
{
    return GetScriptForStructuredData(CLASS_Staking) << STAKE_TxDeclaration
                    << TX_BuyTicket << data.nVersion;
}

CScript GetScriptForTicketContrib(const TicketContribData& data)
{
    return GetScriptForStructuredData(CLASS_Staking) << STAKE_TicketContribution
                    << data.nVersion
                    << ToByteVector(data.rewardAddr) << data.whichAddr
                    << data.contributedAmount
                    << data.voteFeeLimit()
                    << data.revocationFeeLimit();
}

CScript GetScriptForVoteDecl(const VoteData& data)
{
    return GetScriptForStructuredData(CLASS_Staking) << STAKE_TxDeclaration
                    << TX_Vote << data.nVersion
                    << ToByteVector(data.blockHash) << data.blockHeight << data.voteBits.getBits() << data.voterStakeVersion << data.extendedVoteBits.getVector();
}

CScript GetScriptForRevokeTicketDecl(const RevokeTicketData& data)
{
    return GetScriptForStructuredData(CLASS_Staking) << STAKE_TxDeclaration
                    << TX_RevokeTicket << data.nVersion;
}

size_t GetVoteDataSizeWithEmptyExtendedVoteBits()
{
    VoteData voteData;
    return GetSerializeSize(GetScriptForVoteDecl(voteData), SER_NETWORK, PROTOCOL_VERSION);
}

// ======================================================================

bool ParseStakeData(const CTransaction& tx, uint32_t txoutIndex, EStakeDataClass eStakeData, unsigned minItems, std::vector<std::vector<unsigned char> >& items)
{
    if (tx.vout.size() <= txoutIndex || !IsStructuredDataTxout(tx, txoutIndex))
        return false;

    const CScript& script = tx.vout[txoutIndex].scriptPubKey;
    txnouttype scriptType;
    if (!Solver(script, scriptType, items) || scriptType != TX_STRUCT_DATA)
        return false;

    if (items.size() < 3 || items.size() < minItems)     // structVersion, dataClass, stakeDataClass
        return false;

    int structVersion = CScriptNum(items[structVersionIndex], false).getint();
    if (structVersion != 1)
        return false;

    EDataClass eDataClass = (EDataClass) CScriptNum(items[dataClassIndex], false).getint();
    if (eDataClass != CLASS_Staking)
        return false;

    EStakeDataClass eStakeDataClass = (EStakeDataClass) CScriptNum(items[stakeDataClassIndex], false).getint();
    if (eStakeDataClass != eStakeData)
        return false;

    return true;
}

std::string TxClassToString(ETxClass txClass) 
{
    switch (txClass) {
        case ETxClass::TX_Regular: return "standard";
        case ETxClass::TX_BuyTicket: return "stake_purchase";
        case ETxClass::TX_RevokeTicket: return "stake_revocation";
        case ETxClass::TX_Vote: return "vote";
        default:
            assert(!"unknown ETxClass");
            return "unknown";
    }
}

ETxClass ParseTxClass(const CTransaction& tx)
{
    int minItems = 4;   // structHeaderVersion, dataClass, stakeDataClass, txClass
    std::vector<std::vector<unsigned char> > items;
    if (!ParseStakeData(tx, txdeclOutputIndex, STAKE_TxDeclaration, minItems, items))
        return TX_Regular;

    return (ETxClass) CScriptNum(items[txClassIndex], false).getint();
}

bool IsStakeTx(ETxClass eTxClass)
{
    return eTxClass == TX_BuyTicket || eTxClass == TX_Vote || eTxClass == TX_RevokeTicket;
}

bool IsStakeTx(const CTransaction& tx)
{
    return IsStakeTx(ParseTxClass(tx));
}

bool IsStakeTxOutSpendableByRegularTx(const CTransaction& tx, const uint32_t txoutIndex)
{
    // the stake transaction outputs that can be spent directly by a regular transaction are:
    // - ticket purchase: any change output (but not the stake output)
    // - vote: any reward output
    // - revocation: any refund output

    ETxClass txClass = ParseTxClass(tx);
    switch (txClass) {
        case TX_Regular:
            return true;

        case TX_BuyTicket:
            return (txoutIndex >= ticketChangeOutputIndex) && ((txoutIndex - ticketChangeOutputIndex) % 2 == 0);

        case TX_Vote:
            return txoutIndex >= voteRewardOutputIndex;

        case TX_RevokeTicket:
            return txoutIndex >= revocationRefundOutputIndex;
    }

    return false;
}

bool HasStakebaseContents(const CTxIn& txIn)
{
    return txIn.prevout.n == std::numeric_limits<uint32_t>::max() && txIn.scriptSig == Params().GetConsensus().stakeBaseSigScript && txIn.prevout.hash == uint256();
}

bool ParseTicketContrib(const CTransaction& tx, uint32_t txoutIndex, TicketContribData& data)
{
    int numItems = 9;   // structVersion, dataClass, stakeDataClass, contribVersion, rewardAddr, whichAddr, contribAmount, voteFeeLimit, revocationFeeLimit
    std::vector<std::vector<unsigned char> > items;
    if (!ParseStakeData(tx, txoutIndex, STAKE_TicketContribution, numItems, items))
        return false;

    data.nVersion = CScriptNum(items[contribVersionIndex], false).getint();

    data.rewardAddr = uint160(items[contribAddrIndex]);
    data.whichAddr = CScriptNum(items[contribAddrTypeIndex],false).getint();

    // amounts may require more than the default 4 bytes for storing in script numbers

    data.contributedAmount = CScriptNum(items[contribAmountIndex], false, CScriptNum::nMaxNumSizeForInt64).getint64();

    CAmount feeLimit = CScriptNum(items[contribVoteFeeLimitIndex], false, CScriptNum::nMaxNumSizeForInt64).getint64();
    if (!MoneyRange(feeLimit))
        return false;
    data.setVoteFeeLimit(feeLimit);

    feeLimit = CScriptNum(items[contribRevocationFeeLimitIndex], false, CScriptNum::nMaxNumSizeForInt64).getint64();
    if (!MoneyRange(feeLimit))
        return false;
    data.setRevocationFeeLimit(feeLimit);

    return true;
}

bool ParseTicketContribs(const CTransaction& tx, std::vector<TicketContribData>& contributions, CAmount& totalContribution, CAmount& totalVoteFeeLimit, CAmount& totalRevocationFeeLimit)
{
    totalContribution = 0;
    totalVoteFeeLimit = 0;
    totalRevocationFeeLimit = 0;

    for (unsigned i = ticketContribOutputIndex; i < tx.vout.size(); i += 2) {
        TicketContribData contrib;
        if (!ParseTicketContrib(tx, i, contrib))
            return false;

        contributions.push_back(contrib);

        totalContribution += contrib.contributedAmount;

        totalVoteFeeLimit += contrib.voteFeeLimit();
        totalRevocationFeeLimit += contrib.revocationFeeLimit();
    }

    return true;
}

bool ParseVote(const CTransaction& tx, VoteData& data)
{
    int numItems = 10;   // structVersion, dataClass, stakeDataClass, txClass, voteVersion, blockHash, blockHeight, voteBits, voterStakeVersion, extendedVoteBits
    std::vector<std::vector<unsigned char> > items;
    if (!ParseStakeData(tx, txdeclOutputIndex, STAKE_TxDeclaration, numItems, items))
        return false;

    ETxClass txClass = (ETxClass) CScriptNum(items[txClassIndex], false).getint();
    if (txClass != TX_Vote)
        return false;

    data.nVersion = CScriptNum(items[txVersionIndex], false).getint();
    if (data.nVersion != 1)
        return false;

    data.blockHash = uint256(items[voteBlockHashIndex]);
    data.blockHeight = (uint32_t) CScriptNum(items[voteBlockHeightIndex], false).getint();
    int vb = CScriptNum(items[voteBitsIndex], false).getint();
    if (vb < 0 || vb > std::numeric_limits<uint16_t>().max())
        return false;
    data.voteBits = VoteBits{static_cast<uint16_t>(vb)};
    data.voterStakeVersion = (uint32_t) CScriptNum(items[voterStakeVersionIndex], false).getint();
    data.extendedVoteBits = ExtendedVoteBits(items[extendedVoteBitsIndex]);

    return true;
}

// ValidateStakeTx inspects inputs and outputs of a stake transaction
// to see if they are composed in accordance with its stated txClass;
//
bool ValidateStakeTxStructure(const CTransaction& tx, std::string& reason)
{
    ETxClass eTxClass = ParseTxClass(tx);

    switch (eTxClass)
    {
    case TX_BuyTicket:
        return ValidateBuyTicketStructure(tx, reason);
    case TX_Vote:
        return ValidateVoteStructure(tx, reason);
    case TX_RevokeTicket:
        return ValidateRevokeTicketStructure(tx, reason);
    default:
        return false;
    }
}

bool ValidateBuyTicketStructure(const CTransaction &tx, std::string& reason)
{
    // BuyTicket transactions are specified as below.
    //
    // Inputs:
    // stake contribution 1 [index 0]
    // ...
    // stake contribution n [index n-1]  (max n is BUY_TICKET_MAX_INPUTS)
    //
    // Outputs:
    // BuyTicket declaration                         [index 0]
    // stake payment                                 [index 1]
    // address for reward payment to contributor 1   [index 2]
    // change for contribution 1                     [index 3]
    // ...
    // address for reward payment to contributor n   [index 2*n]
    // change for contribution n                     [index 2*n+1]

    // check number of inputs
    uint32_t numInputs = tx.vin.size();
    if (numInputs == 0) {
        reason = "transaction has no inputs";
        return false;
    }
    if (numInputs > BUY_TICKET_MAX_INPUTS) {
        REASON("transaction has " << numInputs << " inputs, maximum is " << BUY_TICKET_MAX_INPUTS);
        return false;
    }

    // check number of outputs
    uint32_t numExpectedOutputs = 1 + 1 + 2 * numInputs;
    uint32_t numOutputs = tx.vout.size();
    if (numOutputs != numExpectedOutputs) {
        REASON("transaction has " << numOutputs << " outputs, expected " << numExpectedOutputs);
        return false;
    }

    // transaction inputs must satisfy certain conditions (they must not be ticket stakes, they must be mature etc.)
    // but this kind of validation requires context and is performed elsewhere;
    // here we check only outputs;

    // check that the first output contains TX_BuyTicket declaration
    unsigned dataSizes[] = { 0, 0 };    // TX_BuyTicket, version
    if (!ValidateTxDeclStructure(tx, TX_BuyTicket, 2, dataSizes, reason))
        return false;

    // check that the second output (ticket stake) is a payment
    if (!IsPaymentTxout(tx, ticketStakeOutputIndex)) {     // ??? should we limit this to P2PKH and P2SH only? why?
        reason = "output 0 (stake) not a payment";
        return false;
    }

    // check the rest of the outputs; they must correspond well to inputs
    for (uint32_t txoutIndex = ticketContribOutputIndex; txoutIndex < numOutputs; ++txoutIndex )
    {
        // odd-indexed outputs should be return change payments
        if (txoutIndex % 2 == 1) {
            if (!IsPaymentTxout(tx, txoutIndex)) {
                REASON("output " << txoutIndex << " (change) not a payment");
                return false;
            }
        }
        // even-indexed outputs should be reward addresses
        else
        {
            unsigned dataSizes[] = { 0, sizeof(uint160), 0, 0, 0, 0 };     // version, 20-bytes address hash, addrType, contributed amount, vote fee limit, revocation fee limit
            if (!ValidateDataTxoutStructure(tx, txoutIndex, 6, dataSizes, nullptr, reason))
                return false;
        }
    }

    return true;
}

bool ValidateVoteStructure(const CTransaction &tx, std::string& reason)
{
    // Vote transactions are specified as below.
    //
    // Inputs:
    // subsidy generation  [index 0]
    // ticket stake        [index 1]
    //
    // Outputs:
    // TX_Vote declaration and voting data   [index 0]
    // payment to contributor 1              [index 1]
    // ...
    // payment to contributor n              [index n]

    // check number of inputs
    uint32_t numInputs = tx.vin.size();
    if (numInputs != 2) {
        REASON("transaction has " << numInputs << " inputs, expected 2");
        return false;
    }

    // check number of outputs
    // exact number must correspond to the number of contributors in the BuyTicket transaction,
    // but this check requires context and is done elsewhere;
    uint32_t numOutputs = tx.vout.size();
    if (numOutputs < 2) {
        REASON("transaction has " << numOutputs << " outputs, expected at least 2");
        return false;
    }

    // check the subsidy input
    if (!tx.vin[voteSubsidyInputIndex].prevout.IsNull()) {
        REASON("input " << voteSubsidyInputIndex << " (subsidy issuance) not a coin generation outpoint");
        return false;
    }

    // check the stake input
    if (tx.vin[voteStakeInputIndex].prevout.n != ticketStakeOutputIndex) {
        REASON("input " << voteStakeInputIndex << " not a ticket stake outpoint");
        return false;
    }

    // check that the first output contains vote declaration (TX_Vote, block hash, block height, vote bits, stake version)
    // validation of voting data itself is contextual and done elsewhere
    unsigned dataSizes[] = { 0, 0, sizeof(uint256), 0, 0, 0, 0 }; // TX_Vote, version, blockHash, blockHeight, voteBits, stakeVersion, extendedVoteBits
    if (!ValidateTxDeclStructure(tx, TX_Vote, 7, dataSizes, reason))
        return false;

    // check that the rest of the outputs are payments;
    // validation of amounts and destination addresses is contextual and done elsewhere
    for (uint32_t txoutIndex = voteRewardOutputIndex; txoutIndex < numOutputs; ++txoutIndex )
    {
        if (!IsPaymentTxout(tx, txoutIndex)) {
            REASON("output " << txoutIndex << " (reward) not a payment");
            return false;
        }
    }

    return true;
}

bool ValidateRevokeTicketStructure(const CTransaction &tx, std::string& reason)
{
    // RevokeTicket transactions are specified as below.
    //
    // Inputs:
    // stake output of the BuyTicket transaction to be revoked [index 0]
    //
    // Outputs:
    // RevokeTicket declaration    [index 0]
    // refund to contributor 1     [index 1]
    // ...
    // refund to contributor n     [index n]

    // check number of inputs
    uint32_t numInputs = tx.vin.size();
    if (numInputs != 1) {
        REASON("transaction has " << numInputs << " inputs, expected 1");
        return false;
    }

    // check number of outputs
    // exact number must correspond to the number of contributors in the BuyTicket transaction,
    // but this check requires context and is done elsewhere;
    uint32_t numOutputs = tx.vout.size();
    if (numOutputs == 0) {
        reason = "transaction has no outputs";
        return false;
    }

    // input 0 must be a BuyTicket stake, but the full check is contextual and done elsewhere;
    // here we'll just verify that its outpoint index is 0, since stake payment is the first output of a BuyTicket
    if (tx.vin[revocationStakeInputIndex].prevout.n != ticketStakeOutputIndex) {
        REASON("input " << revocationStakeInputIndex << " does not reference a stake");
        return false;
    }

    // check that the first output contains TX_RevokeTicket declaration
    unsigned dataSizes[] = { 0, 0 };    // TX_RevokeTicket, version
    if (!ValidateTxDeclStructure(tx, TX_RevokeTicket, 2, dataSizes, reason))
        return false;

    // check that the outputs are payments;
    // validation of amounts and destination addresses is contextual and done elsewhere
    for (uint32_t txoutIndex = revocationRefundOutputIndex; txoutIndex < numOutputs; ++txoutIndex )
    {
        if (!IsPaymentTxout(tx, txoutIndex)) {
            REASON("output " << txoutIndex << " not a payment");
            return false;
        }
    }

    return true;
}

size_t GetEstimatedP2PKHTxInSize(bool compressed)
{
    // a P2PKH input has the following structure:
    // - previous outpoint hash     [32 bytes]
    // - previous outpoint index    [4 bytes]
    // - scriptsig size             [1 byte]
    // - push opcode                [1 byte]
    // - signature                  [71 or 72 bytes]
    // - push opcode                [1 byte]
    // - public key                 [33 bytes compressed, 65 bytes uncompressed]
    // - sequence                   [4 bytes]

    return 32 + 4 + 1 + 1 + 72 + 1 + (compressed ? 33 : 65) + 4;
}

size_t GetEstimatedP2PKHTxOutSize()
{
    // a P2PKH output has the following structure:
    // - value              [8 bytes]
    // - script size        [1 byte]
    // - OP_DUP             [1 byte]
    // - OP_HASH160         [1 byte]
    // - push opcode        [1 byte]
    // - public key hash    [20 bytes]
    // - OP_EQUALVERIFY     [1 byte]
    // - OP_CHECKSIG        [1 byte]

    return 8 + 1 + 1 + 1 + 1 + 20 + 1 + 1;
}

size_t GetEstimatedBuyTicketDeclTxOutSize()
{
    BuyTicketData data{1};
    CTxOut txOut{0, GetScriptForBuyTicketDecl(data)};
    return GetSerializeSize(txOut, SER_NETWORK, PROTOCOL_VERSION);
}

size_t GetEstimatedTicketContribTxOutSize()
{
    TicketContribData data{1, CKeyID(), MAX_MONEY, MAX_MONEY, MAX_MONEY};
    CTxOut txOut{0, GetScriptForTicketContrib(data)};
    return GetSerializeSize(txOut, SER_NETWORK, PROTOCOL_VERSION);
}

size_t GetEstimatedSizeOfBuyTicketTx(bool useVsp, bool includeExpiry)
{
    // since the format of the ticket purchase transaction is fixed,
    // its size can be estimated precisely.
    // A ticket purchase transaction has the structure described in ValidateBuyTicketStructure().
    // version + in count (1|2) + (1|2) regular input + out count (4|5) + buy ticket decl output + ticket address output + pool fee contributor (optional)  + pool fee change output (optional) + contributor info output + change output + locktime + expiry (optional)

    return 4
            + 1
            + (useVsp ? GetEstimatedP2PKHTxInSize() : 0)
            + GetEstimatedP2PKHTxInSize()
            + 1
            + GetEstimatedBuyTicketDeclTxOutSize()
            + GetEstimatedP2PKHTxOutSize()
            + (useVsp ? GetEstimatedTicketContribTxOutSize() : 0)
            + (useVsp ? GetEstimatedP2PKHTxOutSize() : 0)
            + GetEstimatedTicketContribTxOutSize()
            + GetEstimatedP2PKHTxOutSize()
            + 4
            + (includeExpiry ? 4 : 0);
}

size_t GetEstimatedRevokeTicketDeclTxOutSize()
{
    RevokeTicketData data{1};
    CTxOut txOut{0, GetScriptForRevokeTicketDecl(data)};
    return GetSerializeSize(txOut, SER_NETWORK, PROTOCOL_VERSION);
}

size_t GetEstimatedSizeOfRevokeTicketTx(const size_t refundsCount, bool compressedInput, bool includeExpiry)
{
    // since the format of the revocation transaction is fixed,
    // its size can be estimated precisely.
    // A revocation transaction has the structure described in ValidateRevokeTicketStructure().
    // version + in count 1 + 1 regular input + out count (1+n) + revoke ticket decl output + n regular outputs + locktime + expiry (optional)

    return 4
            + 1
            + GetEstimatedP2PKHTxInSize(compressedInput)
            + 2
            + GetEstimatedRevokeTicketDeclTxOutSize()
            + refundsCount * GetEstimatedP2PKHTxOutSize()
            + 4
            + (includeExpiry ? 4 : 0);
}

StakeSlice::StakeSlice(std::vector<CTransactionRef> vtx)
{
    for (size_t i=1; i<vtx.size(); i++) // skip coinbase
    {
        auto txCl = ParseTxClass(*vtx[i]);
        if (txCl == TX_Regular)
            break;  // reached the end of stake transactions
        push_back(vtx[i]);
    }
}

StakeSlice::StakeSlice(std::vector<CTransactionRef> vtx, ETxClass txClass)
{
    for (size_t i=1; i<vtx.size(); i++) // skip coinbase
    {
        auto txCl = ParseTxClass(*vtx[i]);
        if (txCl == TX_Regular)
            break;  // reached the end of stake transactions
        if (txCl == txClass)
            push_back(vtx[i]);
    }
}


// FindSpentTicketsInBlock returns information about tickets spent in a given
// block. This includes voted and revoked tickets, and the vote bits of each
// spent ticket. This is faster than calling the individual functions to
// determine ticket state if all information regarding spent tickets is needed.
//
// Note that the returned hashes are of the originally purchased *tickets* and
// **NOT** of the vote/revoke transaction.
//
// The tickets are determined **only** from the STransactions of the provided
// block and no validation is performed.
//
// This function is only safe to be called with a block that has previously
// had all header commitments validated.
SpentTicketsInBlock FindSpentTicketsInBlock(const CBlock& block)
{
    HashVector revocations;
    HashVector voters;
    VoteVersionVector votes;

    for (auto& it: block.vtx){
        switch (ParseTxClass(*it))
        {
        case TX_Vote: {
                VoteData voteData;
                ParseVote(*it, voteData);
                voters.push_back(
                    it->vin[1].prevout.hash);
                votes.push_back(
                    VoteVersion{
                        static_cast<uint32_t>(voteData.voterStakeVersion),
                        voteData.voteBits
                        });
                }
                break;
        case TX_RevokeTicket:
                revocations.push_back(
                    it->vin[0].prevout.hash);
                break;
        };
    }

    return std::make_tuple(voters, revocations, votes);
}
