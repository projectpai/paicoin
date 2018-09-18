#!/usr/bin/env python3
# Copyright (c) 2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test 'restorewallet' RPC call."""

import os

from test_framework.test_framework import PAIcoinTestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes_bi,
)
import shutil

def has_key(file_name, key):
    """
    Read the given dump and returns if the passed key is there.
    """
    with open(file_name, encoding='utf8') as inputfile:
        for line in inputfile:
            # only read non comment lines
            if line[0] != "#" and len(line) > 10:
                line = line.rstrip()
                tokensLine = line.split(' ')
                tokensKey = key.split(' ')
                if len(tokensLine) != 6 or len(tokensKey) != 6:
                    return False
                if (tokensLine[0] == tokensKey[0] and tokensLine[2] == tokensKey[2] and tokensLine[4] == tokensKey[4] and tokensLine[5] == tokensKey[5]):
                    return True

        return False

class RestoreWalletTest(PAIcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [['-usehd=0'], ['-usehd=1', '-keypool=32']]

    def run_test (self):
        tmpdir = self.options.tmpdir

        # Make sure can't switch off usehd after wallet creation
        self.stop_node(1)
        self.assert_start_raises_init_error(1, ['-usehd=0'], 'already existing HD wallet')
        self.start_node(1)
        connect_nodes_bi(self.nodes, 0, 1)

        # Restore a wallet
        wallet_file = 'wallet_restore_test.dat'
        dump_file = tmpdir + '/wallet_restore_test.dump'
        self.nodes[1].restorewallet('grunt curtain saddle couch ice detect drastic blossom mosquito message giraffe always', wallet_file)

        # Restart the node with the restored wallet file
        self.stop_node(1)
        self.start_node(1, ['-wallet=' + wallet_file])
        connect_nodes_bi(self.nodes, 0, 1)

        # Make sure we use hd, keep masterkeyid
        masterkeyid = self.nodes[1].getwalletinfo()['hdmasterkeyid']
        assert_equal(len(masterkeyid), 40)

        result = self.nodes[1].dumpwallet(dump_file)
        assert_equal(result['filename'], os.path.abspath(dump_file))

        # Verify that key generation is successful
        # Master HD Key
        assert(has_key(
            dump_file,
            'aSUC1GqoM2bTJ2QUKAbuiLxajYfzW4kiuV1JNXqh91e47tWyinJj 2018-09-18T14:36:31Z hdmaster=1 # addr=MYmcLeah2wPeCs7AzCjMsBrFZMJe7Uvune hdkeypath=m')
        )

        # External chain
        assert(has_key(
            dump_file,
            'aZ7pG1YX1mCgTt8jd7TvYQaj5sQ6hqNq8mR5ThuRpmrUtrRnm7JS 2018-09-18T14:36:31Z reserve=1 # addr=MdyN6QXrWZJQAn5G8QEtaj7WhubNnaeMMF hdkeypath=m/0\'/0\'/14\'')
        )
        assert(has_key(
            dump_file,
            'aYh1fJNKv3SFG5F8wLJEaNxLeTAtMWscJVMYSPdv4gy2LBXvicWs 2018-09-18T14:36:31Z reserve=1 # addr=MsgURqVkMWj8oQJuxbtpGr2jhYYxdtt5n4 hdkeypath=m/0\'/0\'/31\'')
        )

        # Internal chain
        assert(has_key(
            dump_file,
            'aVbnM2JJL78gPcn2ddnoiLsgVLLLRGfiRjFssPaSPYrzWHRDcxci 2018-09-18T14:36:54Z reserve=1 # addr=MqEEiQKri81w1CZNaPYhwZ9YtxJRATHRZJ hdkeypath=m/0\'/1\'/19\'')
        )
        assert(has_key(
            dump_file,
            'aVjHGRLQxacPxrEVM7jmhoFyNGhFx1VDhxssncdTBcULBi2iCmZ2 2018-09-18T14:36:54Z reserve=1 # addr=MuE8quWnW23xr65y3s6VZY2rAUQtTM78Fh hdkeypath=m/0\'/1\'/31\'')
        )

if __name__ == '__main__':
    RestoreWalletTest().main ()
