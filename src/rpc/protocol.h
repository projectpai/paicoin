// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAICOIN_RPCPROTOCOL_H
#define PAICOIN_RPCPROTOCOL_H

#include <string>

#include <univalue.h>

//! PAI Coin RPC error codes
enum class RPCErrorCode
{
    //! Standard JSON-RPC 2.0 errors
    // INVALID_REQUEST is internally mapped to HTTPStatusCode::BAD_REQUEST (400).
    // It should not be used for application-layer errors.
    INVALID_REQUEST  = -32600,
    // METHOD_NOT_FOUND is internally mapped to HTTPStatusCode::NOT_FOUND (404).
    // It should not be used for application-layer errors.
    METHOD_NOT_FOUND = -32601,
    INVALID_PARAMS   = -32602,
    // INTERNAL_ERROR should only be used for genuine errors in paicoind
    // (for example datadir corruption).
    INTERNAL_ERROR   = -32603,
    PARSE_ERROR      = -32700,

    //! General application defined errors
    MISC_ERROR                  = -1,  //!< std::exception thrown in command handling
    FORBIDDEN_BY_SAFE_MODE      = -2,  //!< Server is in safe mode, and command is not allowed in safe mode
    TYPE_ERROR                  = -3,  //!< Unexpected type was passed as parameter
    INVALID_ADDRESS_OR_KEY      = -5,  //!< Invalid address or key
    OUT_OF_MEMORY               = -7,  //!< Ran out of memory during operation
    INVALID_PARAMETER           = -8,  //!< Invalid, missing or duplicate parameter
    DATABASE_ERROR              = -20, //!< Database error
    DESERIALIZATION_ERROR       = -22, //!< Error parsing or validating structure in raw format
    VERIFY_ERROR                = -25, //!< General error during transaction or block submission
    VERIFY_REJECTED             = -26, //!< Transaction or block was rejected by network rules
    VERIFY_ALREADY_IN_CHAIN     = -27, //!< Transaction already in chain
    IN_WARMUP                   = -28, //!< Client still warming up

    //! Aliases for backward compatibility
    TRANSACTION_ERROR           = VERIFY_ERROR,
    TRANSACTION_REJECTED        = VERIFY_REJECTED,
    TRANSACTION_ALREADY_IN_CHAIN= VERIFY_ALREADY_IN_CHAIN,

    //! P2P client errors
    CLIENT_NOT_CONNECTED        = -9,  //!< PAI Coin is not connected
    CLIENT_IN_INITIAL_DOWNLOAD  = -10, //!< Still downloading initial blocks
    CLIENT_NODE_ALREADY_ADDED   = -23, //!< Node is already added
    CLIENT_NODE_NOT_ADDED       = -24, //!< Node has not been added before
    CLIENT_NODE_NOT_CONNECTED   = -29, //!< Node to disconnect not found in connected nodes
    CLIENT_INVALID_IP_OR_SUBNET = -30, //!< Invalid IP/Subnet
    CLIENT_P2P_DISABLED         = -31, //!< No valid connection manager instance found

    //! Wallet errors
    WALLET_ERROR                = -4,  //!< Unspecified problem with wallet (key not found etc.)
    WALLET_INSUFFICIENT_FUNDS   = -6,  //!< Not enough funds in wallet or account
    WALLET_INVALID_ACCOUNT_NAME = -11, //!< Invalid account name
    WALLET_KEYPOOL_RAN_OUT      = -12, //!< Keypool ran out, call keypoolrefill first
    WALLET_UNLOCK_NEEDED        = -13, //!< Enter the wallet passphrase with walletpassphrase first
    WALLET_PASSPHRASE_INCORRECT = -14, //!< The wallet passphrase entered was incorrect
    WALLET_WRONG_ENC_STATE      = -15, //!< Command given in wrong wallet encryption state (encrypting an encrypted wallet etc.)
    WALLET_ENCRYPTION_FAILED    = -16, //!< Failed to encrypt the wallet
    WALLET_ALREADY_UNLOCKED     = -17, //!< Wallet is already unlocked
    WALLET_NOT_FOUND            = -18, //!< Invalid wallet specified
    WALLET_NOT_SPECIFIED        = -19, //!< No wallet specified (error when there are multiple wallets loaded)
};

UniValue JSONRPCRequestObj(const std::string& strMethod, const UniValue& params, const UniValue& id);
UniValue JSONRPCReplyObj(const UniValue& result, const UniValue& error, const UniValue& id);
std::string JSONRPCReply(const UniValue& result, const UniValue& error, const UniValue& id);
UniValue JSONRPCError(RPCErrorCode code, const std::string& message);

/** Generate a new RPC authentication cookie and write it to disk */
bool GenerateAuthCookie(std::string *cookie_out);
/** Read the RPC authentication cookie from disk */
bool GetAuthCookie(std::string *cookie_out);
/** Delete RPC authentication cookie from disk */
void DeleteAuthCookie();

#endif // PAICOIN_RPCPROTOCOL_H
