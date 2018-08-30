Multisig with P2SH
==================

This document provides instructions on how to create M-of-N (M<=N) multisig
pay-to-script hash (P2SH) addresses for safe storage of PAI Coin.

Keypair generation
------------------

The first step is to generate a set of N public/private key pairs, of which M will be 
required to spend funds held at the address. One commonly used configuration is with M=2 and N=3,
known as a 2-of-3 multisig address. Recommended values of M and N vary considerably with
the circumstances surrounding who should have access to funds, what the funds are for,
and most importantly, the amount to be stored. The information provided in this document makes
no recommendations with regard to the selection of the M and N parameters.

Typically, the N public/private key pairs would be generated on N separate devices and
held separately by N independent parties. So, the following setup and generation steps may be
repeated N times on N separate devices.

### Setup

This requires root/sudo access.

Run `./setup.sh`

### Generation

Run `python generate-keys.py <output_filename>`

The following usage is displayed with `python generate-keys.py -h`:

```
usage: generate-keys.py [-h] output_file_name

positional arguments:
  output_file_name  Name of the file in which you wish to save your keypair

optional arguments:
  -h, --help        show this help message and exit
```

The output key pair file has the following format (Note: This is just an example; DO NOT use this private key):
```
-------------------------------------------
Date: 2018-06-27 08:03:44.134679
Timestamp: 1530111824.0

Secret key: e3e3d5dc6ca80355e0705fc1c436d69e92ae5cc4a9761f3cf5af916c14051ffe
Public key: 048cebeb3f66ed8d7d60f9f05bfaa867cf0f4a3974213a72f80f149d52877a1d5d7be4bb7a3c6dc1c9330ad6d930cca058201e6ba90a7777a465f50a58d38c07e1

regtest:
publicAddress: MbEManBNa15b5yMuBZi9ReVfukzEQWSxAo
privateAddress: 8c3c1jwcjdZCiham35m7TjhAQiozNFTKWLhmj41ztMSNEF1kWAW
scriptAddress: 2FVqAbndYAGrcatM5LggFypcA6qDv8zExhr

testnet:
publicAddress: MbEManBNa15b5yMuBZi9ReVfukzEQWSxAo
privateAddress: 8c3c1jwcjdZCiham35m7TjhAQiozNFTKWLhmj41ztMSNEF1kWAW
scriptAddress: 2FVqAbndYAGrcatM5LggFypcA6qDv8zExhr

mainnet:
publicAddress: PbvNWKfp7uPyB94LJfNjrGrc4HGwwSp5rf
privateAddress: 9JsEicNHdZkxYvna5Fo6VUHRe7k5239i2mC2rCTpm7aWbFp33ea
scriptAddress: uNw1NMj8fFgohDNk7i2KjYzpdcLkZZt2wZ
```

Address generation
------------------

Given N key pairs, an M-of-N multisig address can be generated with only the N public keys previously generated.
This is done using `paicoin-cli` as follows. Note that `paicoind` must be running. Please ensure the PAI Coin
Daemon is pointed to the correct network (e.g., mainnet, testnet).

Usage of `./paicoin-cli createmultisig`:
```
createmultisig nrequired ["key",...]

Creates a multi-signature address with n signature of m keys required.
It returns a json object with the address and redeemScript.

Arguments:
1. nrequired      (numeric, required) The number of required signatures out of the n keys or addresses.
2. "keys"       (string, required) A json array of keys which are paicoin addresses or hex-encoded public keys
     [
       "key"    (string) paicoin address or hex-encoded public key
       ,...
     ]

Result:
{
  "address":"multisigaddress",  (string) The value of the new multisig address.
  "redeemScript":"script"       (string) The string value of the hex-encoded redemption script.
}
```

### Example

Create a 2-of-2 multisig address from 2 real mainnet public keys, using PAI Coin CLI (just an example, DO NOT USE the resulting address):
```
paicoin-cli createmultisig 2 "[\"048cebeb3f66ed8d7d60f9f05bfaa867cf0f4a3974213a72f80f149d52877a1d5d7be4bb7a3c6dc1c9330ad6d930cca058201e6ba90a7777a465f50a58d38c07e1\",\"04cfa9429bc27d41a425ebf077a26807f540a40d07ebb3d6db48032e08112a28533712cb90d139334bbd6879b8f9f81dbefe16b2d6337c644ae77cd988120cda47\"]"
```

