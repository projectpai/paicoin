#!/usr/bin/env python

# Credit and thanks to: https://www.reddit.com/r/Bitcoin/comments/7fptly/how_to_convert_private_key_wif_compressed_start/

import hashlib
import sys
import base58

wif_key_compressed = sys.argv[1]
compressed_key = base58.b58decode(wif_key_compressed).encode('hex')
key = compressed_key[0 : 66]
first_hash = hashlib.sha256(key.decode('hex')).hexdigest()
second_hash = hashlib.sha256(first_hash.decode('hex')).hexdigest()
wif_key_uncompressed = base58.b58encode((key + second_hash[0 : 8].upper()).decode('hex'))
print(wif_key_uncompressed)
