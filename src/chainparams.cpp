// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "consensus/merkle.h"

#include "tinyformat.h"
#include "util.h"
#include "utilstrencodings.h"

#include <assert.h>

#include <stdlib.h>

#include "chainparamsseeds.h"

/**
 * To initialize the block chain by mining a new genesis block uncomment the following define.
 * WARNING: this should only be done once and prior to release in production!
 */
//#define MINE_FOR_THE_GENESIS_BLOCK

#define GENESIS_BLOCK_TIMESTAMP_STRING  "09/06/2017 - Create your own avatar twin that talks like you"
#define GENESIS_BLOCK_SIGNATURE         "0439cc2db2636303ea74af82dea750c44959ea968be1badf76428cf4cc25ae1c61127d642146e05a55880b163c061b5f81280800463c1d34da99af39dc1be879f4"
#define GENESIS_BLOCK_REWARD            14700000

#define MAINNET_CONSENSUS_POW_LIMIT      uint256S("00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
#define MAINNET_GENESIS_BLOCK_POW_BITS   32
#define MAINNET_GENESIS_BLOCK_NBITS      0x1d00ffff

#define MAINNET_GENESIS_BLOCK_UNIX_TIMESTAMP 1504706400
#define MAINNET_GENESIS_BLOCK_NONCE          3332680579
#define MAINNET_CONSENSUS_HASH_GENESIS_BLOCK uint256S("0x0000000007c88149d1f3a4c7783fd52500261e691f29ec8682c8fe41dc92887b")
#define MAINNET_GENESIS_HASH_MERKLE_ROOT     uint256S("0xff202fc6377ac26b4ea97145eba7f9aae5b2e7646cf6377ee13d3e103874c8ea")

#define TESTNET_CONSENSUS_POW_LIMIT      uint256S("000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
#define TESTNET_GENESIS_BLOCK_POW_BITS   24
#define TESTNET_GENESIS_BLOCK_NBITS      0x1e00ffff

#define TESTNET_GENESIS_BLOCK_UNIX_TIMESTAMP 1504706400
#define TESTNET_GENESIS_BLOCK_NONCE          48945534
#define TESTNET_CONSENSUS_HASH_GENESIS_BLOCK uint256S("0x000000a8c7bb1f17cb66a12ffbf90d477f403abedc635c0461f83c1f728859c3")
#define TESTNET_GENESIS_HASH_MERKLE_ROOT     uint256S("0xff202fc6377ac26b4ea97145eba7f9aae5b2e7646cf6377ee13d3e103874c8ea")

#define REGTEST_CONSENSUS_POW_LIMIT      uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
#define REGTEST_GENESIS_BLOCK_POW_BITS   1
#define REGTEST_GENESIS_BLOCK_NBITS      0x207fffff

#define REGTEST_GENESIS_BLOCK_UNIX_TIMESTAMP 1509798928
#define REGTEST_GENESIS_BLOCK_NONCE          0
#define REGTEST_CONSENSUS_HASH_GENESIS_BLOCK uint256S("0x393d9a7e56a64047dd59a43a0b0906ff44f2bd53570e2d1242dffd0a651d3b65")
#define REGTEST_GENESIS_HASH_MERKLE_ROOT     uint256S("0xff202fc6377ac26b4ea97145eba7f9aae5b2e7646cf6377ee13d3e103874c8ea")

