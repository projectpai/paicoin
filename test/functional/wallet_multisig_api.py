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
        self.setup_clean_chain = True
        self.num_nodes = 3

    def setup_network(self, split=False):
        super().setup_network()
        connect_nodes_bi(self.nodes,0,2)

    def getRawTxFromTxId(self, node_idx, txId):
        txDetails = self.nodes[node_idx].gettransaction(txId, True)
        rawTx = self.nodes[node_idx].decoderawtransaction(txDetails['hex'])
        return rawTx

    def findVoutPayingAmount(self, node_idx, txId, amount):
        rawTx = self.getRawTxFromTxId(node_idx, txId)
        for outpoint in rawTx['vout']:
            if outpoint['value'] == Decimal(amount):
                return outpoint
        return None

    def findVoutPayingToScriptHash(self, node_idx, txId):
        rawTx = self.getRawTxFromTxId(node_idx, txId)
        for outpoint in rawTx['vout']:
            if outpoint['scriptPubKey']['type'] == 'scripthash':
                return outpoint
        return None

    def validateFirstAddressInOutpoint(self, node_idx, outpoint):
        addr = outpoint['scriptPubKey']['addresses'][0]
        validate = self.nodes[node_idx].validateaddress(addr)
        return validate

    def run_test(self):
         #prepare some coins
        self.nodes[2].generate(1)
        self.sync_all()
        self.nodes[0].generate(101)
        self.sync_all()
        self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(),1.5)
        self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(),1.0)
        self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(),5.0)
        self.sync_all()
        self.nodes[0].generate(5)
        self.sync_all()

        # sendtomultisig tests:
        # 1. valid parameters
        addr1 = self.nodes[2].getnewaddress()
        addr2 = self.nodes[2].getnewaddress()
        addr3 = self.nodes[2].getnewaddress()

        addr1Obj = self.nodes[2].validateaddress(addr1)
        addr2Obj = self.nodes[2].validateaddress(addr2)
        addr3Obj = self.nodes[2].validateaddress(addr3)

        amount1 = 1.234
        txId1 = self.nodes[2].sendtomultisig("fromaccount", amount1, [addr1Obj['pubkey'], addr2Obj['pubkey']])
        outpoint1 = self.findVoutPayingAmount(2, txId1 , str(amount1))
        validate = self.validateFirstAddressInOutpoint(2, outpoint1)
        assert(validate['script']=='multisig')
        assert(validate['addresses'] == [addr1Obj['address'], addr2Obj['address']])
        assert(validate['sigsrequired'] == 1)

        amount2 = 2.345
        txId2 = self.nodes[2].sendtomultisig("fromaccount", amount2, [addr1Obj['pubkey'], addr2Obj['pubkey']], 2, 1 , "comment")
        outpoint2 = self.findVoutPayingAmount(2, txId2 , str(amount2))
        validate = self.validateFirstAddressInOutpoint(2, outpoint2)
        assert(validate['script']=='multisig')
        assert(validate['addresses'] == [addr1Obj['address'], addr2Obj['address']])
        assert(validate['sigsrequired'] == 2)

        amount3 = 3.456
        txId3 = self.nodes[2].sendtomultisig("fromaccount", amount3, [addr1Obj['pubkey'], addr2Obj['pubkey'], addr3Obj['pubkey']], 2)
        outpoint3 = self.findVoutPayingAmount(2, txId3 , str(amount3))
        validate = self.validateFirstAddressInOutpoint(2, outpoint3)
        assert(validate['script']=='multisig')
        assert(validate['addresses'] == [addr1Obj['address'], addr2Obj['address'], addr3Obj['address']])
        assert(validate['sigsrequired'] == 2)

        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].sendtomultisig, "fromaccount", 1.1, [addr1Obj['address'], addr2Obj['pubkey']]) # address is not allowed
        assert_raises_rpc_error(-1, None, self.nodes[0].sendtomultisig, "fromaccount", 1.1, [addr2Obj['pubkey']], 2) # required 2 provided 1
        assert_raises_rpc_error(-1, None, self.nodes[0].sendtomultisig)
        assert_raises_rpc_error(-1, None, self.nodes[0].sendtomultisig, "fromaccount")
        assert_raises_rpc_error(-1, None, self.nodes[0].sendtomultisig, "fromaccount", 20)
        assert_raises_rpc_error(-3, None, self.nodes[0].sendtomultisig, "fromaccount", 20, "pubkey1")
        assert_raises_rpc_error(-3, None, self.nodes[0].sendtomultisig, "fromaccount", 20, "pubkey1", 2)
        assert_raises_rpc_error(-1, None, self.nodes[0].sendtomultisig, "param1", "param2")


        # redeemmultisigout tests:
        # 1. valid parameters
        tree = 5 # dummy value - unused at the moment, probably will be removed
        # valid parameters w/o specific address
        redeemOut1 = self.nodes[2].redeemmultisigout(txId1, outpoint1['n'], tree)
        assert(redeemOut1['complete']==True)
        rawTx1 = self.nodes[2].decoderawtransaction(redeemOut1['hex'])
        assert(txId1 == rawTx1['vin'][0]['txid'])

        # valid parameters w/ specific address
        redeemOut2 = self.nodes[2].redeemmultisigout(txId2, outpoint2['n'], tree, addr2)
        assert(redeemOut2['complete']==True)
        rawTx2 = self.nodes[2].decoderawtransaction(redeemOut2['hex'])
        assert(txId2 == rawTx2['vin'][0]['txid'])
        assert(addr2 == rawTx2['vout'][0]['scriptPubKey']['addresses'][0])

        # we can redeem again because sendrawtransaction has not been called, test it doesn't work anymore after send
        redeemOut3 = self.nodes[2].redeemmultisigout(txId2, outpoint2['n'], tree)
        assert(redeemOut3['complete']==True)
        rawTx3 = self.nodes[2].decoderawtransaction(redeemOut3['hex'])

        # we cannot redeem again Input was already spent
        sentTxId = self.nodes[2].sendrawtransaction(redeemOut3['hex'])
        self.nodes[2].generate(1)
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()
        redeemOut4 = self.nodes[2].redeemmultisigout(txId2, outpoint2['n'], tree)
        assert(redeemOut4['complete']==False)
        assert(redeemOut4['errors'][0]['error']== 'Input not found or already spent')

        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].redeemmultisigout)
        assert_raises_rpc_error(-1, None, self.nodes[0].redeemmultisigout, "hash")
        assert_raises_rpc_error(-1, None, self.nodes[0].redeemmultisigout, "hash", 5)
        assert_raises_rpc_error(-1, None, self.nodes[0].redeemmultisigout, "hash", 5, 5, "address", "param1")
        assert_raises_rpc_error(-1, None, self.nodes[0].redeemmultisigout, "param1", "param2")


        # getmultisigoutinfo tests:
        # 1. valid parameters
        getmultisigoutinfo1 = self.nodes[2].getmultisigoutinfo(txId1, outpoint1['n'])
        assert(getmultisigoutinfo1['m'] == 1)
        assert(getmultisigoutinfo1['n'] == 2)
        assert(getmultisigoutinfo1['pubkeys'] == [addr1Obj['pubkey'], addr2Obj['pubkey']])
        assert(getmultisigoutinfo1['spent'] == False)
        assert(getmultisigoutinfo1['amount'] == Decimal(str(amount1)))

        getmultisigoutinfo2 = self.nodes[2].getmultisigoutinfo(txId2, outpoint2['n'])
        assert(getmultisigoutinfo2['m'] == 2)
        assert(getmultisigoutinfo2['n'] == 2)
        assert(getmultisigoutinfo2['pubkeys'] == [addr1Obj['pubkey'], addr2Obj['pubkey']])
        assert(getmultisigoutinfo2['spent'] == True)
        assert(getmultisigoutinfo2['spentby'] == sentTxId)
        assert(getmultisigoutinfo2['spentbyindex'] == 0)
        assert(getmultisigoutinfo2['amount'] == Decimal(str(amount2)))

        getmultisigoutinfo3 = self.nodes[2].getmultisigoutinfo(txId3, outpoint3['n'])
        assert(getmultisigoutinfo3['m'] == 2)
        assert(getmultisigoutinfo3['n'] == 3)
        assert(getmultisigoutinfo3['pubkeys'] == [addr1Obj['pubkey'], addr2Obj['pubkey'], addr3Obj['pubkey']])
        assert(getmultisigoutinfo3['spent'] == False)

        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].getmultisigoutinfo)
        assert_raises_rpc_error(-1, None, self.nodes[0].getmultisigoutinfo, "param1")
        assert_raises_rpc_error(-1, None, self.nodes[0].getmultisigoutinfo, "param1", "param2")


        # redeemmultisigouts tests:
        # 1. valid parameters
        listunspent = self.nodes[2].listunspent()
        scriptHashes = [tx['address'] for tx in listunspent if 'redeemScript' in tx]
        assert(len(scriptHashes) == 2)

        redeemmultisigouts1 = self.nodes[2].redeemmultisigouts(scriptHashes[0])
        assert(len(redeemmultisigouts1) == 1)
        assert(redeemmultisigouts1[0]['complete']==True)

        toaddr = self.nodes[2].getnewaddress()
        redeemmultisigouts2 = self.nodes[2].redeemmultisigouts(scriptHashes[1], toaddr)
        assert(len(redeemmultisigouts2) == 1)
        assert(redeemmultisigouts2[0]['complete']==True)

        self.nodes[2].sendtoaddress(scriptHashes[0], 1.1)
        self.nodes[2].sendtoaddress(scriptHashes[0], 1.2)
        self.nodes[2].sendtoaddress(scriptHashes[0], 1.3)
        self.nodes[2].sendtoaddress(scriptHashes[0], 1.4)
        self.nodes[2].sendtoaddress(scriptHashes[0], 1.5)
        self.nodes[2].generate(1)
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        listunspent = self.nodes[2].listunspent()
        scriptHashes = [tx['address'] for tx in listunspent if 'redeemScript' in tx]
        assert(len(scriptHashes) == 5)

        redeemmultisigouts3 = self.nodes[2].redeemmultisigouts(scriptHashes[0], toaddr)
        assert(len(redeemmultisigouts3) == 5)
        for it in redeemmultisigouts3:
            assert(it['complete']==True)

        redeemmultisigouts4 = self.nodes[2].redeemmultisigouts(scriptHashes[0], toaddr, 3)
        assert(len(redeemmultisigouts4) == 3)
        for it in redeemmultisigouts4:
            assert(it['complete']==True)

        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].redeemmultisigouts)
        assert_raises_rpc_error(-8, None, self.nodes[0].redeemmultisigouts, "param1", 5)
        assert_raises_rpc_error(-8, None, self.nodes[0].redeemmultisigouts, "param1", "param2", "param3")


        # getmasterpubkey tests:
        # 1. valid parameters
        accounts = self.nodes[0].listaccounts()
        assert(len(accounts) == 1)
        assert(accounts[''] != None)

        # def account has one address and its pubkey is equal to the one that getmasterpubkey gets
        defacc_name = ''
        defacc_pk = self.nodes[0].getmasterpubkey()
        addresses = self.nodes[0].getaddressesbyaccount(defacc_name)
        assert(len(addresses) == 1)
        val_addr = self.nodes[0].validateaddress(addresses[0])
        assert(val_addr['pubkey'] == defacc_pk)

        # called with nonexistent account, creates account and works as default
        myacc_name = "myaccount"
        myacc_pk = self.nodes[0].getmasterpubkey(myacc_name)

        accounts = self.nodes[0].listaccounts()
        assert(len(accounts) == 2)
        assert(accounts[''] != None)
        assert(accounts[myacc_name] != None)
        addresses = self.nodes[0].getaddressesbyaccount(myacc_name)
        assert(len(addresses) == 1)
        val_addr = self.nodes[0].validateaddress(addresses[0])
        assert(val_addr['pubkey'] == myacc_pk)

        # while the account has one address, its pubkey is the masterpubkey
        # if we get a new address masterkey remains equal to the first one
        new_addr = self.nodes[0].getnewaddress(myacc_name)
        addresses = self.nodes[0].getaddressesbyaccount(myacc_name)
        assert(len(addresses) == 2)
        myacc_pk_after_new_addr = self.nodes[0].getmasterpubkey(myacc_name)
        assert(myacc_pk == myacc_pk_after_new_addr)
        val_addr1 = self.nodes[0].validateaddress(addresses[0])
        val_addr2 = self.nodes[0].validateaddress(addresses[1])
        assert(myacc_pk in [val_addr1['pubkey'],val_addr2['pubkey']])
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].getmasterpubkey, 5)
        assert_raises_rpc_error(-1, None, self.nodes[0].getmasterpubkey, "param1", "param2")

if __name__ == '__main__':
    WalletMultiSigOperations().main()
