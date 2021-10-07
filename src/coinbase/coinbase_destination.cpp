/* * Copyright (c) 2021 Project PAI Foundation
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include <coinbase/coinbase_destination.h>

#include <compressor.h>
#include <key_io.h>

CoinbaseDestination::CoinbaseDestination()
    : type(ECoinbaseDestinationType::Unknown), height(-1), hardcoded(false)
{
}

CoinbaseDestination::CoinbaseDestination(const std::string& address, const int expiration_height, const bool hardcoded)
    : CoinbaseDestination(DecodeDestination(address), expiration_height, hardcoded)
{
}

CoinbaseDestination::CoinbaseDestination(const ECoinbaseDestinationType type, const  uint160& hash, const int expiration_height, const bool hardcoded)
    : type(type), hash(hash), height(expiration_height), hardcoded(hardcoded)
{
    if (!is_valid())
        throw std::invalid_argument("");
}

CoinbaseDestination::CoinbaseDestination(const CKeyID& keyId, const int expiration_height, const bool hardcoded)
    : type(ECoinbaseDestinationType::PubKeyHash), hash(keyId), height(expiration_height), hardcoded(hardcoded)
{
    if (!is_valid())
        throw std::invalid_argument("");
}

CoinbaseDestination::CoinbaseDestination(const CScriptID& scriptId, const int expiration_height, const bool hardcoded)
    : type(ECoinbaseDestinationType::ScriptHash), hash(scriptId), height(expiration_height), hardcoded(hardcoded)
{
    if (!is_valid())
        throw std::invalid_argument("");
}

CoinbaseDestination::CoinbaseDestination(const CTxDestination& destination, const int expiration_height, const bool hardcoded)
    : type(ECoinbaseDestinationType::Unknown), height(expiration_height), hardcoded(hardcoded)
{
    switch (destination.which()) {
    case 1: {
        type = ECoinbaseDestinationType::PubKeyHash;
        hash =  boost::get<const CKeyID>(destination);
        break;
    }
    case 2: {
        type = ECoinbaseDestinationType::ScriptHash;
        hash = boost::get<const CScriptID>(destination);
        break;
    }
    }

    if (!is_valid())
        throw std::invalid_argument("");
}

bool CoinbaseDestination::is_valid() const
{
    return (type == ECoinbaseDestinationType::PubKeyHash || type == ECoinbaseDestinationType::ScriptHash) && hash != uint160();
}

std::string CoinbaseDestination::address() const
{
    const CTxDestination& dest = destination();

    if (dest.which() == 0)
        return "";

    return EncodeDestination(dest);
}

CTxDestination CoinbaseDestination::destination() const
{
    if (hash == uint160())
        return CNoDestination();

    if (type == ECoinbaseDestinationType::PubKeyHash)
        return CKeyID(hash);

    if (type == ECoinbaseDestinationType::ScriptHash)
        return CScriptID(hash);

    return CNoDestination();
}
