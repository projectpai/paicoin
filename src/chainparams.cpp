//
// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017-2020 Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//


#include "chainparams.h"
#include "consensus/merkle.h"

#include "tinyformat.h"
#include "util.h"
#include "utilstrencodings.h"

#include <assert.h>

#include <stdlib.h>

#include "chainparamsseeds.h"
#include "coinbase_addresses.h"

#include "stake/votebits.h"

/**
 * To initialize the block chain by mining a new genesis block uncomment the following define.
 * WARNING: this should only be done once and prior to release in production!
 */
//#define MINE_FOR_THE_GENESIS_BLOCK

#define GENESIS_BLOCK_TIMESTAMP_STRING  "09/06/2017 - Create your own avatar twin that talks like you"
#define GENESIS_BLOCK_REWARD            1470000000

#define MAINNET_CONSENSUS_POW_LIMIT      uint256S("0x0000000009fe61ffffffffffffffffffffffffffffffffffffffffffffffffff");
#define MAINNET_GENESIS_BLOCK_POW_BITS   36 // 32
#define MAINNET_GENESIS_BLOCK_NBITS      0x1c09fe61
#define MAINNET_GENESIS_BLOCK_SIGNATURE  "95ba0161eb524f97d3847653057baaef7d7ba0ff"

#define MAINNET_GENESIS_BLOCK_UNIX_TIMESTAMP 1504706776
#define MAINNET_GENESIS_BLOCK_NONCE          460938808
#define MAINNET_CONSENSUS_HASH_GENESIS_BLOCK uint256S("0x00000000018151b673df2356e5e25bfcfecbcd7cf888717f2458530461512343")
#define MAINNET_GENESIS_HASH_MERKLE_ROOT     uint256S("0x585ac65f505138efceefb3255086b6d7f63c606219b01f115a2941bb93c8362b")

#define MAINNET_HYBRID_CONSENSUS_POW_LIMIT          uint256S("000003e75d000000000000000000000000000000000000000000000000000000");
#define MAINNET_HYBRID_CONSENSUS_INITIAL_DIFFICULTY 0x1e03e75d

#define TESTNET_CONSENSUS_POW_LIMIT      uint256S("0000000009fe61ffffffffffffffffffffffffffffffffffffffffffffffffff")
#define TESTNET_GENESIS_BLOCK_POW_BITS   36 // 24
#define TESTNET_GENESIS_BLOCK_NBITS      0x1c09fe61 // 0x1e00ffff
#define TESTNET_GENESIS_BLOCK_SIGNATURE  "9a8abac6c3d97d37d627e6ebcaf68be72275168b"

#define TESTNET_GENESIS_BLOCK_UNIX_TIMESTAMP 1504706516
#define TESTNET_GENESIS_BLOCK_NONCE          2253953817
#define TESTNET_CONSENSUS_HASH_GENESIS_BLOCK uint256S("0x0000000003976df1a1393912d10ea68fae1175ee2c7e6011a0dc4e05f18f8403")
#define TESTNET_GENESIS_HASH_MERKLE_ROOT     uint256S("0x017c8b7b919c08887d2d5ddd4d301037ccd53eb887807f8c74f5f824120d8f19")

#define TESTNET_HYBRID_CONSENSUS_POW_LIMIT   uint256S("000003e75d000000000000000000000000000000000000000000000000000000");
#define TESTNET_HYBRID_CONSENSUS_INITIAL_DIFFICULTY 0x1e03e75d

#define REGTEST_CONSENSUS_POW_LIMIT      uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")
#define REGTEST_GENESIS_BLOCK_POW_BITS   1
#define REGTEST_GENESIS_BLOCK_NBITS      0x207fffff
#define REGTEST_GENESIS_BLOCK_SIGNATURE  "23103f0e2d2abbaad0d79b7a37759b1a382b7821"

#define REGTEST_GENESIS_BLOCK_UNIX_TIMESTAMP 1509798928
#define REGTEST_GENESIS_BLOCK_NONCE          0
#define REGTEST_CONSENSUS_HASH_GENESIS_BLOCK uint256S("0x47b736c948f15d787327c84bb3ad30a064e67c79154c7608da4b062c1adfe7bb")
#define REGTEST_GENESIS_HASH_MERKLE_ROOT     uint256S("0xcaed1b804a2aa916d899cb398aed398fa9316d972f615903aafe06d10bedca44")

