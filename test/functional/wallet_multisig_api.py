#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Test the wallet multisignature API.
Scenarios using operations:
    getmultisigoutinfo
    redeemmultisigout
    redeemmultisigouts
    sendtomultisig
    getmasterpubkey
"""

from test_framework.test_framework import PAIcoinTestFramework
from test_framework.util import *

class WalletMultiSigOperations(PAIcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1

    def run_test(self):
        # getmultisigoutinfo tests:
        # 1. valid parameters
        x = self.nodes[0].getmultisigoutinfo("hash", 5)
        assert(x == {})
        # TODO make this test have valid input and output once the command is implemented
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].getmultisigoutinfo)
        assert_raises_rpc_error(-1, None, self.nodes[0].getmultisigoutinfo, "param1")
        assert_raises_rpc_error(-1, None, self.nodes[0].getmultisigoutinfo, "param1", "param2")

        # redeemmultisigout tests:
        # 1. valid parameters
        x = self.nodes[0].redeemmultisigout("hash", 5, 5, "address")
        assert(x == {})
        x = self.nodes[0].redeemmultisigout("hash", 5, 5)
        assert(x == {})
        # TODO make this test have valid input and output once the command is implemented
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].redeemmultisigout)
        assert_raises_rpc_error(-1, None, self.nodes[0].redeemmultisigout, "hash")
        assert_raises_rpc_error(-1, None, self.nodes[0].redeemmultisigout, "hash", 5)
        assert_raises_rpc_error(-1, None, self.nodes[0].redeemmultisigout, "hash", 5, 5, "address", "param1")
        assert_raises_rpc_error(-1, None, self.nodes[0].redeemmultisigout, "param1", "param2")

        # redeemmultisigouts tests:
        # 1. valid parameters
        x = self.nodes[0].redeemmultisigouts("fromaddress")
        assert(x == {})
        x = self.nodes[0].redeemmultisigouts("fromaddress","toaddress")
        assert(x == {})
        x = self.nodes[0].redeemmultisigouts("fromaddress","toaddress",5)
        assert(x == {})
        # TODO make this test have valid input and output once the command is implemented
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].redeemmultisigouts)
        assert_raises_rpc_error(-1, None, self.nodes[0].redeemmultisigouts, "param1", 5)
        assert_raises_rpc_error(-1, None, self.nodes[0].redeemmultisigouts, "param1", "param2", "param3")

        # sendtomultisig tests:
        # 1. valid parameters
        x = self.nodes[0].sendtomultisig("fromaccount", 20, ["pubkey1", "pubkey2"])
        assert(x == "")
        x = self.nodes[0].sendtomultisig("fromaccount", 20, ["pubkey1", "pubkey2","pubkey3"], 2)
        assert(x == "")
        x = self.nodes[0].sendtomultisig("fromaccount", 20, ["pubkey1"], 2, 3)
        assert(x == "")
        x = self.nodes[0].sendtomultisig("fromaccount", 20, ["pubkey1", "pubkey2"], 2, 3,"comment")
        assert(x == "")
        # TODO make this test have valid input and output once the command is implemented
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].sendtomultisig)
        assert_raises_rpc_error(-1, None, self.nodes[0].sendtomultisig, "fromaccount")
        assert_raises_rpc_error(-1, None, self.nodes[0].sendtomultisig, "fromaccount", 20)
        assert_raises_rpc_error(-1, None, self.nodes[0].sendtomultisig, "fromaccount", 20, "pubkey1")
        assert_raises_rpc_error(-1, None, self.nodes[0].sendtomultisig, "fromaccount", 20, "pubkey1", 2)
        assert_raises_rpc_error(-1, None, self.nodes[0].sendtomultisig, "param1", "param2")

        # getmasterpubkey tests:
        # 1. valid parameters
        x = self.nodes[0].getmasterpubkey()
        assert(x == "")
        x = self.nodes[0].getmasterpubkey("account")
        assert(x == "")
        # TODO make this test have valid input and output once the command is implemented
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].getmasterpubkey, 5)
        assert_raises_rpc_error(-1, None, self.nodes[0].getmasterpubkey, "param1", "param2")

if __name__ == '__main__':
    WalletMultiSigOperations().main()
