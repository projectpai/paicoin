// Copyright (c) 2020 The Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAICOIN_WALLET_STAKE_TEST_FIXTURE_H
#define PAICOIN_WALLET_STAKE_TEST_FIXTURE_H

#include "test/test_paicoin.h"
#include "wallet/wallet.h"

// Testing fixture that allows easy generation of a blockchain
// containing stake transactions belonging to a wallet.
// Checking functions are also provided for the stake transactions.

class WalletStakeTestingSetup : public TestingSetup
{
public:
    // Lifecycle

    explicit WalletStakeTestingSetup();
    ~WalletStakeTestingSetup();

    // Regular transactions

    uint256 SendMoney(const std::vector<std::pair<CTxDestination, CAmount>> recipients);

    // Stake transactions

    uint256 AddTicketTx(bool useVsp = false, bool foreign = false);

    uint256 AddVoteTx(const uint256& ticketHash, bool foreignSignature = false);

    uint256 AddRevokeTx(const uint256& ticketHash, bool foreignSignature = false);

    // Memory pool verifications

    bool MempoolHasTicket();

    bool MempoolHasEnoughTicketsForBlock();

    bool MempoolHasVoteForTicket(const uint256& ticketHash);

    bool MempoolHasEnoughVotesForBlock();

    // Block creation and validation

    CBlock CreateAndProcessBlock();

    bool ExtendChain(int blockCount, bool withImplicitVotes = true, bool useVsp = false, bool foreign = false, bool onlyMajorityVotes = false, bool foreignSignature = false);

    bool GetSomeCoins();

    // Stake transactions validation

    void CheckTicket(const CTransaction& ticket, std::vector<TicketContribData> expectedContributions = {}, bool validateAmounts = false);

    void CheckVote(const CTransaction& vote, const CTransaction& ticket, bool inMempool = true);
    void CheckVote(const uint256& voteHash, const uint256& ticketHash, const VoteData& voteData);

    void CheckLatestVotes(bool inMempool = false, bool ifAllMine = true);

    void CheckRevocation(const CTransaction& revocation, const CTransaction& ticket);
    void CheckRevocation(const uint256& revocationHash, const uint256& ticketHash);

    void CheckLatestRevocations(bool inMempool = false, bool ifAllMine = true);

    // RPC

    UniValue CallRpc(std::string args);

public:

    // general

    const CChainParams& params = Params();
    const Consensus::Params& consensus = params.GetConsensus();

    CScript coinbaseScriptPubKey;

    // wallet

    std::unique_ptr<CWallet> wallet;
    const SecureString passphrase = "aV3rySecurePassword!";

    std::vector<CTransactionRef> txsInMempoolBeforeLastBlock; // the transactions in mempool before the last block

    std::vector<CTransactionRef> votesInLastBlock; // the vote transactions in the last block only
    std::vector<CTransactionRef> revocationsInLastBlock; // the revocations transactions in the last block only

    // test helper structures storing frequently used data or categorized data

    struct EmptyData {
        EmptyData();
        const uint256 hash;
        const std::vector<unsigned char> vector;
        const std::string string;
    } emptyData;

    struct ExtendedVoteBitsData {
        ExtendedVoteBitsData();
        const ExtendedVoteBits empty;
        const std::vector<unsigned char> validLargeVector;
        const std::vector<unsigned char> invalidLargeVector;
        std::string validLargeString;
        std::string invalidLargeString;
    } extendedVoteBitsData;

    struct ForeignData {
        ForeignData();

        void AddTicket(CTransactionRef ticket);
        CTransactionRef Ticket(const uint256& ticketHash);

        void AddRedeemScript(const CScript& script, const uint256& ticketHash);
        CScript RedeemScript(const uint256& ticketHash);

        void AddTicketKey(const CKey& key, const uint256& ticketHash);
        const CKey NewTicketKey();
        bool HasTicketKey(const CKey& key);
        bool HasTicketPubKey(const CPubKey& pubKey);
        bool HasTicketKeyId(const CKeyID& keyId);
        CKey KeyForTicket(const uint256& ticketHash);

        void AddVspKey(const CKey& key, const uint256& ticketHash);
        const CKey NewVspKey();
        bool HasVspKey(const CKey& key);
        bool HasVspPubKey(const CPubKey& pubKey);
        bool HasVspKeyId(const CKeyID& keyId);
        CKey KeyForVsp(const uint256& ticketHash);
    private:
        std::vector<CTransactionRef> tickets;
        std::vector<std::pair<CScript, uint256>> redeemScripts;
        std::vector<std::tuple<CKey, CPubKey, CKeyID, uint256>> ticketKeys;
        std::vector<std::tuple<CKey, CPubKey, CKeyID, uint256>> vspKeys;
    } foreignData;
};

#endif // PAICOIN_WALLET_STAKE_TEST_FIXTURE_H
