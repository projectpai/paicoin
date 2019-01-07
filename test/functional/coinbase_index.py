#!/usr/bin/env python3
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test Coinbase Index implementation

Scenarios:

- Invalid height
  Tries to create a coinbase transaction with an invalid height parameter
- Invalid public key
  Tries to create a coinbase transaction with random chars as public key destination
- Valid scenario
  Creates a new coinbase transaction and submits a solved block paying to the newly
  inserted coinbase address. 
"""

import copy
import binascii
import os
import time
import ecdsa
from binascii import b2a_hex
from decimal import Decimal

from test_framework.blocktools import create_coinbase
from test_framework.mininode import CBlock, ToHex, ripemd160
from test_framework.test_framework import PAIcoinTestFramework

from test_framework import util

def generate_keypair():
    sk = ecdsa.SigningKey.generate(curve=ecdsa.SECP256k1)
    pk = sk.get_verifying_key()

    sk_bytes = sk.to_string()
    pk_bytes = b'\x04' + pk.to_string()

    sk_string = binascii.hexlify(sk_bytes).decode('ascii').lower()
    pk_string = binascii.hexlify(pk_bytes).decode('ascii').lower()

    return sk_string, pk_string

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

class CoinbaseIndexTest(PAIcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.enable_mocktime()

    def enable_mocktime (self):
        self.mocktime = 1529934120 # Monday, June 25, 2018 1:42:00 PM GMT
    
    def write_keys_to_dir(self, keys, filename):
        path = self.datadir + '/regtest/coinbase/' + filename
        with open(path, 'w') as f:
            f.writelines(keys)
    
    def remove_cb_index_cache(self):
        path = self.datadir + '/regtest/coinbase/'
        for i in ['cache', 'index']:
            os.unlink(path + i)

    def run_test(self):
        self.datadir = util.get_datadir_path(self.options.tmpdir, 0)
        self.secret_key, self.public_key = generate_keypair()
        self.write_keys_to_dir([self.secret_key + '\n'], 'seckeys')

        self.nodes[0].generate(200)
        self.sync_all()

        self.test_invalidHeight()
        self.test_invalidPubKey()
        self.test_createNewCoinbaseAndSubmitNewBlock()
    
    def test_invalidHeight(self):
        chain_node = self.nodes[0]

        util.assert_raises_jsonrpc(-8, None, chain_node.createcoinbasetransaction, self.public_key, str(-2))
        util.assert_raises_jsonrpc(-8, None, chain_node.createcoinbasetransaction, self.public_key, str(1))

        block_count = int(chain_node.getblockcount())
        util.assert_raises_jsonrpc(-8, None, chain_node.createcoinbasetransaction, self.public_key, str(block_count))
        util.assert_raises_jsonrpc(-32700, None, chain_node.createcoinbasetransaction, self.public_key, "aa")
    
    def test_invalidPubKey(self):
        chain_node = self.nodes[0]

        util.assert_raises_jsonrpc(-5, None, chain_node.createcoinbasetransaction, "aa", str(10000))
    
    def test_createNewCoinbaseAndSubmitNewBlock(self):
        chain_node = self.nodes[0]
        sec_key, pub_key = generate_keypair()

        block_count = int(chain_node.getblockcount())
        result = chain_node.createcoinbasetransaction(pub_key, str(block_count + 100))
        assert result is not None

        chain_node.generate(3)
        self.sync_all()

        block_count = int(chain_node.getblockcount())
        block_template = chain_node.getblocktemplate()
        coinbase_tx = create_coinbase(height=int(block_template["height"]) + 1, pubkey=ripemd160(pub_key.encode('utf-8')))
        block = createBlockFromTemplate(block_template, coinbase_tx=coinbase_tx)
        block.solve()

        result = chain_node.submitblock(ToHex(block))
        assert result is None


if __name__ == '__main__':
    CoinbaseIndexTest().main()
