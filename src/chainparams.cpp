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

#define GENESIS_BLOCK_TIMESTAMP_STRING  "09/06/2017 - Create your own avatar twin that talks like you"
#define GENESIS_BLOCK_REWARD            1470000000

#define MAINNET_CONSENSUS_POW_LIMIT      uint256S("0x0000000009fe61ffffffffffffffffffffffffffffffffffffffffffffffffff")
#define MAINNET_GENESIS_BLOCK_POW_BITS   36 // 32
#define MAINNET_GENESIS_BLOCK_NBITS      0x1c09fe61
#define MAINNET_GENESIS_BLOCK_SIGNATURE  "95ba0161eb524f97d3847653057baaef7d7ba0ff"

#define MAINNET_GENESIS_BLOCK_UNIX_TIMESTAMP 1504706776
#define MAINNET_GENESIS_BLOCK_NONCE          460938808
#define MAINNET_CONSENSUS_HASH_GENESIS_BLOCK uint256S("0x00000000018151b673df2356e5e25bfcfecbcd7cf888717f2458530461512343")
#define MAINNET_GENESIS_HASH_MERKLE_ROOT     uint256S("0x585ac65f505138efceefb3255086b6d7f63c606219b01f115a2941bb93c8362b")

#define MAINNET_HYBRID_CONSENSUS_POW_LIMIT          uint256S("000003e75d000000000000000000000000000000000000000000000000000000")
#define MAINNET_HYBRID_CONSENSUS_INITIAL_DIFFICULTY 0x1e03e75d

#define MAINNET_PAICOIN_HASH_INITIAL_DIFFICULTY 0x1e03e75d

#define TESTNET_CONSENSUS_POW_LIMIT      uint256S("0000000009fe61ffffffffffffffffffffffffffffffffffffffffffffffffff")
#define TESTNET_GENESIS_BLOCK_POW_BITS   36 // 24
#define TESTNET_GENESIS_BLOCK_NBITS      0x1c09fe61 // 0x1e00ffff
#define TESTNET_GENESIS_BLOCK_SIGNATURE  "9a8abac6c3d97d37d627e6ebcaf68be72275168b"

#define TESTNET_GENESIS_BLOCK_UNIX_TIMESTAMP 1504706516  
#define TESTNET_GENESIS_BLOCK_NONCE          2253953817
#define TESTNET_CONSENSUS_HASH_GENESIS_BLOCK uint256S("0x0000000003976df1a1393912d10ea68fae1175ee2c7e6011a0dc4e05f18f8403")
#define TESTNET_GENESIS_HASH_MERKLE_ROOT     uint256S("0x017c8b7b919c08887d2d5ddd4d301037ccd53eb887807f8c74f5f824120d8f19")

#define TESTNET_HYBRID_CONSENSUS_POW_LIMIT   uint256S("000003e75d000000000000000000000000000000000000000000000000000000")
#define TESTNET_HYBRID_CONSENSUS_INITIAL_DIFFICULTY 0x1e03e75d

#define TESTNET_PAICOIN_HASH_INITIAL_DIFFICULTY 0x1e03e75d

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

#define REGTEST_PAICOIN_HASH_INITIAL_DIFFICULTY 0x207fffff

#include "arith_uint256.h"

uint256 GENESIS_UINT256(const char * name, uint256 value)
{
#ifdef PAI_BABY
    return uint256S(gGenesisparams.GetArg(name, ""));
#else
    return value;
#endif
}

uint32_t GENESIS_UINT32(const char * name, uint32_t value)
{
#ifdef PAI_BABY
    return gGenesisparams.GetArg(name, (uint32_t) 0);
#else
    return value;
#endif
}

uint256 CHAINPARAMS_UINT256(const char * name, uint256 value)
{
#ifdef PAI_BABY
    return uint256S(gChainparams.GetArg(name, ""));
#else
    return value;
#endif
}

uint32_t CHAINPARAMS_UINT32(const char * name, uint32_t value)
{
#ifdef PAI_BABY
    return gChainparams.GetArg(name, (uint32_t) 0);
#else
    return value;
#endif
}

std::string CHAINPARAMS_STR(const char * name, const std::string & value)
{
#ifdef PAI_BABY
    return gChainparams.GetArg(name, "");
#else
    return value;
#endif
}

