
#include "coinbase_index.h"
#include "coinbase_addresses.h"
#include "coinbase_keyhandler.h"
#include "coinbase_txhandler.h"
#include "coinbase_utils.h"
#include "wallet/wallet.h"

#include "test/test_paicoin.h"

#include <boost/test/unit_test.hpp>
#include <fstream>

struct RegtestingSetup : public TestingSetup {
    RegtestingSetup() : TestingSetup(CBaseChainParams::REGTEST) {}
};

BOOST_FIXTURE_TEST_SUITE(coinbase_index_tests, RegtestingSetup)

BOOST_AUTO_TEST_CASE(SimpleDefaultCoinbaseAddrs)
{
    CoinbaseIndex cbIndex;
    BOOST_CHECK(cbIndex.IsNull());
    cbIndex.BuildDefault();
    BOOST_CHECK(!cbIndex.IsNull());
    BOOST_CHECK_EQUAL(cbIndex.GetNumCoinbaseAddrs(), fCoinbaseAddrs.size());

    for (auto const& fAddrToHeight : fCoinbaseAddrs) {
        auto const& fAddr = fAddrToHeight.first;
        auto const& height = fAddrToHeight.second;

        auto foundCbAddr = cbIndex.GetCoinbaseWithAddr(fAddr);
        BOOST_CHECK(foundCbAddr);
        BOOST_CHECK_EQUAL(foundCbAddr->GetHashedAddr(), fAddr);
        BOOST_CHECK_EQUAL(foundCbAddr->IsDefault(), true);
        BOOST_CHECK_EQUAL(foundCbAddr->GetExpirationHeight(), height);
    }

    auto foundBogusAddr = cbIndex.GetCoinbaseWithAddr("123");
    BOOST_CHECK(!foundBogusAddr);
}

BOOST_AUTO_TEST_CASE(DefaultCoinbaseAddrsDiskSerialization)
{
    CoinbaseIndex cbIndex;
    BOOST_CHECK(cbIndex.IsNull());
    cbIndex.BuildDefault();
    BOOST_CHECK(!cbIndex.IsNull());

    CoinbaseIndexDisk cbIndexDisk(cbIndex);
    BOOST_CHECK(cbIndexDisk.SaveToDisk());

    cbIndex.SetNull();
    BOOST_CHECK(cbIndex.IsNull());

    BOOST_CHECK(cbIndexDisk.LoadFromDisk());
    BOOST_CHECK(!cbIndex.IsNull());

    for (auto const& fAddrToHeight : fCoinbaseAddrs) {
        auto const& fAddr = fAddrToHeight.first;
        auto const& height = fAddrToHeight.second;

        auto foundCbAddr = cbIndex.GetCoinbaseWithAddr(fAddr);
        BOOST_CHECK(foundCbAddr);
        BOOST_CHECK_EQUAL(foundCbAddr->GetHashedAddr(), fAddr);
        BOOST_CHECK_EQUAL(foundCbAddr->IsDefault(), true);
        BOOST_CHECK_EQUAL(foundCbAddr->GetExpirationHeight(), height);
    }
}

BOOST_AUTO_TEST_CASE(CoinbaseAddrInsertion)
{
    CoinbaseIndex cbIndex;
    BOOST_CHECK(cbIndex.IsNull());

    CoinbaseAddress cbAddr{"1", CPubKey(), false, 5};
    std::vector<unsigned char> baseBlockHash(32, 0);
    baseBlockHash.back() = 1;

    uint256 blockHash(baseBlockHash);
    cbIndex.AddNewAddress(cbAddr, blockHash);
    BOOST_CHECK(!cbIndex.IsNull());

    auto foundCbAddr = cbIndex.GetCoinbaseWithAddr("1");
    BOOST_CHECK(foundCbAddr);
    BOOST_CHECK(*foundCbAddr == cbAddr);
    auto foundBlockHash = cbIndex.GetBlockHashWithAddr("1");
    BOOST_CHECK(foundBlockHash == blockHash);

    auto notExistingCbAddr = cbIndex.GetCoinbaseWithAddr("2");
    BOOST_CHECK(!notExistingCbAddr);
    auto notFoundBlockHash = cbIndex.GetBlockHashWithAddr("2");
    BOOST_CHECK(notFoundBlockHash == uint256());
}

BOOST_AUTO_TEST_CASE(CoinbaseAddrPruning)
{
    CoinbaseIndex cbIndex;
    BOOST_CHECK(cbIndex.IsNull());

    CoinbaseAddress cbAddr{"1", CPubKey(), false, 5};
    std::vector<unsigned char> baseBlockHash(32, 0);
    baseBlockHash.back() = 1;

    uint256 blockHash(baseBlockHash);
    cbIndex.AddNewAddress(cbAddr, blockHash);
    BOOST_CHECK(!cbIndex.IsNull());

    BlockMap blockMap;
    blockMap[blockHash] = nullptr;

    baseBlockHash.back() = 2;
    uint256 blockHash2(baseBlockHash);
    CoinbaseAddress cbAddr2{"2", CPubKey(), false, 5};
    cbIndex.AddNewAddress(cbAddr2, blockHash2);

    auto foundCbAddr = cbIndex.GetCoinbaseWithAddr("1");
    BOOST_CHECK(foundCbAddr);
    BOOST_CHECK(*foundCbAddr == cbAddr);
    auto foundBlockHash = cbIndex.GetBlockHashWithAddr("1");
    BOOST_CHECK(foundBlockHash == blockHash);

    auto foundCbAddr2 = cbIndex.GetCoinbaseWithAddr("2");
    BOOST_CHECK(foundCbAddr2);
    BOOST_CHECK(*foundCbAddr2 == cbAddr2);
    auto foundBlockHash2 = cbIndex.GetBlockHashWithAddr("2");
    BOOST_CHECK(foundBlockHash2 == blockHash2);

    cbIndex.PruneAddrsWithBlocks(blockMap);
    foundCbAddr2 = cbIndex.GetCoinbaseWithAddr("2");
    BOOST_CHECK(!foundCbAddr2);
    foundBlockHash2 = cbIndex.GetBlockHashWithAddr("2");
    BOOST_CHECK(foundBlockHash2 == uint256());

    blockMap.clear();
    cbIndex.PruneAddrsWithBlocks(blockMap);
    foundCbAddr = cbIndex.GetCoinbaseWithAddr("1");
    BOOST_CHECK(!foundCbAddr);
    foundBlockHash = cbIndex.GetBlockHashWithAddr("1");
    BOOST_CHECK(foundBlockHash == uint256());

    BOOST_CHECK(cbIndex.IsNull());
}

