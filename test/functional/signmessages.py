#!/usr/bin/env python3
# Copyright (c) 2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test RPC commands for signing and verifying messages."""

from test_framework.test_framework import PAIcoinTestFramework
from test_framework.util import assert_equal

class SignMessagesTest(PAIcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def run_test(self):
        message = 'This is just a test message'

        self.log.info('test signing with priv_key')
        priv_key = 'aaRneEfX1zQaioZT78Zi3Cz5W4TFRyqGqYAHLcP3tsM8rLkbRmKq'
        address = 'Mf2os4mqUkE31xWEZkk8WRbBDvjmMmXpH9'
        expected_signature = 'H/dFsomHhYcYD4+g+q81PHaviAGNa30bTQa76slhd1iJF0ENLN61RfnHesKiivTcqfCO4jVsX+UIvrRV7Lb6dmk='
        signature = self.nodes[0].signmessagewithprivkey(priv_key, message)
        assert_equal(expected_signature, signature)
        assert(self.nodes[0].verifymessage(address, signature, message))

        self.log.info('test signing with an address with wallet')
        address = self.nodes[0].getnewaddress()
        signature = self.nodes[0].signmessage(address, message)
        assert(self.nodes[0].verifymessage(address, signature, message))

        self.log.info('test verifying with another address should not work')
        other_address = self.nodes[0].getnewaddress()
        other_signature = self.nodes[0].signmessage(other_address, message)
        assert(not self.nodes[0].verifymessage(other_address, signature, message))
        assert(not self.nodes[0].verifymessage(address, other_signature, message))

if __name__ == '__main__':
    SignMessagesTest().main()
