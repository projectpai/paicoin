// Copyright (c) 2018 The PAIcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "coinbase_keyhandler.h"
#include "coinbase_utils.h"
#include "key.h"
#include "pubkey.h"
#include "utilstrencodings.h"

#include <vector>


// very inefficient for now: it loads all secret keys and just gets the first one
CKey CoinbaseIndexKeyHandler::GetSigningKey()
{
    fs::path secKeyFilePath = m_dataDir / "coinbase" / "seckeys";
    auto secKeys = LoadSecretKeys(secKeyFilePath);
    if (secKeys.empty()) {
        CoinbaseIndexLog("%s: No secret/private keys found in data dir.", __FUNCTION__);
        return CKey();
    }

    return secKeys[0];
}

std::vector<CPubKey> CoinbaseIndexKeyHandler::GetPublicKeys()
{
    // We first verify for secret keys. If present,
    // we completely bypass the possibly present public keys,
    // as they might be out of date
    fs::path secKeyFilePath = m_dataDir / "coinbase" / "seckeys";
    auto secKeys = LoadSecretKeys(secKeyFilePath);
    if (!secKeys.empty()) {
        return DerivePublicKeys(secKeys);
    }

    fs::path pubKeyFilePath = m_dataDir / "coinbase" / "pubkeys";
    return LoadPublicKeys(pubKeyFilePath);
}

std::vector<CKey> CoinbaseIndexKeyHandler::LoadSecretKeys(fs::path const& filePath)
{
    auto vKeys = LoadKeysFromFile(filePath);

    std::vector<CKey> secKeys;
    for (auto const& vKey : vKeys) {
        CKey secKey;
        secKey.Set(vKey.cbegin(), vKey.cend(), false);
        if (!secKey.IsValid()) {
            continue;
        }

        secKeys.emplace_back(std::move(secKey));
    }

    return secKeys;
}

std::vector<CPubKey> CoinbaseIndexKeyHandler::LoadPublicKeys(fs::path const& filePath)
{
    auto vKeys = LoadKeysFromFile(filePath);

    std::vector<CPubKey> pubKeys;
    for (auto const& vKey : vKeys) {
        CPubKey pubKey;
        pubKey.Set(vKey.cbegin(), vKey.cend());
        if (!pubKey.IsFullyValid()) {
            continue;
        }

        pubKeys.emplace_back(std::move(pubKey));
    }

    return pubKeys;
}

std::vector<CPubKey> CoinbaseIndexKeyHandler::DerivePublicKeys(std::vector<CKey> const& secKeys)
{
    std::vector<CPubKey> pubKeys;
    pubKeys.reserve(secKeys.size());

    for (auto const& secKey : secKeys) {
        pubKeys.emplace_back(secKey.GetPubKey());
    }

    return pubKeys;
}

// The keys in the file will be represented as one key per line, hex encoded
std::vector<std::vector<unsigned char, secure_allocator<unsigned char>>> CoinbaseIndexKeyHandler::LoadKeysFromFile(fs::path const& filePath)
{
    std::vector<std::vector<unsigned char, secure_allocator<unsigned char>>> keysInFile;
    if (!fs::exists(filePath)) {
        return keysInFile;
    }

    std::vector<char, secure_allocator<char>> keyHexChars;
    keyHexChars.resize(201, 0);

    std::ifstream inputFile(filePath.string(), std::ios_base::in);
    while (!inputFile.eof() && inputFile.good()) {
        inputFile.getline(keyHexChars.data(), 200, '\n');
        if (!inputFile.good()) {
            break;
        }

        SecureString keyHexCharsStr(keyHexChars.data());
        if (keyHexCharsStr.empty()) {
            continue;
        }
        // zero out to not keep eventual secret keys in static memory
        std::fill(keyHexChars.begin(), keyHexChars.end(), 0);

        auto parsedBytes = ParseHex(keyHexCharsStr.c_str());
        std::vector<unsigned char, secure_allocator<unsigned char>> keyBytes(parsedBytes.cbegin(), parsedBytes.cend());
        if (keyBytes.empty()) {
            continue;
        }

        // we need to make sure to overwrite the existing parsed bytes
        std::fill(parsedBytes.begin(), parsedBytes.end(), 0);

        keysInFile.emplace_back(std::move(keyBytes));
    }

    return keysInFile;
}