#ifdef MINE_FOR_THE_GENESIS_BLOCK
#   include "arith_uint256.h"
#endif // MINE_FOR_THE_GENESIS_BLOCK

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * Mainnet:
 * CBlock(hash=0000000007c88149d1f3a4c7783fd52500261e691f29ec8682c8fe41dc92887b, ver=0x00000004, hashPrevBlock=0000000000000000000000000000000000000000000000000000000000000000, hashMerkleRoot=ff202fc6377ac26b4ea97145eba7f9aae5b2e7646cf6377ee13d3e103874c8ea, nTime=1504706400, nBits=1d00ffff, nNonce=3332680579, vtx=1)
 *   CTransaction(hash=ff202fc637, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(0000000000, 4294967295), coinbase 04ffff001d01043c30392f30362f32303137202d2043726561746520796f7572206f776e20617661746172207477696e20746861742074616c6b73206c696b6520796f75)
 *     CScriptWitness()
 *     CTxOut(nValue=14700000.00000000, scriptPubKey=410439cc2db2636303ea74af82dea7)
 *
 * Testnet
 * CBlock(hash=000000a8c7bb1f17cb66a12ffbf90d477f403abedc635c0461f83c1f728859c3, ver=0x00000004, hashPrevBlock=0000000000000000000000000000000000000000000000000000000000000000, hashMerkleRoot=ff202fc6377ac26b4ea97145eba7f9aae5b2e7646cf6377ee13d3e103874c8ea, nTime=1504706400, nBits=1e00ffff, nNonce=48945534, vtx=1)
 *   CTransaction(hash=ff202fc637, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(0000000000, 4294967295), coinbase 04ffff001d01043c30392f30362f32303137202d2043726561746520796f7572206f776e20617661746172207477696e20746861742074616c6b73206c696b6520796f75)
 *     CScriptWitness()
 *     CTxOut(nValue=14700000.00000000, scriptPubKey=410439cc2db2636303ea74af82dea7)
 *
 * Regtest
 * CBlock(hash=393d9a7e56a64047dd59a43a0b0906ff44f2bd53570e2d1242dffd0a651d3b65, ver=0x00000004, hashPrevBlock=0000000000000000000000000000000000000000000000000000000000000000, hashMerkleRoot=ff202fc6377ac26b4ea97145eba7f9aae5b2e7646cf6377ee13d3e103874c8ea, nTime=1509798928, nBits=207fffff, nNonce=0, vtx=1)
 *   CTransaction(hash=ff202fc637, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(0000000000, 4294967295), coinbase 04ffff001d01043c30392f30362f32303137202d2043726561746520796f7572206f776e20617661746172207477696e20746861742074616c6b73206c696b6520796f75)
 *     CScriptWitness()
 *     CTxOut(nValue=14700000.00000000, scriptPubKey=410439cc2db2636303ea74af82dea7)
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = GENESIS_BLOCK_TIMESTAMP_STRING;
    const CScript genesisOutputScript = CScript() << ParseHex(GENESIS_BLOCK_SIGNATURE) << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

void CChainParams::UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    consensus.vDeployments[d].nStartTime = nStartTime;
    consensus.vDeployments[d].nTimeout = nTimeout;
}

bool CChainParams::HasGenesisBlockTxOutPoint(const COutPoint& out) const
{
    for (const auto& tx : genesis.vtx) {
        if (out.hash == tx->GetHash())
            return true;
    }
    return false;
}

/**
 * Main network
 */
/**
 * What makes a good checkpoint block?
 * + Is surrounded by blocks with reasonable timestamps
 *   (no blocks before with a timestamp after, none after with
 *    timestamp before)
 * + Contains no strange transactions
 */

class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = "main";
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.BIP34Height = 0;  // BIP34 is activated from the genesis block
        consensus.BIP65Height = 0;  // BIP65 is activated from the genesis block
        consensus.BIP66Height = 0;  // BIP66 is activated from the genesis block
        consensus.powLimit = MAINNET_CONSENSUS_POW_LIMIT;
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1916; // 95% of 2016
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 999999999999ULL;

        // Deployment of SegWit (BIP141, BIP143, and BIP147)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 999999999999ULL;

        // The best chain should have at least this much work.
        // TODO PAICOIN Update when releasing with the appropriate information in the blockchain info
        consensus.nMinimumChainWork = uint256S("0x0000000000000000000000000000000000000000000000000000000000000000");

        // By default assume that the signatures in ancestors of this block are valid.
        // TODO PAICOIN Update when releasing with the appropriate stable block information
        consensus.defaultAssumeValid = uint256S("0x0000000000000000000000000000000000000000000000000000000000000000");

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0x43;
        pchMessageStart[1] = 0x41;
        pchMessageStart[2] = 0x4b;
        pchMessageStart[3] = 0x45;

        nDefaultPort = 8567;
        nPruneAfterHeight = 100000;

