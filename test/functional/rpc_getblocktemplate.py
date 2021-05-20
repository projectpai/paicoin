#!/usr/bin/env python3
# Copyright (c) 2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test the geblocktemplate RPC commands
    getblocktemplate must return the block previous to the tip until the next block has 3+ votes on the tip
    https://docs.decred.org/faq/proof-of-stake/general/#10-what-happens-if-less-than-3-of-the-selected-tickets-vote-on-a-block
"""

from test_framework.test_framework import PAIcoinTestFramework
from test_framework.blocktools import create_coinbase, create_block, add_witness_commitment
from test_framework.mininode import CBlock, CTransaction, ToHex
from test_framework.util import *
from io import BytesIO

class TestGetBlockTemplate(PAIcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2 # we need at least 2 nodes to have non zerp connections for getblocktemplate to work
        self.extra_args = [['-autobuy','-tblimit=1','-autovote=0','-autorevoke=1','-blockallvoteswaittime=0'],[]] 

    def get_best_block(self, checkHeight = None):
        chainInfo = self.nodes[0].getbestblock()
        print("-------------")
        print("besthash",chainInfo['hash'])
        print("bestheight",chainInfo['height'])
        if checkHeight != None:
            print("expected height ", checkHeight)
        assert(checkHeight == None or chainInfo['height'] == checkHeight)
        lastblock = self.nodes[0].getblock(chainInfo['hash'],2)
        txids = []
        for tx in lastblock['tx']:
            print(tx['txid'],tx['type'])
            txids.append(tx['txid'])
        return txids

    def get_block_template(self, nodeidx=0):
        tmpl = self.nodes[nodeidx].getblocktemplate()
        print("-------------")
        print("gbt prev bhash\t",tmpl['previousblockhash'])
        print("gbt height\t",tmpl['height'])
        rawtxs = [x['data'] for x in tmpl['transactions']]
        decodedrawtxs = [self.nodes[nodeidx].decoderawtransaction(x) for x in rawtxs]
        txids = []
        num_votes = 0
        for decodedtx in decodedrawtxs:
            print(decodedtx['txid'],decodedtx['type'])
            if decodedtx['type'] == 'vote':
                num_votes +=1
            txids.append(decodedtx['txid'])
        return tmpl['height'], num_votes, txids
    
    def get_raw_mempool(self, nodeidx = 0):
        print("-------------")
        print("mempool of node:",nodeidx)
        txids = []
        tickets_spent = []
        for k,v in self.nodes[nodeidx].getrawmempool(True).items():
            if v['type'] == 'vote':
                print(k,v['type'])
                print(v['voting']['blockhash'],v['voting']['blockheight'],v['voting']['ticket'])
                tickets_spent.append(v['voting']['ticket'])
            else:
                print(k,v['type'])

            txids.append(k)
        return tickets_spent, txids

    def mine_using_template(self):
        tmpl = self.nodes[0].getblocktemplate()
        assert 'proposal' in tmpl['capabilities']
        assert 'coinbasetxn' not in tmpl
        coinbase_tx = create_coinbase(height=int(tmpl["height"]))
        block = CBlock()
        block.nVersion = int(tmpl["version"])
        block.hashPrevBlock = int(tmpl["previousblockhash"], 16)
        block.nTime = tmpl["curtime"]
        block.nBits = int(tmpl["bits"], 16)
        block.nNonce = 0

        # extended block
        block.nStakeDifficulty =    int(tmpl["stakedifficulty"], 16)
        block.nVoteBits =           tmpl["votebits"]
        block.nTicketPoolSize =     tmpl["ticketpoolsize"]
        block.ticketLotteryState =  tmpl["ticketlotterystate"]
        block.nVoters =             tmpl["voters"]
        block.nFreshStake =         tmpl["freshstake"]
        block.nRevocations =        tmpl["revocations"]
        block.extraData =           tmpl["extradata"]
        block.nStakeVersion =       tmpl["stakeversion"]

        block.vtx = [coinbase_tx]
        for tx in tmpl["transactions"]:
            ctx = CTransaction()
            ctx.deserialize(BytesIO(hex_str_to_bytes(tx['data'])))
            ctx.rehash()
            block.vtx.append(ctx)
        block.hashMerkleRoot = block.calc_merkle_root()
        add_witness_commitment(block)
        block.solve()
        print('solved hash', block.hash)
        # print("submit for height", idx)
        submit_result = self.nodes[0].submitblock(ToHex(block))
        print(submit_result)
        assert(submit_result in [None,"inconclusive"])

    def run_test(self):
        # these are the same values as in chainparams.cpp for REGTEST, update them if they change
        nStakeEnabledHeight    = 2000
        nStakeValidationHeight = 2100
        nTicketMaturity        = 8
        nMaxFreshStakePerBlock = 20
        nTicketsPerBlock       = 5
        nTicketPoolSize        = 64
        nTicketExpiry          = 5 * nTicketPoolSize
        nStakeDiffWindowSize   = 8

        self.nodes[0].generate(nStakeValidationHeight - 4)
        # self.sync_all()
        self.get_best_block(nStakeValidationHeight - 4)

        for idx in range(nStakeValidationHeight-3, nStakeValidationHeight):
            _, _, gbt_txids = self.get_block_template()
            self.mine_using_template()
            # self.nodes[0].generate(1)
            self.sync_all()
            block_txids = self.get_best_block(idx)
            assert(all(tx in block_txids for tx in gbt_txids))

        assert(idx == nStakeValidationHeight - 1)
        # starting with nStakeValidationHeight we need to add vote txs into blocks, otherwise TestBlockValidity fails: too-few-votes
        assert_raises_rpc_error(-1, None, self.nodes[0].generate,1)

        dummyVoteBits    =  1
        dummyVoteBitsExt = "00"
        majority = int(nTicketsPerBlock / 2 + 1)

        # self.nodes[0].stopticketbuyer()

        for _ in range(3):
            # self.get_raw_mempool()
            # self.get_raw_mempool(1)
            chainInfo = self.nodes[0].getbestblock()
            winners = self.nodes[0].winningtickets()
            print(winners)
            winidx = 0
            if len(winners)>1:
                # find the winning tickets that vote on the side chain tip
                for winidx in range(len(winners)):
                    if chainInfo['hash'] != winners[winidx]['blockhash']:
                        break

            winning_tickets = winners[winidx]['tickets']
            assert(len(winning_tickets)==nTicketsPerBlock)
            blockhash = winners[winidx]['blockhash']#chainInfo['hash']
            if chainInfo['hash'] != blockhash:
                print("<<<Voting on side chain>>>")
            blockheight = chainInfo['height']
            print("voting on ", blockhash, " at height ", blockheight)
            for ticketidx in range(0, majority):
                votehash = self.nodes[0].generatevote(blockhash, blockheight, winning_tickets[ticketidx], dummyVoteBits, dummyVoteBitsExt)
                print("generated vote", votehash, "using ticket", winning_tickets[ticketidx])

            # self.sync_all()
            self.get_raw_mempool()
            self.get_raw_mempool(1)

            print("after adding votes")
            _,_,gbt_txids = self.get_block_template()

            # we have votes so we can progress the chain
            self.mine_using_template()
            # self.nodes[0].generate(1)
            # self.get_raw_mempool()
            # self.get_raw_mempool(1)
            # self.sync_all()
            idx = idx+1
            block_txids = self.get_best_block(idx)
            assert(all(tx in block_txids for tx in gbt_txids))

            # mempooltx_before_invalid = self.get_raw_mempool()
            # winners_before_invalid = self.nodes[0].winningtickets()
            # gbt_txids_invalid = self.get_block_template()

            # getblocktemplate invalidates the tip because there are no votes that vote for it, so the previous winners are used again
            # latest_winners = self.nodes[0].winningtickets()
            # print(latest_winners)
            # assert(latest_winners != winners_before_invalid)
            # assert(latest_winners == winners)

            # assert(set(gbt_txids + mempooltx_before_invalid) == set(gbt_txids_invalid))

            # mempooltx_after_invalid = self.get_raw_mempool()
            # assert(set(mempooltx_after_invalid) == set(gbt_txids_invalid))

            # we have no votes for the current tip but we can progress from previous tip,
            print("progress from previous tip")
            _,_,gbt_txids_previous = self.get_block_template()

            print("-------------")
            # print("sleep 5s before mining")
            # time.sleep(5)
            chaintips = self.nodes[0].getchaintips()
            print("chaintip on node:",0)
            print(chaintips)
            chaintips = self.nodes[1].getchaintips()
            print("chaintip on node:",1)
            print(chaintips)

            self.mine_using_template()

            print("-------------")
            # print("sleep 5s after mining")
            # time.sleep(5)
            chaintips = self.nodes[0].getchaintips()
            print("chaintip on node:",0)
            print(chaintips)
            chaintips = self.nodes[1].getchaintips()
            print("chaintip on node:",1)
            print(chaintips)

            # self.nodes[0].generate(1)
            # self.sync_all() #unable to sync after node 0 invalidates its tip
            block_txids = self.get_best_block(idx)
            # bestblock = self.nodes[1].getbestblock()
            # print("chaintip on node:",1)
            # print(bestblock)
            # assert(all(tx in block_txids for tx in gbt_txids_invalid))

if __name__ == "__main__":
    TestGetBlockTemplate().main()
