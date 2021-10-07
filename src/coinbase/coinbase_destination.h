/* * Copyright (c) 2021 Project PAI Foundation
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#ifndef PAICOIN_COINBASE_DESTINATION_H
#define PAICOIN_COINBASE_DESTINATION_H

#include <string>
#include <vector>

#include <pubkey.h>
#include <script/standard.h>
#include <uint256.h>
#include <serialize.h>

/**
 * The type of address to be used when specifying a new destination.
 */
enum ECoinbaseDestinationType : unsigned int {
    Unknown = 0,
    PubKeyHash,     // P2PKH
    ScriptHash,     // P2SH
    Count
};

/**
 * A destination that can be used for coinbase transaction subsidies.
 */
class CoinbaseDestination {
public:

    //! Constructors throw InvalidArgumentException
    CoinbaseDestination();
    CoinbaseDestination(const std::string& address, const int expiration_height = -1, const bool hardcoded = false);
    CoinbaseDestination(const ECoinbaseDestinationType type, const uint160& hash, const int expiration_height = -1, const bool hardcoded = false);
    CoinbaseDestination(const CKeyID& keyId, const int expiration_height = -1, const bool hardcoded = false);
    CoinbaseDestination(const CScriptID& scriptId, const int expiration_height = -1, const bool hardcoded = false);
    CoinbaseDestination(const CTxDestination& destination, const int expiration_height = -1, const bool hardcoded = false);

    //! Accessors
    ECoinbaseDestinationType address_type() const { return type; }
    int expiration_height() const { return height; }
    int is_hardcoded() const { return hardcoded; }

    //! Simple read-only vector-like interface to the pubkey data.
    size_t size() const { return hash.size(); }
    const unsigned char* begin() const { return hash.begin(); }
    const unsigned char* end() const { return hash.end(); }
    const unsigned char& operator[](unsigned int pos) const { return hash.begin()[pos]; }

    //! Comparisons
    friend bool operator==(const CoinbaseDestination& lhs, const CoinbaseDestination& rhs) { return lhs.type == rhs.type && lhs.hash == rhs.hash && lhs.height == rhs.height && lhs.hardcoded == rhs.hardcoded; }

    //! Validations
    bool is_valid() const;

    //! Conversions
    std::string address() const;
    CTxDestination destination() const;

    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        unsigned int t = type;
        READWRITE(t);
        READWRITE(hash);
        READWRITE(height);
        READWRITE(hardcoded);
    }

private:
    ECoinbaseDestinationType type;
    uint160 hash;
    int height;
    bool hardcoded;
};

#endif // PAICOIN_COINBASE_DESTINATION_H
