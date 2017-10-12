
Debian
====================
This directory contains files used to package paicoind/paicoin-qt
for Debian-based Linux systems. If you compile paicoind/paicoin-qt yourself, there are some useful files here.

## paicoin: URI support ##


paicoin-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install paicoin-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your paicoin-qt binary to `/usr/bin`
and the `../../share/pixmaps/paicoin128.png` to `/usr/share/pixmaps`

paicoin-qt.protocol (KDE)

