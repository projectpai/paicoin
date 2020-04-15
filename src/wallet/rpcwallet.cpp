// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "amount.h"
#include <key_io.h>
#include "chain.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "httpserver.h"
#include "validation.h"
#include "net.h"
#include "policy/feerate.h"
#include "policy/fees.h"
#include "policy/policy.h"
#include "policy/rbf.h"
#include "rpc/mining.h"
#include "rpc/safemode.h"
#include "rpc/server.h"
#include "script/sign.h"
#include "timedata.h"
#include "util.h"
#include "utilmoneystr.h"
#include "wallet/coincontrol.h"
#include "wallet/feebumper.h"
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#include "wallet/fees.h"
#include "pow.h"

#include <init.h>  // For StartShutdown

#include <stdint.h>

#include <univalue.h>

static const std::string WALLET_ENDPOINT_BASE = "/wallet/";

CWallet *GetWalletForJSONRPCRequest(const JSONRPCRequest& request)
{
    if (request.URI.substr(0, WALLET_ENDPOINT_BASE.size()) == WALLET_ENDPOINT_BASE) {
        // wallet endpoint was used
        std::string requestedWallet = urlDecode(request.URI.substr(WALLET_ENDPOINT_BASE.size()));
        for (CWalletRef pwallet : ::vpwallets) {
            if (pwallet->GetName() == requestedWallet) {
                return pwallet;
            }
        }
        throw JSONRPCError(RPCErrorCode::WALLET_NOT_FOUND, "Requested wallet does not exist or is not loaded");
    }
    return ::vpwallets.size() == 1 || (request.fHelp && ::vpwallets.size() > 0) ? ::vpwallets[0] : nullptr;
}

std::string HelpRequiringPassphrase(const CWallet * const pwallet)
{
    return pwallet && pwallet->IsCrypted()
        ? "\nRequires wallet passphrase to be set with walletpassphrase call."
        : "";
}

bool EnsureWalletIsAvailable(const CWallet * const pwallet, bool avoidException)
{
    if (pwallet) return true;
    if (avoidException) return false;
    if (::vpwallets.empty()) {
        // Note: It isn't currently possible to trigger this error because
        // wallet RPC methods aren't registered unless a wallet is loaded. But
        // this error is being kept as a precaution, because it's possible in
        // the future that wallet RPC methods might get or remain registered
        // when no wallets are loaded.
        throw JSONRPCError(
            RPCErrorCode::METHOD_NOT_FOUND, "Method not found (wallet method is disabled because no wallet is loaded)");
    }
    throw JSONRPCError(RPCErrorCode::WALLET_NOT_SPECIFIED,
        "Wallet file not specified (must request wallet RPC through /wallet/<filename> uri-path).");
}

void EnsureWalletIsUnlocked(const CWallet * const pwallet)
{
    if (pwallet->IsLocked()) {
        throw JSONRPCError(RPCErrorCode::WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");
    }
}

static void WalletTxToJSON(const CWalletTx& wtx, UniValue& entry)
{
    const auto confirms = wtx.GetDepthInMainChain();
    entry.push_back(Pair("confirmations", confirms));
    if (wtx.IsCoinBase())
        entry.push_back(Pair("generated", true));
    if (confirms > 0)
    {
        entry.push_back(Pair("blockhash", wtx.hashBlock.GetHex()));
        entry.push_back(Pair("blockindex", wtx.nIndex));
        entry.push_back(Pair("blocktime", mapBlockIndex[wtx.hashBlock]->GetBlockTime()));
    } else {
        entry.push_back(Pair("trusted", wtx.IsTrusted()));
    }

    const auto hash = wtx.GetHash();
    entry.push_back(Pair("txid", hash.GetHex()));
    UniValue conflicts{UniValue::VARR};
    for (const auto& conflict : wtx.GetConflicts())
        conflicts.push_back(conflict.GetHex());
    entry.push_back(Pair("walletconflicts", conflicts));
    entry.push_back(Pair("time", wtx.GetTxTime()));
    entry.push_back(Pair("timereceived", (int64_t)wtx.nTimeReceived));

    // Add opt-in RBF status
    std::string rbfStatus{"no"};
    if (confirms <= 0) {
        LOCK(mempool.cs);
        RBFTransactionState rbfState = IsRBFOptIn(*wtx.tx, mempool);
        if (rbfState == RBF_TRANSACTIONSTATE_UNKNOWN)
            rbfStatus = "unknown";
        else if (rbfState == RBF_TRANSACTIONSTATE_REPLACEABLE_BIP125)
            rbfStatus = "yes";
    }
    entry.push_back(Pair("bip125-replaceable", rbfStatus));

    for (const auto& item : wtx.mapValue)
        entry.push_back(Pair(item.first, item.second));
}

std::string AccountFromValue(const UniValue& value)
{
    const auto& strAccount = value.get_str();
    if (strAccount == "*")
        throw JSONRPCError(RPCErrorCode::WALLET_INVALID_ACCOUNT_NAME, "Invalid account name");
    return strAccount;
}

UniValue getnewaddress(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error{
            "getnewaddress ( \"account\" )\n"
            "\nReturns a new PAIcoin address for receiving payments.\n"
            "If 'account' is specified (DEPRECATED), it is added to the address book \n"
            "so payments received with the address will be credited to 'account'.\n"
            "\nArguments:\n"
            "1. \"account\"        (string, optional) DEPRECATED. The account name for the address to be linked to. If not provided, the default account \"\" is used. It can also be set to the empty string \"\" to represent the default account. The account does not need to exist, it will be created if there is no account by the given name.\n"
            "\nResult:\n"
            "\"address\"    (string) The new paicoin address\n"
            "\nExamples:\n"
            + HelpExampleCli("getnewaddress", "")
            + HelpExampleRpc("getnewaddress", "")
        };

    LOCK2(cs_main, pwallet->cs_wallet);

    // Parse the account first so we don't generate a key if there's an error
    std::string strAccount;
    if (!request.params[0].isNull())
        strAccount = AccountFromValue(request.params[0]);

    if (!pwallet->IsLocked()) {
        pwallet->TopUpKeyPool();
    }

    // Generate a new key that is added to wallet
    CPubKey newKey;
    if (!pwallet->GetKeyFromPool(newKey)) {
        throw JSONRPCError(RPCErrorCode::WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
    }
    const auto keyID = newKey.GetID();

    pwallet->SetAddressBook(keyID, strAccount, "receive");

    return EncodeDestination(keyID);
}

UniValue createnewaccount(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error{
            "createnewaccount ( \"account\" )\n"
            "\nCreates a new account.\n"
            "The wallet must be unlocked for this request to succeed.\n"
            "\nArguments:\n"
            "1. \"account\"        (string) The account name for the address to be linked to.\n"
            "\nExamples:\n"
            + HelpExampleCli("createnewaccount", "A")
            + HelpExampleRpc("createnewaccount", "A")
        };

    if (request.params[0].isNull()) {
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "The account name must be provided");
    }
    getnewaddress(request);

    return NullUniValue;
}

UniValue renameaccount(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error{
            "renameaccount ( \"oldAccountName newAccountName\" )\n"
            "\nRenames an account.\n"
            "The wallet must be unlocked for this request to succeed.\n"
            "\nArguments:\n"
            "1. \"oldAccountName\"        (string) The existing account name to be renamed.\n"
            "2. \"newAccountName\"        (string) The account new name.\n"
            "\nExamples:\n"
            + HelpExampleCli("renamenewaccount", "spending savings")
            + HelpExampleRpc("createnewaccount", "spending savings")
        };

    if (request.params[0].isNull()) {
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "The existing account name must be provided");
    }
    if (request.params[1].isNull()) {
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "The new account name must be provided");
    }

    auto oldAccountName = request.params[0].get_str();
    auto newAccountName = request.params[1].get_str();

    LOCK2(cs_main, pwallet->cs_wallet);
    pwallet->RenameAddressBook(oldAccountName, newAccountName);
    return NullUniValue;
}

CTxDestination GetAccountAddress(CWallet* const pwallet, std::string strAccount, bool bForceNew=false)
{
    CPubKey pubKey;
    if (!pwallet->GetAccountPubkey(pubKey, strAccount, bForceNew)) {
        throw JSONRPCError(RPCErrorCode::WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
    }

    return pubKey.GetID();
}

UniValue getaccountaddress(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error{
            "getaccountaddress \"account\"\n"
            "\nDEPRECATED. Returns the current PAIcoin address for receiving payments to this account.\n"
            "\nArguments:\n"
            "1. \"account\"       (string, required) The account name for the address. It can also be set to the empty string \"\" to represent the default account. The account does not need to exist, it will be created and a new address created  if there is no account by the given name.\n"
            "\nResult:\n"
            "\"address\"          (string) The account paicoin address\n"
            "\nExamples:\n"
            + HelpExampleCli("getaccountaddress", "")
            + HelpExampleCli("getaccountaddress", "\"\"")
            + HelpExampleCli("getaccountaddress", "\"myaccount\"")
            + HelpExampleRpc("getaccountaddress", "\"myaccount\"")
        };

    LOCK2(cs_main, pwallet->cs_wallet);

    // Parse the account first so we don't generate a key if there's an error
    const auto strAccount = AccountFromValue(request.params[0]);

    return EncodeDestination(GetAccountAddress(pwallet, strAccount));
}


UniValue getrawchangeaddress(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 0)
        throw std::runtime_error{
            "getrawchangeaddress\n"
            "\nReturns a new PAIcoin address, for receiving change.\n"
            "This is for use with raw transactions, NOT normal use.\n"
            "\nResult:\n"
            "\"address\"    (string) The address\n"
            "\nExamples:\n"
            + HelpExampleCli("getrawchangeaddress", "")
            + HelpExampleRpc("getrawchangeaddress", "")
        };

    LOCK2(cs_main, pwallet->cs_wallet);

    if (!pwallet->IsLocked()) {
        pwallet->TopUpKeyPool();
    }

    CReserveKey reservekey{pwallet};
    CPubKey vchPubKey;
    if (!reservekey.GetReservedKey(vchPubKey, true))
        throw JSONRPCError(RPCErrorCode::WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");

    reservekey.KeepKey();

    const auto keyID = vchPubKey.GetID();

    return EncodeDestination(keyID);
}


UniValue setaccount(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error{
            "setaccount \"address\" \"account\"\n"
            "\nDEPRECATED. Sets the account associated with the given address.\n"
            "\nArguments:\n"
            "1. \"address\"         (string, required) The paicoin address to be associated with an account.\n"
            "2. \"account\"         (string, required) The account to assign the address to.\n"
            "\nExamples:\n"
            + HelpExampleCli("setaccount", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\" \"tabby\"")
            + HelpExampleRpc("setaccount", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\", \"tabby\"")
        };

    LOCK2(cs_main, pwallet->cs_wallet);

    const auto dest = DecodeDestination(request.params[0].get_str());
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Invalid PAIcoin address");
    }

    std::string strAccount;
    if (!request.params[1].isNull())
        strAccount = AccountFromValue(request.params[1]);

    // Only add the account if the address is yours.
    if (IsMine(*pwallet, dest)) {
        // Detect when changing the account of an address that is the 'unused current key' of another account:
        if (pwallet->mapAddressBook.count(dest)) {
            const auto strOldAccount = pwallet->mapAddressBook[dest].name;
            if (dest == GetAccountAddress(pwallet, strOldAccount)) {
                GetAccountAddress(pwallet, strOldAccount, true);
            }
        }
        pwallet->SetAddressBook(dest, strAccount, "receive");
    }
    else
        throw JSONRPCError(RPCErrorCode::MISC_ERROR, "setaccount can only be used with own address");

    return NullUniValue;
}


UniValue getaccount(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error{
            "getaccount \"address\"\n"
            "\nDEPRECATED. Returns the account associated with the given address.\n"
            "\nArguments:\n"
            "1. \"address\"         (string, required) The paicoin address for account lookup.\n"
            "\nResult:\n"
            "\"accountname\"        (string) the account address\n"
            "\nExamples:\n"
            + HelpExampleCli("getaccount", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\"")
            + HelpExampleRpc("getaccount", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\"")
        };

    LOCK2(cs_main, pwallet->cs_wallet);

    const auto dest = DecodeDestination(request.params[0].get_str());
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Invalid PAIcoin address");
    }

    std::string strAccount;
    auto mi = pwallet->mapAddressBook.find(dest);
    if (mi != pwallet->mapAddressBook.end() && !(*mi).second.name.empty()) {
        strAccount = (*mi).second.name;
    }
    return strAccount;
}


UniValue getaddressesbyaccount(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error{
            "getaddressesbyaccount \"account\"\n"
            "\nDEPRECATED. Returns the list of addresses for the given account.\n"
            "\nArguments:\n"
            "1. \"account\"        (string, required) The account name.\n"
            "\nResult:\n"
            "[                     (json array of string)\n"
            "  \"address\"         (string) a paicoin address associated with the given account\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddressesbyaccount", "\"tabby\"")
            + HelpExampleRpc("getaddressesbyaccount", "\"tabby\"")
        };

    LOCK2(cs_main, pwallet->cs_wallet);

    const auto strAccount = AccountFromValue(request.params[0]);

    // Find all addresses that have the given account
    UniValue ret{UniValue::VARR};
    for (const auto& item : pwallet->mapAddressBook) {
        const auto& dest = item.first;
        const auto& strName = item.second.name;
        if (strName == strAccount) {
            ret.push_back(EncodeDestination(dest));
        }
    }
    return ret;
}

static void SendMoney(CWallet * const pwallet, const CTxDestination &address, CAmount nValue, bool fSubtractFeeFromAmount, CWalletTx& wtxNew, const CCoinControl& coin_control)
{
    const auto curBalance = pwallet->GetBalance();

    // Check amount
    if (nValue <= 0)
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid amount");

    if (nValue > curBalance)
        throw JSONRPCError(RPCErrorCode::WALLET_INSUFFICIENT_FUNDS, "Insufficient funds");

    if (pwallet->GetBroadcastTransactions() && !g_connman) {
        throw JSONRPCError(RPCErrorCode::CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");
    }

    // Parse PAIcoin address
    const auto scriptPubKey = GetScriptForDestination(address);

    // Create and send the transaction
    CReserveKey reservekey{pwallet};
    CAmount nFeeRequired;
    std::string strError;
    std::vector<CRecipient> vecSend;
    int nChangePosRet{-1};
    CRecipient recipient = {scriptPubKey, nValue, fSubtractFeeFromAmount};
    vecSend.push_back(recipient);
    if (!pwallet->CreateTransaction(vecSend, wtxNew, reservekey, nFeeRequired, nChangePosRet, strError, coin_control)) {
        if (!fSubtractFeeFromAmount && nValue + nFeeRequired > curBalance)
            strError = strprintf("Error: This transaction requires a transaction fee of at least %s", FormatMoney(nFeeRequired));
        throw JSONRPCError(RPCErrorCode::WALLET_ERROR, strError);
    }
    CValidationState state;
    if (!pwallet->CommitTransaction(wtxNew, reservekey, g_connman.get(), state)) {
        strError = strprintf("Error: The transaction was rejected! Reason given: %s", state.GetRejectReason());
        throw JSONRPCError(RPCErrorCode::WALLET_ERROR, strError);
    }
}

UniValue sendtoaddress(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 2 || request.params.size() > 8)
        throw std::runtime_error(
            "sendtoaddress \"address\" amount ( \"comment\" \"comment_to\" subtractfeefromamount replaceable conf_target \"estimate_mode\")\n"
            "\nSend an amount to a given address.\n"
            + HelpRequiringPassphrase(pwallet) +
            "\nArguments:\n"
            "1. \"address\"            (string, required) The paicoin address to send to.\n"
            "2. \"amount\"             (numeric or string, required) The amount in " + CURRENCY_UNIT + " to send. eg 0.1\n"
            "3. \"comment\"            (string, optional) A comment used to store what the transaction is for. \n"
            "                             This is not part of the transaction, just kept in your wallet.\n"
            "4. \"comment_to\"         (string, optional) A comment to store the name of the person or organization \n"
            "                             to which you're sending the transaction. This is not part of the \n"
            "                             transaction, just kept in your wallet.\n"
            "5. subtractfeefromamount  (boolean, optional, default=false) The fee will be deducted from the amount being sent.\n"
            "                             The recipient will receive less paicoins than you enter in the amount field.\n"
            "6. replaceable            (boolean, optional) Allow this transaction to be replaced by a transaction with higher fees via BIP 125\n"
            "7. conf_target            (numeric, optional) Confirmation target (in blocks)\n"
            "8. \"estimate_mode\"      (string, optional, default=UNSET) The fee estimate mode, must be one of:\n"
            "       \"UNSET\"\n"
            "       \"ECONOMICAL\"\n"
            "       \"CONSERVATIVE\"\n"
            "\nResult:\n"
            "\"txid\"                  (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1")
            + HelpExampleCli("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 \"donation\" \"seans outpost\"")
            + HelpExampleCli("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 \"\" \"\" true")
            + HelpExampleRpc("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 0.1, \"donation\", \"seans outpost\"")
        );

    ObserveSafeMode();
    LOCK2(cs_main, pwallet->cs_wallet);

    const auto dest = DecodeDestination(request.params[0].get_str());
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Invalid address");
    }

    // Amount
    const auto nAmount = AmountFromValue(request.params[1]);
    if (nAmount <= 0)
        throw JSONRPCError(RPCErrorCode::TYPE_ERROR, "Invalid amount for send");

    // Wallet comments
    CWalletTx wtx;
    if (!request.params[2].isNull() && !request.params[2].get_str().empty())
        wtx.mapValue["comment"] = request.params[2].get_str();
    if (!request.params[3].isNull() && !request.params[3].get_str().empty())
        wtx.mapValue["to"]      = request.params[3].get_str();

    auto fSubtractFeeFromAmount = false;
    if (!request.params[4].isNull()) {
        fSubtractFeeFromAmount = request.params[4].get_bool();
    }

    CCoinControl coin_control;
    if (!request.params[5].isNull()) {
        coin_control.signalRbf = request.params[5].get_bool();
    }

    if (!request.params[6].isNull()) {
        coin_control.m_confirm_target = ParseConfirmTarget(request.params[6]);
    }

    if (!request.params[7].isNull()) {
        if (!FeeModeFromString(request.params[7].get_str(), coin_control.m_fee_mode)) {
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid estimate_mode parameter");
        }
    }


    EnsureWalletIsUnlocked(pwallet);

    SendMoney(pwallet, dest, nAmount, fSubtractFeeFromAmount, wtx, coin_control);

    return wtx.GetHash().GetHex();
}

UniValue listaddressgroupings(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error{
            "listaddressgroupings\n"
            "\nLists groups of addresses which have had their common ownership\n"
            "made public by common use as inputs or as the resulting change\n"
            "in past transactions\n"
            "\nResult:\n"
            "[\n"
            "  [\n"
            "    [\n"
            "      \"address\",            (string) The paicoin address\n"
            "      amount,                 (numeric) The amount in " + CURRENCY_UNIT + "\n"
            "      \"account\"             (string, optional) DEPRECATED. The account\n"
            "    ]\n"
            "    ,...\n"
            "  ]\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("listaddressgroupings", "")
            + HelpExampleRpc("listaddressgroupings", "")
        };

    ObserveSafeMode();
    LOCK2(cs_main, pwallet->cs_wallet);

    UniValue jsonGroupings{UniValue::VARR};
    auto balances = pwallet->GetAddressBalances();
    for (const auto& grouping : pwallet->GetAddressGroupings()) {
        UniValue jsonGrouping{UniValue::VARR};
        for (const auto& address : grouping)
        {
            UniValue addressInfo{UniValue::VARR};
            addressInfo.push_back(EncodeDestination(address));
            addressInfo.push_back(ValueFromAmount(balances[address]));
            {
                if (pwallet->mapAddressBook.find(address) != pwallet->mapAddressBook.end()) {
                    addressInfo.push_back(pwallet->mapAddressBook.find(address)->second.name);
                }
            }
            jsonGrouping.push_back(addressInfo);
        }
        jsonGroupings.push_back(jsonGrouping);
    }
    return jsonGroupings;
}

UniValue signmessage(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error{
            "signmessage \"address\" \"message\"\n"
            "\nSign a message with the private key of an address"
            + HelpRequiringPassphrase(pwallet) + "\n"
            "\nArguments:\n"
            "1. \"address\"         (string, required) The paicoin address to use for the private key.\n"
            "2. \"message\"         (string, required) The message to create a signature of.\n"
            "\nResult:\n"
            "\"signature\"          (string) The signature of the message encoded in base 64\n"
            "\nExamples:\n"
            "\nUnlock the wallet for 30 seconds\n"
            + HelpExampleCli("walletpassphrase", "\"mypassphrase\" 30") +
            "\nCreate the signature\n"
            + HelpExampleCli("signmessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\" \"my message\"") +
            "\nVerify the signature\n"
            + HelpExampleCli("verifymessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\" \"signature\" \"my message\"") +
            "\nAs json rpc\n"
            + HelpExampleRpc("signmessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\", \"my message\"")
        };

    LOCK2(cs_main, pwallet->cs_wallet);

    EnsureWalletIsUnlocked(pwallet);

    const auto& strAddress = request.params[0].get_str();
    const auto& strMessage = request.params[1].get_str();

    const auto dest = DecodeDestination(strAddress);
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPCErrorCode::TYPE_ERROR, "Invalid address");
    }

    const auto * const keyID = boost::get<CKeyID>(&dest);
    if (!keyID) {
        throw JSONRPCError(RPCErrorCode::TYPE_ERROR, "Address does not refer to key");
    }

    CKey key;
    if (!pwallet->GetKey(*keyID, key)) {
        throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "Private key not available");
    }

    CHashWriter ss{SER_GETHASH, 0};
    ss << strMessageMagic;
    ss << strMessage;

    std::vector<unsigned char> vchSig;
    if (!key.SignCompact(ss.GetHash(), vchSig))
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Sign failed");

    return EncodeBase64(vchSig.data(), vchSig.size());
}

