// Copyright (c) 2018 The PAIcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PAICOIN_COINBASE_INDEX_H
#define PAICOIN_COINBASE_INDEX_H

#include "coinbase_address.h"
#include "coinbase_utils.h"
#include "primitives/transaction.h"
#include "pubkey.h"
#include "serialize.h"
#include "streams.h"
#include "sync.h"
#include "uint256.h"
#include "util.h"
#include "validation.h"

#include <boost/optional.hpp>
#include <map>
#include <string>
#include <vector>

/**
 * Indexes all the possible coinbase addresses against which a node
 * could create coinbase transactions.
 */
class CoinbaseIndex
{
public:
    friend class CoinbaseIndexDisk;

    CoinbaseIndex() = default;
    ~CoinbaseIndex() = default;

    //! Build the index from legacy addresses, subject to removal in future releases
    void BuildDefault();

    //! Build the index from newer format with addresses on the disk
    void BuildDefaultFromDisk();

    void SetNull();
    bool IsNull() const;

    bool IsInitialized() const;
    void SetIsInitialized();

    void AddNewAddress(CoinbaseAddress addr, uint256 blockHash);
    boost::optional<CoinbaseAddress> GetCoinbaseWithAddr(const std::string& addr) const;
    uint256 GetBlockHashWithAddr(const std::string& addr) const;

    /**
     * Prunes the index of addresses which were introduced in some blocks which
     * are no longer present in the current chain.
     */
    void PruneAddrsWithBlocks(const BlockMap& blockMap);

    //! Loads up the default coinbase addresses from the disk
    std::vector<CPubKey> GetDefaultCoinbaseKeys() const;

    size_t GetNumCoinbaseAddrs() const;

    void ScanBlockForNewCoinbaseAddrTxs(const std::shared_ptr<const CBlock>& block);

private:
    bool AddNewCoinbaseAddressTransactionToIndex(CTransactionRef txToAdd, uint256 hashBlock);

private:
    std::vector<CoinbaseAddress> m_coinbaseAddrs;
    std::vector<uint256> m_coinbaseBlockHashes;
    std::map<std::string, size_t> m_addrToCoinbaseAddr;

    bool m_modified = false;
    bool m_isInitialized = false;

    mutable CCriticalSection m_lock;
};

class CoinbaseIndexDisk
{
public:
    explicit CoinbaseIndexDisk(CoinbaseIndex& wrappedIndex) : m_index(wrappedIndex) {}
    CoinbaseIndexDisk() = delete;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        if (ser_action.ForRead()) {
            m_index.SetNull();

            int numCoinbaseAddrs = 0;
            READWRITE(VARINT(numCoinbaseAddrs));

            for (auto readIdx = 0; readIdx < numCoinbaseAddrs; ++readIdx) {
                CoinbaseAddress coinbaseAddr;
                READWRITE(coinbaseAddr);

                uint256 coinbaseBlockHash;
                READWRITE(coinbaseBlockHash);

                auto blockIndexIt = mapBlockIndex.find(coinbaseBlockHash);
                if (blockIndexIt == mapBlockIndex.end()) {
                    CoinbaseIndexLog("%s: could not find block hash %s to serialize", __FUNCTION__, coinbaseBlockHash.ToString());
                    continue;
                }

                m_index.AddNewAddress(coinbaseAddr, coinbaseBlockHash);
            }
        } else {
            auto numCoinbaseAddrs = m_index.m_coinbaseAddrs.size();
            BOOST_ASSERT(numCoinbaseAddrs == m_index.m_coinbaseBlockHashes.size());

            READWRITE(VARINT(numCoinbaseAddrs));

            LOCK(m_index.m_lock);
            for (size_t addrIdx = 0; addrIdx < numCoinbaseAddrs; ++addrIdx) {
                CoinbaseAddress writeAddr = m_index.m_coinbaseAddrs[addrIdx];
                auto blockHash = m_index.m_coinbaseBlockHashes[addrIdx];

                READWRITE(writeAddr);
                READWRITE(blockHash);
            }
        }
    }

    bool LoadFromDisk();
    bool SaveToDisk();

private:
    CAutoFile OpenIndexFile(bool fReadOnly);

private:
    CoinbaseIndex& m_index;
};

extern CoinbaseIndex gCoinbaseIndex;

#endif // PAICOIN_COINBASE_INDEX_H
