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
 * Make sure that you select the appropriate initial difficulty level, to be applied only prior to the initial blocks generation, or when the mining gets too low
 */
#define INITIAL_DIFFICULTY_LEVEL_LOW    0
#define INITIAL_DIFFICULTY_LEVEL_MEDIUM 1
#define INITIAL_DIFFICULTY_LEVEL_HIGH   2

#define INITIAL_DIFFICULTY_LEVEL INITIAL_DIFFICULTY_LEVEL_MEDIUM

/**
 * To initialize the block chain by mining a new genesis block uncomment the following define.
 * WARNING: this should only be done once and prior to release in production!
 */
//#define MINE_FOR_THE_GENESIS_BLOCK
#define GENESIS_BLOCK_UNIX_TIMESTAMP    1507377164
#define GENESIS_BLOCK_TIMESTAMP_STRING  "09/06/2017 - Create your own avatar twin that talks like you"
//#define GENESIS_BLOCK_SIGNATURE         "00BAA4D7E64F21135D61324C7B59D00FC5B8EB1BCC8194D256AF4BDF93CBD8D631A713C66A4D3D9E7F7330BA3DBF7DE1C1CA458DBF552C60AA1C07FB45C4AE6087"
//Secret key: 790E2BE478A234B765552F08748AA644F50BE60188DCB82339D01B0C3177E8C7
//Public key: DAE69F877C357A193BB782BEBA2FE506C1179C21850B47DC5228C592CF800E05BCD515FD27937AAAF25BC44D8EF0E7EE0617CF7D9020B662D3EF08B863750CF4
#define GENESIS_BLOCK_SIGNATURE         "00DAE69F877C357A193BB782BEBA2FE506C1179C21850B47DC5228C592CF800E05BCD515FD27937AAAF25BC44D8EF0E7EE0617CF7D9020B662D3EF08B863750CF4"

#if (INITIAL_DIFFICULTY_LEVEL == INITIAL_DIFFICULTY_LEVEL_LOW)

#   define CONSENSUS_POW_LIMIT      uint256S("0000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
#   define GENESIS_BLOCK_POW_BITS   16
#   define GENESIS_BLOCK_NBITS      0x200000ff

#   define GENESIS_BLOCK_NONCE      29452
#   define CONSENSUS_HASH_GENESIS_BLOCK uint256S("0x00007ab0dc3c307c46ddd96db67b32a923d3f509e0ea59b1e56dc1bb148701e9")
#   define GENESIS_HASH_MERKLE_ROOT     uint256S("0xc3e0a19d810c40ea59a86c4d740a337cec107606ea87af241f5df67f078faf88")

#elif (INITIAL_DIFFICULTY_LEVEL == INITIAL_DIFFICULTY_LEVEL_MEDIUM)

#   define CONSENSUS_POW_LIMIT      uint256S("000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
#   define GENESIS_BLOCK_POW_BITS   24
#   define GENESIS_BLOCK_NBITS      0x1e00ffff

#   define GENESIS_BLOCK_NONCE      25448486
#   define CONSENSUS_HASH_GENESIS_BLOCK uint256S("0x000000d8bb5aed8fb8092953119c41c086618b21f9d2a07e4dd6c4daa50ace57")
#   define GENESIS_HASH_MERKLE_ROOT     uint256S("0xcb201b1671423313ed67954904a83b4f2dce58aaef70b7748b4ecbd228cacc54")

#elif (INITIAL_DIFFICULTY_LEVEL == INITIAL_DIFFICULTY_LEVEL_HIGH)

#   define CONSENSUS_POW_LIMIT      uint256S("00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
#   define GENESIS_BLOCK_POW_BITS   32
#   define GENESIS_BLOCK_NBITS      0x1d00ffff

#   define GENESIS_BLOCK_NONCE      2143301838
#   define CONSENSUS_HASH_GENESIS_BLOCK uint256S("0x0000000065ab7ac018583243e617f1f7003cedd67be2ab23eac14d6209e4e840")
#   define GENESIS_HASH_MERKLE_ROOT     uint256S("0x608c387879649b45c6588c243d50fe81ea9c8e162aa9787d872ceb561f4798e7")

