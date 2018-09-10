#include <leveldb/db.h>
#include <leveldb/cache.h>
#include <leveldb/comparator.h>
#include <leveldb/options.h>
#include <leveldb/slice.h>
#include <leveldb/write_batch.h>

#include "fs.h"


class CWalletTxQueue final
{
    class Comparator final: public leveldb::Comparator
    {
    public:
        int Compare(const leveldb::Slice& a, const leveldb::Slice& b) const;

        const char* Name() const;

        void FindShortestSeparator(std::string* start, const leveldb::Slice& limit) const;

        void FindShortSuccessor(std::string* key) const;
    };

public:
    CWalletTxQueue(const fs::path& path);

private:
    CWalletTxQueue(const CWalletTxQueue&) = delete;
    const CWalletTxQueue& operator= (const CWalletTxQueue&) = delete;

public:
    void put(const std::string& txid);

    std::vector<std::string> take(uint count);

    void compact();

private:
    leveldb::Slice newKey();

    unsigned long int getLastIdx();

    leveldb::DB* openDb(const std::string& path);

    static void HandleStatus(const leveldb::Status& status);

private:
    static const int cacheSize = 1024 * 1024;

    const Comparator comparator;

    const leveldb::ReadOptions readOptions;

    const leveldb::WriteOptions writeOptions;

    std::unique_ptr<leveldb::Cache> cache;

    std::unique_ptr<leveldb::DB> db;

    unsigned long int idx;
};
