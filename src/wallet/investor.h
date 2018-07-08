// Copyright (c) 2018 by ObEN, Inc.
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef INVESTOR_H
#define INVESTOR_H

#include <inttypes.h>
#include <string>
#include <vector>
#include "key.h"
#include "sync.h"
#include "script/script.h"

class CWallet;
class CWalletTx;
class CScript;
class CPubKey;
class CKey;
struct CMutableTransaction;
class CTxIn;
class CTxOut;
class CCriticalSection;
class uint256;

class Investor final {
public:
    // Singleton
    static Investor& GetInstance();

    /*
     * Writes the multisig address of type M-of-N to addr and the corresponding redeem script to redeemScript
     * pubKeys can contain the public keys to be used in the address
     * returns true if addr and redeemScript contain the correct information, false otherwise
     */
    bool CreateMultisig(std::string& address, CScript& redeemScript, const int required, const std::vector<CPubKey>& pubKeys);

    /*
     * Stores the investor public key (and generate the multisig addresses and redeem scripts)
     * this should be called as early as possible to have the information stored
     */
    void SetPublicKey(CWallet& wallet, const CPubKey& pubKey);

    /*
     * Gets all the multisig addresses for the investor
     */
    std::vector<std::string> AllMultisigAddresses();

    /* Gets the balance for all the investor's multisig addresses */
    uint64_t GlobalBalance(void);

    /* Process each wallet transaction to account for the amount in the investor output or input for the corresponding multisig address */
    void UpdateGlobalBalance(const CWallet& wallet);

    /* Returns true if the transaction has an output that refers to one of the investor's multisig addresses */
    bool TransactionIsMyInvestment(const CWalletTx* tx);

    /* Returns true if the total balance for the transaction's investor outputs is zero, or if the transaction does not have any investor output */
    bool TransactionIsUnlocked(const CWalletTx* tx);

    /* Returns true if the address is one of the investor's multisig addresses */
    bool AddressIsMyMultisig(std::string& address);

    /* Returns true if the Hash160 is one of the investor's multisig keys */
    bool Hash160IsMyMultisig(const uint160& hash160);

    /* Get the number of seconds until the soonest period expiration in which the investor has funds (non-zero balance) */
    uint64_t SecondsUntilHoldingPeriodExpires(void);

    /* Returns true if the user should be prompted with the popup to update the application */
    bool ShouldUpdateApplication(const CWallet& wallet);

    /* Returns true if the user should be prompted with the popup to unlock the investment */
    bool ShouldUnlockInvestment(const CWallet& wallet);

    /*
     * Creates a transaction that sends all the investor funds that are unlocked to a specified public key
     * returns true if transaction created successfully
     */
    bool CreateUnlockTransaction(CWallet& wallet, const CKey& privateKey, const CPubKey& pubKey, CMutableTransaction& unlockTransaction);

    /* Returns true if this an the unlock transaction (all inputs are from the holding period multisig addresses) */
    bool IsUnlockTransaction(const CWallet& wallet, const CWalletTx* tx);

    /* Resets the investor's balance data only */
    void ResetBalance(void);

    /* Resets the investor's data (public key, multisig address, redeem script, balance) */
    void Wipe(void);

private:
    /*
     * The holding period array is used for data storage throught the investor functions
     * timestamps and the corresponding PAI keys must be added at compile time
     * DO NOT STORE sensitive information (like user private keys) in this array
     */
    typedef struct {
        // Unix timestamp of the expiration date
        const uint64_t expirationTimestamp;

        // PAI public key
        const CPubKey paiPublicKey;

        // PAI private key (only filled when holding period ends and a new build is made)
        const CKey paiPrivateKey;

        // investor's public key
        CPubKey investorPublicKey;

        // investor multisig address, obtained from the paiPublicKey and the investorPublicKey
        std::string multisigAddress;

        // the redeem script that generated the multisig address
        CScript redeemScript;

        // the current balance of the multisig address
        int64_t balance;
    } HoldingPeriod;

    std::vector<HoldingPeriod> HoldingPeriods;

    Investor();
    ~Investor();
    Investor(Investor const&)           = delete;
    void operator= (Investor const&)    = delete;

    mutable CCriticalSection csInvestor;

    // processed transactions
    std::vector<uint256> addedTxHashes;
    void AddTxHash(const CWalletTx *tx);
    void DeleteTxHash(const CWalletTx *tx);
    bool IsTxHashAdded(const CWalletTx *tx);
    void CleanTxHashList(void);

    bool DoesScriptPayToMultisigAddress(const CScript& script, const std::string& multisigAddress);

    bool FundingOutputForInput(const CWallet& wallet, const CTxIn& input, CTxOut& output);
    bool MultisigFundingOutputForInput(const CWallet& wallet, const CTxIn& input, const std::string& multisigAddress, CTxOut& output);

    void SignUnlockTransaction(CWallet& wallet, const CKey& privateKey, CMutableTransaction& unlockTransaction);
};

#endif // INVESTOR_H
