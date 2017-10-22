#!/bin/sh
#
# $FreeBSD: release/10.0.0/release/scripts/pkg-stage.sh 260787 2014-01-16 18:33:10Z gjb $
#

set -e

export ASSUME_ALWAYS_YES=1
export PKG_DBDIR="/tmp/pkg"
export PERMISSIVE="YES"
export REPO_AUTOUPDATE="NO"
export PKGCMD="/usr/sbin/pkg -d"

DVD_PACKAGES="archivers/unzip
devel/subversion
devel/subversion-static
emulators/linux_base-f10
misc/freebsd-doc-all
net/mpd5
net/rsync
ports-mgmt/pkg
ports-mgmt/portmaster
shells/bash
shells/zsh
security/sudo
sysutils/screen
www/firefox
www/links
x11-drivers/xf86-video-vmware
x11/gnome2
x11/kde4
x11/xorg"

# If NOPORTS is set for the release, do not attempt to build pkg(8).
if [ ! -f /usr/ports/Makefile ]; then
	exit 0
fi

if [ ! -x /usr/local/sbin/pkg ]; then
	/usr/bin/make -C /usr/ports/ports-mgmt/pkg install clean
fi

export PKG_ABI=$(pkg -vv | grep ^ABI | awk '{print $3}')
export PKG_CACHEDIR="dvd/packages/${PKG_ABI}"

/bin/mkdir -p ${PKG_CACHEDIR}

# Print pkg(8) information to make debugging easier.
${PKGCMD} -vv
${PKGCMD} update -f
${PKGCMD} fetch -d ${DVD_PACKAGES}

${PKGCMD} repo ${PKG_CACHEDIR}

# Always exit '0', even if pkg(8) complains about conflicts.
exit 0
