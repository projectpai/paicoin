#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Copyright (c) 2021 The PAIcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test the automatic sending of all the vote and
   chain tips on new connections.
"""

from collections import Counter
from test_framework.test_framework import PAIcoinTestFramework
from test_framework.util import *

class HandshakeVoteTest (PAIcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [["-autostake=1", "-tblimit=20"], []]

    def setup_network(self, split=False):
        super().setup_network()
        connect_nodes_bi(self.nodes, 0, 1)

    def run_test(self):
        StakeEnabledHeight = 2000
        StakeValidationHeight = 2100
        TicketMaturity = 8

        print("- advancing chain until right before buying tickets")

        self.nodes[0].generate(StakeEnabledHeight - TicketMaturity - self.nodes[0].getblockcount())
        print("- height before buying tickets: " + str(self.nodes[0].getblockcount()))

        step = 6

        print("continuing until the stake validation height in " + str(step) + " block batches")

        for _ in range(self.nodes[0].getblockcount(), StakeValidationHeight, step):
            self.nodes[0].generate(step)
        
        print("- height at stake validation: " + str(self.nodes[0].getblockcount()))

        self.nodes[0].generate(5)

        self.sync_all()

        mempool0 = set(self.nodes[0].getrawmempool())
        mempool1 = set(self.nodes[1].getrawmempool())
        if len(mempool0 ^ mempool1) != 0:
            print("Error! Mempool mismatch before votes propagation test")
            self.stop_nodes()
            return

        print("- height at second node stop: " + str(self.nodes[0].getblockcount()))

        disconnect_nodes(self.nodes[1], 0)
        disconnect_nodes(self.nodes[0], 1)
        time.sleep(1)

        self.nodes[0].generate(5)

        mempool0 = self.nodes[0].getrawmempool(True)
        voteCount0 = Counter([v for (txid,tx) in mempool0.items() for (k,v) in tx.items() if k == 'type' and v == 'vote']).get('vote')
        print("- vote count on node 0 after only node 0 was mining " + str(voteCount0))

        mempool1 = self.nodes[1].getrawmempool(True)
        voteCount1 = Counter([v for (txid,tx) in mempool1.items() for (k,v) in tx.items() if k == 'type' and v == 'vote']).get('vote')
        print("- vote count on node 1 after only node 0 was mining, but before reconnecting " + str(voteCount1))

        connect_nodes_bi(self.nodes, 0, 1)
        time.sleep(5)

        mempool0 = set(self.nodes[0].getrawmempool())
        mempool1 = set(self.nodes[1].getrawmempool())
        if len(mempool0.symmetric_difference(mempool1)) == 0:
            print("Error! Mempool is identical after reconnecting node 1")
            self.stop_nodes()
            return

        mempool1 = self.nodes[1].getrawmempool(True)
        voteCount1 = Counter([v for (txid,tx) in mempool1.items() for (k,v) in tx.items() if k == 'type' and v == 'vote']).get('vote')
        print("- vote count on node 1 after only node 0 was mining, after reconnection " + str(voteCount1))

        votes0 = set(self.votesInMempool(self.nodes[0].getrawmempool(True)))
        votes1 = set(self.votesInMempool(self.nodes[1].getrawmempool(True)))
        votesIntersection = votes0.intersection(votes1)
        if votesIntersection != votes0:
            print("Error! Mempool votes mismatch after reconnection")
            print("- votes on node 0: " + str(votes0))
            print("- votes on node 1: " + str(votes1))
            print("- votes set intersection: " + str(votesIntersection))
            self.stop_nodes()
            return

        print("Test successful!")

    def votesInMempool(self, rawMempool):
        return [txid for (txid,tx) in rawMempool.items() for (k,v) in tx.items() if k == 'type' and v == 'vote']
        
# class HandshakeChainTipsTest (PAIcoinTestFramework):

#     def set_test_params(self):
#         self.num_nodes = 4

#     def run_test(self):
#         pass

if __name__ == '__main__':
    HandshakeVoteTest().main()
    #HandshakeChainTipsTest().main()
