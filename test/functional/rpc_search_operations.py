#!/usr/bin/env python3
# Copyright (c) 2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test the search operations RPC commands
    - search operations (ported from Decred) include: existsaddress existsaddresses existsmempooltxs searchrawtransactions
"""

from test_framework.test_framework import PAIcoinTestFramework
from test_framework.util import *

class SearchOperations(PAIcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1

    def run_test(self):
        # existsaddress tests:
        # 1. valid parameters
        x = self.nodes[0].existsaddress('3J98t1WpEZ73CNmQviecrnyiWrnqRhWNLy')
        assert(x == False)
        #TODO add check for an existing address once the command is implemented properly

        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].existsaddress)
        assert_raises_rpc_error(-1, None, self.nodes[0].existsaddress, '3J98t1WpEZ73CNmQviecrnyiWrnqRhWNLy', '3J98t1WpEZ73CNmQviecrnyiWrnqRhWNLy')


        # existsaddresses tests:
        # 1. valid parameters
        x = self.nodes[0].existsaddresses([{ 'address' : '3J98t1WpEZ73CNmQviecrnyiWrnqRhWNLy'}, { 'address' : '3J98t1WpEZ73CNmQviecrnyiWrnqRhWNLy'}])
        assert(x == "00")
        #TODO add check for an existing addresses once the command is implemented properly

        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].existsaddresses)
        assert_raises_rpc_error(-1, None, self.nodes[0].existsaddresses, '3J98t1WpEZ73CNmQviecrnyiWrnqRhWNLy', '3J98t1WpEZ73CNmQviecrnyiWrnqRhWNLy')


        # existsmempooltxs tests:
        # 1. valid parameters
        x = self.nodes[0].existsmempooltxs('9b24ef059f6983af2d05274fdf8725b01b9546f0e170d4d4f94135b4e90fa165') 
        assert(x == "00")
        #TODO add check for real txhashblob once the command is implemented properly

        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].existsmempooltxs)
        assert_raises_rpc_error(-1, None, self.nodes[0].existsmempooltxs, '9b24ef059f6983af2d05274fdf8725b01b9546f0e170d4d4f94135b4e90fa165', '9b24ef059f6983af2d05274fdf8725b01b9546f0e170d4d4f94135b4e90fa165')


        # searchrawtransactions tests:
        # 1. valid parameters
        assert_raises_rpc_error(-1, "Address index not enabled", self.nodes[0].searchrawtransactions,'TsfDLrRkk9ciUuwfp2b8PawwnukYD7yAjGd')
        # see more valid tests in SearchRawTransactionsTest, this setup has no "addrindex" flag

        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].searchrawtransactions)
        assert_raises_rpc_error(-1, None, self.nodes[0].searchrawtransactions,"address","verbose","skip","count","vinextra","reverse","filteraddrs","unknown_param")

if __name__ == "__main__":
    SearchOperations().main()