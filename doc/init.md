Sample init scripts and service configuration for paicoind
==========================================================

Sample scripts and configuration files for systemd, Upstart and OpenRC
can be found in the contrib/init folder.

    contrib/init/paicoind.service:    systemd service unit configuration
    contrib/init/paicoind.openrc:     OpenRC compatible SysV style init script
    contrib/init/paicoind.openrcconf: OpenRC conf.d file
    contrib/init/paicoind.conf:       Upstart service configuration file
    contrib/init/paicoind.init:       CentOS compatible SysV style init script

Service User
---------------------------------

All three Linux startup configurations assume the existence of a "paicoin" user
and group.  They must be created before attempting to use these scripts.
The OS X configuration assumes paicoind will be set up for the current user.

Configuration
---------------------------------

At a bare minimum, paicoind requires that the rpcpassword setting be set
when running as a daemon.  If the configuration file does not exist or this
setting is not set, paicoind will shutdown promptly after startup.

This password does not have to be remembered or typed as it is mostly used
as a fixed token that paicoind and client programs read from the configuration
file, however it is recommended that a strong and secure password be used
as this password is security critical to securing the wallet should the
wallet be enabled.

If paicoind is run with the "-server" flag (set by default), and no rpcpassword is set,
it will use a special cookie file for authentication. The cookie is generated with random
content when the daemon starts, and deleted when it exits. Read access to this file
controls who can access it through RPC.

By default the cookie is stored in the data directory, but it's location can be overridden
with the option '-rpccookiefile'.

This allows for running paicoind without having to do any manual configuration.

`conf`, `pid`, and `wallet` accept relative paths which are interpreted as
relative to the data directory. `wallet` *only* supports relative paths.

For an example configuration file that describes the configuration settings,
see `contrib/debian/examples/paicoin.conf`.

Paths
---------------------------------

### Linux

All three configurations assume several paths that might need to be adjusted.

Binary:              `/usr/bin/paicoind`  
Configuration file:  `/etc/paicoin/paicoin.conf`  
Data directory:      `/var/lib/paicoind`  
PID file:            `/var/run/paicoind/paicoind.pid` (OpenRC and Upstart) or `/var/lib/paicoind/paicoind.pid` (systemd)  
Lock file:           `/var/lock/subsys/paicoind` (CentOS)  

The configuration file, PID directory (if applicable) and data directory
should all be owned by the paicoin user and group.  It is advised for security
reasons to make the configuration file and data directory only readable by the
paicoin user and group.  Access to paicoin-cli and other paicoind rpc clients
can then be controlled by group membership.

### Mac OS X

Binary:              `/usr/local/bin/paicoind`  
Configuration file:  `~/Library/Application Support/PAIcoin/paicoin.conf`  
Data directory:      `~/Library/Application Support/PAIcoin`  
Lock file:           `~/Library/Application Support/PAIcoin/.lock`  

Installing Service Configuration
-----------------------------------

### systemd

Installing this .service file consists of just copying it to
/usr/lib/systemd/system directory, followed by the command
`systemctl daemon-reload` in order to update running systemd configuration.

To test, run `systemctl start paicoind` and to enable for system startup run
`systemctl enable paicoind`

### OpenRC

Rename paicoind.openrc to paicoind and drop it in /etc/init.d.  Double
check ownership and permissions and make it executable.  Test it with
`/etc/init.d/paicoind start` and configure it to run on startup with
`rc-update add paicoind`

### Upstart (for Debian/Ubuntu based distributions)

Drop paicoind.conf in /etc/init.  Test by running `service paicoind start`
it will automatically start on reboot.

NOTE: This script is incompatible with CentOS 5 and Amazon Linux 2014 as they
use old versions of Upstart and do not supply the start-stop-daemon utility.

### CentOS

Copy paicoind.init to /etc/init.d/paicoind. Test by running `service paicoind start`.

Using this script, you can adjust the path and flags to the paicoind program by
setting the PAICOIND and FLAGS environment variables in the file
/etc/sysconfig/paicoind. You can also use the DAEMONOPTS environment variable here.

### Mac OS X

Copy org.paicoin.paicoind.plist into ~/Library/LaunchAgents. Load the launch agent by
running `launchctl load ~/Library/LaunchAgents/org.paicoin.paicoind.plist`.

This Launch Agent will cause paicoind to start whenever the user logs in.

NOTE: This approach is intended for those wanting to run paicoind as the current user.
You will need to modify org.paicoin.paicoind.plist if you intend to use it as a
Launch Daemon with a dedicated paicoin user.

Auto-respawn
-----------------------------------

Auto respawning is currently only configured for Upstart and systemd.
Reasonable defaults have been chosen but YMMV.
