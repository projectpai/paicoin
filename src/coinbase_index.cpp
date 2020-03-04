// Copyright (c) 2018 The PAIcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "coinbase_index.h"
#include "base58.h"
#include "chainparams.h"
#include "clientversion.h"
#include "coinbase_addresses.h"
#include "coinbase_keyhandler.h"
#include "coinbase_txhandler.h"
#include "coinbase_utils.h"
#include "key_io.h"
#include "util.h"

CoinbaseIndex gCoinbaseIndex;

void CoinbaseIndex::BuildDefault()
{
    SetNull();

    std::vector<CoinbaseAddress> newAddrs;
    newAddrs.reserve(fCoinbaseAddrs.size());

    for (auto const& fAddrToHeight : fCoinbaseAddrs) {
        auto const& fAddr = fAddrToHeight.first;
        auto const& height = fAddrToHeight.second;

        newAddrs.emplace_back(CoinbaseAddress{fAddr, CPubKey(), true, height});
    }

    if (newAddrs.empty()) {
        return;
    }

    LOCK(m_lock);
    for (auto&& addr : newAddrs) {
        AddNewAddress(addr, Params().GetConsensus().hashGenesisBlock);
    }
}

void CoinbaseIndex::BuildDefaultFromDisk()
{
    SetNull();

    auto keyLoader = CoinbaseIndexKeyHandler(GetDataDir());
    auto pubKeys = keyLoader.GetPublicKeys();
    if (pubKeys.empty()) {
        return;
    }

    std::vector<CoinbaseAddress> newAddrs;
    newAddrs.reserve(pubKeys.size());

    for (auto const& pubKey : pubKeys) {
        auto pubKeyAsAddr = EncodeDestination(pubKey.GetID());

        newAddrs.emplace_back(CoinbaseAddress{pubKeyAsAddr, pubKey, true, -1});
    }

    if (newAddrs.empty()) {
        return;
    }

    LOCK(m_lock);
    for (auto&& addr : newAddrs) {
        AddNewAddress(addr, Params().GetConsensus().hashGenesisBlock);
    }
}

void CoinbaseIndex::SetNull()
{
    LOCK(m_lock);
    m_addrToCoinbaseAddr.clear();
    m_coinbaseBlockHashes.clear();
    m_coinbaseAddrs.clear();
}

bool CoinbaseIndex::IsNull() const
{
    bool isNull = false;
    {
        LOCK(m_lock);
        isNull = (m_addrToCoinbaseAddr.empty() &&
            m_coinbaseBlockHashes.empty() &&
            m_coinbaseAddrs.empty());
    }

    return isNull;
}

bool CoinbaseIndex::IsInitialized() const
{
    bool isInitialized = false;
    {
        LOCK(m_lock);
        isInitialized = m_isInitialized;
    }

    return isInitialized;
}

void CoinbaseIndex::SetIsInitialized()
{
    LOCK(m_lock);
    m_isInitialized = true;
}

void CoinbaseIndex::AddNewAddress(CoinbaseAddress addr, uint256 blockHash)
{
    LOCK(m_lock);
    /* The coinbase addr is already in the index, skip adding */
    auto existingCoinbaseAddr = GetCoinbaseWithAddr(addr.GetHashedAddr());
    if (!!existingCoinbaseAddr) {
        return;
    }

    m_coinbaseAddrs.emplace_back(addr);
    m_coinbaseBlockHashes.emplace_back(blockHash);
    m_addrToCoinbaseAddr[addr.GetHashedAddr()] = m_coinbaseAddrs.size() - 1;

    m_modified = true;
}

boost::optional<CoinbaseAddress> CoinbaseIndex::GetCoinbaseWithAddr(const std::string& addr) const
{
    LOCK(m_lock);

    auto foundCoinbase = m_addrToCoinbaseAddr.find(addr);
    if (foundCoinbase == m_addrToCoinbaseAddr.end()) {
        return boost::optional<CoinbaseAddress>();
    }

    BOOST_ASSERT(foundCoinbase->second < m_coinbaseAddrs.size());
    return boost::make_optional(m_coinbaseAddrs[foundCoinbase->second]);
}

uint256 CoinbaseIndex::GetBlockHashWithAddr(const std::string& addr) const
{
    LOCK(m_lock);

    auto foundCoinbase = m_addrToCoinbaseAddr.find(addr);
    if (foundCoinbase == m_addrToCoinbaseAddr.end()) {
        return uint256();
    }

    BOOST_ASSERT(foundCoinbase->second < m_coinbaseBlockHashes.size());

    return m_coinbaseBlockHashes[foundCoinbase->second];
}

void CoinbaseIndex::PruneAddrsWithBlocks(const BlockMap& blockMap)
{
    LOCK(m_lock);

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

    LOCK(m_lock);
    for (auto const& addr : m_coinbaseAddrs) {
        if (addr.IsDefault() && addr.m_pubKey.IsValid()) {
            keys.push_back(addr.m_pubKey);
        }
    }

    return keys;
}

size_t CoinbaseIndex::GetNumCoinbaseAddrs() const
{
    size_t numAddrs = 0;
    {
        LOCK(m_lock);
        numAddrs = m_coinbaseAddrs.size();
    }

    return numAddrs;
}

void CoinbaseIndex::ScanBlockForNewCoinbaseAddrTxs(const std::shared_ptr<const CBlock>& block)
{
    if (!block) {
        return;
    }

    CoinbaseIndexTxHandler txHandler;
    for (auto& tx : block->vtx) {
        if (txHandler.HasTransactionNewCoinbaseAddress(tx)) {
            AddNewCoinbaseAddressTransactionToIndex(tx, block->GetHash());
        }
    }
}

bool CoinbaseIndex::AddNewCoinbaseAddressTransactionToIndex(CTransactionRef txToAdd, uint256 hashBlock)
{
    if (!txToAdd) {
        return false;
    }

    CoinbaseIndexTxHandler txHandler;
    auto coinbaseAddr = txHandler.ExtractNewCoinbaseAddrFromTransaction(txToAdd, *this);
    if (!coinbaseAddr) {
        return false;
    }

    if (hashBlock.IsNull()) {
        auto tip = chainActive.Tip();
        if (tip != nullptr) {
            hashBlock = *(tip->phashBlock);
        }
    }

    AddNewAddress(*coinbaseAddr, std::move(hashBlock));
    return true;
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
        CoinbaseIndexLog("Unable to open file %s\n", path.string());
        return CAutoFile(nullptr, SER_DISK, CLIENT_VERSION);
    }

    return CAutoFile(file, SER_DISK, CLIENT_VERSION);
}

bool CoinbaseIndexDisk::LoadFromDisk()
{
    CAutoFile indexFile = OpenIndexFile(true);
    if (indexFile.IsNull()) {
        CoinbaseIndexLog("%s: coinbase index does not (yet) exist", __FUNCTION__);
        return false;
    }

    indexFile >> *this;
    return true;
}

bool CoinbaseIndexDisk::SaveToDisk()
{
    CAutoFile indexFile = OpenIndexFile(false);
    if (indexFile.IsNull()) {
        CoinbaseIndexLog("%s: coinbase index could not be written to disk", __FUNCTION__);
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
