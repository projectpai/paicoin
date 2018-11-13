<pre>PDP: 2
Layer: Applications
Title: PAI Data Storage and Sharing
Author: Alex Waters [alex@waters.nyc]
        Mark Harvilla [mark@oben.com]
        Patrick Gerzanics [patrick.gerzanics@upandrunningsoftware.com]
Status: Proposed
Type: Informational
Created: 2018-05-15
License: MIT
</pre>

# PAI Coin Development Proposal (PDP)
## Data Storage and Sharing
- [Abstract](#abstract)
- [Copyright](#copyright)
- [Motivation](#motivation)
- [Specification](#specification)
  * [Data Storage Flow Summary](#data-storage-flow-summary)
    + [Glossary](#glossary-)
    + [Flow](#flow-)
  * [OP_RETURN Protocol](#op-return-protocol)
    + [Glossary](#glossary--1)
    + [Protocol Definition](#protocol-definition)
    + [Operations Details](#operations-details-)
  * [Storage Provider API and Behavior](#storage-provider-api-and-behavior)
- [Compatibility](#--compatibility--)
- [Implementation](#--implementation--)


## Abstract

This document describes an initial specification for extending the standard PAI Coin protocol to include a method of submitting and provisioning access to arbitrary data.  This is achieved by utilizing a segmented network of torrent nodes, a custom `OP_RETURN` data protocol and elliptical encryption.

The specification consists of several parts. In the first part, the protocol for the `OP_RETURN` format for submitters and recipients is detailed. The second part discusses how the data is stored and distributed in v1 of the implementation.  The third part covers further expansion of ideas for indexing, storage and participant rewards.

## Copyright

This PDP is licensed under the MIT license.

## Motivation

Securely sharing data, such as on a public website, has been well-covered for some time using TLS/SSL.  However, what happens to that data once it has been shared and is at rest is undefined.  One of the classical uses of blockchain is to act as a distributed ledger.  We believe that this ledger functionality can be leveraged for tracking grants and revocations to allow end users full control over what happens to their data in transit and at rest.

## Specification

### Data Storage Flow Summary

#### Glossary

**Submitter** -- User that is providing data to a Recipient.

**Provider** -- Data storage provider as tracked in the PAI blockchain.

**Recipient** -- Entity that is to receive the data from the Submitter.

#### Flow

**Submitter and Recipient**: Monitor the PAI blockchain for data storage provider OP_RETURN messages to have an index of available storage providers; adding and removing as necessary.

**Submitter**: Encrypts required data using the intended recipient’s public key.

**Submitter**: Submits encrypted data to a PAI data storage provider using the Provider API and receives the unique data ID.

**Provider**: Stores data within their storage network.

**Submitter**: Creates a transaction on the blockchain according to the OP_RETURN protocol. This grants access to the data to the intended recipient as well as pays the storage provider, if necessary.

**Recipient**: Notified via PAI transaction with `OP_RETURN` and their wallet address that data has been submitted.  Retrieves ID from `OP_RETURN`.

**Recipient**: Calls Provider API `/get-torrent` to retrieve the torrent file for accessing data.

**Recipient**: Retrieves data using the torrent protocol.

**Recipient**: Monitors for ‘revoke’ operations.  When such is received, immediately destroys all copies of provided data and discontinues any usage.

### OP_RETURN Protocol

In order to achieve our goals with using the PAI blockchain to track provisioning, revocation and storage requests we need to embed this data into transactions.  To do so, we have chosen to use the standard approach of embedding data in the chain utilizing the op code `OP_RETURN`.

#### Glossary

**Header**:  The initial 8 bytes after the `OP_RETURN` code that indicate it is a PAI data storage txout and the version currently being used.

**Payload**: The operation and associated parameters for the request.

**Operations**: Types of requests that can be made via the PAI Coin data storage implementation.

**Storage Method**: This is the storage method being utilized.  In the initial implementation we have 0x01 which is the segmented bittorrent based solution as detailed below.

#### Protocol Definition

<table>
  <tr>
    <td>PAI PROTOCOL</td>
    <td>Purpose</td>
    <td>Length (bits)</td>
    <td>Value (hex)</td>
  </tr>
  <tr>
    <td>PAI HEADER</td>
    <td>PAI delimiter</td>
    <td>8</td>
    <td>0x92 = crc8("PAI")</td>
  </tr>
  <tr>
    <td></td>
    <td>Protocol Version</td>
    <td>8</td>
    <td>0x10 = 1.0</td>
  </tr>
  <tr>
    <td></td>
    <td>Reserved</td>
    <td>48</td>
    <td>0xFFFFFFFFFFFF</td>
  </tr>
  <tr>
    <td>PAI PAYLOAD</td>
    <td>Operation</td>
    <td>8</td>
    <td>0x00..0xFE + encoded NOP(0xFF)</td>
  </tr>
  <tr>
    <td></td>
    <td>Storage method</td>
    <td>8</td>
    <td>0x00..0xFE + encoded NST(0xFF)</td>
  </tr>
  <tr>
    <td></td>
    <td>First operand size</td>
    <td>8</td>
    <td>0x0..0x41 (max 65 bytes)</td>
  </tr>
  <tr>
    <td></td>
    <td>First operand</td>
    <td>256</td>
    <td></td>
  </tr>
  <tr>
    <td></td>
    <td>Second operand size</td>
    <td>8</td>
    <td>0x0..0x40 (max 64 bytes)</td>
  </tr>
  <tr>
    <td></td>
    <td>Second operand</td>
    <td>256</td>
    <td></td>
  </tr>
  <tr>
    <td>PAI CRC</td>
    <td>Checksum</td>
    <td>32</td>
    <td>crc32(header + payload)</td>
  </tr>
</table>


#### Protocol Operations and Operands

<table>
  <tr>
    <td>Operation ID</td>
    <td>Operation</td>
    <td>Operand 1</td>
    <td>Operand 2</td>
  </tr>
  <tr>
    <td>0x00</td>
    <td>grant</td>
    <td>data hash reference (sha-256 of encrypted data)</td>
    <td>txout index of wallet address</td>
  </tr>
  <tr>
    <td>0x01</td>
    <td>revoke</td>
    <td>data hash reference (sha-256 of encrypted data)</td>
    <td></td>
  </tr>
  <tr>
    <td>0x02</td>
    <td>add_provider</td>
    <td>ip address/hostname (max 32 bytes)</td>
    <td>port</td>
  </tr>
  <tr>
    <td>0x03</td>
    <td>remove_provider</td>
    <td>ip address/hostname (max 32 bytes)</td>
    <td>port</td>
  </tr>
</table>


#### Operations Details

**Grant** -- This is the method an end user follows to grant access to arbitrary data to a recipient.  The first operand is the hash reference returned by the data storage provider upon submission for later retrieval.  The second operand is the txout index of the intended recipient of that data within the transaction as a whole.  One additional note is that if the storage provider requires a payment for this operation that should be paid for with the Grant request as one of the txouts of the transaction.

**Revoke** -- This method is used to tell the original recipient of the encrypted data that all copies of the provided data should be destroyed and further usage is prohibited.

**Add Provider** -- This method is used with the initial PAI data storage method for submitting base URLs for submitting data to the PAI data storage system per the API definition defined below.

**Remove Provider** -- This method is used with the initial PAI data storage method to indicate that an existing entrypoint should no longer be used and has been retired.

### Storage Provider API and Behavior

In order to standardize interactions with PAI data storage providers, below are the initial API paths, parameters and responses.  This is only storage method 0x01 per the `OP_RETURN` protocol. 

<table>
  <tr>
    <td>Path</td>
    <td>Parameters</td>
    <td>Success Response</td>
    <td>Error Codes</td>
  </tr>
  <tr>
    <td>/info</td>
    <td>N/A</td>
    <td>{
"provider": "ACME Data Store",
"retention_period": "30d",
"cost":0,
"wallet": "[Payment Address]"
}</td>
    <td>N/A</td>
  </tr>
  <tr>
    <td>/make-torrent</td>
    <td>file: [encrypted data file for storage]
key: [public key used to confirm ownership for other operations]</td>
    <td>{
"result": "Success",
"id": "[Data Hash]",
"cost": 0
}</td>
    <td>405 -- Invalid params</td>
  </tr>
  <tr>
    <td>/get-torrent</td>
    <td>id: [hash of data previously submitted]</td>
    <td>{
"result": "Success",
"url": "[torrent URL]"
}</td>
    <td>404 -- Unknown data hash</td>
  </tr>
  <tr>
    <td>/remove</td>
    <td>id: [hash of data previously submitted]
signature: [signed hash proving ownership of original submission]</td>
    <td>{
"result": "Success",
}</td>
    <td>403 -- Invalid signature</td>
  </tr>
</table>


**Info** -- Used to retrieve high-level information about a given provider.  This can be used to determine information about an unknown provider that has been added via the `/add-entrypoint` operation.  Response data is currently limited to the provider name, retention period and cost (e.g., per MB) stored.

**Make Torrent** -- This is used to submit an encrypted data file to be stored on the provider along with a public key to use for later operations.  The data provider should create the torrent file, associate it with the public key, add it to its index and return the ID to be used by other operations.

**Get Torrent** -- This is used by the recipient of the data file to get the torrent file to be used for retrieval.

**Remove** -- This is used by the original submitter to remove the data from the storage provider at which time it is no longer required.  It requires a signed version of the file ID using the private side of the keypair submitted in the Make Torrent request.

In addition to these operations, the storage provider is free to use the ‘retention_period’ as returned in the ‘info’ request in order to cleanup old files.  Any files older than the retention period can be removed.  Likewise, if there is a cost for the data provider than the file can be removed after 24 hours if a transaction is not received to pay for storage for the length of the retention period to the ‘wallet’ specified by the ‘info’ endpoint.

## Compatibility

This PDP on its own does not cause any backwards incompatibility.

## Implementation

An initial implementation of this PDP is included in the PAI Coin core in the `contrib/data-store` directory.  This includes a Python API implementation of the Data Provider specification and Dockerfiles for launching torrent nodes using OpenTracker.  It also includes sample code for submitting `OP_RETURN` transactions and exercising the protocol for Submitters and Recipients.
