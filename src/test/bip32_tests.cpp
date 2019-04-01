// Copyright (c) 2013-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <key.h>
#include <key_io.h>
#include <uint256.h>
#include <util.h>
#include <utilstrencodings.h>
#include <test/test_paicoin.h>

#include <string>
#include <vector>

struct TestDerivation {
    std::string pub;
    std::string prv;
    unsigned int nChild;
};

struct TestVector {
    std::string strHexMaster;
    std::vector<TestDerivation> vDerive;

    explicit TestVector(std::string strHexMasterIn) : strHexMaster(strHexMasterIn) {}

    TestVector& operator()(std::string pub, std::string prv, unsigned int nChild) {
        vDerive.push_back(TestDerivation());
        TestDerivation &der = vDerive.back();
        der.pub = pub;
        der.prv = prv;
        der.nChild = nChild;
        return *this;
    }
};

/**
 * PAICOIN Note: if unit test updating is required
 * For updating the following test vectors:
 * - clone bip32utils from https://github.com/prusnak/bip32utils.git;
 * - in BIP32Key.py (lines 26-29), update the EX_MAIN_PRIVATE, EX_MAIN_PUBLIC, EX_TEST_PRIVATE and EX_TEST_PUBLIC with their corresponding base58Prefixes from chainparams.cpp;
 * - in BIP32Key.py (line 43), change the hmac seed to "PAI Coin seed";
 * - in BIP32Key.py (at the end of file) add the following lines:

    # BIP0032 Test vector 3
    entropy = '4b381541583be4423346c643850da4b320e46a87ae3d2a4e6da11eba819cd4acba45d239319ac14f863b8d5ab5a0d0c64d2e8a1e7d1457df2e5a3c51c73235be'.decode('hex')
    m = BIP32Key.fromEntropy(entropy)
    print("Test vector 3:")
    print("Master (hex):", entropy.encode('hex'))
    print("* [Chain m]")
    m.dump()

    print("* [Chain m/0]")
    m = m.ChildKey(0+BIP32_HARDEN)
    m.dump()

 * - in bip0032-vectors.sh (at the end of file) add the following lines:
 # BIP0032 Test vector #3

echo Generating BIP0032 test vector 3:
echo 4b381541583be4423346c643850da4b320e46a87ae3d2a4e6da11eba819cd4acba45d239319ac14f863b8d5ab5a0d0c64d2e8a1e7d1457df2e5a3c51c73235be | \
    ../bip32gen -v \
    -i entropy -f - -x -n 512 \
    -o privkey,wif,pubkey,addr,xprv,xpub -F - -X \
    m \
    m/0h

 * - run the bip0032-vectors.sh script with the following command line:

 ./bip0032-vectors.sh > bip0032-vectors.out

  * - copy the public and private keys from the bip0032-vectors.out file into the three vectors below (test1, test2 and test3).
  * - run make check
 */

