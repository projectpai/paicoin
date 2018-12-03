// Copyright (c) 2018 The PAIcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAICOIN_COINBASE_ADDRESS_H
#define PAICOIN_COINBASE_ADDRESS_H

#include "pubkey.h"
#include "serialize.h"

#include <string>

/**
 * Represents a coinbase address against which we can create
 * a coinbase transaction.
 */
class CoinbaseAddress
{
public:
    //! The hashed address which is usually represented as a hex-string
    std::string m_hashedAddr;
    CPubKey m_pubKey;

    /** Represents whether this coinbase address is included by default
     * in all distributions of PAIcoin and known to all the nodes.
     */
    bool m_isDefault;
    int m_expirationBlockHeight;

    friend bool operator==(const CoinbaseAddress& r1, const CoinbaseAddress& r2)
    {
        return (r1.m_hashedAddr == r2.m_hashedAddr) &&
               (r1.m_pubKey == r2.m_pubKey);
    }

    const std::string& GetHashedAddr() const { return m_hashedAddr; }
    bool IsDefault() const { return m_isDefault; }
    int GetExpirationHeight() const { return m_expirationBlockHeight; }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(m_hashedAddr);
        READWRITE(m_pubKey);
        READWRITE(m_isDefault);
        READWRITE(m_expirationBlockHeight);
    }
};

#endif // PAICOIN_COINBASE_ADDRESS_H
