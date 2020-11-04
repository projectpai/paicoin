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
        self.extra_args = [['-autobuy', '-tblimit=1', '-autovote=1', '-autorevoke=0', '-blockenoughvoteswaittime=0', '-blockallvoteswaittime=0'],
                           ['-autobuy', '-tblimit=1', '-autovote=1', '-autorevoke=0', '-blockenoughvoteswaittime=0', '-blockallvoteswaittime=0']]

    def get_best_block(self, checkHeight = None):
        chainInfo = self.nodes[0].getbestblock()
        print("-------------")
        print("besthash",chainInfo['hash'])
        print("bestheight",chainInfo['height'])
        assert(checkHeight == None or chainInfo['height'] == checkHeight)
        lastblock = self.nodes[0].getblock(chainInfo['hash'],2)
        txids = []
        for tx in lastblock['tx']:
            print(tx['txid'],tx['type'])
            txids.append(tx['txid'])
        return txids

    def get_block_template(self, nodeidx = 0):
        tmpl = self.nodes[nodeidx].getblocktemplate()
        print("-------------")
        print("gbt prev bhash\t",tmpl['previousblockhash'])
        print("gbt height\t",tmpl['height'])
        rawtxs = [x['data'] for x in tmpl['transactions']]
        decodedrawtxs = [self.nodes[nodeidx].decoderawtransaction(x) for x in rawtxs]
        txids = []
        for decodedtx in decodedrawtxs:
            print(decodedtx['txid'],decodedtx['type'])
            txids.append(decodedtx['txid'])
        return txids
    
    def get_raw_mempool(self, nodeidx = 0):
        print("-------------")
        print("mempool of node:",nodeidx)
        txids = []
        for k,v in self.nodes[nodeidx].getrawmempool(True).items():
            print(k,v['type'])
            txids.append(k)
        return txids

    def mine_using_template(self, nodeidx = 0):
        tmpl = self.nodes[nodeidx].getblocktemplate()
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
        print("-------------")
        print("mine using template on node:", nodeidx)
        print('solved hash', block.hash)
        # print("submit for height", idx)
        submit_result = self.nodes[nodeidx].submitblock(ToHex(block))
        print(submit_result)
        assert(submit_result in [None,"inconclusive","duplicate-inconclusive"])

    def run_test(self):
        # these are the same values as in chainparams.cpp for REGTEST, update them if they change
        StakeEnabledHeight    = 2000
        StakeValidationHeight = 2100
        TicketMaturity        = 8
        MaxFreshStakePerBlock = 20
        TicketsPerBlock       = 5
        TicketPoolSize        = 64
        TicketExpiry          = 5 * TicketPoolSize
        StakeDiffWindowSize   = 8
        voteBits              = 1

        startAutoBuyerHeight = StakeEnabledHeight - TicketMaturity
        # self.nodes[0].generate(int(startAutoBuyerHeight/2))
        # self.sync_all()
        # self.nodes[1].generate(int(startAutoBuyerHeight/2))
        # self.sync_all()
        # self.get_best_block(startAutoBuyerHeight)

        for idx in range(1, startAutoBuyerHeight+1):
            self.nodes[idx % self.num_nodes].generate(1) # mine on each of the nodes, so also the seconds one has funds
            self.sync_all()
            print(idx)
            # self.get_best_block(blkidx)

        self.get_best_block(startAutoBuyerHeight)

        for idx in range(startAutoBuyerHeight+1, StakeValidationHeight + 1):
            gbt_txids = self.get_block_template(idx % self.num_nodes)
            self.mine_using_template(idx % self.num_nodes)# mine on each of the nodes, so also the seconds one has funds
            self.sync_all()
            block_txids = self.get_best_block(idx)
            assert(all(tx in block_txids for tx in gbt_txids))

        self.nodes[0].stopautovoter()
        self.nodes[1].stopautovoter()

        for _ in range(3):
            # self.get_raw_mempool(0)
            # self.get_raw_mempool(1)
            # gbt_txids = self.get_block_template(0)
            self.mine_using_template(0)
            # idx = idx+1
            block_txids = self.get_best_block()
            chaintips = self.nodes[0].getchaintips()
            print("chaintip on node:",0)
            print(chaintips)
            chaintips = self.nodes[1].getchaintips()
            print("chaintip on node:",1)
            print(chaintips)

            latest_winners = self.nodes[0].winningtickets()
            print("latest_winners on node:",0)
            print(latest_winners)
            latest_winners = self.nodes[1].winningtickets()
            print("latest_winners on node:",1)
            print(latest_winners)

        self.nodes[0].startautovoter(voteBits)
        self.nodes[1].startautovoter(voteBits)

        for _ in range(3):
            # self.get_raw_mempool(0)
            # self.get_raw_mempool(1)
            # gbt_txids = self.get_block_template(0)
            self.mine_using_template(0)
            # idx = idx+1
            block_txids = self.get_best_block()
            chaintips = self.nodes[0].getchaintips()
            print("chaintip on node:",0)
            print(chaintips)
            chaintips = self.nodes[1].getchaintips()
            print("chaintip on node:",1)
            print(chaintips)

            latest_winners = self.nodes[0].winningtickets()
            print("latest_winners on node:",0)
            print(latest_winners)
            latest_winners = self.nodes[1].winningtickets()
            print("latest_winners on node:",1)
            print(latest_winners)


if __name__ == "__main__":
    TestGetBlockTemplate().main()
