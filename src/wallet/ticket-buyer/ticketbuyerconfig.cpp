//
// Copyright (c) 2017-2020 Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//


#include "ticketbuyerconfig.h"
#include "util.h"
#include "stake/staketx.h"

CTicketBuyerConfig::CTicketBuyerConfig() :
    buyTickets(false),
    maintain(0),
    votingAddress(CNoDestination()),
    rewardAddress(CNoDestination()),
    poolFeeAddress(CNoDestination()),
    poolFees(0.0),
    limit(1),
    txExpiry(defaultTicketTxExpiry)
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

    if (gArgs.IsArgSet("-tbtxexpiry")) {
        auto expiry = gArgs.GetArg("-tbtxexpiry", txExpiry);
        if (expiry < ticketTxExpiryMin)
            txExpiry = ticketTxExpiryMin;
        else if (expiry > ticketTxExpiryMax)
            txExpiry = ticketTxExpiryMax;
        else
            txExpiry = static_cast<int>(expiry);
    }
}
