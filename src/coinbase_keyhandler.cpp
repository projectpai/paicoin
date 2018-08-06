#include "coinbase_keyhandler.h"
#include "key.h"
#include "pubkey.h"
#include "utilstrencodings.h"

#include <vector>


// very inefficient for now: it loads all secret keys and just gets the first one
CKey CoinbaseKeyHandler::GetCoinbaseSigningKey()
{
    fs::path secKeyFilePath = m_dataDir / "coinbase" / "seckeys";
    auto secKeys = LoadSecretKeys(secKeyFilePath);
    if (secKeys.empty()) {
        return CKey();
    }

    return secKeys[0];
}

std::vector<CPubKey> CoinbaseKeyHandler::GetCoinbasePublicKeys()
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

std::vector<CKey> CoinbaseKeyHandler::LoadSecretKeys(fs::path const& filePath)
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

std::vector<CPubKey> CoinbaseKeyHandler::LoadPublicKeys(fs::path const& filePath)
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

std::vector<CPubKey> CoinbaseKeyHandler::DerivePublicKeys(std::vector<CKey> const& secKeys)
{
    std::vector<CPubKey> pubKeys;
    pubKeys.reserve(secKeys.size());

    for (auto const& secKey : secKeys) {
        pubKeys.emplace_back(secKey.GetPubKey());
    }

    return pubKeys;
}

// The keys in the file will be represented as one key per line, hex encoded
std::vector<std::vector<unsigned char, secure_allocator<unsigned char>>> CoinbaseKeyHandler::LoadKeysFromFile(fs::path const& filePath)
{
    std::vector<std::vector<unsigned char, secure_allocator<unsigned char>>> keysInFile;
    if (!fs::exists(filePath)) {
        return keysInFile;
    }

    char keyHexChars[201] = {0};
    std::ifstream inputFile(filePath.string(), std::ios_base::in);
    while (!inputFile.eof() && inputFile.good()) {
        inputFile.getline(keyHexChars, 200, '\n');
        if (!inputFile.good()) {
            break;
        }

        std::string keyHexCharsStr(keyHexChars);
        if (keyHexCharsStr.empty()) {
            continue;
        }

        auto parsedBytes = ParseHex(keyHexCharsStr);
        std::vector<unsigned char, secure_allocator<unsigned char>> keyBytes(parsedBytes.cbegin(), parsedBytes.cend());
        if (keyBytes.empty()) {
            continue;
        }

        // zero out to not keep eventual secret keys in static memory
        memset(keyHexChars, 0, 201);

        // we need to make sure to overwrite the existing parsed bytes
        for (auto& pb : parsedBytes) {
            pb = 0xFF; // random value
        }

        keysInFile.emplace_back(std::move(keyBytes));
    }

    return keysInFile;
}
