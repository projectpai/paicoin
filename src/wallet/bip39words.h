// Copyright (c) 2018 ObEN, Inc.
// Copyright (c) 2015 breadwallet LLC
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIP39WORDS_H
#define BIP39WORDS_H

#define BIP39_WORDLIST_COUNT 2048       // number of words in a BIP39 wordlist

enum BIP39_LANGUAGES {
    BIP39_ENGLISH           = 0,
    BIP39_LANGUAGES_COUNT
};

extern const char* BIP39WordList[BIP39_LANGUAGES_COUNT][BIP39_WORDLIST_COUNT];

#endif // BIP39WORDS_H
