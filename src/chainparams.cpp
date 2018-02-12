// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "consensus/merkle.h"
#include "rpc/server.h"
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
#define GENESIS_BLOCK_REWARD            1470000000

#define MAINNET_CONSENSUS_POW_LIMIT      uint256S("00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
#define MAINNET_GENESIS_BLOCK_POW_BITS   32
#define MAINNET_GENESIS_BLOCK_NBITS      0x1d00ffff
#define MAINNET_GENESIS_BLOCK_SIGNATURE  "23103f0e2d2abbaad0d79b7a37759b1a382b7821"

#define MAINNET_GENESIS_BLOCK_UNIX_TIMESTAMP 1504706400
#define MAINNET_GENESIS_BLOCK_NONCE          2876968165
#define MAINNET_CONSENSUS_HASH_GENESIS_BLOCK uint256S("0x000000005bcab4d8d77d338d3719c1cda996c5181ffd1c46ee311a69ac3f9397")
#define MAINNET_GENESIS_HASH_MERKLE_ROOT     uint256S("0x4121f4f0d8528d506a3b373035250bf9889846fac61fd90787a3ecdebf22d87e")

#define TESTNET_CONSENSUS_POW_LIMIT      uint256S("0000000009fe61ffffffffffffffffffffffffffffffffffffffffffffffffff")
#define TESTNET_GENESIS_BLOCK_POW_BITS   36 // 24
#define TESTNET_GENESIS_BLOCK_NBITS      0x1c09fe61 // 0x1e00ffff
#define TESTNET_GENESIS_BLOCK_SIGNATURE  "9a8abac6c3d97d37d627e6ebcaf68be72275168b"

#define TESTNET_GENESIS_BLOCK_UNIX_TIMESTAMP 1504706516 
#define TESTNET_GENESIS_BLOCK_NONCE          2253953817 
#define TESTNET_CONSENSUS_HASH_GENESIS_BLOCK uint256S("0x0000000003976df1a1393912d10ea68fae1175ee2c7e6011a0dc4e05f18f8403")
#define TESTNET_GENESIS_HASH_MERKLE_ROOT     uint256S("0x017c8b7b919c08887d2d5ddd4d301037ccd53eb887807f8c74f5f824120d8f19")

#define REGTEST_CONSENSUS_POW_LIMIT      uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
#define REGTEST_GENESIS_BLOCK_POW_BITS   1
#define REGTEST_GENESIS_BLOCK_NBITS      0x207fffff
#define REGTEST_GENESIS_BLOCK_SIGNATURE  "23103f0e2d2abbaad0d79b7a37759b1a382b7821"

