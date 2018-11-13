import argparse
import json
import subprocess
from binascii import hexlify


PAICOIN_CLI = '/home/ubuntu/paicoin/src/paicoin-cli'


def create_paichain_tx_with_data(data, address):
    unspent_cmd = ' '.join([PAICOIN_CLI, 'listunspent', '2', '99999999', "'[\"{}\"]'".format(address)])
    unspent = subprocess.check_output(unspent_cmd, shell=True)
    u_tx = json.loads(unspent)[0]
    txid = u_tx['txid']
    vout = u_tx['vout']
    amount = u_tx['amount']
    change_amount = amount - 0.0001
    hex_data = hexlify(data)
    raw_tx = subprocess.check_output([
        PAICOIN_CLI, 'createrawtransaction', '[{{"txid":"{}","vout":{}}}]'.format(txid, vout),
        '{{"{}":{},"data":"{}"}}'.format(address, change_amount, hex_data)
    ])
    signed_raw_tx = subprocess.check_output(
        [PAICOIN_CLI, 'signrawtransaction', '{}'.format(raw_tx.replace('\n', ''))]
    )
    signed_raw_tx_hex = json.loads(signed_raw_tx)['hex']
    # decoded_tx = subprocess.check_output([PAICOIN_CLI, 'decoderawtransaction', '{}'.format(signed_raw_tx_hex)])
    sent_tx = subprocess.check_output([PAICOIN_CLI, 'sendrawtransaction', '{}'.format(signed_raw_tx_hex)])
    print(sent_tx)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Create tx with data stored in OP_RETURN')
    parser.add_argument('-u', '--url', help='torrent URL', required=True)
    parser.add_argument('-a', '--address', help='PAIchain address', default='MpDscYD8xp4MnDLW7WPsEkKVAnAbkUz5t4')
    args = vars(parser.parse_args())
    data = args['url']
    address = args['address']
    create_paichain_tx_with_data(data, address)
