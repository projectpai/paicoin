//
// Copyright (c) 2017-2020 Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//


#include "autovoterconfig.h"
#include "util.h"

CAutoVoterConfig::CAutoVoterConfig() :
    autoVote(false),
    voteBits(VoteBits::rttAccepted)
{
}

void CAutoVoterConfig::ParseCommandline()
{
    if (gArgs.IsArgSet("-avvotebits")) {
        int64_t vb = gArgs.GetArg("-avvotebits", 1);
        if (vb >= std::numeric_limits<uint16_t>().min() || vb <= std::numeric_limits<uint16_t>().max())
            voteBits = VoteBits(static_cast<uint16_t>(vb));
        else
            LogPrintf("ERROR: automatic vote bits out of range %lld. Using default: %d", vb, VoteBits().getBits());
    }

    if (gArgs.IsArgSet("-avextendedvotebits")) {
        std::string evb = gArgs.GetArg("-avextendedvotebits", "");
        if (ExtendedVoteBits::containsValidExtendedVoteBits(evb))
            extendedVoteBits = ExtendedVoteBits(evb);
        else
            LogPrintf("ERROR: automatic extended vote bits are note valid %s. Using default: %s", evb, ExtendedVoteBits().getHex());
    }
}
