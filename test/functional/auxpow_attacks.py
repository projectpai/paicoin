#!/usr/bin/env python3
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test AuxPow attacks

- double proof attack
- self merged mining attack
- merged mining auxpow depth check
- invalid merkle branch"""

import copy
import binascii
import time
from binascii import b2a_hex
from decimal import Decimal

from test_framework.blocktools import create_coinbase
from test_framework.mininode import CBlock, CBlockHeader, CAuxPow, CMerkleTx, hash256, ToHex
from test_framework.test_framework import PAIcoinTestFramework

class AuxPowAttackTest(PAIcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 4
        self.enable_mocktime()
        self.mergedMiningDepthCheck = 20

    def enable_mocktime (self):
        self.mocktime = 1529934120 # Monday, June 25, 2018 1:42:00 PM GMT

    def run_test(self):
        # simulate 2 chains/networks for parent/child chain
        self.split_network()
        #self.test_doubleProofAttack()
        #self.test_selfMergeAttack_Parent_Merged()
        #self.test_selfMergeAttack_Merged_Parent()
        #self.test_selfMergeAttack_Parent_Merged_WithDepth()
        self.test_invalidMerkleBranch()

    def test_doubleProofAttack(self):

        node_child_chain = self.nodes[0]    # child chain network
        node_parent_chain = self.nodes[2]   # parent chain network

        # Mine a block to leave initial block download
        node_child_chain.generate(1)
        child_blk_tmpl = node_child_chain.getblocktemplate()

        # create first child block of auxpow attack
        coinbase_tx = self.createCoinbaseWithSequence(height=int(child_blk_tmpl["height"]) + 1)
        child_block1 = self.createBlockFromTemplate(block_template=child_blk_tmpl, coinbase_tx=coinbase_tx)

        # create second child block of auxpow attack
        coinbase_tx = self.createCoinbaseWithSequence(height=int(child_blk_tmpl["height"]) + 2)
        child_block2 = self.createBlockFromTemplate(block_template=child_blk_tmpl, coinbase_tx=coinbase_tx, hash_prevblock=child_block1.sha256)

        hash_block1 = bytearray(binascii.unhexlify(child_block1.hash))
        hash_block1.reverse()
        hash_block2 = bytearray(binascii.unhexlify(child_block2.hash))
        hash_block2.reverse()

        combined_hash = hash256(hash_block1 + hash_block2)
        combined_hash = bytearray(combined_hash)
        combined_hash.reverse()

        # create parent block of auxpow attack
        node_parent_chain.generate(1)
        parent_blk_tmpl = node_parent_chain.getblocktemplate()
        coinbase_tx = self.createCoinbaseWithSequence(height=int(parent_blk_tmpl["height"]) + 1)
        coinbase_tx.vin[0].scriptSig += b"fabe" + binascii.hexlify (b"m" * 2)
        coinbase_tx.vin[0].scriptSig += combined_hash
        coinbase_tx.vin[0].scriptSig += b"01000000" + (b"00" * 4)
        coinbase_tx.rehash()

        parent_block = self.createBlockFromTemplate(block_template=parent_blk_tmpl, coinbase_tx=coinbase_tx)
        parent_block.solve()

        result = node_parent_chain.submitblock(ToHex(parent_block))
        assert result is None

        # Now, build valid auxpow information to submit to child chain
        merkleTx = CMerkleTx.fromData(copy.deepcopy(coinbase_tx), parent_block.sha256, [], 0)

        child_block1.auxpow = CAuxPow.fromData(merkleTx, [child_block2.sha256], 0, parent_block)
        result = node_child_chain.submitblock(ToHex(child_block1))
        assert result is None

        child_block2.auxpow = CAuxPow.fromData(copy.deepcopy(merkleTx), [child_block1.sha256], 1, parent_block)
        result = node_child_chain.submitblock(ToHex(child_block2))
        assert 'auxpow-double-proof' in result

        # Sync blocks within their own chains
        self.sync_all([self.nodes[:2], self.nodes[2:]])

    def test_selfMergeAttack_Parent_Merged(self):

        node_chain = self.nodes[0]

        # Mine a block to leave initial block download
        node_chain.generate(1)
        block_template = node_chain.getblocktemplate()

        # create merged mined block of auxpow attack
        coinbase_tx = self.createCoinbaseWithSequence(height=int(block_template["height"]) + 1)
        merged_mined_block = self.createBlockFromTemplate(block_template=block_template, coinbase_tx=coinbase_tx)
        hash_merged_mined_block = bytearray(binascii.unhexlify(merged_mined_block.hash))

        # create parent block of auxpow attack, include in coinbase the merged mined block hash
        coinbase_tx = self.createCoinbaseWithSequence(height=int(block_template["height"]) + 1)
        coinbase_tx.vin[0].scriptSig += b"fabe" + binascii.hexlify (b"m" * 2)
        coinbase_tx.vin[0].scriptSig += hash_merged_mined_block
        coinbase_tx.vin[0].scriptSig += b"01000000" + (b"00" * 4)
        coinbase_tx.rehash()

        parent_block = self.createBlockFromTemplate(block_template=block_template, coinbase_tx=coinbase_tx)
        parent_block.solve()

        result = node_chain.submitblock(ToHex(parent_block))
        assert result is None

        # Now, build valid auxpow information to try to submit to the chain
        merkleTx = CMerkleTx.fromData(copy.deepcopy(coinbase_tx), parent_block.sha256, [], 0)
        merged_mined_block.auxpow = CAuxPow.fromData(merkleTx, [], 0, parent_block)

        result = node_chain.submitblock(ToHex(merged_mined_block))
        assert 'auxpow-self' in result

        # Sync blocks within their own chains
        self.sync_all([self.nodes[:2], self.nodes[2:]])

    def test_selfMergeAttack_Parent_Merged_WithDepth(self):

        node_chain = self.nodes[0]

        # Mine a block to leave initial block download
        node_chain.generate(1)
        block_template = node_chain.getblocktemplate()

        # create merged mined block of auxpow attack
        coinbase_tx = self.createCoinbaseWithSequence(height=int(block_template["height"]) + 1)
        merged_mined_block = self.createBlockFromTemplate(block_template=block_template, coinbase_tx=coinbase_tx)
        hash_merged_mined_block = bytearray(binascii.unhexlify(merged_mined_block.hash))

        # create parent block of auxpow attack, include in coinbase the merged mined block hash
        coinbase_tx = self.createCoinbaseWithSequence(height=int(block_template["height"]) + 1)
        coinbase_tx.vin[0].scriptSig += b"fabe" + binascii.hexlify (b"m" * 2)
        coinbase_tx.vin[0].scriptSig += hash_merged_mined_block
        coinbase_tx.vin[0].scriptSig += b"01000000" + (b"00" * 4)
        coinbase_tx.rehash()

        parent_block = self.createBlockFromTemplate(block_template=block_template, coinbase_tx=coinbase_tx)
        parent_block.solve()

        result = node_chain.submitblock(ToHex(parent_block))
        assert result is None

        # Now, build valid auxpow information to try to submit to the chain
        merkleTx = CMerkleTx.fromData(copy.deepcopy(coinbase_tx), parent_block.sha256, [], 0)
        merged_mined_block.auxpow = CAuxPow.fromData(merkleTx, [], 0, parent_block)

        node_chain.generate(self.mergedMiningDepthCheck - 1)

        result = node_chain.submitblock(ToHex(merged_mined_block))
        assert 'auxpow-self' in result

        node_chain.generate(1)
        result = node_chain.submitblock(ToHex(merged_mined_block))
        assert 'inconclusive' in result

        # Sync blocks within their own chains
        self.sync_all([self.nodes[:2], self.nodes[2:]])


    def test_selfMergeAttack_Merged_Parent(self):

        node_chain = self.nodes[0]

        # Mine a block to leave initial block download
        node_chain.generate(1)
        block_template = node_chain.getblocktemplate()

        # create merged mined block of auxpow attack
        coinbase_tx = self.createCoinbaseWithSequence(height=int(block_template["height"]) + 1)
        merged_mined_block = self.createBlockFromTemplate(block_template=block_template, coinbase_tx=coinbase_tx)
        hash_merged_mined_block = bytearray(binascii.unhexlify(merged_mined_block.hash))

        # create parent block of auxpow attack, include in coinbase the merged mined block hash
        coinbase_tx = self.createCoinbaseWithSequence(height=int(block_template["height"]) + 1)
        coinbase_tx.vin[0].scriptSig += b"fabe" + binascii.hexlify (b"m" * 2)
        coinbase_tx.vin[0].scriptSig += hash_merged_mined_block
        coinbase_tx.vin[0].scriptSig += b"01000000" + (b"00" * 4)
        coinbase_tx.rehash()

        parent_block = self.createBlockFromTemplate(block_template=block_template, coinbase_tx=coinbase_tx)
        parent_block.solve()

        # Now, build valid auxpow information to try to submit to the chain
        merkleTx = CMerkleTx.fromData(copy.deepcopy(coinbase_tx), parent_block.sha256, [], 0)
        merged_mined_block.auxpow = CAuxPow.fromData(merkleTx, [], 0, parent_block)

        result = node_chain.submitblock(ToHex(merged_mined_block))
        assert result is None

        result = node_chain.submitblock(ToHex(parent_block))
        assert 'auxpow-self' in result

        # Sync blocks within their own chains
        self.sync_all([self.nodes[:2], self.nodes[2:]])

    def test_invalidMerkleBranch(self):

        node_child_chain = self.nodes[0]    # child chain network
        node_parent_chain = self.nodes[2]   # parent chain network

        # Mine a block to leave initial block download
        node_child_chain.generate(1)
        child_blk_tmpl = node_child_chain.getblocktemplate()

        # create first child block of auxpow attack
        coinbase_tx = self.createCoinbaseWithSequence(height=int(child_blk_tmpl["height"]) + 1)
        child_block1 = self.createBlockFromTemplate(block_template=child_blk_tmpl, coinbase_tx=coinbase_tx)

        # create second child block of auxpow attack
        coinbase_tx = self.createCoinbaseWithSequence(height=int(child_blk_tmpl["height"]) + 2)
        child_block2 = self.createBlockFromTemplate(block_template=child_blk_tmpl, coinbase_tx=coinbase_tx, hash_prevblock=child_block1.sha256)

        hash_block1 = bytearray(binascii.unhexlify(child_block1.hash))
        hash_block1.reverse()
        hash_block2 = bytearray(binascii.unhexlify(child_block2.hash))
        hash_block2.reverse()

        combined_hash = hash256(hash_block1 + hash_block2)
        combined_hash = bytearray(combined_hash)
        combined_hash.reverse()

        # create parent block of auxpow attack
        node_parent_chain.generate(1)
        parent_blk_tmpl = node_parent_chain.getblocktemplate()
        coinbase_tx = self.createCoinbaseWithSequence(height=int(parent_blk_tmpl["height"]) + 1)
        coinbase_tx.vin[0].scriptSig += b"fabe" + binascii.hexlify (b"m" * 2)
        coinbase_tx.vin[0].scriptSig += combined_hash
        coinbase_tx.vin[0].scriptSig += b"01000000" + (b"00" * 4)
        coinbase_tx.rehash()

        parent_block = self.createBlockFromTemplate(block_template=parent_blk_tmpl, coinbase_tx=coinbase_tx)
        parent_block.solve()

        result = node_parent_chain.submitblock(ToHex(parent_block))
        assert result is None

        # Now, build valid auxpow information to submit to child chain
        merkleTx = CMerkleTx.fromData(copy.deepcopy(coinbase_tx), parent_block.sha256, [], 0)

        child_block1.auxpow = CAuxPow.fromData(merkleTx, [child_block2.sha256], 1, parent_block)
        result = node_child_chain.submitblock(ToHex(child_block1))
        assert 'high-hash' in result

        child_block1.auxpow = CAuxPow.fromData(merkleTx, [child_block2.sha256], 0, parent_block)
        result = node_child_chain.submitblock(ToHex(child_block1))
        assert result is None

        child_block2.auxpow = CAuxPow.fromData(copy.deepcopy(merkleTx), [child_block1.sha256], 0, parent_block)
        result = node_child_chain.submitblock(ToHex(child_block2))
        assert 'high-hash' in result

        # Sync blocks within their own chains
        self.sync_all([self.nodes[:2], self.nodes[2:]])


    @staticmethod
    def createCoinbaseWithSequence(height=0):
        coinbase_tx = create_coinbase(height=height)
        coinbase_tx.vin[0].nSequence = 2 ** 32 - 2
        coinbase_tx.rehash()
        return coinbase_tx

    @staticmethod
    def createBlockFromTemplate(block_template, coinbase_tx=None, hash_prevblock=None):
        block = CBlock()
        block.nVersion = block_template["version"]

        if hash_prevblock is None:
            block.hashPrevBlock = int(block_template["previousblockhash"], 16)
        else:
            block.hashPrevBlock = hash_prevblock

        block.nTime = block_template["curtime"]
        block.nBits = int(block_template["bits"], 16)
        block.nNonce = 0
        block.vtx = [coinbase_tx]
        block.hashMerkleRoot = block.calc_merkle_root()
        block.rehash()
        return block


if __name__ == '__main__':
    AuxPowAttackTest().main()
