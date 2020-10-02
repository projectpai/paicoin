//
// Copyright (c) 2017-2020 Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//

#include "safemode.h"

#include "rpc/protocol.h"
#include "util.h"
#include "warnings.h"

void ObserveSafeMode()
{
    const auto warning = GetWarnings("rpc");
    if (warning != "" && !gArgs.GetBoolArg("-disablesafemode", DEFAULT_DISABLE_SAFEMODE)) {
        throw JSONRPCError(RPCErrorCode::FORBIDDEN_BY_SAFE_MODE, "Safe mode: " + warning);
    }
}

