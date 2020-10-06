#!/usr/bin/env python3
# Copyright (c) 2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test the geblocktemplate RPC commands
    getblocktemplate must return the block previous to the tip until the next block has 3+ votes on the tip
    https://docs.decred.org/faq/proof-of-stake/general/#10-what-happens-if-less-than-3-of-the-selected-tickets-vote-on-a-block
"""

from test_framework.blocktools import create_coinbase, create_block, add_witness_commitment
from test_framework.mininode import CBlock, CTransaction, ToHex
from test_framework.test_framework import PAIcoinTestFramework
from test_framework.util import *
import io

class TestGetBlockTemplate(PAIcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2 # we need at least 2 nodes to have non zerp connections for getblocktemplate to work
        self.extra_args = [['-autobuy','-tblimit=1','-blockenoughvoteswaittime=0','-blockallvoteswaittime=0'],[]] 

    def get_best_block(self, checkHeight):
        chainInfo = self.nodes[0].getbestblock()
        print("-------------")
        print("besthash",chainInfo['hash'])
        print("bestheight",chainInfo['height'])
        assert(chainInfo['height'] == checkHeight)
        lastblock = self.nodes[0].getblock(chainInfo['hash'],2)
        txids = []
        for tx in lastblock['tx']:
            print(tx['txid'],tx['type'])
            txids.append(tx['txid'])
        return txids

    def get_block_template(self):
        tmpl = self.nodes[0].getblocktemplate()
        print("-------------")
        print("gbt prev bhash\t",tmpl['previousblockhash'])
        print("gbt height\t",tmpl['height'])
        rawtxs = [x['data'] for x in tmpl['transactions']]
        decodedrawtxs = [self.nodes[0].decoderawtransaction(x) for x in rawtxs]
        txids = []
        for decodedtx in decodedrawtxs:
            print(decodedtx['txid'],decodedtx['type'])
            txids.append(decodedtx['txid'])
        return txids
    
    def get_raw_mempool(self):
        print("-------------")
        print("mempool")
        txids = []
        for k,v in self.nodes[0].getrawmempool(True).items():
            print(k,v['type'])
            txids.append(k)
        return txids

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
        self.sync_all()
        self.get_best_block(nStakeValidationHeight - 4)

        for idx in range(nStakeValidationHeight-3, nStakeValidationHeight):
            gbt_txids = self.get_block_template()
            # assert 'proposal' in tmpl['capabilities']
            # assert 'coinbasetxn' not in tmpl
            # coinbase_tx = create_coinbase(height=int(tmpl["height"]))
            # block = create_block(int("0x"+tmpl["previousblockhash"],0), coinbase_tx, int(tmpl["curtime"]))
            # # sequence numbers must not be max for nLockTime to have effect
            # # coinbase_tx.vin[0].nSequence = 2 ** 32 - 2
            # # coinbase_tx.rehash()
            # # block = CBlock()
            # block.nVersion = int(tmpl["version"])
            # # block.hashPrevBlock = int(tmpl["previousblockhash"], 16)
            # # block.nTime = tmpl["curtime"]
            # block.nBits = int(tmpl["bits"], 16)
            # # block.nNonce = 0
            # # block.vtx = [coinbase_tx]# + tmpl["transactions"]
            # # for tx in tmpl["transactions"]:
            # #     ctx = CTransaction()
            # #     ctx.deserialize(io.BytesIO(hex_str_to_bytes(tx['data'])))
            # #     ctx.rehash()
            # #     block.vtx.append(ctx)
            # # block.hashMerkleRoot = block.calc_merkle_root()
            # add_witness_commitment(block)
            # block.rehash()
            # block.solve()
            # # self.nodes[0].submitblock(ToHex(block))
            # self.nodes[0].submitblock(bytes_to_hex_str(block.serialize(True)))
            self.nodes[0].generate(1)
            self.sync_all()
            block_txids = self.get_best_block(idx)
            assert(all(tx in block_txids for tx in gbt_txids))

        assert(idx == nStakeValidationHeight - 1)
        # starting with nStakeValidationHeight we need to add vote txs into blocks, otherwise TestBlockValidity fails: too-few-votes
        assert_raises_rpc_error(-1, None, self.nodes[0].generate,1)

        dummyVoteBits    =  1
        dummyVoteBitsExt = "00"
        majority = int(nTicketsPerBlock / 2 + 1)

        for _ in range(3):
            winners = self.nodes[0].winningtickets()
            assert(len(winners['tickets'])==nTicketsPerBlock)
            chainInfo = self.nodes[0].getbestblock()
            blockhash = chainInfo['hash']
            blockheight = chainInfo['height']
            for winneridx in range(0, majority):
                votehash = self.nodes[0].generatevote(blockhash, blockheight, winners['tickets'][winneridx], dummyVoteBits, dummyVoteBitsExt)

            print("after adding votes")
            gbt_txids = self.get_block_template()

            # we have votes so we can progress the chain
            idx = idx+1
            self.nodes[0].generate(1)
            self.sync_all()
            block_txids = self.get_best_block(idx)
            assert(all(tx in block_txids for tx in gbt_txids))

            mempooltx_before_invalid = self.get_raw_mempool()
            winners_before_invalid = self.nodes[0].winningtickets()
            gbt_txids_invalid = self.get_block_template()
            # getblocktemplate invalidates the tip because there are no votes that vote for it, so the previous winners are used again
            latest_winners = self.nodes[0].winningtickets()
            assert(latest_winners != winners_before_invalid)
            assert(latest_winners == winners)

            assert(set(gbt_txids + mempooltx_before_invalid) == set(gbt_txids_invalid))

            mempooltx_after_invalid = self.get_raw_mempool()
            assert(set(mempooltx_after_invalid) == set(gbt_txids_invalid))
            # we have no votes for the invalid tip but we can progress from previous tip, 
            self.nodes[0].generate(1)
            # self.sync_all() unable to sync after node 0 invalidates its tip
            block_txids = self.get_best_block(idx)
            assert(all(tx in block_txids for tx in gbt_txids_invalid))

if __name__ == "__main__":
    TestGetBlockTemplate().main()
