#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Test that all stake transactions are syncronized between nodes
"""

from test_framework.test_framework import PAIcoinTestFramework
from test_framework.util import *
from test_framework.mininode import COIN

class MultinodeStakeSync(PAIcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [['-autostake', '-tblimit=2'], ['-autostake', '-tblimit=2'] ] 

    def setup_network(self, split=False):
        super().setup_network()
        connect_nodes_bi(self.nodes,0,1)

    def run_test(self):
        HybridForkHeight = 1501 # in chainparams.cpp -> consensus.HybridConsensusHeight = 1500;
        StakeValidationHeight = 2100
        StakeEnabledHeight   = 2000
        TicketMaturity        = 8
        TicketPoolSize        = 64
        TicketExpiry          = 3 * TicketPoolSize
        MinimumStakeDiff = Decimal('0.0002')

        self.nodes[0].generate(500)
        self.sync_all()
        self.nodes[1].generate(500)
        self.sync_all()
        info = self.nodes[0].getblockchaininfo()
        assert(info['blocks'] == 1000)
        blockinfo = self.nodes[0].getblock(info['bestblockhash'])
        assert(blockinfo['version'] ==  536870912)

        balance = [ self.nodes[i].getbalance() for i in range(0, self.num_nodes) ]

        self.nodes[0].generate(HybridForkHeight - 1000 - 1)
        self.sync_all()
        info = self.nodes[0].getblockchaininfo()
        # block height just before hybrid fork
        assert(info['blocks'] == HybridForkHeight - 1)
        blockinfo = self.nodes[0].getblock(info['bestblockhash'])
        assert(blockinfo['version'] ==  536870912)

        # block height just after hybrid fork
        self.nodes[0].generate(1)
        self.sync_all()
        info = self.nodes[0].getblockchaininfo()
        # block height just after hybrid fork
        assert(info['blocks'] == HybridForkHeight)
        blockinfo = self.nodes[0].getblock(info['bestblockhash'])
        assert(blockinfo['version'] == -1610612736) # we change the sign bit of the version at fork

        stakeDiff = self.nodes[0].getstakedifficulty()
        assert(stakeDiff['current'] == MinimumStakeDiff)

        self.nodes[0].generate(400)
        self.sync_all()

        startAutoBuyerHeight = StakeEnabledHeight - TicketMaturity
        for blkidx in range(HybridForkHeight + 401, startAutoBuyerHeight+90):
            self.nodes[0].generate(1)
            print(blkidx)
            chainInfo = self.nodes[0].getblockchaininfo()
            assert(chainInfo['blocks'] == blkidx)

        self.sync_all()

        HeightExpiredBecomeMissed = TicketExpiry + StakeEnabledHeight
        assert(HeightExpiredBecomeMissed > StakeValidationHeight)

        for blkidx in range(startAutoBuyerHeight+90, HeightExpiredBecomeMissed+20):
            self.nodes[blkidx % self.num_nodes].generate(1) # mine on each of the nodes, so also the seconds one has funds
            if blkidx >= StakeValidationHeight-1:
                mempool = [self.nodes[i].getrawmempool(True) for i in range(0, self.num_nodes)]
                for nodeidx in range(0, self.num_nodes):
                    print("node:",nodeidx)
                    for tx, entry in mempool[nodeidx].items():
                        print(tx + ": " + entry['type'])
                        raw = self.nodes[nodeidx].getrawtransaction(tx)
                        dec = self.nodes[nodeidx].decoderawtransaction(raw)
                        assert(dec['txid'] == tx)
                        assert(dec['type'] == entry['type'])
                        # print(dec['txid'] + " " + dec['type'])

            self.sync_all()
            print(blkidx)
            chainInfo = self.nodes[0].getblockchaininfo()
            assert(chainInfo['blocks'] == blkidx)
        
        assert(blkidx == HeightExpiredBecomeMissed+20 -1)

        # tickets = [self.nodes[i].gettickets(True) for i in range(0, self.num_nodes)]
        # print(tickets)
        # mempool = [self.nodes[i].getrawmempool() for i in range(0, self.num_nodes)]
        # print(mempool)

        # for blkidx in range(StakeValidationHeight, StakeValidationHeight + 100):
        #     mempool = [self.nodes[i].getrawmempool() for i in range(0, self.num_nodes)]
        #     print(mempool)
        #     wins = [self.nodes[i].winningtickets() for i in range(0, self.num_nodes)]
        #     print(wins)

        #     self.nodes[0].generate(1)
        #     self.sync_all()
        #     chainInfo = self.nodes[0].getblockchaininfo()
        #     assert(chainInfo['blocks'] == blkidx)

if __name__ == '__main__':
    MultinodeStakeSync().main()