Output:
```
{
  "address": "uNMTaHRJS1vM4FwqLtoURoDm6UPjNaWbzt",
  "redeemScript": "5241048cebeb3f66ed8d7d60f9f05bfaa867cf0f4a3974213a72f80f149d52877a1d5d7be4bb7a3c6dc1c9330ad6d930cca058201e6ba90a7777a465f50a58d38c07e14104cfa9429bc27d41a425ebf077a26807f540a40d07ebb3d6db48032e08112a28533712cb90d139334bbd6879b8f9f81dbefe16b2d6337c644ae77cd988120cda4752ae"
}
```

Alternatively, create a multisig address via JSON-RPC call (DO NOT USE the resulting address):
```
curl --user yourusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "createmultisig", "params": [2, ["048cebeb3f66ed8d7d60f9f05bfaa867cf0f4a3974213a72f80f149d52877a1d5d7be4bb7a3c6dc1c9330ad6d930cca058201e6ba90a7777a465f50a58d38c07e1","04cfa9429bc27d41a425ebf077a26807f540a40d07ebb3d6db48032e08112a28533712cb90d139334bbd6879b8f9f81dbefe16b2d6337c644ae77cd988120cda47"]] }' -H 'content-type: text/plain;' http://<hostip>:8566/
```

Response:
```
{"result":{"address":"uNMTaHRJS1vM4FwqLtoURoDm6UPjNaWbzt","redeemScript":"5241048cebeb3f66ed8d7d60f9f05bfaa867cf0f4a3974213a72f80f149d52877a1d5d7be4bb7a3c6dc1c9330ad6d930cca058201e6ba90a7777a465f50a58d38c07e14104cfa9429bc27d41a425ebf077a26807f540a40d07ebb3d6db48032e08112a28533712cb90d139334bbd6879b8f9f81dbefe16b2d6337c644ae77cd988120cda4752ae"},"error":null,"id":"curltest"}
```

Spending funds from a multisig address 
--------------------------------------

In order to spend funds from an M-of-N multisig address, the raw transaction needs to be signed by M of the 
private keys (the `privateAddress` in the key pair text file, under the appropriate network, e.g., mainnet, testnet).
The `redeemScript` is also required.

### Create the raw transaction

The first step to creating a raw transaction is obtaining the Transaction IDs (txid) and Unspent Transaction Output (UTXO) 
indices (vout) that you intend to spend. You will also need a destination address and a change address which you control,
if you do not wish to send the full value of the transaction(s) being spent. These instructions will use `uNMTaHRJS1vM4FwqLtoURoDm6UPjNaWbzt`
as an example 2-of-2 P2SH address containing a single UTXO to be partially spent. To start, import any transactions received
by this address as follows:

```
paicoin-cli importaddress "uNMTaHRJS1vM4FwqLtoURoDm6UPjNaWbzt"
```

The required information about the UTXOs can then be found with:
```
paicoin-cli listunspent
```

This yields the following example output:
```
[
  {
    "txid": "228e0cb75f672f913af20223ab6c306b6f32bd323b6d93eeffee90fed541c480",
    "vout": 1,
    "address": "uNMTaHRJS1vM4FwqLtoURoDm6UPjNaWbzt",
    "account": "",
    "scriptPubKey": "a914257481de4b6a4f5030cff147f7480c716d3597ff87",
    "amount": 0.50000000,
    "confirmations": 671,
    "spendable": false,
    "solvable": false,
    "safe": true
  }
] 
```

In this example, 0.4 PAI Coin will be sent to the destination address `Pm4ojBqHnAyKBxNDgWEcaXejE1xXS3r9iV` while the "change"
will be sent to `PmKgrKASftviR88QVzb9kpVFys75yv84mK`, which is an address the sender controls. *Note:* Specifying a proper
change address is critical when creating a raw transaction. Without the change address, the difference between the value of
the UTXO and the amount(s) sent to the recipient(s) will be forfeited as a mining fee. *When UTXOs are spent, they are always
consumed in full.* 