UniValue getreceivedbyaddress(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error{
            "getreceivedbyaddress \"address\" ( minconf )\n"
            "\nReturns the total amount received by the given address in transactions with at least minconf confirmations.\n"
            "\nArguments:\n"
            "1. \"address\"         (string, required) The paicoin address for transactions.\n"
            "2. minconf             (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "\nResult:\n"
            "amount   (numeric) The total amount in " + CURRENCY_UNIT + " received at this address.\n"
            "\nExamples:\n"
            "\nThe amount from transactions with at least 1 confirmation\n"
            + HelpExampleCli("getreceivedbyaddress", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\"") +
            "\nThe amount including unconfirmed transactions, zero confirmations\n"
            + HelpExampleCli("getreceivedbyaddress", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\" 0") +
            "\nThe amount with at least 6 confirmations\n"
            + HelpExampleCli("getreceivedbyaddress", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\" 6") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("getreceivedbyaddress", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\", 6")
        };

    ObserveSafeMode();
    LOCK2(cs_main, pwallet->cs_wallet);

    // PAIcoin address
    const auto dest = DecodeDestination(request.params[0].get_str());
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Invalid PAIcoin address");
    }
    const auto scriptPubKey = GetScriptForDestination(dest);
    if (!IsMine(*pwallet, scriptPubKey)) {
        return ValueFromAmount(0);
    }

    // Minimum confirmations
    int nMinDepth{1};
    if (!request.params[1].isNull())
        nMinDepth = request.params[1].get_int();

    // Tally
    CAmount nAmount{0};
    for (const auto& pairWtx : pwallet->mapWallet) {
        const auto& wtx = pairWtx.second;
        if (wtx.IsCoinBase() || !CheckFinalTx(*wtx.tx))
            continue;

        for (const auto& txout : wtx.tx->vout)
            if (txout.scriptPubKey == scriptPubKey)
                if (wtx.GetDepthInMainChain() >= nMinDepth)
                    nAmount += txout.nValue;
    }

    return ValueFromAmount(nAmount);
}

UniValue getticketfee(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error{
            "getticketfee\n"
            "\nGet the current fee per kB of the serialized tx size used for an authored stake transaction.\n"
            "\nArguments:\n"
            "None\n"
            "\nResult:\n"
            "n.nnn (numeric)    The current fee\n"
            + HelpExampleCli("getticketfee", "")
            + HelpExampleRpc("getticketfee", "")
        };

    auto ret = UniValue{0.0};
    return ret;
}

UniValue gettickets(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error{
            "gettickets includeimmature\n"
            "\nReturning the hashes of the tickets currently owned by wallet.\n"
            "\nArguments:\n"
            "1. includeimmature                 (boolean, required)     If true include immature tickets in the results.\n"
            "\nResult:\n"
            "{\n"
            "   \"hashes\": [\"value\",...],    (array of string)       Hashes of the tickets owned by the wallet encoded as strings\n"
            "}\n"
            + HelpExampleCli("gettickets", "true")
            + HelpExampleRpc("gettickets", "false")
        };

    const auto& bIncludeImmature = request.params[0].get_bool();

    ObserveSafeMode();
    LOCK2(cs_main, pwallet->cs_wallet);

    const auto& txOrdered = pwallet->wtxOrdered;

    auto tx_arr = UniValue{UniValue::VARR};
    for (const auto& it : txOrdered) {
        const auto* const pwtx = it.second.first;

        if (pwtx != nullptr) {

            std::string reason;
            const CTransaction& current_tx = *(pwtx->tx);
            if (ParseTxClass(current_tx) != TX_BuyTicket)
                continue;

            const auto& confirmations = pwtx->GetDepthInMainChain();
            if (confirmations < 0)
                continue;

            if (pwtx->isAbandoned())
                continue;

            if (!bIncludeImmature && confirmations < Params().GetConsensus().nTicketMaturity)
                continue;

            tx_arr.push_back(pwtx->GetHash().GetHex());
        }
    }

    auto ret = UniValue{UniValue::VOBJ};
    ret.pushKV("hashes",tx_arr);
    return ret;
}

UniValue revoketickets(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error{
            "revoketickets\n"
            "\nRequests the wallet create revovactions for any previously missed tickets.  Wallet must be unlocked.\n"
            "\nArguments:\n"
            "None\n"
            "\nResult:\n"
            "Nothing\n"
            + HelpExampleCli("revoketickets", "")
            + HelpExampleRpc("revoketickets", "")
        };

    auto ret = UniValue{UniValue::VNULL};
    return ret;
}

UniValue listtickets(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error{
            "listtickets\n"
            "\nProduces an array of ticket information objects.\n"
            "\nArguments:\n"
            "None\n"
            "\nResult:\n"
            "[\n"
            "   {\n"
            "       \"ticket\": { ... }   JSON object containting ticket information\n"
            "   }\n"
            "]\n"
            + HelpExampleCli("listtickets", "")
            + HelpExampleRpc("listtickets", "")
        };

    auto ret = UniValue{UniValue::VARR};
    return ret;
}

UniValue setticketfee(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error{
            "setticketfee fee\n"
            "\nModify the fee per kB of the serialized tx size used each time more fee is required for an authored stake transaction.\n"
            "\nArguments:\n"
            "1. fee     (numeric, required) The new fee per kB of the serialized tx size valued in PAI\n"
            "\nResult:\n"
            "true|false (boolean)           The boolean return status\n"
            + HelpExampleCli("setticketfee", "0.02")
            + HelpExampleRpc("setticketfee", "0.02")
        };

    const auto& nFee = request.params[0].get_real();

    auto ret = UniValue{UniValue::VBOOL};
    return ret;
}

UniValue getreceivedbyaccount(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error{
            "getreceivedbyaccount \"account\" ( minconf )\n"
            "\nDEPRECATED. Returns the total amount received by addresses with <account> in transactions with at least [minconf] confirmations.\n"
            "\nArguments:\n"
            "1. \"account\"      (string, required) The selected account, may be the default account using \"\".\n"
            "2. minconf          (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "\nResult:\n"
            "amount              (numeric) The total amount in " + CURRENCY_UNIT + " received for this account.\n"
            "\nExamples:\n"
            "\nAmount received by the default account with at least 1 confirmation\n"
            + HelpExampleCli("getreceivedbyaccount", "\"\"") +
            "\nAmount received at the tabby account including unconfirmed amounts with zero confirmations\n"
            + HelpExampleCli("getreceivedbyaccount", "\"tabby\" 0") +
            "\nThe amount with at least 6 confirmations\n"
            + HelpExampleCli("getreceivedbyaccount", "\"tabby\" 6") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("getreceivedbyaccount", "\"tabby\", 6")
        };

    ObserveSafeMode();
    LOCK2(cs_main, pwallet->cs_wallet);

    // Minimum confirmations
    int nMinDepth{1};
    if (!request.params[1].isNull())
        nMinDepth = request.params[1].get_int();

    // Get the set of pub keys assigned to account
    const auto strAccount = AccountFromValue(request.params[0]);
    const auto setAddress = pwallet->GetAccountAddresses(strAccount);

    // Tally
    CAmount nAmount{0};
    for (const auto& pairWtx : pwallet->mapWallet) {
        const auto& wtx = pairWtx.second;
        if (wtx.IsCoinBase() || !CheckFinalTx(*wtx.tx))
            continue;

        for (const auto& txout : wtx.tx->vout)
        {
            CTxDestination address;
            if (ExtractDestination(txout.scriptPubKey, address) && IsMine(*pwallet, address) && setAddress.count(address)) {
                if (wtx.GetDepthInMainChain() >= nMinDepth)
                    nAmount += txout.nValue;
            }
        }
    }

    return ValueFromAmount(nAmount);
}


UniValue getbalance(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 3)
        throw std::runtime_error{
            "getbalance ( \"account\" minconf include_watchonly )\n"
            "\nIf account is not specified, returns the server's total available balance.\n"
            "If account is specified (DEPRECATED), returns the balance in the account.\n"
            "Note that the account \"\" is not the same as leaving the parameter out.\n"
            "The server total may be different to the balance in the default \"\" account.\n"
            "\nArguments:\n"
            "1. \"account\"         (string, optional) DEPRECATED. The account string may be given as a\n"
            "                     specific account name to find the balance associated with wallet keys in\n"
            "                     a named account, or as the empty string (\"\") to find the balance\n"
            "                     associated with wallet keys not in any named account, or as \"*\" to find\n"
            "                     the balance associated with all wallet keys regardless of account.\n"
            "                     When this option is specified, it calculates the balance in a different\n"
            "                     way than when it is not specified, and which can count spends twice when\n"
            "                     there are conflicting pending transactions (such as those created by\n"
            "                     the bumpfee command), temporarily resulting in low or even negative\n"
            "                     balances. In general, account balance calculation is not considered\n"
            "                     reliable and has resulted in confusing outcomes, so it is recommended to\n"
            "                     avoid passing this argument.\n"
            "2. minconf           (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "3. include_watchonly (bool, optional, default=false) Also include balance in watch-only addresses (see 'importaddress')\n"
            "\nResult:\n"
            "amount              (numeric) The total amount in " + CURRENCY_UNIT + " received for this account.\n"
            "\nExamples:\n"
            "\nThe total amount in the wallet with 1 or more confirmations\n"
            + HelpExampleCli("getbalance", "") +
            "\nThe total amount in the wallet at least 6 blocks confirmed\n"
            + HelpExampleCli("getbalance", "\"*\" 6") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("getbalance", "\"*\", 6")
        };

    ObserveSafeMode();
    LOCK2(cs_main, pwallet->cs_wallet);

    const auto& account_value = request.params[0];
    const auto& minconf = request.params[1];
    const auto& include_watchonly = request.params[2];

    if (account_value.isNull()) {
        if (!minconf.isNull()) {
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER,
                "getbalance minconf option is only currently supported if an account is specified");
        }
        if (!include_watchonly.isNull()) {
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER,
                "getbalance include_watchonly option is only currently supported if an account is specified");
        }
        return ValueFromAmount(pwallet->GetBalance());
    }

    const auto& account_param = account_value.get_str();
    const std::string* account{account_param != "*" ? &account_param : nullptr};

    int nMinDepth{1};
    if (!minconf.isNull())
        nMinDepth = minconf.get_int();
    isminefilter filter{ISMINE_SPENDABLE};
    if(!include_watchonly.isNull())
        if(include_watchonly.get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    return ValueFromAmount(pwallet->GetLegacyBalance(filter, nMinDepth, account));
}

UniValue getunconfirmedbalance(const JSONRPCRequest &request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 0)
        throw std::runtime_error{
                "getunconfirmedbalance\n"
                "Returns the server's total unconfirmed balance\n"};

    ObserveSafeMode();
    LOCK2(cs_main, pwallet->cs_wallet);

    return ValueFromAmount(pwallet->GetUnconfirmedBalance());
}


UniValue movecmd(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 3 || request.params.size() > 5)
        throw std::runtime_error{
            "move \"fromaccount\" \"toaccount\" amount ( minconf \"comment\" )\n"
            "\nDEPRECATED. Move a specified amount from one account in your wallet to another.\n"
            "\nArguments:\n"
            "1. \"fromaccount\"   (string, required) The name of the account to move funds from. May be the default account using \"\".\n"
            "2. \"toaccount\"     (string, required) The name of the account to move funds to. May be the default account using \"\".\n"
            "3. amount            (numeric) Quantity of " + CURRENCY_UNIT + " to move between accounts.\n"
            "4. (dummy)           (numeric, optional) Ignored. Remains for backward compatibility.\n"
            "5. \"comment\"       (string, optional) An optional comment, stored in the wallet only.\n"
            "\nResult:\n"
            "true|false           (boolean) true if successful.\n"
            "\nExamples:\n"
            "\nMove 0.01 " + CURRENCY_UNIT + " from the default account to the account named tabby\n"
            + HelpExampleCli("move", "\"\" \"tabby\" 0.01") +
            "\nMove 0.01 " + CURRENCY_UNIT + " timotei to akiko with a comment and funds have 6 confirmations\n"
            + HelpExampleCli("move", "\"timotei\" \"akiko\" 0.01 6 \"happy birthday!\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("move", "\"timotei\", \"akiko\", 0.01, 6, \"happy birthday!\"")
        };

    ObserveSafeMode();
    LOCK2(cs_main, pwallet->cs_wallet);

    const auto strFrom = AccountFromValue(request.params[0]);
    const auto strTo = AccountFromValue(request.params[1]);
    const auto nAmount = AmountFromValue(request.params[2]);
    if (nAmount <= 0)
        throw JSONRPCError(RPCErrorCode::TYPE_ERROR, "Invalid amount for send");
    if (!request.params[3].isNull())
        // unused parameter, used to be nMinDepth, keep type-checking it though
        (void)request.params[3].get_int();
    std::string strComment;
    if (!request.params[4].isNull())
        strComment = request.params[4].get_str();

    if (!pwallet->AccountMove(strFrom, strTo, nAmount, strComment)) {
        throw JSONRPCError(RPCErrorCode::DATABASE_ERROR, "database error");
    }

    return true;
}

UniValue sendfrom(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 3 || request.params.size() > 6)
        throw std::runtime_error{
            "sendfrom \"fromaccount\" \"toaddress\" amount ( minconf \"comment\" \"comment_to\" )\n"
            "\nDEPRECATED (use sendtoaddress). Sent an amount from an account to a paicoin address."
            + HelpRequiringPassphrase(pwallet) + "\n"
            "\nArguments:\n"
            "1. \"fromaccount\"       (string, required) The name of the account to send funds from. May be the default account using \"\".\n"
            "                       Specifying an account does not influence coin selection, but it does associate the newly created\n"
            "                       transaction with the account, so the account's balance computation and transaction history can reflect\n"
            "                       the spend.\n"
            "2. \"toaddress\"         (string, required) The paicoin address to send funds to.\n"
            "3. amount                (numeric or string, required) The amount in " + CURRENCY_UNIT + " (transaction fee is added on top).\n"
            "4. minconf               (numeric, optional, default=1) Only use funds with at least this many confirmations.\n"
            "5. \"comment\"           (string, optional) A comment used to store what the transaction is for. \n"
            "                                     This is not part of the transaction, just kept in your wallet.\n"
            "6. \"comment_to\"        (string, optional) An optional comment to store the name of the person or organization \n"
            "                                     to which you're sending the transaction. This is not part of the transaction, \n"
            "                                     it is just kept in your wallet.\n"
            "\nResult:\n"
            "\"txid\"                 (string) The transaction id.\n"
            "\nExamples:\n"
            "\nSend 0.01 " + CURRENCY_UNIT + " from the default account to the address, must have at least 1 confirmation\n"
            + HelpExampleCli("sendfrom", "\"\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.01") +
            "\nSend 0.01 from the tabby account to the given address, funds must have at least 6 confirmations\n"
            + HelpExampleCli("sendfrom", "\"tabby\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.01 6 \"donation\" \"seans outpost\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("sendfrom", "\"tabby\", \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 0.01, 6, \"donation\", \"seans outpost\"")
        };

    ObserveSafeMode();
    LOCK2(cs_main, pwallet->cs_wallet);

    const auto strAccount = AccountFromValue(request.params[0]);
    const auto dest = DecodeDestination(request.params[1].get_str());
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Invalid PAIcoin address");
    }
    const auto nAmount = AmountFromValue(request.params[2]);
    if (nAmount <= 0)
        throw JSONRPCError(RPCErrorCode::TYPE_ERROR, "Invalid amount for send");
    int nMinDepth{1};
    if (!request.params[3].isNull())
        nMinDepth = request.params[3].get_int();

    CWalletTx wtx;
    wtx.strFromAccount = strAccount;
    if (!request.params[4].isNull() && !request.params[4].get_str().empty())
        wtx.mapValue["comment"] = request.params[4].get_str();
    if (!request.params[5].isNull() && !request.params[5].get_str().empty())
        wtx.mapValue["to"]      = request.params[5].get_str();

    EnsureWalletIsUnlocked(pwallet);

    // Check funds
    const auto nBalance = pwallet->GetLegacyBalance(ISMINE_SPENDABLE, nMinDepth, &strAccount);
    if (nAmount > nBalance)
        throw JSONRPCError(RPCErrorCode::WALLET_INSUFFICIENT_FUNDS, "Account has insufficient funds");

    SendMoney(pwallet, dest, nAmount, false, wtx, CCoinControl{}); // This is a deprecated API

    return wtx.GetHash().GetHex();
}

UniValue sendmany(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 2 || request.params.size() > 8)
        throw std::runtime_error{
            "sendmany \"fromaccount\" {\"address\":amount,...} ( minconf \"comment\" [\"address\",...] replaceable conf_target \"estimate_mode\")\n"
            "\nSend multiple times. Amounts are double-precision floating point numbers."
            + HelpRequiringPassphrase(pwallet) + "\n"
            "\nArguments:\n"
            "1. \"fromaccount\"         (string, required) DEPRECATED. The account to send the funds from. Should be \"\" for the default account\n"
            "2. \"amounts\"             (string, required) A json object with addresses and amounts\n"
            "    {\n"
            "      \"address\":amount   (numeric or string) The paicoin address is the key, the numeric amount (can be string) in " + CURRENCY_UNIT + " is the value\n"
            "      ,...\n"
            "    }\n"
            "3. minconf                 (numeric, optional, default=1) Only use the balance confirmed at least this many times.\n"
            "4. \"comment\"             (string, optional) A comment\n"
            "5. subtractfeefrom         (array, optional) A json array with addresses.\n"
            "                           The fee will be equally deducted from the amount of each selected address.\n"
            "                           Those recipients will receive less paicoins than you enter in their corresponding amount field.\n"
            "                           If no addresses are specified here, the sender pays the fee.\n"
            "    [\n"
            "      \"address\"          (string) Subtract fee from this address\n"
            "      ,...\n"
            "    ]\n"
            "6. replaceable            (boolean, optional) Allow this transaction to be replaced by a transaction with higher fees via BIP 125\n"
            "7. conf_target            (numeric, optional) Confirmation target (in blocks)\n"
            "8. \"estimate_mode\"      (string, optional, default=UNSET) The fee estimate mode, must be one of:\n"
            "       \"UNSET\"\n"
            "       \"ECONOMICAL\"\n"
            "       \"CONSERVATIVE\"\n"
             "\nResult:\n"
            "\"txid\"                   (string) The transaction id for the send. Only 1 transaction is created regardless of \n"
            "                                    the number of addresses.\n"
            "\nExamples:\n"
            "\nSend two amounts to two different addresses:\n"
            + HelpExampleCli("sendmany", "\"\" \"{\\\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\\\":0.01,\\\"1353tsE8YMTA4EuV7dgUXGjNFf9KpVvKHz\\\":0.02}\"") +
            "\nSend two amounts to two different addresses setting the confirmation and comment:\n"
            + HelpExampleCli("sendmany", "\"\" \"{\\\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\\\":0.01,\\\"1353tsE8YMTA4EuV7dgUXGjNFf9KpVvKHz\\\":0.02}\" 6 \"testing\"") +
            "\nSend two amounts to two different addresses, subtract fee from amount:\n"
            + HelpExampleCli("sendmany", "\"\" \"{\\\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\\\":0.01,\\\"1353tsE8YMTA4EuV7dgUXGjNFf9KpVvKHz\\\":0.02}\" 1 \"\" \"[\\\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\\\",\\\"1353tsE8YMTA4EuV7dgUXGjNFf9KpVvKHz\\\"]\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("sendmany", "\"\", \"{\\\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX\\\":0.01,\\\"1353tsE8YMTA4EuV7dgUXGjNFf9KpVvKHz\\\":0.02}\", 6, \"testing\"")
        };

    ObserveSafeMode();
    LOCK2(cs_main, pwallet->cs_wallet);

    if (pwallet->GetBroadcastTransactions() && !g_connman) {
        throw JSONRPCError(RPCErrorCode::CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");
    }

    std::string strAccount = AccountFromValue(request.params[0]);
    UniValue sendTo = request.params[1].get_obj();
    int nMinDepth = 1;
    if (!request.params[2].isNull())
        nMinDepth = request.params[2].get_int();

    CWalletTx wtx;
    wtx.strFromAccount = strAccount;
    if (!request.params[3].isNull() && !request.params[3].get_str().empty())
        wtx.mapValue["comment"] = request.params[3].get_str();

    UniValue subtractFeeFromAmount{UniValue::VARR};
    if (!request.params[4].isNull())
        subtractFeeFromAmount = request.params[4].get_array();

    CCoinControl coin_control;
    if (!request.params[5].isNull()) {
        coin_control.signalRbf = request.params[5].get_bool();
    }

    if (!request.params[6].isNull()) {
        coin_control.m_confirm_target = ParseConfirmTarget(request.params[6]);
    }

    if (!request.params[7].isNull()) {
        if (!FeeModeFromString(request.params[7].get_str(), coin_control.m_fee_mode)) {
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid estimate_mode parameter");
        }
    }

    std::set<CTxDestination> destinations;
    std::vector<CRecipient> vecSend;

    CAmount totalAmount = 0;
    for (const auto& name_ : sendTo.getKeys()) {
        const auto dest = DecodeDestination(name_);
        if (!IsValidDestination(dest)) {
            throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, std::string("Invalid PAIcoin address: ") + name_);
        }

        if (destinations.count(dest)) {
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, std::string("Invalid parameter, duplicated address: ") + name_);
        }
        destinations.insert(dest);

        const auto scriptPubKey = GetScriptForDestination(dest);
        const auto nAmount = AmountFromValue(sendTo[name_]);
        if (nAmount <= 0)
            throw JSONRPCError(RPCErrorCode::TYPE_ERROR, "Invalid amount for send");
        totalAmount += nAmount;

        auto fSubtractFeeFromAmount = false;
        for (size_t idx{0}; idx < subtractFeeFromAmount.size(); ++idx) {
            const auto& addr = subtractFeeFromAmount[idx];
            if (addr.get_str() == name_)
                fSubtractFeeFromAmount = true;
        }

        const CRecipient recipient = {scriptPubKey, nAmount, fSubtractFeeFromAmount};
        vecSend.push_back(recipient);
    }

    EnsureWalletIsUnlocked(pwallet);

    // Check funds
    const auto nBalance = pwallet->GetLegacyBalance(ISMINE_SPENDABLE, nMinDepth, &strAccount);
    if (totalAmount > nBalance)
        throw JSONRPCError(RPCErrorCode::WALLET_INSUFFICIENT_FUNDS, "Account has insufficient funds");

    // Send
    CReserveKey keyChange{pwallet};
    CAmount nFeeRequired{0};
    int nChangePosRet{-1};
    std::string strFailReason;
    const auto fCreated = pwallet->CreateTransaction(vecSend, wtx, keyChange, nFeeRequired, nChangePosRet, strFailReason, coin_control);
    if (!fCreated)
        throw JSONRPCError(RPCErrorCode::WALLET_INSUFFICIENT_FUNDS, strFailReason);
    CValidationState state;
    if (!pwallet->CommitTransaction(wtx, keyChange, g_connman.get(), state)) {
        strFailReason = strprintf("Transaction commit failed:: %s", state.GetRejectReason());
        throw JSONRPCError(RPCErrorCode::WALLET_ERROR, strFailReason);
    }

    return wtx.GetHash().GetHex();
}

