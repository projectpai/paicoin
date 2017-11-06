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
//Secret key: 790e2be478a234b765552f08748aa644f50be60188dcb82339d01b0c3177e8c7
//Public key: 00dae69f877c357a193bb782beba2fe506c1179c21850b47dc5228c592cf800e05bcd515fd27937aaaf25bc44d8ef0e7ee0617cf7d9020b662d3ef08b863750cf4
#define GENESIS_BLOCK_SIGNATURE         "00dae69f877c357a193bb782beba2fe506c1179c21850b47dc5228c592cf800e05bcd515fd27937aaaf25bc44d8ef0e7ee0617cf7d9020b662d3ef08b863750cf4"

#define MAIN_CONSENSUS_POW_LIMIT                uint256S("000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
#define MAIN_GENESIS_BLOCK_UNIX_TIMESTAMP       1507377164
#define MAIN_GENESIS_BLOCK_POW_BITS             24
#define MAIN_GENESIS_BLOCK_NBITS                0x1e00ffff
#define MAIN_GENESIS_BLOCK_NONCE                25448486
#define MAIN_CONSENSUS_HASH_GENESIS_BLOCK       uint256S("0x000000d8bb5aed8fb8092953119c41c086618b21f9d2a07e4dd6c4daa50ace57")
#define MAIN_GENESIS_HASH_MERKLE_ROOT           uint256S("0xcb201b1671423313ed67954904a83b4f2dce58aaef70b7748b4ecbd228cacc54")

#define TESTNET_CONSENSUS_POW_LIMIT             uint256S("000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
#define TESTNET_GENESIS_BLOCK_UNIX_TIMESTAMP    1507377164
#define TESTNET_GENESIS_BLOCK_POW_BITS          24
#define TESTNET_GENESIS_BLOCK_NBITS             0x1e00ffff
#define TESTNET_GENESIS_BLOCK_NONCE             25448486
#define TESTNET_CONSENSUS_HASH_GENESIS_BLOCK    uint256S("0x000000d8bb5aed8fb8092953119c41c086618b21f9d2a07e4dd6c4daa50ace57")
#define TESTNET_GENESIS_HASH_MERKLE_ROOT        uint256S("0xcb201b1671423313ed67954904a83b4f2dce58aaef70b7748b4ecbd228cacc54")

#define REGTEST_CONSENSUS_POW_LIMIT             uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
#define REGTEST_GENESIS_BLOCK_UNIX_TIMESTAMP    1507377164
#define REGTEST_GENESIS_BLOCK_POW_BITS          1
#define REGTEST_GENESIS_BLOCK_NBITS             0x207fffff
#define REGTEST_GENESIS_BLOCK_NONCE             0
#define REGTEST_CONSENSUS_HASH_GENESIS_BLOCK    uint256S("0x49ef40b82df5f405142f002fef63fd41169b402448b6cf35804e0b26440cdfe2")
#define REGTEST_GENESIS_HASH_MERKLE_ROOT        uint256S("0x9ca8cf48f8068061101cecd1258bd72b99f7c473c05023fd5f50857ff201f909")

// #   define CONSENSUS_POW_LIMIT      uint256S("0000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
// #   define GENESIS_BLOCK_POW_BITS   16
// #   define GENESIS_BLOCK_NBITS      0x200000ff
// #   define GENESIS_BLOCK_NONCE      29452
// #   define CONSENSUS_HASH_GENESIS_BLOCK uint256S("0x00007ab0dc3c307c46ddd96db67b32a923d3f509e0ea59b1e56dc1bb148701e9")
// #   define GENESIS_HASH_MERKLE_ROOT     uint256S("0xc3e0a19d810c40ea59a86c4d740a337cec107606ea87af241f5df67f078faf88")

// #   define CONSENSUS_POW_LIMIT      uint256S("00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
// #   define GENESIS_BLOCK_POW_BITS   32
// #   define GENESIS_BLOCK_NBITS      0x1d00ffff
// #   define GENESIS_BLOCK_NONCE      2143301838
// #   define CONSENSUS_HASH_GENESIS_BLOCK uint256S("0x0000000065ab7ac018583243e617f1f7003cedd67be2ab23eac14d6209e4e840")
// #   define GENESIS_HASH_MERKLE_ROOT     uint256S("0x608c387879649b45c6588c243d50fe81ea9c8e162aa9787d872ceb561f4798e7")