Given the txid and vout values above, together with the destination and change addresses, the `createrawtransaction` command
can be used as follows:
```
paicoin-cli createrawtransaction "[{\"txid\":\"228e0cb75f672f913af20223ab6c306b6f32bd323b6d93eeffee90fed541c480\",\"vout\":1}]" "{\"Pm4ojBqHnAyKBxNDgWEcaXejE1xXS3r9iV\":0.4,\"PmKgrKASftviR88QVzb9kpVFys75yv84mK\":0.09999}"
```

Note that the sum of the outputs is 0.49999. The difference of 0.00001 is the mining fee, also known as a transaction fee. A non-zero
transaction fee is required to relay the transaction. More information on how to properly calculate transaction fees will be provided. Executing the `createrawtransaction` command above produces the following hex string, which will be used in the signing step.

```
020000000180c441d5fe90eeffee936d3b32bd326f6b306cab2302f23a912f675fb70c8e220100000000ffffffff02005a6202000000001976a914901e49db87c87f8b522e86cad5b66781943ef72588ac98929800000000001976a91492eec985767887aa9e209191d2db18806465375b88ac00000000
```

### Sign the raw transaction M times

Given the hex string identifier previously outputted from the `createrawtransaction` command, the transaction needs to be signed M times before it is valid to be broadcast. As mentioned, a secure solution may require each of the M signing actions occur on M separate devices. In this case, secure movement of the intermediate hex strings between signers is recommended, and can be accomplished via a single-use medium such as optical disk, or paper which is shredded after use.

#### Sign transaction with first private key

The private key corresponding to the first public key used to create the multisig address from which funds are being spent signs the transaction as follows. The values of `scriptPubKey` and `redeemScript` can be obtained from the output of the `paicoin-cli listunspent` command and the output of the `paicoin-cli createmultisig` command, respectively. 

```
paicoin-cli signrawtransaction "020000000180c441d5fe90eeffee936d3b32bd326f6b306cab2302f23a912f675fb70c8e220100000000ffffffff02005a6202000000001976a914901e49db87c87f8b522e86cad5b66781943ef72588ac98929800000000001976a91492eec985767887aa9e209191d2db18806465375b88ac00000000" "[{\"txid\":\"228e0cb75f672f913af20223ab6c306b6f32bd323b6d93eeffee90fed541c480\",\"vout\":1,\"scriptPubKey\":\"a914257481de4b6a4f5030cff147f7480c716d3597ff87\",\"redeemScript\":\"5241048cebeb3f66ed8d7d60f9f05bfaa867cf0f4a3974213a72f80f149d52877a1d5d7be4bb7a3c6dc1c9330ad6d930cca058201e6ba90a7777a465f50a58d38c07e14104cfa9429bc27d41a425ebf077a26807f540a40d07ebb3d6db48032e08112a28533712cb90d139334bbd6879b8f9f81dbefe16b2d6337c644ae77cd988120cda4752ae\"}]" "[\"9JsEicNHdZkxYvna5Fo6VUHRe7k5239i2mC2rCTpm7aWbFp33ea\"]"
```

The final value in brackets is the first private key and is obtained from the `mainnet.privateAddress` field in the output key pair file of `generate-keys.py`.

Execution of this command produces a new hex string identifier, which is then used as input to the second signing step. Note that the `"complete": false` and `"error": "Operation not valid with the current stack size"` messages appear because the transaction has, as yet, been signed an insufficient number of times to be valid.

