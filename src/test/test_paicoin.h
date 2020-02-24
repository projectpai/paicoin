// Copyright (c) 2015-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAICOIN_TEST_TEST_PAICOIN_H
#define PAICOIN_TEST_TEST_PAICOIN_H

#include "chainparamsbase.h"
#include "fs.h"
#include "key.h"
#include "pubkey.h"
#include "random.h"
#include "scheduler.h"
#include "txdb.h"
#include "txmempool.h"
#include "script/standard.h"
#include <consensus/validation.h>
#include <validationinterface.h>

#include <boost/thread.hpp>

extern uint256 insecure_rand_seed;
extern FastRandomContext insecure_rand_ctx;

static inline void SeedInsecureRand(bool fDeterministic = false)
{
    if (fDeterministic) {
        insecure_rand_seed = uint256();
    } else {
        insecure_rand_seed = GetRandHash();
    }
    insecure_rand_ctx = FastRandomContext(insecure_rand_seed);
}

static inline uint32_t InsecureRand32() { return insecure_rand_ctx.rand32(); }
static inline uint256 InsecureRand256() { return insecure_rand_ctx.rand256(); }
static inline uint64_t InsecureRandBits(int bits) { return insecure_rand_ctx.randbits(bits); }
static inline uint64_t InsecureRandRange(uint64_t range) { return insecure_rand_ctx.randrange(range); }
static inline bool InsecureRandBool() { return insecure_rand_ctx.randbool(); }

/** Basic testing setup.
 * This just configures logging and chain parameters.
 */
struct BasicTestingSetup {
    ECCVerifyHandle globalVerifyHandle;

    explicit BasicTestingSetup(const std::string& chainName = CBaseChainParams::MAIN);
    ~BasicTestingSetup();
};

/** Testing setup that configures a complete environment.
 * Included are data directory, coins database, script check threads setup.
 */
class CConnman;
class PeerLogicValidation;
struct TestingSetup: public BasicTestingSetup {
    CCoinsViewDB *pcoinsdbview;
    fs::path pathTemp;
    boost::thread_group threadGroup;
    CConnman* connman;
    CScheduler scheduler;
    std::unique_ptr<PeerLogicValidation> peerLogic;

    explicit TestingSetup(const std::string& chainName = CBaseChainParams::MAIN);
    ~TestingSetup();
};

class CBlock;
struct CMutableTransaction;
class CScript;

enum class scriptPubKeyType{
    NoKey,
    P2PK,
    P2PKH,
};
//
// Testing fixture that pre-creates a
// 100-block REGTEST-mode block chain
//
struct TestChain100Setup : public TestingSetup {
    TestChain100Setup(scriptPubKeyType pkType = scriptPubKeyType::P2PK);

    // Create a new block with just given transactions, coinbase paying to
    // scriptPubKey, and try to add it to the current chain.
    CBlock CreateAndProcessBlock(const std::vector<CMutableTransaction>& txns,
                                 const CScript& scriptPubKey);

    ~TestChain100Setup();

    std::vector<CTransaction> coinbaseTxns; // For convenience, coinbase transactions
    CKey coinbaseKey; // private/public key needed to spend coinbase transactions
};

class Generator: public TestingSetup
{
public:
    explicit Generator(const std::string& chainName = CBaseChainParams::REGTEST);
    ~Generator();

    // voteBitNo and voteBitYes represent no and yes votes, respectively, on
    // whether or not to approve the previous block.
    static const uint32_t voteNoBits  = 0x0000;
    static const uint32_t voteYesBits = 0x0001;

    static const uint16_t minStakebaseScriptLen = 2;
    static const uint16_t maxStakebaseScriptLen = 100;

    struct SpendableOut {
        COutPoint prevOut;
        int       blockHeight;
        // uint32_t  blockIndex;
        CAmount   amount;
    };

    class submitblock_StateCatcher : public CValidationInterface
    {
    public:
        uint256 hash;
        bool found;
        CValidationState state;

        explicit submitblock_StateCatcher(const uint256 &hashIn) : hash{hashIn}, found{false} {}

    protected:
        void BlockChecked(const CBlock& block, const CValidationState& stateIn) override {
            if (block.GetHash() != hash)
                return;
            found = true;
            state = stateIn;
        }
    };

    // struct StakeTicket {
    //     CTransactionRef tx;
    //     uint32_t  blockHeight;
    //     uint32_t  blockIndex;
    // };
    typedef std::function<void(CBlock&)> MungerType;

