// Copyright (c) 2020 The Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "autorevokerconfig.h"
#include "util.h"

CAutoRevokerConfig::CAutoRevokerConfig() :
    autoRevoke(false)
{
}

void CAutoRevokerConfig::ParseCommandline()
{
    if (gArgs.IsArgSet("-autoRevoke"))
        autoRevoke = gArgs.GetBoolArg("-autoRevoke", false);
}
