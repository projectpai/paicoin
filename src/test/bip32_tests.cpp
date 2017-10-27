// Copyright (c) 2013-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include "base58.h"
#include "key.h"
#include "uint256.h"
#include "util.h"
#include "utilstrencodings.h"
#include "test/test_paicoin.h"

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
 * For updating the following test vectors:
 * - clone bip32utils from https://github.com/prusnak/bip32utils.git;
 * - in BIP32Key.py (lines 26-29), update the EX_MAIN_PRIVATE, EX_MAIN_PUBLIC, EX_TEST_PRIVATE and EX_TEST_PUBLIC with their corresponding base58Prefixes from chainparams.cpp;
 * - in BIP32Key.py (line 43), change the hmac seed to "PAIcoin seed";
 * - in BIP32Key.py (at the end of file) add the following lines:

# BIP0032 Test vector 3
    entropy = '4b381541583be4423346c643850da4b320e46a87ae3d2a4e6da11eba819cd4acba45d239319ac14f863b8d5ab5a0d0c64d2e8a1e7d1457df2e5a3c51c73235be'.decode('hex')
    m = BIP32Key.fromEntropy(entropy)
    print("Test vector 3:")
    print("Master (hex):", entropy.encode('hex'))
    print("* [Chain m]")
    m.dump()

    print("* [Chain m/0]")
    m = m.ChildKey(0)
    m.dump()

 * - in bip0032-vectors.sh (at the end of file) add the following lines:
 # BIP0032 Test vector #3

echo Generating BIP0032 test vector 3:
echo 4b381541583be4423346c643850da4b320e46a87ae3d2a4e6da11eba819cd4acba45d239319ac14f863b8d5ab5a0d0c64d2e8a1e7d1457df2e5a3c51c73235be | \
    ../bip32gen -v \
    -i entropy -f - -x -n 512 \
    -o privkey,wif,pubkey,addr,xprv,xpub -F - -X \
    m \
    m/0

 * - run the bip0032-vectors.sh script with the following command line:

 ./bip0032-vectors.sh > bip0032-vectors.out

  * - copy the public and private keys from the bip0032-vectors.out file into the three vectors below (test1, test2 and test3).
  * - run make check
 */

TestVector test1 =
  TestVector("000102030405060708090a0b0c0d0e0f")
    ("4Hpp25sKpXYN8GWRbPSpdoTP9TeJ4cuCej4JNKJSjLroyktJ2LDZ6mDrrgTg5FWLE3FizzYhQsabWQ7mgMku9jpP8H3we8HwCTnCJ9FsN6AhGUEW",
     "4HqG4V6QyNQ3pBdQvjC9CtH9aTKg9Uiam2zdqKbwZQvqB9mMTvyPMSTRJ5TLBHGzQxzC4jYnv4acxvWog9Dmu8YyCDWh9azfsCrGWiwPtJoP6qhb",
     0x80000000)
    ("4Hpp25vsen4jV1xrqnuRdVGxeNNwXreGw5YTur5rd5XgSraH6UX314bJ72wdGXuWaKDDqH67hcmBLT9sZhJRMZW7afEypPocywYv9x6HFHsqm56W",
     "4HqG4V9xocvRAw5rB8ekCa6j5N4KciTf3PUoNrPMT9bheFTLY5GsFjprYRwHNZgPDABsiTu7KiT9pBuzeS5wKHty5wRpsewGm8bGdhvJNXB1sm4X",
     1)
    ("4Hpp25xe6HB89c21iu4NSK6Wnjc2mZ5a7J9thdp3a88J7aCDwoss5ZZwuijQE7oVrocySpawpnphKjenk9F6dVfwfG9qkmuwjdJj6LFmKGJ3K1JQ",
     "4HqG4VBjF82oqX914Eoh1PvHDjHQrQtxDc6EAe7YQCCKJy5HPQdhLEoWM7j4L9aEhBUnmwQJc9zdR9xnyif7D4MWjuKuL5Q4iKRmxpEqSSQobkFT",
     0x80000002)
    ("4Hpp25y4NSzpzPsgXziSvZZ9thFc4dhxh8jPWP4McNzLpPGLHMtSuhr9ovQNCPNm7bws6mNyrfmn4GitRfmSFFiSbNAUEc4r1uSpZrKeAQEg2hoY",
     "4HqG4VC9XHrWgJzfsLTmVeNvKgvz9VXLoSfiyPMrST4N1n9PixeHAP5iFKQ2JR9PGRpnZ7aM4BxCjzGYNvWfGfysaPFYsJpFa8RtZQWaw5eKoxHD",
     2)
    ("4Hpp2613ioycyytvwQTV46XMwxaUt4toAarjUved6k71QRg1vUvzw1NkM6sFzk78cNMUUR2ePM8pzfDYZUGbFvLcBkkHVpcM9ncuZGmbQWoYDMVS",
     "4HqG4VE8seqJfu1vGkCodBM8NxFrxviBGto4wvx7vpB2bpZ5N5gqBgcJnVrv6mqSFDq9s5CzqS8vPzMmAnzafyRBXkeoudqmSnbpK5dz2b9qvEkS",
     1000000000)
    ("4Hpp262jajzDe9q8i3bRRDLCs9W8b9RoJpQ2GJLHNFZdLRN2ki7trn8X37NvJLtX8pGpNP5wh85GttE5Z8obiaw41df2xznXMzd4yJ2VdGPfAbAv",
     "4HqG4VFpjaquL4x83PLjzJ9yJ9BWg1FBR8LMjJdnCKdeXpF6CJsj7TN5UWNaQNeuUWooUHzEHkJxiH7gwmRpeAcdqYdVAKgLJY18GD5fktLpAsHP",
     0);

