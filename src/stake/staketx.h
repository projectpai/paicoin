#ifndef PAICOIN_STAKE_STAKETX_H
#define PAICOIN_STAKE_STAKETX_H

#include <string>
#include "script/pai_data_classifier.h"
#include "script/standard.h"
#include "pubkey.h"
#include "stakenode.h"

class CTransaction;
class CBlock;

// ======================================================================
// classification and structural validation of transaction outputs;
// these functions are general purpose and unrelated to staking transactions; they could be moved elsewhere

bool IsPaymentTxout(const CTransaction& tx, uint32_t txoutIndex);
bool IsDataTxout(const CTransaction& tx, uint32_t txoutIndex);
bool IsStructuredDataTxout(const CTransaction& tx, uint32_t txoutIndex);

bool ValidateDataTxoutStructure(const CTransaction& tx, uint32_t txoutIndex,
                                unsigned numExpectedItems, unsigned expectedItemSizes[],
                                std::vector<std::vector<unsigned char> > *pRetItems, std::string& reason);

// ======================================================================
// creation of data scripts for staking transactions

enum EStakeDataClass {         // these values must not be changed (they are stored in scripts), so only appending is allowed
    STAKE_TxDeclaration,
    STAKE_TicketContribution
};

struct BuyTicketData {
    int nVersion;
};

struct TicketContribData {
    TicketContribData()
    {;}
    explicit TicketContribData(int version, const CTxDestination& reward, const CAmount& amount)
    {
        nVersion = version;

        assert(IsValidDestination(reward));
        whichAddr = reward.which();
        if (whichAddr == 1)
            rewardAddr = boost::get<const CKeyID>(reward);
        else if(whichAddr == 2)
            rewardAddr = boost::get<const CScriptID>(reward);

        contributedAmount = amount;
    }
    friend bool operator==(const TicketContribData& l, const TicketContribData& r)
    {
        return std::tie(l.nVersion, l.rewardAddr, l.whichAddr, l.contributedAmount)
            == std::tie(r.nVersion, r.rewardAddr, r.whichAddr, r.contributedAmount);
    }
    int nVersion;
    uint160 rewardAddr;
    int whichAddr;
    CAmount contributedAmount;
};

struct VoteData {
    int nVersion;
    uint256 blockHash;
    uint32_t blockHeight;
    uint32_t voteBits;
    uint32_t voterStakeVersion;
};

struct RevokeTicketData {
    int nVersion;
};

CScript GetScriptForBuyTicketDecl(const BuyTicketData& data);
CScript GetScriptForTicketContrib(const TicketContribData& data);
CScript GetScriptForVoteDecl(const VoteData& data);
CScript GetScriptForRevokeTicketDecl(const RevokeTicketData& data);

// ======================================================================
// classification and validation of staking transactions

static const uint32_t BUY_TICKET_MAX_INPUTS = 64;

// indices of inputs in stake transactions
const uint32_t ticketFundingInputIndex = 0;
const uint32_t voteSubsidyInputIndex = 0;
const uint32_t voteStakeInputIndex = 1;
const uint32_t revocationStakeInputIndex = 0;

// indices of outputs in stake transactions
const uint32_t txdeclOutputIndex = 0;
const uint32_t ticketStakeOutputIndex = 1;
const uint32_t ticketContribOutputIndex = 2;
const uint32_t ticketChangeOutputIndex = 3;
const uint32_t voteRewardOutputIndex = 1;
const uint32_t revocationRefundOutputIndex = 1;

// indices of data items in structured OP_RETURN outputs of stake transactions
const uint32_t structVersionIndex = 0;
const uint32_t dataClassIndex = 1;
const uint32_t stakeDataClassIndex = 2;
//---
const uint32_t txClassIndex = 3;
const uint32_t txVersionIndex = 4;
const uint32_t voteBlockHashIndex = 5;
const uint32_t voteBlockHeightIndex = 6;
const uint32_t voteBitsIndex = 7;
const uint32_t voterStakeVersionIndex = 8;
//---
const uint32_t contribVersionIndex = 3;
const uint32_t contribAddrIndex = 4;
const uint32_t contribAddrTypeIndex = 5;
const uint32_t contribAmountIndex = 6;

enum ETxClass {         // these values must not be changed (they are stored in scripts), so only appending is allowed
    TX_Regular,
    TX_BuyTicket,
    TX_Vote,
    TX_RevokeTicket
};

ETxClass ParseTxClass(const CTransaction& tx);
bool ParseTicketContrib(const CTransaction& tx, uint32_t txoutIndex, TicketContribData& data);
bool ParseVote(const CTransaction& tx, VoteData& data);
bool IsStakeTx(ETxClass txClass);

bool ValidateStakeTxDeclStructure(const CTransaction& tx, ETxClass eTxClassExpected, unsigned numExpectedItems, unsigned expectedItemSizes[], std::string& reason);
bool ValidateStakeTxStructure(const CTransaction& tx, std::string& reason);
bool ValidateBuyTicketStructure(const CTransaction& tx, std::string& reason);
bool ValidateVoteStructure(const CTransaction& tx, std::string& reason);
bool ValidateRevokeTicketStructure(const CTransaction& tx, std::string& reason);

// ==============================

// StakeSlice is a helper class that makes it easier to traverse stake transactions of a block.
// It assumes that transactions in the original container satisfy the rules that:
// - The first transaction is the coinbase
// - Then follow stake transactions in any order
// - Then follow regular transactions
// Example usage:
// for (const auto& tx : StakeSlice(block.vtx, TX_BuyTicket))  // traverses BuyTicket transactions
// or:
// for (const auto& tx : StakeSlice(block.vtx))                // traverses all stake transactions
// Implementation details:
// The current implementation uses a simple but non-optimal approach:
// It makes a copy of transaction references into a new local container that contain only the desired transactions.
// Making a temp vector shouldn't be too big a deal since there aren't many stake transactions in blocks.
// However, a better implementation should rather provide a custom range or custom iterators over the given container.
// Also, in case we'd need a slice of regular transactions as well, we wouldn't have it using this approach as it would be costly.
// So feel free to reimplement.
class StakeSlice : public std::vector<CTransactionRef>
{
public:
    StakeSlice(std::vector<CTransactionRef> vtx);
    StakeSlice(std::vector<CTransactionRef> vtx, ETxClass txClass);
};

SpentTicketsInBlock FindSpentTicketsInBlock(const CBlock& block);
#endif //PAICOIN_STAKE_STAKETX_H
