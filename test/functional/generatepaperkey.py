#!/usr/bin/env python3

# https://github.com/trezor/python-mnemonic/blob/master/mnemonic/mnemonic.py:
# Copyright (c) 2013 Pavol Rusnak
# Copyright (c) 2017 mruddy
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
# of the Software, and to permit persons to whom the Software is furnished to do
# so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
# WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
# Copyright (c) 2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test 'generatepaperkey' RPC call."""

from test_framework.test_framework import PAIcoinTestFramework
from test_framework.util import (
    assert_equal,
)
import binascii
import hashlib

class GeneratePaperKeyTest(PAIcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def import_word_list(self, bip39_words_file):
        file = open(bip39_words_file, 'r')

        self.bip39_words = []
        line = file.readline()
        while line:
            self.bip39_words.append(line.strip())
            line = file.readline()

        file.close()
        assert_equal(len(self.bip39_words), 2048)
    
    def check_paper_key(self, paper_key):
        paper_key = paper_key.split(' ')

        assert_equal(len(paper_key), 12)

        try:
            idx = map(lambda x: bin(self.bip39_words.index(x))[2:].zfill(11), paper_key)
            b = ''.join(idx)
        except ValueError:
            assert(False)
        l = len(b)  # noqa: E741
        d = b[:l // 33 * 32]
        h = b[-l // 33:]
        nd = binascii.unhexlify(hex(int(d, 2))[2:].rstrip('L').zfill(l // 33 * 8))
        nh = bin(int(hashlib.sha256(nd).hexdigest(), 16))[2:].zfill(256)[:l // 33]
        assert_equal(h, nh)

    def run_test (self):
        self.import_word_list('../../src/wallet/bip39words')

        for i in range(1000):
            paper_key = self.nodes[0].generatepaperkey()
            self.check_paper_key(paper_key)

if __name__ == '__main__':
    GeneratePaperKeyTest().main ()
