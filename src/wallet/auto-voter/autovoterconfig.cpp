// Copyright (c) 2020 The Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "autovoterconfig.h"
#include "util.h"

CAutoVoterConfig::CAutoVoterConfig() :
    autoVote(false),
    voteBits(VoteBits::Rtt, true)
{
}

void CAutoVoterConfig::ParseCommandline()
{
    if (gArgs.IsArgSet("-autoVote"))
        autoVote = gArgs.GetBoolArg("-autoVote", false);
}
