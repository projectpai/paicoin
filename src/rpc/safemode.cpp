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

