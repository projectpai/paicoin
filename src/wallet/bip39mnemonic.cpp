// Copyright (c) 2018 ObEN, Inc.
// Copyright (c) 2015 breadwallet LLC
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bip39mnemonic.h"
#include <assert.h>
#include <string.h>
#include "../support/cleanse.h"
#include "../crypto/common.h"
#include "../crypto/sha256.h"
#include "../crypto/hmac_sha512.h"

#define be32(x) ((((x) & 0xff) << 24) | (((x) & 0xff00) << 8) | (((x) & 0xff0000) >> 8) | (((x) & 0xff000000) >> 24))

using namespace std;

BIP39Mnemonic::BIP39Mnemonic()
{
    language = BIP39_ENGLISH;
}

BIP39Mnemonic::~BIP39Mnemonic()
{
}

size_t BIP39Mnemonic::Encode(char *phrase, size_t phraseLen, const uint8_t *data, size_t dataLen)
{
    uint32_t x;
    uint8_t buf[dataLen + 32];
    const char *word;
    size_t i, len = 0;

    assert((BIP39WordList != NULL) && (language < BIP39_LANGUAGES_COUNT));
    assert(data != NULL || dataLen == 0);
    assert(dataLen > 0 && (dataLen % 4) == 0);
    if (! data || (dataLen % 4) != 0) return 0; // data length must be a multiple of 32 bits

    memcpy(buf, data, dataLen);

    // append SHA256 checksum
    CSHA256 sha256;
    sha256.Write((const uint8_t *)data, dataLen);
    sha256.Finalize(&buf[dataLen]);

    for (i = 0; i < dataLen*3/4; i++) {
        x = ReadBE32((const unsigned char *)&buf[i*11/8]);
        word = BIP39WordList[language][(x >> (32 - (11 + ((i*11) % 8)))) % BIP39_WORDLIST_COUNT];
        if (i > 0 && phrase && len < phraseLen) phrase[len] = ' ';
        if (i > 0) len++;
        if (phrase && len < phraseLen) strncpy(&phrase[len], word, phraseLen - len);
        len += strlen(word);
    }

    memory_cleanse(&word, strlen(word));
    memory_cleanse(&x, sizeof(x));
    memory_cleanse(buf, sizeof(buf));
    return (! phrase || len + 1 <= phraseLen) ? len + 1 : 0;
}

size_t BIP39Mnemonic::Decode(uint8_t *data, size_t dataLen, const char *phrase)
{
    uint32_t x, y, count = 0, idx[24], i;
    uint8_t b = 0, hash[32];
    const char *word = phrase;
    size_t r = 0;

    assert((BIP39WordList != NULL) && (language < BIP39_LANGUAGES_COUNT));
    assert(phrase != NULL);

    while (word && *word && count < 24) {
        for (i = 0, idx[count] = INT32_MAX; i < BIP39_WORDLIST_COUNT; i++) { // not fast, but simple and correct
            if (strncmp(word, BIP39WordList[language][i], strlen(BIP39WordList[language][i])) != 0 ||
                (word[strlen(BIP39WordList[language][i])] != ' ' && word[strlen(BIP39WordList[language][i])] != '\0')) continue;
            idx[count] = i;
            break;
        }

        if (idx[count] == INT32_MAX) break; // phrase contains unknown word
        count++;
        word = strchr(word, ' ');
        if (word) word++;
    }

    if ((count % 3) == 0 && (! word || *word == '\0')) { // check that phrase has correct number of words
        uint8_t buf[(count*11 + 7)/8];

        for (i = 0; i < (count*11 + 7)/8; i++) {
            x = idx[i*8/11];
            y = (i*8/11 + 1 < count) ? idx[i*8/11 + 1] : 0;
            b = ((x*BIP39_WORDLIST_COUNT + y) >> ((i*8/11 + 2)*11 - (i + 1)*8)) & 0xff;
            buf[i] = b;
        }

        CSHA256 sha256;
        sha256.Write((const uint8_t *)buf, count*4/3);
        sha256.Finalize(hash);

        if (b >> (8 - count/3) == (hash[0] >> (8 - count/3))) { // verify checksum
            r = count*4/3;
            if (data && r <= dataLen) memcpy(data, buf, r);
        }

        memory_cleanse(buf, sizeof(buf));
    }

    memory_cleanse(&b, sizeof(b));
    memory_cleanse(&x, sizeof(x));
    memory_cleanse(&y, sizeof(y));
    memory_cleanse(idx, sizeof(idx));
    return (! data || r <= dataLen) ? r : 0;
}

