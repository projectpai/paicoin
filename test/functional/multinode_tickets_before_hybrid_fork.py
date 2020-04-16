#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Test that intentionally purchases tickets before the Hybrid Consensus Fork height
on one of the nodes then expects the sync_all to timeout
    - when tickets are purchased the stake difficulty is automatically adjusted
    - before the fork we have no way of sending the adjusted stake difficulty to other nodes
    - the other nodes apply the validation rules, but not knowing the adjusted value, the
stake difficulty check fails
    - see multinode_hybrid_fork.py for the scenario that buys after the fork
"""

from test_framework.test_framework import PAIcoinTestFramework
from test_framework.util import *
from test_framework.mininode import COIN

class MultinodeHybridConsensusForkWithTickets(PAIcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2

    def setup_network(self, split=False):
        super().setup_network()
        connect_nodes_bi(self.nodes,0,1)

    def run_test(self):
        MinimumStakeDiff = Decimal('0.0002')
        HybridForkHeight = 1352
        BlockHeightBeforeBuy = 900
        assert(BlockHeightBeforeBuy < HybridForkHeight)

        self.nodes[0].generate(BlockHeightBeforeBuy)
        self.sync_all()
        info = self.nodes[1].getblockchaininfo()
        assert(info['blocks'] == BlockHeightBeforeBuy)
        blockinfo = self.nodes[1].getblock(info['bestblockhash'])
        assert(blockinfo['version'] ==  536870912)

        stakeDiff = self.nodes[0].getstakedifficulty()
        assert(stakeDiff['current'] == MinimumStakeDiff)

        nMaxFreshStakePerBlock = 20
        txaddress = self.nodes[0].getnewaddress()
        txs = []
        for blkidx in range(BlockHeightBeforeBuy + 1, HybridForkHeight):
            print("purchase", nMaxFreshStakePerBlock, "at", blkidx)
            txs.append(self.nodes[0].purchaseticket("", 1.5, 1, txaddress, nMaxFreshStakePerBlock))
            assert(len(txs[-1])==nMaxFreshStakePerBlock)

            self.nodes[0].generate(1)
            chainInfo = self.nodes[0].getblockchaininfo()
            assert_equal(chainInfo['blocks'], blkidx)

            stakeDiff = self.nodes[0].getstakedifficulty()
            print("stake difficulty at", blkidx, "is", stakeDiff)
            if stakeDiff['current'] > MinimumStakeDiff:
                break

        # check that having ticket purchases before the fork the nodes cannnot sync 
        assert_raises(AssertionError, lambda: self.sync_all())

if __name__ == '__main__':
    MultinodeHybridConsensusForkWithTickets().main()
