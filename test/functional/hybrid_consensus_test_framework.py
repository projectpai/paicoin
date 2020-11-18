#!/usr/bin/env python3
# Copyright (c) 2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test that the PAIcoinTestFramwork was updated to use the extended BlockHeader and 
   the SHAKE256 hashing algorithm above Hybrid Consensus Fork
"""

from test_framework.test_framework import PAIcoinTestFramework
from test_framework.util import *
from test_framework.blocktools import *
from io import BytesIO
# from test_framework.mininode import CBlock, CTransaction, ToHex

class TestHybridConsensusTestFramework(PAIcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2 # we need at least 2 nodes to have non zero connections for getblocktemplate to work
        self.extra_args = [['-autostake','-tblimit=1','-blockallvoteswaittime=0'],[]] 

    def run_test(self):
        # these are the same values as in chainparams.cpp for REGTEST, update them if they change
        nHybridConsensusHeight = 1500
        nStakeEnabledHeight    = 2000
        nStakeValidationHeight = 2100
        nTicketMaturity        = 8

        self.nodes[0].generate(nHybridConsensusHeight - 10)
        self.sync_all()
        chainInfo = self.nodes[0].getbestblock()
        assert(chainInfo['height'] == nHybridConsensusHeight - 10)

        # height = self.nodes[0].getblockcount()
        for height in range(nHybridConsensusHeight - 9, nStakeValidationHeight + 10):
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
            if height > nHybridConsensusHeight:
                block.nStakeDifficulty =    int(tmpl["stakedifficulty"], 16)
                block.nVoteBits =           tmpl["votebits"]
                block.nTicketPoolSize =     tmpl["ticketpoolsize"]
                block.ticketLotteryState =  tmpl["ticketlotterystate"]
                block.nVoters =             tmpl["voters"]
                block.nFreshStake =         tmpl["freshstake"]
                block.nRevocations =        tmpl["revocations"]
                block.extraData =           tmpl["extradata"]
                block.nStakeVersion =       tmpl["stakeversion"]

            if height > nStakeValidationHeight: # voting height
                assert(len(tmpl["transactions"]) >= 5)  # at least the votes
            if height == nStakeEnabledHeight - nTicketMaturity + 1: # ticket buy height
                assert(len(tmpl["transactions"]) >= 2)  # at least a ticket purchase and the funding tx

            block.vtx = [coinbase_tx] #+ tmpl["transactions"]
            for tx in tmpl["transactions"]:
                ctx = CTransaction()
                ctx.deserialize(BytesIO(hex_str_to_bytes(tx['data'])))
                ctx.rehash()
                block.vtx.append(ctx)
            block.hashMerkleRoot = block.calc_merkle_root()
            add_witness_commitment(block)
            # block.rehash()
            block.solve()
            # print('solved hash', block.hash)
            print("submit for height", height)
            submit_result = self.nodes[0].submitblock(ToHex(block))
            assert(submit_result == None)
            chainInfo = self.nodes[0].getbestblock()
            # print("best hash", chainInfo['hash'])
            assert(chainInfo['height'] == height)
            assert(chainInfo['hash'] == block.hash)
        
        self.sync_all()

if __name__ == "__main__":
    TestHybridConsensusTestFramework().main()
