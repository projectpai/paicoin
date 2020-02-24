#!/usr/bin/env python3
# Copyright (c) 2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test the search operations RPC commands
    - search operations (ported from Decred) include: existsaddress existsaddresses existsmempooltxs searchrawtransactions
"""

from test_framework.test_framework import PAIcoinTestFramework
from test_framework.util import *

class TicketOperations(PAIcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1

    def run_test(self):
        # existsexpiredtickets tests:
        # 1. valid parameters
        x = self.nodes[0].existsexpiredtickets('txhashblob')
        assert(x == "")
        # TODO make this test have valid input and output once the command is implemented

        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].existsexpiredtickets)
        assert_raises_rpc_error(-1, None, self.nodes[0].existsexpiredtickets, "param1", "param2")


        # existsliveticket tests:
        # 1. valid parameters
        x = self.nodes[0].existsliveticket('txhash')
        assert(x == False)
        # TODO make this test have valid input and output once the command is implemented

        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].existsliveticket)
        assert_raises_rpc_error(-1, None, self.nodes[0].existsliveticket, "param1", "param2")


        # existslivetickets tests:
        # 1. valid parameters
        x = self.nodes[0].existslivetickets('txhashblob')
        assert(x == "")
        # TODO make this test have valid input and output once the command is implemented

        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].existslivetickets)
        assert_raises_rpc_error(-1, None, self.nodes[0].existslivetickets, "param1", "param2")


        # existsmissedtickets tests:
        # 1. valid parameters
        x = self.nodes[0].existsmissedtickets('txhashblob')
        assert(x == "")
        # TODO make this test have valid input and output once the command is implemented

        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].existsmissedtickets)
        assert_raises_rpc_error(-1, None, self.nodes[0].existsmissedtickets, "param1", "param2")


        # getticketpoolvalue tests:
        # 1. valid parameters
        x = self.nodes[0].getticketpoolvalue()
        assert(x == 0.000)
        # TODO make this test have valid input and output once the command is implemented

        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].getticketpoolvalue, 'param')
        assert_raises_rpc_error(-1, None, self.nodes[0].getticketpoolvalue, "param1", "param2")


        # livetickets tests:
        # 1. valid parameters
        x = self.nodes[0].livetickets()
        assert(x == {})
        # TODO make this test have valid input and output once the command is implemented

        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].livetickets, 'param')
        assert_raises_rpc_error(-1, None, self.nodes[0].livetickets, "param1", "param2")


        # missedtickets tests:
        # 1. valid parameters
        x = self.nodes[0].missedtickets()
        assert(x == {})
        # TODO make this test have valid input and output once the command is implemented

        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].missedtickets, 'param')
        assert_raises_rpc_error(-1, None, self.nodes[0].missedtickets, "param1", "param2")


        # ticketfeeinfo tests:
        # 1. valid parameters
        x = self.nodes[0].ticketfeeinfo()
        assert(x == {})
        x = self.nodes[0].ticketfeeinfo(5)   # set blocks
        assert(x == {})
        x = self.nodes[0].ticketfeeinfo(5,3) # set blocks and windows
        assert(x == {})
        # TODO make this test have valid input and output once the command is implemented

        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].ticketfeeinfo, "param1", "param2", "param3")


        # ticketsforaddress tests:
        # 1. valid parameters
        x = self.nodes[0].ticketsforaddress("address")
        assert(x == {})
        # TODO make this test have valid input and output once the command is implemented

        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].ticketsforaddress)
        assert_raises_rpc_error(-1, None, self.nodes[0].ticketsforaddress, "param1", "param2")


        # ticketvwap tests:
        # 1. valid parameters
        x = self.nodes[0].ticketvwap()
        assert(x == 0.000)
        x = self.nodes[0].ticketvwap(10)    # set start
        assert(x == 0.000)
        x = self.nodes[0].ticketvwap(10,20) # set start and stop
        assert(x == 0.000)
        # TODO make this test have valid input and output once the command is implemented

        # 2. invalid parameters
        assert_raises_rpc_error(-1, None, self.nodes[0].ticketvwap, "param1", "param2", "param3")


if __name__ == "__main__":
    TicketOperations().main()