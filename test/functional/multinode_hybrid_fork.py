#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Test that purchases tickets after the Hybrid Consensus Fork height
on one of the nodes then expects the sync_all to timeout
    - when tickets are purchased the stake difficulty is automatically adjusted
    - after the fork we use an extended block header that carries the adjusted stake difficulty to other nodes
    - the other nodes apply the validation rules, and knowing the adjusted value, the
stake difficulty does not fail
    - see multinode_tickets_before_hybrid_fork.py for the scenario that buys before the fork
"""

from test_framework.test_framework import PAIcoinTestFramework
from test_framework.util import *
from test_framework.mininode import COIN

class MultinodeHybridConsensusFork(PAIcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2

    def setup_network(self, split=False):
        super().setup_network()
        connect_nodes_bi(self.nodes,0,1)

    def run_test(self):
        HybridForkHeight = 1501 # in chainparams.cpp -> consensus.HybridConsensusHeight = 1500;
        StakeValidationHeight = 2100
        MinimumStakeDiff = Decimal('0.0002')

        self.nodes[0].generate(1000)
        self.sync_all()
        info = self.nodes[0].getblockchaininfo()
        assert(info['blocks'] == 1000)
        blockinfo = self.nodes[0].getblock(info['bestblockhash'])
        assert(blockinfo['version'] ==  536870912)

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

        nMaxFreshStakePerBlock = 20
        txaddress = self.nodes[0].getnewaddress()
        rewardaddress = self.nodes[0].getnewaddress()
        txs = []
        for blkidx in range(HybridForkHeight + 401, StakeValidationHeight):
            print("purchase", nMaxFreshStakePerBlock, "at", blkidx)
            txs.append(self.nodes[0].purchaseticket("", 1.5, 1, txaddress, rewardaddress, nMaxFreshStakePerBlock))
            assert(len(txs[-1])==nMaxFreshStakePerBlock)

            self.nodes[0].generate(1)
            chainInfo = self.nodes[0].getblockchaininfo()
            assert(chainInfo['blocks'] == blkidx)

            stakeDiff = self.nodes[0].getstakedifficulty()
            print("stake difficulty at", blkidx, "is", stakeDiff)
            if stakeDiff['current'] > MinimumStakeDiff:
                break

        # check that after hybrid fork we can sync the two nodes while buying tickets
        # this means that the extended BlockHeader is transmitted correctly
        self.sync_all()

if __name__ == '__main__':
    MultinodeHybridConsensusFork().main()