#ifdef MINE_FOR_THE_GENESIS_BLOCK

        genesis = CreateGenesisBlock(MAINNET_GENESIS_BLOCK_UNIX_TIMESTAMP, 0, MAINNET_GENESIS_BLOCK_NBITS, 4, GENESIS_BLOCK_REWARD * COIN);

        arith_uint256 bnProofOfWorkLimit(~arith_uint256() >> MAINNET_GENESIS_BLOCK_POW_BITS);

        LogPrintf("Recalculating params for mainnet.\n");
        LogPrintf("- old mainnet genesis nonce: %u\n", genesis.nNonce);
        LogPrintf("- old mainnet genesis hash:  %s\n", genesis.GetHash().ToString().c_str());
        LogPrintf("- old mainnet genesis merkle root: %s\n", genesis.hashMerkleRoot.ToString().c_str());

        // deliberately empty for loop finds nonce value.
        for (genesis.nNonce = 0; UintToArith256(genesis.GetHash()) > bnProofOfWorkLimit; genesis.nNonce++) { } 

        LogPrintf("- new mainnet genesis nonce: %u\n", genesis.nNonce);
        LogPrintf("- new mainnet genesis hash: %s\n", genesis.GetHash().ToString().c_str());
        LogPrintf("- new mainnet genesis merkle root: %s\n", genesis.hashMerkleRoot.ToString().c_str());

        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.BIP34Hash = consensus.hashGenesisBlock;

        LogPrintf("- new mainnet genesis block: %s\n", genesis.ToString().c_str());

#else

        // TODO: Update the values below with the nonce from the above mining for the genesis block
        //       This should only be done once, after the mining and prior to production release
        genesis = CreateGenesisBlock(MAINNET_GENESIS_BLOCK_UNIX_TIMESTAMP, MAINNET_GENESIS_BLOCK_NONCE, MAINNET_GENESIS_BLOCK_NBITS, 4, GENESIS_BLOCK_REWARD * COIN);

        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.BIP34Hash = consensus.hashGenesisBlock;

        // TODO: Update the values below with the data from the above mining for the genesis block
        //       This should only be done once, after the mining and prior to production release
        assert(consensus.hashGenesisBlock == MAINNET_CONSENSUS_HASH_GENESIS_BLOCK);
        assert(genesis.hashMerkleRoot == MAINNET_GENESIS_HASH_MERKLE_ROOT);

#endif  // MINE_FOR_THE_GENESIS_BLOCK

        vFixedSeeds.clear();
        vSeeds.clear();
        // Note that of those with the service bits flag, most only support a subset of possible options
        vSeeds.emplace_back("34.215.125.66", true);
        vSeeds.emplace_back("13.58.110.183", true);
        vSeeds.emplace_back("13.124.177.237", true);

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));
        
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,44);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,131);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,247);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x03, 0xDD, 0x47, 0xAF};  // paip
        base58Prefixes[EXT_SECRET_KEY] = {0x03, 0xDD, 0x47, 0xD9};  // paiv

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;

        // TODO PAICOIN Update this when releasing, using the suitable blocks heights and corresponding hashes
        // Check the above note for what make a good checkpoint
        checkpointData = (CCheckpointData) {
            {
                { 0, MAINNET_CONSENSUS_HASH_GENESIS_BLOCK }
            }
        };

        // TODO PAICOIN Update this when releasing, using the block timestamp and the number of transactions upto that block
        // use the blockchain info
        chainTxData = ChainTxData{
            MAINNET_GENESIS_BLOCK_UNIX_TIMESTAMP, // * UNIX timestamp of last known number of transactions
            0,          // * total number of transactions between genesis and that timestamp
                        //   (the tx=... number in the SetBestChain debug.log lines)
            3.1         // * estimated number of transactions per second after that timestamp
        };
    }
};

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = "test";
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.BIP34Height = 0;  // BIP34 is activated from the genesis block
        consensus.BIP65Height = 0;  // BIP65 is activated from the genesis block
        consensus.BIP66Height = 0;  // BIP66 is activated from the genesis block
        consensus.powLimit = TESTNET_CONSENSUS_POW_LIMIT;
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 999999999999ULL;

        // Deployment of SegWit (BIP141, BIP143, and BIP147)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 999999999999ULL;

        // The best chain should have at least this much work.
        // TODO PAICOIN Update when releasing with the appropriate information in the blockchain info
        consensus.nMinimumChainWork = uint256S("0x0000000000000000000000000000000000000000000000000000000000000000");

        // By default assume that the signatures in ancestors of this block are valid.
        // TODO PAICOIN Update when releasing with the appropriate stable block information
        consensus.defaultAssumeValid = uint256S("0x0000000000000000000000000000000000000000000000000000000000000000");

        pchMessageStart[0] = 0x43;
        pchMessageStart[1] = 0x41;
        pchMessageStart[2] = 0x4b;
        pchMessageStart[3] = 0x54;

        nDefaultPort = 18567;
        nPruneAfterHeight = 1000;

