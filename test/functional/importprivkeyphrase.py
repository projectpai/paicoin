#!/usr/bin/env python3
# Copyright (c) 2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test 'importprivkeyphrase' RPC call."""

import os

from test_framework.test_framework import PAIcoinTestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes_bi,
)
import shutil

def read_dump(file_name, label):
    """
    Read the given dump and returns the key path, the key and the address.
    """
    with open(file_name, encoding='utf8') as inputfile:
        for line in inputfile:
            # only read non comment lines
            if line[0] != "#" and len(line) > 10:
                # split out some data
                key_label, comment = line.split("#")
                key = key_label.split(' ')[0]
                keytype = key_label.split(' ')[2]
                found_type = keytype.split('=')[0]
                found_label = keytype.split('=')[1]
                assert(len(comment) > 1)
                if found_type == 'label' and found_label == label:
                    addr_keypath = comment.split(" addr=")[1]
                    keypath = addr_keypath.rstrip().split("hdkeypath=")[1]
                    addr = addr_keypath.split(' ')[0]
                    return keypath, key, addr

        return None, None, None

class ImportPrivKeyPhraseTest(PAIcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [['-usehd=0'], ['-usehd=1', '-keypool=0']]

    def run_test (self):
        tmpdir = self.options.tmpdir

        # Make sure can't switch off usehd after wallet creation
        self.stop_node(1)
        self.assert_start_raises_init_error(1, ['-usehd=0'], 'already existing HD wallet')
        self.start_node(1)
        connect_nodes_bi(self.nodes, 0, 1)

        # Make sure we use hd, keep masterkeyid
        masterkeyid = self.nodes[1].getwalletinfo()['hdmasterkeyid']
        assert_equal(len(masterkeyid), 40)

        # Import an HD private key (not going to be activated)
        label = 'frompaperkey'
        self.nodes[1].importprivkeyphrase('grunt curtain saddle couch ice detect drastic blossom mosquito message giraffe always', label)
        dump_file = tmpdir + '/wallet.dump'
        result = self.nodes[1].dumpwallet(dump_file)
        assert_equal(result['filename'], os.path.abspath(dump_file))

        # Read the dump
        read_keypath, read_key, read_addr = read_dump(dump_file, label)

        # Verify the key value, address and whether it is imported with path of 'm'
        assert_equal(read_keypath, 'm')
        assert_equal(read_key, 'aSUC1GqoM2bTJ2QUKAbuiLxajYfzW4kiuV1JNXqh91e47tWyinJj')
        assert_equal(read_addr, 'MYmcLeah2wPeCs7AzCjMsBrFZMJe7Uvune')

if __name__ == '__main__':
    ImportPrivKeyPhraseTest().main ()