TestVector test2 =
  TestVector("fffcf9f6f3f0edeae7e4e1dedbd8d5d2cfccc9c6c3c0bdbab7b4b1aeaba8a5a29f9c999693908d8a8784817e7b7875726f6c696663605d5a5754514e4b484542")
    ("4Hpp25sKpXYN8GWRbMMXBzMgsjoicCSWsqjZ4pzWgPoBiWaCWurW4w3tBAiNR1BeY56DFhYp6rYrXuJuj6QkxXDYN9AUBAdJVyXwvKs6j1LJ6jCv",
     "4HqG4V6QyNQ3pBdQvh6qm5BTJjV6h4Ftz9ftXqJ1WTsCuuTFxWcLKcHScZi2X2yANqmfcWsM9UBEWUiB7u3WNQXCJc3FS6G3y5g2qfbsxydvKv9W",
     0)
    ("4Hpp25vAgWpwhyK4vVpgDwPDiHUmiLWAmSsSNgNLb6ZPHMhRVoX1oBKSg5UMb5wqJZ3rvjwKPtPn9JPkycCMuKWkzfBXaQkHW6JdnLgc9gFGBnn4",
     "4HqG4V9FqMgdPtS4FqZzo2Cz9HA9oCKYskomqgfqRAdQUkaUwQGr3rZ17UU1h7gTNcqqpvfSauMuJ1mNvhaU7FRsJyvAkJ6hnoJFC5W6xxefbSQs",
     0xFFFFFFFF)
    ("4Hpp25xC2GQqcvUxXiHi7Vxx4YrfW3dGd7798PPi1rMfngTzcAJLsiWfuKa8XbpBEYKSvVhRdyHYVfJrJZ21LkiCLHDqvaiscHkbLyeQBdrMjig2",
     "4HqG4VBHB7GXJqbws432ganiVYY3auSejR3UbPhCqvRgz5M43m4B8PkELiZnddYAo6WwL9orbXay3tN1SY7x6vnAbDQrUHNt3BxUp18pxx9jvYct",
     1)
    ("4Hpp25zRb6iHE3zGtY9NvThhvRjF2iz3dGWv1m15hDDNSqsGBNgCL6ugvHnKnBvvjEJFzaJE5bJzKsxav9ZBUPxLprh69WzNCdJ94ZFKWMyM8sMf",
     "4HqG4VDWjwZxuy7GDsthVYXUMRQd7aoRjaTFUmJaXHHPeEkKcyS2an9FMgmytDgQ7TBtt7bbLsBoZX8ejagrNRdJazcDT9gWFFaaRCLuLUXWcFpQ",
     0xFFFFFFFE)
    ("4Hpp2629ekiBLKnPwQmLqhoXNRtizXQnFt9ubQFwi5wcqnX7DeF61UYYL3HbRiZECiEwHaemMk9LCoumnHqDnb4dkNRFzPFFxv5AcRWhwZBwzv9g",
     "4HqG4VFEobZs2EuPGkWfQndHoRa75PEANC6F4QZSYA1e3BQAfEzvG9n6mSHFXkGyaB6JtAt6hP1SMqwE1EKyVeNVAywb5V9QWcb7rHS48inmX6z2",
     2)
    ("4Hpp2632Mo3oGsYv6K9QFYUvTA6ksQt4mfSt46pFwPzEyQPf9EaKLJkmbpNVKATkaEmX8Jb5F58Tg7HoKLKZ6npNAvePtaBLpYYR1XecDkESDaUd",
     "4HqG4VG7WduUxnfuRetipdJgt9n8xGhSsyPDX77kmU4GAoGiaqL9ayzL3DN9RCB4S9gHBcNUchYheVK98qrHDhJL278DempbkqUeXizpTqPxEr5z",
     0);

