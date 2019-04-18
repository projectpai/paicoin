#!/usr/bin/env python3
# Copyright (c) 2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test the search operations RPC commands
    - search operations (ported from Decred) include: existsaddress existsaddresses existsmempooltxs searchrawtransactions
"""

from test_framework.test_framework import PAIcoinTestFramework
from test_framework.util import *

class NodeOperations(PAIcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1

    def run_test(self):
        # node tests:
        # 1. valid parameters
        x = self.nodes[0].node('connect', '127.0.0.1:8566', 'temp')
        assert(x == None)
        x = self.nodes[0].node('remove', '127.0.0.1:8566')
        assert(x == None)
        # TODO make this test have valid input and output once the command is implemented

        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].node)
        assert_raises_rpc_error(-1, None, self.nodes[0].node, "remove")
        assert_raises_rpc_error(-1, None, self.nodes[0].node, "remove", "127.0.0.1:8566", "perm", "temp")


        # getheaders tests:
        # 1. valid parameters
        x = self.nodes[0].getheaders("blocklocators", "hashstop")
        assert(x == [])
        # TODO make this test have valid input and output once the command is implemented

        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].getheaders)
        assert_raises_rpc_error(-1, None, self.nodes[0].getheaders,"blocklocators", "hashstop", "hashstop")


        # rebroadcastmissed tests:
        # 1. valid parameters
        x = self.nodes[0].rebroadcastmissed() 
        assert(x == None)

        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].rebroadcastmissed, "param")
        assert_raises_rpc_error(-1, None, self.nodes[0].rebroadcastmissed, 'param1', 'param2')
        

        # rebroadcastwinners tests:
        # 1. valid parameters
        x = self.nodes[0].rebroadcastwinners() 
        assert(x == None)

        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].rebroadcastwinners, "param")
        assert_raises_rpc_error(-1, None, self.nodes[0].rebroadcastwinners, 'param1', 'param2')


if __name__ == "__main__":
    NodeOperations().main()