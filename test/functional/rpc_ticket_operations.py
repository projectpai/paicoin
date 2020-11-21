#!/usr/bin/env python3
# Copyright (c) 2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test the search operations RPC commands
    - ticket operations (ported from Decred) include: 
        existsexpiredtickets
        existsliveticket
        existslivetickets
        existsmissedtickets
        getticketpoolvalue
        livetickets
        missedtickets
        ticketfeeinfo
        ticketsforaddress
        ticketvwrap
"""

from test_framework.test_framework import PAIcoinTestFramework
from test_framework.util import *

def have_same_elements(left, right):
    return len(left) == len(right) and set(left) == set(right)

class TicketOperations(PAIcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [['-txindex','-autovote=0','-autorevoke=0']]

    def test_tx_type_info_last_block(self, expected_tx_types = []):
        # test getblock and getrawtransaction
        chainInfo = self.nodes[0].getblockchaininfo()
        last_block = self.nodes[0].getblock(chainInfo['bestblockhash'],2)
        for tx in last_block['tx']:
            raw_tx = self.nodes[0].getrawtransaction(tx['txid'],True)
            # print(raw_tx)
            raw_tx_type = raw_tx['type']
            assert(raw_tx_type in expected_tx_types)
            assert(raw_tx_type == tx['type']) # check that type is present in both raw and block output

            if raw_tx_type == 'stake_purchase' or raw_tx_type == 'stake_revocation':
                assert(raw_tx['staking']['ticket_price'] > 0)
                assert(raw_tx['staking']['fee_limit'] > 0)
                assert(raw_tx['staking'] == tx['staking'])# check that same info is present in both raw and block output

            elif raw_tx_type == 'vote':
                assert(raw_tx['voting']['version'] == 1)
                assert(raw_tx['voting']['vote'] == 'valid' or raw_tx['voting']['vote'] == 'invalid')
                assert(raw_tx['voting'] == tx['voting'])# check that sme info is present in both raw and block output


    def run_test(self):
        # these are the same values as in chainparams.cpp for REGTEST, update them if they change
        nStakeEnabledHeight    = 2000
        nStakeValidationHeight = 2100
        nTicketMaturity        = 8
        nMaxFreshStakePerBlock = 20
        nTicketsPerBlock       = 5
        nTicketPoolSize        = 64
        nTicketExpiry          = 3 * nTicketPoolSize
        nStakeDiffWindowSize   = 8

        nPurchaseMaxHeight     = 2067

        self.nodes[0].generate(nStakeEnabledHeight - nTicketMaturity - 1)
        self.sync_all()

        txaddress = self.nodes[0].getnewaddress()
        print(txaddress)
        rewardaddress = self.nodes[0].getnewaddress()

        txs = []
        for blkidx in range(nStakeEnabledHeight - nTicketMaturity , nStakeEnabledHeight+1):
            funds = self.nodes[0].getbalance()
            print("funds:", funds)
            ticketPrice = self.nodes[0].getstakedifficulty()
            print("ticketPrice", ticketPrice)
            print("purchase", nMaxFreshStakePerBlock, "at", blkidx)
            txs.append(self.nodes[0].purchaseticket("", 1.5, 1, txaddress, rewardaddress, nMaxFreshStakePerBlock))
            assert(len(txs[-1])==nMaxFreshStakePerBlock)
            
            stakeinfo = self.nodes[0].getstakeinfo()
            # print(stakeinfo)
            assert(stakeinfo['ownmempooltix'] == nMaxFreshStakePerBlock)
            assert(stakeinfo['ownmempooltix'] == stakeinfo['allmempooltix']) # only our ticket tx are in mempool
            assert(stakeinfo['immature'] == stakeinfo['unspent'])
            assert(stakeinfo['poolsize'] == 0)
            assert(stakeinfo['live'] == 0)
            assert(stakeinfo['missed'] == 0)
            assert(stakeinfo['expired'] == 0)
            assert(stakeinfo['voted'] == 0)
            assert(stakeinfo['revoked'] == 0)

            self.nodes[0].generate(1)
            self.sync_all()
            chainInfo = self.nodes[0].getblockchaininfo()
            assert(chainInfo['blocks'] == blkidx)

            stakeinfo = self.nodes[0].getstakeinfo()
            assert(stakeinfo['ownmempooltix'] == 0) # no ticket tx in mempool after generate
            assert(stakeinfo['ownmempooltix'] == stakeinfo['allmempooltix'])

        self.test_tx_type_info_last_block(['coinbase', 'standard', 'stake_purchase'])

        # we should be at nStakeEnabledHeight, so only tickets in txs[0] should be live
        assert(blkidx == nStakeEnabledHeight)

        # existsexpiredtickets tests:
        # 1. valid parameters
        x = self.nodes[0].existsexpiredtickets(txs[0])
        assert(have_same_elements(x.keys(), txs[0]))
        assert(not any(x.values())) # no ticket exists so all False
        x = self.nodes[0].existsexpiredtickets(txs[1])
        assert(have_same_elements(x.keys(), txs[1]))
        assert(not any(x.values())) # no ticket exists so all False
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].existsexpiredtickets)
        assert_raises_rpc_error(-1, None, self.nodes[0].existsexpiredtickets, "param1", "param2")
        assert_raises_rpc_error(-1, None, self.nodes[0].existsexpiredtickets, ["hash1", "hash2"])

        # existsliveticket tests:
        # 1. valid parameters
        x = self.nodes[0].existsliveticket(txs[0][5])
        assert(x == True)  # first bought txs are live since we are at nStakeEnabledHeight
        x = self.nodes[0].existsliveticket(txs[1][4])
        assert(x == False)
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].existsliveticket)
        assert_raises_rpc_error(-1, None, self.nodes[0].existsliveticket, "param1", "param2")
        assert_raises_rpc_error(-1, None, self.nodes[0].existsliveticket,"txhash")


        # existslivetickets tests:
        # 1. valid parameters
        x = self.nodes[0].existslivetickets(txs[0])
        assert(have_same_elements(x.keys(), txs[0]))
        assert(any(x.values())) # all tickets bought first are live
        x = self.nodes[0].existslivetickets(txs[1])
        assert(have_same_elements(x.keys(), txs[1]))
        assert(not any(x.values())) # these are bought one block after so not live yet
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].existslivetickets)
        assert_raises_rpc_error(-1, None, self.nodes[0].existslivetickets, "param1", "param2")
        assert_raises_rpc_error(-1, None, self.nodes[0].existslivetickets, ["hash1", "hash2"])

        # getstakeinfo tests:
        stakeinfo = self.nodes[0].getstakeinfo()
        assert(stakeinfo['immature'] + stakeinfo['live'] == stakeinfo['unspent']) # tickets bought first are live
        assert(stakeinfo['live'] == stakeinfo['poolsize'])
        assert(stakeinfo['proportionlive'] == 1) # all live are in pool

        # existsmissedtickets tests:
        # 1. valid parameters
        x = self.nodes[0].existsmissedtickets(txs[0])
        assert(have_same_elements(x.keys(), txs[0]))
        assert(not any(x.values())) # no ticket exists so all False
        x = self.nodes[0].existsmissedtickets(txs[1])
        assert(have_same_elements(x.keys(), txs[1]))
        assert(not any(x.values())) # no ticket exists so all False
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].existsmissedtickets)
        assert_raises_rpc_error(-1, None, self.nodes[0].existsmissedtickets, "param1", "param2")
        assert_raises_rpc_error(-1, None, self.nodes[0].existsmissedtickets, ["hash1", "hash2"])


        # # getticketpoolvalue tests:
        # # 1. valid parameters
        x = self.nodes[0].getticketpoolvalue()
        ticketPrice = self.nodes[0].getstakedifficulty()
        currentTicketPrice = float(ticketPrice["current"])
        assert(float(x) == currentTicketPrice * len(txs[0])) # all live tickets have payed the same price
        # # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].getticketpoolvalue, 'param')
        assert_raises_rpc_error(-1, None, self.nodes[0].getticketpoolvalue, "param1", "param2")


        # livetickets tests:
        # 1. valid parameters
        x = self.nodes[0].livetickets()
        assert(have_same_elements(x["tickets"], txs[0]))
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].livetickets, 'param')
        assert_raises_rpc_error(-1, None, self.nodes[0].livetickets, "param1", "param2")


        # missedtickets tests:
        # 1. valid parameters
        x = self.nodes[0].missedtickets()
        assert(x == { "tickets": [] })
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].missedtickets, 'param')
        assert_raises_rpc_error(-1, None, self.nodes[0].missedtickets, "param1", "param2")


        # ticketfeeinfo tests:
        # 1. valid parameters
        x = self.nodes[0].ticketfeeinfo() # at this point mempool is empty, so no fee info
        assert(x == {'feeinfomempool': {'number': 0}, 'feeinfoblocks': [], 'feeinfowindows': []})
        x = self.nodes[0].ticketfeeinfo(3)   # set blocks
        assert(x['feeinfomempool']['number'] == 0)
        assert(len(x['feeinfoblocks']) == 3)
        assert(x['feeinfoblocks'][0]['height'] == nStakeEnabledHeight)
        assert(x['feeinfoblocks'][0]['number'] == nMaxFreshStakePerBlock)
        assert(len(x['feeinfowindows']) == 0)
        x = self.nodes[0].ticketfeeinfo(3,2) # set blocks and windows
        assert(x['feeinfomempool']['number'] == 0)
        assert(len(x['feeinfoblocks']) == 3)
        assert(len(x['feeinfowindows']) == 2)
        assert(x['feeinfowindows'][-1]['endheight'] - x['feeinfowindows'][-1]['startheight']  == nStakeDiffWindowSize)
        assert(x['feeinfowindows'][-1]['number'] == nMaxFreshStakePerBlock * nStakeDiffWindowSize)
        # TODO add checks for the fee rate results when the fees/ticket prices are not hardcoded
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].ticketfeeinfo, "param1", "param2", "param3")

        # ticketsforaddress tests:
        # 1. valid parameters
        unused = self.nodes[0].getnewaddress()
        x = self.nodes[0].ticketsforaddress(unused)
        assert(x == { "tickets": [] })
        x = self.nodes[0].ticketsforaddress(txaddress)
        assert(have_same_elements(x["tickets"], txs[0]))
        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].ticketsforaddress)
        assert_raises_rpc_error(-5, None, self.nodes[0].ticketsforaddress, 'param')
        assert_raises_rpc_error(-1, None, self.nodes[0].ticketsforaddress, "param1", "param2")


        # ticketvwap tests:
        # 1. valid parameters
        x = self.nodes[0].ticketvwap()
        assert(float(x) == currentTicketPrice)
        x = self.nodes[0].ticketvwap(10)    # set start
        assert(float(x) == currentTicketPrice)
        x = self.nodes[0].ticketvwap(10, 20) # set start and stop
        assert(x == 0.000)
        # TODO make this test have valid input and output once the command is implemented
        # the command was implemented, but to give non-zero results we need to first set a nStakeDiffuculty correctly
        # 2. invalid parameters
        assert_raises_rpc_error(-8, None, self.nodes[0].ticketvwap, nStakeEnabledHeight + 1, nStakeEnabledHeight)
        assert_raises_rpc_error(-8, None, self.nodes[0].ticketvwap, nStakeEnabledHeight - 1, nStakeEnabledHeight + 1)
        assert_raises_rpc_error(-1, None, self.nodes[0].ticketvwap, "param1")
        assert_raises_rpc_error(-1, None, self.nodes[0].ticketvwap, "param1", "param2")
        assert_raises_rpc_error(-1, None, self.nodes[0].ticketvwap, "param1", "param2", "param3")

        self.nodes[0].generate(1)
        self.sync_all()
        chainInfo = self.nodes[0].getblockchaininfo()
        assert(chainInfo['blocks'] == nStakeEnabledHeight + 1)

        # at this height more also tickets in txs[1] should be live, but missed and expired should be the same
        x = self.nodes[0].existsexpiredtickets(txs[0])
        assert(have_same_elements(x.keys(), txs[0]))
        assert(not any(x.values())) # no ticket exists so all False
        x = self.nodes[0].existsexpiredtickets(txs[1])
        assert(have_same_elements(x.keys(), txs[1]))
        assert(not any(x.values())) # no ticket exists so all False

        x = self.nodes[0].existsliveticket(txs[0][5])
        assert(x == True)
        x = self.nodes[0].existsliveticket(txs[1][4])
        assert(x == True)
        
        x = self.nodes[0].existslivetickets(txs[0])
        assert(have_same_elements(x.keys(), txs[0]))
        assert(any(x.values())) # all tickets bought first are live
        x = self.nodes[0].existslivetickets(txs[1])
        assert(have_same_elements(x.keys(), txs[1]))
        assert(any(x.values())) # these are also live

        x = self.nodes[0].existsmissedtickets(txs[0])
        assert(have_same_elements(x.keys(), txs[0]))
        assert(not any(x.values())) # no ticket exists so all False
        x = self.nodes[0].existsmissedtickets(txs[1])
        assert(have_same_elements(x.keys(), txs[1]))
        assert(not any(x.values())) # no ticket exists so all False

        x = self.nodes[0].livetickets()
        assert(have_same_elements(x["tickets"], txs[0] + txs[1]))

        x = self.nodes[0].missedtickets()
        assert(x == { "tickets": [] })

        # build the chain up to nStakeValidationHeight - 1
        txs = []
        for blkidx in range(nStakeEnabledHeight + 2 , nStakeValidationHeight):
            # purchase tickets until an empirical found blockindex
            # we cannot purchase after that because the ticketPrice increases too much and we do not 
            # support yet multiple funding inputs to our ticket purchasing transactions
            # TODO try to go further when multiple inputs are supported
            if blkidx < nPurchaseMaxHeight:
                funds = self.nodes[0].getbalance()
                print("funds:", funds)
                ticketPrice = self.nodes[0].getstakedifficulty()
                print("ticketPrice", ticketPrice)
                print("purchase", nMaxFreshStakePerBlock, "at", blkidx)
                txs.append(self.nodes[0].purchaseticket("", 1.5, 1, txaddress, rewardaddress,nMaxFreshStakePerBlock))
                assert(len(txs[-1])==nMaxFreshStakePerBlock)

                x = self.nodes[0].ticketfeeinfo() # we should see the fee info for mempool before calling generate
                assert(x['feeinfomempool']['number'] == nMaxFreshStakePerBlock)
                assert(len(x['feeinfoblocks']) == 0)
                assert(len(x['feeinfowindows']) == 0)
            
            self.nodes[0].generate(1)
            self.sync_all()
            chainInfo = self.nodes[0].getblockchaininfo()
            assert(chainInfo['blocks'] == blkidx)

        assert(blkidx == nStakeValidationHeight - 1)

        # starting with nStakeValidationHeight we need to add vote txs into blocks, otherwise TestBlockValidity fails: too-few-votes
        assert_raises_rpc_error(-1, None, self.nodes[0].generate,1)
        dummyVoteBits    =  1
        dummyVoteBitsExt = "00"

        nHeightExpiredBecomeMissed = nTicketExpiry + nStakeEnabledHeight
        assert(nHeightExpiredBecomeMissed > nStakeValidationHeight)

        stakeinfo = self.nodes[0].getstakeinfo()
        # print(stakeinfo)
        assert(stakeinfo['live'] == stakeinfo['unspent']) # tickets bought first are live
        assert(stakeinfo['live'] == stakeinfo['poolsize'])
        allPurchasedTickets = 20 * ((nPurchaseMaxHeight - 1) - (nStakeEnabledHeight-nTicketMaturity))
        assert(stakeinfo['live'] == allPurchasedTickets)

        totalVotes = 0
        for blkidx in range(nStakeValidationHeight, nHeightExpiredBecomeMissed + 10):
            # winners vote
            winners = self.nodes[0].winningtickets()
            assert(len(winners) == 1)
            assert(len(winners[0]['tickets'])==nTicketsPerBlock)
            blockhash = chainInfo['bestblockhash']
            blockheight = chainInfo['blocks']
            for tickethash in winners[0]['tickets']:
                votehash = self.nodes[0].generatevote(blockhash, blockheight, tickethash, dummyVoteBits, dummyVoteBitsExt)
                totalVotes += 1
            
            missed = self.nodes[0].missedtickets()
            assert(blkidx <= nHeightExpiredBecomeMissed or len(missed["tickets"]) > 0) # we expect missed tickets 
            for tickethash in missed['tickets']:
                revokehash = self.nodes[0].revoketicket(tickethash)

            stakeinfo = self.nodes[0].getstakeinfo()
            assert(stakeinfo['missed'] == len(missed['tickets']))

            print(blkidx)
            self.nodes[0].generate(1)
            self.sync_all()
            chainInfo = self.nodes[0].getblockchaininfo()
            assert(chainInfo['blocks'] == blkidx)

            walletinfo = self.nodes[0].getwalletinfo()
            stakeinfo = self.nodes[0].getstakeinfo()
            print(stakeinfo)
            print(walletinfo)
            allPurchasedTickets -= len(winners[0]['tickets'])
            # TODO seems the wallet finds only one ticket to be spent by a vote, retry this after integrating the auto-voter changes
            assert(stakeinfo['voted'] == totalVotes)
            assert(stakeinfo['unspent'] + stakeinfo['expired'] == allPurchasedTickets)
            assert(stakeinfo['live'] == stakeinfo['poolsize'])
            assert(stakeinfo['live'] + stakeinfo['missed'] + stakeinfo['expired'] == allPurchasedTickets)

            live = self.nodes[0].livetickets()
            print("len live",stakeinfo['live'])
            assert(stakeinfo['live'] == len(live['tickets']))

        self.test_tx_type_info_last_block(['coinbase', 'vote', 'stake_revocation'])
        
        # getstakeversioninfo
        numintervals = 2
        stakeversioninfo = self.nodes[0].getstakeversioninfo(numintervals)
        assert(stakeversioninfo['currentheight'] == blkidx)
        assert(stakeversioninfo['hash'] == chainInfo['bestblockhash'])
        assert(len(stakeversioninfo['intervals']) == numintervals)
        adjust = 1 # as in the RPC call implementation
        for interval in stakeversioninfo['intervals']:
            difference = interval['startheight'] - interval['endheight'] + adjust
            posver_total = 0
            for posver in interval['posversions']:
                posver_total += posver['count']
            assert(difference == posver_total)
            if interval['startheight'] > nStakeValidationHeight:
                votever_total = 0
                for votever in interval['voteversions']:
                    votever_total += votever['count']
                assert(votever_total == posver_total*nTicketsPerBlock)
            else:
                assert(len(interval['voteversions']) == 0)
            adjust = 0

        # getstakeversions
        numblocks = 3
        stakeversions = self.nodes[0].getstakeversions(chainInfo['bestblockhash'], numblocks)
        assert(len(stakeversions) == numblocks)
        assert(stakeversions[0]['hash'] == chainInfo['bestblockhash'])
        expectedHeight = blkidx
        for stakeversion in stakeversions:
            assert(stakeversion['height'] == expectedHeight)
            expectedHeight -= 1
            assert(len(stakeversion['votes']) == nTicketsPerBlock)
            for vote in stakeversion['votes']:
                assert(vote['version'] == stakeversion['stakeversion'])
                assert(vote['bits'] == dummyVoteBits)

if __name__ == "__main__":
    TicketOperations().main()
