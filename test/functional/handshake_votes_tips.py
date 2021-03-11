#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Copyright (c) 2021 The PAIcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test the automatic sending of all the vote and
   chain tips on new connections.
"""

from collections import Counter
from time import sleep
from test_framework.test_framework import PAIcoinTestFramework
from test_framework.util import connect_nodes_bi, disconnect_nodes

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

        # generate block until 5 blocks after stake validation height
        # and synchornize the 2 nodes; to avoid log saving timeouts,
        # mine the entire block until right before starting to buy tickets
        # and then use incremental steps with small sleeps, to allow the
        # log file to be stored properly.

        self.nodes[0].generate(StakeEnabledHeight - TicketMaturity - self.nodes[0].getblockcount())

        step = 6
        for _ in range(self.nodes[0].getblockcount(), StakeValidationHeight, step):
            self.nodes[0].generate(step)
            sleep(0.1)
        
        self.nodes[0].generate(5)
        sleep(0.1)

        self.sync_all()

        mempools = [set(self.nodes[0].getrawmempool()), set(self.nodes[1].getrawmempool())]
        if len(mempools[0] ^ mempools[1]) != 0:
            print("Error! Mempool mismatch before votes propagation test")
            self.stop_nodes()
            return

        # disconnect the second node and advance the blockchain 5 blocks;
        # verify that the mempools on the two nodes are different after this.

        disconnect_nodes(self.nodes[1], 0)
        disconnect_nodes(self.nodes[0], 1)
        sleep(1)

        self.nodes[0].generate(5)

        #self.print_votes_in_mempool_for_node(0)
        #self.print_votes_in_mempool_for_node(1)
        mempools = [set(self.nodes[0].getrawmempool()), set(self.nodes[1].getrawmempool())]
        if len(mempools[0] ^ mempools[1]) == 0:
            print("Error! Mempools are still identical after disconnecting node 1 and node 0 has mined")
            self.stop_nodes()
            return

        # reconnect the second node and leave enough time to send the votes;
        # then, verify that the two mempools are identical.

        connect_nodes_bi(self.nodes, 0, 1)
        sleep(1)

        #self.print_votes_in_mempool_for_node(0)
        #self.print_votes_in_mempool_for_node(1)
        votes = [set(self.votes_in_mempool(self.nodes[0].getrawmempool(True))), set(self.votes_in_mempool(self.nodes[1].getrawmempool(True)))]
        votesIntersection = votes[0].intersection(votes[1])
        if votesIntersection != votes[0]:
            print("Error! Mempool votes mismatch after reconnection")
            print("- votes on node 0: " + str(votes[0]))
            print("- votes on node 1: " + str(votes[1]))
            print("- votes set intersection: " + str(votesIntersection))
            self.stop_nodes()
            return

    def votes_in_mempool(self, rawMempool):
        return [txid for (txid,tx) in rawMempool.items() for (k,v) in tx.items() if k == 'type' and v == 'vote']

    def print_votes_in_mempool_for_node(self, node_num):
        assert(node_num < len(self.nodes))
        mempool = self.nodes[node_num].getrawmempool(True)
        voteCount = Counter([v for (txid,tx) in mempool.items() for (k,v) in tx.items() if k == 'type' and v == 'vote']).get('vote')
        print("- mempool vote count on node " + str(node_num) + ": " + str(voteCount))

        
class HandshakeChainTipsTest (PAIcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 4
        self.extra_args = [["-autostake=1", "-tblimit=20"], ["-autostake=1", "-tblimit=20"], ["-autostake=1", "-tblimit=20"], ["-autostake=1", "-tblimit=20"]]

    def run_test(self):
        StakeEnabledHeight = 2000
        StakeValidationHeight = 2100
        TicketMaturity = 8

        # generate block until 5 blocks after stake validation height
        # and synchornize the 2 nodes; to avoid log saving timeouts,
        # mine the entire block until right before starting to buy tickets
        # and then use incremental steps with small sleeps, to allow the
        # log file to be stored properly.

        self.nodes[0].generate(StakeEnabledHeight - TicketMaturity - self.nodes[0].getblockcount())

        step = 6
        for _ in range(self.nodes[0].getblockcount(), StakeValidationHeight, step):
            self.nodes[0].generate(step)
            sleep(0.1)
        
        self.nodes[0].generate(5)
        sleep(0.1)

        self.sync_all()

        if not self.check_chain_tips():
            print("Error! Chain tips mismatch before propagation test")
            self.stop_nodes()
            return

        # disconnect all nodes, so that we get 4 concurrent chains;
        # then, mine on each of them and validate the height and chaintips

        self.disconnect_all_nodes()

        for i in range(self.num_nodes):
            self.nodes[i].generate(1)

        if not self.check_chain_tips(True):
            print("Error! Chain tips are still identical after disconnection")
            self.stop_nodes()
            return

        # Reconnect all the nodes and check that the chaintips are the same

        self.connect_all_nodes()
        sleep(1)
        
        if not self.check_chain_tips():
            print("Error! Chain tips are different after reconnection")
            self.stop_nodes()
            return

    def connect_all_nodes(self):
        for i in range(self.num_nodes):
            for j in range(i+1, self.num_nodes):
                connect_nodes_bi(self.nodes, i, j)

    def disconnect_all_nodes(self):
        for i in range(self.num_nodes):
            for j in range(self.num_nodes):
                if i != j:
                    disconnect_nodes(self.nodes[i], j)

    def check_chain_tips(self, different=False):
        tip_list = []
        for i in range(self.num_nodes):
            tip_list.append(self.nodes[i].getchaintips())

        for i in range(self.num_nodes):
            for j in range(i+1, self.num_nodes):
                if (different and self.equal_tip_lists(tip_list[i], tip_list[j])) or ((not different) and (not self.equal_tip_lists(tip_list[i], tip_list[j]))):
                    return False

        return True

    def equal_tip_lists(self, tips0, tips1):
        for t0 in tips0:
            found = False
            for t1 in tips1:
                if t0['height'] == t1['height'] and t0['hash'] == t1['hash']:
                    found = True
                    break
            if not found:
                return False
        return True

if __name__ == '__main__':
    HandshakeVoteTest().main()
    HandshakeChainTipsTest().main()
