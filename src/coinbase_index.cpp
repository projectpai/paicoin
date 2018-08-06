#include "coinbase_index.h"
#include "base58.h"
#include "chainparams.h"
#include "clientversion.h"
#include "coinbase_addresses.h"
#include "coinbase_keyhandler.h"
#include "coinbase_txhandler.h"
#include "util.h"

CoinbaseIndex gCoinbaseIndex;
CoinbaseIndexCache gCoinbaseIndexCache;

void CoinbaseIndex::BuildDefault()
{
    SetNull();

    for (auto const& fAddrToHeight : fCoinbaseAddrs) {
        auto const& fAddr = fAddrToHeight.first;
        auto const& height = fAddrToHeight.second;

        CoinbaseAddress coinbaseAddr{fAddr, CPubKey(), true, height};
        AddNewAddress(coinbaseAddr, Params().GetConsensus().hashGenesisBlock);
    }
}

void CoinbaseIndex::BuildDefaultFromDisk()
{
    SetNull();

    auto keyLoader = CoinbaseKeyHandler(GetDataDir());
    auto pubKeys = keyLoader.GetCoinbasePublicKeys();
    if (pubKeys.empty()) {
        return;
    }

    for (auto const& pubKey : pubKeys) {
        auto pubKeyAsAddr = EncodeDestination(pubKey.GetID());

        CoinbaseAddress coinbaseAddr{pubKeyAsAddr, pubKey, true, -1};
        AddNewAddress(coinbaseAddr, Params().GetConsensus().hashGenesisBlock);
    }
}

void CoinbaseIndex::SetNull()
{
    m_addrToCoinbaseAddr.clear();
    m_coinbaseBlockHashes.clear();
    m_coinbaseAddrs.clear();
}

bool CoinbaseIndex::IsNull() const
{
    return (m_addrToCoinbaseAddr.empty() &&
            m_coinbaseBlockHashes.empty() &&
            m_coinbaseAddrs.empty());
}

bool CoinbaseIndex::IsModified() const
{
    return m_modified;
}

void CoinbaseIndex::AddNewAddress(CoinbaseAddress addr, uint256 blockHash)
{
    /* The coinbase addr is already in the index, skip adding */
    auto existingCoinbaseAddr = GetCoinbaseWithAddr(addr.GetHashedAddr());
    if (existingCoinbaseAddr != nullptr) {
        return;
    }

    m_coinbaseAddrs.emplace_back(addr);
    m_coinbaseBlockHashes.emplace_back(blockHash);
    m_addrToCoinbaseAddr[addr.GetHashedAddr()] = m_coinbaseAddrs.size() - 1;

    m_modified = true;
}

const CoinbaseAddress* CoinbaseIndex::GetCoinbaseWithAddr(const std::string& addr) const
{
    auto foundCoinbase = m_addrToCoinbaseAddr.find(addr);
    if (foundCoinbase == m_addrToCoinbaseAddr.end()) {
        return nullptr;
    }

    BOOST_ASSERT(foundCoinbase->second < m_coinbaseAddrs.size());

    return &m_coinbaseAddrs[foundCoinbase->second];
}

uint256 CoinbaseIndex::GetBlockHashWithAddr(const std::string& addr) const
{
    auto foundCoinbase = m_addrToCoinbaseAddr.find(addr);
    if (foundCoinbase == m_addrToCoinbaseAddr.end()) {
        return uint256();
    }

    BOOST_ASSERT(foundCoinbase->second < m_coinbaseBlockHashes.size());

    return m_coinbaseBlockHashes[foundCoinbase->second];
}

void CoinbaseIndex::PruneAddrsWithBlocks(const BlockMap& blockMap)
{
    size_t numHashes = m_coinbaseBlockHashes.size();
    std::vector<size_t> indicesToKeep;
    indicesToKeep.reserve(numHashes); // worst case

    for (size_t hashIdx = 0; hashIdx < numHashes; ++hashIdx) {
        auto const& testHash = m_coinbaseBlockHashes[hashIdx];

        if (blockMap.find(testHash) != blockMap.end()) {
            indicesToKeep.push_back(hashIdx);
        }
    }

    if (indicesToKeep.size() == numHashes) {
        return;
    }

    size_t numFilteredItems = indicesToKeep.size();
    std::vector<CoinbaseAddress> filteredAddrs;
    std::vector<uint256> filteredHashes;
    filteredAddrs.reserve(numFilteredItems);
    filteredHashes.reserve(numFilteredItems);

    for (size_t keepIdx = 0; keepIdx < numFilteredItems; ++keepIdx) {
        auto addrIdx = indicesToKeep[keepIdx];

        filteredAddrs.emplace_back(std::move(m_coinbaseAddrs[addrIdx]));
        filteredHashes.emplace_back(std::move(m_coinbaseBlockHashes[addrIdx]));
    }

    SetNull();
    for (size_t itemIdx = 0; itemIdx < numFilteredItems; ++itemIdx) {
        AddNewAddress(std::move(filteredAddrs[itemIdx]), std::move(filteredHashes[itemIdx]));
    }
}