uint32_t CHAINPARAMS_XUINT32(const char * name, uint32_t value)
{
#ifdef PAI_BABY
    return strtoll(gChainparams.GetArg(name, "").c_str(), nullptr, 16);
#else
    return value;
#endif
}

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
/*
    genesis.nStakeDifficulty = nStakeDiff;
    genesis.nVoteBits = VoteBits::rttAccepted;
    genesis.nTicketPoolSize = 0;
    std::fill(genesis.ticketLotteryState.begin(), genesis.ticketLotteryState.end(), 0);
    genesis.nStakeVersion = 0;
*/
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
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward, const std::string & signature)
{
    std::string sTimestamp = CHAINPARAMS_STR("GENESIS_BLOCK_TIMESTAMP_STRING", GENESIS_BLOCK_TIMESTAMP_STRING);
    const CScript genesisOutputScript = CScript() << OP_HASH160 << ParseHex(signature.c_str()) << OP_EQUAL;
    return CreateGenesisBlock(sTimestamp.c_str(), genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
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

void CChainParams::LoadGenesisParams() const
{
#ifdef PAI_BABY
    std::string genesisConfFilename = GetDataDir().string() + '/' + PAICOIN_GENESIS_CONF_FILENAME;
    try
    {
        gGenesisparams.ReadConfigFile(genesisConfFilename);
    }
    catch (std::exception & ex)
    {
        throw std::runtime_error(std::string("Error reading genesis configuration (") + genesisConfFilename + ") : " + ex.what());
    }
#endif
}

void SaveGenesisParams(const std::string & prefix, const CBlock & genesis)
{
#ifdef PAI_BABY
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%u", genesis.nNonce);
    gGenesisparams.SoftSetArg(prefix + "_GENESIS_BLOCK_NONCE", buffer);
    gGenesisparams.SoftSetArg(prefix + "_CONSENSUS_HASH_GENESIS_BLOCK", std::string("0x") + genesis.GetHash().ToString());
    gGenesisparams.SoftSetArg(prefix + "_GENESIS_HASH_MERKLE_ROOT", std::string("0x") + genesis.hashMerkleRoot.ToString());
#endif
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
    CMainParams(): CChainParams() {
        strNetworkID = "main";

        consensus.nSubsidyHalvingInterval = 210000;
        consensus.nTotalBlockSubsidy = 1500;
        consensus.nWorkSubsidyProportion = 4;
        consensus.nStakeSubsidyProportion = 6;

        consensus.BIP34Height = 1;  // BIP34 is activated from the genesis block
        consensus.BIP65Height = 1;  // BIP65 is activated from the genesis block
        consensus.BIP66Height = 1;  // BIP66 is activated from the genesis block
        consensus.powLimit = CHAINPARAMS_UINT256("MAINNET_CONSENSUS_POW_LIMIT", MAINNET_CONSENSUS_POW_LIMIT);
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = CHAINPARAMS_UINT32("BLOCK_TIME", 10 * 60);
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
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x000000000000000000000000000000000000000000000000000000000000000");

        // coinbase whitelist parameters
        consensus.nCoinbaseWhitelistExpiration = 144; // two weeks

        // hybrid consensus fork parameters
        consensus.nHybridConsensusHeight = 154550; // must be above coinbase maturity (>100)
        consensus.hybridConsensusPowLimit = MAINNET_HYBRID_CONSENSUS_POW_LIMIT;
        consensus.nHybridConsensusInitialDifficulty = MAINNET_HYBRID_CONSENSUS_INITIAL_DIFFICULTY;
        consensus.nHybridConsensusInitialDifficultyBlockCount = 10;

        // hashing algorithm parameters
        consensus.nPaicoinHashTimestamp = 1632841200LL;
        consensus.nPaicoinHashMaximumActivationHeight = 220000;
        consensus.nPaicoinHashInitialDifficulty = MAINNET_PAICOIN_HASH_INITIAL_DIFFICULTY;
        consensus.nPaicoinHashInitialDifficultyBlockCount = 10;

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

        if (gArgs.IsArgSet("-mine-genesis-block"))
        {
            genesis = CreateGenesisBlock(
                CHAINPARAMS_UINT32("MAINNET_GENESIS_BLOCK_UNIX_TIMESTAMP", MAINNET_GENESIS_BLOCK_UNIX_TIMESTAMP),
                0,
                CHAINPARAMS_XUINT32("MAINNET_GENESIS_BLOCK_NBITS", MAINNET_GENESIS_BLOCK_NBITS),
                4,
                CHAINPARAMS_UINT32("GENESIS_BLOCK_REWARD", GENESIS_BLOCK_REWARD) * COIN,
                CHAINPARAMS_STR("MAINNET_GENESIS_BLOCK_SIGNATURE", MAINNET_GENESIS_BLOCK_SIGNATURE)
            );

            arith_uint256 bnProofOfWorkLimit(~arith_uint256() >> CHAINPARAMS_UINT32("MAINNET_GENESIS_BLOCK_POW_BITS", MAINNET_GENESIS_BLOCK_POW_BITS));

        LogPrintf("Recalculating params for mainnet.\n");
        LogPrintf("- old mainnet genesis nonce: %u\n", genesis.nNonce);
        LogPrintf("- old mainnet genesis hash:  %s\n", genesis.GetHash().ToString().c_str());
        LogPrintf("- old mainnet genesis merkle root: %s\n", genesis.hashMerkleRoot.ToString().c_str());

            for (genesis.nNonce = 0; UintToArith256(genesis.GetHash()) > bnProofOfWorkLimit; genesis.nNonce++) { }

        LogPrintf("- new mainnet genesis nonce: %u\n", genesis.nNonce);
        LogPrintf("- new mainnet genesis hash: %s\n", genesis.GetHash().ToString().c_str());
        LogPrintf("- new mainnet genesis merkle root: %s\n", genesis.hashMerkleRoot.ToString().c_str());

        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.BIP34Hash = consensus.hashGenesisBlock;

        LogPrintf("- new mainnet genesis block: %s\n", genesis.ToString().c_str());
        SaveGenesisParams("MAINNET", genesis);

        vFixedSeeds.clear();
        vSeeds.clear();
        // Note that of those with the service bits flag, most only support a subset of possible options
        }
        else
        {
            LoadGenesisParams();

            genesis = CreateGenesisBlock(
                CHAINPARAMS_UINT32("MAINNET_GENESIS_BLOCK_UNIX_TIMESTAMP", MAINNET_GENESIS_BLOCK_UNIX_TIMESTAMP),
                GENESIS_UINT32("MAINNET_GENESIS_BLOCK_NONCE", MAINNET_GENESIS_BLOCK_NONCE),
                CHAINPARAMS_XUINT32("MAINNET_GENESIS_BLOCK_NBITS", MAINNET_GENESIS_BLOCK_NBITS),
                4,
                CHAINPARAMS_UINT32("GENESIS_BLOCK_REWARD", GENESIS_BLOCK_REWARD) * COIN,
                CHAINPARAMS_STR("MAINNET_GENESIS_BLOCK_SIGNATURE", MAINNET_GENESIS_BLOCK_SIGNATURE)
            );

            consensus.hashGenesisBlock = genesis.GetHash();
            consensus.BIP34Hash = consensus.hashGenesisBlock;

            assert(consensus.hashGenesisBlock == GENESIS_UINT256("MAINNET_CONSENSUS_HASH_GENESIS_BLOCK", MAINNET_CONSENSUS_HASH_GENESIS_BLOCK));
            assert(genesis.hashMerkleRoot == GENESIS_UINT256("MAINNET_GENESIS_HASH_MERKLE_ROOT", MAINNET_GENESIS_HASH_MERKLE_ROOT));
        }

        for (int index = 0; index < 3; ++index)
        {
            char buf[20];
            snprintf(buf, sizeof(buf), "MAINNET_SEED_%d", index); 
            std::string seed = CHAINPARAMS_STR(buf, "");
            if (!seed.empty())
            {
                vSeeds.emplace_back(seed, false);
            }
        }
        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,56);  // P
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,130); // u
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,247); // 9
        base58Prefixes[EXT_PUBLIC_KEY] = {0x03, 0xDD, 0x47, 0xAF};  // paip
        base58Prefixes[EXT_SECRET_KEY] = {0x03, 0xDD, 0x47, 0xD9};  // paiv

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;

#ifndef PAI_BABY
        checkpointData = (CCheckpointData) {
            {
                {     0, MAINNET_CONSENSUS_HASH_GENESIS_BLOCK },
                {   500, uint256S("0x0000000004d612f13cf5426902fb1533a659b16b2e349d941588d6b2de60f99c")},
                { 15000, uint256S("0x00000000000031c7063e5c25c471474d8e983a0456944bef32a09ebfbe38adcd")},
                { 25000, uint256S("0x000000000000000ca65878fd1ed20fb623c2c0f4eac208280dfedfed32765bec")}
            }
        };

        chainTxData = ChainTxData {
            // tx hash = c54bee8227b2b009dcd4d53b1f01de328b86417b475a1f0540b8cca91797b256
            // block hash = 0x000000000000000ca65878fd1ed20fb623c2c0f4eac208280dfedfed32765bec
            // block index = 25000
            // tx index = 25073
            // tx timestamp = 1523981287

            1523981287, // * UNIX timestamp of last known number of transactions
            25073,      // * total number of transactions between genesis and that timestamp
                        //   (the tx=... number in the SetBestChain debug.log lines)
            0.00179     // * estimated number of transactions per second after that timestamp
        };
#else
        checkpointData = (CCheckpointData) {
            {
                { 0, GENESIS_UINT256("MAINNET_CONSENSUS_HASH_GENESIS_BLOCK", MAINNET_CONSENSUS_HASH_GENESIS_BLOCK) }
            }
        };

        chainTxData = ChainTxData {
            CHAINPARAMS_UINT32("MAINNET_GENESIS_BLOCK_UNIX_TIMESTAMP", MAINNET_GENESIS_BLOCK_UNIX_TIMESTAMP), // * UNIX timestamp of last known number of transactions
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
    CTestNetParams(): CChainParams() {
        strNetworkID = "test";

        consensus.nSubsidyHalvingInterval = 210000;
        consensus.nTotalBlockSubsidy = 1500;
        consensus.nWorkSubsidyProportion = 4;
        consensus.nStakeSubsidyProportion = 6;

        consensus.BIP34Height = 1;  // BIP34 is activated from the genesis block
        consensus.BIP65Height = 1;  // BIP65 is activated from the genesis block
        consensus.BIP66Height = 1;  // BIP66 is activated from the genesis block
        consensus.powLimit = CHAINPARAMS_UINT256("TESTNET_CONSENSUS_POW_LIMIT", TESTNET_CONSENSUS_POW_LIMIT);
        //consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetTimespan = 60 * 60; // every hour
        consensus.nPowTargetSpacing = CHAINPARAMS_UINT32("BLOCK_TIME", 10 * 60);
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        //consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nRuleChangeActivationThreshold = 540; // 75% for testchains
        //consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.nMinerConfirmationWindow = 720; // nPowTargetTimespan / nPowTargetSpacing

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
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        // coinbase whitelist parameters
        consensus.nCoinbaseWhitelistExpiration = 144; // one day

        // hybrid consensus fork parameters
        consensus.nHybridConsensusHeight = 78600; // must be above coinbase maturity (>100)
        consensus.hybridConsensusPowLimit = TESTNET_HYBRID_CONSENSUS_POW_LIMIT;
        consensus.nHybridConsensusInitialDifficulty = TESTNET_HYBRID_CONSENSUS_INITIAL_DIFFICULTY;
        consensus.nHybridConsensusInitialDifficultyBlockCount = 10;

        // hashing algorithm parameters
        consensus.nPaicoinHashTimestamp = 1628686800LL;
        consensus.nPaicoinHashMaximumActivationHeight = 90000;
        consensus.nPaicoinHashInitialDifficulty = TESTNET_PAICOIN_HASH_INITIAL_DIFFICULTY;
        consensus.nPaicoinHashInitialDifficultyBlockCount = 10;

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

        nDefaultPort = CHAINPARAMS_UINT32("TESTNET_PORT", 18567);;
        nPruneAfterHeight = 1000;

        if (gArgs.IsArgSet("-mine-genesis-block"))
        {
            genesis = CreateGenesisBlock(
                CHAINPARAMS_UINT32("TESTNET_GENESIS_BLOCK_UNIX_TIMESTAMP", TESTNET_GENESIS_BLOCK_UNIX_TIMESTAMP),
                0,
                CHAINPARAMS_XUINT32("TESTNET_GENESIS_BLOCK_NBITS", TESTNET_GENESIS_BLOCK_NBITS),
                4,
                CHAINPARAMS_UINT32("GENESIS_BLOCK_REWARD", GENESIS_BLOCK_REWARD) * COIN,
                CHAINPARAMS_STR("TESTNET_GENESIS_BLOCK_SIGNATURE", TESTNET_GENESIS_BLOCK_SIGNATURE)
            );

            arith_uint256 bnProofOfWorkLimit(~arith_uint256() >> CHAINPARAMS_UINT32("TESTNET_GENESIS_BLOCK_POW_BITS", TESTNET_GENESIS_BLOCK_POW_BITS));

        LogPrintf("Recalculating params for testnet.\n");
        LogPrintf("- old testnet genesis nonce: %u\n", genesis.nNonce);
        LogPrintf("- old testnet genesis hash:  %s\n", genesis.GetHash().ToString().c_str());
        LogPrintf("- old testnet genesis merkle root: %s\n", genesis.hashMerkleRoot.ToString().c_str());

            for (genesis.nNonce = 0; UintToArith256(genesis.GetHash()) > bnProofOfWorkLimit; genesis.nNonce++) { }

        LogPrintf("- new testnet genesis nonce: %u\n", genesis.nNonce);
        LogPrintf("- new testnet genesis hash: %s\n", genesis.GetHash().ToString().c_str());
        LogPrintf("- new testnet genesis merkle root: %s\n", genesis.hashMerkleRoot.ToString().c_str());

        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.BIP34Hash = consensus.hashGenesisBlock;

        LogPrintf("- new testnet genesis block: %s\n", genesis.ToString().c_str());

            SaveGenesisParams("TESTNET", genesis);
        }
        else {
            LoadGenesisParams();
            genesis = CreateGenesisBlock(
                CHAINPARAMS_UINT32("TESTNET_GENESIS_BLOCK_UNIX_TIMESTAMP", TESTNET_GENESIS_BLOCK_UNIX_TIMESTAMP),
                GENESIS_UINT32("TESTNET_GENESIS_BLOCK_NONCE", TESTNET_GENESIS_BLOCK_NONCE),
                CHAINPARAMS_XUINT32("TESTNET_GENESIS_BLOCK_NBITS", TESTNET_GENESIS_BLOCK_NBITS),
                4,
                CHAINPARAMS_UINT32("GENESIS_BLOCK_REWARD", GENESIS_BLOCK_REWARD) * COIN,
                CHAINPARAMS_STR("TESTNET_GENESIS_BLOCK_SIGNATURE", TESTNET_GENESIS_BLOCK_SIGNATURE)
            );

        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.BIP34Hash = consensus.hashGenesisBlock;

            assert(consensus.hashGenesisBlock == GENESIS_UINT256("TESTNET_CONSENSUS_HASH_GENESIS_BLOCK", TESTNET_CONSENSUS_HASH_GENESIS_BLOCK));
            assert(genesis.hashMerkleRoot == GENESIS_UINT256("TESTNET_GENESIS_HASH_MERKLE_ROOT", TESTNET_GENESIS_HASH_MERKLE_ROOT));
        }

        vFixedSeeds.clear();
        vSeeds.clear();

        // nodes with support for servicebits filtering should be at the top
        for (int index = 0; index < 3; ++index)
        {
            char buf[20];
            snprintf(buf, sizeof(buf), "TESTNET_SEED_%d", index);
            std::string seed = CHAINPARAMS_STR(buf, "");
            if (!seed.empty())
            {
                vSeeds.emplace_back(seed, false);
            }
        }

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        // same as for the CRegTestParams
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,CHAINPARAMS_UINT32("TESTNET_PUBKEY_ADDRESS", 51));
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,CHAINPARAMS_UINT32("TESTNET_SCRIPT_ADDRESS", 180));
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,CHAINPARAMS_UINT32("TESTNET_SECRET_KEY", 226));
        base58Prefixes[EXT_PUBLIC_KEY] = {0x03, 0xE3, 0xC5, 0x26};  // ptpu
        base58Prefixes[EXT_SECRET_KEY] = {0x03, 0xE3, 0xC5, 0x2D};  // ptpv

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;

