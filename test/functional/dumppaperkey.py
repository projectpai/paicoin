#!/usr/bin/env python3
# Copyright (c) 2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test 'dumppaperkey' RPC call."""

from test_framework.test_framework import PAIcoinTestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes_bi,
)

class DumpPaperKeyTest(PAIcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [['-usehd=0'], ['-usehd=1']]

    def run_test (self):
        tmpdir = self.options.tmpdir

        # Make sure can't switch off usehd after wallet creation
        self.stop_node(1)
        self.assert_start_raises_init_error(1, ['-usehd=0'], 'already existing HD wallet')
        self.start_node(1)
        connect_nodes_bi(self.nodes, 0, 1)

        paper_key = 'grunt curtain saddle couch ice detect drastic blossom mosquito message giraffe always'

        # Restore a wallet
        wallet_file = 'wallet_dumppaperkey_test.dat'
        self.nodes[1].restorewallet(paper_key, wallet_file)

        # Restart the node with the restored wallet file
        self.stop_node(1)
        self.start_node(1, ['-wallet=' + wallet_file])
        connect_nodes_bi(self.nodes, 0, 1)

        # Make sure we use HD
        masterkeyid = self.nodes[1].getwalletinfo()['hdmasterkeyid']
        assert_equal(len(masterkeyid), 40)

        # Verify that the paper key we get from the unencrypted wallet is the same as the one we restored it from
        paper_key_unencrypted = self.nodes[1].dumppaperkey()
        assert_equal(paper_key, paper_key_unencrypted)

        # Encrypt wallet and restart it
        wallet_pass = 'abcd'
        self.nodes[1].encryptwallet(wallet_pass)
        self.stop_node(1)
        self.start_node(1, ['-wallet=' + wallet_file])
        connect_nodes_bi(self.nodes, 0, 1)

        # Make sure we use HD
        masterkeyid = self.nodes[1].getwalletinfo()['hdmasterkeyid']
        assert_equal(len(masterkeyid), 40)

        # Verify that the paper key we get from the encrypted wallet is the same as the one we restored it from
        self.nodes[1].walletpassphrase(wallet_pass, 3600)
        paper_key_encrypted = self.nodes[1].dumppaperkey()
        assert_equal(paper_key, paper_key_encrypted)

if __name__ == '__main__':
    DumpPaperKeyTest().main ()