#endif

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
 *
 * High difficulty:
 * CBlock(hash=0000000065ab7ac018583243e617f1f7003cedd67be2ab23eac14d6209e4e840, ver=0x00000004, hashPrevBlock=0000000000000000000000000000000000000000000000000000000000000000, hashMerkleRoot=608c387879649b45c6588c243d50fe81ea9c8e162aa9787d872ceb561f4798e7, nTime=1507377164, nBits=1d00ffff, nNonce=2143301838, vtx=1)
 * CTransaction(hash=608c387879, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *   CTxIn(COutPoint(0000000000, 4294967295), coinbase 04ffff001d01043c30392f30362f32303137202d2043726561746520796f7572206f776e20617661746172207477696e20746861742074616c6b73206c696b6520796f75)
 *   CScriptWitness()
 *   CTxOut(nValue=50.00000000, scriptPubKey=4100baa4d7e64f21135d61324c7b59)
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
        consensus.powLimit = CONSENSUS_POW_LIMIT;
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1916; // 95% of 2016
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 999999999999ULL;

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 999999999999ULL;

        // Deployment of SegWit (BIP141, BIP143, and BIP147)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 999999999999ULL;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x000000000000000000000000000000000000000000000000000000000000000");

        // By default assume that the signatures in ancestors of this block are valid.
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

        genesis = CreateGenesisBlock(GENESIS_BLOCK_UNIX_TIMESTAMP, 0, GENESIS_BLOCK_NBITS, 4, 50 * COIN);

        arith_uint256 bnProofOfWorkLimit(~arith_uint256() >> GENESIS_BLOCK_POW_BITS);

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
        genesis = CreateGenesisBlock(GENESIS_BLOCK_UNIX_TIMESTAMP, GENESIS_BLOCK_NONCE, GENESIS_BLOCK_NBITS, 4, 50 * COIN);

        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.BIP34Hash = consensus.hashGenesisBlock;

        // TODO: Update the values below with the data from the above mining for the genesis block
        //       This should only be done once, after the mining and prior to production release
        assert(consensus.hashGenesisBlock == CONSENSUS_HASH_GENESIS_BLOCK);
        assert(genesis.hashMerkleRoot == GENESIS_HASH_MERKLE_ROOT);

#endif  // MINE_FOR_THE_GENESIS_BLOCK
                
        //commented by subodh to remove paicoin seed nodes
        // Note that of those with the service bits flag, most only support a subset of possible options
        //vSeeds.emplace_back("seed.paicoin.sipa.be", true); // Pieter Wuille, only supports x1, x5, x9, and xd
        //vSeeds.emplace_back("dnsseed.bluematt.me", true); // Matt Corallo, only supports x9
        //vSeeds.emplace_back("dnsseed.paicoin.dashjr.org", false); // Luke Dashjr
        //vSeeds.emplace_back("seed.paicoinstats.com", true); // Christian Decker, supports x1 - xf
        //vSeeds.emplace_back("seed.paicoin.jonasschnelli.ch", true); // Jonas Schnelli, only supports x1, x5, x9, and xd
        //vSeeds.emplace_back("seed.PAI.petertodd.org", true); // Peter Todd, only supports x1, x5, x9, and xd

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,44);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,131);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,247);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x0F, 0x7F, 0x7D, 0x55};
        base58Prefixes[EXT_SECRET_KEY] = {0x0F, 0x7F, 0xA6, 0x89};

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        //cleared by subodh to remove paicoin seed nodes
         vFixedSeeds.clear();

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;

