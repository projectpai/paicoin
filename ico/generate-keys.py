import ecdsa
import binascii

sk = ecdsa.SigningKey.generate(curve=ecdsa.SECP256k1)
pk = sk.get_verifying_key()

print("Secret key: " + binascii.hexlify(sk.to_string()).decode('ascii').upper())
print("Public key: " + binascii.hexlify(pk.to_string()).decode('ascii').upper())