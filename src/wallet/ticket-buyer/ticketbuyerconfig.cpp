// Copyright (c) 2020 The Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ticketbuyerconfig.h"
#include "util.h"

CTicketBuyerConfig::CTicketBuyerConfig() :
    buyTickets(false),
    maintain(0),
    poolFees(0.0),
    limit(1)
{
}

void CTicketBuyerConfig::ParseCommandline()
{
    if (gArgs.IsArgSet("-tbbalancetomaintainabsolute"))
        maintain = gArgs.GetArg("-tbbalancetomaintainabsolute", 0);

    if (gArgs.IsArgSet("-tbvotingaddress"))
        votingAddress = gArgs.GetArg("-tbvotingaddress", "");

    if (gArgs.IsArgSet("-tblimit"))
        limit = static_cast<int>(gArgs.GetArg("-tblimit", 0));

    if (gArgs.IsArgSet("-tbvotingaccount"))
        votingAccount = gArgs.GetArg("-tbvotingaccount", "");
}