// Defined in rpc/misc.cpp
extern CScript _createmultisig_redeemScript(const CWallet * const pwallet, const UniValue& params);

UniValue addmultisigaddress(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 2 || request.params.size() > 3)
    {
        throw std::runtime_error{
            "addmultisigaddress nrequired [\"key\",...] ( \"account\" )\n"
            "\nAdd a nrequired-to-sign multisignature address to the wallet.\n"
            "Each key is a PAIcoin address or hex-encoded public key.\n"
            "If 'account' is specified (DEPRECATED), assign address to that account.\n"

            "\nArguments:\n"
            "1. nrequired        (numeric, required) The number of required signatures out of the n keys or addresses.\n"
            "2. \"keys\"         (string, required) A json array of paicoin addresses or hex-encoded public keys\n"
            "     [\n"
            "       \"address\"  (string) paicoin address or hex-encoded public key\n"
            "       ...,\n"
            "     ]\n"
            "3. \"account\"      (string, optional) DEPRECATED. An account to assign the addresses to.\n"

            "\nResult:\n"
            "\"address\"         (string) A paicoin address associated with the keys.\n"

            "\nExamples:\n"
            "\nAdd a multisig address from 2 addresses\n"
            + HelpExampleCli("addmultisigaddress", "2 \"[\\\"16sSauSf5pF2UkUwvKGq4qjNRzBZYqgEL5\\\",\\\"171sgjn4YtPu27adkKGrdDwzRTxnRkBfKV\\\"]\"") +
            "\nAs json rpc call\n"
            + HelpExampleRpc("addmultisigaddress", "2, \"[\\\"16sSauSf5pF2UkUwvKGq4qjNRzBZYqgEL5\\\",\\\"171sgjn4YtPu27adkKGrdDwzRTxnRkBfKV\\\"]\"")
        };
    }

    LOCK2(cs_main, pwallet->cs_wallet);

    std::string strAccount;
    if (!request.params[2].isNull())
        strAccount = AccountFromValue(request.params[2]);

    // Construct using pay-to-script-hash:
    const auto inner = _createmultisig_redeemScript(pwallet, request.params);
    CScriptID innerID{inner};
    pwallet->AddCScript(inner);

    pwallet->SetAddressBook(innerID, strAccount, "send");
    return EncodeDestination(innerID);
}

class Witnessifier : public boost::static_visitor<bool>
{
public:
    CWallet * const pwallet;
    CScriptID result;

    explicit Witnessifier(CWallet *_pwallet) : pwallet{_pwallet} {}

    bool operator()(const CNoDestination&) const { return false; }

    bool operator()(const CKeyID &keyID) {
        if (pwallet) {
            const auto basescript = GetScriptForDestination(keyID);
            const auto witscript = GetScriptForWitness(basescript);
            SignatureData sigs;
            // This check is to make sure that the script we created can actually be solved for and signed by us
            // if we were to have the private keys. This is just to make sure that the script is valid and that,
            // if found in a transaction, we would still accept and relay that transaction.
            if (!ProduceSignature(DummySignatureCreator(pwallet), witscript, sigs) ||
                !VerifyScript(sigs.scriptSig, witscript, &sigs.scriptWitness, MANDATORY_SCRIPT_VERIFY_FLAGS | SCRIPT_VERIFY_WITNESS_PUBKEYTYPE, DummySignatureCreator(pwallet).Checker())) {
                return false;
            }
            pwallet->AddCScript(witscript);
            result = CScriptID(witscript);
            return true;
        }
        return false;
    }

    bool operator()(const CScriptID &scriptID) {
        CScript subscript;
        if (pwallet && pwallet->GetCScript(scriptID, subscript)) {
            int witnessversion;
            std::vector<unsigned char> witprog;
            if (subscript.IsWitnessProgram(witnessversion, witprog)) {
                result = scriptID;
                return true;
            }
            const auto witscript = GetScriptForWitness(subscript);
            SignatureData sigs;
            // This check is to make sure that the script we created can actually be solved for and signed by us
            // if we were to have the private keys. This is just to make sure that the script is valid and that,
            // if found in a transaction, we would still accept and relay that transaction.
            if (!ProduceSignature(DummySignatureCreator(pwallet), witscript, sigs) ||
                !VerifyScript(sigs.scriptSig, witscript, &sigs.scriptWitness, MANDATORY_SCRIPT_VERIFY_FLAGS | SCRIPT_VERIFY_WITNESS_PUBKEYTYPE, DummySignatureCreator(pwallet).Checker())) {
                return false;
            }
            pwallet->AddCScript(witscript);
            result = CScriptID(witscript);
            return true;
        }
        return false;
    }
};

UniValue addwitnessaddress(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 1)
    {
        throw std::runtime_error{"addwitnessaddress \"address\"\n"
            "\nAdd a witness address for a script (with pubkey or redeemscript known).\n"
            "It returns the witness script.\n"

            "\nArguments:\n"
            "1. \"address\"       (string, required) An address known to the wallet\n"

            "\nResult:\n"
            "\"witnessaddress\",  (string) The value of the new address (P2SH of witness script).\n"
            "}\n"
        };
    }

    {
        LOCK(cs_main);
        if (!IsWitnessEnabled(chainActive.Tip(), Params().GetConsensus()) && !gArgs.GetBoolArg("-walletprematurewitness", false)) {
            throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "Segregated witness not enabled on network");
        }
    }

    const auto dest = DecodeDestination(request.params[0].get_str());
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Invalid PAIcoin address");
    }

    Witnessifier w{pwallet};
    if (!boost::apply_visitor(w, dest)) {
        throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "Public key or redeemscript not known to wallet, or the key is uncompressed");
    }

    pwallet->SetAddressBook(w.result, "", "receive");

    return EncodeDestination(w.result);
}

struct tallyitem
{
    CAmount nAmount;
    int nConf;
    std::vector<uint256> txids;
    bool fIsWatchonly;
    tallyitem()
    {
        nAmount = 0;
        nConf = std::numeric_limits<int>::max();
        fIsWatchonly = false;
    }
};

static UniValue ListReceived(const CWallet * const pwallet, const UniValue& params, bool fByAccounts)
{
    // Minimum confirmations
    int nMinDepth{1};
    if (!params[0].isNull())
        nMinDepth = params[0].get_int();

    // Whether to include empty accounts
    auto fIncludeEmpty = false;
    if (!params[1].isNull())
        fIncludeEmpty = params[1].get_bool();

    isminefilter filter{ISMINE_SPENDABLE};
    if(!params[2].isNull())
        if(params[2].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    // Tally
    std::map<CTxDestination, tallyitem> mapTally;
    for (const auto& pairWtx : pwallet->mapWallet) {
        const auto& wtx = pairWtx.second;

        if (wtx.IsCoinBase() || !CheckFinalTx(*wtx.tx))
            continue;

        const auto nDepth = wtx.GetDepthInMainChain();
        if (nDepth < nMinDepth)
            continue;

        for (const auto& txout : wtx.tx->vout)
        {
            CTxDestination address;
            if (!ExtractDestination(txout.scriptPubKey, address))
                continue;

            isminefilter mine{static_cast<isminefilter>(IsMine(*pwallet, address))};
            if(!(mine & filter))
                continue;

            auto& item = mapTally[address];
            item.nAmount += txout.nValue;
            item.nConf = std::min(item.nConf, nDepth);
            item.txids.push_back(wtx.GetHash());
            if (mine & ISMINE_WATCH_ONLY)
                item.fIsWatchonly = true;
        }
    }

    // Reply
    UniValue ret{UniValue::VARR};
    std::map<std::string, tallyitem> mapAccountTally;
    for (const auto& item : pwallet->mapAddressBook) {
        const auto& dest = item.first;
        const auto& strAccount = item.second.name;
        auto it = mapTally.find(dest);
        if (it == mapTally.end() && !fIncludeEmpty)
            continue;

        CAmount nAmount{0};
        auto nConf = std::numeric_limits<int>::max();
        auto fIsWatchonly = false;
        if (it != mapTally.end())
        {
            nAmount = (*it).second.nAmount;
            nConf = (*it).second.nConf;
            fIsWatchonly = (*it).second.fIsWatchonly;
        }

        if (fByAccounts)
        {
            auto& _item = mapAccountTally[strAccount];
            _item.nAmount += nAmount;
            _item.nConf = std::min(_item.nConf, nConf);
            _item.fIsWatchonly = fIsWatchonly;
        }
        else
        {
            UniValue obj{UniValue::VOBJ};
            if(fIsWatchonly)
                obj.push_back(Pair("involvesWatchonly", true));

            obj.push_back(Pair("address",       EncodeDestination(dest)));
            obj.push_back(Pair("account",       strAccount));
            obj.push_back(Pair("amount",        ValueFromAmount(nAmount)));
            obj.push_back(Pair("confirmations", (nConf == std::numeric_limits<int>::max() ? 0 : nConf)));
            if (!fByAccounts)
                obj.push_back(Pair("label", strAccount));
            UniValue transactions{UniValue::VARR};
            if (it != mapTally.end())
            {
                for (const auto& _item : (*it).second.txids)
                {
                    transactions.push_back(_item.GetHex());
                }
            }
            obj.push_back(Pair("txids", transactions));
            ret.push_back(obj);
        }
    }

    if (fByAccounts)
    {
        for (const auto& item : mapAccountTally)
        {
            const auto& nAmount = item.second.nAmount;
            const auto& nConf = item.second.nConf;
            UniValue obj{UniValue::VOBJ};
            if(item.second.fIsWatchonly)
                obj.push_back(Pair("involvesWatchonly", true));

            obj.push_back(Pair("account",       item.first));
            obj.push_back(Pair("amount",        ValueFromAmount(nAmount)));
            obj.push_back(Pair("confirmations", (nConf == std::numeric_limits<int>::max() ? 0 : nConf)));
            ret.push_back(obj);
        }
    }

    return ret;
}

UniValue listreceivedbyaddress(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 3)
        throw std::runtime_error{
            "listreceivedbyaddress ( minconf include_empty include_watchonly)\n"
            "\nList balances by receiving address.\n"
            "\nArguments:\n"
            "1. minconf           (numeric, optional, default=1) The minimum number of confirmations before payments are included.\n"
            "2. include_empty     (bool, optional, default=false) Whether to include addresses that haven't received any payments.\n"
            "3. include_watchonly (bool, optional, default=false) Whether to include watch-only addresses (see 'importaddress').\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"involvesWatchonly\" : true,        (bool) Only returned if imported addresses were involved in transaction\n"
            "    \"address\" : \"receivingaddress\",  (string) The receiving address\n"
            "    \"account\" : \"accountname\",       (string) DEPRECATED. The account of the receiving address. The default account is \"\".\n"
            "    \"amount\" : x.xxx,                  (numeric) The total amount in " + CURRENCY_UNIT + " received by the address\n"
            "    \"confirmations\" : n,               (numeric) The number of confirmations of the most recent transaction included\n"
            "    \"label\" : \"label\",               (string) A comment for the address/transaction, if any\n"
            "    \"txids\": [\n"
            "       n,                                (numeric) The ids of transactions received with the address \n"
            "       ...\n"
            "    ]\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("listreceivedbyaddress", "")
            + HelpExampleCli("listreceivedbyaddress", "6 true")
            + HelpExampleRpc("listreceivedbyaddress", "6, true, true")
        };

    ObserveSafeMode();
    LOCK2(cs_main, pwallet->cs_wallet);

    return ListReceived(pwallet, request.params, false);
}

UniValue listreceivedbyaccount(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 3)
        throw std::runtime_error{
            "listreceivedbyaccount ( minconf include_empty include_watchonly)\n"
            "\nDEPRECATED. List balances by account.\n"
            "\nArguments:\n"
            "1. minconf           (numeric, optional, default=1) The minimum number of confirmations before payments are included.\n"
            "2. include_empty     (bool, optional, default=false) Whether to include accounts that haven't received any payments.\n"
            "3. include_watchonly (bool, optional, default=false) Whether to include watch-only addresses (see 'importaddress').\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"involvesWatchonly\" : true,   (bool) Only returned if imported addresses were involved in transaction\n"
            "    \"account\" : \"accountname\",  (string) The account name of the receiving account\n"
            "    \"amount\" : x.xxx,             (numeric) The total amount received by addresses with this account\n"
            "    \"confirmations\" : n,          (numeric) The number of confirmations of the most recent transaction included\n"
            "    \"label\" : \"label\"           (string) A comment for the address/transaction, if any\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("listreceivedbyaccount", "")
            + HelpExampleCli("listreceivedbyaccount", "6 true")
            + HelpExampleRpc("listreceivedbyaccount", "6, true, true")
        };

    ObserveSafeMode();
    LOCK2(cs_main, pwallet->cs_wallet);

    return ListReceived(pwallet, request.params, true);
}

static void MaybePushAddress(UniValue & entry, const CTxDestination &dest)
{
    if (IsValidDestination(dest)) {
        entry.push_back(Pair("address", EncodeDestination(dest)));
    }
}

/**
 * List transactions based on the given criteria.
 *
 * @param  pwallet    The wallet.
 * @param  wtx        The wallet transaction.
 * @param  strAccount The account, if any, or "*" for all.
 * @param  nMinDepth  The minimum confirmation depth.
 * @param  fLong      Whether to include the JSON version of the transaction.
 * @param  ret        The UniValue into which the result is stored.
 * @param  filter     The "is mine" filter bool.
 */
static void ListTransactions(CWallet* const pwallet, const CWalletTx& wtx, const std::string& strAccount, int nMinDepth, bool fLong, UniValue& ret, const isminefilter& filter)
{
    CAmount nFee;
    std::string strSentAccount;
    std::list<COutputEntry> listReceived;
    std::list<COutputEntry> listSent;

    wtx.GetAmounts(listReceived, listSent, nFee, strSentAccount, filter);

    const auto fAllAccounts = (strAccount == "*");
    const auto involvesWatchonly = wtx.IsFromMe(ISMINE_WATCH_ONLY);

    // Sent
    if ((!listSent.empty() || nFee != 0) && (fAllAccounts || strAccount == strSentAccount))
    {
        for (const auto& s : listSent)
        {
            UniValue entry{UniValue::VOBJ};
            if (involvesWatchonly || (::IsMine(*pwallet, s.destination) & ISMINE_WATCH_ONLY)) {
                entry.push_back(Pair("involvesWatchonly", true));
            }
            entry.push_back(Pair("account", strSentAccount));
            MaybePushAddress(entry, s.destination);
            entry.push_back(Pair("category", "send"));
            entry.push_back(Pair("amount", ValueFromAmount(-s.amount)));
            if (pwallet->mapAddressBook.count(s.destination)) {
                entry.push_back(Pair("label", pwallet->mapAddressBook[s.destination].name));
            }
            entry.push_back(Pair("vout", s.vout));
            entry.push_back(Pair("fee", ValueFromAmount(-nFee)));
            if (fLong)
                WalletTxToJSON(wtx, entry);
            entry.push_back(Pair("abandoned", wtx.isAbandoned()));
            ret.push_back(entry);
        }
    }

    // Received
    if (listReceived.size() > 0 && wtx.GetDepthInMainChain() >= nMinDepth)
    {
        for (const auto& r : listReceived)
        {
            std::string account;
            if (pwallet->mapAddressBook.count(r.destination)) {
                account = pwallet->mapAddressBook[r.destination].name;
            }
            if (fAllAccounts || (account == strAccount))
            {
                UniValue entry{UniValue::VOBJ};
                if (involvesWatchonly || (::IsMine(*pwallet, r.destination) & ISMINE_WATCH_ONLY)) {
                    entry.push_back(Pair("involvesWatchonly", true));
                }
                entry.push_back(Pair("account", account));
                MaybePushAddress(entry, r.destination);
                if (wtx.IsCoinBase())
                {
                    if (wtx.GetDepthInMainChain() < 1)
                        entry.push_back(Pair("category", "orphan"));
                    else if (wtx.GetBlocksToMaturity() > 0)
                        entry.push_back(Pair("category", "immature"));
                    else
                        entry.push_back(Pair("category", "generate"));
                }
                else
                {
                    entry.push_back(Pair("category", "receive"));
                }
                entry.push_back(Pair("amount", ValueFromAmount(r.amount)));
                if (pwallet->mapAddressBook.count(r.destination)) {
                    entry.push_back(Pair("label", account));
                }
                entry.push_back(Pair("vout", r.vout));
                if (fLong)
                    WalletTxToJSON(wtx, entry);
                ret.push_back(entry);
            }
        }
    }
}

static void AcentryToJSON(const CAccountingEntry& acentry, const std::string& strAccount, UniValue& ret)
{
    const auto fAllAccounts = (strAccount == "*");

    if (fAllAccounts || acentry.strAccount == strAccount)
    {
        UniValue entry{UniValue::VOBJ};
        entry.push_back(Pair("account", acentry.strAccount));
        entry.push_back(Pair("category", "move"));
        entry.push_back(Pair("time", acentry.nTime));
        entry.push_back(Pair("amount", ValueFromAmount(acentry.nCreditDebit)));
        entry.push_back(Pair("otheraccount", acentry.strOtherAccount));
        entry.push_back(Pair("comment", acentry.strComment));
        ret.push_back(entry);
    }
}

UniValue listtransactions(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 4)
        throw std::runtime_error{
            "listtransactions ( \"account\" count skip include_watchonly)\n"
            "\nReturns up to 'count' most recent transactions skipping the first 'from' transactions for account 'account'.\n"
            "\nArguments:\n"
            "1. \"account\"    (string, optional) DEPRECATED. The account name. Should be \"*\".\n"
            "2. count          (numeric, optional, default=10) The number of transactions to return\n"
            "3. skip           (numeric, optional, default=0) The number of transactions to skip\n"
            "4. include_watchonly (bool, optional, default=false) Include transactions to watch-only addresses (see 'importaddress')\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"account\":\"accountname\",       (string) DEPRECATED. The account name associated with the transaction. \n"
            "                                                It will be \"\" for the default account.\n"
            "    \"address\":\"address\",    (string) The paicoin address of the transaction. Not present for \n"
            "                                                move transactions (category = move).\n"
            "    \"category\":\"send|receive|move\", (string) The transaction category. 'move' is a local (off blockchain)\n"
            "                                                transaction between accounts, and not associated with an address,\n"
            "                                                transaction id or block. 'send' and 'receive' transactions are \n"
            "                                                associated with an address, transaction id and block details\n"
            "    \"amount\": x.xxx,          (numeric) The amount in " + CURRENCY_UNIT + ". This is negative for the 'send' category, and for the\n"
            "                                         'move' category for moves outbound. It is positive for the 'receive' category,\n"
            "                                         and for the 'move' category for inbound funds.\n"
            "    \"label\": \"label\",       (string) A comment for the address/transaction, if any\n"
            "    \"vout\": n,                (numeric) the vout value\n"
            "    \"fee\": x.xxx,             (numeric) The amount of the fee in " + CURRENCY_UNIT + ". This is negative and only available for the \n"
            "                                         'send' category of transactions.\n"
            "    \"confirmations\": n,       (numeric) The number of confirmations for the transaction. Available for 'send' and \n"
            "                                         'receive' category of transactions. Negative confirmations indicate the\n"
            "                                         transaction conflicts with the block chain\n"
            "    \"trusted\": xxx,           (bool) Whether we consider the outputs of this unconfirmed transaction safe to spend.\n"
            "    \"blockhash\": \"hashvalue\", (string) The block hash containing the transaction. Available for 'send' and 'receive'\n"
            "                                          category of transactions.\n"
            "    \"blockindex\": n,          (numeric) The index of the transaction in the block that includes it. Available for 'send' and 'receive'\n"
            "                                          category of transactions.\n"
            "    \"blocktime\": xxx,         (numeric) The block time in seconds since epoch (1 Jan 1970 GMT).\n"
            "    \"txid\": \"transactionid\", (string) The transaction id. Available for 'send' and 'receive' category of transactions.\n"
            "    \"time\": xxx,              (numeric) The transaction time in seconds since epoch (midnight Jan 1 1970 GMT).\n"
            "    \"timereceived\": xxx,      (numeric) The time received in seconds since epoch (midnight Jan 1 1970 GMT). Available \n"
            "                                          for 'send' and 'receive' category of transactions.\n"
            "    \"comment\": \"...\",       (string) If a comment is associated with the transaction.\n"
            "    \"otheraccount\": \"accountname\",  (string) DEPRECATED. For the 'move' category of transactions, the account the funds came \n"
            "                                          from (for receiving funds, positive amounts), or went to (for sending funds,\n"
            "                                          negative amounts).\n"
            "    \"bip125-replaceable\": \"yes|no|unknown\",  (string) Whether this transaction could be replaced due to BIP125 (replace-by-fee);\n"
            "                                                     may be unknown for unconfirmed transactions not in the mempool\n"
            "    \"abandoned\": xxx          (bool) 'true' if the transaction has been abandoned (inputs are respendable). Only available for the \n"
            "                                         'send' category of transactions.\n"
            "  }\n"
            "]\n"

            "\nExamples:\n"
            "\nList the most recent 10 transactions in the systems\n"
            + HelpExampleCli("listtransactions", "") +
            "\nList transactions 100 to 120\n"
            + HelpExampleCli("listtransactions", "\"*\" 20 100") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("listtransactions", "\"*\", 20, 100")
        };

    ObserveSafeMode();
    LOCK2(cs_main, pwallet->cs_wallet);

    std::string strAccount{"*"};
    if (!request.params[0].isNull())
        strAccount = request.params[0].get_str();
    int nCount{10};
    if (!request.params[1].isNull())
        nCount = request.params[1].get_int();
    int nFrom{0};
    if (!request.params[2].isNull())
        nFrom = request.params[2].get_int();
    isminefilter filter{ISMINE_SPENDABLE};
    if(!request.params[3].isNull())
        if(request.params[3].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    if (nCount < 0)
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Negative count");
    if (nFrom < 0)
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Negative from");

    UniValue ret{UniValue::VARR};

    const auto& txOrdered = pwallet->wtxOrdered;

    // iterate backwards until we have nCount items to return:
    for (auto it = txOrdered.rbegin(); it != txOrdered.rend(); ++it)
    {
        const auto * const pwtx = (*it).second.first;
        if (pwtx != nullptr)
            ListTransactions(pwallet, *pwtx, strAccount, 0, true, ret, filter);
        const auto * const pacentry = (*it).second.second;
        if (pacentry != nullptr)
            AcentryToJSON(*pacentry, strAccount, ret);

        if (static_cast<int>(ret.size()) >= (nCount+nFrom)) break;
    }
    // ret is newest to oldest

    if (nFrom > (int)ret.size())
        nFrom = ret.size();
    if ((nFrom + nCount) > (int)ret.size())
        nCount = ret.size() - nFrom;

    auto arrTmp = ret.getValues();

    auto first = arrTmp.begin();
    std::advance(first, nFrom);
    auto last = arrTmp.begin();
    std::advance(last, nFrom+nCount);

    if (last != arrTmp.end()) arrTmp.erase(last, arrTmp.end());
    if (first != arrTmp.begin()) arrTmp.erase(arrTmp.begin(), first);

    std::reverse(arrTmp.begin(), arrTmp.end()); // Return oldest to newest

    ret.clear();
    ret.setArray();
    ret.push_backV(arrTmp);

    return ret;
}

