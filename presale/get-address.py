import ecdsa
import binascii
import hashlib
import base58
import datetime
import time
import sys

# Settings

mainnet = "mainnet"
testnet = "testnet"
regtest = "regtest"
publicAddress = "publicAddress"

base58prefix = {
	mainnet: {
		publicAddress: 56,
	},

	testnet: {
		publicAddress: 51,
	},

	regtest: {
		publicAddress: 51,
	} 
}

if (len(sys.argv) < 3):
  sys.exit('Usage: python get-address.py [network code] [public key]')

network_code = sys.argv[1]
pk_string = sys.argv[2]

pk_bytes = pk_string.decode('hex')
pk_string = binascii.hexlify(pk_bytes).decode('ascii').lower()

addressPrefix = base58prefix[network_code][publicAddress]

b = bytes()
b += bytes(chr(addressPrefix))
sha256 = hashlib.new("sha256")
sha256.update(pk_bytes)
p1 = sha256.digest()

ripemd160 = hashlib.new("ripemd160")
ripemd160.update(p1)
p2 = ripemd160.digest()

b += p2

sha256 = hashlib.new("sha256")
sha256.update(b)
c1 = sha256.digest()

sha256 = hashlib.new("sha256")
sha256.update(c1)
c2 = sha256.digest()

b += c2[0:4]

s = base58.b58encode(b)
print("Public address: " + s)