#if (INITIAL_DIFFICULTY_LEVEL == INITIAL_DIFFICULTY_LEVEL_LOW)

        checkpointData = (CCheckpointData) {
            {
                { 0, uint256S("0x00007ab0dc3c307c46ddd96db67b32a923d3f509e0ea59b1e56dc1bb148701e9")},
                { 504, uint256S("0x0000eb049d920fb62d83d83eee2e2e62c86f82fc12ea6ecf88943136fffb55bb")},
                { 1002, uint256S("0x0000993d4cf3d409f21845e6d9b8884bb61fd41ce3abda90fe71271c67d78b38")},
                { 1200, uint256S("0x0000bd2424ff19e01d58a9cf56887f0dbf4b201500f922706b3d7afa0862a039")},
                { 1500, uint256S("0x0000f99c3e65143b5e809db909ce487c719741f5f754650dba5bd42dc8249882")},
                { 1800, uint256S("0x0000b7adcfea3ce92d610fe32ceeded5baf8faaffd86aff0c832272c06cbb0be")},
                { 1903, uint256S("0x00003d58e056adedda72ccc68d4652fb8ecb14ecbb10940a9bfb0b45a451e4d2")},
                { 2030, uint256S("0x0000014b9b012e62dd8610424ed8e538c93b624577bc64cd5c157541c9ec337d")}
            }
        };

        chainTxData = ChainTxData {
            // Data as of block 0000014b9b012e62dd8610424ed8e538c93b624577bc64cd5c157541c9ec337d (height 2030).
            1508184002, // * UNIX timestamp of last known number of transactions
            2030,       // * total number of transactions between genesis and that timestamp
                        //   (the tx=... number in the SetBestChain debug.log lines)
            3.1         // * estimated number of transactions per second after that timestamp
        };

#elif (INITIAL_DIFFICULTY_LEVEL == INITIAL_DIFFICULTY_LEVEL_MEDIUM)

        checkpointData = (CCheckpointData) {
            {
                { 0, uint256S("0x000000c3bcadb98a581ae21489302f93dada493b38abe69add776e7f5a8c0778")},
                { 20, uint256S("0x000000289611c8629be74ba1a2940f288d6ab92c9ffed853d02779c1fa798274")},
                { 50, uint256S("0x000000d3a1a43a25a6714b2e6941cf117a8193f8e5522f08caaacd629360d480")},
                { 100, uint256S("0x000000bae1f5e415ee5a4067153d71e77efd6c177e7e596901c5b37ac406fedb")},
                { 120, uint256S("0x0000004b340346172e9e6a40fff5212761a310a1299c7d6e12244b502e6f0fe5")},
                { 150, uint256S("0x000000a6b02c9ca7845e2197eb079c30156c97fc8b2c078d6524c28a2ba3f241")},
                { 180, uint256S("0x0000004b64317228df6590aec6d5c27bf614eb409bcc441d85697453ef107c5c")},
                { 200, uint256S("0x000000ca18d42b881951d3c1171fd3ea5258d8b10805be2b41f2f7dd8730eddf")}
            }
        };

        chainTxData = ChainTxData {
            // Data as of block 000000ca18d42b881951d3c1171fd3ea5258d8b10805be2b41f2f7dd8730eddf (height 200).
            1508268361, // * UNIX timestamp of last known number of transactions
            200,        // * total number of transactions between genesis and that timestamp
                        //   (the tx=... number in the SetBestChain debug.log lines)
            3.1         // * estimated number of transactions per second after that timestamp
        };

#elif (INITIAL_DIFFICULTY_LEVEL == INITIAL_DIFFICULTY_LEVEL_HIGH)

        checkpointData = (CCheckpointData) {
            {
                { 0, uint256S("0x0000000065ab7ac018583243e617f1f7003cedd67be2ab23eac14d6209e4e840")}
            }
        };

        chainTxData = ChainTxData{
            // Data as of block 000000000000000000d97e53664d17967bd4ee50b23abb92e54a34eb222d15ae (height 478913).
            1507377164, // * UNIX timestamp of last known number of transactions
            0,          // * total number of transactions between genesis and that timestamp
                        //   (the tx=... number in the SetBestChain debug.log lines)
            3.1         // * estimated number of transactions per second after that timestamp
        };

#endif
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
        consensus.powLimit = CONSENSUS_POW_LIMIT;
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 999999999999ULL;

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 999999999999ULL;

        // Deployment of SegWit (BIP141, BIP143, and BIP147)
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
        pchMessageStart[3] = 0x54;

        nDefaultPort = 18567;
        nPruneAfterHeight = 1000;