std::vector<CPubKey> CoinbaseIndex::GetDefaultCoinbaseKeys() const
{
    std::vector<CPubKey> keys;

    for (auto const& addr : m_coinbaseAddrs) {
        if (addr.IsDefault() && addr.m_pubKey.IsValid()) {
            keys.push_back(addr.m_pubKey);
        }
    }

    return keys;
}

size_t CoinbaseIndex::GetNumCoinbaseAddrs() const
{
    return m_coinbaseAddrs.size();
}

CAutoFile OpenDiskFile(fs::path path, bool fReadOnly)
{
    if (!fReadOnly) {
        fs::create_directories(path.parent_path());
    }

    FILE* file = fsbridge::fopen(path, "rb+");
    if (!file && !fReadOnly)
        file = fsbridge::fopen(path, "wb+");
    if (!file) {
        LogPrintf("Unable to open file %s\n", path.string());
        return CAutoFile(nullptr, SER_DISK, CLIENT_VERSION);
    }

    return CAutoFile(file, SER_DISK, CLIENT_VERSION);
}

bool CoinbaseIndexDisk::LoadFromDisk()
{
    CAutoFile indexFile = OpenIndexFile(true);
    if (indexFile.IsNull()) {
        // TODO: report here that the index does not exist
        return false;
    }

    indexFile >> *this;
    return true;
}

bool CoinbaseIndexDisk::SaveToDisk()
{
    CAutoFile indexFile = OpenIndexFile(false);
    if (indexFile.IsNull()) {
        // TODO: fail here, the index should be writable
        throw 1;
        return false;
    }

    indexFile << *this;
    return true;
}

CAutoFile CoinbaseIndexDisk::OpenIndexFile(bool fReadOnly)
{
    fs::path path = GetDataDir() / "coinbase" / "index";
    return OpenDiskFile(path, fReadOnly);
}

bool CoinbaseIndexCache::AddTransactionToCache(CTransactionRef txToCache)
{
    if (!txToCache) {
        return false;
    }

    if (!m_cachedTx) {
        m_cachedTx = std::move(txToCache);
        return true;
    }

    // verify if the previously cached tx is still valid
    CTransactionRef txSentinel(nullptr);
    uint256 hashBlock;
    auto txFound = GetTransaction(m_cachedTx->GetHash(), txSentinel, Params().GetConsensus(), hashBlock, true);
    if (!txFound) {
        // the old cached tx is no longer present, maybe it was evicted?
        m_cachedTx = std::move(txToCache);
        return true;
    } else {
        CoinbaseTxHandler txHandler;

        CoinbaseOperationType cachedOpType = COT_INVALID;
        CoinbaseOperationType candidateOpType = COT_INVALID;
        auto coinbaseAddr = txHandler.GetCoinbaseAddrFromTransactions(m_cachedTx, txToCache, gCoinbaseIndex, cachedOpType, candidateOpType);
        if (!coinbaseAddr) {
            return false;
        }

        if (candidateOpType == COT_ADD) {
            txFound = GetTransaction(txToCache->GetHash(), txSentinel, Params().GetConsensus(), hashBlock, true);
            if (!txFound) {
                return false;
            }
        }

        gCoinbaseIndex.AddNewAddress(*coinbaseAddr, std::move(hashBlock));

        m_cachedTx.reset();
        return true;
    }

    return false;
}

bool CoinbaseIndexCacheDisk::LoadFromDisk()
{
    CAutoFile indexFile = OpenCacheFile(true);
    if (indexFile.IsNull()) {
        // TODO: report here that the index does not exist
        return false;
    }

    indexFile >> *this;
    return true;
}

bool CoinbaseIndexCacheDisk::SaveToDisk()
{
    CAutoFile indexFile = OpenCacheFile(false);
    if (indexFile.IsNull()) {
        // TODO: fail here, the index should be writable
        throw 1;
        return false;
    }

    indexFile << *this;
    return true;
}

CAutoFile CoinbaseIndexCacheDisk::OpenCacheFile(bool fReadOnly)
{
    fs::path path = GetDataDir() / "coinbase" / "cache";
    return OpenDiskFile(path, fReadOnly);
}