```
{
  "hex": "020000000180c441d5fe90eeffee936d3b32bd326f6b306cab2302f23a912f675fb70c8e2201000000d30048304502210094395ae0a1d5bac4d981b3b2dfd114db877d819a8eb7ef164994869251235864022073d4dc8d59846a75b9d8980a96e088d9eb054c14b29f2deb6d5ae9babe20695a014c875241048cebeb3f66ed8d7d60f9f05bfaa867cf0f4a3974213a72f80f149d52877a1d5d7be4bb7a3c6dc1c9330ad6d930cca058201e6ba90a7777a465f50a58d38c07e14104cfa9429bc27d41a425ebf077a26807f540a40d07ebb3d6db48032e08112a28533712cb90d139334bbd6879b8f9f81dbefe16b2d6337c644ae77cd988120cda4752aeffffffff02005a6202000000001976a914901e49db87c87f8b522e86cad5b66781943ef72588ac98929800000000001976a91492eec985767887aa9e209191d2db18806465375b88ac00000000",
  "complete": false,
  "errors": [
    {
      "txid": "228e0cb75f672f913af20223ab6c306b6f32bd323b6d93eeffee90fed541c480",
      "vout": 1,
      "witness": [
      ],
      "scriptSig": "0048304502210094395ae0a1d5bac4d981b3b2dfd114db877d819a8eb7ef164994869251235864022073d4dc8d59846a75b9d8980a96e088d9eb054c14b29f2deb6d5ae9babe20695a014c875241048cebeb3f66ed8d7d60f9f05bfaa867cf0f4a3974213a72f80f149d52877a1d5d7be4bb7a3c6dc1c9330ad6d930cca058201e6ba90a7777a465f50a58d38c07e14104cfa9429bc27d41a425ebf077a26807f540a40d07ebb3d6db48032e08112a28533712cb90d139334bbd6879b8f9f81dbefe16b2d6337c644ae77cd988120cda4752ae",
      "sequence": 4294967295,
      "error": "Operation not valid with the current stack size"
    }
  ]
}
```

#### Sign transaction with second private key

The private key corresponding to the second public key used to create the multisig address from which funds are being spent signs the transaction in the same way as the first, but this time using the (longer) hex string identifier outputted from the first signing step. The complete command, with private key redacted, and the corresponding output, is shown below.

```
paicoin-cli signrawtransaction "020000000180c441d5fe90eeffee936d3b32bd326f6b306cab2302f23a912f675fb70c8e2201000000d30048304502210094395ae0a1d5bac4d981b3b2dfd114db877d819a8eb7ef164994869251235864022073d4dc8d59846a75b9d8980a96e088d9eb054c14b29f2deb6d5ae9babe20695a014c875241048cebeb3f66ed8d7d60f9f05bfaa867cf0f4a3974213a72f80f149d52877a1d5d7be4bb7a3c6dc1c9330ad6d930cca058201e6ba90a7777a465f50a58d38c07e14104cfa9429bc27d41a425ebf077a26807f540a40d07ebb3d6db48032e08112a28533712cb90d139334bbd6879b8f9f81dbefe16b2d6337c644ae77cd988120cda4752aeffffffff02005a6202000000001976a914901e49db87c87f8b522e86cad5b66781943ef72588ac98929800000000001976a91492eec985767887aa9e209191d2db18806465375b88ac00000000" "[{\"txid\":\"228e0cb75f672f913af20223ab6c306b6f32bd323b6d93eeffee90fed541c480\",\"vout\":1,\"scriptPubKey\":\"a914257481de4b6a4f5030cff147f7480c716d3597ff87\",\"redeemScript\":\"5241048cebeb3f66ed8d7d60f9f05bfaa867cf0f4a3974213a72f80f149d52877a1d5d7be4bb7a3c6dc1c9330ad6d930cca058201e6ba90a7777a465f50a58d38c07e14104cfa9429bc27d41a425ebf077a26807f540a40d07ebb3d6db48032e08112a28533712cb90d139334bbd6879b8f9f81dbefe16b2d6337c644ae77cd988120cda4752ae\"}]" "[\"9K3UYBs5uqpfqHVUxQVLaFsPLwuh8aXZn2iaujStArSwSv81iMo\"]"
{
  "hex": "020000000180c441d5fe90eeffee936d3b32bd326f6b306cab2302f23a912f675fb70c8e2201000000fd1c010048304502210094395ae0a1d5bac4d981b3b2dfd114db877d819a8eb7ef164994869251235864022073d4dc8d59846a75b9d8980a96e088d9eb054c14b29f2deb6d5ae9babe20695a01483045022100e7aa8115856e55d3feba9d61156477a740d57fa3ca55f1b8173313315c5ba15e02201c88d20e403921b575217eddcd2e814fe808a92ef839686cb8542156ca9aaf9e014c875241048cebeb3f66ed8d7d60f9f05bfaa867cf0f4a3974213a72f80f149d52877a1d5d7be4bb7a3c6dc1c9330ad6d930cca058201e6ba90a7777a465f50a58d38c07e14104cfa9429bc27d41a425ebf077a26807f540a40d07ebb3d6db48032e08112a28533712cb90d139334bbd6879b8f9f81dbefe16b2d6337c644ae77cd988120cda4752aeffffffff02005a6202000000001976a914901e49db87c87f8b522e86cad5b66781943ef72588ac98929800000000001976a91492eec985767887aa9e209191d2db18806465375b88ac00000000",
  "complete": true
}
```