#ifdef MINE_FOR_THE_GENESIS_BLOCK

        genesis = CreateGenesisBlock(TESTNET_GENESIS_BLOCK_UNIX_TIMESTAMP, 0, TESTNET_GENESIS_BLOCK_NBITS, 4, GENESIS_BLOCK_REWARD * COIN);

        arith_uint256 bnProofOfWorkLimit(~arith_uint256() >> TESTNET_GENESIS_BLOCK_POW_BITS);

        LogPrintf("Recalculating params for testnet.\n");
        LogPrintf("- old testnet genesis nonce: %u\n", genesis.nNonce);
        LogPrintf("- old testnet genesis hash:  %s\n", genesis.GetHash().ToString().c_str());
        LogPrintf("- old testnet genesis merkle root: %s\n", genesis.hashMerkleRoot.ToString().c_str());

        // deliberately empty for loop finds nonce value.
        for (genesis.nNonce = 0; UintToArith256(genesis.GetHash()) > bnProofOfWorkLimit; genesis.nNonce++) { } 

        LogPrintf("- new testnet genesis nonce: %u\n", genesis.nNonce);
        LogPrintf("- new testnet genesis hash: %s\n", genesis.GetHash().ToString().c_str());
        LogPrintf("- new testnet genesis merkle root: %s\n", genesis.hashMerkleRoot.ToString().c_str());

        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.BIP34Hash = consensus.hashGenesisBlock;

        LogPrintf("- new testnet genesis block: %s\n", genesis.ToString().c_str());

#else

        // TODO: Update the values below with the nonce from the above mining for the genesis block
        //       This should only be done once, after the mining and prior to production release
        genesis = CreateGenesisBlock(TESTNET_GENESIS_BLOCK_UNIX_TIMESTAMP, TESTNET_GENESIS_BLOCK_NONCE, TESTNET_GENESIS_BLOCK_NBITS, 4, GENESIS_BLOCK_REWARD * COIN);

        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.BIP34Hash = consensus.hashGenesisBlock;

        // TODO: Update the values below with the data from the above mining for the genesis block
        //       This should only be done once, after the mining and prior to production release
        assert(consensus.hashGenesisBlock == TESTNET_CONSENSUS_HASH_GENESIS_BLOCK);
        assert(genesis.hashMerkleRoot == TESTNET_GENESIS_HASH_MERKLE_ROOT);

