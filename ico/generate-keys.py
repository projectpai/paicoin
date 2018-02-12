import ecdsa
import binascii
import hashlib
import base58
import datetime
import time

# Settings

mainnet = "mainnet"
testnet = "testnet"
regtest = "regtest"
publicAddress = "publicAddress"
scriptAddress = "scriptAddress"
privateAddress = "privateAddress"

base58prefix = {
	mainnet: {
		publicAddress: 56,
		scriptAddress: 130,
		privateAddress: 247
	},

	testnet: {
		publicAddress: 51,
		scriptAddress: 180,
		privateAddress: 226
	},

	regtest: {
		publicAddress: 51,
		scriptAddress: 180,
		privateAddress: 226
	} 
}

# Data generation

d = datetime.datetime.now()
t = time.mktime(d.timetuple())

sk = ecdsa.SigningKey.generate(curve=ecdsa.SECP256k1)
pk = sk.get_verifying_key()

sk_bytes = sk.to_string()
pk_bytes = pk.to_string()

sk_string = binascii.hexlify(sk_bytes).decode('ascii').lower()
pk_string = "04" + binascii.hexlify(pk_bytes).decode('ascii').lower()

addresses = base58prefix

for network, addressList in base58prefix.items():
	for addressType, addressPrefix in addressList.items():
		if addressType == privateAddress:
			b = bytes()
			b += bytes(chr(addressPrefix))
			b += sk_bytes

			sha256 = hashlib.new("sha256")
			sha256.update(b)
			c1 = sha256.digest()

			sha256 = hashlib.new("sha256")
			sha256.update(c1)
			c2 = sha256.digest()

			b += c2[0:4]

			s = base58.b58encode(b)

			addresses[network][addressType] = s

		else:
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

			addresses[network][addressType] = s

# File output

file = open("keypair.txt", "a")
file.write("\n\n-------------------------------------------")
file.write("\nDate: " + str(d))
file.write("\nTimestamp: " + str(t))
file.write("\n\nSecret key: " + sk_string)
file.write("\nPublic key: " + pk_string)

for network, addressList in addresses.items():
	file.write("\n\n")
	file.write(network + ":")

	for addressType, address in addressList.items():
		file.write("\n" + addressType + ": " + address)

file.close

print("The keypair was generated in the keypair.txt file")