The `"complete": true` output indicates the transaction is ready to be publicly broadcast to the network.

### Send the raw transaction

The hex string output of the final `signrawtransaction` command is used with `sendrawtransaction` to broadcast the transaction to the mempool for permanent inclusion in a block.

```
./paicoin-cli sendrawtransaction "020000000180c441d5fe90eeffee936d3b32bd326f6b306cab2302f23a912f675fb70c8e2201000000fd1c010048304502210094395ae0a1d5bac4d981b3b2dfd114db877d819a8eb7ef164994869251235864022073d4dc8d59846a75b9d8980a96e088d9eb054c14b29f2deb6d5ae9babe20695a01483045022100e7aa8115856e55d3feba9d61156477a740d57fa3ca55f1b8173313315c5ba15e02201c88d20e403921b575217eddcd2e814fe808a92ef839686cb8542156ca9aaf9e014c875241048cebeb3f66ed8d7d60f9f05bfaa867cf0f4a3974213a72f80f149d52877a1d5d7be4bb7a3c6dc1c9330ad6d930cca058201e6ba90a7777a465f50a58d38c07e14104cfa9429bc27d41a425ebf077a26807f540a40d07ebb3d6db48032e08112a28533712cb90d139334bbd6879b8f9f81dbefe16b2d6337c644ae77cd988120cda4752aeffffffff02005a6202000000001976a914901e49db87c87f8b522e86cad5b66781943ef72588ac98929800000000001976a91492eec985767887aa9e209191d2db18806465375b88ac00000000"
```

Upon success, this command produces the Transaction ID, or `txid`, as output.

```
ac27910180b6c5a324741ccf3e39eba8c993e5ecd12f36e0eedd683af0a49b16
```

### View the transaction on the PAI Blockchain

If `paicoind` is run using the `-reindex -txindex` flags to maintain a full index of transactions, the CLI can be used to retrieve information about the transaction using its `txid` as follows, complete with its corresponding output.

