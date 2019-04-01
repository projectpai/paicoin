### paicoin-cli and paicoind command line options and help

##### You may need to use sudo to execute these help commands as shown
##### (eg, 'sudo paicoin-cli --help')

# paicoin-cli --help
```
PAI Coin Core RPC client 

Usage:
  paicoin-cli [options] <command> [params]  Send command to PAI Coin Core
  paicoin-cli [options] help                List commands
  paicoin-cli [options] help <command>      Get help for a command

Options:

  -?
       This help message

  -conf=<file>
       Specify configuration file (default: paicoin.conf)

  -datadir=<dir>
       Specify data directory

  -testnet
       Use the test network

  -regtest
       Enter regression test mode, which uses a special chain in which blocks
       can be solved instantly. This is intended for regression testing tools
       and app development.

  -rpcconnect=<ip>
       Send commands to node running on <ip> (default: 127.0.0.1)

  -rpcport=<port>
       Connect to JSON-RPC on <port> (default: 8332 or testnet: 18332)

  -rpcwait
       Wait for RPC server to start

  -rpcuser=<user>
       Username for JSON-RPC connections

  -rpcpassword=<pw>
       Password for JSON-RPC connections

SSL options: (see the PAI Coin Wiki for SSL setup instructions)

  -rpcssl
       Use OpenSSL (https) for JSON-RPC connections
```

# paicoin-cli help
```
== Blockchain ==
getbestblockhash
getblock "hash" ( verbose )
getblockchaininfo
getblockcount
getblockhash index
getchaintips
getdifficulty
getmempoolinfo
getrawmempool ( verbose )
gettxout "txid" n ( includemempool )
gettxoutproof ["txid",...] ( blockhash )
gettxoutsetinfo
verifychain ( checklevel numblocks )
verifytxoutproof "proof"

== Control ==
getinfo
help ( "command" )
stop

== Generating ==
generate numblocks
getgenerate
setgenerate generate ( genproclimit )

== Mining ==
getblocktemplate ( "jsonrequestobject" )
getmininginfo
getnetworkhashps ( blocks height )
prioritisetransaction <txid> <priority delta> <fee delta>
submitblock "hexdata" ( "jsonparametersobject" )

== Network ==
addnode "node" "add|remove|onetry"
getaddednodeinfo dns ( "node" )
getconnectioncount
getnettotals
getnetworkinfo
getpeerinfo
ping

== Rawtransactions ==
createrawtransaction [{"txid":"id","vout":n},...] {"address":amount,...}
decoderawtransaction "hexstring"
decodescript "hex"
getrawtransaction "txid" ( verbose )
sendrawtransaction "hexstring" ( allowhighfees )
signrawtransaction "hexstring" ( [{"txid":"id","vout":n,"scriptPubKey":"hex","redeemScript":"hex"},...] ["privatekey1",...] sighashtype )

== Util ==
createmultisig nrequired ["key",...]
estimatefee nblocks
estimatepriority nblocks
validateaddress "paicoinaddress"
verifymessage "paicoinaddress" "signature" "message"

== Wallet ==
addmultisigaddress nrequired ["key",...] ( "account" )
backupwallet "destination"
dumpprivkey "paicoinaddress"
dumpwallet "filename"
encryptwallet "passphrase"
getaccount "paicoinaddress"
getaccountaddress "account"
getaddressesbyaccount "account"
getbalance ( "account" minconf includeWatchonly )
getnewaddress ( "account" )
getrawchangeaddress
getreceivedbyaccount "account" ( minconf )
getreceivedbyaddress "paicoinaddress" ( minconf )
gettransaction "txid" ( includeWatchonly )
getunconfirmedbalance
getwalletinfo
importaddress "address" ( "label" rescan )
importprivkey "paicoinprivkey" ( "label" rescan )
importwallet "filename"
keypoolrefill ( newsize )
listaccounts ( minconf includeWatchonly)
listaddressgroupings
listlockunspent
listreceivedbyaccount ( minconf includeempty includeWatchonly)
listreceivedbyaddress ( minconf includeempty includeWatchonly)
listsinceblock ( "blockhash" target-confirmations includeWatchonly)
listtransactions ( "account" count from includeWatchonly)
listunspent ( minconf maxconf  ["address",...] )
lockunspent unlock [{"txid":"txid","vout":n},...]
move "fromaccount" "toaccount" amount ( minconf "comment" )
sendfrom "fromaccount" "topaicoinaddress" amount ( minconf "comment" "comment-to" )
sendmany "fromaccount" {"address":amount,...} ( minconf "comment" ["address",...] )
sendtoaddress "paicoinaddress" amount ( "comment" "comment-to" subtractfeefromamount )
setaccount "paicoinaddress" "account"
settxfee amount
signmessage "paicoinaddress" "message"
```

