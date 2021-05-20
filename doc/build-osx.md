Mac OS X Build Instructions and Notes
====================================
The commands in this guide should be executed in a Terminal application.
The built-in one is located in `/Applications/Utilities/Terminal.app`.

Preparation
-----------
Install the OS X command line tools:

`xcode-select --install`

When the popup appears, click `Install`.

Then install [Homebrew](https://brew.sh).

Dependencies
----------------------

    brew install automake libtool pkg-config python3

See [dependencies.md](dependencies.md) for a complete overview.

If you want to build the disk image with `make deploy` (.dmg / optional), you need RSVG

    brew install librsvg

NOTE: Building with Qt4 is still supported, however, could result in a broken UI. Building with Qt5 is recommended.

## Berkeley DB
It is recommended to use Berkeley DB 4.8. If you have to build it yourself,
you can use [this script](/contrib/install_db4.sh) to install it
like so:

```shell
./contrib/install_db4.sh .
```

from the root of the repository.

**Note**: You only need Berkeley DB if the wallet is enabled (see [*Disable-wallet mode*](/doc/build-osx.md#disable-wallet-mode)).


Build PAI Coin Core
------------------------

1. Clone the paicoin source code and cd into `paicoin`

        git clone https://github.com/projectpai/paicoin
        cd paicoin

2. If you have Catalina 10.15 or a higher version, then you must run this command:

	If you have XCode installed:

	export CPATH=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include

	If you have only the command-line tools:

	CPATH=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/

3.  Build paicoin-core:

    Configure and build the headless paicoin binaries as well as the GUI (if Qt is found).

    You can enable the GUI build by passing `--with-gui=auto` to configure.

        cd depends
        make
        cd ..
        ./autogen.sh
        CONFIG_SITE=`pwd`/depends/`build-aux/config.guess`/share/config.site ./configure
        make

4.  It is recommended to build and run the unit tests:

        make check

5.  You can also create a .dmg that contains the .app bundle (optional):

        make deploy

## `disable-wallet` mode
When the intention is to run only a P2P node without a wallet, Bitcoin Core may be
compiled in `disable-wallet` mode with:
```shell
./configure --disable-wallet
```

In this case there is no dependency on Berkeley DB 4.8 and SQLite.

Mining is also possible in disable-wallet mode using the `getblocktemplate` RPC call.

Running
-------

PAI Coin Core is now available at `./src/paicoind`

Before running, it's recommended you create an RPC configuration file.

    echo -e "rpcuser=paicoinrpc\nrpcpassword=$(xxd -l 16 -p /dev/urandom)" > "/Users/${USER}/Library/Application Support/PAIcoin/paicoin.conf"

    chmod 600 "/Users/${USER}/Library/Application Support/PAIcoin/paicoin.conf"

The first time you run paicoind, it will start downloading the blockchain. This process could take several hours.

You can monitor the download process by looking at the debug.log file:

    tail -f $HOME/Library/Application\ Support/PAIcoin/debug.log

Other commands:
-------

    ./src/paicoind -daemon # Starts the paicoin daemon.
    ./src/paicoin-cli --help # Outputs a list of command-line options.
    ./src/paicoin-cli help # Outputs a list of RPC commands when the daemon is running.

Using Qt Creator as IDE
------------------------
You can use Qt Creator as an IDE, for paicoin development.
Download and install the community edition of [Qt Creator](https://www.qt.io/download/).
Uncheck everything except Qt Creator during the installation process.

1. Make sure you installed everything through Homebrew mentioned above
2. Do a proper ./configure --enable-debug
3. In Qt Creator do "New Project" -> Import Project -> Import Existing Project
4. Enter "paicoin-qt" as project name, enter src/qt as location
5. Leave the file selection as it is
6. Confirm the "summary page"
7. In the "Projects" tab select "Manage Kits..."
8. Select the default "Desktop" kit and select "Clang (x86 64bit in /usr/bin)" as compiler
9. Select LLDB as debugger (you might need to set the path to your installation)
10. Start debugging with Qt Creator

Notes
-----

* Tested on OS X 10.8 through 10.12 on 64-bit Intel processors only.

* Building with downloaded Qt binaries is not officially supported. See the notes in [#7714](https://github.com/bitcoin/bitcoin/issues/7714)
