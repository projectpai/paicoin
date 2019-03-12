"""
PAI Coin helpers for packing/unpacking PAI Coin transactions
"""

# unpacker.py
#
# PAI Coin helpers for packing/unpacking PAI Coin transactions
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

from hashlib import sha256
from struct import pack, unpack
from binascii import a2b_hex, b2a_hex, Error, Incomplete

from constants import *


def pack_txn(txn):
    binary = b''

    binary += pack('<L', txn['version'])

    binary += pack_varint(len(txn['vin']))

    for tx_input in txn['vin']:
        binary += hex2bin(tx_input['txid'])[::-1]
        binary += pack('<L', tx_input['vout'])
        binary += pack_varint(int(len(tx_input['scriptSig']) / 2))
        binary += hex2bin(tx_input['scriptSig'])
        binary += pack('<L', tx_input['sequence'])

    binary += pack_varint(len(txn['vout']))

    for tx_output in txn['vout']:
        binary += pack_uint64(int(round(tx_output['value'] * 100000000)))
        binary += pack_varint(int(len(tx_output['scriptPubKey']) / 2))
        binary += hex2bin(tx_output['scriptPubKey'])

    binary += pack('<L', txn['locktime'])

    return binary


def pack_varint(integer):
    if integer > 0xFFFFFFFF:
        packed = '\xFF' + pack_uint64(integer)
    elif integer > 0xFFFF:
        packed = '\xFE' + pack('<L', integer)
    elif integer > 0xFC:
        packed = '\xFD' + pack('<H', integer)
    else:
        packed = pack('B', integer)

    return packed


def pack_uint64(integer):
    upper = int(integer / SF32)
    lower = integer - upper * SF32

    return pack('<L', lower) + pack('<L', upper)


def unpack_block(binary):
    buff = PAIcoinBuffer(binary)
    block = {'version': buff.shift_unpack(4, '<L'),
             'hashPrevBlock': bin2hex(buff.shift(32)[::-1]),
             'hashMerkleRoot': bin2hex(buff.shift(32)[::-1]),
             'time': buff.shift_unpack(4, '<L'),
             'bits': buff.shift_unpack(4, '<L'),
             'nonce': buff.shift_unpack(4, '<L'),
             'tx_count': buff.shift_varint(), 'txs': {}}

    old_ptr = buff.used()

    while buff.remaining():
        transaction = unpack_txn_buffer(buff)
        new_ptr = buff.used()
        size = new_ptr - old_ptr

        raw_txn_binary = binary[old_ptr:old_ptr + size]
        h_key = sha256(sha256(raw_txn_binary).digest()).digest()
        txid = bin2hex(h_key[::-1])

        old_ptr = new_ptr

        transaction['size'] = size
        block['txs'][txid] = transaction

    return block


def unpack_txn(binary):
    return unpack_txn_buffer(PAIcoinBuffer(binary))


def unpack_txn_buffer(buff):
    txn = {'vin': [], 'vout': [], 'version': buff.shift_unpack(4, '<L')}

    inputs = buff.shift_varint()
    if inputs > 100000:
        return None

    witness_flag = False

    if inputs == 0:
        inputs = buff.shift_unpack(1, 'B')

        if inputs == 1:
            witness_flag = True
            txn['tx_witnesses'] = []
        else:
            raise BufferError

        inputs = buff.shift_varint()

    for _ in range(inputs):
        tx_in = {'txid': bin2hex(buff.shift(32)[::-1]),
                 'vout': buff.shift_unpack(4, '<L'),
                 'scriptSig': bin2hex(buff.shift(buff.shift_varint())),
                 'sequence': buff.shift_unpack(4, '<L')}
        txn['vin'].append(tx_in)

    outputs = buff.shift_varint()
    if outputs > 100000:  # sanity check
        return None

    for _ in range(outputs):
        tx_out = {'value': float(buff.shift_uint64()) / 100000000,
                  'scriptPubKey': bin2hex(buff.shift(buff.shift_varint()))}
        txn['vout'].append(tx_out)

    if witness_flag:
        witnesses = buff.shift_varint()

        if witnesses > 100000:  # sanity check
            return None

        for _ in range(witnesses):
            tx_witness = {'data': bin2hex(buff.shift(buff.shift_varint()))}
            txn['tx_witnesses'].append(tx_witness)

    txn['locktime'] = buff.shift_unpack(4, '<L')

    return txn


class PAIcoinBuffer(object):
    def __init__(self, data, ptr=0):
        self._data = data
        self._len = len(data)
        self._ptr = ptr

    def shift(self, chars):
        prefix = self._data[self._ptr:self._ptr + chars]
        self._ptr += chars

        return prefix

    def shift_unpack(self, chars, fmt):
        unpacked = unpack(fmt, self.shift(chars))

        return unpacked[0] if len(unpacked) == 1 else unpacked

    def shift_varint(self):
        value = self.shift_unpack(1, 'B')

        if value == 0xFF:
            value = self.shift_uint64()
        elif value == 0xFE:
            value = self.shift_unpack(4, '<L')
        elif value == 0xFD:
            value = self.shift_unpack(2, '<H')

        return value

    def shift_uint64(self):
        return self.shift_unpack(4, '<L') + SF32 * self.shift_unpack(4, '<L')

    def used(self):
        return min(self._ptr, self._len)

    def remaining(self):
        return max(self._len - self._ptr, 0)


def hex2bin(hex_):
    try:
        raw = a2b_hex(hex_)
    except (TypeError, Error, Incomplete):
        return None

    return raw


def bin2hex(string):
    return b2a_hex(string).decode('utf-8')
