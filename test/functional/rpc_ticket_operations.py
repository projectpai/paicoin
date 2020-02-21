#!/usr/bin/env python3
# Copyright (c) 2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test the search operations RPC commands
    - ticket operations (ported from Decred) include: 
        existsexpiredtickets
        existsliveticket
        existslivetickets
        existsmissedtickets
        getticketpoolvalue
        livetickets
        missedtickets
        ticketfeeinfo
        ticketsforaddress
        ticketvwrap
"""

from test_framework.test_framework import PAIcoinTestFramework
from test_framework.util import *

def have_same_elements(left, right):
    return len(left) == len(right) and set(left) == set(right)

class TicketOperations(PAIcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [['-txindex']] 


    def run_test(self):
        # these are the same values as in chainparams.cpp for REGTEST, update them if they change
        nStakeEnabledHeight    = 2000
        nStakeValidationHeight = 2100
        nTicketMaturity        = 8
        nMaxFreshStakePerBlock = 20
        nTicketsPerBlock       = 5
        nTicketPoolSize        = 64
        nTicketExpiry          = 5 * nTicketPoolSize
        nStakeDiffWindowSize   = 8

        self.nodes[0].generate(nStakeEnabledHeight - nTicketMaturity - 1)
        self.sync_all()
        txaddress = self.nodes[0].getnewaddress()
        print(txaddress)

        txs = []
        for blkidx in range(nStakeEnabledHeight - nTicketMaturity , nStakeEnabledHeight+1):
            print("purchase", nMaxFreshStakePerBlock, "at", blkidx)
            txs.append(self.nodes[0].purchaseticket("", 1.5, 1, txaddress, nMaxFreshStakePerBlock))
            assert(len(txs[-1])==nMaxFreshStakePerBlock)

            self.nodes[0].generate(1)
            self.sync_all()
            chainInfo = self.nodes[0].getblockchaininfo()
            assert(chainInfo['blocks'] == blkidx)

        # we should be at nStakeEnabledHeight, so only tickets in txs[0] should be live
        assert(blkidx == nStakeEnabledHeight)

        # existsexpiredtickets tests:
        # 1. valid parameters
        x = self.nodes[0].existsexpiredtickets(txs[0])
        assert(have_same_elements(x.keys(), txs[0]))
        assert(not any(x.values())) # no ticket exists so all False
        x = self.nodes[0].existsexpiredtickets(txs[1])
        assert(have_same_elements(x.keys(), txs[1]))
        assert(not any(x.values())) # no ticket exists so all False
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].existsexpiredtickets)
        assert_raises_rpc_error(-1, None, self.nodes[0].existsexpiredtickets, "param1", "param2")
        assert_raises_rpc_error(-1, None, self.nodes[0].existsexpiredtickets, ["hash1", "hash2"])

        # existsliveticket tests:
        # 1. valid parameters
        x = self.nodes[0].existsliveticket(txs[0][5])
        assert(x == True)  # first bought txs are live since we are at nStakeEnabledHeight
        x = self.nodes[0].existsliveticket(txs[1][4])
        assert(x == False)
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].existsliveticket)
        assert_raises_rpc_error(-1, None, self.nodes[0].existsliveticket, "param1", "param2")
        assert_raises_rpc_error(-1, None, self.nodes[0].existsliveticket,"txhash")


        # existslivetickets tests:
        # 1. valid parameters
        x = self.nodes[0].existslivetickets(txs[0])
        assert(have_same_elements(x.keys(), txs[0]))
        assert(any(x.values())) # all tickets bought first are live
        x = self.nodes[0].existslivetickets(txs[1])
        assert(have_same_elements(x.keys(), txs[1]))
        assert(not any(x.values())) # these are bought one block after so not live yet
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].existslivetickets)
        assert_raises_rpc_error(-1, None, self.nodes[0].existslivetickets, "param1", "param2")
        assert_raises_rpc_error(-1, None, self.nodes[0].existslivetickets, ["hash1", "hash2"])

        # existsmissedtickets tests:
        # 1. valid parameters
        x = self.nodes[0].existsmissedtickets(txs[0])
        assert(have_same_elements(x.keys(), txs[0]))
        assert(not any(x.values())) # no ticket exists so all False
        x = self.nodes[0].existsmissedtickets(txs[1])
        assert(have_same_elements(x.keys(), txs[1]))
        assert(not any(x.values())) # no ticket exists so all False
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].existsmissedtickets)
        assert_raises_rpc_error(-1, None, self.nodes[0].existsmissedtickets, "param1", "param2")
        assert_raises_rpc_error(-1, None, self.nodes[0].existsmissedtickets, ["hash1", "hash2"])


        # # getticketpoolvalue tests:
        # # 1. valid parameters
        x = self.nodes[0].getticketpoolvalue()
        ticketPrice = 0.00034500 # TODO replace this hardcoded value with a calculated one once it is available also in purchaseticket
        assert(float(x) == ticketPrice * len(txs[0])) # all live tickets have payed the same price
        # # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].getticketpoolvalue, 'param')
        assert_raises_rpc_error(-1, None, self.nodes[0].getticketpoolvalue, "param1", "param2")


        # livetickets tests:
        # 1. valid parameters
        x = self.nodes[0].livetickets()
        assert(have_same_elements(x["tickets"], txs[0]))
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].livetickets, 'param')
        assert_raises_rpc_error(-1, None, self.nodes[0].livetickets, "param1", "param2")


        # missedtickets tests:
        # 1. valid parameters
        x = self.nodes[0].missedtickets()
        assert(x == { "tickets": [] })
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].missedtickets, 'param')
        assert_raises_rpc_error(-1, None, self.nodes[0].missedtickets, "param1", "param2")


        # ticketfeeinfo tests:
        # 1. valid parameters
        x = self.nodes[0].ticketfeeinfo() # at this point mempool is empty, so no fee info
        assert(x == {'feeinfomempool': {'number': 0}, 'feeinfoblocks': [], 'feeinfowindows': []})
        x = self.nodes[0].ticketfeeinfo(3)   # set blocks
        assert(x['feeinfomempool']['number'] == 0)
        assert(len(x['feeinfoblocks']) == 3)
        assert(x['feeinfoblocks'][0]['height'] == nStakeEnabledHeight)
        assert(x['feeinfoblocks'][0]['number'] == nMaxFreshStakePerBlock)
        assert(len(x['feeinfowindows']) == 0)
        x = self.nodes[0].ticketfeeinfo(3,2) # set blocks and windows
        assert(x['feeinfomempool']['number'] == 0)
        assert(len(x['feeinfoblocks']) == 3)
        assert(len(x['feeinfowindows']) == 2)
        assert(x['feeinfowindows'][-1]['endheight'] - x['feeinfowindows'][-1]['startheight']  == nStakeDiffWindowSize)
        assert(x['feeinfowindows'][-1]['number'] == nMaxFreshStakePerBlock * nStakeDiffWindowSize)
        # TODO add checks for the fee rate results when the fees/ticket prices are not hardcoded
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].ticketfeeinfo, "param1", "param2", "param3")

        # ticketsforaddress tests:
        # 1. valid parameters
        unused = self.nodes[0].getnewaddress()
        x = self.nodes[0].ticketsforaddress(unused)
        assert(x == { "tickets": [] })
        x = self.nodes[0].ticketsforaddress(txaddress)
        assert(have_same_elements(x["tickets"], txs[0]))
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].ticketsforaddress)
        assert_raises_rpc_error(-5, None, self.nodes[0].ticketsforaddress, 'param')
        assert_raises_rpc_error(-1, None, self.nodes[0].ticketsforaddress, "param1", "param2")


        # ticketvwap tests:
        # 1. valid parameters
        x = self.nodes[0].ticketvwap()
        assert(x == 0.000)
        x = self.nodes[0].ticketvwap(10)    # set start
        assert(x == 0.000)
        x = self.nodes[0].ticketvwap(10, 20) # set start and stop
        assert(x == 0.000)
        # TODO make this test have valid input and output once the command is implemented
        # the command was implemented, but to give non-zero results we need to first set a nStakeDiffuculty correctly
        # 2. invalid parameters
        assert_raises_rpc_error(-8, None, self.nodes[0].ticketvwap, nStakeEnabledHeight + 1, nStakeEnabledHeight)
        assert_raises_rpc_error(-8, None, self.nodes[0].ticketvwap, nStakeEnabledHeight - 1, nStakeEnabledHeight + 1)
        assert_raises_rpc_error(-1, None, self.nodes[0].ticketvwap, "param1")
        assert_raises_rpc_error(-1, None, self.nodes[0].ticketvwap, "param1", "param2")
        assert_raises_rpc_error(-1, None, self.nodes[0].ticketvwap, "param1", "param2", "param3")

        self.nodes[0].generate(1)
        self.sync_all()
        chainInfo = self.nodes[0].getblockchaininfo()
        assert(chainInfo['blocks'] == nStakeEnabledHeight + 1)

        # at this height more also tickets in txs[1] should be live, but missed and expired should be the same
        x = self.nodes[0].existsexpiredtickets(txs[0])
        assert(have_same_elements(x.keys(), txs[0]))
        assert(not any(x.values())) # no ticket exists so all False
        x = self.nodes[0].existsexpiredtickets(txs[1])
        assert(have_same_elements(x.keys(), txs[1]))
        assert(not any(x.values())) # no ticket exists so all False

        x = self.nodes[0].existsliveticket(txs[0][5])
        assert(x == True)
        x = self.nodes[0].existsliveticket(txs[1][4])
        assert(x == True)
        
        x = self.nodes[0].existslivetickets(txs[0])
        assert(have_same_elements(x.keys(), txs[0]))
        assert(any(x.values())) # all tickets bought first are live
        x = self.nodes[0].existslivetickets(txs[1])
        assert(have_same_elements(x.keys(), txs[1]))
        assert(any(x.values())) # these are also live

        x = self.nodes[0].existsmissedtickets(txs[0])
        assert(have_same_elements(x.keys(), txs[0]))
        assert(not any(x.values())) # no ticket exists so all False
        x = self.nodes[0].existsmissedtickets(txs[1])
        assert(have_same_elements(x.keys(), txs[1]))
        assert(not any(x.values())) # no ticket exists so all False

        x = self.nodes[0].livetickets()
        assert(have_same_elements(x["tickets"], txs[0] + txs[1]))

        x = self.nodes[0].missedtickets()
        assert(x == { "tickets": [] })

        # build the chain up to nStakeValidationHeight - 1
        nMaxFreshStakePerBlock = 11
        txs = []
        for blkidx in range(nStakeEnabledHeight + 2 , nStakeValidationHeight):
            print("purchase ", nMaxFreshStakePerBlock, " at ", blkidx)
            txs.append(self.nodes[0].purchaseticket("", 1.5, 1, txaddress, nMaxFreshStakePerBlock))
            assert(len(txs[-1])==nMaxFreshStakePerBlock)

            x = self.nodes[0].ticketfeeinfo() # we should see the fee info for mempool before calling generate
            assert(x['feeinfomempool']['number'] == nMaxFreshStakePerBlock)
            assert(len(x['feeinfoblocks']) == 0)
            assert(len(x['feeinfowindows']) == 0)
            
            self.nodes[0].generate(1)
            self.sync_all()
            chainInfo = self.nodes[0].getblockchaininfo()
            assert(chainInfo['blocks'] == blkidx)

        assert(blkidx == nStakeValidationHeight - 1)

        # starting with nStakeValidationHeight we need to add vote txs into blocks, otherwise TestBlockValidity fails: too-few-votes
        assert_raises_rpc_error(-1, None, self.nodes[0].generate,1)
        dummyVoteBits    =  1
        dummyVoteBitsExt = "a"

        nHeightExpiredBecomeMissed = nTicketExpiry + nStakeEnabledHeight
        assert(nHeightExpiredBecomeMissed > nStakeValidationHeight)

        for blkidx in range(nStakeValidationHeight, nHeightExpiredBecomeMissed + 10):
            winners = self.nodes[0].winningtickets()
            assert(len(winners['tickets'])==nTicketsPerBlock)
            blockhash = chainInfo['bestblockhash']
            blockheight = chainInfo['blocks']
            for tickethash in winners['tickets']:
                votehash = self.nodes[0].generatevote(blockhash, blockheight, tickethash, dummyVoteBits, dummyVoteBitsExt)
            
            self.nodes[0].generate(1)
            self.sync_all()
            chainInfo = self.nodes[0].getblockchaininfo()
            assert(chainInfo['blocks'] == blkidx)
            print("generated block ", blkidx)

            x = self.nodes[0].missedtickets()
            print("missed", x)
            assert(blkidx < nHeightExpiredBecomeMissed or len(x["tickets"]) > 0) # we expect missed tickets 
        

if __name__ == "__main__":
    TicketOperations().main()