UniValue listaccounts(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 2)
        throw std::runtime_error{
            "listaccounts ( minconf include_watchonly)\n"
            "\nDEPRECATED. Returns Object that has account names as keys, account balances as values.\n"
            "\nArguments:\n"
            "1. minconf             (numeric, optional, default=1) Only include transactions with at least this many confirmations\n"
            "2. include_watchonly   (bool, optional, default=false) Include balances in watch-only addresses (see 'importaddress')\n"
            "\nResult:\n"
            "{                      (json object where keys are account names, and values are numeric balances\n"
            "  \"account\": x.xxx,  (numeric) The property name is the account name, and the value is the total balance for the account.\n"
            "  ...\n"
            "}\n"
            "\nExamples:\n"
            "\nList account balances where there at least 1 confirmation\n"
            + HelpExampleCli("listaccounts", "") +
            "\nList account balances including zero confirmation transactions\n"
            + HelpExampleCli("listaccounts", "0") +
            "\nList account balances for 6 or more confirmations\n"
            + HelpExampleCli("listaccounts", "6") +
            "\nAs json rpc call\n"
            + HelpExampleRpc("listaccounts", "6")
        };

    ObserveSafeMode();
    LOCK2(cs_main, pwallet->cs_wallet);

    int nMinDepth{1};
    if (!request.params[0].isNull())
        nMinDepth = request.params[0].get_int();
    isminefilter includeWatchonly{ISMINE_SPENDABLE};
    if(!request.params[1].isNull())
        if(request.params[1].get_bool())
            includeWatchonly = includeWatchonly | ISMINE_WATCH_ONLY;

    std::map<std::string, CAmount> mapAccountBalances;
    for (const auto& entry : pwallet->mapAddressBook) {
        if (IsMine(*pwallet, entry.first) & includeWatchonly) {  // This address belongs to me
            mapAccountBalances[entry.second.name] = 0;
        }
    }

    for (const auto& pairWtx : pwallet->mapWallet) {
        const auto& wtx = pairWtx.second;
        CAmount nFee;
        std::string strSentAccount;
        std::list<COutputEntry> listReceived;
        std::list<COutputEntry> listSent;
        const auto nDepth = wtx.GetDepthInMainChain();
        if (wtx.GetBlocksToMaturity() > 0 || nDepth < 0)
            continue;
        wtx.GetAmounts(listReceived, listSent, nFee, strSentAccount, includeWatchonly);
        mapAccountBalances[strSentAccount] -= nFee;
        for (const auto& s : listSent)
            mapAccountBalances[strSentAccount] -= s.amount;
        if (nDepth >= nMinDepth)
        {
            for (const auto& r : listReceived)
                if (pwallet->mapAddressBook.count(r.destination)) {
                    mapAccountBalances[pwallet->mapAddressBook[r.destination].name] += r.amount;
                }
                else
                    mapAccountBalances[""] += r.amount;
        }
    }

    const auto& acentries = pwallet->laccentries;
    for (const auto& entry : acentries)
        mapAccountBalances[entry.strAccount] += entry.nCreditDebit;

    UniValue ret{UniValue::VOBJ};
    for (const auto& accountBalance : mapAccountBalances) {
        ret.push_back(Pair(accountBalance.first, ValueFromAmount(accountBalance.second)));
    }
    return ret;
}

UniValue listsinceblock(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 4)
        throw std::runtime_error{
            "listsinceblock ( \"blockhash\" target_confirmations include_watchonly include_removed )\n"
            "\nGet all transactions in blocks since block [blockhash], or all transactions if omitted.\n"
            "If \"blockhash\" is no longer a part of the main chain, transactions from the fork point onward are included.\n"
            "Additionally, if include_removed is set, transactions affecting the wallet which were removed are returned in the \"removed\" array.\n"
            "\nArguments:\n"
            "1. \"blockhash\"            (string, optional) The block hash to list transactions since\n"
            "2. target_confirmations:    (numeric, optional, default=1) Return the nth block hash from the main chain. e.g. 1 would mean the best block hash. Note: this is not used as a filter, but only affects [lastblock] in the return value\n"
            "3. include_watchonly:       (bool, optional, default=false) Include transactions to watch-only addresses (see 'importaddress')\n"
            "4. include_removed:         (bool, optional, default=true) Show transactions that were removed due to a reorg in the \"removed\" array\n"
            "                                                           (not guaranteed to work on pruned nodes)\n"
            "\nResult:\n"
            "{\n"
            "  \"transactions\": [\n"
            "    \"account\":\"accountname\",       (string) DEPRECATED. The account name associated with the transaction. Will be \"\" for the default account.\n"
            "    \"address\":\"address\",    (string) The paicoin address of the transaction. Not present for move transactions (category = move).\n"
            "    \"category\":\"send|receive\",     (string) The transaction category. 'send' has negative amounts, 'receive' has positive amounts.\n"
            "    \"amount\": x.xxx,          (numeric) The amount in " + CURRENCY_UNIT + ". This is negative for the 'send' category, and for the 'move' category for moves \n"
            "                                          outbound. It is positive for the 'receive' category, and for the 'move' category for inbound funds.\n"
            "    \"vout\" : n,               (numeric) the vout value\n"
            "    \"fee\": x.xxx,             (numeric) The amount of the fee in " + CURRENCY_UNIT + ". This is negative and only available for the 'send' category of transactions.\n"
            "    \"confirmations\": n,       (numeric) The number of confirmations for the transaction. Available for 'send' and 'receive' category of transactions.\n"
            "                                          When it's < 0, it means the transaction conflicted that many blocks ago.\n"
            "    \"blockhash\": \"hashvalue\",     (string) The block hash containing the transaction. Available for 'send' and 'receive' category of transactions.\n"
            "    \"blockindex\": n,          (numeric) The index of the transaction in the block that includes it. Available for 'send' and 'receive' category of transactions.\n"
            "    \"blocktime\": xxx,         (numeric) The block time in seconds since epoch (1 Jan 1970 GMT).\n"
            "    \"txid\": \"transactionid\",  (string) The transaction id. Available for 'send' and 'receive' category of transactions.\n"
            "    \"time\": xxx,              (numeric) The transaction time in seconds since epoch (Jan 1 1970 GMT).\n"
            "    \"timereceived\": xxx,      (numeric) The time received in seconds since epoch (Jan 1 1970 GMT). Available for 'send' and 'receive' category of transactions.\n"
            "    \"bip125-replaceable\": \"yes|no|unknown\",  (string) Whether this transaction could be replaced due to BIP125 (replace-by-fee);\n"
            "                                                   may be unknown for unconfirmed transactions not in the mempool\n"
            "    \"abandoned\": xxx,         (bool) 'true' if the transaction has been abandoned (inputs are respendable). Only available for the 'send' category of transactions.\n"
            "    \"comment\": \"...\",       (string) If a comment is associated with the transaction.\n"
            "    \"label\" : \"label\"       (string) A comment for the address/transaction, if any\n"
            "    \"to\": \"...\",            (string) If a comment to is associated with the transaction.\n"
            "  ],\n"
            "  \"removed\": [\n"
            "    <structure is the same as \"transactions\" above, only present if include_removed=true>\n"
            "    Note: transactions that were readded in the active chain will appear as-is in this array, and may thus have a positive confirmation count.\n"
            "  ],\n"
            "  \"lastblock\": \"lastblockhash\"     (string) The hash of the block (target_confirmations-1) from the best block on the main chain. This is typically used to feed back into listsinceblock the next time you call it. So you would generally use a target_confirmations of say 6, so you will be continually re-notified of transactions until they've reached 6 confirmations plus any new ones\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("listsinceblock", "")
            + HelpExampleCli("listsinceblock", "\"000000000000000bacf66f7497b7dc45ef753ee9a7d38571037cdb1a57f663ad\" 6")
            + HelpExampleRpc("listsinceblock", "\"000000000000000bacf66f7497b7dc45ef753ee9a7d38571037cdb1a57f663ad\", 6")
        };

    ObserveSafeMode();
    LOCK2(cs_main, pwallet->cs_wallet);

    const CBlockIndex* pindex = nullptr;    // Block index of the specified block or the common ancestor, if the block provided was in a deactivated chain.
    const CBlockIndex* paltindex = nullptr; // Block index of the specified block, even if it's in a deactivated chain.
    int target_confirms{1};
    isminefilter filter{ISMINE_SPENDABLE};

    if (!request.params[0].isNull()) {
        uint256 blockId;

        blockId.SetHex(request.params[0].get_str());
        auto it = mapBlockIndex.find(blockId);
        if (it != mapBlockIndex.end()) {
            paltindex = pindex = it->second;
            if (chainActive[pindex->nHeight] != pindex) {
                // the block being asked for is a part of a deactivated chain;
                // we don't want to depend on its perceived height in the block
                // chain, we want to instead use the last common ancestor
                pindex = chainActive.FindFork(pindex);
            }
        }
    }

    if (!request.params[1].isNull()) {
        target_confirms = request.params[1].get_int();

        if (target_confirms < 1) {
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid parameter");
        }
    }

    if (!request.params[2].isNull() && request.params[2].get_bool()) {
        filter = filter | ISMINE_WATCH_ONLY;
    }

    const auto include_removed = (request.params[3].isNull() || request.params[3].get_bool());

    const int depth{pindex ? (1 + chainActive.Height() - pindex->nHeight) : -1};

    UniValue transactions{UniValue::VARR};

    for (const auto& pairWtx : pwallet->mapWallet) {
        const auto& tx = pairWtx.second;

        if (depth == -1 || tx.GetDepthInMainChain() < depth) {
            ListTransactions(pwallet, tx, "*", 0, true, transactions, filter);
        }
    }

    // when a reorg'd block is requested, we also list any relevant transactions
    // in the blocks of the chain that was detached
    UniValue removed{UniValue::VARR};
    while (include_removed && paltindex && paltindex != pindex) {
        CBlock block;
        if (!ReadBlockFromDisk(block, paltindex, Params().GetConsensus())) {
            throw JSONRPCError(RPCErrorCode::INTERNAL_ERROR, "Can't read block from disk");
        }
        for (const auto& tx : block.vtx) {
            auto it = pwallet->mapWallet.find(tx->GetHash());
            if (it != pwallet->mapWallet.end()) {
                // We want all transactions regardless of confirmation count to appear here,
                // even negative confirmation ones, hence the big negative.
                ListTransactions(pwallet, it->second, "*", -100000000, true, removed, filter);
            }
        }
        paltindex = paltindex->pprev;
    }

    const auto pblockLast = chainActive[chainActive.Height() + 1 - target_confirms];
    const uint256 lastblock{pblockLast ? pblockLast->GetBlockHash() : uint256{}};

    UniValue ret{UniValue::VOBJ};
    ret.push_back(Pair("transactions", transactions));
    if (include_removed) ret.push_back(Pair("removed", removed));
    ret.push_back(Pair("lastblock", lastblock.GetHex()));

    return ret;
}

