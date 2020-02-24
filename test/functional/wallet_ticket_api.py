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
from test_framework.mininode import COIN

class WalletTicketOperations(PAIcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2

    def setup_network(self, split=False):
        super().setup_network()
        connect_nodes_bi(self.nodes,0,1)

    def getOutpointValue(self, node_idx, outputPoint):
        rawtx = self.nodes[node_idx].getrawtransaction(outputPoint[0])
        decoded = self.nodes[node_idx].decoderawtransaction(rawtx)
        return decoded["vout"][outputPoint[1]]["value"]

    def validatePurchaseTicketTx(self, txids, node_idx, ticketAddress = None):
        for txid in txids:
            tx = self.nodes[node_idx].gettransaction(txid)
            decoded = self.nodes[node_idx].decoderawtransaction(tx["hex"])

            assert(len(decoded["vin"]) == 1)
            paying_op = (decoded["vin"][0]["txid"], decoded["vin"][0]["vout"])
            payedValue = self.getOutpointValue(node_idx, paying_op)

            assert(len(decoded["vout"]) == 4) # Buy Decl + Stake payment + reward for contribution 1 + change for contribution 1

            buyDeclOut = decoded["vout"][0]
            assert(buyDeclOut["value"] == 0.0)
            assert(buyDeclOut["scriptPubKey"]["type"] == "structdata")

            stakePaymentOut = decoded["vout"][1]
            assert(payedValue > stakePaymentOut["value"])
            assert(stakePaymentOut["value"] > 0.0) # TODO this must be equal to ticket price: estimatestakediff
            assert(stakePaymentOut["scriptPubKey"]["type"] == "pubkeyhash")
            assert(len(stakePaymentOut["scriptPubKey"]["addresses"]))
            if ticketAddress != None:
                assert(stakePaymentOut["scriptPubKey"]["addresses"][0] == ticketAddress)

            reward1stContribOut = decoded["vout"][2]
            assert(reward1stContribOut["value"] == 0.0) # TODO extract contrib amount and check is equal to the input and greater than the ticket price
            assert(reward1stContribOut["scriptPubKey"]["type"] == "structdata")
            asm = reward1stContribOut["scriptPubKey"]["asm"].split()
            assert(asm[0] == "OP_RETURN")
            assert(asm[1] == "OP_STRUCT")
            assert(asm[2] == "1") #verion
            assert(asm[3] == "0") #CLASS_STAKING
            assert(asm[4] == "1") #STAKE_TicketContribution
            assert(asm[5] == "1") #ContribVersion
            # asm[6] is the reward address
            assert(payedValue * COIN == Decimal(asm[7])) # contribAmount

            changeFor1stContribOut = decoded["vout"][3]
            assert(changeFor1stContribOut["value"] >= 0.0)
            assert(changeFor1stContribOut["scriptPubKey"]["type"] == "pubkeyhash")

    
    def run_test(self):
        #prepare some coins
        self.nodes[0].generate(1)
        self.sync_all()
        self.nodes[1].generate(101)
        self.sync_all()
        self.nodes[1].sendtoaddress(self.nodes[0].getnewaddress(),5.1)
        self.nodes[1].sendtoaddress(self.nodes[0].getnewaddress(),5.2)
        self.nodes[1].sendtoaddress(self.nodes[0].getnewaddress(),5.3)
        self.sync_all()
        self.nodes[0].generate(5)
        self.sync_all()


        # getticketfee tests:
        # 1. valid parameters
        x = self.nodes[0].getticketfee()
        assert(x == 0)
        # TODO make this test have valid input and output once the command is implemented
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].getticketfee, "param1")
        assert_raises_rpc_error(-1, None, self.nodes[0].getticketfee, "param1", "param2")

        # purchaseticket tests:
        # 1. valid parameters
        txids = self.nodes[0].purchaseticket("", 1.5)
        assert(len(txids) == 1)
        self.validatePurchaseTicketTx(txids, 0)
        total_purchased_tickets = txids

        txids = self.nodes[0].purchaseticket("default", 2.3)
        assert(len(txids) == 1)
        self.validatePurchaseTicketTx(txids, 0)
        total_purchased_tickets.extend(txids)

        minconf = 2
        txids = self.nodes[0].purchaseticket("default", 2.3, minconf)
        assert(len(txids) == 1)
        self.validatePurchaseTicketTx(txids, 0)
        total_purchased_tickets.extend(txids)

        ticketaddr = self.nodes[0].getnewaddress()
        assert(len(txids) == 1)
        txids = self.nodes[0].purchaseticket("default", 2.3, 1, ticketaddr)
        self.validatePurchaseTicketTx(txids, 0, ticketaddr)
        total_purchased_tickets.extend(txids)

        txids = self.nodes[0].purchaseticket("default", 2.3, 1, "") # a new ticketaddr is generated automatically
        assert(len(txids) == 1)
        self.validatePurchaseTicketTx(txids, 0)
        total_purchased_tickets.extend(txids)

        tickets_to_buy = 2
        txids = self.nodes[0].purchaseticket("default", 2.3, 1, ticketaddr, tickets_to_buy) # buy multiple tickets
        assert(len(txids) == 2)
        self.validatePurchaseTicketTx(txids, 0, ticketaddr)
        total_purchased_tickets.extend(txids)

        expiry = 500
        tickets_to_buy = 1
        txids = self.nodes[0].purchaseticket("default", 2.3, 1, ticketaddr, tickets_to_buy, "", 0.0, expiry)
        assert(len(txids) == 1)
        self.validatePurchaseTicketTx(txids, 0, ticketaddr)
        total_purchased_tickets.extend(txids)

        ticketFeeRate = 0.5 # TODO this is unused at the moment, update this value to a valid one
        txids = self.nodes[0].purchaseticket("default", 2.3, 1, ticketaddr, tickets_to_buy, "", 0.0, expiry, "unused", ticketFeeRate)
        assert(len(txids) == 1)
        self.validatePurchaseTicketTx(txids, 0, ticketaddr)
        total_purchased_tickets.extend(txids)

        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].purchaseticket)
        # TODO adjust the spendlimit value using estimatestakediff, now it uses a hardcoded ticketPrice
        assert_raises_rpc_error(-8, None, self.nodes[0].purchaseticket,"", 0.00034) # ticket price above spend limit (-8)
        assert_raises_rpc_error(-1, None, self.nodes[0].purchaseticket, "param1")
        assert_raises_rpc_error(-3, None, self.nodes[0].purchaseticket, "param1", "param2")
        assert_raises_rpc_error(-1, None, self.nodes[0].purchaseticket, "param1", 1, "param2")
        assert_raises_rpc_error(-5, None, self.nodes[0].purchaseticket, "param1", 1, 1 , "param2")
        assert_raises_rpc_error(-1, None, self.nodes[0].purchaseticket, "param1", 1, 1 , ticketaddr, "param2")
        assert_raises_rpc_error(-5, None, self.nodes[0].purchaseticket, "param1", 1, 1 , ticketaddr, 1, "param2")

        pooladdr = self.nodes[0].getnewaddress()
        assert_raises_rpc_error(-1, None, self.nodes[0].purchaseticket, "param1", 1, 1 , ticketaddr, 1, pooladdr, "param2")
        assert_raises_rpc_error(-8, None, self.nodes[0].purchaseticket, "default", 2.3, 1, ticketaddr, 2, pooladdr, 0.0) # when a pool address is given poolFee cannot be zero
        assert_raises_rpc_error(-8, None, self.nodes[0].purchaseticket, "default", 2.3, 1, ticketaddr, 2, pooladdr, 0.1) # using pool address is not yet implemented
        assert_raises_rpc_error(-1, None, self.nodes[0].purchaseticket, "param1", 1, 1 , ticketaddr, 1, pooladdr, 1, "param2")
        assert_raises_rpc_error(-8, None, self.nodes[0].purchaseticket, "default", 2.3, 1, ticketaddr, 1, "", 0.0, 10) # expiry height must be above next block height
        assert_raises_rpc_error(-3, None, self.nodes[0].purchaseticket, "param1", 1, 1 , ticketaddr, 1, pooladdr, 1, 1, "", "param2")

        # gettickets tests:
        # 1. valid parameters
        include_immature = False
        tickets = self.nodes[0].gettickets(include_immature)
        # all purchased tickets should be immature, so list is empty
        assert_equal(len(tickets["hashes"]), 0)

        include_immature = True
        tickets = self.nodes[0].gettickets(include_immature)
        assert_equal(len(tickets["hashes"]), len(total_purchased_tickets))
        assert(set(tickets["hashes"]) == set(total_purchased_tickets))

        self.nodes[0].generate(15)
        self.sync_all()

        # not yet matured need to have 16 confirmations
        include_immature = False
        tickets = self.nodes[0].gettickets(include_immature)
        assert_equal(len(tickets["hashes"]), 0)

        #purchase one more ticket
        txids = self.nodes[0].purchaseticket("", 1.5)
        assert(len(txids) == 1)
        include_immature = True
        tickets = self.nodes[0].gettickets(include_immature)
        assert_equal(len(tickets["hashes"]), len(total_purchased_tickets) + 1)
        assert(set(tickets["hashes"]) >= set(total_purchased_tickets))
        assert(txids[0] in tickets["hashes"])

        self.nodes[0].generate(1)
        self.sync_all()

        # TODO reenable this piece one the  nStakeEnabledHeight gets a lower value, now is 2000
        # all must tickets have matured (15 + 1 confirmations), except the last purchased one
        # include_immature = False
        # tickets = self.nodes[0].gettickets(include_immature)
        # assert_equal(len(tickets["hashes"]), len(total_purchased_tickets)) 
        # assert(set(tickets["hashes"]) == set(total_purchased_tickets))

        # TODO make this test have valid input and output once the command is implemented
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].gettickets)
        assert_raises_rpc_error(-1, None, self.nodes[0].gettickets, "param1")
        assert_raises_rpc_error(-1, None, self.nodes[0].gettickets, "param1", "param2")

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
