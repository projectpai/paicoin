#!/usr/bin/env python3
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test info operations RPC implementation

Tested RPCs:
- getblocksubsidy
- getcoinsupply
- getbestblock
- getcfilter
- getcfilterheader
- getvoteinfo
- version
- getinfo
- txfeeinfo
"""

import os
import time

from test_framework.test_framework import PAIcoinTestFramework
from test_framework import util

class InfoOPTest(PAIcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.enable_mocktime()
        self.setup_clean_chain = True
        self.extra_args = [['-txindex']] # needed for txfeeinfo

    def enable_mocktime (self):
        self.mocktime = 1529934120 # Monday, June 25, 2018 1:42:00 PM GMT
    
    def run_test(self):
        self.nodes[0].generate(200)
        self.sync_all()

        self.test_getblocksubsidy()
        self.test_getcoinsupply()
        self.test_getbestblock()
        self.test_getcfilter()
        self.test_getcfilterheader()
        self.test_getvoteinfo()
        self.test_version()
        self.test_getinfo()
        self.test_txfeeinfo()
    
    def test_getblocksubsidy(self):
        chain_node = self.nodes[0]
        assert chain_node

        result = chain_node.getblocksubsidy(100, 5)
        assert result is not None
        assert 'developer' in result.keys()
        assert 'pos' in result.keys()
        assert 'pow' in result.keys()
        assert 'total' in result.keys()

        util.assert_raises_rpc_error(-1, None, chain_node.getblocksubsidy, -1)
        util.assert_raises_rpc_error(-1, None, chain_node.getblocksubsidy)
    
    def test_getcoinsupply(self):
        chain_node = self.nodes[0]
        assert chain_node

        result = chain_node.getcoinsupply()
        assert result is not None

        util.assert_raises_rpc_error(-1, None, chain_node.getcoinsupply, 5)

    def test_getbestblock(self):
        chain_node = self.nodes[0]
        assert chain_node

        result = chain_node.getbestblock()
        assert result is not None
        assert 'hash' in result.keys()
        assert 'height' in result.keys()
        assert int(result['height']) == 200

        util.assert_raises_rpc_error(-1, None, chain_node.getbestblock, 5)
    
    def test_getcfilter(self):
        chain_node = self.nodes[0]
        assert chain_node

        result = chain_node.getcfilter("000", "111")
        assert result is not None
        assert result == "000000"

        util.assert_raises_rpc_error(-1, None, chain_node.getcfilter)
        util.assert_raises_rpc_error(-1, None, chain_node.getcfilter, "aa")
    
    def test_getcfilterheader(self):
        chain_node = self.nodes[0]
        assert chain_node

        result = chain_node.getcfilterheader("000", "111")
        assert result is not None
        assert result == "000000"

        util.assert_raises_rpc_error(-1, None, chain_node.getcfilterheader)
        util.assert_raises_rpc_error(-1, None, chain_node.getcfilterheader, "aa")
        util.assert_raises_rpc_error(-1, None, chain_node.getcfilterheader, "aa", "bb", "cc")

    def test_getvoteinfo(self):
        chain_node = self.nodes[0]
        assert chain_node

        result = chain_node.getvoteinfo(0)
        assert result is not None

        assert 'currentheight' in result
        assert result['currentheight'] == -1
        assert 'startheight' in result
        assert result['startheight'] == -1
        assert 'endheight' in result
        assert result['endheight'] == -1
        assert 'hash' in result
        assert result['hash'] == "AB"
        assert 'voteversion' in result
        assert result['voteversion'] == 0
        assert 'quorum' in result
        assert result['quorum'] == 0
        assert 'totalvotes' in result
        assert result['totalvotes'] == 1
        assert 'agendas' in result
        assert len(result['agendas']) == 1

        agenda = result['agendas'][0]
        assert 'id' in agenda
        assert agenda['id'] == "agenda"
        assert 'description' in agenda
        assert agenda['description'] == "agenda description"
        assert 'mask' in agenda
        assert agenda['mask'] == 0
        assert 'starttime' in agenda
        assert agenda['starttime'] == 0
        assert 'expiretime' in agenda
        assert agenda['expiretime'] == 0
        assert 'status' in agenda
        assert agenda['status'] == "agenda status"
        assert 'quorumprogress' in agenda
        assert agenda['quorumprogress'] == 0
        assert 'choices' in agenda
        assert len(agenda['choices']) == 1

        choice = agenda['choices'][0]
        assert 'id' in choice
        assert choice['id'] == "choice"
        assert 'description' in choice
        assert choice['description'] == "choice description"
        assert 'bits' in choice
        assert choice['bits'] == 0
        assert 'isabstain' in choice
        assert choice['isabstain'] == True
        assert 'isno' in choice
        assert choice['isno'] == True
        assert 'count' in choice
        assert choice['count'] == 1
        assert 'progress' in choice
        assert choice['progress'] == 0

        util.assert_raises_rpc_error(-1, None, chain_node.getvoteinfo)
        util.assert_raises_rpc_error(-1, None, chain_node.getvoteinfo, 1, 1)

    def test_version(self):
        chain_node = self.nodes[0]
        assert chain_node

        result = chain_node.version()
        assert result is not None
        assert 'versionstring' in result.keys()
        assert 'major' in result.keys()
        assert 'minor' in result.keys()
        assert 'patch' in result.keys()
        assert 'prerelease' in result.keys()
        assert 'buildmetadata' in result.keys()

        util.assert_raises_rpc_error(-1, None, chain_node.version, 5)

    def test_getinfo(self):
        chain_node = self.nodes[0]
        assert chain_node

        result = chain_node.getinfo()
        assert result is not None
        assert 'version' in result.keys()
        assert 'protocolversion' in result.keys()
        assert 'blocks' in result.keys()
        assert 'timeoffset' in result.keys()
        assert 'connections' in result.keys()
        assert 'proxy' in result.keys()
        assert 'difficulty' in result.keys()
        assert 'testnet' in result.keys()
        assert 'relayfee' in result.keys()
        assert 'errors' in result.keys()

        util.assert_raises_rpc_error(-1, None, chain_node.getinfo, 5)

    def test_txfeeinfo(self):
        chain_node = self.nodes[0]
        assert chain_node

        # make a tx to have smth in mempool
        newaddr = chain_node.getnewaddress()
        tx = chain_node.sendtoaddress(newaddr,0.1)

        result = chain_node.txfeeinfo(3, 1, 3)
        assert result is not None

        assert 'feeinfomempool' in result.keys()
        fim = result['feeinfomempool']
        assert 'number' in fim.keys()
        assert 'min' in fim.keys()
        assert 'max' in fim.keys()
        assert 'mean' in fim.keys()
        assert 'median' in fim.keys()
        assert 'stddev' in fim.keys()

        # generate block with tx
        chain_node.generate(1)
        self.sync_all()

        blocks = chain_node.getblockchaininfo()['blocks']
        rangeStart = blocks - 2
        rangeEnd = blocks + 1

        result = chain_node.txfeeinfo(3, rangeStart, rangeEnd)
        assert result is not None

        assert 'feeinfoblocks' in result.keys()
        assert len(result['feeinfoblocks']) == 3
        block  = result['feeinfoblocks'][0] # only last block has a transaction
        assert 'height' in block
        assert 'number' in block
        assert 'min' in block
        assert 'max' in block
        assert 'mean' in block
        assert 'median' in block
        assert 'stddev' in block
        
        assert 'feeinforange' in result.keys()
        fir = result['feeinforange']
        assert 'number' in fir.keys()
        assert 'min' in fir.keys()
        assert 'max' in fir.keys()
        assert 'mean' in fir.keys()
        assert 'median' in fir.keys()
        assert 'stddev' in fir.keys()

        util.assert_raises_rpc_error(-8, None, chain_node.txfeeinfo, 1000)
        util.assert_raises_rpc_error(-8, None, chain_node.txfeeinfo, 5, 5, 3)
        util.assert_raises_rpc_error(-8, None, chain_node.txfeeinfo, 5, 1, 500)


if __name__ == '__main__':
    InfoOPTest().main()