#define REGTEST_HYBRID_CONSENSUS_POW_LIMIT          REGTEST_CONSENSUS_POW_LIMIT
#define REGTEST_HYBRID_CONSENSUS_INITIAL_DIFFICULTY 0x207fffff

#ifdef MINE_FOR_THE_GENESIS_BLOCK
#   include "arith_uint256.h"
#endif // MINE_FOR_THE_GENESIS_BLOCK

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int64_t nStakeDiff, int32_t nVersion, const CAmount& genesisReward)
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
    genesis.nStakeDifficulty = nStakeDiff;
    genesis.nVoteBits = VoteBits::rttAccepted;
    genesis.nTicketPoolSize = 0;
    std::fill(genesis.ticketLotteryState.begin(), genesis.ticketLotteryState.end(), 0);
    genesis.nStakeVersion = 0;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block.
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int64_t nStakeDiff, int32_t nVersion, const CAmount& genesisReward, const char* signature)
{
    const char* pszTimestamp = GENESIS_BLOCK_TIMESTAMP_STRING;
    const CScript genesisOutputScript = CScript() << OP_HASH160 << ParseHex(signature) << OP_EQUAL;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nStakeDiff, nVersion, genesisReward);
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
    CMainParams(): CChainParams(fCoinbaseAddrs) {
        strNetworkID = "main";

        consensus.nSubsidyHalvingInterval = 210000;
        consensus.nTotalBlockSubsidy = 1500;
        consensus.nWorkSubsidyProportion = 4;
        consensus.nStakeSubsidyProportion = 6;

        consensus.BIP34Height = 1;  // BIP34 is activated from the genesis block
        consensus.BIP65Height = 1;  // BIP65 is activated from the genesis block
        consensus.BIP66Height = 1;  // BIP66 is activated from the genesis block
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
        consensus.nMinimumChainWork = uint256S("0x00000000000000000000000000000000000000000000000058dbc3b60ba2df18");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x000000000000000000000000000000000000000000000000000000000000000");

        // coinbase whitelist parameters
        consensus.nCoinbaseWhitelistExpiration = 144; // two weeks

        // hybrid consensus fork parameters
        consensus.nHybridConsensusHeight = 154550; // must be above coinbase maturity (>100)
        consensus.hybridConsensusPowLimit = MAINNET_HYBRID_CONSENSUS_POW_LIMIT;
        consensus.nHybridConsensusInitialDifficulty = MAINNET_HYBRID_CONSENSUS_INITIAL_DIFFICULTY;
        consensus.nHybridConsensusInitialDifficultyBlockCount = 10;

        // stake parameters
        consensus.nMinimumStakeDiff                 = COIN * 2;
        consensus.nTicketPoolSize                   = 8192;
        consensus.nTicketsPerBlock                  = 5;
        consensus.nTicketMaturity                   = 256;
        consensus.nTicketExpiry                     = 5 * consensus.nTicketPoolSize;
        consensus.nMempoolVoteExpiry                = 10;
        // consensus.nCoinbaseMaturity                 = 256;
        consensus.nSStxChangeMaturity               = 1;
        consensus.nTicketPoolSizeWeight             = 4;
        consensus.nStakeDiffAlpha                   = 1;
        consensus.nStakeDiffWindowSize              = 144;
        consensus.nStakeDiffWindows                 = 20;
        consensus.nStakeVersionInterval             = 144 * 2 * 7; // ~2 weeks
        consensus.nMaxFreshStakePerBlock            = 4 * consensus.nTicketsPerBlock;
        consensus.nStakeEnabledHeight               = 155062;         //Hybrid fork height + (consensus.nTicketMaturity * 2)
        consensus.nStakeValidationHeight            = 155574;        // Stake enabled height + (consensus.nTicketMaturity * 2)
        consensus.stakeBaseSigScript                = CScript() << 0x00 << 0x00;
        consensus.nStakeMajorityMultiplier          = 3;
        consensus.nStakeMajorityDivisor             = 4;
        consensus.nMinimumTotalVoteFeeLimit         = 0;
        consensus.nMinimumTotalRevocationFeeLimit   = 1LL << 15;
        //organization related parameters
        consensus.organizationPkScript              = CScript(); //uint256S("TODO add some predef")
        consensus.nOrganizationPkScriptVersion      = 0;
        consensus.vBlockOneLedger                   = {}; //TODO update with smtg resembling BlockOneLedgerMainNet in premine.go

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

        genesis = CreateGenesisBlock(MAINNET_GENESIS_BLOCK_UNIX_TIMESTAMP, 0, MAINNET_GENESIS_BLOCK_NBITS, consensus.nMinimumStakeDiff, 4, GENESIS_BLOCK_REWARD * COIN, MAINNET_GENESIS_BLOCK_SIGNATURE);

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

        genesis = CreateGenesisBlock(MAINNET_GENESIS_BLOCK_UNIX_TIMESTAMP, MAINNET_GENESIS_BLOCK_NONCE, MAINNET_GENESIS_BLOCK_NBITS, consensus.nMinimumStakeDiff, 4, GENESIS_BLOCK_REWARD * COIN, MAINNET_GENESIS_BLOCK_SIGNATURE);

        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.BIP34Hash = consensus.hashGenesisBlock;

        assert(consensus.hashGenesisBlock == MAINNET_CONSENSUS_HASH_GENESIS_BLOCK);
        assert(genesis.hashMerkleRoot == MAINNET_GENESIS_HASH_MERKLE_ROOT);

#endif  // MINE_FOR_THE_GENESIS_BLOCK

        vFixedSeeds.clear();
        vSeeds.clear();
        // Note that of those with the service bits flag, most only support a subset of possible options
        vSeeds.emplace_back("seed.projectpai.com", false);  //Project PAI Foundation - Fallback DNS
        vSeeds.emplace_back("us-west-seed.projectpai.com", false);  //Project PAI Foundation
        vSeeds.emplace_back("us-east-seed.projectpai.com", false); //Project PAI Foundation
        vSeeds.emplace_back("seoul-seed.projectpai.com", false); //Project PAI Foundation
        vSeeds.emplace_back("sing-seed.projectpai.com", false); //Project PAI Foundation
        vSeeds.emplace_back("china-seed.projectpai.com", false); //Project PAI Foundation
        vSeeds.emplace_back("painode.lanier.ai", false); //Lanier AI Labs
        vSeeds.emplace_back("painode.everchain.site", false); //Ever Chain
        vSeeds.emplace_back("relay.j1149.com", false); //J1149
        vSeeds.emplace_back("pai-seed-ohio-1.paico.in", false); //MJH Technologies
        vSeeds.emplace_back("pai-seed-china-1.paico.in", false); //MJH Technologies
        vSeeds.emplace_back("paiseed.upandrunningsoftware.com", false); //Up and Running Software
        vSeeds.emplace_back("paiseed.oben.me", false); //ObEN


        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,56);  // P
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,130); // u
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,247); // 9
        base58Prefixes[EXT_PUBLIC_KEY] = {0x03, 0xDD, 0x47, 0xAF};  // paip
        base58Prefixes[EXT_SECRET_KEY] = {0x03, 0xDD, 0x47, 0xD9};  // paiv

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;

        checkpointData = (CCheckpointData) {
            {
                {     0, MAINNET_CONSENSUS_HASH_GENESIS_BLOCK },
                {   500, uint256S("0x0000000004d612f13cf5426902fb1533a659b16b2e349d941588d6b2de60f99c")},
                { 15000, uint256S("0x00000000000031c7063e5c25c471474d8e983a0456944bef32a09ebfbe38adcd")},
                { 25000, uint256S("0x000000000000000ca65878fd1ed20fb623c2c0f4eac208280dfedfed32765bec")},
                { 50000, uint256S("0x000000000000003d4edeac906ef9f89754d41d13a6a899b6001c0fab6040c3e1")},
                { 60000, uint256S("0x0000000000000058a109ea89e318e96742afcac4d7e255d24ca623c5d59438f5")},
                { 70000, uint256S("0x00000000000000485bef5d43df2af26334cf4d8ada499932b6310d31b8304727")},
                { 80000, uint256S("0x0000000000000048234cb97daa5dd2ade2134958c36b561c303f14f7aee19b84")},
                { 90000, uint256S("0x00000000000000355d4cc5892252b99b497e6d004704da252e2c20147d6252de")},
                { 100000, uint256S("0x000000000000001f63655bcc07ca1940e0413eae21129d55a314f6edcd11adf4")},
                { 110000, uint256S("0x000000000000006276a6b130c48867c725cc46d2dce0b1fd85e57a03b8e62b51")},
                { 120000, uint256S("0x000000000000009a81363c9b3ba2dfc722a7df8ac8a0a78e12eefb6dba9bfe5a")},
                { 130000, uint256S("0x000000000000002a0f12ad179996aa0a5efed53717aa6854be34f9d94c8cef5a")},
                { 140000, uint256S("0x000000000000004ea95aeaf16356c0061e530bcf796beccc657b931dad51ea2a")},
                { 150000, uint256S("0x00000000000000dde450506496572c1825c1ccf682699398f4df8a8219bc33a2")}
            }
        };

        chainTxData = ChainTxData{
      // tx hash = 6096bcf59a26d391bc87a92b4c9a4a1de6fff11ff924e233f2fb5f85bf2ba8fe
      // block hash = 000000000000003d4edeac906ef9f89754d41d13a6a899b6001c0fab6040c3e1
      // block index = 50000
      // tx index = 212838
      // tx timestamp = 1537710546

            1537710546, // * UNIX timestamp of last known number of transactions
            212838,     // * total number of transactions between genesis and that timestamp
                        //   (the tx=... number in the SetBestChain debug.log lines)
            0.00368     // * estimated number of transactions per second after that timestamp
        };
    }
};

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams(): CChainParams(fCoinbaseAddrs) {
        strNetworkID = "test";

        consensus.nSubsidyHalvingInterval = 210000;
        consensus.nTotalBlockSubsidy = 1500;
        consensus.nWorkSubsidyProportion = 4;
        consensus.nStakeSubsidyProportion = 6;

        consensus.BIP34Height = 1;  // BIP34 is activated from the genesis block
        consensus.BIP65Height = 1;  // BIP65 is activated from the genesis block
        consensus.BIP66Height = 1;  // BIP66 is activated from the genesis block
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
        consensus.nMinimumChainWork = uint256S("0x0000000000000000000000000000000000000000000000000000c886e8450964");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x0000000000b13e58c76917eb3b416fc284e36641d952a96c3422b0808d828646");

        // coinbase whitelist parameters
        consensus.nCoinbaseWhitelistExpiration = 144; // one day

        // hybrid consensus fork parameters
        consensus.nHybridConsensusHeight = 78600; // must be above coinbase maturity (>100)
        consensus.hybridConsensusPowLimit = TESTNET_HYBRID_CONSENSUS_POW_LIMIT;
        consensus.nHybridConsensusInitialDifficulty = TESTNET_HYBRID_CONSENSUS_INITIAL_DIFFICULTY;
        consensus.nHybridConsensusInitialDifficultyBlockCount = 10;

        // stake parameters
        consensus.nMinimumStakeDiff                 = COIN * 0.2;
        consensus.nTicketPoolSize                   = 1024;
        consensus.nTicketsPerBlock                  = 5;
        consensus.nTicketMaturity                   = 16;
        consensus.nTicketExpiry                     = 6 * consensus.nTicketPoolSize;
        consensus.nMempoolVoteExpiry                = 10;
        //consensus.nCoinbaseMaturity                 = 100;
        consensus.nSStxChangeMaturity               = 1;
        consensus.nTicketPoolSizeWeight             = 4;
        consensus.nStakeDiffAlpha                   = 1;
        consensus.nStakeDiffWindowSize              = 144;
        consensus.nStakeDiffWindows                 = 20;
        consensus.nStakeVersionInterval             = 144 * 2 * 7; // ~2 weeks
        consensus.nMaxFreshStakePerBlock            = 4 * consensus.nTicketsPerBlock;
        consensus.nStakeEnabledHeight               = 78700;         // > nHybridConsensusHeight + nTicketMaturity
        consensus.nStakeValidationHeight            = 78800;         // > CoinbaseMaturity (100) + nHybridConsensusHeight;
        consensus.stakeBaseSigScript                = CScript() << 0x00 << 0x00;
        consensus.nStakeMajorityMultiplier          = 3;
        consensus.nStakeMajorityDivisor             = 4;
        consensus.nMinimumTotalVoteFeeLimit         = 0;
        consensus.nMinimumTotalRevocationFeeLimit   = 1LL << 15;
        //organization related parameters
        consensus.organizationPkScript              = CScript(); //uint256S("TODO add some predef")
        consensus.nOrganizationPkScriptVersion      = 0;
        consensus.vBlockOneLedger                   = {}; //TODO update with smtg resembling BlockOneLedgerTestNet3 in premine.go

        pchMessageStart[0] = 0x0b;
        pchMessageStart[1] = 0x09;
        pchMessageStart[2] = 0x11;
        pchMessageStart[3] = 0x07;

        nDefaultPort = 18567;
        nPruneAfterHeight = 1000;

#ifdef MINE_FOR_THE_GENESIS_BLOCK

        genesis = CreateGenesisBlock(TESTNET_GENESIS_BLOCK_UNIX_TIMESTAMP, 0, TESTNET_GENESIS_BLOCK_NBITS, consensus.nMinimumStakeDiff, 4, GENESIS_BLOCK_REWARD * COIN, TESTNET_GENESIS_BLOCK_SIGNATURE);

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

        genesis = CreateGenesisBlock(TESTNET_GENESIS_BLOCK_UNIX_TIMESTAMP, TESTNET_GENESIS_BLOCK_NONCE, TESTNET_GENESIS_BLOCK_NBITS, consensus.nMinimumStakeDiff, 4, GENESIS_BLOCK_REWARD * COIN, TESTNET_GENESIS_BLOCK_SIGNATURE);

        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.BIP34Hash = consensus.hashGenesisBlock;

        assert(consensus.hashGenesisBlock == TESTNET_CONSENSUS_HASH_GENESIS_BLOCK);
        assert(genesis.hashMerkleRoot == TESTNET_GENESIS_HASH_MERKLE_ROOT);

#endif  // MINE_FOR_THE_GENESIS_BLOCK

        vFixedSeeds.clear();
        vSeeds.clear();

        // nodes with support for servicebits filtering should be at the top
        vSeeds.emplace_back("34.212.218.239", false);
        vSeeds.emplace_back("13.59.205.159", false);
        vSeeds.emplace_back("52.78.224.215", false);
        vSeeds.emplace_back("13.251.4.241", false);

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

        checkpointData = (CCheckpointData) {
            {
                {    0, TESTNET_CONSENSUS_HASH_GENESIS_BLOCK },
                 {    1, uint256S("0x0000000007f33c46116ced43fbb7eb0307080ab7071c134e4b9ccd1334c61177")},
                 { 1000, uint256S("0x0000000008668e5c597a6f0a97c3aced17389a8bd842afe61dd2310b4f301c9a")},
                 { 2000, uint256S("0x00000000057ba272b77e932a86748252e69ef3bb77ae1756787d2e4240167a4b")},
                 {25000, uint256S("0x0000000001a2549ab5b9496082d269abfece06fdf156899f9163e86414613765")},
                 {40000, uint256S("0x00000000071942d9765ecbf1fd5e267b747975bf3143bdaf58915eb52e595802")},
                 {50000, uint256S("0x00000000000be7744de43a52e6830cafaec5bc2dfa6e446c2dfe0f59fa2c2ff0")},
                 {60000, uint256S("0x00000000040ea839b380c6be96bf8584d6079e0e7c6f772f979d2cafe98f1043")},
                 {70000, uint256S("0x000000000005f2259dfe38b1c2f63726c4c61224800966eacf400ffd2bb46be6")},
                 {77839, uint256S("0x0000000000000ed9faec080ad18e821413f47ea055acc149ad03fba825e82537")} //Pre hybrid consensus checkpoint
            }
        };

        chainTxData = ChainTxData{
      // tx hash = 5e018f1b6d7df6f25a6f6aad59e9176924d5af1390b4c56eee84ced31756519e
      // block hash = 0x0000000001a2549ab5b9496082d269abfece06fdf156899f9163e86414613765
      // block index = 25000
      // tx index = 
      // tx timestamp = 1538415658

            1538415658, // * UNIX timestamp of last known number of transactions
            36432,      // * total number of transactions between genesis and that timestamp
                        //   (the tx=... number in the SetBestChain debug.log lines)
            0.00293     // * estimated number of transactions per second after that timestamp
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
        consensus.nTotalBlockSubsidy = 1500;
        consensus.nWorkSubsidyProportion = 4;
        consensus.nStakeSubsidyProportion = 6;

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

        // coinbase whitelist parameters
        consensus.nCoinbaseWhitelistExpiration = 1; // one block

        // hybrid consensus fork parameters
        consensus.nHybridConsensusHeight = 1500; // with the new DAA it is not required to be a multiple of DifficultyAdjustmentInterval
        consensus.hybridConsensusPowLimit = REGTEST_HYBRID_CONSENSUS_POW_LIMIT;
        consensus.nHybridConsensusInitialDifficulty = REGTEST_HYBRID_CONSENSUS_INITIAL_DIFFICULTY;
        consensus.nHybridConsensusInitialDifficultyBlockCount = 10;

        // stake paramters
        consensus.nMinimumStakeDiff               = 20000;
        consensus.nTicketPoolSize                 = 64;
        consensus.nTicketsPerBlock                = 5;
        consensus.nTicketMaturity                 = 8;
        consensus.nTicketExpiry                   = 3 * consensus.nTicketPoolSize;
        consensus.nMempoolVoteExpiry              = 10;
        // consensus.nCoinbaseMaturity               = 16;
        consensus.nSStxChangeMaturity             = 1;
        consensus.nTicketPoolSizeWeight           = 4;
        consensus.nStakeDiffAlpha                 = 1;
        consensus.nStakeDiffWindowSize            = 8;
        consensus.nStakeDiffWindows               = 8;
        consensus.nStakeVersionInterval           = 6 * 24; // ~1 day
        consensus.nMaxFreshStakePerBlock          = 4 * consensus.nTicketsPerBlock;
        consensus.nStakeEnabledHeight             = 2000;//must be above nHybridConsensusHeight
        consensus.nStakeValidationHeight          = 2100;//must be above nStakeEnabledHeight
        consensus.stakeBaseSigScript              = CScript() << 0x73 << 0x57;
        consensus.nStakeMajorityMultiplier        = 3;
        consensus.nStakeMajorityDivisor           = 4;
        consensus.nMinimumTotalVoteFeeLimit       = 0;
        consensus.nMinimumTotalRevocationFeeLimit = 1LL << 15;
        //organization related parameters
        consensus.organizationPkScript            = CScript(); //uint256S("TODO add some predef")
        consensus.nOrganizationPkScriptVersion    = 0;
        consensus.vBlockOneLedger                 = {}; //TODO update with smtg resembling BlockOneLedgerRegNet in premine.go

        pchMessageStart[0] = 0xff;
        pchMessageStart[1] = 0xd1;
        pchMessageStart[2] = 0xd6;
        pchMessageStart[3] = 0xf3;

        nDefaultPort = 19567;
        nPruneAfterHeight = 1000;

#ifdef MINE_FOR_THE_GENESIS_BLOCK

        genesis = CreateGenesisBlock(REGTEST_GENESIS_BLOCK_UNIX_TIMESTAMP, 0, REGTEST_GENESIS_BLOCK_NBITS, consensus.nMinimumStakeDiff, 4, GENESIS_BLOCK_REWARD * COIN, REGTEST_GENESIS_BLOCK_SIGNATURE);

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
        genesis = CreateGenesisBlock(REGTEST_GENESIS_BLOCK_UNIX_TIMESTAMP, REGTEST_GENESIS_BLOCK_NONCE, REGTEST_GENESIS_BLOCK_NBITS, consensus.nMinimumStakeDiff, 4, GENESIS_BLOCK_REWARD * COIN, REGTEST_GENESIS_BLOCK_SIGNATURE);

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
