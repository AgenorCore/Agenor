
Debian
====================
This directory contains files used to package agenord/agenor-qt
for Debian-based Linux systems. If you compile agenord/agenor-qt yourself, there are some useful files here.

## agenor: URI support ##


agenor-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install agenor-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your agenor-qt binary to `/usr/bin`
and the `../../share/pixmaps/agenor128.png` to `/usr/share/pixmaps`

agenor-qt.protocol (KDE)

