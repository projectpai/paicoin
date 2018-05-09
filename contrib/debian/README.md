
Debian
====================
This directory contains files used to package paicoind/paiup
for Debian-based Linux systems. If you compile paicoind/paiup yourself, there are some useful files here.

## paicoin: URI support ##


paiup.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install paiup.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your paiup binary to `/usr/bin`
and the `../../share/pixmaps/paicoin128.png` to `/usr/share/pixmaps`

paiup.protocol (KDE)

