#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Test the wallet ticket API.

Scenarios using operations:
    getticketfee
    gettickets
    purchaseticket
    revoketickets
    setticketfee
    listtickets
"""

from test_framework.test_framework import PAIcoinTestFramework
from test_framework.util import *

class WalletTicketOperations(PAIcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
    
    def run_test(self):
        # getticketfee tests:
        # 1. valid parameters
        x = self.nodes[0].getticketfee()
        assert(x == 0)
        # TODO make this test have valid input and output once the command is implemented
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].getticketfee, "param1")
        assert_raises_rpc_error(-1, None, self.nodes[0].getticketfee, "param1", "param2")

        # gettickets tests:
        # 1. valid parameters
        x = self.nodes[0].gettickets(True)
        assert(x == {})
        # TODO make this test have valid input and output once the command is implemented
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].gettickets)
        assert_raises_rpc_error(-1, None, self.nodes[0].gettickets, "param1")
        assert_raises_rpc_error(-1, None, self.nodes[0].gettickets, "param1", "param2")
        
        # TODO add test for "purchasetickets" once it is merged

        # revoketickets tests:
        # 1. valid parameters
        x = self.nodes[0].revoketickets()
        assert(x == None)
        # TODO make this test have valid input and output once the command is implemented
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].revoketickets, "param1")
        assert_raises_rpc_error(-1, None, self.nodes[0].revoketickets, "param1", "param2")

        # setticketfee tests:
        # 1. valid parameters
        x = self.nodes[0].setticketfee(0.02)
        assert(x == False)
        # TODO make this test have valid input and output once the command is implemented
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].setticketfee)
        assert_raises_rpc_error(-1, None, self.nodes[0].setticketfee, "param1")
        assert_raises_rpc_error(-1, None, self.nodes[0].setticketfee, "param1", "param2")

        # listtickets tests:
        # 1. valid parameters
        x = self.nodes[0].listtickets()
        assert(x == [])
        # TODO make this test have valid input and output once the command is implemented
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].listtickets, "param1")
        assert_raises_rpc_error(-1, None, self.nodes[0].listtickets, "param1", "param2")

if __name__ == '__main__':
    WalletTicketOperations().main()
