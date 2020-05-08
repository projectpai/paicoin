// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAICOIN_PRIMITIVES_BLOCK_H
#define PAICOIN_PRIMITIVES_BLOCK_H

#include "primitives/transaction.h"
#include "serialize.h"
#include "uint256.h"
#include "stake/votebits.h"

#include <array>

static const int HARDFORK_VERSION_BIT = 0x80000000;
/** Nodes collect new transactions into a block, hash them into a hash tree,
 * and scan through nonce values to make the block's hash satisfy proof-of-work
 * requirements.  When they solve the proof-of-work, they broadcast the block
 * to everyone and the block is added to the block chain.  The first transaction
 * in the block is a special one that creates a new coin owned by the creator
 * of the block.
 */
class CBlockHeader
{
public:
    // header
    int32_t    nVersion;
    uint256    hashPrevBlock;
    uint256    hashMerkleRoot;
    uint32_t   nTime;
    uint32_t   nBits;
    uint32_t   nNonce;
    int64_t    nStakeDifficulty;
    VoteBits   nVoteBits;
    uint32_t   nTicketPoolSize;
    uint48     ticketLotteryState;
    uint16_t   nVoters;
    uint8_t    nFreshStake;
    uint8_t    nRevocations;
    int8_t     extraData[32];
    uint32_t   nStakeVersion;

    CBlockHeader()
    {
        SetNull();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(this->nVersion);
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);
        // serialization of stake fields activates when Hybrid PoW/PoS deploys
        if (this->nVersion & HARDFORK_VERSION_BIT) {
            READWRITE(nStakeDifficulty);
            READWRITE(nVoteBits);
            READWRITE(nTicketPoolSize);
            READWRITE(ticketLotteryState);
            READWRITE(nVoters);
            READWRITE(nFreshStake);
            READWRITE(nRevocations);
            READWRITE(FLATDATA(extraData));
            READWRITE(nStakeVersion);
        }
        else if (ser_action.ForRead())
        {
           SetReadStakeDefaultBeforeFork(); 
        }
    }

    void SetReadStakeDefaultBeforeFork();

    void SetNull()
    {
        nVersion = 0;
        hashPrevBlock.SetNull();
        hashMerkleRoot.SetNull();
        nTime = 0;
        nBits = 0;
        nNonce = 0;
        nStakeDifficulty = 0;
        nVoteBits = VoteBits(VoteBits::Rtt, true);
        nTicketPoolSize = 0;
        ticketLotteryState.SetNull();
        nVoters = 0;
        nFreshStake = 0;
        nRevocations = 0;
        std::fill(std::begin(extraData), std::end(extraData), 0);
        nStakeVersion = 0;
    }

    bool IsNull() const
    {
        return (nBits == 0);
    }

    uint256 GetHash() const;

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }
};


class CBlock : public CBlockHeader
{
public:
    // network and disk
    std::vector<CTransactionRef> vtx;

    // memory only
    mutable bool fChecked;

    CBlock()
    {
        SetNull();
    }

    CBlock(const CBlockHeader &header)
    {
        SetNull();
        *(static_cast<CBlockHeader*>(this)) = header;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(*static_cast<CBlockHeader*>(this));
        READWRITE(vtx);
    }

    void SetNull()
    {
        CBlockHeader::SetNull();
        vtx.clear();
        fChecked = false;
    }

    CBlockHeader GetBlockHeader() const
    {
        CBlockHeader block;
        block.nVersion       = nVersion;
        block.hashPrevBlock  = hashPrevBlock;
        block.hashMerkleRoot = hashMerkleRoot;
        block.nTime          = nTime;
        block.nBits          = nBits;
        block.nNonce         = nNonce;
        block.nVoteBits      = nVoteBits;
        block.nStakeDifficulty = nStakeDifficulty;
        block.nTicketPoolSize = nTicketPoolSize;
        block.ticketLotteryState = ticketLotteryState;
        block.nFreshStake    = nFreshStake;
        block.nStakeVersion  = nStakeVersion;
        return block;
    }

    std::string ToString() const;
};

/** Describes a place in the block chain to another node such that if the
 * other node doesn't have the same branch, it can find a recent common trunk.
 * The further back it is, the further before the fork it may be.
 */
struct CBlockLocator
{
    std::vector<uint256> vHave;

    CBlockLocator() {}

    explicit CBlockLocator(const std::vector<uint256>& vHaveIn) : vHave(vHaveIn) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vHave);
    }

    void SetNull()
    {
        vHave.clear();
    }

    bool IsNull() const
    {
        return vHave.empty();
    }
};

#endif // PAICOIN_PRIMITIVES_BLOCK_H