#endif  // MINE_FOR_THE_GENESIS_BLOCK

        vFixedSeeds.clear();
        vSeeds.clear();
       
        // nodes with support for servicebits filtering should be at the top
        vSeeds.emplace_back("52.37.189.65", true);
        vSeeds.emplace_back("13.59.205.159", true);
        vSeeds.emplace_back("52.78.224.215", true);

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));
        
        // same as for the CRegTestParams
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,51);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,180);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,226);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x03, 0xE3, 0xC5, 0x26};  // ptpu
        base58Prefixes[EXT_SECRET_KEY] = {0x03, 0xE3, 0xC5, 0x2D};  // ptpv

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;

        // TODO PAICOIN Update this when releasing, using the suitable blocks heights and corresponding hashes
        // Check the above note for what make a good checkpoint
        checkpointData = (CCheckpointData) {
            {
                { 0, TESTNET_CONSENSUS_HASH_GENESIS_BLOCK }
            }
        };

        // TODO PAICOIN Update this when releasing, using the block timestamp and the number of transactions upto that block
        // use the blockchain info
        chainTxData = ChainTxData{
            TESTNET_GENESIS_BLOCK_UNIX_TIMESTAMP, // * UNIX timestamp of last known number of transactions
            0,          // * total number of transactions between genesis and that timestamp
                        //   (the tx=... number in the SetBestChain debug.log lines)
            3.1         // * estimated number of transactions per second after that timestamp
        };
    }
};

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    CRegTestParams() {
        strNetworkID = "regtest";
        consensus.nSubsidyHalvingInterval = 150;
        // NOTE PAICOIN Do not mofify the BIP settings, otherwise the current txvalidationcache_tests will fail
        consensus.BIP34Height = 100000000; // BIP34 has not activated on regtest (far in the future so block v1 are not rejected in tests)
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1351; // BIP65 activated on regtest (Used in rpc activation tests)
        consensus.BIP66Height = 1251; // BIP66 activated on regtest (Used in rpc activation tests)
        consensus.powLimit = REGTEST_CONSENSUS_POW_LIMIT;
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 999999999999ULL;

        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 999999999999ULL;

        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 999999999999ULL;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        pchMessageStart[0] = 0x43;
        pchMessageStart[1] = 0x41;
        pchMessageStart[2] = 0x4b;
        pchMessageStart[3] = 0x52;

        nDefaultPort = 19567;
        nPruneAfterHeight = 1000;

#ifdef MINE_FOR_THE_GENESIS_BLOCK

        genesis = CreateGenesisBlock(REGTEST_GENESIS_BLOCK_UNIX_TIMESTAMP, 0, REGTEST_GENESIS_BLOCK_NBITS, 4, GENESIS_BLOCK_REWARD * COIN);

        arith_uint256 bnProofOfWorkLimit(~arith_uint256() >> REGTEST_GENESIS_BLOCK_POW_BITS);

        LogPrintf("Recalculating params for regtest.\n");
        LogPrintf("- old regtest genesis nonce: %u\n", genesis.nNonce);
        LogPrintf("- old regtest genesis hash:  %s\n", genesis.GetHash().ToString().c_str());
        LogPrintf("- old regtest genesis merkle root: %s\n", genesis.hashMerkleRoot.ToString().c_str());

        // deliberately empty for loop finds nonce value.
        for (genesis.nNonce = 0; UintToArith256(genesis.GetHash()) > bnProofOfWorkLimit; genesis.nNonce++) { } 

        LogPrintf("- new regtest genesis nonce: %u\n", genesis.nNonce);
        LogPrintf("- new regtest genesis hash: %s\n", genesis.GetHash().ToString().c_str());
        LogPrintf("- new regtest genesis merkle root: %s\n", genesis.hashMerkleRoot.ToString().c_str());

        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.BIP34Hash = consensus.hashGenesisBlock;

        LogPrintf("- new regtest genesis block: %s\n", genesis.ToString().c_str());

#else

        // TODO: Update the values below with the nonce from the above mining for the genesis block
        //       This should only be done once, after the mining and prior to production release
        genesis = CreateGenesisBlock(REGTEST_GENESIS_BLOCK_UNIX_TIMESTAMP, REGTEST_GENESIS_BLOCK_NONCE, REGTEST_GENESIS_BLOCK_NBITS, 4, GENESIS_BLOCK_REWARD * COIN);

        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.BIP34Hash = consensus.hashGenesisBlock;

        // TODO: Update the values below with the data from the above mining for the genesis block
        //       This should only be done once, after the mining and prior to production release
        assert(consensus.hashGenesisBlock == REGTEST_CONSENSUS_HASH_GENESIS_BLOCK);
        assert(genesis.hashMerkleRoot == REGTEST_GENESIS_HASH_MERKLE_ROOT);

#endif  // MINE_FOR_THE_GENESIS_BLOCK

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;

        // same as for the CTestNetParams
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,51);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,180);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,226);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x03, 0xE3, 0xC5, 0x26};  // ptpu
        base58Prefixes[EXT_SECRET_KEY] = {0x03, 0xE3, 0xC5, 0x2D};  // ptpv

        checkpointData = (CCheckpointData) {
            {
                {0, REGTEST_CONSENSUS_HASH_GENESIS_BLOCK },
            }
        };

        chainTxData = ChainTxData{
            REGTEST_GENESIS_BLOCK_UNIX_TIMESTAMP,
            0,
            0
        };
    }
};

static std::unique_ptr<CChainParams> globalChainParams;

const CChainParams &Params() {
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<CChainParams> CreateChainParams(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
        return std::unique_ptr<CChainParams>(new CMainParams());
    else if (chain == CBaseChainParams::TESTNET)
        return std::unique_ptr<CChainParams>(new CTestNetParams());
    else if (chain == CBaseChainParams::REGTEST)
        return std::unique_ptr<CChainParams>(new CRegTestParams());
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(network);
}

void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    globalChainParams->UpdateVersionBitsParameters(d, nStartTime, nTimeout);
}