#define REGTEST_GENESIS_BLOCK_UNIX_TIMESTAMP 1509798928
#define REGTEST_GENESIS_BLOCK_NONCE          1
#define REGTEST_CONSENSUS_HASH_GENESIS_BLOCK uint256S("0x190a4f6022b980ee9719200b024c1b9df515baea3afbccf1adc93c70aa93941f")
#define REGTEST_GENESIS_HASH_MERKLE_ROOT     uint256S("0x4121f4f0d8528d506a3b373035250bf9889846fac61fd90787a3ecdebf22d87e")

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
 * CBlock(hash=000000005bcab4d8d77d338d3719c1cda996c5181ffd1c46ee311a69ac3f9397, ver=0x00000004, hashPrevBlock=0000000000000000000000000000000000000000000000000000000000000000, hashMerkleRoot=4121f4f0d8528d506a3b373035250bf9889846fac61fd90787a3ecdebf22d87e, nTime=1504706400, nBits=1d00ffff, nNonce=2876968165, vtx=1)
 *   CTransaction(hash=4121f4f0d8, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(0000000000, 4294967295), coinbase 04ffff001d01043c30392f30362f32303137202d2043726561746520796f7572206f776e20617661746172207477696e20746861742074616c6b73206c696b6520796f75)
 *     CScriptWitness()
 *     CTxOut(nValue=1470000000.00000000, scriptPubKey=410439cc2db2636303ea74af82dea7)
 *
 * Testnet
 * CBlock(hash=00000011c4cc444bb57cb52be2fb5efd3609c40b12900be7d86876fb4bf73c92, ver=0x00000004, hashPrevBlock=0000000000000000000000000000000000000000000000000000000000000000, hashMerkleRoot=caed1b804a2aa916d899cb398aed398fa9316d972f615903aafe06d10bedca44, nTime=1504706400, nBits=1e0017f6, nNonce=70790274, vtx=1)
 *   CTransaction(hash=caed1b804a, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(0000000000, 4294967295), coinbase 04ffff001d01043c30392f30362f32303137202d2043726561746520796f7572206f776e20617661746172207477696e20746861742074616c6b73206c696b6520796f75)
 *     CScriptWitness()
 *     CTxOut(nValue=1470000000.00000000, scriptPubKey=a91423103f0e2d2abbaad0d79b7a37)
 *
 * Regtest
 * CBlock(hash=190a4f6022b980ee9719200b024c1b9df515baea3afbccf1adc93c70aa93941f, ver=0x00000004, hashPrevBlock=0000000000000000000000000000000000000000000000000000000000000000, hashMerkleRoot=4121f4f0d8528d506a3b373035250bf9889846fac61fd90787a3ecdebf22d87e, nTime=1509798928, nBits=207fffff, nNonce=1, vtx=1)
 *  CTransaction(hash=4121f4f0d8, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(0000000000, 4294967295), coinbase 04ffff001d01043c30392f30362f32303137202d2043726561746520796f7572206f776e20617661746172207477696e20746861742074616c6b73206c696b6520796f75)
 *     CScriptWitness()
 *     CTxOut(nValue=1470000000.00000000, scriptPubKey=410439cc2db2636303ea74af82dea7)
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward, const char* signature)
{
    const char* pszTimestamp = GENESIS_BLOCK_TIMESTAMP_STRING;
    const CScript genesisOutputScript = CScript() << OP_HASH160 << ParseHex(signature) << OP_EQUAL;
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
        pchMessageStart[0] = 0xfe;
        pchMessageStart[1] = 0xd0;
        pchMessageStart[2] = 0xd5;
        pchMessageStart[3] = 0xf2;

        nDefaultPort = 8567;
        nPruneAfterHeight = 100000;

#ifdef MINE_FOR_THE_GENESIS_BLOCK

        genesis = CreateGenesisBlock(MAINNET_GENESIS_BLOCK_UNIX_TIMESTAMP, 0, MAINNET_GENESIS_BLOCK_NBITS, 4, GENESIS_BLOCK_REWARD * COIN, MAINNET_GENESIS_BLOCK_SIGNATURE);

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
        genesis = CreateGenesisBlock(MAINNET_GENESIS_BLOCK_UNIX_TIMESTAMP, MAINNET_GENESIS_BLOCK_NONCE, MAINNET_GENESIS_BLOCK_NBITS, 4, GENESIS_BLOCK_REWARD * COIN, MAINNET_GENESIS_BLOCK_SIGNATURE);

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
        
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,56);  // P
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,130); // u
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,247); // 9
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

        pchMessageStart[0] = 0x0b;
        pchMessageStart[1] = 0x09;
        pchMessageStart[2] = 0x11;
        pchMessageStart[3] = 0x07;

        nDefaultPort = 18567;
        nPruneAfterHeight = 1000;

