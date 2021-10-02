#!/bin/sh

set -e

export ASSUME_ALWAYS_YES="YES"
export PKG_DBDIR="/tmp/pkg"
export PERMISSIVE="YES"
export REPO_AUTOUPDATE="NO"
export PKGCMD="/usr/sbin/mport"
export PORTSDIR="${PORTSDIR:-/usr/mports}"

_DVD_PACKAGES="archivers/unzip
devel/subversion
devel/subversion-static
emulators/linux_base-c6
graphics/drm-fbsd11.2-kmod
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
x11/gnome3
x11/xorg"

export DVD_DIR="dvd/packages"
export PKG_ABI=$(pkg config ABI)
export PKG_ALTABI=$(pkg config ALTABI 2>/dev/null)
export PKG_REPODIR="${DVD_DIR}/${PKG_ABI}"

/bin/mkdir -p ${PKG_REPODIR}
if [ ! -z "${PKG_ALTABI}" ]; then
	(cd ${DVD_DIR} && ln -s ${PKG_ABI} ${PKG_ALTABI})
fi

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
${PKGCMD} download -o ${PKG_REPODIR} -d ${DVD_PACKAGES}

# Create the 'Latest/pkg.txz' symlink so 'pkg bootstrap' works
# using the on-disc packages.
mkdir -p ${PKG_REPODIR}/Latest
(cd ${PKG_REPODIR}/Latest && \
	ln -s ../All/$(${PKGCMD} rquery %n-%v pkg).txz pkg.txz)

${PKGCMD} repo ${PKG_REPODIR}

# Always exit '0', even if pkg(8) complains about conflicts.
exit 0
