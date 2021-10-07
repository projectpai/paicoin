/* * Copyright (c) 2021 Project PAI Foundation
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#ifndef PAICOIN_COINBASE_DESTINATIONS_INDEX_H
#define PAICOIN_COINBASE_DESTINATIONS_INDEX_H

#include <string>
#include <vector>
#include <map>

#include <coinbase/coinbase_destination.h>
#include <fs.h>
#include <serialize.h>
#include <sync.h>

class CAutoFile;

/**
 * The database with the coinbase destinations.
 * This is initialized with the hardcoded values and stores the other ones in the specified file.
 */
class CoinbaseDestinationsIndex {
public:

    //! Lifecycle management
    CoinbaseDestinationsIndex() = delete;
    explicit CoinbaseDestinationsIndex(const std::map<const std::string, const int>& hardcodedAddresses, const fs::path& filepath);
    ~CoinbaseDestinationsIndex();

    //! Serialization

    ADD_SERIALIZE_METHODS

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        if (ser_action.ForRead()) {
            LOCK(cs);

            destinations.clear();

            size_t count = 0;
            READWRITE(VARINT(count));

            for (size_t i = 0; i < count; ++i) {
                CoinbaseDestination destination;
                READWRITE(destination);

                destinations.push_back(destination);
            }
        } else {
            LOCK(cs);

            size_t count = destinations.size();
            READWRITE(VARINT(count));

            for (size_t i = 0; i < count; ++i)
                READWRITE(destinations[i]);
        }
    }

private:
    CAutoFile open_file(bool readonly);
    bool load_from_file();
    bool save_to_file();

private:
    std::vector<CoinbaseDestination> destinations;
    fs::path file_path;
    CCriticalSection cs;
};

#endif // PAICOIN_COINBASE_DESTINATIONS_INDEX_H
