# send_paicoin.py
# 
# CLI wrapper for sending PAI Coin transaction with OP_RETURN metadata
#
# Copyright (c) ObEN, Inc. - https://oben.me/
# Copyright (c) Coin Sciences Ltd
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

from sys import argv, exit
from paicointxn import Transaction, hex2bin


usage_string = \
    '''Usage:
    python send_paicoin.py <send-address> <amount> <metadata> <testnet (opt)>

    Examples:
    python send_paicoin.py <send-address> 0.001 "Hello, myPAI!"
    python send_paicoin.py <send-address> 0.001 48656c6c6f2c206d7950414921
    python send_paicoin.py <send-address> 0.001 "Hello, testnet PAI Coin!" 1'''

if len(argv) < 4:
    exit(usage_string)

_, address, amount, metadata = argv[0:4]

if len(argv) > 4:
    testnet = bool(argv[4])
else:
    testnet = False

metadata_from_hex = hex2bin(metadata)

if metadata_from_hex is not None:
    metadata = metadata_from_hex

paicoin_txn = Transaction(testnet)
res = paicoin_txn.send(address, float(amount), metadata)

# Equivalent with:
# res = Transaction.paicoin_send(address, float(amount), metadata, testnet)

if 'error' in res:
    print('PAIcoinTransactionError: {}\n'.format(res['error']))
else:
    print('TxID: {}\n'.format(res['txid']))
