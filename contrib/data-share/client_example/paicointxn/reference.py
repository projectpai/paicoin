"""
PAI Coin reference for labeling OP_RETURN PAI Coin transactions
"""

# reference.py
#
# PAI Coin reference for labeling OP_RETURN PAI Coin transactions
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

from re import search
from unpacker import hex2bin
from constants import SF8, SF16, MAX_BLOCKS


def calc_ref(next_height, txid, avoid_txids):
    """
    Working with data references

    The format of a data reference is: [estimated block height]-[partial txid]:

    [estimated block height] is the block where the first transaction might
    appear and followed by all subsequent transactions are expected to appear.
    In the event of a weird blockchain reorg, it is possible the first
    transaction might appear in a slightly earlier block. When embedding data,
    we set [estimated block height] to 1+(the current block height).

    [partial txid] contains 2 adjacent bytes from the txid, at a specific
    position in the txid: 2*([partial txid] div 65536) gives the offset of the 2
    adjacent bytes, between 0 and 28. ([partial txid] mod 256) is the byte of
    the txid at that offset. (([partial txid] mod 65536) div 256) - byte of the
    txid at that offset + 1.

    Note that the txid is ordered according to user presentation,
    not raw data in the block.
    :param next_height: Next block height
    :param txid: Tx Id
    :param avoid_txids: Tx IDs that needs to be avoided
    :return: transaction reference
    """
    txid_bin = hex2bin(txid)

    clashed = False
    offset = 0

    for offset in range(15):
        sub_txid = txid_bin[2 * offset:2 * offset + 2]
        clashed = False

        for avoid_txid in avoid_txids:
            avoid_txid_bin = hex2bin(avoid_txid)

            if ((avoid_txid_bin[2 * offset:2 * offset + 2] ==
                 sub_txid) and (txid_bin != avoid_txid_bin)):
                clashed = True
                break

        if not clashed:
            break

    if clashed:
        return None

    ord1 = ord(txid_bin[2 * offset:1 + 2 * offset])
    ord2 = ord(txid_bin[1 + 2 * offset:2 + 2 * offset])
    tx_ref = ord1 + SF8 * ord2 + SF16 * offset

    return '%06d-%06d' % (next_height, tx_ref)


def match_ref_txid(ref, txid):
    parts = get_ref_parts(ref)
    if not parts:
        return None

    txid_offset = int(parts[1] / SF16)
    txid_binary = hex2bin(txid)

    txid_part = txid_binary[2 * txid_offset:2 * txid_offset + 2]
    txid_match = bytearray([parts[1] % SF8, int((parts[1] % SF16) / SF8)])

    return txid_part == txid_match


def get_ref_parts(ref):
    if not search('^[0-9]+-[0-9A-Fa-f]+$', ref):
        return None

    parts = ref.split('-')

    if search('[A-Fa-f]', parts[1]):
        if len(parts[1]) >= 4:
            txid_bin = hex2bin(parts[1][0:4])
            parts[1] = ord(txid_bin[0:1]) + SF8 * ord(txid_bin[1:2]) + SF16 * 0
        else:
            return None

    parts = list(map(int, parts))

    if parts[1] > (14 * SF16 * SF16):
        return None

    return parts


def get_ref_heights(ref, max_height):
    parts = get_ref_parts(ref)

    if not parts:
        return None

    return get_try_heights(parts[0], max_height, True)


def get_try_heights(est_height, max_height, also_back):
    forward_height = est_height
    back_height = min(forward_height - 1, max_height)

    heights = []
    mempool = False
    try_height = 0

    while True:
        if also_back and ((try_height % 3) == 2):  # step back every 3 tries
            heights.append(back_height)
            back_height -= 1
        else:
            if forward_height > max_height:
                if not mempool:
                    heights.append(0)  # indicates to try mempool
                    mempool = True

                elif not also_back:
                    break
            else:
                heights.append(forward_height)

            forward_height += 1

        if len(heights) >= MAX_BLOCKS:
            break

        try_height += 1

    return heights


def find_spent_txid(txns, spent_txid, spent_vout):
    for txid, txn_unpacked in txns.items():
        for tx_in in txn_unpacked['vin']:
            if (tx_in['txid'] == spent_txid) and (tx_in['vout'] == spent_vout):
                return txid

    return None


def find_txn_data(txn_unpacked):
    for index, output in enumerate(txn_unpacked['vout']):
        op_return = get_script_data(hex2bin(output['scriptPubKey']))

        if op_return:
            return {'index': index, 'op_return': op_return}

    return None


def get_script_data(scr_pk):
    op_return = None

    if scr_pk[0:1] == b'\x6a':
        first_ord = ord(scr_pk[1:2])

        if first_ord <= 75:
            op_return = scr_pk[2:2 + first_ord]
        elif first_ord == 0x4c:
            op_return = scr_pk[3:3 + ord(scr_pk[2:3])]
        elif first_ord == 0x4d:
            op_return = scr_pk[4:4 + ord(scr_pk[2:3]) + SF8 * ord(scr_pk[3:4])]

    return op_return
