# Key Uncompress #

Utilities to convert (uncompress) compressed public and private keys to their corresponding uncompressed forms.

## Public Key Uncompress ##

Credit and thanks to: https://bitcointalk.org/index.php?topic=644919.msg7205689#msg7205689

Converts a compressed public key to its corresponding uncompressed form.

* Usage:

    $ ./pubkey-uncompress.py "compressed-public-key"

* Example:

    $ ./pubkey-uncompress.py 0361c58f2edb9a6d9a0ca1d97752ceaf9cb29beecfa3ee72d530507f9ad751fbe4
      0461c58f2edb9a6d9a0ca1d97752ceaf9cb29beecfa3ee72d530507f9ad751fbe4ceb77affc9a2e9303f1ee2ecfbe81a7c157cfafc037411c7f91ae2d2ca085b5b

## Private Key Uncompress ##

Credit and thanks to: https://www.reddit.com/r/Bitcoin/comments/7fptly/how_to_convert_private_key_wif_compressed_start/

Converts a compressed private key to its corresponding uncompressed form.

* Usage:

    $ ./privkey-uncompress.py "wif-key"

* Example:

    $ ./privkey-uncompress.py KxFC1jmwwCoACiCAWZ3eXa96mBM6tb3TYzGmf6YwgdGWZgawvrtJ
      5J3mBbAH58CpQ3Y5RNJpUKPE62SQ5tfcvU2JpbnkeyhfsYB1Jcn