class CoinbaseIndexWithBalanceToSpendSetup : public TestChain100Setup
{
public:
    CoinbaseIndexWithBalanceToSpendSetup()
    {
        CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
        ::bitdb.MakeMock();
        wallet.reset(new CWallet(std::unique_ptr<CWalletDBWrapper>(new CWalletDBWrapper(&bitdb, "wallet_test.dat"))));
        bool firstRun;
        wallet->LoadWallet(firstRun);

        {
            LOCK(wallet->cs_wallet);
            wallet->AddKeyPubKey(coinbaseKey, coinbaseKey.GetPubKey());
        }
        wallet->ScanForWalletTransactions(chainActive.Genesis());

        dummyWallet.reset(new CWallet());
    }

    ~CoinbaseIndexWithBalanceToSpendSetup()
    {
        wallet.reset();
        dummyWallet.reset();
        ::bitdb.Flush(true);
        ::bitdb.Reset();
    }

    std::unique_ptr<CWallet> wallet;
    std::unique_ptr<CWallet> dummyWallet;
};

BOOST_FIXTURE_TEST_CASE(CoinbaseUtils_SelectInputs, CoinbaseIndexWithBalanceToSpendSetup)
{
    auto unspentInputs = SelectInputs(dummyWallet.get(), DEFAULT_COINBASE_TX_FEE);
    BOOST_CHECK(unspentInputs.empty());

    unspentInputs = SelectInputs(wallet.get(), DEFAULT_COINBASE_TX_FEE);
    BOOST_CHECK(!unspentInputs.empty());
}

BOOST_FIXTURE_TEST_CASE(CoinbaseUtils_CreateCoinbaseTransaction, CoinbaseIndexWithBalanceToSpendSetup)
{
    auto mutTx = CreateCoinbaseTransaction({}, DEFAULT_COINBASE_TX_FEE, wallet.get());
    BOOST_CHECK(mutTx.vout.empty());

    auto unspentInputs = SelectInputs(wallet.get(), DEFAULT_COINBASE_TX_FEE);
    BOOST_CHECK(!unspentInputs.empty());

    mutTx = CreateCoinbaseTransaction(unspentInputs, DEFAULT_COINBASE_TX_FEE, nullptr);
    BOOST_CHECK(mutTx.vout.empty());
    
    mutTx = CreateCoinbaseTransaction(unspentInputs, CAmount(0), wallet.get());
    BOOST_CHECK(mutTx.vout.empty());

    mutTx = CreateCoinbaseTransaction(unspentInputs, DEFAULT_COINBASE_TX_FEE, wallet.get());
    BOOST_CHECK(!mutTx.vout.empty());

    mutTx = CreateCoinbaseTransaction(unspentInputs, DEFAULT_COINBASE_TX_FEE, dummyWallet.get());
    BOOST_CHECK(mutTx.vout.empty());
}

BOOST_FIXTURE_TEST_CASE(CoinbaseUtils_SignCoinbaseTransaction, CoinbaseIndexWithBalanceToSpendSetup)
{
    CMutableTransaction mutTx;
    auto result = SignCoinbaseTransaction(mutTx, wallet.get());
    BOOST_CHECK(result);

    result = SignCoinbaseTransaction(mutTx, nullptr);
    BOOST_CHECK(!result);

    auto unspentInputs = SelectInputs(wallet.get(), DEFAULT_COINBASE_TX_FEE);
    BOOST_CHECK(!unspentInputs.empty());

    mutTx = CreateCoinbaseTransaction(unspentInputs, DEFAULT_COINBASE_TX_FEE, wallet.get());
    BOOST_CHECK(!mutTx.vout.empty());

    result = SignCoinbaseTransaction(mutTx, nullptr);
    BOOST_CHECK(!result);

    result = SignCoinbaseTransaction(mutTx, wallet.get());
    BOOST_CHECK(result);
}

BOOST_FIXTURE_TEST_CASE(CoinbaseKeyHandler_SigningKey, CoinbaseIndexWithBalanceToSpendSetup)
{
    CKey dummyPrivKey;
    dummyPrivKey.MakeNewKey(false);
    BOOST_CHECK(dummyPrivKey.IsValid());

    auto keyHex = HexStr(dummyPrivKey.begin(), dummyPrivKey.end());
    boost::filesystem::path savePath = GetDataDir() / "coinbase" / "seckeys";

    std::ofstream outputFile(savePath.string());
    outputFile << keyHex << std::endl;
    outputFile.close();

    CoinbaseKeyHandler keyHandler(GetDataDir());
    auto loadedPrivKey = keyHandler.GetCoinbaseSigningKey();
    BOOST_CHECK(loadedPrivKey.IsValid());
    BOOST_CHECK(dummyPrivKey == loadedPrivKey);
}

// TODO: add here the remaining tests from development branch once they all pass

BOOST_AUTO_TEST_SUITE_END()
