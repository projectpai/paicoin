// Copyright (c) 2018 ObEN, Inc.
// Copyright (c) 2015 breadwallet LLC
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// BIP39 is method for generating a deterministic wallet seed from a mnemonic phrase
// https://github.com/bitcoin/bips/blob/master/bip-0039.mediawiki

#ifndef BIP39MNEMONIC_H
#define BIP39MNEMONIC_H

#include <stdlib.h>
#include <stdint.h>
#include "bip39words.h"

#define BIP39_CREATION_TIME  1388534400 // oldest possible BIP39 phrase creation time, seconds after unix epoch

class BIP39Mnemonic final
{
public:
    BIP39Mnemonic();
    ~BIP39Mnemonic();

    // returns number of bytes written to phrase including NULL terminator, or phraseLen needed if phrase is NULL
    size_t Encode(char *phrase, size_t phraseLen, const uint8_t *data, size_t dataLen);

    // returns number of bytes written to data, or dataLen needed if data is NULL
    size_t Decode(uint8_t *data, size_t dataLen, const char *phrase);

    // verifies that all phrase words are contained in wordlist and checksum is valid
    int PhraseIsValid(const char *phrase);

    // key64 must hold 64 bytes (512 bits), phrase and passphrase must be unicode NFKD normalized
    // http://www.unicode.org/reports/tr15/#Norm_Forms
    // BUG: does not currently support passphrases containing NULL characters
    void DeriveKey(void *key64, const char *phrase, const char *passphrase);

private:
    enum BIP39_LANGUAGES language;

    void PBKDF2(void *dk, const void *pw, size_t pwLen, const void *salt, size_t saltLen);
};

#endif // BIP39MNEMONIC_H
