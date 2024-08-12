#!/bin/sh

set -e

export ASSUME_ALWAYS_YES="YES"
export PKG_DBDIR="/tmp/pkg"
export PERMISSIVE="YES"
export REPO_AUTOUPDATE="NO"
export PKGCMD="/usr/sbin/mport"
export PORTSDIR="${PORTSDIR:-/usr/mports}"

_DVD_PACKAGES="archivers/unzip
devel/git
emulators/linux_base-c7
graphics/drm-fbsd12.0-kmod
net/mpd5
net/rsync
shells/bash
shells/zsh
security/sudo
sysutils/screen
sysutils/tmux
www/firefox
www/links
x11-drivers/xf86-video-vmware
x11/gnome
x11/xorg"

if [ ! -f ${PORTSDIR}/Makefile ]; then
	echo "*** ${PORTSDIR} is missing!    ***"
	echo "*** Skipping pkg-stage.sh     ***"
	echo "*** Unset NOPORTS to fix this ***"
	exit 0
fi
export DVD_DIR="dvd/packages"
export PKG_REPODIR="${DVD_DIR}"

/bin/mkdir -p ${PKG_REPODIR}

# Ensure the ports listed in _DVD_PACKAGES exist to sanitize the
# final list.
for _P in ${_DVD_PACKAGES}; do
	if [ -d "${PORTSDIR}/${_P}" ]; then
		DVD_PACKAGES="${DVD_PACKAGES} ${_P}"
	else
		echo "*** Skipping nonexistent port: ${_P}"
	fi
done

# Make sure the package list is not empty.
if [ -z "${DVD_PACKAGES}" ]; then
	echo "*** The package list is empty."
	echo "*** Something is very wrong."
	# Exit '0' so the rest of the build process continues
	# so other issues (if any) can be addressed as well.
	exit 0
fi

# Print pkg(8) information to make debugging easier.
${PKGCMD} index
cp /var/db/mport/index.db ${PKG_REPODIR}/index.db
${PKGCMD} -o ${PKG_REPODIR} download -d ${DVD_PACKAGES}

# Always exit '0', even if pkg(8) complains about conflicts.
exit 0