#ifndef PAI_BABY
        checkpointData = (CCheckpointData) {
            {
                {    0, TESTNET_CONSENSUS_HASH_GENESIS_BLOCK },
                {    1, uint256S("0x0000000007f33c46116ced43fbb7eb0307080ab7071c134e4b9ccd1334c61177")},
                { 1000, uint256S("0x0000000008668e5c597a6f0a97c3aced17389a8bd842afe61dd2310b4f301c9a")},
                { 2500, uint256S("0x00000000057ba272b77e932a86748252e69ef3bb77ae1756787d2e4240167a4b")}
            }
        };

        chainTxData = ChainTxData{
            // tx hash = 0xd714e38737c9a4f2f0f59bdd6ffa6e527a6874a7f599849dfe042c8bd1f49ce5
            // block hash = 0x00000000057ba272b77e932a86748252e69ef3bb77ae1756787d2e4240167a4b
            // block index = 2500
            // tx index = 3434
            // tx timestamp = 1523601213

            1523601213, // * UNIX timestamp of last known number of transactions
            3434,       // * total number of transactions between genesis and that timestamp
                        //   (the tx=... number in the SetBestChain debug.log lines)
            0.00052     // * estimated number of transactions per second after that timestamp
        };
#else

        checkpointData = (CCheckpointData) {
            {
                { 0, GENESIS_UINT256("TESTNET_CONSENSUS_HASH_GENESIS_BLOCK", TESTNET_CONSENSUS_HASH_GENESIS_BLOCK) }
            }
        };

        chainTxData = ChainTxData{
            CHAINPARAMS_UINT32("TESTNET_GENESIS_BLOCK_UNIX_TIMESTAMP", TESTNET_GENESIS_BLOCK_UNIX_TIMESTAMP), // * UNIX timestamp of last known number of transactions
            0,          // * total number of transactions between genesis and that timestamp
                        //   (the tx=... number in the SetBestChain debug.log lines)
            3.1         // * estimated number of transactions per second after that timestamp
        };
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
        consensus.nTotalBlockSubsidy = 1500;
        consensus.nWorkSubsidyProportion = 4;
        consensus.nStakeSubsidyProportion = 6;

        // NOTE PAICOIN Do not mofify the BIP settings, otherwise the current txvalidationcache_tests will fail
        consensus.BIP34Height = 100000000; // BIP34 has not activated on regtest (far in the future so block v1 are not rejected in tests)
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1351; // BIP65 activated on regtest (Used in rpc activation tests)
        consensus.BIP66Height = 1251; // BIP66 activated on regtest (Used in rpc activation tests)
        consensus.powLimit = CHAINPARAMS_UINT256("REGTEST_CONSENSUS_POW_LIMIT", REGTEST_CONSENSUS_POW_LIMIT);
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = CHAINPARAMS_UINT32("BLOCK_TIME", 10 * 60);
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

        // hashing algorithm parameters
        consensus.nPaicoinHashTimestamp = 0LL;
        consensus.nPaicoinHashMaximumActivationHeight = 2100;
        consensus.nPaicoinHashInitialDifficulty = REGTEST_PAICOIN_HASH_INITIAL_DIFFICULTY;
        consensus.nPaicoinHashInitialDifficultyBlockCount = 10;

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

        nDefaultPort = CHAINPARAMS_UINT32("REGTEST_PORT", 19567);
        nPruneAfterHeight = 1000;

        if (gArgs.IsArgSet("-mine-genesis-block"))
        {
            genesis = CreateGenesisBlock(
                CHAINPARAMS_UINT32("REGTEST_GENESIS_BLOCK_UNIX_TIMESTAMP", REGTEST_GENESIS_BLOCK_UNIX_TIMESTAMP),
                0,
                CHAINPARAMS_XUINT32("REGTEST_GENESIS_BLOCK_NBITS", REGTEST_GENESIS_BLOCK_NBITS),
                4,
                CHAINPARAMS_UINT32("GENESIS_BLOCK_REWARD", GENESIS_BLOCK_REWARD) * COIN,
                CHAINPARAMS_STR("REGTEST_GENESIS_BLOCK_SIGNATURE", REGTEST_GENESIS_BLOCK_SIGNATURE)
            );

            arith_uint256 bnProofOfWorkLimit(~arith_uint256() >> CHAINPARAMS_UINT32("REGTEST_GENESIS_BLOCK_POW_BITS", REGTEST_GENESIS_BLOCK_POW_BITS));

        LogPrintf("Recalculating params for regtest.\n");
        LogPrintf("- old regtest genesis nonce: %u\n", genesis.nNonce);
        LogPrintf("- old regtest genesis hash:  %s\n", genesis.GetHash().ToString().c_str());
        LogPrintf("- old regtest genesis merkle root: %s\n", genesis.hashMerkleRoot.ToString().c_str());

            for (genesis.nNonce = 0; UintToArith256(genesis.GetHash()) > bnProofOfWorkLimit; genesis.nNonce++) { } 

        LogPrintf("- new regtest genesis nonce: %u\n", genesis.nNonce);
        LogPrintf("- new regtest genesis hash: %s\n", genesis.GetHash().ToString().c_str());
        LogPrintf("- new regtest genesis merkle root: %s\n", genesis.hashMerkleRoot.ToString().c_str());

        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.BIP34Hash = consensus.hashGenesisBlock;

        LogPrintf("- new regtest genesis block: %s\n", genesis.ToString().c_str());

            SaveGenesisParams("REGTEST", genesis);
        }
        else {
            LoadGenesisParams();
            // TODO: Update the values below with the nonce from the above mining for the genesis block
            //       This should only be done once, after the mining and prior to production release
            genesis = CreateGenesisBlock(
                CHAINPARAMS_UINT32("REGTEST_GENESIS_BLOCK_UNIX_TIMESTAMP", REGTEST_GENESIS_BLOCK_UNIX_TIMESTAMP),
                GENESIS_UINT32("REGTEST_GENESIS_BLOCK_NONCE", REGTEST_GENESIS_BLOCK_NONCE),
                CHAINPARAMS_XUINT32("REGTEST_GENESIS_BLOCK_NBITS", REGTEST_GENESIS_BLOCK_NBITS),
                4,
                CHAINPARAMS_UINT32("GENESIS_BLOCK_REWARD", GENESIS_BLOCK_REWARD) * COIN,
                CHAINPARAMS_STR("REGTEST_GENESIS_BLOCK_SIGNATURE", REGTEST_GENESIS_BLOCK_SIGNATURE)
            );

        consensus.hashGenesisBlock = genesis.GetHash();
        consensus.BIP34Hash = consensus.hashGenesisBlock;

            // TODO: Update the values below with the data from the above mining for the genesis block
            //       This should only be done once, after the mining and prior to production release
            assert(consensus.hashGenesisBlock == GENESIS_UINT256("REGTEST_CONSENSUS_HASH_GENESIS_BLOCK", REGTEST_CONSENSUS_HASH_GENESIS_BLOCK));
            assert(genesis.hashMerkleRoot == GENESIS_UINT256("REGTEST_GENESIS_HASH_MERKLE_ROOT", REGTEST_GENESIS_HASH_MERKLE_ROOT));
        }

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;

        // same as for the CTestNetParams
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,CHAINPARAMS_UINT32("REGTEST_PUBKEY_ADDRESS", 51));
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,CHAINPARAMS_UINT32("REGTEST_SCRIPT_ADDRESS", 180));
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,CHAINPARAMS_UINT32("REGTEST_SECRET_KEY", 226));
        base58Prefixes[EXT_PUBLIC_KEY] = {0x03, 0xE3, 0xC5, 0x26};  // ptpu
        base58Prefixes[EXT_SECRET_KEY] = {0x03, 0xE3, 0xC5, 0x2D};  // ptpv

        checkpointData = (CCheckpointData) {
            {
                {0, GENESIS_UINT256("REGTEST_CONSENSUS_HASH_GENESIS_BLOCK", REGTEST_CONSENSUS_HASH_GENESIS_BLOCK) },
            }
        };

        chainTxData = ChainTxData{
            CHAINPARAMS_UINT32("REGTEST_GENESIS_BLOCK_UNIX_TIMESTAMP", REGTEST_GENESIS_BLOCK_UNIX_TIMESTAMP),
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
