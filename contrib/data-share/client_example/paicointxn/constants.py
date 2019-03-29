"""
PAI Coin configuration constants
"""

# constants.py
#
# PAI Coin configuration constants
#
# Copyright (c) ObEN, Inc. - https://oben.me/
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

from platform import system


# RPC ports
PAICOIN_IP = '127.0.0.1'
PAICOIN_PORT_MAINNET = 8566
PAICOIN_PORT_TESTNET = 18566

# REGTEST NOT SUPPORTED
# PAICOIN_PORT_REGTEST = 19566

PAICOIN_USER = ''
PAICOIN_PASSWORD = ''

PAICOIN_FEE = 0.0001
PAICOIN_DUST = 0.00001

OP_RETURN_MAX = 80

MAX_BLOCKS = 10

NET_TIMEOUT = 10

# RANGES
SF8 = 1 << 8
SF16 = 1 << 16
SF32 = 1 << 32

os_type = system()

if os_type == 'Darwin':
    PAICOIN_CONF_PATH = '/Library/Application Support/PAIcoin/paicoin.conf'
elif os_type == 'Linux':
    PAICOIN_CONF_PATH = '/.paicoin/paicoin.conf'
else:
    PAICOIN_CONF_PATH = ''
    raise OSError('Operating Sytem: {} not yet supported'.format(os_type))
