Multisig with P2SH
==================

This document provides instructions on how to create M-of-N (M<=N) multisig P2SH addresses
for safe storage of PAI Coin.

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
This is done using `paicoin-cli` as follows.