TestVector test1 =
  TestVector("000102030405060708090a0b0c0d0e0f")
    ("paip6Nt6Wry8bUrjUJgwaB5mfZAobHkoMeic9HwtM3EZP6pG6QJLkjX8wc821h4BidygTyxU22iDwqvkcQdNeJ27i7fMkERLSixhWiaXQniWwPQ",
     "paiv7CLsatDqdaCRhYkYcLaRJw8kK4KT8CJvtgKUiZoRYPycRK8UGamMWniBfmvJsbC2ZrHCFZewWpbCS34MqSH9p8SfdXpM2LGBBeUyDMdQvd4",
     0x80000000)
    ("paip6SRvmPLVLwHysmHwFzfGaHpGq2q5i8t9g5Mn5i72UnoLEhnF46xPJ65DJ6EXzbUWkXNkmDJ3zt2dwx9aTyka6JhY1k77vVgZKYzQcXkw1oT",
     "paiv7FthqQbCP2dg71MYJA9vDfnDYoPjUgUURTjNTEfte5xgZccNZxCbsGfNxBK74nsgJCbbuSBnnDnAiuDmznH3Y3aPhUREx5GJAdPTRf1aNbC",
     1)
    ("paip6UCNGVj9vzSryvEk5pDQwWuWXU8FvkJwToYj8JihCQkBa4cKZ5cBysrAszDpV1E8J2CswGp3HNwpPtprQ9aehDZUPrRscFVVhiUUazt2qMo",
     "paiv7Hf9LWyry5nZDAJM7yi4atsTFEguhHuGDBvKVqHZMhuXtyST4vrQZ4SLY5Ab65njmhntLyfPkGaW1UPfmEphVwer7wDC8umdGwvXLyedDLi",
     0x80000002)
    ("paip6UceSKRzir7g5aKELGrWuAUoc6WqmKokD3rmPAmQ1UrX85C9hMp6BYp99ZV5HL7nEpEupDtmpT3VvRAUAC5aoEBxE1L9tPayDnMKitLPe6p",
     "paiv7J5RWLghkwTNJpNqNSMAYYSkKs5VXsQ4xSEMkhLGAn1sSz2HDD4JkjQJoeKALRnWwsqLNwEiaaKuDKwjNsBXysJPMMQ3wutDsDg1zB99t83",
     2)
    ("paip6WbzoJDzJsN5VKMMsF4aAVMd3HMKDT9ike8FkHRz3tYAF7kAztQdN1hwWHra3jj9tTuSVawiCwhdivKUppFBBp1DSYqHmZfxeEJZqWBC2Zx",
     "paiv7L4msKUhLxhmiZQxuQZDosKZm3uxyzk3W2Vr7ozrDBhWa2aJWjeqwCJ7ALN98S9puWV7d7xNafYh5os8gJVVMGZRgNuvc5oyYM57VmjD7n2",
     1000000000)
    ("paip6YHrjJpeUoZr8THiz3uVMR1L7pMTSzSW8KnXFk3v3aYzUJe6meBKNXNF75F6Vf53rXCkGXPcRxEdPTKwVQh14ikgcj1VyZqNfVCnb2gnJvT",
     "paiv7MkdoL5MWtuYMhML2DQ8znyGqav7DY2psiA7dGcnCsiLoDUEHVRXwhxQm9qNRQoS8HiZwHzgsRUU4F76sVwo9FEgNDUnMV7vfnkqns2eytj",
     0);

TestVector test2 =
  TestVector("fffcf9f6f3f0edeae7e4e1dedbd8d5d2cfccc9c6c3c0bdbab7b4b1aeaba8a5a29f9c999693908d8a8784817e7b7875726f6c696663605d5a5754514e4b484542")
    ("paip6Nt6Wry8bUrjSDPVm5PVwibMAq52UKyJez1qPycJ8nikg3FJvZYTRrpMmNNVkUTwAz59zzyFT34oM4VBRhBMaEBtnZndxUiKhKotKzGXieS",
     "paiv7CLsatDqdaCRfTT6oEt9b6ZHtbdgEsZdQNPRmWBAJ5t6zx5SSQng13QXRU6GkNfaMAqRfAGV51xeBrnq7QWGCezx8oCSuA2W8Jy3tApQKnS",
     0)
    ("paip6RixW9YiJHW4agYXi6vLVPeTJtiv5TrcWMqk6joryuwjZhm3Aq6xLcoXr8ZGES7cDNaT2qtrr7v3rr68DzPz6FFJ2gme5FQBi9KJzpxwiPF",
     "paiv7FBjaAoRLNqkovc8kGQz8mcQ2fHZr1SwFkDLUGNj9D75tcbAggMAuoPhWBPGXSqnkxvs6LwGc5ASzPkZxKBGaXvGLdrGcnErYDC3s6gdWL3",
     0xFFFFFFFF)
    ("paip6TkJFjSdFTPfo9aRGgegkmYF21pmjhZNDPDArY6NJgWqvV67i2LBaiaUMzuCDhhby8gh7jfDD31NofjZfBqKiHZeCfMkGhMkM77LxWV9qnF",
     "paiv7HD5KkhLHYjN2Pe2Jr9LQ9WBjnPRWF9gxmamE4fETygCFPvFDsaQ9uAe236h17wHz7Lsia12UfnxpwEZdfUYp2bzKv2X1SUUTqv3rck5wS9",
     1)
    ("paip6Vys62tENxi2d1FEERQYde7mhNbmu7LFazarDPo2U5nR8rwa6RMCYvmix7egugWg3jV8jm73RgjzQCuhJRypHkos8vrLcEuTvi2fgj6TnXP",
     "paiv7KSeA48wR43irFJqGauCH25iR9ARfevaLNxSavMtdNwmTmmhcGbR87MtcBL1Mntqwu5d4AqY7SSFsW8q8WcYbDxyCDej54a5f3zRP8a252J",
     0xFFFFFFFE)
    ("paip6Xhvk2nLekq5VdD9UXDzdobjVoLQWkKqEFSs683RQjdTQRqFU4CcJS3NUjxAPdBy462QtbSvMdvrYUx1VYGjoUyi1Bk6u1w1nyR6stfeNeu",
     "paiv7MAhp433grAmisGkWgieHBZgDZu4HHv9ydpTTecHa2nojLfNyuSpscdY8muU5hJr1BayZzULSF1XX9FxMFo8aZLbXgYzS57Wk99DdG3ygmQ",
     2)
    ("paip6YadnNQHCXMEQ1GZKCd5N1dcPGcvJ3JHvom6QAfZ2cBNzm4aJGRt5WwFveUXv9mon2LJDaaPf1xPayHKhJ1AMi7cC7pxXVBQu7KP4wgWWyf",
     "paiv7N3QrPezEcgvdFLAMN7j1PbZ73Ba4atcgC8gmhERBuLjKfthp7g6ehXRafzL4HH9SfxttXjd5cvf8fZgQBdyhjyApMkEexeCBhuYjwvLFCe",
     0);