UniValue gettransaction(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error{
            "gettransaction \"txid\" ( include_watchonly )\n"
            "\nGet detailed information about in-wallet transaction <txid>\n"
            "\nArguments:\n"
            "1. \"txid\"                  (string, required) The transaction id\n"
            "2. \"include_watchonly\"     (bool, optional, default=false) Whether to include watch-only addresses in balance calculation and details[]\n"
            "\nResult:\n"
            "{\n"
            "  \"amount\" : x.xxx,        (numeric) The transaction amount in " + CURRENCY_UNIT + "\n"
            "  \"fee\": x.xxx,            (numeric) The amount of the fee in " + CURRENCY_UNIT + ". This is negative and only available for the \n"
            "                              'send' category of transactions.\n"
            "  \"confirmations\" : n,     (numeric) The number of confirmations\n"
            "  \"blockhash\" : \"hash\",  (string) The block hash\n"
            "  \"blockindex\" : xx,       (numeric) The index of the transaction in the block that includes it\n"
            "  \"blocktime\" : ttt,       (numeric) The time in seconds since epoch (1 Jan 1970 GMT)\n"
            "  \"txid\" : \"transactionid\",   (string) The transaction id.\n"
            "  \"time\" : ttt,            (numeric) The transaction time in seconds since epoch (1 Jan 1970 GMT)\n"
            "  \"timereceived\" : ttt,    (numeric) The time received in seconds since epoch (1 Jan 1970 GMT)\n"
            "  \"bip125-replaceable\": \"yes|no|unknown\",  (string) Whether this transaction could be replaced due to BIP125 (replace-by-fee);\n"
            "                                                   may be unknown for unconfirmed transactions not in the mempool\n"
            "  \"details\" : [\n"
            "    {\n"
            "      \"account\" : \"accountname\",      (string) DEPRECATED. The account name involved in the transaction, can be \"\" for the default account.\n"
            "      \"address\" : \"address\",          (string) The paicoin address involved in the transaction\n"
            "      \"category\" : \"send|receive\",    (string) The category, either 'send' or 'receive'\n"
            "      \"amount\" : x.xxx,                 (numeric) The amount in " + CURRENCY_UNIT + "\n"
            "      \"label\" : \"label\",              (string) A comment for the address/transaction, if any\n"
            "      \"vout\" : n,                       (numeric) the vout value\n"
            "      \"fee\": x.xxx,                     (numeric) The amount of the fee in " + CURRENCY_UNIT + ". This is negative and only available for the \n"
            "                                           'send' category of transactions.\n"
            "      \"abandoned\": xxx                  (bool) 'true' if the transaction has been abandoned (inputs are respendable). Only available for the \n"
            "                                           'send' category of transactions.\n"
            "    }\n"
            "    ,...\n"
            "  ],\n"
            "  \"hex\" : \"data\"         (string) Raw data for transaction\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
            + HelpExampleCli("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\" true")
            + HelpExampleRpc("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
        };

    ObserveSafeMode();
    LOCK2(cs_main, pwallet->cs_wallet);

    uint256 hash;
    hash.SetHex(request.params[0].get_str());

    isminefilter filter{ISMINE_SPENDABLE};
    if(!request.params[1].isNull())
        if(request.params[1].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    UniValue entry{UniValue::VOBJ};
    auto it = pwallet->mapWallet.find(hash);
    if (it == pwallet->mapWallet.end()) {
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Invalid or non-wallet transaction id");
    }
    const auto& wtx = it->second;

    const auto nCredit = wtx.GetCredit(filter);
    const auto nDebit = wtx.GetDebit(filter);
    const CAmount nNet{nCredit - nDebit};
    const CAmount nFee{wtx.IsFromMe(filter) ? wtx.tx->GetValueOut() - nDebit : 0};

    entry.push_back(Pair("amount", ValueFromAmount(nNet - nFee)));
    if (wtx.IsFromMe(filter))
        entry.push_back(Pair("fee", ValueFromAmount(nFee)));

    WalletTxToJSON(wtx, entry);

    UniValue details{UniValue::VARR};
    ListTransactions(pwallet, wtx, "*", 0, false, details, filter);
    entry.push_back(Pair("details", details));

    const auto strHex = EncodeHexTx(static_cast<const CTransaction&>(*wtx.tx), RPCSerializationFlags());
    entry.push_back(Pair("hex", strHex));

    return entry;
}

UniValue abandontransaction(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error{
            "abandontransaction \"txid\"\n"
            "\nMark in-wallet transaction <txid> as abandoned\n"
            "This will mark this transaction and all its in-wallet descendants as abandoned which will allow\n"
            "for their inputs to be respent.  It can be used to replace \"stuck\" or evicted transactions.\n"
            "It only works on transactions which are not included in a block and are not currently in the mempool.\n"
            "It has no effect on transactions which are already conflicted or abandoned.\n"
            "\nArguments:\n"
            "1. \"txid\"    (string, required) The transaction id\n"
            "\nResult:\n"
            "\nExamples:\n"
            + HelpExampleCli("abandontransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
            + HelpExampleRpc("abandontransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
        };

    ObserveSafeMode();
    LOCK2(cs_main, pwallet->cs_wallet);

    uint256 hash;
    hash.SetHex(request.params[0].get_str());

    if (!pwallet->mapWallet.count(hash)) {
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Invalid or non-wallet transaction id");
    }
    if (!pwallet->AbandonTransaction(hash)) {
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Transaction not eligible for abandonment");
    }

    return NullUniValue;
}

UniValue backupwallet(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error{
            "backupwallet \"destination\"\n"
            "\nSafely copies current wallet file to destination, which can be a directory or a path with filename.\n"
            "\nArguments:\n"
            "1. \"destination\"   (string) The destination directory or file\n"
            "\nExamples:\n"
            + HelpExampleCli("backupwallet", "\"backup.dat\"")
            + HelpExampleRpc("backupwallet", "\"backup.dat\"")
        };

    LOCK2(cs_main, pwallet->cs_wallet);

    if (!pwallet->BackupWallet(request.params[0].get_str())) {
        throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "Error: Wallet backup failed!");
    }

    return NullUniValue;
}

UniValue keypoolrefill(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error{
            "keypoolrefill ( newsize )\n"
            "\nFills the keypool."
            + HelpRequiringPassphrase(pwallet) + "\n"
            "\nArguments\n"
            "1. newsize     (numeric, optional, default=100) The new keypool size\n"
            "\nExamples:\n"
            + HelpExampleCli("keypoolrefill", "")
            + HelpExampleRpc("keypoolrefill", "")
        };

    LOCK2(cs_main, pwallet->cs_wallet);

    // 0 is interpreted by TopUpKeyPool() as the default keypool size given by -keypool
    unsigned int kpSize{0};
    if (!request.params[0].isNull()) {
        if (request.params[0].get_int() < 0)
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid parameter, expected valid size.");
        kpSize = request.params[0].get_int();
    }

    EnsureWalletIsUnlocked(pwallet);
    pwallet->TopUpKeyPool(kpSize);

    if (pwallet->GetKeyPoolSize() < kpSize) {
        throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "Error refreshing keypool.");
    }

    return NullUniValue;
}

static void LockWallet(CWallet* pWallet)
{
    LOCK(pWallet->cs_wallet);
    pWallet->nRelockTime = 0;
    pWallet->Lock();
}

UniValue walletpassphrase(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (pwallet->IsCrypted() && (request.fHelp || request.params.size() != 2)) {
        throw std::runtime_error{
            "walletpassphrase \"passphrase\" timeout\n"
            "\nStores the wallet decryption key in memory for 'timeout' seconds.\n"
            "This is needed prior to performing transactions related to private keys such as sending paicoins\n"
            "\nArguments:\n"
            "1. \"passphrase\"     (string, required) The wallet passphrase\n"
            "2. timeout            (numeric, required) The time to keep the decryption key in seconds.\n"
            "\nNote:\n"
            "Issuing the walletpassphrase command while the wallet is already unlocked will set a new unlock\n"
            "time that overrides the old one.\n"
            "\nExamples:\n"
            "\nUnlock the wallet for 60 seconds\n"
            + HelpExampleCli("walletpassphrase", "\"my pass phrase\" 60") +
            "\nLock the wallet again (before 60 seconds)\n"
            + HelpExampleCli("walletlock", "") +
            "\nAs json rpc call\n"
            + HelpExampleRpc("walletpassphrase", "\"my pass phrase\", 60")
        };
    }

    LOCK2(cs_main, pwallet->cs_wallet);

    if (request.fHelp)
        return true;
    if (!pwallet->IsCrypted()) {
        throw JSONRPCError(RPCErrorCode::WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletpassphrase was called.");
    }

    // Note that the walletpassphrase is stored in request.params[0] which is not mlock()ed
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    // TODO: get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make request.params[0] mlock()'d to begin with.
    strWalletPass = request.params[0].get_str().c_str();

    if (strWalletPass.length() > 0)
    {
        if (!pwallet->Unlock(strWalletPass)) {
            throw JSONRPCError(RPCErrorCode::WALLET_PASSPHRASE_INCORRECT, "Error: The wallet passphrase entered was incorrect.");
        }
    }
    else
        throw std::runtime_error{
            "walletpassphrase <passphrase> <timeout>\n"
            "Stores the wallet decryption key in memory for <timeout> seconds."};

    pwallet->TopUpKeyPool();

    const auto nSleepTime = request.params[1].get_int64();
    pwallet->nRelockTime = GetTime() + nSleepTime;
    RPCRunLater(strprintf("lockwallet(%s)", pwallet->GetName()), boost::bind(LockWallet, pwallet), nSleepTime);

    return NullUniValue;
}

UniValue walletpassphrasechange(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (pwallet->IsCrypted() && (request.fHelp || request.params.size() != 2)) {
        throw std::runtime_error{
            "walletpassphrasechange \"oldpassphrase\" \"newpassphrase\"\n"
            "\nChanges the wallet passphrase from 'oldpassphrase' to 'newpassphrase'.\n"
            "\nArguments:\n"
            "1. \"oldpassphrase\"      (string) The current passphrase\n"
            "2. \"newpassphrase\"      (string) The new passphrase\n"
            "\nExamples:\n"
            + HelpExampleCli("walletpassphrasechange", "\"old one\" \"new one\"")
            + HelpExampleRpc("walletpassphrasechange", "\"old one\", \"new one\"")
        };
    }

    LOCK2(cs_main, pwallet->cs_wallet);

    if (request.fHelp)
        return true;
    if (!pwallet->IsCrypted()) {
        throw JSONRPCError(RPCErrorCode::WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletpassphrasechange was called.");
    }

    // TODO: get rid of these .c_str() calls by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make request.params[0] mlock()'d to begin with.
    SecureString strOldWalletPass;
    strOldWalletPass.reserve(100);
    strOldWalletPass = request.params[0].get_str().c_str();

    SecureString strNewWalletPass;
    strNewWalletPass.reserve(100);
    strNewWalletPass = request.params[1].get_str().c_str();

    if (strOldWalletPass.length() < 1 || strNewWalletPass.length() < 1)
        throw std::runtime_error(
            "walletpassphrasechange <oldpassphrase> <newpassphrase>\n"
            "Changes the wallet passphrase from <oldpassphrase> to <newpassphrase>.");

    if (!pwallet->ChangeWalletPassphrase(strOldWalletPass, strNewWalletPass)) {
        throw JSONRPCError(RPCErrorCode::WALLET_PASSPHRASE_INCORRECT, "Error: The wallet passphrase entered was incorrect.");
    }

    return NullUniValue;
}

UniValue walletlock(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (pwallet->IsCrypted() && (request.fHelp || request.params.size() != 0)) {
        throw std::runtime_error{
            "walletlock\n"
            "\nRemoves the wallet encryption key from memory, locking the wallet.\n"
            "After calling this method, you will need to call walletpassphrase again\n"
            "before being able to call any methods which require the wallet to be unlocked.\n"
            "\nExamples:\n"
            "\nSet the passphrase for 2 minutes to perform a transaction\n"
            + HelpExampleCli("walletpassphrase", "\"my pass phrase\" 120") +
            "\nPerform a send (requires passphrase set)\n"
            + HelpExampleCli("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 1.0") +
            "\nClear the passphrase since we are done before 2 minutes is up\n"
            + HelpExampleCli("walletlock", "") +
            "\nAs json rpc call\n"
            + HelpExampleRpc("walletlock", "")
        };
    }

    LOCK2(cs_main, pwallet->cs_wallet);

    if (request.fHelp)
        return true;
    if (!pwallet->IsCrypted()) {
        throw JSONRPCError(RPCErrorCode::WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletlock was called.");
    }

    pwallet->Lock();
    pwallet->nRelockTime = 0;

    return NullUniValue;
}


UniValue encryptwallet(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (!pwallet->IsCrypted() && (request.fHelp || request.params.size() != 1)) {
        throw std::runtime_error{
            "encryptwallet \"passphrase\"\n"
            "\nEncrypts the wallet with 'passphrase'. This is for first time encryption.\n"
            "After this, any calls that interact with private keys such as sending or signing \n"
            "will require the passphrase to be set prior the making these calls.\n"
            "Use the walletpassphrase call for this, and then walletlock call.\n"
            "If the wallet is already encrypted, use the walletpassphrasechange call.\n"
            "Note that this will shutdown the server.\n"
            "\nArguments:\n"
            "1. \"passphrase\"    (string) The pass phrase to encrypt the wallet with. It must be at least 1 character, but should be long.\n"
            "\nExamples:\n"
            "\nEncrypt your wallet\n"
            + HelpExampleCli("encryptwallet", "\"my pass phrase\"") +
            "\nNow set the passphrase to use the wallet, such as for signing or sending paicoin\n"
            + HelpExampleCli("walletpassphrase", "\"my pass phrase\"") +
            "\nNow we can do something like sign\n"
            + HelpExampleCli("signmessage", "\"address\" \"test message\"") +
            "\nNow lock the wallet again by removing the passphrase\n"
            + HelpExampleCli("walletlock", "") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("encryptwallet", "\"my pass phrase\"")
        };
    }

    LOCK2(cs_main, pwallet->cs_wallet);

    if (request.fHelp)
        return true;
    if (pwallet->IsCrypted()) {
        throw JSONRPCError(RPCErrorCode::WALLET_WRONG_ENC_STATE, "Error: running with an encrypted wallet, but encryptwallet was called.");
    }

    // TODO: get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make request.params[0] mlock()'d to begin with.
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    strWalletPass = request.params[0].get_str().c_str();

    if (strWalletPass.length() < 1)
        throw std::runtime_error(
            "encryptwallet <passphrase>\n"
            "Encrypts the wallet with <passphrase>.");

    if (!pwallet->EncryptWallet(strWalletPass)) {
        throw JSONRPCError(RPCErrorCode::WALLET_ENCRYPTION_FAILED, "Error: Failed to encrypt the wallet.");
    }

    // BDB seems to have a bad habit of writing old data into
    // slack space in .dat files; that is bad if the old data is
    // unencrypted private keys. So:
    StartShutdown();
    return "wallet encrypted; PAIcoin server stopping, restart to run with encrypted wallet. The keypool has been flushed and a new HD seed was generated (if you are using HD). You need to make a new backup.";
}

UniValue lockunspent(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error{
            "lockunspent unlock ([{\"txid\":\"txid\",\"vout\":n},...])\n"
            "\nUpdates list of temporarily unspendable outputs.\n"
            "Temporarily lock (unlock=false) or unlock (unlock=true) specified transaction outputs.\n"
            "If no transaction outputs are specified when unlocking then all current locked transaction outputs are unlocked.\n"
            "A locked transaction output will not be chosen by automatic coin selection, when spending paicoins.\n"
            "Locks are stored in memory only. Nodes start with zero locked outputs, and the locked output list\n"
            "is always cleared (by virtue of process exit) when a node stops or fails.\n"
            "Also see the listunspent call\n"
            "\nArguments:\n"
            "1. unlock            (boolean, required) Whether to unlock (true) or lock (false) the specified transactions\n"
            "2. \"transactions\"  (string, optional) A json array of objects. Each object the txid (string) vout (numeric)\n"
            "     [           (json array of json objects)\n"
            "       {\n"
            "         \"txid\":\"id\",    (string) The transaction id\n"
            "         \"vout\": n         (numeric) The output number\n"
            "       }\n"
            "       ,...\n"
            "     ]\n"

            "\nResult:\n"
            "true|false    (boolean) Whether the command was successful or not\n"

            "\nExamples:\n"
            "\nList the unspent transactions\n"
            + HelpExampleCli("listunspent", "") +
            "\nLock an unspent transaction\n"
            + HelpExampleCli("lockunspent", "false \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nList the locked transactions\n"
            + HelpExampleCli("listlockunspent", "") +
            "\nUnlock the transaction again\n"
            + HelpExampleCli("lockunspent", "true \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("lockunspent", "false, \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"")
        };

    LOCK2(cs_main, pwallet->cs_wallet);

    RPCTypeCheckArgument(request.params[0], UniValue::VBOOL);

    const auto fUnlock = request.params[0].get_bool();

    if (request.params[1].isNull()) {
        if (fUnlock)
            pwallet->UnlockAllCoins();
        return true;
    }

    RPCTypeCheckArgument(request.params[1], UniValue::VARR);

    const auto& outputs = request.params[1].get_array();
    for (size_t idx{0}; idx < outputs.size(); ++idx) {
        const auto& output = outputs[idx];
        if (!output.isObject())
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid parameter, expected object");
        const auto& o = output.get_obj();

        RPCTypeCheckObj(o,
            {
                {"txid", UniValueType(UniValue::VSTR)},
                {"vout", UniValueType(UniValue::VNUM)},
            });

        const auto& txid = find_value(o, "txid").get_str();
        if (!IsHex(txid))
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid parameter, expected hex txid");

        const auto nOutput = find_value(o, "vout").get_int();
        if (nOutput < 0)
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid parameter, vout must be positive");

        const COutPoint outpt{uint256S(txid), static_cast<uint32_t>(nOutput)};

        if (fUnlock)
            pwallet->UnlockCoin(outpt);
        else
            pwallet->LockCoin(outpt);
    }

    return true;
}

UniValue listlockunspent(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 0)
        throw std::runtime_error{
            "listlockunspent\n"
            "\nReturns list of temporarily unspendable outputs.\n"
            "See the lockunspent call to lock and unlock transactions for spending.\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"txid\" : \"transactionid\",     (string) The transaction id locked\n"
            "    \"vout\" : n                      (numeric) The vout value\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            "\nList the unspent transactions\n"
            + HelpExampleCli("listunspent", "") +
            "\nLock an unspent transaction\n"
            + HelpExampleCli("lockunspent", "false \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nList the locked transactions\n"
            + HelpExampleCli("listlockunspent", "") +
            "\nUnlock the transaction again\n"
            + HelpExampleCli("lockunspent", "true \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("listlockunspent", "")
        };

    ObserveSafeMode();
    LOCK2(cs_main, pwallet->cs_wallet);

    std::vector<COutPoint> vOutpts;
    pwallet->ListLockedCoins(vOutpts);

    UniValue ret{UniValue::VARR};

    for (const auto &outpt : vOutpts) {
        UniValue o{UniValue::VOBJ};

        o.push_back(Pair("txid", outpt.hash.GetHex()));
        o.push_back(Pair("vout", static_cast<int>(outpt.n)));
        ret.push_back(o);
    }

    return ret;
}

UniValue settxfee(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 1)
        throw std::runtime_error{
            "settxfee amount\n"
            "\nSet the transaction fee per kB. Overwrites the paytxfee parameter.\n"
            "\nArguments:\n"
            "1. amount         (numeric or string, required) The transaction fee in " + CURRENCY_UNIT + "/kB\n"
            "\nResult\n"
            "true|false        (boolean) Returns true if successful\n"
            "\nExamples:\n"
            + HelpExampleCli("settxfee", "0.00001")
            + HelpExampleRpc("settxfee", "0.00001")
        };

    LOCK2(cs_main, pwallet->cs_wallet);

    payTxFee = CFeeRate(AmountFromValue(request.params[0]), 1000);
    return true;
}

UniValue getwalletinfo(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || !request.params.empty())
        throw std::runtime_error{
            "getwalletinfo\n"
            "Returns an object containing various wallet state info.\n"
            "\nResult:\n"
            "{\n"
            "  \"walletname\": xxxxx,             (string) the wallet name\n"
            "  \"walletversion\": xxxxx,          (numeric) the wallet version\n"
            "  \"balance\": xxxxxxx,              (numeric) the total confirmed balance of the wallet in " + CURRENCY_UNIT + "\n"
            "  \"unconfirmed_balance\": xxx,      (numeric) the total unconfirmed balance of the wallet in " + CURRENCY_UNIT + "\n"
            "  \"immature_balance\": xxxxxx,      (numeric) the total immature balance of the wallet in " + CURRENCY_UNIT + "\n"
            "  \"txcount\": xxxxxxx,              (numeric) the total number of transactions in the wallet\n"
            "  \"keypoololdest\": xxxxxx,         (numeric) the timestamp (seconds since Unix epoch) of the oldest pre-generated key in the key pool\n"
            "  \"keypoolsize\": xxxx,             (numeric) how many new keys are pre-generated (only counts external keys)\n"
            "  \"keypoolsize_hd_internal\": xxxx, (numeric) how many new keys are pre-generated for internal use (used for change outputs, only appears if the wallet is using this feature, otherwise external keys are used)\n"
            "  \"unlocked_until\": ttt,           (numeric) the timestamp in seconds since epoch (midnight Jan 1 1970 GMT) that the wallet is unlocked for transfers, or 0 if the wallet is locked\n"
            "  \"paytxfee\": x.xxxx,              (numeric) the transaction fee configuration, set in " + CURRENCY_UNIT + "/kB\n"
            "  \"hdmasterkeyid\": \"<hash160>\"     (string) the Hash160 of the HD master pubkey\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getwalletinfo", "")
            + HelpExampleRpc("getwalletinfo", "")
        };

    ObserveSafeMode();
    LOCK2(cs_main, pwallet->cs_wallet);

    UniValue obj{UniValue::VOBJ};

    const auto kpExternalSize = pwallet->KeypoolCountExternalKeys();
    obj.push_back(Pair("walletname", pwallet->GetName()));
    obj.push_back(Pair("walletversion", pwallet->GetVersion()));
    obj.push_back(Pair("balance",       ValueFromAmount(pwallet->GetBalance())));
    obj.push_back(Pair("unconfirmed_balance", ValueFromAmount(pwallet->GetUnconfirmedBalance())));
    obj.push_back(Pair("immature_balance",    ValueFromAmount(pwallet->GetImmatureBalance())));
    obj.push_back(Pair("txcount",       (int)pwallet->mapWallet.size()));
    obj.push_back(Pair("keypoololdest", pwallet->GetOldestKeyPoolTime()));
    obj.push_back(Pair("keypoolsize", static_cast<int64_t>(kpExternalSize)));
    const CKeyID masterKeyID{pwallet->GetHDChain().masterKeyID};
    if (!masterKeyID.IsNull() && pwallet->CanSupportFeature(FEATURE_HD_SPLIT)) {
        obj.push_back(Pair("keypoolsize_hd_internal",   static_cast<int64_t>((pwallet->GetKeyPoolSize() - kpExternalSize))));
    }
    if (pwallet->IsCrypted()) {
        obj.push_back(Pair("unlocked_until", pwallet->nRelockTime));
    }
    obj.push_back(Pair("paytxfee",      ValueFromAmount(payTxFee.GetFeePerK())));
    if (!masterKeyID.IsNull())
         obj.push_back(Pair("hdmasterkeyid", masterKeyID.GetHex()));
    return obj;
}

UniValue getstakeinfo(const JSONRPCRequest& request)
{
    if (request.fHelp || !request.params.empty())
        throw std::runtime_error{
            "getstakeinfo\n"
            "Returns statistics about staking from the wallet.\n"
            "\nArguments:\n"
            "None\n"
            "\nResult:\n"
            "{\n"
            "    \"blockheight\": n,          (numeric) Current block height for stake info.\n"
            "    \"difficulty\": n.nnn,       (numeric) Current stake difficulty.\n"
            "    \"totalsubsidy\": n.nnn,     (numeric) Total amount of coins earned by proof-of-stake voting\n"
            "    \"ownmempooltix\": n,        (numeric) Number of tickets submitted by this wallet currently in mempool\n"
            "    \"immature\": n,             (numeric) Number of tickets from this wallet that are in the blockchain but which are not yet mature\n"
            "    \"unspent\": n,              (numeric) Number of unspent tickets\n"
            "    \"voted\": n,                (numeric) Number of votes cast by this wallet\n"
            "    \"revoked\": n,              (numeric) Number of missed tickets that were missed and then revoked\n"
            "    \"unspentexpired\": n,       (numeric) Number of unspent tickets which are past expiry\n"
            "    \"poolsize\": n,             (numeric) Number of live tickets in the ticket pool.\n"
            "    \"allmempooltix\": n,        (numeric) Number of tickets currently in the mempool\n"
            "    \"live\": n,                 (numeric) Number of mature, active tickets owned by this wallet\n"
            "    \"proportionlive\": n.nnn,   (numeric) (Live / PoolSize)\n"
            "    \"missed\": n,               (numeric) Number of missed tickets (failure to vote, not including expired)\n"
            "    \"proportionmissed\": n.nnn, (numeric) (Missed / (Missed + Voted))\n"
            "    \"expired\": n,              (numeric) Number of tickets that have expired\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getstakeinfo", "")
            + HelpExampleRpc("getstakeinfo", "")
        };

    UniValue obj{UniValue::VOBJ};

    return obj;
}

UniValue stakepooluserinfo(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error{
            "stakepooluserinfo \"user\"\n"
            "Get user info for stakepool.\n"
            "\nArguments:\n"
            "1. user                       (string, required)   The id of the user to be looked up\n"
            "\nResult:\n"
            "{\n"
            "    \"tickets\": [{             (array of object)    A list of valid tickets that the user has added\n"
            "       \"status\": \"value\",     (string)             The current status of the added ticket\n"
            "       \"ticket\": \"value\",     (string)             The hash of the added ticket\n"
            "       \"ticketheight\": n,     (numeric)            The height in which the ticket was added\n"
            "       \"spentby\": \"value\",    (string)             The vote in which the ticket was spent\n"
            "       \"spentbyheight\": n,    (numeric)            The height in which the ticket was spent\n"
            "     },...],\n"
            "    \"invalid\": [\"value\",...], (array of string)    A list of invalid tickets that the user has added\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("stakepooluserinfo", "\"user\"")
            + HelpExampleRpc("stakepooluserinfo", "\"user\"")
        };

    const auto& user = request.params[0].get_str();

    UniValue obj{UniValue::VOBJ};

    return obj;
}

UniValue listwallets(const JSONRPCRequest& request)
{
    if (request.fHelp || !request.params.empty())
        throw std::runtime_error{
            "listwallets\n"
            "Returns a list of currently loaded wallets.\n"
            "For full information on the wallet, use \"getwalletinfo\"\n"
            "\nResult:\n"
            "[                         (json array of strings)\n"
            "  \"walletname\"            (string) the wallet name\n"
            "   ...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("listwallets", "")
            + HelpExampleRpc("listwallets", "")
        };

    UniValue obj{UniValue::VARR};

    for (auto pwallet : vpwallets) {

        if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
            return NullUniValue;
        }

        LOCK(pwallet->cs_wallet);

        obj.push_back(pwallet->GetName());
    }

    return obj;
}

UniValue resendwallettransactions(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || !request.params.empty())
        throw std::runtime_error(
            "resendwallettransactions\n"
            "Immediately re-broadcast unconfirmed wallet transactions to all peers.\n"
            "Intended only for testing; the wallet code periodically re-broadcasts\n"
            "automatically.\n"
            "Returns an RPC error if -walletbroadcast is set to false.\n"
            "Returns array of transaction ids that were re-broadcast.\n"
            );

    if (!g_connman)
        throw JSONRPCError(RPCErrorCode::CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");

    LOCK2(cs_main, pwallet->cs_wallet);

    if (!pwallet->GetBroadcastTransactions()) {
        throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "Error: Wallet transaction broadcasting is disabled with -walletbroadcast");
    }

    const auto txids = pwallet->ResendWalletTransactionsBefore(GetTime(), g_connman.get());
    UniValue result{UniValue::VARR};
    for (const auto& txid : txids)
    {
        result.push_back(txid.ToString());
    }
    return result;
}

