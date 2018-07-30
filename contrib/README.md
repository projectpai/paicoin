Repository Tools
---------------------

### [Linearize](/contrib/linearize) ###
Construct a linear, no-fork, best version of the blockchain.

### [Qos](/contrib/qos) ###

A Linux bash script that will set up traffic control (tc) to limit the outgoing bandwidth for connections to the PAIcoin network. This means one can have an always-on paicoind instance running, and another local paicoind/paiup instance which connects to this node and receives blocks from it.

### [Seeds](/contrib/seeds) ###
Utility to generate the pnSeed[] array that is compiled into the client.

Build Tools and Keys
---------------------

### [Debian](/contrib/debian) ###
Contains files used to package paicoind/paicoin-qt
for Debian-based Linux systems. If you compile paicoind/paicoin-qt yourself, there are some useful files here.

### [MacDeploy](/contrib/macdeploy) ###
Scripts and notes for Mac builds. 

### [RPM](/contrib/rpm) ###
RPM spec file for building paicoin-core on RPM based distributions.