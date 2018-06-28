#!/usr/bin/env python3
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test AuxPow attacks

- double proof attack
- self merged mining attack"""

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

    def enable_mocktime (self):
        self.mocktime = 1529934120 # Monday, June 25, 2018 1:42:00 PM GMT

    def run_test(self):
        # simulate 2 chains/networks for parent/child chain
        self.split_network()
        self.test_doubleProofAttack()

    def test_doubleProofAttack(self):

        node_child_chain = self.nodes[0]    # child chain network
        node_parent_chain = self.nodes[2]   # parent chain network

        # Mine a block to leave initial block download
        node_child_chain.generate(1)
        child_blk_tmpl = node_child_chain.getblocktemplate()

        # create first child block of auxpow attack
        coinbase_tx = create_coinbase(height=int(child_blk_tmpl["height"]) + 1)
        coinbase_tx.vin[0].nSequence = 2 ** 32 - 2
        coinbase_tx.rehash()

        block1 = CBlock()
        block1.nVersion = child_blk_tmpl["version"]
        block1.hashPrevBlock = int(child_blk_tmpl["previousblockhash"], 16)
        block1.nTime = child_blk_tmpl["curtime"]
        block1.nBits = int(child_blk_tmpl["bits"], 16)
        block1.nNonce = 0
        block1.vtx = [coinbase_tx]
        block1.hashMerkleRoot = block1.calc_merkle_root()
        block1.rehash()

        # create second child block of auxpow attack
        coinbase_tx = create_coinbase(height=int(child_blk_tmpl["height"]) + 2)
        coinbase_tx.vin[0].nSequence = 2 ** 32 - 2
        coinbase_tx.rehash()

        block2 = CBlock()
        block2.nVersion = child_blk_tmpl["version"]
        block2.hashPrevBlock = block1.sha256
        block2.nTime = child_blk_tmpl["curtime"]
        block2.nBits = int(child_blk_tmpl["bits"], 16)
        block2.nNonce = 0
        block2.vtx = [coinbase_tx]
        block2.hashMerkleRoot = block2.calc_merkle_root()
        block2.rehash()

        hash_block1 = bytearray(binascii.unhexlify(block1.hash))
        hash_block1.reverse()
        hash_block2 = bytearray(binascii.unhexlify(block2.hash))
        hash_block2.reverse()

        combined_hash = hash256(hash_block1 + hash_block2)
        combined_hash = bytearray(combined_hash)
        combined_hash.reverse()

        # create parent block of auxpow attack
        node_parent_chain.generate(1)
        parent_blk_tmpl = node_parent_chain.getblocktemplate()
        coinbase_tx = create_coinbase(height=int(parent_blk_tmpl["height"]) + 1)
        coinbase_tx.vin[0].nSequence = 2 ** 32 - 2
        coinbase_tx.vin[0].scriptSig += b"fabe" + binascii.hexlify (b"m" * 2)
        coinbase_tx.vin[0].scriptSig += combined_hash
        coinbase_tx.vin[0].scriptSig += b"01000000" + (b"00" * 4)
        coinbase_tx.rehash()

        block_parent = CBlock()
        block_parent.nVersion = parent_blk_tmpl["version"]
        block_parent.hashPrevBlock = int(parent_blk_tmpl["previousblockhash"], 16)
        block_parent.nTime = parent_blk_tmpl["curtime"]
        block_parent.nBits = int(parent_blk_tmpl["bits"], 16)
        block_parent.nNonce = 0
        block_parent.vtx = [coinbase_tx]
        block_parent.hashMerkleRoot = block_parent.calc_merkle_root()
        block_parent.rehash()
        block_parent.solve()

        node_parent_chain.submitblock(ToHex(block_parent))

        # Now, build valid auxpow information to submit to child chain
        merkleTx = CMerkleTx()
        merkleTx.tx = copy.deepcopy(coinbase_tx)
        merkleTx.hashBlock = block_parent.sha256
        merkleTx.vMerkleBranch = []
        merkleTx.nIndex = 0

        auxpow = CAuxPow()
        auxpow.parentCoinbase = merkleTx
        auxpow.vChainMerkleBranch = [block2.sha256]
        auxpow.nChainIndex = 0
        auxpow.parentBlock = CBlockHeader(header=block_parent)

        block1.auxpow = auxpow
        result = node_child_chain.submitblock(ToHex(block1))
        assert result is None

        auxpow2 = CAuxPow()
        auxpow2.parentCoinbase = copy.deepcopy(merkleTx)
        auxpow2.vChainMerkleBranch = [block1.sha256]
        auxpow2.nChainIndex = 1
        auxpow2.parentBlock = CBlockHeader(header=block_parent)

        block2.auxpow = auxpow2
        result = node_child_chain.submitblock(ToHex(block2))
        assert 'auxpow-double-proof' in result

        # Sync blocks within their own chains
        self.sync_all([self.nodes[:2], self.nodes[2:]])

if __name__ == '__main__':
    AuxPowAttackTest().main()
