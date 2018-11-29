// Copyright (c) 2018 The PAIcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAICOIN_COINBASE_KEYHANDLER_H
#define PAICOIN_COINBASE_KEYHANDLER_H

#include "util.h"


class CKey;
class CPubKey;
template <typename T> class vector;
template <typename T> struct secure_allocator;

class CoinbaseKeyHandler
{
public:
    explicit CoinbaseKeyHandler(fs::path dataDir) : m_dataDir(dataDir) {}
    CoinbaseKeyHandler() = delete;

    CKey GetCoinbaseSigningKey();
    std::vector<CPubKey> GetCoinbasePublicKeys();

private:
    std::vector<CKey> LoadSecretKeys(fs::path const& filePath);
    std::vector<CPubKey> LoadPublicKeys(fs::path const& filePath);

    std::vector<CPubKey> DerivePublicKeys(std::vector<CKey> const& secKeys);

    // The keys in the file will be represented as one key per line, hex encoded
    std::vector<std::vector<unsigned char, secure_allocator<unsigned char>>> LoadKeysFromFile(fs::path const& filePath);

private:
    fs::path m_dataDir;
};

#endif // PAICOIN_COINBASE_KEYHANDLER_H
