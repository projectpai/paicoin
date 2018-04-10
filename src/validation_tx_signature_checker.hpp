// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SIGNATURE_CHECKER_HPP
#define SIGNATURE_CHECKER_HPP

#include "script/drivechain.h"

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>


class CachingTransactionSignatureCheckerWithBlockReader : public CachingTransactionSignatureChecker, public BaseBlockReader
{
    typedef std::pair<uint256, CTransaction> CoinbaseCacheItem;
    typedef boost::multi_index_container<
        CoinbaseCacheItem,
        boost::multi_index::indexed_by<
            boost::multi_index::sequenced<>,
            boost::multi_index::ordered_unique<
                boost::multi_index::member<CoinbaseCacheItem, uint256, &CoinbaseCacheItem::first>
            >
        >
    > CoinbaseCacheContainer;

    int nHeight;
    uint256 hash;

    mutable boost::mutex mutexCache;
    mutable CoinbaseCacheContainer cacheCoinbase; // block hash -> block coinbase

    void UpdateCache(const CoinbaseCacheItem& coinbase) const
    {
        boost::lock_guard<boost::mutex> lock(mutexCache);
        std::pair<CoinbaseCacheContainer::iterator, bool> result = cacheCoinbase.push_front(coinbase);
        if (!result.second) {
            cacheCoinbase.relocate(cacheCoinbase.begin(), result.first);
        } else if (cacheCoinbase.size() > MAX_COUNT_ACKS_CACHE) {
            cacheCoinbase.pop_back();
        }
    }

    CTransaction ReadBlockCoinbase(CBlockIndex* pblockindex) const
    {
        CBlock block;

        if (fHavePruned && !(pblockindex->nStatus & BLOCK_HAVE_DATA) && pblockindex->nTx > 0)
            return CTransaction();

        if (!ReadBlockFromDisk(block, pblockindex, Params().GetConsensus()))
            return CTransaction();

        if (block.vtx.empty())
            return CTransaction();

        const CTransaction& firstTx = *(block.vtx[0]);
        if (!firstTx.IsCoinBase())
            return CTransaction();

        return firstTx;
    }

public:
    const unsigned int MAX_COUNT_ACKS_CACHE = 1000;

public:
    virtual int GetBlockNumber() const
    {
        return nHeight;
    }

    virtual CTransaction GetBlockCoinbase(int blockNumber) const
    {
        //AssertLockHeld(cs_main);

        int nHeight = blockNumber;
        if (nHeight < 0 || nHeight > chainActive.Height())
            return CTransaction();

        CBlockIndex* pblockindex = chainActive[nHeight];

        {
            boost::lock_guard<boost::mutex> lock(mutexCache);
            CoinbaseCacheContainer::nth_index<1>::type::iterator it = cacheCoinbase.get<1>().find(*pblockindex->phashBlock);
            if (it != cacheCoinbase.get<1>().end())
                return it->second;
        }

        CTransaction coinbase = ReadBlockCoinbase(pblockindex);
        if (!coinbase.IsNull())
            UpdateCache(std::make_pair(*pblockindex->phashBlock, coinbase));

        return coinbase;
    }

    virtual bool CountAcks(const std::vector<unsigned char>& chainId, int periodAck, int periodLiveness, int& positiveAcks, int& negativeAcks) const
    {
        std::vector<unsigned char> hashSpend(hash.begin(), hash.end());
        return ::CountAcks(hashSpend, chainId, periodAck, periodLiveness, positiveAcks, negativeAcks, *this);
    }

    CachingTransactionSignatureCheckerWithBlockReader(const CTransaction* txToIn, unsigned int nInIn, const CAmount& amount, bool storeIn, PrecomputedTransactionData& txdataIn, int height)
        : CachingTransactionSignatureChecker(txToIn, nInIn, amount, storeIn, txdataIn), nHeight(height), hash(txToIn->GetHash())
    {
    }
};

#endif // SIGNATURE_CHECKER_HPP
