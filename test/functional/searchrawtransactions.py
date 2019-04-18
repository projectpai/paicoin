#!/usr/bin/env python3
# Copyright (c) 2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test the searchrawtransactions RPC commands on a setup having the 'addrindex' flag set
    - setup, generate coins and new addreses
    - make a few transactions
    - test the result of 'searchrawtransactions'
"""

from test_framework.test_framework import PAIcoinTestFramework
from test_framework.util import *

def checkResultAgainstDecodedRawTx(resultTx, decodedRawTx, prevDecodedTxs = None):
    # check several attributes to be as in our decoded element
    assert_equal(resultTx["txid"],     decodedRawTx["txid"])
    assert_equal(resultTx["version"],  decodedRawTx["version"])
    assert_equal(resultTx["size"],     decodedRawTx["size"])
    assert_equal(resultTx["locktime"], decodedRawTx["locktime"])
    assert_equal(resultTx["vout"],     decodedRawTx["vout"])
    if (prevDecodedTxs == None):
        assert_equal(resultTx["vin"],  decodedRawTx["vin"])
    else:
        assert_equal(len(resultTx["vin"]),  len(decodedRawTx["vin"]))
        # add prevOut to the one in decoded to able to check everything else in vin
        addedPrevOut = [vin for vin in decodedRawTx["vin"]]
        for i in range(len(addedPrevOut)):
            addedPrevOut[i]["prevOut"] = resultTx["vin"][i]["prevOut"] 
        assert_equal(resultTx["vin"],  addedPrevOut)

        # check prevOut
        assert_equal(len(resultTx["vin"]),  len(prevDecodedTxs))
        for i in range(len(prevDecodedTxs)):
            resPrevOut = resultTx["vin"][i]["prevOut"]
            prevOut = prevDecodedTxs[i]["vout"]
            assert_equal(len(resPrevOut), len(prevOut))
            for j in range(len(prevOut)):
                assert_equal(resPrevOut[j]["value"], prevOut[j]["value"])
                assert_equal(resPrevOut[j]["addresses"], prevOut[j]["scriptPubKey"]["addresses"])
    
def getSendingAddress(decRawTxs, index):
    addr = [vout["scriptPubKey"]["addresses"] for vout in decRawTxs[index]["vout"] if vout["value"] == Decimal(index+1)]
    return addr[0]

class SearchRawTransactionsTest(PAIcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        # only first node has txindex to be able to test cases when it is required
        self.extra_args = [['-addrindex','-txindex'],['-addrindex']] 

    def sendandgenerate(self, address, amount):
        txid = self.nodes[0].sendtoaddress(address,amount)
        self.nodes[0].generate(1)
        self.sync_all()
        return txid

    def run_test(self):
        #prepare
        self.nodes[0].generate(101)
        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()

        addrs = [ self.nodes[n].getnewaddress() for n in range(self.num_nodes) ]
        self.log.info(addrs)

        txids = [ self.sendandgenerate(addrs[1],i) for i in range(1,21) ]
        rawTxs = [ self.nodes[0].getrawtransaction(txid) for txid in txids ]
        decRawTxs = [ self.nodes[0].decoderawtransaction(rawTx) for rawTx in rawTxs ]

        #################
        # Verbose tests #
        #################
        verbose = 1
        res = self.nodes[0].searchrawtransactions(addrs[1], verbose)
        assert_equal(len(res), len(txids))

        res = self.nodes[0].searchrawtransactions(addrs[1], verbose, 5, 2)
        assert_equal(len(res), 2)

        # check one skip zero
        res = self.nodes[0].searchrawtransactions(addrs[1], verbose, 0, 1)
        assert_equal(len(res), 1)
        # check if it returns the same txid as in our list 
        assert_equal(res[0]["txid"], txids[0])
        checkResultAgainstDecodedRawTx(res[0], decRawTxs[0])

        # check one skip rest
        res = self.nodes[0].searchrawtransactions(addrs[1], verbose, 19, 1)
        assert_equal(len(res), 1)
        # check if it returns the same txid as in our list 
        assert_equal(res[0]["txid"], txids[-1])
        checkResultAgainstDecodedRawTx(res[0], decRawTxs[-1])

        # check with vinextra
        assert_raises_rpc_error(-1, "Transaction index not enabled", self.nodes[1].searchrawtransactions, addrs[1], verbose, 19, 1, True)

        res = self.nodes[0].searchrawtransactions(addrs[1], verbose, 19, 1, True)
        assert_equal(len(res), 1)
        # check if it returns the same txid as in our list 
        assert_equal(res[0]["txid"], txids[-1])
        checkResultAgainstDecodedRawTx(res[0], decRawTxs[-1], [decRawTxs[-2]])

        # check reverse
        res = self.nodes[0].searchrawtransactions(addrs[1], verbose, 0, 5, False, False)
        reversed_res = self.nodes[0].searchrawtransactions(addrs[1], verbose, 0, 5, False, True)
        assert_equal(res, list(reversed(reversed_res)))
        
        # check filteraddrs
        assert_raises_rpc_error(-1, "Transaction index not enabled", self.nodes[1].searchrawtransactions, addrs[1], verbose, 19, 1, False, False, [addrs[0]])

        sending_addr1 = getSendingAddress(decRawTxs, 7)
        sending_addr2 = getSendingAddress(decRawTxs, 9)
        assert_equal(sending_addr1, sending_addr2)
        res = self.nodes[0].searchrawtransactions(addrs[1], verbose, 0, 100, False, False, sending_addr1)
        # all transactions sent from the same address
        assert_equal(len(res), len(txids))

        res = self.nodes[0].searchrawtransactions(addrs[1], verbose, 0, 100, False, False, [addrs[0]])
        assert_equal(len(res), 0)

        #####################
        # Non-verbose tests #
        #####################
        verbose = 0
        res = self.nodes[0].searchrawtransactions(addrs[1], verbose, 2, 2)
        assert_equal(len(res), 2)

        res = self.nodes[0].searchrawtransactions(addrs[1], verbose, 2)
        assert_equal(len(res), 18)

        # check one skip zero
        res = self.nodes[0].searchrawtransactions(addrs[1], verbose, 0, 1)
        assert_equal(len(res), 1)
        assert_equal(res[0], rawTxs[0])

        # check one skip rest
        res = self.nodes[0].searchrawtransactions(addrs[1], verbose, 19, 1)
        assert_equal(len(res), 1)
        assert_equal(res[0], rawTxs[-1])

        # check with vinextra [ disregarded when not verbose ]
        res = self.nodes[0].searchrawtransactions(addrs[1], verbose, 19, 1, True)
        assert_equal(len(res), 1)
        assert_equal(res[0], rawTxs[-1])

        # check reverse
        res = self.nodes[0].searchrawtransactions(addrs[1], verbose, 0, 5, False, False)
        reversed_res = self.nodes[0].searchrawtransactions(addrs[1], verbose, 0, 5, False, True)
        assert_equal(res, list(reversed(reversed_res)))

        # check filteraddrs [ disregarded when not verbose ]
        res = self.nodes[0].searchrawtransactions(addrs[1], verbose, 0, 100, False, False, [addrs[0]])
        assert_equal(res, rawTxs)

if __name__ == "__main__":
    SearchRawTransactionsTest().main()