#ifdef MINE_FOR_THE_GENESIS_BLOCK
	LogPrintf("Mining for the genesis block to log...\r\n");
        int threadCount = 70;
        int pids[70];
        int pid = 0;
        for (int x = 0; x < threadCount; x++) {
            pid = fork();

            if (pid > 0) {
		printf("[%d] Processing in thread with timestamp: %d\r\n", pid, TESTNET_GENESIS_BLOCK_UNIX_TIMESTAMP + x);
		int timestamp = TESTNET_GENESIS_BLOCK_UNIX_TIMESTAMP + x;
            	genesis = CreateGenesisBlock(timestamp, 0, TESTNET_GENESIS_BLOCK_NBITS, 4, GENESIS_BLOCK_REWARD * COIN, TESTNET_GENESIS_BLOCK_SIGNATURE);
                arith_uint256 bnProofOfWorkLimit(~arith_uint256() >> TESTNET_GENESIS_BLOCK_POW_BITS);

                // deliberately empty for loop finds nonce value.
                for (genesis.nNonce = 0; UintToArith256(genesis.GetHash()) > bnProofOfWorkLimit; genesis.nNonce++) {
			if (genesis.nNonce > 4294967290) {
				printf("[%d] Nonce exceeded limit, resetting with new timestamp\r\n", pid);
				genesis.nNonce = 0;
				timestamp += threadCount;
				genesis = CreateGenesisBlock(timestamp, 0, TESTNET_GENESIS_BLOCK_NBITS, 4, GENESIS_BLOCK_REWARD * COIN, TESTNET_GENESIS_BLOCK_SIGNATURE);
			}	
		}
		printf("NONCE FOUND, HURRAY!\r\n");
                break;
            }
            else {
            	pids[x] = pid;
            }
        }

        if (pid == 0)
        {
   		while (true) {
			sleep(100);
		}
        } else {
            LogPrintf("- new testnet genesis nonce: %u\n", genesis.nNonce);
            LogPrintf("- new testnet genesis hash: %s\n", genesis.GetHash().ToString().c_str());
            LogPrintf("- new testnet genesis merkle root: %s\n", genesis.hashMerkleRoot.ToString().c_str());

            consensus.hashGenesisBlock = genesis.GetHash();
            consensus.BIP34Hash = consensus.hashGenesisBlock;

            LogPrintf("- new testnet genesis block: %s\n", genesis.ToString().c_str());
        	
        }


#else

        // TODO: Update the values below with the nonce from the above mining for the genesis block
        //       This should only be done once, after the mining and prior to production release
        genesis = CreateGenesisBlock(TESTNET_GENESIS_BLOCK_UNIX_TIMESTAMP, TESTNET_GENESIS_BLOCK_NONCE, TESTNET_GENESIS_BLOCK_NBITS, 4, GENESIS_BLOCK_REWARD * COIN, TESTNET_GENESIS_BLOCK_SIGNATURE);

        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.BIP34Hash = consensus.hashGenesisBlock;

        CDataStream ssBlock(SER_NETWORK, PROTOCOL_VERSION | 0);
        ssBlock << genesis;
        std::string strHex = HexStr(ssBlock.begin(), ssBlock.end());
	printf("Genesis block hex: %s\r\n\r\n", strHex.c_str());
        printf("- new testnet genesis nonce: %u\n", genesis.nNonce);
        printf("- new testnet genesis hash: %s\n", genesis.GetHash().ToString().c_str());
        printf("- new testnet genesis merkle root: %s\n", genesis.hashMerkleRoot.ToString().c_str());

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

        pchMessageStart[0] = 0xff;
        pchMessageStart[1] = 0xd1;
        pchMessageStart[2] = 0xd6;
        pchMessageStart[3] = 0xf3;

        nDefaultPort = 19567;
        nPruneAfterHeight = 1000;

#ifdef MINE_FOR_THE_GENESIS_BLOCK

        genesis = CreateGenesisBlock(REGTEST_GENESIS_BLOCK_UNIX_TIMESTAMP, 0, REGTEST_GENESIS_BLOCK_NBITS, 4, GENESIS_BLOCK_REWARD * COIN, REGTEST_GENESIS_BLOCK_SIGNATURE);

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
        genesis = CreateGenesisBlock(REGTEST_GENESIS_BLOCK_UNIX_TIMESTAMP, REGTEST_GENESIS_BLOCK_NONCE, REGTEST_GENESIS_BLOCK_NBITS, 4, GENESIS_BLOCK_REWARD * COIN, REGTEST_GENESIS_BLOCK_SIGNATURE);

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
