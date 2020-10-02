#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Test the wallet stake API.
Scenarios using operations:
    getstakeinfo
    stakepooluserinfo
"""

from test_framework.test_framework import PAIcoinTestFramework
from test_framework.util import *

class WalletStakeOperations(PAIcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1

    def run_test(self):
        # getstakeinfo tests:
        # 1. valid parameters
        x = self.nodes[0].getstakeinfo()
        assert(len(x) == 16)
        assert(x["blockheight"] == 200)
        assert(float(x["difficulty"]) == 0.0002) # minStakeDiff param
        assert(float(x["totalsubsidy"]) == 0.0)
        # there are no tickets, all values are zero
        assert(x["ownmempooltix"] == 0)
        assert(x["immature"] == 0)
        assert(x["unspent"] == 0)
        assert(x["voted"] == 0)
        assert(x["revoked"] == 0)
        assert(x["unspentexpired"] == 0)
        assert(x["poolsize"] == 0)
        assert(x["allmempooltix"] == 0)
        assert(x["live"] == 0)
        assert(x["proportionlive"] == 0)
        assert(x["missed"] == 0)
        assert(x["proportionmissed"] == 0)
        assert(x["expired"] == 0)
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].getstakeinfo, "param1")
        assert_raises_rpc_error(-1, None, self.nodes[0].getstakeinfo, "param1", "param2")
        
        # stakepooluserinfo tests:
        # 1. valid parameters
        x = self.nodes[0].stakepooluserinfo("user")
        assert(x == {})
        # TODO make this test have valid input and output once the command is implemented
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].stakepooluserinfo)
        assert_raises_rpc_error(-1, None, self.nodes[0].stakepooluserinfo, "param1", "param2")

if __name__ == '__main__':
    WalletStakeOperations().main()