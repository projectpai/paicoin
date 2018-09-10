#include "wallet/wallettxqueue.h"

int CWalletTxQueue::Comparator::Compare(const leveldb::Slice& a, const leveldb::Slice& b) const
{
    unsigned long int ak = *reinterpret_cast<const unsigned long int*>(a.data());
    unsigned long int bk = *reinterpret_cast<const unsigned long int*>(b.data());
    return ak - bk;
}

const char* CWalletTxQueue::Comparator::Name() const
{
    return "CWalletTxQueue.Comparator";
}

void CWalletTxQueue::Comparator::FindShortestSeparator(std::string* start, const leveldb::Slice& limit) const
{
}

void CWalletTxQueue::Comparator::FindShortSuccessor(std::string* key) const
{
}

CWalletTxQueue::CWalletTxQueue(const fs::path& path):
    comparator(),
    readOptions(),
    writeOptions(),
    cache(leveldb::NewLRUCache(cacheSize / 2)),
    db(openDb(path.string())),
    idx(getLastIdx())
{
}

leveldb::DB* CWalletTxQueue::openDb(const std::string& path)
{
    leveldb::Options options;
    options.block_cache = cache.get();
    options.write_buffer_size = cacheSize / 4;
    options.compression = leveldb::kNoCompression;
    options.max_open_files = 64;
    options.create_if_missing = true;
    options.comparator = &comparator;
    leveldb::DB* db = nullptr;
    HandleStatus(leveldb::DB::Open(options, path, &db));
    return db;
}

void CWalletTxQueue::put(const std::string& txid)
{
    HandleStatus(db->Put(writeOptions, newKey(), txid));
}

std::vector<std::string> CWalletTxQueue::take(unsigned int count)
{
    leveldb::WriteBatch batch;
    std::vector<std::string> values;
    values.reserve(count);
    std::unique_ptr<leveldb::Iterator> iterator(db->NewIterator(readOptions));
    for (iterator->SeekToFirst(); count != 0 && iterator->Valid(); iterator->Next(), --count)
    {
        values.push_back(iterator->value().ToString());
        batch.Delete(iterator->key());
    }
    if (!values.empty())
        HandleStatus(db->Write(writeOptions, &batch));
    return values;
}

void CWalletTxQueue::compact()
{
    db->CompactRange(nullptr, nullptr);
}

void CWalletTxQueue::HandleStatus(const leveldb::Status& status)
{
    if (status.ok())
        return;
    if (status.IsCorruption())
        throw std::runtime_error("Database corrupted");
    if (status.IsIOError())
        throw std::runtime_error("Database I/O error");
    if (status.IsNotFound())
        throw std::runtime_error("Database entry missing");
    throw std::runtime_error("Unknown database error");
}

leveldb::Slice CWalletTxQueue::newKey()
{
    ++idx;
    return leveldb::Slice(reinterpret_cast<char*>(&idx), sizeof(idx));
}

unsigned long int CWalletTxQueue::getLastIdx()
{
    std::unique_ptr<leveldb::Iterator> iterator(db->NewIterator(readOptions));
    iterator->SeekToLast();
    if (!iterator->Valid())
        return 0;
    return *reinterpret_cast<const unsigned long int*>(iterator->key().data());
}
