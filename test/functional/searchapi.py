#!/usr/bin/env python3
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test Stake API implementation

Scenarios:
- existsaddress
- existsaddresses
- existsmempooltxs
"""

import os
import time

from test_framework.test_framework import PAIcoinTestFramework
from test_framework import util

class SearchAPITest(PAIcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.enable_mocktime()

    def enable_mocktime (self):
        self.mocktime = 1529934120 # Monday, June 25, 2018 1:42:00 PM GMT
    
    def run_test(self):
        self.nodes[0].generate(200)
        self.sync_all()
        
        self.test_existsaddress()
        self.test_existsaddresses()
        self.test_existsmempooltxs()
    
    def test_existsaddress(self):
        newaddress = self.nodes[0].getnewaddress()
        assert newaddress is not None

        txid = self.nodes[0].sendtoaddress(newaddress, 1)
        assert txid is not None

        self.sync_all()

        existsaddr = self.nodes[0].existsaddress(newaddress)
        assert existsaddr

        existsaddr = self.nodes[0].existsaddress('aaaaaaaaaaa')
        assert not existsaddr

    def test_existsaddresses(self):
        newaddresses = [self.nodes[0].getnewaddress(), self.nodes[0].getnewaddress()]
        assert newaddresses is not None

        for newaddr in newaddresses:
            txid = self.nodes[0].sendtoaddress(newaddr, 1)
            assert txid is not None

        self.sync_all()

        existaddrs = self.nodes[0].existsaddresses(newaddresses)
        assert existaddrs is not None
        assert existaddrs == "03" # first 2 bits of a byte showing that both addresses exist

        existsaddrs = self.nodes[0].existsaddresses(["{'address' : 'aaaaa'"])
        assert existsaddrs is not None
        assert existsaddrs == "00"

    def test_existsmempooltxs(self):
        newaddress = self.nodes[0].getnewaddress()
        assert newaddress is not None

        txid = self.nodes[0].sendtoaddress(newaddress, 1)
        assert txid is not None

        self.sync_all()

        txhashblob = txid + ("00" * 32) # append one invalid tx hash
        existmempooltxs = self.nodes[0].existsmempooltxs(txhashblob)
        assert existmempooltxs is not None
        assert existmempooltxs == "01" # only the first transaction should be present

if __name__ == '__main__':
    SearchAPITest().main()
