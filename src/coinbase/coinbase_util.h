/* * Copyright (c) 2021 Project PAI Foundation
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#ifndef PAICOIN_COINBASE_UTIL_H
#define PAICOIN_COINBASE_UTIL_H

#include "util.h"

template <typename... Args>
 void CoinbaseLog(Args... params)
 {
     LogPrint(BCLog::COINBASE, params...);
 }

#endif // PAICOIN_COINBASE_UTIL_H
