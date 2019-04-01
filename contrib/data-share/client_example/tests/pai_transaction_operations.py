# pai_transaction_operations.py
#
# PAI Coin Send/Store/Retrieve PAI Protocol in blockchain test script
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

from time import sleep
from pprint import pprint
from paicointxn import PAI, Transaction


address = 'MqUSBmN24cvWh2AKnzg8aj59snJxd8hdjU'
amount = 0.0001

pai = PAI(version=0x10)

pai_pack = pai.pack('revoke', '', 'MyPAI', 'voice')
print('Raw PAI pai_pack: {}'.format(pai_pack))
print('Assembled PAI pai_pack:')
print(pai)

paicoin_txn = Transaction(testnet=True)
res = paicoin_txn.send(address, amount, pai_pack)

if 'error' in res:
    print('PAI Coin::Send TransactionError: {}\n'.format(res['error']))
else:
    print('PAI Coin::Send TxID: {}\n'.format(res['txid']))

timeout = 20

print 'Wait {} seconds to make sure transaction is sent...'.format(timeout)

sleep(timeout)


res = paicoin_txn.store(pai_pack)

pai_reference = None
if 'error' in res:
    print('PAI Coin::Store TransactionError: {}\n'.format(res['error']))
else:
    pai_reference = res['ref']
    print('TxIDs:\n\n{}\n\nRef: {}\n'.format(''.join(res['txids']), res['ref']))

if pai_reference:
    res = paicoin_txn.retrieve(pai_reference, 1)
    if 'error' in res:
        print('PAI Coin::Retrieve TransactionError: {}\n'.format(res['error']))

    elif len(res):
        for r in res:
            data = r['data']
            pai_unpack = pai.unpack(data)
            print('PAI unpack pack[1] result:')
            pprint(pai_unpack)
            print('\nDisassembled PAI pack[1]:')
            print(pai)