#ifdef MINE_FOR_THE_GENESIS_BLOCK

        genesis = CreateGenesisBlock(GENESIS_BLOCK_UNIX_TIMESTAMP, 0, GENESIS_BLOCK_NBITS, 4, 50 * COIN);

        arith_uint256 bnProofOfWorkLimit(~arith_uint256() >> GENESIS_BLOCK_POW_BITS);

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
        genesis = CreateGenesisBlock(GENESIS_BLOCK_UNIX_TIMESTAMP, GENESIS_BLOCK_NONCE, GENESIS_BLOCK_NBITS, 4, 50 * COIN);

        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.BIP34Hash = consensus.hashGenesisBlock;

        // TODO: Update the values below with the data from the above mining for the genesis block
        //       This should only be done once, after the mining and prior to production release
        assert(consensus.hashGenesisBlock == CONSENSUS_HASH_GENESIS_BLOCK);
        assert(genesis.hashMerkleRoot == GENESIS_HASH_MERKLE_ROOT);

#endif  // MINE_FOR_THE_GENESIS_BLOCK

        vFixedSeeds.clear();
        vSeeds.clear();

        //commented by subodh to remove paicoin seed nodes
        // nodes with support for servicebits filtering should be at the top
        //vSeeds.emplace_back("testnet-seed.paicoin.jonasschnelli.ch", true);
        //vSeeds.emplace_back("seed.tPAI.petertodd.org", true);
        //vSeeds.emplace_back("testnet-seed.bluematt.me", false);
        //vSeeds.emplace_back("testnet-seed.paicoin.schildbach.de", false);

        // same as for the CRegTestParams
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,51);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,180);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,226);
        base58Prefixes[EXT_PUBLIC_KEY] = {0xA5, 0x96, 0xE3, 0xF8};
        base58Prefixes[EXT_SECRET_KEY] = {0xA5, 0x96, 0x46, 0x79};

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));
        //cleared by  subodh to remove paicoin seed nodes
        vFixedSeeds.clear();

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;

#if (INITIAL_DIFFICULTY_LEVEL == INITIAL_DIFFICULTY_LEVEL_LOW)

        checkpointData = (CCheckpointData) {
            {
                { 0, uint256S("0x00007ab0dc3c307c46ddd96db67b32a923d3f509e0ea59b1e56dc1bb148701e9")},
                { 504, uint256S("0x0000eb049d920fb62d83d83eee2e2e62c86f82fc12ea6ecf88943136fffb55bb")},
                { 1002, uint256S("0x0000993d4cf3d409f21845e6d9b8884bb61fd41ce3abda90fe71271c67d78b38")},
                { 1200, uint256S("0x0000bd2424ff19e01d58a9cf56887f0dbf4b201500f922706b3d7afa0862a039")},
                { 1500, uint256S("0x0000f99c3e65143b5e809db909ce487c719741f5f754650dba5bd42dc8249882")},
                { 1800, uint256S("0x0000b7adcfea3ce92d610fe32ceeded5baf8faaffd86aff0c832272c06cbb0be")},
                { 1903, uint256S("0x00003d58e056adedda72ccc68d4652fb8ecb14ecbb10940a9bfb0b45a451e4d2")},
                { 2030, uint256S("0x0000014b9b012e62dd8610424ed8e538c93b624577bc64cd5c157541c9ec337d")}
            }
        };

        chainTxData = ChainTxData {
            // Data as of block 0000014b9b012e62dd8610424ed8e538c93b624577bc64cd5c157541c9ec337d (height 2030).
            1508184002, // * UNIX timestamp of last known number of transactions
            2030,       // * total number of transactions between genesis and that timestamp
                        //   (the tx=... number in the SetBestChain debug.log lines)
            3.1         // * estimated number of transactions per second after that timestamp
        };

