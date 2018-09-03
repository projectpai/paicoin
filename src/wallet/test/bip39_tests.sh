#!/bin/bash

# Copyright (c) 2018 ObEN, Inc.
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

echo 
echo ================
echo Building test...

#gcc -Isecp256k1 -DBITCOIN_TESTNET -o test-createmultisigaddress test-createmultisigaddress.c BRAddress.c BRBase58.c BRCrypto.c BRKey.c
g++ -I../../ -I/usr/local/Cellar/berkeley-db@4/4.8.30/include -I/usr/local/Cellar/openssl/1.0.2o_1/include -L/usr/local/Cellar/openssl/1.0.2o_1/lib -lcrypto -std=c++11 -o bip39_test bip39_tests.cpp ../bip39mnemonic.cpp ../../support/cleanse.cpp ../../crypto/sha256.cpp ../../crypto/sha512.cpp ../../crypto/hmac_sha512.cpp

echo ...done

echo 
echo ===============
echo Running test...

./bip39_test

echo 
echo ================
echo Test completed.
echo 
