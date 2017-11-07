import ecdsa
import binascii

sk = ecdsa.SigningKey.generate(curve=ecdsa.SECP256k1)
pk = sk.get_verifying_key()

sk_string = "Secret key: " + binascii.hexlify(sk.to_string()).decode('ascii').lower()
pk_string = "Public key: 04" + binascii.hexlify(pk.to_string()).decode('ascii').lower()

file = open("keypair.txt", "a")
file.write("\n")
file.write(sk_string);
file.write("\n")
file.write(pk_string);
file.write("\n")
file.close

print("The keypair was generated in the keypair.txt file")