UniValue listunspent(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 5)
        throw std::runtime_error{
            "listunspent ( minconf maxconf  [\"addresses\",...] [include_unsafe] [query_options])\n"
            "\nReturns array of unspent transaction outputs\n"
            "with between minconf and maxconf (inclusive) confirmations.\n"
            "Optionally filter to only include txouts paid to specified addresses.\n"
            "\nArguments:\n"
            "1. minconf          (numeric, optional, default=1) The minimum confirmations to filter\n"
            "2. maxconf          (numeric, optional, default=9999999) The maximum confirmations to filter\n"
            "3. \"addresses\"      (string) A json array of paicoin addresses to filter\n"
            "    [\n"
            "      \"address\"     (string) paicoin address\n"
            "      ,...\n"
            "    ]\n"
            "4. include_unsafe (bool, optional, default=true) Include outputs that are not safe to spend\n"
            "                  See description of \"safe\" attribute below.\n"
            "5. query_options    (json, optional) JSON with query options\n"
            "    {\n"
            "      \"minimumAmount\"    (numeric or string, default=0) Minimum value of each UTXO in " + CURRENCY_UNIT + "\n"
            "      \"maximumAmount\"    (numeric or string, default=unlimited) Maximum value of each UTXO in " + CURRENCY_UNIT + "\n"
            "      \"maximumCount\"     (numeric or string, default=unlimited) Maximum number of UTXOs\n"
            "      \"minimumSumAmount\" (numeric or string, default=unlimited) Minimum sum value of all UTXOs in " + CURRENCY_UNIT + "\n"
            "    }\n"
            "\nResult\n"
            "[                   (array of json object)\n"
            "  {\n"
            "    \"txid\" : \"txid\",          (string) the transaction id \n"
            "    \"vout\" : n,               (numeric) the vout value\n"
            "    \"address\" : \"address\",    (string) the paicoin address\n"
            "    \"account\" : \"account\",    (string) DEPRECATED. The associated account, or \"\" for the default account\n"
            "    \"scriptPubKey\" : \"key\",   (string) the script key\n"
            "    \"amount\" : x.xxx,         (numeric) the transaction output amount in " + CURRENCY_UNIT + "\n"
            "    \"confirmations\" : n,      (numeric) The number of confirmations\n"
            "    \"redeemScript\" : n        (string) The redeemScript if scriptPubKey is P2SH\n"
            "    \"spendable\" : xxx,        (bool) Whether we have the private keys to spend this output\n"
            "    \"solvable\" : xxx,         (bool) Whether we know how to spend this output, ignoring the lack of keys\n"
            "    \"safe\" : xxx              (bool) Whether this output is considered safe to spend. Unconfirmed transactions\n"
            "                              from outside keys and unconfirmed replacement transactions are considered unsafe\n"
            "                              and are not eligible for spending by fundrawtransaction and sendtoaddress.\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples\n"
            + HelpExampleCli("listunspent", "")
            + HelpExampleCli("listunspent", "6 9999999 \"[\\\"1PGFqEzfmQch1gKD3ra4k18PNj3tTUUSqg\\\",\\\"1LtvqCaApEdUGFkpKMM4MstjcaL4dKg8SP\\\"]\"")
            + HelpExampleRpc("listunspent", "6, 9999999 \"[\\\"1PGFqEzfmQch1gKD3ra4k18PNj3tTUUSqg\\\",\\\"1LtvqCaApEdUGFkpKMM4MstjcaL4dKg8SP\\\"]\"")
            + HelpExampleCli("listunspent", "6 9999999 '[]' true '{ \"minimumAmount\": 0.005 }'")
            + HelpExampleRpc("listunspent", "6, 9999999, [] , true, { \"minimumAmount\": 0.005 } ")
        };

    ObserveSafeMode();

    int nMinDepth{1};
    if (!request.params[0].isNull()) {
        RPCTypeCheckArgument(request.params[0], UniValue::VNUM);
        nMinDepth = request.params[0].get_int();
    }

    int nMaxDepth{9999999};
    if (!request.params[1].isNull()) {
        RPCTypeCheckArgument(request.params[1], UniValue::VNUM);
        nMaxDepth = request.params[1].get_int();
    }

    std::set<CTxDestination> destinations;
    if (!request.params[2].isNull()) {
        RPCTypeCheckArgument(request.params[2], UniValue::VARR);
        const auto& inputs = request.params[2].get_array();
        for (std::size_t idx{0}; idx < inputs.size(); ++idx) {
            const auto& input = inputs[idx];
            const auto dest = DecodeDestination(input.get_str());
            if (!IsValidDestination(dest)) {
                throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, std::string("Invalid PAIcoin address: ") + input.get_str());
            }
            if (!destinations.insert(dest).second) {
                throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, std::string("Invalid parameter, duplicated address: ") + input.get_str());
            }
        }
    }

    auto include_unsafe = true;
    if (!request.params[3].isNull()) {
        RPCTypeCheckArgument(request.params[3], UniValue::VBOOL);
        include_unsafe = request.params[3].get_bool();
    }

    CAmount nMinimumAmount{0};
    CAmount nMaximumAmount{MAX_MONEY};
    CAmount nMinimumSumAmount{MAX_MONEY};
    uint64_t nMaximumCount{0};

    if (!request.params[4].isNull()) {
        const auto& options = request.params[4].get_obj();

        if (options.exists("minimumAmount"))
            nMinimumAmount = AmountFromValue(options["minimumAmount"]);

        if (options.exists("maximumAmount"))
            nMaximumAmount = AmountFromValue(options["maximumAmount"]);

        if (options.exists("minimumSumAmount"))
            nMinimumSumAmount = AmountFromValue(options["minimumSumAmount"]);

        if (options.exists("maximumCount"))
            nMaximumCount = options["maximumCount"].get_int64();
    }

    UniValue results{UniValue::VARR};
    std::vector<COutput> vecOutputs;
    assert(pwallet != nullptr);
    LOCK2(cs_main, pwallet->cs_wallet);

    pwallet->AvailableCoins(vecOutputs, !include_unsafe, nullptr, nMinimumAmount, nMaximumAmount, nMinimumSumAmount, nMaximumCount, nMinDepth, nMaxDepth);
    for (const auto& out : vecOutputs) {
        CTxDestination address;
        const auto& scriptPubKey = out.tx->tx->vout[out.i].scriptPubKey;
        const auto fValidAddress = ExtractDestination(scriptPubKey, address);

        if (destinations.size() && (!fValidAddress || !destinations.count(address)))
            continue;

        UniValue entry{UniValue::VOBJ};
        entry.push_back(Pair("txid", out.tx->GetHash().GetHex()));
        entry.push_back(Pair("vout", out.i));

        if (fValidAddress) {
            entry.push_back(Pair("address", EncodeDestination(address)));

            if (pwallet->mapAddressBook.count(address)) {
                entry.push_back(Pair("account", pwallet->mapAddressBook[address].name));
            }

            if (scriptPubKey.IsPayToScriptHash()) {
                const auto& hash = boost::get<CScriptID>(address);
                CScript redeemScript;
                if (pwallet->GetCScript(hash, redeemScript)) {
                    entry.push_back(Pair("redeemScript", HexStr(redeemScript.begin(), redeemScript.end())));
                }
            }
        }

        entry.push_back(Pair("scriptPubKey", HexStr(scriptPubKey.begin(), scriptPubKey.end())));
        entry.push_back(Pair("amount", ValueFromAmount(out.tx->tx->vout[out.i].nValue)));
        entry.push_back(Pair("confirmations", out.nDepth));
        entry.push_back(Pair("spendable", out.fSpendable));
        entry.push_back(Pair("solvable", out.fSolvable));
        entry.push_back(Pair("safe", out.fSafe));
        results.push_back(entry);
    }

    return results;
}

UniValue purchaseticket(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 2 || request.params.size() > 10)
        throw std::runtime_error{
            "purchaseticket \"fromaccount\" spendlimit (minconf=1 \"ticketaddress\" numtickets \"pooladdress\" poolfees expiry \"comment\" ticketfee)\n"
            "\nPurchase ticket using available funds.\n"
            "\nArguments:\n"
            "1.  \"fromaccount\"    (string, required)             The account to use for purchase (default=\"default\")\n"
            "2.  spendlimit       (numeric, required)            Limit on the amount to spend on ticket\n"
            "3.  minconf          (numeric, optional, default=1) Minimum number of block confirmations required\n"
            "4.  ticketaddress    (string, optional)             Override the ticket address to which voting rights are given\n"
            "5.  numtickets       (numeric, optional)            The number of tickets to purchase\n"
            "6.  pooladdress      (string, optional)             The address to pay stake pool fees to\n"
            "7.  poolfees         (numeric, optional)            The amount of fees to pay to the stake pool\n"
            "8.  expiry           (numeric, optional)            Height at which the purchase tickets expire\n"
            "9.  comment          (string, optional)             Unused\n"
            "10. ticketfee        (numeric, optional)            The transaction fee rate (PAI/kB) to use (overrides fees set by the wallet config or settxfee RPC)\n"

            "\nResult:\n"
            "\"value\"              (string) Hash of the resulting ticket\n"

            "\nExamples:\n"
            "\nUse PAI from your default account to purchase a ticket if the current ticket price was a max of 50 PAI\n"
            + HelpExampleCli("purchaseticket", "\"default\" 50") +
            "\nPurchase 5 tickets, as the 5th argument (numtickets) is set to 5\n"
            + HelpExampleCli("purchaseticket", "\"default\" 50 1 \"\" 5") +
            "\nPurchase 5 tickets that would expire from the mempool if not mined by block 100,000, as the 8th argument (expiry) is set to 100000\n"
            + HelpExampleCli("purchaseticket", "\"default\" 50 1 \"\" 5 \"\" 0 100000")
        };

    ObserveSafeMode();
    LOCK2(cs_main, pwallet->cs_wallet);

    // Account
    const auto strAccount = AccountFromValue(request.params[0]);

    // Spend limit
    const auto nSpendLimit = AmountFromValue(request.params[1]);
    if (nSpendLimit <= 0)
        throw JSONRPCError(RPCErrorCode::TYPE_ERROR, "Invalid spend limit");

    // Minimum confirmations
    int nMinDepth{1};
    if (!request.params[2].isNull())
        nMinDepth = request.params[2].get_int();

    if (nMinDepth < 0)
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "negative minconf");

    // Ticket address
    CTxDestination ticketAddress;
    if (!request.params[3].isNull()) {
        const auto& str = request.params[3].get_str();
        if (!str.empty()) {
            ticketAddress = DecodeDestination(str);
            if (!IsValidDestination(ticketAddress)) {
                throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Invalid ticket address");
            }
        }
    }

    if (!IsValidDestination(ticketAddress)) {
        // Generate a new key that is added to wallet
        CPubKey newKey;
        if (!pwallet->GetKeyFromPool(newKey)) {
            throw JSONRPCError(RPCErrorCode::WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
        }
        ticketAddress = newKey.GetID();
    }

    // Number of tickets
    int nNumTickets{1};
    if (!request.params[4].isNull()) {
        nNumTickets = request.params[4].get_int();
        if (nNumTickets < 1)
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER,"Number of tickets must be at least 1");
    }
    
    // Pool address
    CTxDestination poolAddress;
    if (!request.params[5].isNull()) {
        const auto& str = request.params[5].get_str();
        if (!str.empty()) {
            poolAddress = DecodeDestination(request.params[5].get_str());
            if (!IsValidDestination(poolAddress)) {
                throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Invalid pool address");
            }
        }
    }

    double dfPoolFee{0.0};
    if(!request.params[6].isNull()) {
        dfPoolFee = request.params[6].get_real();
        // TODO make it CFeeRate and validate it
    }

    // Expiry
    int nExpiry{0};
    if (!request.params[7].isNull())
        nExpiry = request.params[7].get_int();

    if (nExpiry < 0)
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "negative expiry");

    // Ticket Fee
    CAmount ticketFeeIncrement;
    if (!request.params[9].isNull()) {
        ticketFeeIncrement = AmountFromValue(request.params[9]);
    }
    if (ticketFeeIncrement == 0) {
        // TODO read the wallet's default increment
    }


    // Perform a sanity check on expiry.
    if (nExpiry > 0  && nExpiry <= chainActive.Height() + 1)
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "expiry height must be above next block height");

    auto ticketPrice = CalculateNextRequiredStakeDifficulty(chainActive.Tip(), Params().GetConsensus());

    // Ensure the ticket price does not exceed the spend limit if set.
    if (ticketPrice > nSpendLimit)
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "ticket price above spend limit");

    // Check sanity of poolfee
    if (IsValidDestination(poolAddress) && dfPoolFee == 0.0)
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "stakepool fee percent unset");

    // check ticketAddr type, only P2PKH and P2SH are accepted
    // seems to always be the case while the address is valid

    // TODO calculate ticket fee based on estimated size and the ticketFee parameter
    const auto& ticketFee = CAmount{1000};
    const auto& neededPerTicket = ticketPrice + ticketFee;
    assert(neededPerTicket > 0);

    UniValue results{UniValue::VARR};

    EnsureWalletIsUnlocked(pwallet);
    const auto& splitTxAddr = GetAccountAddress(pwallet,"",true);

    for (int i = 0; i < nNumTickets; ++i) {
        const auto curBalance = pwallet->GetBalance();
        if (neededPerTicket > curBalance)
            throw JSONRPCError(RPCErrorCode::WALLET_INSUFFICIENT_FUNDS, "Insufficient funds");

        if (pwallet->GetBroadcastTransactions() && !g_connman) {
            throw JSONRPCError(RPCErrorCode::CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");
        }

        if (!IsValidDestination(poolAddress)) {
            // no pool used
            CMutableTransaction mFundTx;

            // NOTE: in Decred they are adding another regular transaction that collects all the needed funds
            // see purchaseTickets in createtx.go, they pay the needed value to an Internal address
            // For the moment we decided avoid constructing a regular transaction before purchase
            // as there were problems creating the block with both required transactions in it

            // create an output that pays the ticket
            mFundTx.vout.push_back(CTxOut(neededPerTicket, CScript()));

            CAmount nFeeRet;
            int nChangePosInOut = -1;
            auto strFailReason = std::string{};
            if (!pwallet->FundTransaction(mFundTx,nFeeRet,nChangePosInOut,strFailReason,false,{},CCoinControl{})) {
                throw JSONRPCError(RPCErrorCode::WALLET_ERROR, strFailReason);
            }

            CMutableTransaction mTicketTx;
            BuyTicketData buyTicketData = { 1 };    // version
            CScript declScript = GetScriptForBuyTicketDecl(buyTicketData);
            mTicketTx.vout.push_back(CTxOut(0, declScript));

            // create an output that pays ticket stake
            CScript ticketScript = GetScriptForDestination(ticketAddress);
            mTicketTx.vout.push_back(CTxOut(ticketPrice, ticketScript));

            if(mFundTx.vin.size() == 1) {
                //TODO only working for one input, fix this
                for (const auto& input : mFundTx.vin)
                {
                    mTicketTx.vin.push_back(input);

                    CKeyID rewardAddress;
                    {
                        // Generate a new key that is added to wallet
                        CPubKey newKey;
                        if (!pwallet->GetKeyFromPool(newKey)) {
                            throw JSONRPCError(RPCErrorCode::WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
                        }
                        rewardAddress = newKey.GetID();
                    }
                    const auto& contributedAmount = neededPerTicket; // in case no pool is used, this is equal to the price
                    const auto& ticketContribData = TicketContribData{ 1, rewardAddress, contributedAmount };
                    CScript contributorInfoScript = GetScriptForTicketContrib(ticketContribData);
                    mTicketTx.vout.push_back(CTxOut(0, contributorInfoScript));

                    // create an output which pays back change
                    auto changeKey = CKey();
                    changeKey.MakeNewKey(false);
                    auto changeAddr = changeKey.GetPubKey().GetID();
                    CAmount change = mFundTx.vout[nChangePosInOut].nValue + nFeeRet;
                    assert(change >= 0);
                    CScript changeScript = GetScriptForDestination(changeAddr);
                    mTicketTx.vout.push_back(CTxOut(change, changeScript));
                }
                std::string reason;
                if (!ValidateBuyTicketStructure(mTicketTx,reason))
                    throw JSONRPCError(RPCErrorCode::TRANSACTION_ERROR, "Error while constructing buy ticket transaction :" + reason);
                
                if (!pwallet->SignTransaction(mTicketTx))
                    throw JSONRPCError(RPCErrorCode::TRANSACTION_ERROR, "Signing transaction failed");

            }
            else {
                assert(!"Purchase ticket tx with multiple inputs not yet supported!");
            }

            CValidationState state;
            CWalletTx wtx;
            wtx.fTimeReceivedIsTxTime = true;
            wtx.BindWallet(pwallet);
            wtx.SetTx(MakeTransactionRef(std::move(mTicketTx)));
            CReserveKey reservekey{pwallet};
            if (!pwallet->CommitTransaction(wtx, reservekey, g_connman.get(), state)) {
                throw JSONRPCError(RPCErrorCode::TRANSACTION_ERROR, "CommitTransaction failed");
            }
            
            results.push_back(wtx.GetHash().GetHex());
        }
        else {
            // use pool adress
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Using pool address is not supported yet");
        }
    }

    return results;
}

UniValue generatevote(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 5)
        throw std::runtime_error{
            "generatevote \"blockhash\" height \"tickethash\" votebits \"votebitsext\"\n"
            "\nReturns the vote transaction encoded as a hexadecimal string\n"
            "\nArguments:\n"
            "1. blockhash   (string, required)  Block hash for the ticket\n"
            "2. height      (numeric, required) Block height for the ticket\n"
            "3. tickethash  (string, required)  The hash of the ticket\n"
            "4. votebits    (numeric, required) The voteBits to set for the ticket\n"
            "5. votebitsext (string, required)  The extended voteBits to set for the ticket\n"
            
            "\nResult:\n"
            "{\n"
            " \"hex\": \"value\", (string) The hex encoded transaction\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("generatevote", "\"blockhash\" 50 \"tickethash\" 1 \"ext\"")
        };

    ObserveSafeMode();
    LOCK2(cs_main, pwallet->cs_wallet);

    const auto& blockhash = uint256S(request.params[0].get_str());

    const auto& blockheight = request.params[1].get_int();
    if (blockheight < Params().GetConsensus().nStakeValidationHeight - 1)
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Height is lower than expected");

    const auto& tickethash = uint256S(request.params[2].get_str());

    const auto& nVoteBits = request.params[3].get_int();
    const auto& sVoteBitsExt = request.params[4].get_str();

    // identify the ticket tx to use for voting
    auto it = pwallet->mapWallet.find(tickethash);
    if (it == pwallet->mapWallet.end()) {
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Invalid or non-wallet ticket hash");
    }
    const auto& ticketTxPtr = it->second.tx;
    if (TX_BuyTicket != ParseTxClass(*ticketTxPtr)){
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Invalid ticket hash, must be hash of a ticket purchase transaction");
    }

    // extract contributions from ticket tx's outputs
    CAmount contributionSum = 0;
    auto contributions = std::vector<TicketContribData>{};
    for (unsigned i = ticketContribOutputIndex; i < ticketTxPtr->vout.size(); i += 2) {
        TicketContribData contrib;
        ParseTicketContrib(*ticketTxPtr, i, contrib);
        contributions.push_back(contrib);
        contributionSum += contrib.contributedAmount;
    }

    CMutableTransaction mVoteTx;

    // create a reward generation input
    mVoteTx.vin.push_back(CTxIn(COutPoint(), Params().GetConsensus().stakeBaseSigScript));
    mVoteTx.vin.push_back(CTxIn(COutPoint(tickethash, ticketStakeOutputIndex)));

    // create a structured OP_RETURN output containing tx declaration and voting data
    // TODO use specified votebits and votebitsext
    uint32_t voteYesBits = 0x0001;
    int voteVersion = 1;
    VoteData voteData = { voteVersion, blockhash, static_cast<uint32_t>(blockheight), voteYesBits };
    CScript declScript = GetScriptForVoteDecl(voteData);
    mVoteTx.vout.push_back(CTxOut(0, declScript));

    // All remaining outputs pay to the output destinations and amounts tagged
    // by the ticket purchase.
    const auto& subsidy = GetVoterSubsidy(chainActive.Tip()->nHeight+1/*spending Height*/, Params().GetConsensus());
    const auto& ticketPriceAtPurchase = ticketTxPtr->vout[ticketStakeOutputIndex].nValue;
    for ( const auto& contrib : contributions){
        const auto& reward = CalcContributorRemuneration( contrib.contributedAmount, ticketPriceAtPurchase, subsidy, contributionSum);
        CScript rewardScript;
        if (contrib.whichAddr == 1) {
            rewardScript = GetScriptForDestination(CKeyID(contrib.rewardAddr));
        } else {
            rewardScript = GetScriptForDestination(CScriptID(contrib.rewardAddr));
        }
        mVoteTx.vout.push_back(CTxOut(reward, rewardScript));
    }

    // sign the new vote tx
    {
        CTransaction txNewConst(mVoteTx);
        int nIn = voteStakeInputIndex;
        const auto& input = mVoteTx.vin[nIn];
        std::map<uint256, CWalletTx>::const_iterator mi = pwallet->mapWallet.find(input.prevout.hash);
        if(mi == pwallet->mapWallet.end() || input.prevout.n >= mi->second.tx->vout.size()) {
            throw JSONRPCError(RPCErrorCode::TRANSACTION_ERROR, "Signing transaction failed");
        }
        const CScript& scriptPubKey = mi->second.tx->vout[input.prevout.n].scriptPubKey;
        const CAmount& amount = mi->second.tx->vout[input.prevout.n].nValue;
        SignatureData sigdata;
        if (!ProduceSignature(TransactionSignatureCreator(pwallet, &txNewConst, nIn, amount, SIGHASH_ALL), scriptPubKey, sigdata)) {
            throw JSONRPCError(RPCErrorCode::TRANSACTION_ERROR, "Signing transaction failed");
        }
        UpdateTransaction(mVoteTx, nIn, sigdata);
    }

    std::string reason;
    if (!ValidateVoteStructure(mVoteTx,reason))
        throw JSONRPCError(RPCErrorCode::TRANSACTION_ERROR, "Error while constructing buy ticket transaction :" + reason);

    CValidationState state;
    CWalletTx wtx;
    wtx.fTimeReceivedIsTxTime = true;
    wtx.BindWallet(pwallet);
    wtx.SetTx(MakeTransactionRef(std::move(mVoteTx)));
    CReserveKey reservekey{pwallet};
    if (!pwallet->CommitTransaction(wtx, reservekey, g_connman.get(), state)) {
        throw JSONRPCError(RPCErrorCode::TRANSACTION_ERROR, "CommitTransaction failed");
    }
    
    UniValue result{UniValue::VOBJ};
    result.push_back(Pair("hex", wtx.GetHash().GetHex()));

    return result;
}