TestVector test3 =
  TestVector("4b381541583be4423346c643850da4b320e46a87ae3d2a4e6da11eba819cd4acba45d239319ac14f863b8d5ab5a0d0c64d2e8a1e7d1457df2e5a3c51c73235be")
    ("4Hpp25sKpXYN8GWRbPsbLZcFPVfgms6CQgC5mwiW5BBnEXaGb5i97A5ndrzXsCE7LUjJF1mBDyEGL5hP6AgZDaoQZdHP5Yr9wk4fygyNq2CBMW55",
     "4HqG4V6QyNQ3pBdQvjcuueS1pVM4riuaWz8REx1zuFFoRvTL2gTyMqKM5FzByDxjz17PiRd6m99peaXj5YnDktJX4wRAFC4eqxsHozUXN9iupfVU",
      0x80000000)
    ("4Hpp25v8PgSX9W2BGzXceaxRxmSF37GQdWdfhcxCdWpJ8qps515o1jze6Jv7EHuEHJWizhGh43VconvjsJtLivKyFAGdjuPadKMrHWLB4yKZxatv",
     "4HqG4V9DYXJCqR9AcLGwDfnCPm7d7y5njpa1AdFhTatKLEhvWbqdGRECXhumLKh3TNwcSREZvMFoU6YN6ENMZ67M5vL93EMtgh2JyJYCYNMnpURz",
      0);

void RunTest(const TestVector &test) {
    std::vector<unsigned char> seed = ParseHex(test.strHexMaster);
    CExtKey key;
    CExtPubKey pubkey;
    key.SetMaster(&seed[0], seed.size());
    pubkey = key.Neuter();
    for (const TestDerivation &derive : test.vDerive) {
        unsigned char data[74];
        key.Encode(data);
        pubkey.Encode(data);

        // Test private key
        CPAIcoinExtKey b58key; b58key.SetKey(key);
        BOOST_CHECK(b58key.ToString() == derive.prv);

        CPAIcoinExtKey b58keyDecodeCheck(derive.prv);
        CExtKey checkKey = b58keyDecodeCheck.GetKey();
        assert(checkKey == key); //ensure a base58 decoded key also matches

        // Test public key
        CPAIcoinExtPubKey b58pubkey; b58pubkey.SetKey(pubkey);
        BOOST_CHECK(b58pubkey.ToString() == derive.pub);

        CPAIcoinExtPubKey b58PubkeyDecodeCheck(derive.pub);
        CExtPubKey checkPubKey = b58PubkeyDecodeCheck.GetKey();
        assert(checkPubKey == pubkey); //ensure a base58 decoded pubkey also matches

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
