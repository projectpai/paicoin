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
        self.extra_args = [['-autobuy', '-tblimit=3', '-autovote=1', '-autorevoke=0', '-blockallvoteswaittime=0']
                           ,['-autobuy', '-tblimit=3', '-autovote=1', '-autorevoke=0', '-blockallvoteswaittime=0']]

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

    def get_block_template(self, nodeidx = 0):
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
        assert(submit_result in [None,"inconclusive"])
        return block.hash

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

        for idx in range(1, startAutoBuyerHeight+1):
            self.nodes[idx % self.num_nodes].generate(1) # mine on each of the nodes, so also the seconds one has funds
            self.sync_all()
            print(idx)
            # self.get_best_block(blkidx)

        self.get_best_block(startAutoBuyerHeight)

        for idx in range(startAutoBuyerHeight+1, StakeValidationHeight + 1):
            _, _, gbt_txids = self.get_block_template(idx % self.num_nodes)
            last_mined = self.mine_using_template(idx % self.num_nodes)# mine on each of the nodes, so also the seconds one has funds
            self.sync_all()
            block_txids = self.get_best_block(idx)
            assert(all(tx in block_txids for tx in gbt_txids))

        # from now on only the second node is allowed to vote and the first node is the one we mine on
        self.nodes[0].stopautovoter()
        # self.nodes[1].stopautovoter()

        for i in range(100):
            print("mempool before get block template:")
            self.get_raw_mempool(0)
            idx += 1 # expected to increase
            gbt_height, num_votes, gbt_txids = self.get_block_template(0)
            if gbt_height < idx:
                print("forked")
                idx -= 1 # in this case the best block stays the same

            last_mined = self.mine_using_template(0)
            self.sync_all()

            block_txids = self.get_best_block(idx)

            # check chain tips
            chaintips0 = self.nodes[0].getchaintips()
            chaintips1 = self.nodes[1].getchaintips()
            tip_hashes0 = [tips['hash'] for tips in chaintips0]
            tip_hashes1 = [tips['hash'] for tips in chaintips1]
            assert(set(tip_hashes0) == set(tip_hashes1))
            assert (last_mined in tip_hashes0)

            # check all tips have same winner tickets
            winningtickets0 = self.nodes[0].winningtickets()
            winningtickets1 = self.nodes[1].winningtickets()
            block_winner0 = {winner['blockhash']: winner['tickets'] for winner in winningtickets0}
            block_winner1 = {winner['blockhash']: winner['tickets'] for winner in winningtickets1}
            shared_items = {k: block_winner0[k] for k in block_winner0 if k in block_winner1 and set(block_winner0[k]) == set(block_winner1[k])}
            assert(len(shared_items) == len(block_winner0))
            assert(last_mined in block_winner0)

            winners_for_last_mined = block_winner0[last_mined]

            tickets_spent0, all_txids0 = self.get_raw_mempool(0)
            tickets_spent1, all_txids1 = self.get_raw_mempool(1)
            
            tickets_owned_by_voting_node = self.nodes[1].gettickets(False)
            voting_tickets_owned_by_voting_node = [winner in tickets_owned_by_voting_node['hashes'] for winner in winners_for_last_mined]

            voted_tickets = [ticket_spent in winners_for_last_mined for ticket_spent in tickets_spent1]
            assert(sum(voting_tickets_owned_by_voting_node) >= sum(voted_tickets)) # all voting tickets are owned by the voting node

if __name__ == "__main__":
    TestGetBlockTemplate().main()
