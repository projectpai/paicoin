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

In this example, 0.4 PAI Coin will be sent to the destination address `PdVX7Grppi76QyTXGXYk4v2N7aip7ToKyL` while the "change"
will be sent to `Ph6wqJEjn8fJheRWL2Mabw2of2ThJSgHRS`, which is an address the sender controls. *Note:* Specifying a proper
change address is critical when creating a raw transaction. Without the change address, the difference between the value of
the UTXO and the amount(s) sent to the recipient(s) will be forfeited as a mining fee. *When UTXOs are spent, they are always
consumed in full.* 

Given the txid and vout values above, together with the destination and change addresses, the `createrawtransaction` command
can be used as follows:
```
paicoin-cli createrawtransaction "[{\"txid\":\"228e0cb75f672f913af20223ab6c306b6f32bd323b6d93eeffee90fed541c480\",\"vout\":1}]" "{\"PdVX7Grppi76QyTXGXYk4v2N7aip7ToKyL\":0.4,\"Ph6wqJEjn8fJheRWL2Mabw2of2ThJSgHRS\":0.09999}"
```

Note that the sum of the outputs is 0.49999. The difference of 0.00001 is the mining fee, also known as a transaction fee. A non-zero
transaction fee is required to relay the transaction. More information on how to properly calculate transaction fees will be provided.

### Sign the raw transaction M times

In progress.

### Send the raw transaction

In progress.