int BIP39Mnemonic::PhraseIsValid(const char *phrase)
{
    assert((BIP39WordList != NULL) && (language < BIP39_LANGUAGES_COUNT));
    assert(phrase != NULL);
    return (Decode(NULL, 0, phrase) > 0);
}

void BIP39Mnemonic::DeriveKey(void *key64, const char *phrase, const char *passphrase)
{
    char salt[strlen("mnemonic") + (passphrase ? strlen(passphrase) : 0) + 1];

    assert(key64 != NULL);
    assert(phrase != NULL);

    if (phrase) {
        strcpy(salt, "mnemonic");
        if (passphrase) strcpy(salt + strlen("mnemonic"), passphrase);
        PBKDF2(key64, phrase, strlen(phrase), salt, strlen(salt));
        memory_cleanse(salt, sizeof(salt));
    }
}

bool BIP39Mnemonic::WordIsValid(const char* word)
{
    bool result = false;
    size_t len = 0;
    for (int i = 0; i < BIP39_WORDLIST_COUNT; ++i)
    {
        len = strlen(BIP39WordList[language][i]);
        if (strlen(word) == len && strncmp(word, BIP39WordList[language][i], len) == 0)
        {
            result = true;
            break;
        }
    }
    return result;
}

void BIP39Mnemonic::PBKDF2(void *dk, const void *pw, size_t pwLen, const void *salt, size_t saltLen)
{
    size_t dkLen = 64;
    size_t hashLen = 512 / 8;
    unsigned rounds = 2048;

    // dk = T1 || T2 || ... || Tdklen/hlen
    // Ti = U1 xor U2 xor ... xor Urounds
    // U1 = hmac_hash(pw, salt || be32(i))
    // U2 = hmac_hash(pw, U1)
    // ...
    // Urounds = hmac_hash(pw, Urounds-1)

    uint8_t s[saltLen + sizeof(uint32_t)];
    uint32_t i, j, U[hashLen/sizeof(uint32_t)], T[hashLen/sizeof(uint32_t)];

    assert(dk != NULL || dkLen == 0);
    assert(hashLen > 0 && (hashLen % 4) == 0);
    assert(pw != NULL || pwLen == 0);
    assert(salt != NULL || saltLen == 0);
    assert(rounds > 0);

    memcpy(s, salt, saltLen);

    for (i = 0; i < (dkLen + hashLen - 1)/hashLen; i++) {
        j = be32(i + 1);
        memcpy(s + saltLen, &j, sizeof(j));

        // U1 = hmac_hash(pw, salt || be32(i))
        CHMAC_SHA512 hmacSha512s((const unsigned char*)pw, pwLen);
        hmacSha512s.Write(s, sizeof(s));
        hmacSha512s.Finalize((unsigned char*)U);
        memcpy(T, U, sizeof(U));

        for (unsigned r = 1; r < rounds; r++) {
            // Urounds = hmac_hash(pw, Urounds-1)
            CHMAC_SHA512 hmacSha512u((const unsigned char*)pw, pwLen);
            hmacSha512u.Write((const unsigned char*)U, sizeof(U));
            hmacSha512u.Finalize((unsigned char*)U);
            for (j = 0; j < hashLen/sizeof(uint32_t); j++) T[j] ^= U[j]; // Ti = U1 ^ U2 ^ ... ^ Urounds
        }

        // dk = T1 || T2 || ... || Tdklen/hlen
        memcpy((uint8_t *)dk + i*hashLen, T, (i*hashLen + hashLen <= dkLen) ? hashLen : dkLen % hashLen);
    }

    memory_cleanse(s, sizeof(s));
    memory_cleanse(U, sizeof(U));
    memory_cleanse(T, sizeof(T));
}
