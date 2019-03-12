"""
PAI Coin API for generating and retrieving OP_RETURN PAI Coin transactions
"""

# transaction.py
#
# Python API for generating and retrieving OP_RETURN PAI Coin transactions
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

from time import time
from json import dumps
from requests import post, ConnectionError
from random import randint
from os.path import expanduser

from constants import *
from reference import *
from unpacker import pack_txn, unpack_txn, unpack_block, bin2hex


class Transaction(object):
    """PAI Coin OP_RETURN transactions"""

    def __init__(self, testnet=False):
        self._testnet = testnet
        self._url = None
        self._auth = None
        self._paicoin_load_config()

    def send(self, address, amount, op_return_metadata):
        """
        Send PAI Coin transaction with OP_RETURN metadata

        :param address: PAI Coin address of the recipient
        :param amount: amount to send (in units of PAI Coin)
        :param op_return_metadata: string of raw bytes containing OP_RETURN data
        :return: {'error': 'error string'}
                  or: {'txid': 'sent txid'}
        """
        try:
            if not self._paicoin_check():
                return {'error': 'Check if PAI Coin Core is running correctly'}

            result = self._paicoin_rpc('validateaddress', address)
            if not ('isvalid' in result and result['isvalid']):
                return {'error': 'Invalid send address: {}'.format(address)}

            if isinstance(op_return_metadata, basestring):
                op_return_metadata = op_return_metadata.encode('utf-8')

            meta_sz = len(op_return_metadata)

            if meta_sz > SF16:
                return {'error': 'Library supports metadata up to 65536 bytes'}

            if meta_sz > OP_RETURN_MAX:
                msg = 'Metadata size {} > {}'.format(str(meta_sz),
                                                     str(OP_RETURN_MAX))
                return {'error': msg}

            output_amount = amount + PAICOIN_FEE
            inputs_spend = self._select_inputs(output_amount)

            if 'error' in inputs_spend:
                return {'error': inputs_spend['error']}

            change_amount = float(inputs_spend['total']) - output_amount
            change_address = self._paicoin_rpc('getrawchangeaddress')

            outputs = {address: amount}

            if change_amount >= PAICOIN_DUST:
                outputs[change_address] = change_amount

            raw_txn = self._create_txn(inputs_spend['inputs'], outputs,
                                       op_return_metadata, len(outputs))
            signed_txn = self._sign_send_txn(raw_txn)

        except self.Error as err:
            return {'error': err}

        return signed_txn

    @classmethod
    def paicoin_send(cls, address, amount, op_return_metadata, testnet=False):
        txn = cls(testnet)

        return txn.send(address, amount, op_return_metadata)

    def store(self, op_return_metadata):
        """
        Store data in Blockchain using OP_RETURN

        Data will be stored in OP_RETURN within a series of chained transactions
        If the OP_RETURN is followed by another output, the data continues in
        the transaction spending that output.
        When the OP_RETURN is the last output, this signifies the end of data.

        :param op_return_metadata: string of raw bytes containing OP_RETURN data
        :return: {'error': 'error string'}
                 or: {'txid': 'sent txid', 'ref': 'ref for retrieving data' }
        """
        try:
            if not self._paicoin_check():
                return {'error': 'Check if PAI Coin Core is running correctly'}

            if isinstance(op_return_metadata, basestring):
                op_return_metadata = op_return_metadata.encode('utf-8')

            data_sz = len(op_return_metadata)
            if data_sz == 0:
                return {'error': 'Some data is required to be stored'}

            change_address = self._paicoin_rpc('getrawchangeaddress')

            output_amount = PAICOIN_FEE * int((data_sz + OP_RETURN_MAX - 1) /
                                              OP_RETURN_MAX)

            inputs_spend = self._select_inputs(output_amount)
            if 'error' in inputs_spend:
                return {'error': inputs_spend['error']}

            inputs = inputs_spend['inputs']
            input_amount = inputs_spend['total']

            height = int(self._paicoin_rpc('getblockcount'))
            avoid_txids = self._paicoin_rpc('getrawmempool')
            result = {'txids': []}

            for data_ptr in range(0, data_sz, OP_RETURN_MAX):
                last_txn = ((data_ptr + OP_RETURN_MAX) >= data_sz)
                change_amount = input_amount - PAICOIN_FEE
                metadata = op_return_metadata[data_ptr:data_ptr + OP_RETURN_MAX]

                outputs = {}
                if change_amount >= PAICOIN_DUST:
                    outputs[change_address] = change_amount

                raw_txn = self._create_txn(inputs, outputs, metadata,
                                           len(outputs) if last_txn else 0)

                send = self._sign_send_txn(raw_txn)

                if 'error' in send:
                    result['error'] = send['error']
                    break

                result['txids'].append(send['txid'])

                if data_ptr == 0:
                    result['ref'] = calc_ref(height, send['txid'], avoid_txids)

                inputs = [{'txid': send['txid'], 'vout': 1}]
                input_amount = change_amount

        except self.Error as err:
            return {'error': err}

        return result

    @classmethod
    def paicoin_store(cls, op_return_metadata, testnet=False):
        txn = cls(testnet)

        return txn.store(op_return_metadata)

    def retrieve(self, ref, max_results=1):
        """
        Retrieve data from OP_RETURN int the Blockchain

        :param ref: reference that was returned by a previous storage operation
        :param max_results: maximum number of results to retrieve (omit for 1)
        :return: {'error': 'error string'}
                  or: {'data': 'raw binary data',
                       'txids': ['1st txid', '2nd txid', ...],
                       'heights': [block 1 used, block 2 used, ...],
                       'ref': 'best ref for retrieving data',
                       'error': 'error if data only partially retrieved'}
        """
        try:
            if not self._paicoin_check():
                return {'error': 'Check if PAI Coin Core is running correctly'}

            max_height = int(self._paicoin_rpc('getblockcount'))
            heights = get_ref_heights(ref, max_height)

            if not isinstance(heights, list):
                return {'error': 'Ref is not valid'}

            results = []

            for height in heights:
                if height == 0:
                    txids = self._list_mempool_txns()
                    txns = None
                else:
                    txns = self._get_block_txns(height)
                    txids = txns.keys()

                for txid in txids:
                    if match_ref_txid(ref, txid):
                        if height == 0:
                            txn_unpacked = self._get_mempool_txn(txid)
                        else:
                            txn_unpacked = txns[txid]

                        result = self._find_op_return(txn_unpacked, txid,
                                                      height, txns, max_height)
                        if result:
                            results.append(result)

                if len(results) >= max_results:
                    break

        except self.Error as err:
            return {'error': err}

        return results

    @classmethod
    def paicoin_retrieve(cls, ref, max_results, testnet=False):
        txn = cls(testnet)

        return txn.retrieve(ref, max_results)

    def _find_op_return(self, txn_unpacked, txid, height, txns, max_height):
        txn_unpacked = dict(txn_unpacked)
        found = find_txn_data(txn_unpacked)
        result = None

        if found:
            result = {'txids': [str(txid)], 'data': found['op_return']}
            key_heights = {height: True}

            if height == 0:
                try_heights = []
            else:
                result['ref'] = calc_ref(height, txid, txns.keys())
                try_heights = get_try_heights(height + 1, max_height, False)

            if height == 0:
                this_txns = self._get_mempool_txns()
            else:
                this_txns = txns

            last_txid = txid
            this_height = height

            while found['index'] < (len(txn_unpacked['vout']) - 1):
                next_txid = find_spent_txid(this_txns, last_txid,
                                            found['index'] + 1)

                if next_txid:
                    result['txids'].append(str(next_txid))
                    txn_unpacked = this_txns[next_txid]
                    found = find_txn_data(txn_unpacked)

                    if found:
                        result['data'] += found['op_return']
                        key_heights[this_height] = True
                    else:
                        result['error'] = 'Missing OP_RETURN'
                        break
                    last_txid = next_txid
                else:
                    if len(try_heights):
                        this_height = try_heights.pop(0)

                        if this_height == 0:
                            this_txns = self._get_mempool_txns()
                        else:
                            this_txns = self._get_block_txns(this_height)
                    else:
                        result['error'] = 'Next transaction missing'
                        break

            result['heights'] = list(key_heights.keys())

        return result

    def _select_inputs(self, total_amount):
        usp_inputs = self._paicoin_rpc('listunspent', 0)

        if not isinstance(usp_inputs, list):
            return {'error': 'Could not retrieve list of unspent inputs'}

        usp_inputs.sort(key=lambda usp: usp['amount'] * usp['confirmations'],
                        reverse=True)

        inputs_spend = []
        input_amount = 0

        for unspent_input in usp_inputs:
            inputs_spend.append(unspent_input)

            input_amount += unspent_input['amount']
            if input_amount >= total_amount:
                break

        if input_amount < total_amount:
            return {'error': 'Not enough funds to cover the amount and fee'}

        return {'inputs': inputs_spend, 'total': input_amount}

    def _create_txn(self, inputs, outputs, metadata, metadata_pos):
        raw_txn = self._paicoin_rpc('createrawtransaction', inputs, outputs)

        txn_unpacked = unpack_txn(hex2bin(raw_txn))

        metadata_len = len(metadata)

        if metadata_len <= 75:
            payload = bytearray((metadata_len,)) + metadata
        elif metadata_len <= SF8:
            payload = '\x4c' + bytearray((metadata_len,)) + metadata
        else:
            payload = '\x4d' + bytearray((metadata_len % SF8,)) + \
                      bytearray((int(metadata_len / SF8),)) + metadata

        metadata_pos = min(max(0, metadata_pos), len(txn_unpacked['vout']))

        txn_unpacked['vout'][metadata_pos:metadata_pos] =\
            [{'value': 0, 'scriptPubKey': '6a' + bin2hex(payload)}]

        return bin2hex(pack_txn(txn_unpacked))

    def _sign_send_txn(self, raw_txn):
        signed_txn = self._paicoin_rpc('signrawtransaction', raw_txn)
        if not ('complete' in signed_txn and signed_txn['complete']):
            return {'error': 'Could not sign the transaction'}

        send_txid = self._paicoin_rpc('sendrawtransaction', signed_txn['hex'])
        if not (isinstance(send_txid, basestring) and len(send_txid) == 64):
            return {'error': 'Could not send the transaction'}

        return {'txid': str(send_txid)}

    def _list_mempool_txns(self):
        return self._paicoin_rpc('getrawmempool')

    def _get_mempool_txn(self, txid):
        raw_txn = self._paicoin_rpc('getrawtransaction', txid)
        return unpack_txn(hex2bin(raw_txn))

    def _get_mempool_txns(self):
        txids = self._list_mempool_txns()

        txns = {}
        for txid in txids:
            txns[txid] = self._get_mempool_txn(txid)

        return txns

    def _get_raw_block(self, height):
        block_hash = self._paicoin_rpc('getblockhash', height)

        if not (isinstance(block_hash, basestring) and len(block_hash) == 64):
            return {'error': 'Block at height {} not found'.format(str(height))}

        block = hex2bin(self._paicoin_rpc('getblock', block_hash, False))

        return {'block': block}

    def _get_block_txns(self, height):
        raw_block = self._get_raw_block(height)

        if 'error' in raw_block:
            return {'error': raw_block['error']}

        block = unpack_block(raw_block['block'])

        return block['txs']

    def _paicoin_check(self):
        info = self._paicoin_rpc('getwalletinfo')

        return isinstance(info, dict) and 'balance' in info

    def _paicoin_load_config(self):
        port = PAICOIN_PORT_MAINNET
        user = PAICOIN_USER
        password = PAICOIN_PASSWORD

        if not (len(user) and len(password)):
            conf_lines = open(
                expanduser('~') + PAICOIN_CONF_PATH).readlines()

            for conf_line in conf_lines:
                parts = conf_line.strip().split('=', 1)

                if (parts[0] == 'rpcport') and not len(port):
                    port = int(parts[1])
                if (parts[0] == 'rpcuser') and not len(user):
                    user = parts[1]
                if (parts[0] == 'rpcpassword') and not len(password):
                    password = parts[1]
                if parts[0] == 'testnet':
                    self._testnet = bool(parts[1])

        if self._testnet:
            port = PAICOIN_PORT_TESTNET

        if not (len(user) and len(password)):
            return None

        self._auth = user, password
        self._url = 'http://{}:{}/'.format(PAICOIN_IP,  str(port))

    def _paicoin_rpc(self, rpc, *args):
        id_ = str(time()) + '-' + str(randint(100000, 999999))

        request = {'id': id_, 'method': rpc, 'params': args}
        data = dumps(request).encode('utf-8')

        try:
            raw_res = post(url=self._url, auth=self._auth, data=data)
        except ConnectionError as err:
            raise self.Error(str(err.message).split('>')[1][:-4])

        result = raw_res.json()

        if raw_res.status_code != 200:
            raise self.Error(result['error']['message'])

        result = result['result']

        return result

    class Error(Exception):
        pass