TestVector test3 =
  TestVector("4b381541583be4423346c643850da4b320e46a87ae3d2a4e6da11eba819cd4acba45d239319ac14f863b8d5ab5a0d0c64d2e8a1e7d1457df2e5a3c51c73235be")
    ("paip6Nt6Wry8bUrjUjTeLKx1haZWqUkZJnW1mi1EBNCp9nnpqttM9bSv88yoxQqJA7YvVCSH7gP3dRYARLHSVH3Z4M6oAne5j1SP4S5zLwzGqGX",
     "paiv7CLsatDqdaCRhyXFNVSfLxXTZFKD5L6LX6NpYtmgK5xBAoiUfSh8hKZycTfsuiPgFvb3L8rdAqWbqbWDbBq2Y2umEboKnMHUTBcT4CLC5SA",
      0x80000000)
    ("paip6Rgffm8JAf9PDs8fG4JBKWVsAuHXXDeP3z6WytiAPC9pSjJ5JnXzdGhTB1pscU8KiW9JteqAapWZXTPn9TYUG1UYeH7DRcNjrurFV9jhyy5",
     "paiv7F9SjnP1CkV5T7CGJDnpxtTotfrBHmEhoNU7MRH2YVKAme8CpdnDCTHcq4oakLm5Qg68ZU1KYWKARdkvoM28ToZqE8kRELTGYq8b1MLSEoi",
      0);

void RunTest(const TestVector &test) {
    std::vector<unsigned char> seed = ParseHex(test.strHexMaster);
    CExtKey key;
    CExtPubKey pubkey;
    key.SetMaster(seed.data(), seed.size());
    pubkey = key.Neuter();
    for (const TestDerivation &derive : test.vDerive) {
        unsigned char data[74];
        key.Encode(data);
        pubkey.Encode(data);

        // Test private key
        BOOST_CHECK(EncodeExtKey(key) == derive.prv);
        BOOST_CHECK(DecodeExtKey(derive.prv) == key); //ensure a base58 decoded key also matches

        // Test public key
        BOOST_CHECK(EncodeExtPubKey(pubkey) == derive.pub);
        BOOST_CHECK(DecodeExtPubKey(derive.pub) == pubkey); //ensure a base58 decoded pubkey also matches

        // Derive new keys
        CExtKey keyNew;
        BOOST_CHECK(key.Derive(keyNew, derive.nChild));
        CExtPubKey pubkeyNew = keyNew.Neuter();
        if (!(derive.nChild & 0x80000000)) {
            // Compare with public derivation
            CExtPubKey pubkeyNew2;
            BOOST_CHECK(pubkey.Derive(pubkeyNew2, derive.nChild));
            BOOST_CHECK(pubkeyNew == pubkeyNew2);
        }
        key = keyNew;
        pubkey = pubkeyNew;

        CDataStream ssPub(SER_DISK, CLIENT_VERSION);
        ssPub << pubkeyNew;
        BOOST_CHECK(ssPub.size() == 75);

        CDataStream ssPriv(SER_DISK, CLIENT_VERSION);
        ssPriv << keyNew;
        BOOST_CHECK(ssPriv.size() == 75);

        CExtPubKey pubCheck;
        CExtKey privCheck;
        ssPub >> pubCheck;
        ssPriv >> privCheck;

        BOOST_CHECK(pubCheck == pubkeyNew);
        BOOST_CHECK(privCheck == keyNew);
    }
}

BOOST_FIXTURE_TEST_SUITE(bip32_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(bip32_test1) {
    RunTest(test1);
}

BOOST_AUTO_TEST_CASE(bip32_test2) {
    RunTest(test2);
}

BOOST_AUTO_TEST_CASE(bip32_test3) {
    RunTest(test3);
}

BOOST_AUTO_TEST_SUITE_END()
