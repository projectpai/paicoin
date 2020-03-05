#!/usr/bin/env python3
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test Stake API implementation

Scenarios:
- estimatestakediff
- getstakedifficulty
- getstakeversioninfo
- getstakeversions
"""

import os
import time

from test_framework.test_framework import PAIcoinTestFramework
from test_framework import util

class StakeAPITest(PAIcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.enable_mocktime()

    def enable_mocktime (self):
        self.mocktime = 1529934120 # Monday, June 25, 2018 1:42:00 PM GMT
    
    def run_test(self):
        self.nodes[0].generate(200)
        self.sync_all()
        
        self.test_estimatestakediff()
        self.test_getstakedifficulty()
        self.test_getstakeversioninfo()
        self.test_getstakeversions()
    
    def test_estimatestakediff(self):
        chain_node = self.nodes[0]
        assert chain_node

        result = chain_node.estimatestakediff(1)
        assert result is not None
        assert 'min' in result.keys()
        assert 'max' in result.keys()
        assert 'expected' in result.keys()
        assert 'user' in result.keys()

        result = chain_node.estimatestakediff()
        assert result is not None
        assert 'min' in result.keys()
        assert 'max' in result.keys()
        assert 'expected' in result.keys()
        assert(result['user'] == 0)

        util.assert_raises_rpc_error(-8, None, chain_node.estimatestakediff, -1)
        util.assert_raises_rpc_error(-1, None, chain_node.estimatestakediff, 1, 5)
    
    def test_getstakedifficulty(self):
        chain_node = self.nodes[0]
        assert chain_node

        result = chain_node.getstakedifficulty()
        assert result is not None
        assert 'current' in result.keys()
        assert 'next' in result.keys()

        util.assert_raises_rpc_error(-1, None, chain_node.getstakedifficulty, 1)
    
    def _check_getstakeversioninfo_result(self, result, numIntervals):
        assert 'currentheight' in result.keys()
        assert 'hash' in result.keys()
        assert 'intervals' in result.keys()
        assert len(result['intervals']) == numIntervals
        for interval in result['intervals']:
            assert 'startheight' in interval.keys()
            assert 'endheight' in interval.keys()
            assert 'posversions' in interval.keys()
            assert 'voteversions' in interval.keys()

            for posv in interval['posversions']:
                assert 'version' in posv.keys()
                assert 'count' in posv.keys()
            
            for votev in interval['voteversions']:
                assert 'version' in votev.keys()
                assert 'count' in votev.keys()
        return True
    
    def test_getstakeversioninfo(self):
        chain_node = self.nodes[0]
        assert chain_node

        util.assert_raises_rpc_error(-1, None, chain_node.getstakeversioninfo, 1, 2)
        util.assert_raises_rpc_error(-8, None, chain_node.getstakeversioninfo, -1)

        numIntervals = 1
        result = chain_node.getstakeversioninfo()
        assert result is not None
        assert self._check_getstakeversioninfo_result(result, numIntervals)

        numIntervals = 2
        result = chain_node.getstakeversioninfo(numIntervals)
        assert result is not None
        assert self._check_getstakeversioninfo_result(result, numIntervals)
    
    def _check_getstakeversions_result(self, result, numBlocks):
        assert len(result) == numBlocks
        for block in result:
            assert 'hash' in block.keys()
            assert 'height' in block.keys()
            assert 'blockversion' in block.keys()
            assert 'stakeversion' in block.keys()
            assert 'votes' in block.keys()

            for vote in block['votes']:
                assert 'version' in vote.keys()
                assert 'bits' in vote.keys()
        return True
    
    def test_getstakeversions(self):
        chain_node = self.nodes[0]
        assert chain_node

        util.assert_raises_rpc_error(-1, None, chain_node.getstakeversions)
        util.assert_raises_rpc_error(-1, None, chain_node.getstakeversions, "aa", 1, 5)
        util.assert_raises_rpc_error(-8, None, chain_node.getstakeversions, "zz", 1)
        util.assert_raises_rpc_error(-8, None, chain_node.getstakeversions, "aa", -1)
        util.assert_raises_rpc_error(-8, None, chain_node.getstakeversions, "zz", -1)

        info = self.nodes[0].getblockchaininfo()
        numBlocks = 1
        result = chain_node.getstakeversions(info['bestblockhash'], numBlocks)
        assert result is not None
        assert self._check_getstakeversions_result(result, numBlocks)

        numBlocks = 3
        result = chain_node.getstakeversions(info['bestblockhash'], numBlocks)
        assert result is not None
        assert self._check_getstakeversions_result(result, numBlocks)

if __name__ == '__main__':
    StakeAPITest().main()