UniValue fundrawtransaction(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw std::runtime_error{
                            "fundrawtransaction \"hexstring\" ( options )\n"
                            "\nAdd inputs to a transaction until it has enough in value to meet its out value.\n"
                            "This will not modify existing inputs, and will add at most one change output to the outputs.\n"
                            "No existing outputs will be modified unless \"subtractFeeFromOutputs\" is specified.\n"
                            "Note that inputs which were signed may need to be resigned after completion since in/outputs have been added.\n"
                            "The inputs added will not be signed, use signrawtransaction for that.\n"
                            "Note that all existing inputs must have their previous output transaction be in the wallet.\n"
                            "Note that all inputs selected must be of standard form and P2SH scripts must be\n"
                            "in the wallet using importaddress or addmultisigaddress (to calculate fees).\n"
                            "You can see whether this is the case by checking the \"solvable\" field in the listunspent output.\n"
                            "Only pay-to-pubkey, multisig, and P2SH versions thereof are currently supported for watch-only\n"
                            "\nArguments:\n"
                            "1. \"hexstring\"           (string, required) The hex string of the raw transaction\n"
                            "2. options                 (object, optional)\n"
                            "   {\n"
                            "     \"changeAddress\"          (string, optional, default pool address) The paicoin address to receive the change\n"
                            "     \"changePosition\"         (numeric, optional, default random) The index of the change output\n"
                            "     \"includeWatching\"        (boolean, optional, default false) Also select inputs which are watch only\n"
                            "     \"lockUnspents\"           (boolean, optional, default false) Lock selected unspent outputs\n"
                            "     \"feeRate\"                (numeric, optional, default not set: makes wallet determine the fee) Set a specific fee rate in " + CURRENCY_UNIT + "/kB\n"
                            "     \"subtractFeeFromOutputs\" (array, optional) A json array of integers.\n"
                            "                              The fee will be equally deducted from the amount of each specified output.\n"
                            "                              The outputs are specified by their zero-based index, before any change output is added.\n"
                            "                              Those recipients will receive less paicoins than you enter in their corresponding amount field.\n"
                            "                              If no outputs are specified here, the sender pays the fee.\n"
                            "                                  [vout_index,...]\n"
                            "     \"replaceable\"            (boolean, optional) Marks this transaction as BIP125 replaceable.\n"
                            "                              Allows this transaction to be replaced by a transaction with higher fees\n"
                            "     \"conf_target\"            (numeric, optional) Confirmation target (in blocks)\n"
                            "     \"estimate_mode\"          (string, optional, default=UNSET) The fee estimate mode, must be one of:\n"
                            "         \"UNSET\"\n"
                            "         \"ECONOMICAL\"\n"
                            "         \"CONSERVATIVE\"\n"
                            "   }\n"
                            "                         for backward compatibility: passing in a true instead of an object will result in {\"includeWatching\":true}\n"
                            "\nResult:\n"
                            "{\n"
                            "  \"hex\":       \"value\", (string)  The resulting raw transaction (hex-encoded string)\n"
                            "  \"fee\":       n,         (numeric) Fee in " + CURRENCY_UNIT + " the resulting transaction pays\n"
                            "  \"changepos\": n          (numeric) The position of the added change output, or -1\n"
                            "}\n"
                            "\nExamples:\n"
                            "\nCreate a transaction with no inputs\n"
                            + HelpExampleCli("createrawtransaction", "\"[]\" \"{\\\"myaddress\\\":0.01}\"") +
                            "\nAdd sufficient unsigned inputs to meet the output value\n"
                            + HelpExampleCli("fundrawtransaction", "\"rawtransactionhex\"") +
                            "\nSign the transaction\n"
                            + HelpExampleCli("signrawtransaction", "\"fundedtransactionhex\"") +
                            "\nSend the transaction\n"
                            + HelpExampleCli("sendrawtransaction", "\"signedtransactionhex\"")
        };

    ObserveSafeMode();
    RPCTypeCheck(request.params, {UniValue::VSTR});

    CCoinControl coinControl;
    int changePosition{-1};
    auto lockUnspents = false;
    UniValue subtractFeeFromOutputs;
    std::set<int> setSubtractFeeFromOutputs;

    if (!request.params[1].isNull()) {
      if (request.params[1].type() == UniValue::VBOOL) {
        // backward compatibility bool only fallback
        coinControl.fAllowWatchOnly = request.params[1].get_bool();
      }
      else {
        RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::VOBJ});

        const auto& options = request.params[1];

        RPCTypeCheckObj(options,
            {
                {"changeAddress", UniValueType(UniValue::VSTR)},
                {"changePosition", UniValueType(UniValue::VNUM)},
                {"includeWatching", UniValueType(UniValue::VBOOL)},
                {"lockUnspents", UniValueType(UniValue::VBOOL)},
                {"reserveChangeKey", UniValueType(UniValue::VBOOL)}, // DEPRECATED (and ignored), should be removed in 0.16 or so.
                {"feeRate", UniValueType()}, // will be checked below
                {"subtractFeeFromOutputs", UniValueType(UniValue::VARR)},
                {"replaceable", UniValueType(UniValue::VBOOL)},
                {"conf_target", UniValueType(UniValue::VNUM)},
                {"estimate_mode", UniValueType(UniValue::VSTR)},
            },
            true, true);

        if (options.exists("changeAddress")) {
            const auto dest = DecodeDestination(options["changeAddress"].get_str());

            if (!IsValidDestination(dest)) {
                throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "changeAddress must be a valid paicoin address");
            }

            coinControl.destChange = dest;
        }

        if (options.exists("changePosition"))
            changePosition = options["changePosition"].get_int();

        if (options.exists("includeWatching"))
            coinControl.fAllowWatchOnly = options["includeWatching"].get_bool();

        if (options.exists("lockUnspents"))
            lockUnspents = options["lockUnspents"].get_bool();

        if (options.exists("feeRate"))
        {
            coinControl.m_feerate = CFeeRate(AmountFromValue(options["feeRate"]));
            coinControl.fOverrideFeeRate = true;
        }

        if (options.exists("subtractFeeFromOutputs"))
            subtractFeeFromOutputs = options["subtractFeeFromOutputs"].get_array();

        if (options.exists("replaceable")) {
            coinControl.signalRbf = options["replaceable"].get_bool();
        }
        if (options.exists("conf_target")) {
            if (options.exists("feeRate")) {
                throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Cannot specify both conf_target and feeRate");
            }
            coinControl.m_confirm_target = ParseConfirmTarget(options["conf_target"]);
        }
        if (options.exists("estimate_mode")) {
            if (options.exists("feeRate")) {
                throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Cannot specify both estimate_mode and feeRate");
            }
            if (!FeeModeFromString(options["estimate_mode"].get_str(), coinControl.m_fee_mode)) {
                throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid estimate_mode parameter");
            }
        }
      }
    }

    // parse hex string from parameter
    CMutableTransaction tx;
    if (!DecodeHexTx(tx, request.params[0].get_str(), true))
        throw JSONRPCError(RPCErrorCode::DESERIALIZATION_ERROR, "TX decode failed");

    if (tx.vout.size() == 0)
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "TX must have at least one output");

    if (changePosition != -1 && (changePosition < 0 || static_cast<unsigned int>(changePosition) > tx.vout.size()))
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "changePosition out of bounds");

    for (size_t idx{0}; idx < subtractFeeFromOutputs.size(); ++idx) {
        const int pos{subtractFeeFromOutputs[idx].get_int()};
        if (setSubtractFeeFromOutputs.count(pos))
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, strprintf("Invalid parameter, duplicated position: %d", pos));
        if (pos < 0)
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, strprintf("Invalid parameter, negative position: %d", pos));
        if (pos >= int(tx.vout.size()))
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, strprintf("Invalid parameter, position too large: %d", pos));
        setSubtractFeeFromOutputs.insert(pos);
    }

    CAmount nFeeOut;
    std::string strFailReason;

    if (!pwallet->FundTransaction(tx, nFeeOut, changePosition, strFailReason, lockUnspents, setSubtractFeeFromOutputs, coinControl)) {
        throw JSONRPCError(RPCErrorCode::WALLET_ERROR, strFailReason);
    }

    UniValue result{UniValue::VOBJ};
    result.push_back(Pair("hex", EncodeHexTx(tx)));
    result.push_back(Pair("changepos", changePosition));
    result.push_back(Pair("fee", ValueFromAmount(nFeeOut)));

    return result;
}

UniValue bumpfee(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp))
        return NullUniValue;

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2) {
        throw std::runtime_error{
            "bumpfee \"txid\" ( options ) \n"
            "\nBumps the fee of an opt-in-RBF transaction T, replacing it with a new transaction B.\n"
            "An opt-in RBF transaction with the given txid must be in the wallet.\n"
            "The command will pay the additional fee by decreasing (or perhaps removing) its change output.\n"
            "If the change output is not big enough to cover the increased fee, the command will currently fail\n"
            "instead of adding new inputs to compensate. (A future implementation could improve this.)\n"
            "The command will fail if the wallet or mempool contains a transaction that spends one of T's outputs.\n"
            "By default, the new fee will be calculated automatically using estimatefee.\n"
            "The user can specify a confirmation target for estimatefee.\n"
            "Alternatively, the user can specify totalFee, or use RPC settxfee to set a higher fee rate.\n"
            "At a minimum, the new fee rate must be high enough to pay an additional new relay fee (incrementalfee\n"
            "returned by getnetworkinfo) to enter the node's mempool.\n"
            "\nArguments:\n"
            "1. txid                  (string, required) The txid to be bumped\n"
            "2. options               (object, optional)\n"
            "   {\n"
            "     \"confTarget\"        (numeric, optional) Confirmation target (in blocks)\n"
            "     \"totalFee\"          (numeric, optional) Total fee (NOT feerate) to pay, in satoshis.\n"
            "                         In rare cases, the actual fee paid might be slightly higher than the specified\n"
            "                         totalFee if the tx change output has to be removed because it is too close to\n"
            "                         the dust threshold.\n"
            "     \"replaceable\"       (boolean, optional, default true) Whether the new transaction should still be\n"
            "                         marked bip-125 replaceable. If true, the sequence numbers in the transaction will\n"
            "                         be left unchanged from the original. If false, any input sequence numbers in the\n"
            "                         original transaction that were less than 0xfffffffe will be increased to 0xfffffffe\n"
            "                         so the new transaction will not be explicitly bip-125 replaceable (though it may\n"
            "                         still be replaceable in practice, for example if it has unconfirmed ancestors which\n"
            "                         are replaceable).\n"
            "     \"estimate_mode\"     (string, optional, default=UNSET) The fee estimate mode, must be one of:\n"
            "         \"UNSET\"\n"
            "         \"ECONOMICAL\"\n"
            "         \"CONSERVATIVE\"\n"
            "   }\n"
            "\nResult:\n"
            "{\n"
            "  \"txid\":    \"value\",   (string)  The id of the new transaction\n"
            "  \"origfee\":  n,         (numeric) Fee of the replaced transaction\n"
            "  \"fee\":      n,         (numeric) Fee of the new transaction\n"
            "  \"errors\":  [ str... ] (json array of strings) Errors encountered during processing (may be empty)\n"
            "}\n"
            "\nExamples:\n"
            "\nBump the fee, get the new transaction\'s txid\n" +
            HelpExampleCli("bumpfee", "<txid>")
        };
    }

    RPCTypeCheck(request.params, {UniValue::VSTR, UniValue::VOBJ});
    uint256 hash;
    hash.SetHex(request.params[0].get_str());

    // optional parameters
    CAmount totalFee{0};
    CCoinControl coin_control;
    coin_control.signalRbf = true;
    if (!request.params[1].isNull()) {
        const auto& options = request.params[1];
        RPCTypeCheckObj(options,
            {
                {"confTarget", UniValueType{UniValue::VNUM}},
                {"totalFee", UniValueType{UniValue::VNUM}},
                {"replaceable", UniValueType{UniValue::VBOOL}},
                {"estimate_mode", UniValueType{UniValue::VSTR}},
            },
            true, true);

        if (options.exists("confTarget") && options.exists("totalFee")) {
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "confTarget and totalFee options should not both be set. Please provide either a confirmation target for fee estimation or an explicit total fee for the transaction.");
        } else if (options.exists("confTarget")) { // TODO: alias this to conf_target
            coin_control.m_confirm_target = ParseConfirmTarget(options["confTarget"]);
        } else if (options.exists("totalFee")) {
            totalFee = options["totalFee"].get_int64();
            if (totalFee <= 0) {
                throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, strprintf("Invalid totalFee %s (must be greater than 0)", FormatMoney(totalFee)));
            }
        }

        if (options.exists("replaceable")) {
            coin_control.signalRbf = options["replaceable"].get_bool();
        }
        if (options.exists("estimate_mode")) {
            if (!FeeModeFromString(options["estimate_mode"].get_str(), coin_control.m_fee_mode)) {
                throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid estimate_mode parameter");
            }
        }
    }

    LOCK2(cs_main, pwallet->cs_wallet);
    EnsureWalletIsUnlocked(pwallet);

    CFeeBumper feeBump{pwallet, hash, coin_control, totalFee};
    const auto res = feeBump.getResult();
    if (res != BumpFeeResult::OK)
    {
        switch(res) {
            case BumpFeeResult::INVALID_ADDRESS_OR_KEY:
                throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, feeBump.getErrors()[0]);
                break;
            case BumpFeeResult::INVALID_REQUEST:
                throw JSONRPCError(RPCErrorCode::INVALID_REQUEST, feeBump.getErrors()[0]);
                break;
            case BumpFeeResult::INVALID_PARAMETER:
                throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, feeBump.getErrors()[0]);
                break;
            case BumpFeeResult::WALLET_ERROR:
                throw JSONRPCError(RPCErrorCode::WALLET_ERROR, feeBump.getErrors()[0]);
                break;
            default:
                throw JSONRPCError(RPCErrorCode::MISC_ERROR, feeBump.getErrors()[0]);
                break;
        }
    }

    // sign bumped transaction
    if (!feeBump.signTransaction(pwallet)) {
        throw JSONRPCError(RPCErrorCode::WALLET_ERROR, "Can't sign transaction.");
    }
    // commit the bumped transaction
    if(!feeBump.commit(pwallet)) {
        throw JSONRPCError(RPCErrorCode::WALLET_ERROR, feeBump.getErrors()[0]);
    }
    UniValue result{UniValue::VOBJ};
    result.push_back(Pair("txid", feeBump.getBumpedTxId().GetHex()));
    result.push_back(Pair("origfee", ValueFromAmount(feeBump.getOldFee())));
    result.push_back(Pair("fee", ValueFromAmount(feeBump.getNewFee())));
    UniValue errors{UniValue::VARR};
    for (const auto& err : feeBump.getErrors())
        errors.push_back(err);
    result.push_back(Pair("errors", errors));

    return result;
}

UniValue generate(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2) {
        throw std::runtime_error{
            "generate nblocks ( maxtries )\n"
            "\nMine up to nblocks blocks immediately (before the RPC call returns) to an address in the wallet.\n"
            "\nArguments:\n"
            "1. nblocks      (numeric, required) How many blocks are generated immediately.\n"
            "2. maxtries     (numeric, optional) How many iterations to try (default = 1000000).\n"
            "\nResult:\n"
            "[ blockhashes ]     (array) hashes of blocks generated\n"
            "\nExamples:\n"
            "\nGenerate 11 blocks\n"
            + HelpExampleCli("generate", "11")
        };
    }

    const auto num_generate = request.params[0].get_int();
    uint64_t max_tries{1000000};
    if (!request.params[1].isNull()) {
        max_tries = request.params[1].get_int();
    }

    std::shared_ptr<CReserveScript> coinbase_script;
    pwallet->GetScriptForMining(coinbase_script);

    // If the keypool is exhausted, no script is returned at all.  Catch this.
    if (!coinbase_script) {
        throw JSONRPCError(RPCErrorCode::WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
    }

    //throw an error if no script was provided
    if (coinbase_script->reserveScript.empty()) {
        throw JSONRPCError(RPCErrorCode::INTERNAL_ERROR, "No coinbase script available");
    }

    return generateBlocks(coinbase_script, num_generate, max_tries, true);
}

UniValue rescanblockchain(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 2) {
        throw std::runtime_error(
            "rescanblockchain (\"start_height\") (\"stop_height\")\n"
            "\nRescan the local blockchain for wallet related transactions.\n"
            "\nArguments:\n"
            "1. \"start_height\"    (numeric, optional) block height where the rescan should start\n"
            "2. \"stop_height\"     (numeric, optional) the last block height that should be scanned\n"
            "\nResult:\n"
            "{\n"
            "  \"start_height\"     (numeric) The block height where the rescan has started. If omitted, rescan started from the genesis block.\n"
            "  \"stop_height\"      (numeric) The height of the last rescanned block. If omitted, rescan stopped at the chain tip.\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("rescanblockchain", "100000 120000")
            + HelpExampleRpc("rescanblockchain", "100000 120000")
            );
    }

    LOCK2(cs_main, pwallet->cs_wallet);

    CBlockIndex *pindexStart = chainActive.Genesis();
    CBlockIndex *pindexStop = nullptr;
    if (!request.params[0].isNull()) {
        pindexStart = chainActive[request.params[0].get_int()];
        if (!pindexStart) {
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid start_height");
        }
    }

    if (!request.params[1].isNull()) {
        pindexStop = chainActive[request.params[1].get_int()];
        if (!pindexStop) {
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "Invalid stop_height");
        }
        else if (pindexStop->nHeight < pindexStart->nHeight) {
            throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "stop_height must be greater then start_height");
        }
    }

    // We can't rescan beyond non-pruned blocks, stop and throw an error
    if (fPruneMode) {
        CBlockIndex *block = pindexStop ? pindexStop : chainActive.Tip();
        while (block && block->nHeight >= pindexStart->nHeight) {
            if (!(block->nStatus & BLOCK_HAVE_DATA)) {
                throw JSONRPCError(RPCErrorCode::MISC_ERROR, "Can't rescan beyond pruned data. Use RPC call getblockchaininfo to determine your pruned height.");
            }
            block = block->pprev;
        }
    }

    CBlockIndex *stopBlock = pwallet->ScanForWalletTransactions(pindexStart, pindexStop, true);
    if (!stopBlock) {
        if (pwallet->IsAbortingRescan()) {
            throw JSONRPCError(RPCErrorCode::MISC_ERROR, "Rescan aborted.");
        }
        // if we got a nullptr returned, ScanForWalletTransactions did rescan up to the requested stopindex
        stopBlock = pindexStop ? pindexStop : chainActive.Tip();
    }
    else {
        throw JSONRPCError(RPCErrorCode::MISC_ERROR, "Rescan failed. Potentially corrupted data files.");
    }

    UniValue response(UniValue::VOBJ);
    response.pushKV("start_height", pindexStart->nHeight);
    response.pushKV("stop_height", stopBlock->nHeight);
    return response;
}

UniValue getmultisigoutinfo(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() !=2)
    {
        throw std::runtime_error{
            "getmultisigoutinfo \"hash\" index\n"
            "\nReturns information about a multisignature output.\n"
            "\nArguments:\n"
            "1. hash             (string, required)  Input hash to check.\n"
            "2. index            (numeric, required) Index of input.\n"
            "\nResult:\n"
            "{\n"
            "\"address\": \"value\",       (string)          Script address.\n"
            "\"redeemscript\": \"value\",  (string)          Hex of the redeeming script.\n"
            "\"m\": n,                   (numeric)         m (in m-of-n)\n"
            "\"n\": n,                   (numeric)         n (in m-of-n)\n"
            "\"pubkeys\": [\"value\",...], (array of string) Associated pubkeys.\n"
            "\"txhash\": \"value\",        (string)          txhash\n"
            "\"blockheight\": n,         (numeric)         Height of the containing block.\n"
            "\"blockhash\": \"value\",     (string)          Hash of the containing block.\n"
            "\"spent\": true|false,      (boolean)         If it has been spent.\n"
            "\"spentby\": \"value\",       (string)          Hash of spending tx.\n"
            "\"spentbyindex\": n,        (numeric)         Index of spending tx.\n"
            "\"amount\": n.nnn,          (numeric)         Amount of coins contained.\n"
            "}\n"
            "\nExamples:\n"
            //TODO update examples
            + HelpExampleCli("getmultisigoutinfo", "\"hash\" 2")
            + HelpExampleRpc("getmultisigoutinfo", "\"hash\" 2")
        };
    }

    ObserveSafeMode();
    LOCK2(cs_main, pwallet->cs_wallet);

    uint256 hash;
    hash.SetHex(request.params[0].get_str());

    const auto& nIndex = request.params[1].get_int();

    auto it = pwallet->mapWallet.find(hash);
    if (it == pwallet->mapWallet.end()) {
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Invalid or non-wallet transaction id");
    }

    const auto& wtx = it->second;
    const auto& txOut = wtx.tx->vout[nIndex];

    const auto& scriptPubKey = txOut.scriptPubKey;

    auto result = UniValue{UniValue::VOBJ};

    CTxDestination address;
    const auto fValidAddress = ExtractDestination(scriptPubKey, address);

    if (fValidAddress) {
        result.push_back(Pair("address", EncodeDestination(address)));

        if (txOut.scriptPubKey.IsPayToScriptHash()){
            const auto& hash = boost::get<CScriptID>(address);
            CScript redeemScript;
            if (pwallet->GetCScript(hash, redeemScript)) {
                result.push_back(Pair("redeemScript", HexStr(redeemScript.begin(), redeemScript.end())));

                txnouttype type;
                std::vector<CTxDestination> addresses;
                int nRequired;
                if (ExtractDestinations(redeemScript, type, addresses, nRequired)) {
                    result.push_back(Pair("m", nRequired));
                    result.push_back(Pair("n", static_cast<uint64_t>(addresses.size())));
                    auto pubkeys = UniValue{UniValue::VARR};
                    for (const auto& addr: addresses){
                        CPubKey pubkey;
                        if (pwallet->GetPubKey(boost::get<CKeyID>(addr),pubkey))
                            pubkeys.push_back(HexStr(pubkey));
                    }
                    result.push_back(Pair("pubkeys",pubkeys));
                }
            }

            result.push_back(Pair("txhash",wtx.GetHash().GetHex()));
            result.push_back(Pair("blockheight", mapBlockIndex[wtx.hashBlock]->nHeight));
            result.push_back(Pair("blockhash",wtx.hashBlock.GetHex()));

            auto spendingWtx = CWalletTx{};
            if (pwallet->IsSpent(wtx.GetHash(), nIndex, &spendingWtx)) {
                result.push_back(Pair("spent",true));
                result.push_back(Pair("spentby", spendingWtx.GetHash().GetHex()));

                // find the index in vin of spending transaction that comes from our hash
                size_t idx = 0;
                while (idx < spendingWtx.tx->vin.size() && spendingWtx.tx->vin[idx].prevout.hash != wtx.GetHash()) ++idx;
                assert( idx < spendingWtx.tx->vin.size());

                result.push_back(Pair("spentbyindex", static_cast<uint64_t>(idx)));
            } else {
                result.push_back(Pair("spent",false));
            }

        }
    }

    result.push_back(Pair("amount", ValueFromAmount(txOut.nValue)));

    return result;
}

// Defined in rpc/rawtransaction.cpp
extern void TxInErrorToJSON(const CTxIn& txin, UniValue& vErrorsRet, const std::string& strMessage);

