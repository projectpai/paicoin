/* * Copyright (c) 2021 Project PAI Foundation
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include <coinbase/coinbase_destinations_index.h>

#include <coinbase/coinbase_destination.h>
#include <coinbase/coinbase_util.h>
#include <streams.h>
#include <clientversion.h>

CoinbaseDestinationsIndex::CoinbaseDestinationsIndex(const std::map<const std::string, const int>& hardcodedAddresses, const fs::path& filepath)
    : file_path(filepath)
{
    for (auto p: hardcodedAddresses) {
        try {
            CoinbaseDestination destination(p.first, p.second, true);
            destinations.push_back(destination);
        }  catch (std::invalid_argument e) {
            assert(false && "Failed to load a hardcoded coinbase address!");
        }
    }

    load_from_file();
}

CoinbaseDestinationsIndex::~CoinbaseDestinationsIndex()
{
}

CAutoFile CoinbaseDestinationsIndex::open_file(bool readonly)
{
    if (!readonly)
         fs::create_directories(file_path.parent_path());

    FILE* file = fsbridge::fopen(file_path, readonly ? "rb" : "wb");
    if (!file) {
        CoinbaseLog("Could not open the coinbase addresses file!");
        return CAutoFile(nullptr, SER_DISK, CLIENT_VERSION);
    }

    return CAutoFile(file, SER_DISK, CLIENT_VERSION);
}

bool CoinbaseDestinationsIndex::load_from_file()
{
    CAutoFile file = open_file(true);
    if (file.IsNull()) {
        CoinbaseLog("Could not open the coinbase addresses file for read!");
        return false;
    }

    file >> *this;
    return true;
}

bool CoinbaseDestinationsIndex::save_to_file()
{
    CAutoFile file = open_file(false);
    if (file.IsNull()) {
        CoinbaseLog("Could not open the coinbase addresses file for read!");
        return false;
    }

    file << *this;
    return true;
}