# paicoind --help -help-debug
```
PAI Coin Core Daemon version v0.11.0

Usage:
  paicoind [options]                     Start PAI Coin Core Daemon

Options:

  -?
       This help message

  -alerts
       Receive and display P2P network alerts (default: 1)

  -alertnotify=<cmd>
       Execute command when a relevant alert is received or we see a really
       long fork (%s in cmd is replaced by message)

  -blocknotify=<cmd>
       Execute command when the best block changes (%s in cmd is replaced by
       block hash)

  -checkblocks=<n>
       How many blocks to check at startup (default: 288, 0 = all)

  -checklevel=<n>
       How thorough the block verification of -checkblocks is (0-4, default: 3)

  -conf=<file>
       Specify configuration file (default: paicoin.conf)

  -daemon
       Run in the background as a daemon and accept commands

  -datadir=<dir>
       Specify data directory

  -dbcache=<n>
       Set database cache size in megabytes (4 to 1024, default: 100)

  -loadblock=<file>
       Imports blocks from external blk000??.dat file on startup

  -maxorphantx=<n>
       Keep at most <n> unconnectable transactions in memory (default: 100)

  -par=<n>
       Set the number of script verification threads (-2 to 16, 0 = auto, <0 =
       leave that many cores free, default: 0)

  -pid=<file>
       Specify pid file (default: paicoind.pid)

  -prune=<n>
       Reduce storage requirements by pruning (deleting) old blocks. This mode
       disables wallet support and is incompatible with -txindex. Warning:
       Reverting this setting requires re-downloading the entire blockchain.
       (default: 0 = disable pruning blocks, >550 = target size in MiB to use
       for block files)

  -reindex
       Rebuild block chain index from current blk000??.dat files on startup

  -sysperms
       Create new files with system default permissions, instead of umask 077
       (only effective with disabled wallet functionality)

  -txindex
       Maintain a full transaction index, used by the getrawtransaction rpc
       call (default: 0)

Connection options:

  -addnode=<ip>
       Add a node to connect to and attempt to keep the connection open

  -banscore=<n>
       Threshold for disconnecting misbehaving peers (default: 100)

  -bantime=<n>
       Number of seconds to keep misbehaving peers from reconnecting (default:
       86400)

  -bind=<addr>
       Bind to given address and always listen on it. Use [host]:port notation
       for IPv6

  -connect=<ip>
       Connect only to the specified node(s)

  -discover
       Discover own IP addresses (default: 1 when listening and no -externalip
       or -proxy)

  -dns
       Allow DNS lookups for -addnode, -seednode and -connect (default: 1)

  -dnsseed
       Query for peer addresses via DNS lookup, if low on addresses (default: 1
       unless -connect)

  -externalip=<ip>
       Specify your own public address

  -forcednsseed
       Always query for peer addresses via DNS lookup (default: 0)

  -listen
       Accept connections from outside (default: 1 if no -proxy or -connect)

  -maxconnections=<n>
       Maintain at most <n> connections to peers (default: 125)

  -maxreceivebuffer=<n>
       Maximum per-connection receive buffer, <n>*1000 bytes (default: 5000)

  -maxsendbuffer=<n>
       Maximum per-connection send buffer, <n>*1000 bytes (default: 1000)

  -onion=<ip:port>
       Use separate SOCKS5 proxy to reach peers via Tor hidden services
       (default: -proxy)

  -onlynet=<net>
       Only connect to nodes in network <net> (ipv4, ipv6 or onion)

  -permitbaremultisig
       Relay non-P2SH multisig (default: 1)

  -port=<port>
       Listen for connections on <port> (default: 8333 or testnet: 18333)

  -proxy=<ip:port>
       Connect through SOCKS5 proxy

  -proxyrandomize
       Randomize credentials for every proxy connection. This enables Tor
       stream isolation (default: 1)

  -seednode=<ip>
       Connect to a node to retrieve peer addresses, and disconnect

  -timeout=<n>
       Specify connection timeout in milliseconds (minimum: 1, default: 5000)

  -upnp
       Use UPnP to map the listening port (default: 1 when listening)

  -whitebind=<addr>
       Bind to given address and whitelist peers connecting to it. Use
       [host]:port notation for IPv6

  -whitelist=<netmask>
       Whitelist peers connecting from the given netmask or IP address. Can be
       specified multiple times. Whitelisted peers cannot be DoS banned and
       their transactions are always relayed, even if they are already in the
       mempool, useful e.g. for a gateway

Wallet options:

  -disablewallet
       Do not load the wallet and disable wallet RPC calls

  -keypool=<n>
       Set key pool size to <n> (default: 100)

  -mintxfee=<amt>
       Fees (in PAI/Kb) smaller than this are considered zero fee for
       transaction creation (default: 0.00001)

  -paytxfee=<amt>
       Fee (in PAI/kB) to add to transactions you send (default: 0.00)

  -rescan
       Rescan the block chain for missing wallet transactions on startup

  -salvagewallet
       Attempt to recover private keys from a corrupt wallet.dat on startup

  -sendfreetransactions
       Send transactions as zero-fee transactions if possible (default: 0)

  -spendzeroconfchange
       Spend unconfirmed change when sending transactions (default: 1)

  -txconfirmtarget=<n>
       If paytxfee is not set, include enough fee so transactions begin
       confirmation on average within n blocks (default: 2)

  -maxtxfee=<amt>
       Maximum total fees to use in a single wallet transaction; setting this
       too low may abort large transactions (default: 0.10)

  -upgradewallet
       Upgrade wallet to latest format on startup

  -wallet=<file>
       Specify wallet file (within data directory) (default: wallet.dat)

  -walletbroadcast
       Make the wallet broadcast transactions (default: 1)

  -walletnotify=<cmd>
       Execute command when a wallet transaction changes (%s in cmd is replaced
       by TxID)

  -zapwallettxes=<mode>
       Delete all wallet transactions and only recover those parts of the
       blockchain through -rescan on startup (1 = keep tx meta data e.g.
       account owner and payment request information, 2 = drop tx meta data)

Debugging/Testing options:

  -checkpoints
       Only accept block chain matching built-in checkpoints (default: 1)

  -dblogsize=<n>
       Flush database activity from memory pool to disk log every <n> megabytes
       (default: 100)

  -disablesafemode
       Disable safemode, override a real safe mode event (default: 0)

  -testsafemode
       Force safe mode (default: 0)

  -dropmessagestest=<n>
       Randomly drop 1 of every <n> network messages

  -fuzzmessagestest=<n>
       Randomly fuzz 1 of every <n> network messages

  -flushwallet
       Run a thread to flush wallet periodically (default: 1)

  -stopafterblockimport
       Stop running after importing blocks from disk (default: 0)

  -debug=<category>
       Output debugging information (default: 0, supplying <category> is
       optional). If <category> is not supplied, output all debugging
       information.<category> can be: addrman, alert, bench, coindb, db, lock,
       rand, rpc, selectcoins, mempool, net, proxy, prune.

  -gen
       Generate coins (default: 0)

  -genproclimit=<n>
       Set the number of threads for coin generation if enabled (-1 = all
       cores, default: 1)

  -help-debug
       Show all debugging options (usage: --help -help-debug)

  -logips
       Include IP addresses in debug output (default: 0)

  -logtimestamps
       Prepend debug output with timestamp (default: 1)

  -limitfreerelay=<n>
       Continuously rate-limit free transactions to <n>*1000 bytes per minute
       (default: 15)

  -relaypriority
       Require high priority for relaying free or low-fee transactions
       (default: 1)

  -maxsigcachesize=<n>
       Limit size of signature cache to <n> entries (default: 50000)

  -minrelaytxfee=<amt>
       Fees (in PAI/Kb) smaller than this are considered zero fee for relaying
       (default: 0.00001)

  -printtoconsole
       Send trace/debug info to console instead of debug.log file

  -printpriority
       Log transaction priority and fee per kB when mining blocks (default: 0)

  -privdb
       Sets the DB_PRIVATE flag in the wallet db environment (default: 1)

  -regtest
       Enter regression test mode, which uses a special chain in which blocks
       can be solved instantly. This is intended for regression testing tools
       and app development.

  -shrinkdebugfile
       Shrink debug.log file on client startup (default: 1 when no -debug)

  -testnet
       Use the test network

Node relay options:

  -datacarrier
       Relay and mine data carrier transactions (default: 1)

  -datacarriersize
       Maximum size of data in data carrier transactions we relay and mine
       (default: 80)

Block creation options:

  -blockminsize=<n>
       Set minimum block size in bytes (default: 0)

  -blockmaxsize=<n>
       Set maximum block size in bytes (default: 750000)

  -blockprioritysize=<n>
       Set maximum size of high-priority/low-fee transactions in bytes
       (default: 50000)

RPC server options:

  -server
       Accept command line and JSON-RPC commands

  -rest
       Accept public REST requests (default: 0)

  -rpcbind=<addr>
       Bind to given address to listen for JSON-RPC connections. Use
       [host]:port notation for IPv6. This option can be specified multiple
       times (default: bind to all interfaces)

  -rpcuser=<user>
       Username for JSON-RPC connections

  -rpcpassword=<pw>
       Password for JSON-RPC connections

  -rpcport=<port>
       Listen for JSON-RPC connections on <port> (default: 8332 or testnet:
       18332)

  -rpcallowip=<ip>
       Allow JSON-RPC connections from specified source. Valid for <ip> are a
       single IP (e.g. 1.2.3.4), a network/netmask (e.g. 1.2.3.4/255.255.255.0)
       or a network/CIDR (e.g. 1.2.3.4/24). This option can be specified
       multiple times

  -rpcthreads=<n>
       Set the number of threads to service RPC calls (default: 4)

  -rpckeepalive
       RPC support for HTTP persistent connections (default: 1)

RPC SSL options: (see the PAI Coin Wiki for SSL setup instructions)

  -rpcssl
       Use OpenSSL (https) for JSON-RPC connections

  -rpcsslcertificatechainfile=<file.cert>
       Server certificate file (default: server.cert)

  -rpcsslprivatekeyfile=<file.pem>
       Server private key (default: server.pem)

  -rpcsslciphers=<ciphers>
       Acceptable ciphers (default:
       TLSv1.2+HIGH:TLSv1+HIGH:!SSLv2:!aNULL:!eNULL:!3DES:@STRENGTH)
```