    CBlock NextBlock(const std::string& blockName
                    , const SpendableOut* spend
                    , const std::list<SpendableOut>& ticketSpends
                    , const MungerType& munger = MungerType());
    CMutableTransaction CreateTicketPurchaseTx(const SpendableOut& spend, const CAmount& ticketPrice, const CAmount& fee);
    CMutableTransaction CreateVoteTx(const uint256& voteBlockHash, int voteBlockHeight, const uint256& ticketTxHash, uint32_t voteBits = voteYesBits) const;
    CMutableTransaction CreateRevocationTx(const uint256& ticketTxHash) const;
    CMutableTransaction CreateSpendTx(const SpendableOut& spend, const CAmount& fee) const;
    CMutableTransaction CreateSplitSpendTx(const SpendableOut& spend, const std::vector<CAmount>& payments, const CAmount& fee) const;
    
    const CBlockIndex* Tip() const;
    const Consensus::Params& ConsensusParams() const;
    CAmount NextRequiredStakeDifficulty() const;
    const CValidationState& LastValidationState() const { return lastValidationState;}

    SpendableOut MakeSpendableOut(const CTransaction& tx, uint32_t indexOut) const;
    void SaveAllSpendableOuts(const CBlock& b);
    void SaveSpendableOuts(const CBlock& b, uint32_t indexBlock, const std::vector<uint32_t>& indicesTxOut);
    void SaveCoinbaseOut(const CBlock& b);

    void ReplaceVoteBits(CTransactionRef& tx, uint32_t voteBits) const;
    void ReplaceStakeBaseSigScript(CTransactionRef& tx, const CScript& sigScript) const;
    CScript RepeatOpCode(opcodetype opCode, uint16_t numRepeats) const;

    std::list<SpendableOut> OldestCoinOuts();

private:
    void SignTx(CMutableTransaction& tx, unsigned int nIn, const CScript& script, const CKey& key) const;

    // keep keys, addr and scripts as references to the coinbase ones to be
    // in case we need to make separate ones
    const CKey  coinbaseKey;
    const CKey& stakeKey;
    const CKey& rewardKey;
    const CKey& changeKey;

    const CKeyID  coinbaseAddr;
    const CKeyID& stakeAddr;
    const CKeyID& rewardAddr;
    const CKeyID& changeAddr;

    const CScript  coinbaseScript;
    const CScript& stakeScript;
    const CScript& rewardScript;
    const CScript& changeScript;

    std::string tipName;
    // std::map<uint256,CBlock&> blocks;//           map[chainhash.Hash]*wire.MsgBlock
    // std::map<uint256,uint32_t> blockHeights;//     map[chainhash.Hash]uint32
    // std::map<std::string,CBlock&> blocksByName;//     map[string]*wire.MsgBlock

    // Used for tracking spendable coinbase outputs.
    std::list<std::list<SpendableOut>> spendableOuts;//     [][]SpendableOut
    // prevCollectedHash chainhash.Hash

    // // Used for tracking the live ticket pool and revocations.
    // originalParents map[chainhash.Hash]chainhash.Hash
    // std::vector<CTransactionRef> immatureTickets;
    // std::vector<CTransactionRef> liveTickets;
    // std::vector<CTransactionRef> expiredTickets;
    // std::map<uint256, std::vector<StakeTicket>> wonTickets;//      map[chainhash.Hash][]*stakeTicket
    // std::map<uint256, std::vector<StakeTicket>> revokedTickets;//  map[chainhash.Hash][]*stakeTicket
    // missedVotes     map[chainhash.Hash]*stakeTicket

    std::map<uint256, CAmount> boughtTicketHashToPrice;

    CValidationState lastValidationState;
};

class CTxMemPoolEntry;

struct TestMemPoolEntryHelper
{
    // Default values
    CAmount nFee;
    int64_t nTime;
    unsigned int nHeight;
    bool spendsCoinbase;
    unsigned int sigOpCost;
    LockPoints lp;

    TestMemPoolEntryHelper() :
        nFee(0), nTime(0), nHeight(1),
        spendsCoinbase(false), sigOpCost(4) { }

    CTxMemPoolEntry FromTx(const CMutableTransaction &tx);
    CTxMemPoolEntry FromTx(const CTransaction &tx);

    // Change the default value
    TestMemPoolEntryHelper &Fee(CAmount _fee) { nFee = _fee; return *this; }
    TestMemPoolEntryHelper &Time(int64_t _time) { nTime = _time; return *this; }
    TestMemPoolEntryHelper &Height(unsigned int _height) { nHeight = _height; return *this; }
    TestMemPoolEntryHelper &SpendsCoinbase(bool _flag) { spendsCoinbase = _flag; return *this; }
    TestMemPoolEntryHelper &SigOpsCost(unsigned int _sigopsCost) { sigOpCost = _sigopsCost; return *this; }
};

CBlock getBlock13b8a();

#endif
