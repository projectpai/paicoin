// Copyright (c) 2021 Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <sstream>
#include <thread>

#include <util/threadids.h>

const std::string util::ThreadGetId() {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}