#ifdef MINE_FOR_THE_GENESIS_BLOCK
#   include "arith_uint256.h"
#endif // MINE_FOR_THE_GENESIS_BLOCK

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << nBits << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
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
 * Low difficulty:
 * CBlock(hash=00007ab0dc3c307c46ddd96db67b32a923d3f509e0ea59b1e56dc1bb148701e9, ver=0x00000004, hashPrevBlock=0000000000000000000000000000000000000000000000000000000000000000, hashMerkleRoot=c3e0a19d810c40ea59a86c4d740a337cec107606ea87af241f5df67f078faf88, nTime=1507377164, nBits=200000ff, nNonce=29452, vtx=1)
 *  CTransaction(hash=c3e0a19d81, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *    CTxIn(COutPoint(0000000000, 4294967295), coinbase 04ff00002001043c30392f30362f32303137202d2043726561746520796f7572206f776e20617661746172207477696e20746861742074616c6b73206c696b6520796f75)
 *    CScriptWitness()
 *    CTxOut(nValue=50.00000000, scriptPubKey=4100baa4d7e64f21135d61324c7b59)
 *
 * Medium difficulty:
 * CBlock(hash=000000c3bcadb98a581ae21489302f93dada493b38abe69add776e7f5a8c0778, ver=0x00000004, hashPrevBlock=0000000000000000000000000000000000000000000000000000000000000000, hashMerkleRoot=32d635437ebda5da7c1f412c7e0ef6804c73d0e4a2e9c015ce6e519db29d5172, nTime=1507377164, nBits=1e00ffff, nNonce=29647447, vtx=1)
 * CTransaction(hash=32d635437e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *   CTxIn(COutPoint(0000000000, 4294967295), coinbase 04ffff001e01043c30392f30362f32303137202d2043726561746520796f7572206f776e20617661746172207477696e20746861742074616c6b73206c696b6520796f75)
 *   CScriptWitness()
 *   CTxOut(nValue=50.00000000, scriptPubKey=4100baa4d7e64f21135d61324c7b59)
 *
 * CBlock(hash=000000d8bb5aed8fb8092953119c41c086618b21f9d2a07e4dd6c4daa50ace57, ver=0x00000004, hashPrevBlock=0000000000000000000000000000000000000000000000000000000000000000, hashMerkleRoot=cb201b1671423313ed67954904a83b4f2dce58aaef70b7748b4ecbd228cacc54, nTime=1507377164, nBits=1e00ffff, nNonce=25448486, vtx=1)
 * CTransaction(hash=cb201b1671, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *   CTxIn(COutPoint(0000000000, 4294967295), coinbase 04ffff001e01043c30392f30362f32303137202d2043726561746520796f7572206f776e20617661746172207477696e20746861742074616c6b73206c696b6520796f75)
 *   CScriptWitness()
 *   CTxOut(nValue=50.00000000, scriptPubKey=4100dae69f877c357a193bb782beba)
 *
 * High difficulty:
 * CBlock(hash=0000000065ab7ac018583243e617f1f7003cedd67be2ab23eac14d6209e4e840, ver=0x00000004, hashPrevBlock=0000000000000000000000000000000000000000000000000000000000000000, hashMerkleRoot=608c387879649b45c6588c243d50fe81ea9c8e162aa9787d872ceb561f4798e7, nTime=1507377164, nBits=1d00ffff, nNonce=2143301838, vtx=1)
 * CTransaction(hash=608c387879, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *   CTxIn(COutPoint(0000000000, 4294967295), coinbase 04ffff001d01043c30392f30362f32303137202d2043726561746520796f7572206f776e20617661746172207477696e20746861742074616c6b73206c696b6520796f75)
 *   CScriptWitness()
 *   CTxOut(nValue=50.00000000, scriptPubKey=4100baa4d7e64f21135d61324c7b59)
 *
 * Regtest:
 * CBlock(hash=49ef40b82df5f405142f002fef63fd41169b402448b6cf35804e0b26440cdfe2, ver=0x00000004, hashPrevBlock=0000000000000000000000000000000000000000000000000000000000000000, hashMerkleRoot=9ca8cf48f8068061101cecd1258bd72b99f7c473c05023fd5f50857ff201f909, nTime=1507377164, nBits=207fffff, nNonce=0, vtx=1)
 *  CTransaction(hash=9ca8cf48f8, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *    CTxIn(COutPoint(0000000000, 4294967295), coinbase 04ffff7f2001043c30392f30362f32303137202d2043726561746520796f7572206f776e20617661746172207477696e20746861742074616c6b73206c696b6520796f75)
 *    CScriptWitness()
 *    CTxOut(nValue=50.00000000, scriptPubKey=4100dae69f877c357a193bb782beba)
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
        consensus.powLimit = MAIN_CONSENSUS_POW_LIMIT;
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
        consensus.nMinimumChainWork = uint256S("0x000000000000000000000000000000000000000000000000000000000000000");

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

        genesis = CreateGenesisBlock(MAIN_GENESIS_BLOCK_UNIX_TIMESTAMP, 0, MAIN_GENESIS_BLOCK_NBITS, 4, 50 * COIN);

        arith_uint256 bnProofOfWorkLimit(~arith_uint256() >> MAIN_GENESIS_BLOCK_POW_BITS);

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
        genesis = CreateGenesisBlock(MAIN_GENESIS_BLOCK_UNIX_TIMESTAMP, MAIN_GENESIS_BLOCK_NONCE, MAIN_GENESIS_BLOCK_NBITS, 4, 50 * COIN);

        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.BIP34Hash = consensus.hashGenesisBlock;

        // TODO: Update the values below with the data from the above mining for the genesis block
        //       This should only be done once, after the mining and prior to production release
        assert(consensus.hashGenesisBlock == MAIN_CONSENSUS_HASH_GENESIS_BLOCK);
        assert(genesis.hashMerkleRoot == MAIN_GENESIS_HASH_MERKLE_ROOT);

#endif  // MINE_FOR_THE_GENESIS_BLOCK
                
        // Note that of those with the service bits flag, most only support a subset of possible options
        vSeeds.emplace_back("34.215.125.66", true);
        vSeeds.emplace_back("13.58.110.183", true);
        vSeeds.emplace_back("13.124.177.237", true);

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,44);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,131);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,247);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x0F, 0x7F, 0x7D, 0x55};
        base58Prefixes[EXT_SECRET_KEY] = {0x0F, 0x7F, 0xA6, 0x89};

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;

        // TODO PAICOIN Update this when releasing, using the suitable blocks heights and corresponding hashes
        // Check the above note for what make a good checkpoint
        checkpointData = (CCheckpointData) {
            {
                {0, MAIN_CONSENSUS_HASH_GENESIS_BLOCK}
                //{ 0, uint256S("0x000000d8bb5aed8fb8092953119c41c086618b21f9d2a07e4dd6c4daa50ace57")}
            }
        };

        // TODO PAICOIN Update this when releasing, using the block timestamp and the number of transactions upto that block
        // use the blockchain info
        chainTxData = ChainTxData {
            // Data as of block 000000d8bb5aed8fb8092953119c41c086618b21f9d2a07e4dd6c4daa50ace57 (height 0).
            1507377164, // * UNIX timestamp of last known number of transactions
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

        genesis = CreateGenesisBlock(TESTNET_GENESIS_BLOCK_UNIX_TIMESTAMP, 0, TESTNET_GENESIS_BLOCK_NBITS, 4, 50 * COIN);

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
        genesis = CreateGenesisBlock(TESTNET_GENESIS_BLOCK_UNIX_TIMESTAMP, TESTNET_GENESIS_BLOCK_NONCE, TESTNET_GENESIS_BLOCK_NBITS, 4, 50 * COIN);

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
        base58Prefixes[EXT_PUBLIC_KEY] = {0xA5, 0x96, 0xE3, 0xF8};
        base58Prefixes[EXT_SECRET_KEY] = {0xA5, 0x96, 0x46, 0x79};

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;

        // TODO PAICOIN Update this when releasing, using the suitable blocks heights and corresponding hashes
        // Check the above note for what make a good checkpoint
        checkpointData = (CCheckpointData) {
            {
                {0, TESTNET_CONSENSUS_HASH_GENESIS_BLOCK}
                //{ 0, uint256S("0x000000d8bb5aed8fb8092953119c41c086618b21f9d2a07e4dd6c4daa50ace57")}
            }
        };

        // TODO PAICOIN Update this when releasing, using the block timestamp and the number of transactions upto that block
        // use the blockchain info
        chainTxData = ChainTxData {
            // Data as of block 000000d8bb5aed8fb8092953119c41c086618b21f9d2a07e4dd6c4daa50ace57 (height 0).
            1507377164, // * UNIX timestamp of last known number of transactions
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
        consensus.BIP34Height = 0;  // BIP34 is activated from the genesis block
        consensus.BIP65Height = 0;  // BIP65 is activated from the genesis block
        consensus.BIP66Height = 0;  // BIP66 is activated from the genesis block
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
        consensus.nMinimumChainWork = uint256S("0x0000000000000000000000000000000000000000000000000000000000000000");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x0000000000000000000000000000000000000000000000000000000000000000");

    	pchMessageStart[0] = 0x43;
    	pchMessageStart[1] = 0x41;
    	pchMessageStart[2] = 0x4b;
        pchMessageStart[3] = 0x52;

        nDefaultPort = 19567;
        nPruneAfterHeight = 1000;

#ifdef MINE_FOR_THE_GENESIS_BLOCK

        genesis = CreateGenesisBlock(REGTEST_GENESIS_BLOCK_UNIX_TIMESTAMP, 0, REGTEST_GENESIS_BLOCK_NBITS, 4, 50 * COIN);

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
        genesis = CreateGenesisBlock(REGTEST_GENESIS_BLOCK_UNIX_TIMESTAMP, REGTEST_GENESIS_BLOCK_NONCE, REGTEST_GENESIS_BLOCK_NBITS, 4, 50 * COIN);

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
        base58Prefixes[EXT_PUBLIC_KEY] = {0xA5, 0x96, 0xE3, 0xF8};
        base58Prefixes[EXT_SECRET_KEY] = {0xA5, 0x96, 0x46, 0x79};

        checkpointData = (CCheckpointData) {
            {
                {0, REGTEST_CONSENSUS_HASH_GENESIS_BLOCK}
                //{0, uint256S("0x49ef40b82df5f405142f002fef63fd41169b402448b6cf35804e0b26440cdfe2")},
            }
        };

        chainTxData = ChainTxData {
            1507377164,
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