#elif (INITIAL_DIFFICULTY_LEVEL == INITIAL_DIFFICULTY_LEVEL_MEDIUM)

        checkpointData = (CCheckpointData) {
            {
                { 0, uint256S("0x000000d8bb5aed8fb8092953119c41c086618b21f9d2a07e4dd6c4daa50ace57")},
                { 20, uint256S("0x000000356f5b476a800724b1f397d9c2fd055e1924d49c7f360706fcf0a4c3b2")},
                { 50, uint256S("0x0000004073d25ff0365066171813b4af15cd2851836d89a3ac698c3c23c6ec47")},
                { 100, uint256S("0x00000095a4c1a619bf0f113dae044d20da64f721a56130e0dfb97e4715d86397")},
                { 120, uint256S("0x0000002f872d7d9736dd492d7fd8075c5f994865bd56249880e9576ed142c89d")},
                { 150, uint256S("0x000000274a24ee1fa3190bc4966c8c36841372fefb21d766119d37a25aea90c0")},
                { 180, uint256S("0x000000c392fd8e378e2ccda37d4d366df969b39ae39f3c501214708d62b31dfe")},
                { 200, uint256S("0x00000004e96638bc0329e21e14b0ae905610e8253a2a3d5472863f07ffa8c3a4")}
            }
        };

        chainTxData = ChainTxData {
            // Data as of block 00000004e96638bc0329e21e14b0ae905610e8253a2a3d5472863f07ffa8c3a4 (height 200).
            1508448029, // * UNIX timestamp of last known number of transactions
            200,        // * total number of transactions between genesis and that timestamp
                        //   (the tx=... number in the SetBestChain debug.log lines)
            3.1         // * estimated number of transactions per second after that timestamp
        };

#elif (INITIAL_DIFFICULTY_LEVEL == INITIAL_DIFFICULTY_LEVEL_HIGH)

        // checkpointData = (CCheckpointData) {
        //     {
        //         { 0, uint256S("0x000000007822691fb5a61ed358644e51246e27fa755252c9a6dc6be9859937d8")}
        //     }
        // };

        // chainTxData = ChainTxData{
        //     // Data as of block 000000000000000000d97e53664d17967bd4ee50b23abb92e54a34eb222d15ae (height 478913).
        //     1507377164, // * UNIX timestamp of last known number of transactions
        //     0,          // * total number of transactions between genesis and that timestamp
        //                 //   (the tx=... number in the SetBestChain debug.log lines)
        //     3.1         // * estimated number of transactions per second after that timestamp
        // };

#endif
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
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
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

        genesis = CreateGenesisBlock(GENESIS_BLOCK_UNIX_TIMESTAMP, 2, 0x207fffff, 4, 50 * COIN);

        arith_uint256 bnProofOfWorkLimit(~arith_uint256() >> 1);

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
        genesis = CreateGenesisBlock(GENESIS_BLOCK_UNIX_TIMESTAMP, 2, 0x207fffff, 4, 50 * COIN);

        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.BIP34Hash = consensus.hashGenesisBlock;

        // TODO: Update the values below with the data from the above mining for the genesis block
        //       This should only be done once, after the mining and prior to production release
        assert(consensus.hashGenesisBlock == uint256S("0x0a9f0c7316691559d1c1f4b1d2de2fd45761190c0beb4ad3409b50f1c62e2fc5"));
        assert(genesis.hashMerkleRoot == uint256S("0x608c387879649b45c6588c243d50fe81ea9c8e162aa9787d872ceb561f4798e7"));

#endif  // MINE_FOR_THE_GENESIS_BLOCK

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;

        // commented by subodh- will add checkpoints when testing
        /*
        checkpointData = (CCheckpointData) {
            {
                {0, uint256S("0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206")},
            }
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
        };
*/

        checkpointData = (CCheckpointData) {
            {
                {0, uint256S("0x0a9f0c7316691559d1c1f4b1d2de2fd45761190c0beb4ad3409b50f1c62e2fc5")},
            }
        };

        chainTxData = ChainTxData{
            1507377164,
            0,
            0
        };

        // same as for the CTestNetParams
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,51);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,180);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,226);
        base58Prefixes[EXT_PUBLIC_KEY] = {0xA5, 0x96, 0xE3, 0xF8};
        base58Prefixes[EXT_SECRET_KEY] = {0xA5, 0x96, 0x46, 0x79};
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