static void RedeemMultisigOut(CWallet *const pwallet, const uint256& hash, int index, const CTxDestination& dest, UniValue& result)
{

    auto it = pwallet->mapWallet.find(hash);
    if (it == pwallet->mapWallet.end()) {
        throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Invalid or non-wallet transaction id");
    }

    const auto& wtx = it->second;
    const auto& txOut = wtx.tx->vout[index];

    const auto& rawtxIn = CTxIn{COutPoint(hash, index),CScript()};
    const auto& rawtxOut= CTxOut(txOut.nValue,GetScriptForDestination(dest));

    CMutableTransaction mtx;
    mtx.vin.push_back(rawtxIn);
    mtx.vout.push_back(rawtxOut);

    // Fetch previous transactions (inputs):
    CCoinsView viewDummy;
    CCoinsViewCache view(&viewDummy);
    {
        LOCK(mempool.cs);
        CCoinsViewCache &viewChain = *pcoinsTip;
        CCoinsViewMemPool viewMempool(&viewChain, mempool);
        view.SetBackend(viewMempool); // temporarily switch cache backend to db+mempool view

        for (const CTxIn& txin : mtx.vin) {
            view.AccessCoin(txin.prevout); // Load entries from viewChain into view; can fail.
        }

        view.SetBackend(viewDummy); // switch back to avoid locking mempool for too long
    }

    // Script verification errors
    UniValue vErrors(UniValue::VARR);

    CTxIn& txin = mtx.vin[0];
    // deduct the fee
    if (!pwallet->DummySignTx(mtx, std::set<CInputCoin>{CInputCoin(&wtx, txin.prevout.n)})) {
        TxInErrorToJSON(txin, vErrors, "Failed to dummy sign the transction!");
    }

    const auto& nBytes = GetVirtualTransactionSize(mtx);
    const auto& nFeeNeeded = GetRequiredFee(nBytes);
    auto& currentValue = mtx.vout[0].nValue;
    if (currentValue < nFeeNeeded) {
        TxInErrorToJSON(txin, vErrors, "The transaction amount is too small to pay the fee");
    }
    currentValue -= nFeeNeeded;

    const Coin& coin = view.AccessCoin(txin.prevout);
    if (coin.IsSpent()) {
        TxInErrorToJSON(txin, vErrors, "Input not found or already spent");
    }
    const CScript& prevPubKey = coin.out.scriptPubKey;
    const CAmount& amount = coin.out.nValue;

    SignatureData sigdata;
    const CTransaction txConst(mtx);
    const CKeyStore& keystore = *pwallet;
    ProduceSignature(MutableTransactionSignatureCreator(&keystore, &mtx, 0, amount, SIGHASH_ALL), prevPubKey, sigdata);
    sigdata = CombineSignatures(prevPubKey, TransactionSignatureChecker(&txConst, 0, amount), sigdata, DataFromTransaction(mtx, 0));

    UpdateTransaction(mtx, 0, sigdata);

    ScriptError serror = SCRIPT_ERR_OK;
    if (!VerifyScript(txin.scriptSig, prevPubKey, &txin.scriptWitness, STANDARD_SCRIPT_VERIFY_FLAGS, TransactionSignatureChecker(&txConst, 0, amount), &serror)) {
        TxInErrorToJSON(txin, vErrors, ScriptErrorString(serror));
    }

    bool fComplete = vErrors.empty();

    result.push_back(Pair("hex", EncodeHexTx(mtx)));
    result.push_back(Pair("complete", fComplete));
    if (!vErrors.empty()) {
        result.push_back(Pair("errors", vErrors));
    }
}

CTxDestination GetOrGenerateAddress(const JSONRPCRequest& request, int paramIndex, CWallet *const pwallet)
{
    CTxDestination dest;
    if (!request.params[paramIndex].isNull()) {
        dest = DecodeDestination(request.params[paramIndex].get_str());
        if (!IsValidDestination(dest)) {
            throw JSONRPCError(RPCErrorCode::INVALID_ADDRESS_OR_KEY, "Invalid PAIcoin address");
        }
    }
    else{
        // Generate a new key that is added to wallet
        CPubKey newKey;
        if (!pwallet->GetKeyFromPool(newKey)) {
            throw JSONRPCError(RPCErrorCode::WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
        }
        dest = newKey.GetID();
    }
    return dest;
}

UniValue redeemmultisigout(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 3 || request.params.size() > 4)
    {
        throw std::runtime_error{
            "redeemmultisigout \"hash\" index tree (\"address\")\n"
            "\nTakes the input and constructs a P2PKH paying to the specified address.\n"
            "\nArguments:\n"
            "1. hash    (string, required)  Hash of the input transaction\n"
            "2. index   (numeric, required) Idx of the input transaction\n"
            "3. tree    (numeric, required) Tree the transaction is on.\n"
            "4. address (string, optional)  Address to pay to.\n"
            "\nResult:\n"
            "{\n"
            " \"hex\": \"value\",         (string)          Resulting hash.\n"
            " \"complete\": true|false, (boolean)         Shows if opperation was completed.\n"
            " \"errors\": [{            (array of object) Any errors generated.\n"
            "  \"txid\": \"value\",       (string)          The transaction hash of the referenced previous output\n"
            "  \"vout\": n,             (numeric)         The output index of the referenced previous output\n"
            "  \"scriptSig\": \"value\",  (string)          The hex-encoded signature script\n"
            "  \"sequence\": n,         (numeric)         Script sequence number\n"
            "  \"error\": \"value\",      (string)          Verification or signing error related to the input\n"
            " },...],\n"
            "}\n"
            "\nExamples:\n"
            //TODO update examples
            + HelpExampleCli("redeemmultisigout", "\"hash\" 2 220")
            + HelpExampleRpc("redeemmultisigout", "\"hash\" 2 220")
        };
    }

    ObserveSafeMode();
    LOCK2(cs_main, pwallet->cs_wallet);

    uint256 hash;
    hash.SetHex(request.params[0].get_str());

    const auto& nIndex = request.params[1].get_int();
    const auto& nTree = request.params[2].get_int();

    EnsureWalletIsUnlocked(pwallet);
    const auto& dest = GetOrGenerateAddress(request,3,pwallet);

    UniValue result(UniValue::VOBJ);
    RedeemMultisigOut(pwallet, hash, nIndex, dest, result);

    return result;
}

UniValue redeemmultisigouts(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 3)
    {
        throw std::runtime_error{
            "redeemmultisigouts \"fromscraddress\" (\"toaddress\" number)\n"
            "\nTakes a hash, looks up all unspent outpoints and generates list of signed transactions spending to either an address specified or internal addresses\n"
            "\nArguments:\n"
            "1. fromscraddress (string, required)  Input script hash address.\n"
            "2. toaddress      (string, optional)  Address to look for (if not internal addresses).\n"
            "3. number         (numeric, optional) Number of outpoints found.\n"
            "\nResult:\n"
            "{\n"
            " \"hex\": \"value\",         (string)          Resulting hash.\n"
            " \"complete\": true|false, (boolean)         Shows if opperation was completed.\n"
            " \"errors\": [{            (array of object) Any errors generated.\n"
            "  \"txid\": \"value\",       (string)          The transaction hash of the referenced previous output\n"
            "  \"vout\": n,             (numeric)         The output index of the referenced previous output\n"
            "  \"scriptSig\": \"value\",  (string)          The hex-encoded signature script\n"
            "  \"sequence\": n,         (numeric)         Script sequence number\n"
            "  \"error\": \"value\",      (string)          Verification or signing error related to the input\n"
            " },...],\n"
            "}\n"
            "\nExamples:\n"
            //TODO update examples
            + HelpExampleCli("redeemmultisigouts", "\"fromaddress\"")
            + HelpExampleRpc("redeemmultisigouts", "\"fromaddress\"")
        };
    }

    ObserveSafeMode();

    const auto& scrAddress = DecodeDestination(request.params[0].get_str());
    if (scrAddress.which() != 2/*CScriptID template pos*/) {
        throw JSONRPCError(RPCErrorCode::INVALID_PARAMETER, "fromscraddress must be a script hash address");
    }

    auto nMaxOutputs = std::numeric_limits<int>::max();
    if (!request.params[2].isNull())
        nMaxOutputs = request.params[2].get_int();

    EnsureWalletIsUnlocked(pwallet);

    const auto& sToAddress = GetOrGenerateAddress(request,1,pwallet);

    UniValue results{UniValue::VARR};

    auto nCount = 1;
    std::vector<COutput> vecOutputs;
    pwallet->AvailableCoins(vecOutputs);
    for (const auto& out : vecOutputs) {
        const auto& scriptPubKey = out.tx->tx->vout[out.i].scriptPubKey;
        if (scriptPubKey.IsPayToScriptHash()) {
            CTxDestination address;
            const auto fValidAddress = ExtractDestination(scriptPubKey, address);

            if (!fValidAddress || address != scrAddress) {
                continue;
            }

            if (nCount > nMaxOutputs) {
                break;
            }

            UniValue entry{UniValue::VOBJ};
            RedeemMultisigOut(pwallet, out.tx->GetHash(), out.i, sToAddress, entry);
            results.push_back(entry);
            ++nCount;
        }
    }

    return results;
}

UniValue sendtomultisig(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() < 3 || request.params.size() > 6)
    {
        throw std::runtime_error{
            "sendtomultisig \"fromaccount\" amount [\"pubkey\",...] (nrequired=1 minconf=1 \"comment\")\n"
            "\nAuthors, signs, and sends a transaction that outputs some amount to a multisig address.\n"
            "Unlike sendfrom, outputs are always chosen from the default account.\n"
            "A change output is automatically included to send extra output value back to the original account.\n"
            "\nArguments:\n"
            "1. fromaccount (string, required)             Unused\n"
            "2. amount      (numeric, required)            Amount to send to the payment address valued in PAI coin\n"
            "3. pubkeys     (array of string, required)    Pubkey to send to.\n"
            "4. nrequired   (numeric, optional, default=1) The number of signatures required to redeem outputs paid to this address\n"
            "5. minconf     (numeric, optional, default=1) Minimum number of block confirmations required\n"
            "6. comment     (string, optional)             Unused\n"
            "\nResult:\n"
            "\"value\"    (string)      The transaction hash of the sent transaction\n"
            "\nExamples:\n"
            //TODO update examples
            + HelpExampleCli("sendtomultisig", "\"fromaccount\" 2 \"[\\\"16sSauSf5pF2UkUwvKGq4qjNRzBZYqgEL5\\\",\\\"171sgjn4YtPu27adkKGrdDwzRTxnRkBfKV\\\"]\"")
            + HelpExampleRpc("sendtomultisig", "\"fromaccount\" 2 \"[\\\"16sSauSf5pF2UkUwvKGq4qjNRzBZYqgEL5\\\",\\\"171sgjn4YtPu27adkKGrdDwzRTxnRkBfKV\\\"]\"")
        };
    }

    ObserveSafeMode();
    LOCK2(cs_main, pwallet->cs_wallet);

    const auto& sFromAccount = request.params[0].get_str();
    // Amount
    const auto nAmount = AmountFromValue(request.params[1]);
    if (nAmount <= 0)
        throw JSONRPCError(RPCErrorCode::TYPE_ERROR, "Invalid amount for send");

    auto nRequired = 1;
    if (!request.params[3].isNull())
        nRequired = request.params[3].get_int();
    if (nRequired < 1)
        throw std::runtime_error("a multisignature address must require at least one key to redeem");

    RPCTypeCheckArgument(request.params[2], UniValue::VARR);
    const auto& keys = request.params[2].get_array();
    if (static_cast<int>(keys.size()) < nRequired)
        throw std::runtime_error(
            strprintf("not enough keys supplied "
                      "(got %u keys, but need at least %d to redeem)", keys.size(), nRequired));
    if (keys.size() > 16)
        throw std::runtime_error("Number of addresses involved in the multisignature address creation > 16\nReduce the number");
    std::vector<CPubKey> pubkeys;
    pubkeys.resize(keys.size());
    for (size_t i{0}; i < keys.size(); ++i)
    {
        const auto& ks = keys[i].get_str();
        if (IsHex(ks))
        {
            CPubKey vchPubKey{ParseHex(ks)};
            if (!vchPubKey.IsFullyValid())
                throw std::runtime_error(" Invalid public key: "+ks);
            pubkeys[i] = vchPubKey;
        }
        else
        {
            throw std::runtime_error(" Invalid public key: "+ks);
        }
    }

    auto nMinConf = 1;
    if (!request.params[4].isNull())
        nMinConf = request.params[4].get_int();

    auto sComment = std::string{};
    if (!request.params[5].isNull())
        sComment = request.params[5].get_str();

    const auto inner = GetScriptForMultisig(nRequired, pubkeys);
    if (inner.size() > MAX_SCRIPT_ELEMENT_SIZE)
        throw std::runtime_error(
                strprintf("redeemScript exceeds size limit: %d > %d", inner.size(), MAX_SCRIPT_ELEMENT_SIZE));
    CScriptID innerID{inner};

    EnsureWalletIsUnlocked(pwallet);
    pwallet->AddCScript(inner);

    const auto& dest = CTxDestination{innerID}; 

    // Wallet comments
    CWalletTx wtx;
    SendMoney(pwallet, dest, nAmount, false /*fSubtractFeeFromAmount*/, wtx, CCoinControl{});
    return wtx.GetHash().GetHex();
}

UniValue getmasterpubkey(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 1)
    {
        throw std::runtime_error{
            "getmasterpubkey (\"account\")\n"
            "\nRequests the master pubkey from the wallet.\n"
            "\nArguments:\n"
            "1. account (string, optional) The account to get the master pubkey for\n"
            "\nResult:\n"
            "\"value\" (string) The master pubkey for the wallet\n"
            "\nExamples:\n"
            //TODO update examples
            + HelpExampleCli("getmasterpubkey", "\"account\"")
            + HelpExampleRpc("getmasterpubkey", "")
        };
    }

    auto sAccount = std::string{};
    if (!request.params[0].isNull())
        sAccount = request.params[0].get_str();

    CPubKey pubkey;
    if (!pwallet->GetAccountPubkey(pubkey, sAccount)) {
        throw JSONRPCError(RPCErrorCode::WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
    }

    return HexStr(pubkey);
}

UniValue listscripts(const JSONRPCRequest& request)
{
    const auto pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || !request.params.empty())
    {
        throw std::runtime_error{
            "listscripts\n"
            "\nList all scripts that have been added to wallet.\n"
            "\nResult:\n"
            "\"scripts\"            (array) A list of the imported scripts\n"
            "[\n"
            "  {\n"
            "    \"hash160\"        (string) The script hash\n"
            "    \"address\"        (string) The script address\n"
            "    \"redeemscript\"   (string) The redeem script\n"
            "  },\n"
            "  ...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("listscripts", "")
            + HelpExampleRpc("listscripts", "")
        };
    }

    std::vector<std::pair<CScriptID, CScript>> allScripts;
    {
        // Lock for the least amount of time
        LOCK(pwallet->cs_wallet);
        allScripts = pwallet->GetAllCScripts();
    }
    if (allScripts.empty()) {
        return NullUniValue;
    }

    UniValue result{UniValue::VARR};
    for (auto const& scriptPair : allScripts) {
        auto const& scriptID = scriptPair.first;
        auto const& script = scriptPair.second;

        UniValue uniScript{UniValue::VOBJ};
        uniScript.push_back(std::make_pair("hash160", HexStr(scriptID)));
        uniScript.push_back(std::make_pair("address", EncodeDestination(scriptID)));
        uniScript.push_back(std::make_pair("redeemscript", HexStr(script)));

        result.push_back(uniScript);
    }

    return result;
}

extern UniValue abortrescan(const JSONRPCRequest& request); // in rpcdump.cpp
extern UniValue dumpprivkey(const JSONRPCRequest& request); // in rpcdump.cpp
extern UniValue importprivkey(const JSONRPCRequest& request);
extern UniValue importaddress(const JSONRPCRequest& request);
extern UniValue importpubkey(const JSONRPCRequest& request);
extern UniValue dumpwallet(const JSONRPCRequest& request);
extern UniValue importwallet(const JSONRPCRequest& request);
extern UniValue importprunedfunds(const JSONRPCRequest& request);
extern UniValue removeprunedfunds(const JSONRPCRequest& request);
extern UniValue importmulti(const JSONRPCRequest& request);
extern UniValue importscript(const JSONRPCRequest& request);
extern UniValue rescanblockchain(const JSONRPCRequest& request);

static const CRPCCommand commands[] =
{ //  category              name                        actor (function)           argNames
    //  --------------------- ------------------------    -----------------------  ----------
    { "rawtransactions",    "fundrawtransaction",       &fundrawtransaction,       {"hexstring","options"} },
    { "hidden",             "resendwallettransactions", &resendwallettransactions, {} },
    { "wallet",             "abandontransaction",       &abandontransaction,       {"txid"} },
    { "wallet",             "abortrescan",              &abortrescan,              {} },
    { "wallet",             "addmultisigaddress",       &addmultisigaddress,       {"nrequired","keys","account"} },
    { "wallet",             "addwitnessaddress",        &addwitnessaddress,        {"address"} },
    { "wallet",             "backupwallet",             &backupwallet,             {"destination"} },
    { "wallet",             "bumpfee",                  &bumpfee,                  {"txid", "options"} },
    { "wallet",             "dumpprivkey",              &dumpprivkey,              {"address"}  },
    { "wallet",             "dumpwallet",               &dumpwallet,               {"filename"} },
    { "wallet",             "encryptwallet",            &encryptwallet,            {"passphrase"} },
    { "wallet",             "getaccountaddress",        &getaccountaddress,        {"account"} },
    { "wallet",             "getaccount",               &getaccount,               {"address"} },
    { "wallet",             "getaddressesbyaccount",    &getaddressesbyaccount,    {"account"} },
    { "wallet",             "getbalance",               &getbalance,               {"account","minconf","include_watchonly"} },
    { "wallet",             "getnewaddress",            &getnewaddress,            {"account"} },
    { "wallet",             "createnewaccount",         &createnewaccount,         {"account"} },
    { "wallet",             "renameaccount",            &renameaccount,            {"oldAccount", "newAccount"} },
    { "wallet",             "getrawchangeaddress",      &getrawchangeaddress,      {} },
    { "wallet",             "getreceivedbyaccount",     &getreceivedbyaccount,     {"account","minconf"} },
    { "wallet",             "getreceivedbyaddress",     &getreceivedbyaddress,     {"address","minconf"} },
    { "wallet",             "getticketfee",             &getticketfee,             {} },
    { "wallet",             "gettickets",               &gettickets,               {"includeimmature"} },
    { "wallet",             "revoketickets",            &revoketickets,            {} },
    { "wallet",             "setticketfee",             &setticketfee,             {"fee"} },
    { "wallet",             "listtickets",              &listtickets,              {} },
    { "wallet",             "gettransaction",           &gettransaction,           {"txid","include_watchonly"} },
    { "wallet",             "getunconfirmedbalance",    &getunconfirmedbalance,    {} },
    { "wallet",             "getwalletinfo",            &getwalletinfo,            {} },
    { "wallet",             "getstakeinfo",             &getstakeinfo,             {} },
    { "wallet",             "stakepooluserinfo",        &stakepooluserinfo,        {"user"} },
    { "wallet",             "importmulti",              &importmulti,              {"requests","options"} },
    { "wallet",             "importprivkey",            &importprivkey,            {"privkey","label","rescan"} },
    { "wallet",             "importwallet",             &importwallet,             {"filename"} },
    { "wallet",             "importaddress",            &importaddress,            {"address","label","rescan","p2sh"} },
    { "wallet",             "importscript",             &importscript,             {"script","rescan","scanfrom"} },
    { "wallet",             "importprunedfunds",        &importprunedfunds,        {"rawtransaction","txoutproof"} },
    { "wallet",             "importpubkey",             &importpubkey,             {"pubkey","label","rescan"} },
    { "wallet",             "keypoolrefill",            &keypoolrefill,            {"newsize"} },
    { "wallet",             "listaccounts",             &listaccounts,             {"minconf","include_watchonly"} },
    { "wallet",             "listaddressgroupings",     &listaddressgroupings,     {} },
    { "wallet",             "listlockunspent",          &listlockunspent,          {} },
    { "wallet",             "listreceivedbyaccount",    &listreceivedbyaccount,    {"minconf","include_empty","include_watchonly"} },
    { "wallet",             "listreceivedbyaddress",    &listreceivedbyaddress,    {"minconf","include_empty","include_watchonly"} },
    { "wallet",             "listsinceblock",           &listsinceblock,           {"blockhash","target_confirmations","include_watchonly","include_removed"} },
    { "wallet",             "listtransactions",         &listtransactions,         {"account","count","skip","include_watchonly"} },
    { "wallet",             "listunspent",              &listunspent,              {"minconf","maxconf","addresses","include_unsafe","query_options"} },
    { "wallet",             "purchaseticket",           &purchaseticket,           {"spendlimit","fromaccount","minconf","ticketaddress","numtickets","pooladdress","poolfees","expiry","comment","ticketfee"} },
    { "wallet",             "generatevote",             &generatevote,             {"blockhash","height","tickethash","votebits","votebitsext"} },
    { "wallet",             "listwallets",              &listwallets,              {} },
    { "wallet",             "listscripts",              &listscripts,              {} },
    { "wallet",             "lockunspent",              &lockunspent,              {"unlock","transactions"} },
    { "wallet",             "move",                     &movecmd,                  {"fromaccount","toaccount","amount","minconf","comment"} },
    { "wallet",             "sendfrom",                 &sendfrom,                 {"fromaccount","toaddress","amount","minconf","comment","comment_to"} },
    { "wallet",             "sendmany",                 &sendmany,                 {"fromaccount","amounts","minconf","comment","subtractfeefrom","replaceable","conf_target","estimate_mode"} },
    { "wallet",             "sendtoaddress",            &sendtoaddress,            {"address","amount","comment","comment_to","subtractfeefromamount","replaceable","conf_target","estimate_mode"} },
    { "wallet",             "setaccount",               &setaccount,               {"address","account"} },
    { "wallet",             "settxfee",                 &settxfee,                 {"amount"} },
    { "wallet",             "signmessage",              &signmessage,              {"address","message"} },
    { "wallet",             "walletlock",               &walletlock,               {} },
    { "wallet",             "walletpassphrasechange",   &walletpassphrasechange,   {"oldpassphrase","newpassphrase"} },
    { "wallet",             "walletpassphrase",         &walletpassphrase,         {"passphrase","timeout"} },
    { "wallet",             "removeprunedfunds",        &removeprunedfunds,        {"txid"} },
    { "wallet",             "rescanblockchain",         &rescanblockchain,         {"start_height", "stop_height"} },
    { "wallet",             "getmultisigoutinfo",       &getmultisigoutinfo,       {"hash", "index"} },
    { "wallet",             "redeemmultisigout",        &redeemmultisigout,        {"hash", "index", "tree", "address"} },
    { "wallet",             "redeemmultisigouts",       &redeemmultisigouts,       {"fromscraddress", "toaddress", "number"} },
    { "wallet",             "sendtomultisig",           &sendtomultisig,           {"fromaccount", "amount", "pubkeys", "nrequired", "minconf", "comment"} },
    { "wallet",             "getmasterpubkey",          &getmasterpubkey,          {"account"} },

    { "generating",         "generate",                 &generate,                 {"nblocks","maxtries"} },
};

void RegisterWalletRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