```
paicoin-cli getrawtransaction ac27910180b6c5a324741ccf3e39eba8c993e5ecd12f36e0eedd683af0a49b16 1
{
  "txid": "ac27910180b6c5a324741ccf3e39eba8c993e5ecd12f36e0eedd683af0a49b16",
  "hash": "ac27910180b6c5a324741ccf3e39eba8c993e5ecd12f36e0eedd683af0a49b16",
  "version": 2,
  "size": 405,
  "vsize": 405,
  "locktime": 0,
  "vin": [
    {
      "txid": "228e0cb75f672f913af20223ab6c306b6f32bd323b6d93eeffee90fed541c480",
      "vout": 1,
      "scriptSig": {
        "asm": "0 304502210094395ae0a1d5bac4d981b3b2dfd114db877d819a8eb7ef164994869251235864022073d4dc8d59846a75b9d8980a96e088d9eb054c14b29f2deb6d5ae9babe20695a[ALL] 3045022100e7aa8115856e55d3feba9d61156477a740d57fa3ca55f1b8173313315c5ba15e02201c88d20e403921b575217eddcd2e814fe808a92ef839686cb8542156ca9aaf9e[ALL] 5241048cebeb3f66ed8d7d60f9f05bfaa867cf0f4a3974213a72f80f149d52877a1d5d7be4bb7a3c6dc1c9330ad6d930cca058201e6ba90a7777a465f50a58d38c07e14104cfa9429bc27d41a425ebf077a26807f540a40d07ebb3d6db48032e08112a28533712cb90d139334bbd6879b8f9f81dbefe16b2d6337c644ae77cd988120cda4752ae",
        "hex": "0048304502210094395ae0a1d5bac4d981b3b2dfd114db877d819a8eb7ef164994869251235864022073d4dc8d59846a75b9d8980a96e088d9eb054c14b29f2deb6d5ae9babe20695a01483045022100e7aa8115856e55d3feba9d61156477a740d57fa3ca55f1b8173313315c5ba15e02201c88d20e403921b575217eddcd2e814fe808a92ef839686cb8542156ca9aaf9e014c875241048cebeb3f66ed8d7d60f9f05bfaa867cf0f4a3974213a72f80f149d52877a1d5d7be4bb7a3c6dc1c9330ad6d930cca058201e6ba90a7777a465f50a58d38c07e14104cfa9429bc27d41a425ebf077a26807f540a40d07ebb3d6db48032e08112a28533712cb90d139334bbd6879b8f9f81dbefe16b2d6337c644ae77cd988120cda4752ae"
      },
      "sequence": 4294967295
    }
  ],
  "vout": [
    {
      "value": 0.40000000,
      "n": 0,
      "scriptPubKey": {
        "asm": "OP_DUP OP_HASH160 901e49db87c87f8b522e86cad5b66781943ef725 OP_EQUALVERIFY OP_CHECKSIG",
        "hex": "76a914901e49db87c87f8b522e86cad5b66781943ef72588ac",
        "reqSigs": 1,
        "type": "pubkeyhash",
        "addresses": [
          "Pm4ojBqHnAyKBxNDgWEcaXejE1xXS3r9iV"
        ]
      }
    }, 
    {
      "value": 0.09999000,
      "n": 1,
      "scriptPubKey": {
        "asm": "OP_DUP OP_HASH160 92eec985767887aa9e209191d2db18806465375b OP_EQUALVERIFY OP_CHECKSIG",
        "hex": "76a91492eec985767887aa9e209191d2db18806465375b88ac",
        "reqSigs": 1,
        "type": "pubkeyhash",
        "addresses": [
          "PmKgrKASftviR88QVzb9kpVFys75yv84mK"
        ]
      }
    }
  ],
  "hex": "020000000180c441d5fe90eeffee936d3b32bd326f6b306cab2302f23a912f675fb70c8e2201000000fd1c010048304502210094395ae0a1d5bac4d981b3b2dfd114db877d819a8eb7ef164994869251235864022073d4dc8d59846a75b9d8980a96e088d9eb054c14b29f2deb6d5ae9babe20695a01483045022100e7aa8115856e55d3feba9d61156477a740d57fa3ca55f1b8173313315c5ba15e02201c88d20e403921b575217eddcd2e814fe808a92ef839686cb8542156ca9aaf9e014c875241048cebeb3f66ed8d7d60f9f05bfaa867cf0f4a3974213a72f80f149d52877a1d5d7be4bb7a3c6dc1c9330ad6d930cca058201e6ba90a7777a465f50a58d38c07e14104cfa9429bc27d41a425ebf077a26807f540a40d07ebb3d6db48032e08112a28533712cb90d139334bbd6879b8f9f81dbefe16b2d6337c644ae77cd988120cda4752aeffffffff02005a6202000000001976a914901e49db87c87f8b522e86cad5b66781943ef72588ac98929800000000001976a91492eec985767887aa9e209191d2db18806465375b88ac00000000",
  "blockhash": "000000000000004bb3a583176d4c699205119eda8195b6cb3ddf9a8b01a49897",
  "confirmations": 1,
  "time": 1535669109,
  "blocktime": 1535669109
}
```

Alternatively, an online block explorer, like https://paichain.info, can be used as well. See the transaction [here](https://paichain.info/ui/tx/ac27910180b6c5a324741ccf3e39eba8c993e5ecd12f36e0eedd683af0a49b16).
