// Copyright (c) 2020 The Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ticketbuyerconfig.h"
#include "util.h"

CTicketBuyerConfig::CTicketBuyerConfig() :
    buyTickets(false),
    maintain(0),
    votingAddress(CNoDestination()),
    rewardAddress(CNoDestination()),
    poolFeeAddress(CNoDestination()),
    poolFees(0.0),
    limit(1)
{
}

void CTicketBuyerConfig::ParseCommandline()
{
    if (gArgs.IsArgSet("-tbbalancetomaintainabsolute"))
        maintain = gArgs.GetArg("-tbbalancetomaintainabsolute", 0);

    if (gArgs.IsArgSet("-tbvotingaddress")) {
        std::string addr = gArgs.GetArg("-tbvotingaddress", "");
        votingAddress = DecodeDestination(addr);
    }

    if (gArgs.IsArgSet("-tbrewardaddress")) {
        std::string addr = gArgs.GetArg("-tbrewardaddress", "");
        rewardAddress = DecodeDestination(addr);
    }

    if (gArgs.IsArgSet("-tblimit"))
        limit = static_cast<int>(gArgs.GetArg("-tblimit", 0));

    if (gArgs.IsArgSet("-tbvotingaccount"))
        votingAccount = gArgs.GetArg("-tbvotingaccount", "");
}
