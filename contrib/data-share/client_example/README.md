# Python module for PAI Coin OP_RETURN transactions

Simple Python scripts and API for using OP_RETURN in PAI Coin transactions.

Copyright (c) ObEN, Inc. - https://oben.me/ <br>
Copyright (c) Coin Sciences Ltd - http://coinsecrets.org/

MIT License


## REQUIREMENTS

* Python 2.5 or later (including Python 3)
* PAI Coin Core 0.9 or later
* requests (2.18.4)

## INSTALLATION
```bash
git clone git@gitlab.int.oben.me:pai/paicointxn.git
cd paicointxn
pip install -r requirements.txt
pip install .
```

## BEFORE YOU START

If you just installed PAI Coin Core, wait for it to download and verify old blocks.<br>
If using as a module, add `from paicointxn import Transaction` in your Python script file.


## SEND PAI Coin TRANSACTION WITH OP_RETURN METADATA

### Command line:

```bash
python send_paicoin.py <address> <amount> <metadata> <testnet (optional)>
Where:
  <address>  - PAI Coin address of the recipient
  <amount>   - amount to send (in units of PAI Coin)
  <metadata> - hex string or raw string containing the OP_RETURN metadata (auto-detection: treated as a hex string if it is a valid one)
  <testnet>  - PAI Coin testnet network flag (False if omitted)
```

* Outputs an error if one occurred or the txid if sending was successful


#### Examples:
```bash
python send_paicoin.py 149wHUMa41Xm2jnZtqgRx94uGbZD9kPXnS 0.001 "Hello, myPAI!"
python send_paicoin.py 149wHUMa41Xm2jnZtqgRx94uGbZD9kPXnS 0.001 48656c6c6f2c206d7950414921
python send_paicoin.py mzEJxCrdva57shpv62udriBBgMECmaPce4 0.001 "Hello, PAI Coin testnet!" 1
```

### API:

**Transaction.paicoin_send**(*address, amount, metadata, testnet=False*)<br>
`address` - PAI Coin address of the recipient<br>
`amount` - amount to send (in units of PAI Coin)<br>
`metadata` - string of raw bytes containing OP_RETURN metadata<br>
`testnet` - PAI Coin testnet network flag (False if omitted)<br>

**Return:**<br>
{'`PAIcoinTransactionError`': 'error string'}<br>
or:<br>
{'txid': 'sent txid'}

#### Examples:

```python
from paicointxn import Transaction

Transaction.paicoin_send('149wHUMa41Xm2jnZtqgRx94uGbZD9kPXnS', 0.001, 'Hello, myPAI!')
Transaction.paicoin_send('mzEJxCrdva57shpv62udriBBgMECmaPce4', 0.001, 'Hello, PAI Coin testnet!', testnet=True)

# Equivalent with:

paicoin_txn = Transaction(testnet=True)
res = paicoin_txn.send('mzEJxCrdva57shpv62udriBBgMECmaPce4', 0.001, 'Hello, PAI Coin testnet!')
```

## STORE DATA IN THE BLOCKCHAIN USING OP_RETURN

### Command line:
```bash
python store_paicoin.py <data> <testnet (optional)>
Where:
  <data>    - hex string or raw string containing the data to be stored (auto-detection: treated as a hex string if it is a valid one)
  <testnet> - PAI Coin testnet network flag (False if omitted)

```
* Outputs an error if one occurred or if successful, the txids that were used to store
  the data and a short reference that can be used to retrieve it using this library.

#### Examples:
```bash
python store_paicoin.py "This example stores 47 bytes in the blockchain."
python store_paicoin.py "This example stores 44 bytes in the testnet." 1
```
  
### API:

**Transaction.paicoin_store**(*data, testnet=False*)<br>
`data` -  string of raw bytes containing OP_RETURN metadata<br>
`testnet` - PAI Coin testnet network flag (False if omitted)<br>
  
**Return:**<br>
{'`PAIcoinTransactionError`': 'error string'}<br>
or:<br>
{'txid': 'sent txid', 'ref': 'ref for retrieving data' }
           
#### Examples:

```python
from paicointxn import Transaction

Transaction.paicoin_store('This example stores 47 bytes in the blockchain.')
Transaction.paicoin_store('This example stores 44 bytes in the testnet.', testnet=True)

# Equivalent with:

paicoin_txn = Transaction(testnet=True)
res = paicoin_txn.store('This example stores 44 bytes in the testnet.')
```

## RETRIEVE DATA FROM OP_RETURN IN THE BLOCKCHAIN

### Command line:

```bash
python retrieve_paicoin.py <ref> <testnet (optional)>
Where:
  <ref>     - reference that was returned by a previous storage operation
  <testnet> - PAI Coin testnet network flag (False if omitted)
```

* Outputs an error if one occurred or if successful, the retrieved data in hexadecimal
  and ASCII format, a list of the txids used to store the data, a list of the blocks in
  which the data is stored, and (if available) the best ref for retrieving the data
  quickly in future. This may or may not be different from the ref you provided.
  
#### Examples:

```bash
python retrieve_paicoin.py 356115-052075
python retrieve_paicoin.py 396381-059737 1
```
  
### API:

**Transaction.paicoin_retrieve**(*ref, max_results=1, testnet=False*)<br>
`ref` - reference that was returned by a previous storage operation<br>
`max_results` - maximum number of results to retrieve (omit for 1)<br>
`testnet` - PAI Coin testnet network flag (False if omitted)<br>

**Returns**:<br>
{'`PAIcoinTransactionError`': 'error string'}<br>
or:<br>
{'data': 'raw binary data',<br>
 'txids': ['1st txid', '2nd txid', ...],<br>
 'heights': [block 1 used, block 2 used, ...],<br>
 'ref': 'best ref for retrieving data',<br>
 'error': 'error if data only partially retrieved'}

**NOTE:**<br>
A value of 0 in the 'heights' array means that data is still in the mempool.<br>
The 'ref' and 'error' elements are only present if appropriate.
                 
#### Examples:
```python
from paicointxn import Transaction

Transaction.paicoin_retrieve('356115-052075')
Transaction.paicoin_retrieve('396381-059737', 1, True)

# Equivalent with:

paicoin_txn = Transaction(testnet=True)
res = paicoin_txn.retrieve('396381-059737', max_results=1)
```

##### VERSION HISTORY

v2.2.0 - 5 February 2018
