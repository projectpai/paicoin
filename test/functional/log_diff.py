#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
"""

from test_framework.test_framework import PAIcoinTestFramework
from test_framework.util import *
from test_framework.mininode import COIN

class LogDiff(PAIcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2

    def setup_network(self, split=False):
        super().setup_network()
        connect_nodes_bi(self.nodes,0,1)

    def run_test(self):
        StakeValidationHeight = 2100

        info = self.nodes[0].getblockchaininfo()
        blockinfo = self.nodes[0].getblock(info['bestblockhash'])
        prevDiff = blockinfo['difficulty']
        prevVers = blockinfo['version']
        print('at', 0, 'diff:', prevDiff, 'version:', prevVers)
        print(blockinfo['bits'])

        for blkidx in range(1, StakeValidationHeight-1):
            self.nodes[0].generate(1)
            info = self.nodes[0].getblockchaininfo()
            blockinfo = self.nodes[0].getblock(info['bestblockhash'])
            diff = blockinfo['difficulty']
            vers = blockinfo['version']
            if diff != prevDiff or vers != prevVers:
                prevDiff = diff
                prevVers = vers
                print('at', blkidx, 'diff:', prevDiff, 'version:', prevVers)
                # self.sync_all()
                print(blockinfo['bits'])

        # stakeDiff = self.nodes[0].getstakedifficulty()
        # assert(stakeDiff['current'] == MinimumStakeDiff)

if __name__ == '__main__':
    LogDiff().main()
