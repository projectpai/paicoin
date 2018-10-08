#!/usr/bin/env python

# Credit and thanks to: https://bitcointalk.org/index.php?topic=644919.msg7205689#msg7205689

import sys

def pow_mod(x, y, z):
    "Calculate (x ** y) % z efficiently."
    number = 1
    while y:
        if y & 1:
            number = number * x % z
        y >>= 1
        x = x * x % z
    return number

p = 0xfffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f
compressed_key = sys.argv[1]
y_parity = int(compressed_key[:2]) - 2
x = int(compressed_key[2:], 16)
a = (pow_mod(x, 3, p) + 7) % p
y = pow_mod(a, (p+1)//4, p)
if y % 2 != y_parity:
    y = -y % p
uncompressed_key = '04{:x}{:x}'.format(x, y)
print(uncompressed_key)
