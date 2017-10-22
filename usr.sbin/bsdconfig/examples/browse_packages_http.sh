#!/bin/sh
# $FreeBSD: release/10.0.0/usr.sbin/bsdconfig/examples/browse_packages_http.sh 255818 2013-09-23 16:47:52Z dteske $
#
# This sample downloads the package INDEX file from HTTP to /tmp (if it doesn't
# already exist) and then displays the package configuration/management screen
# using the local INDEX file (results in faster browsing of packages from-start
# since the INDEX can be loaded from local media).
#
# NOTE: Packages cannot be installed unless staged to /tmp/packages/All
#
. /usr/share/bsdconfig/script.subr
nonInteractive=1
TMPDIR=/tmp
if [ ! -e "$TMPDIR/packages/INDEX" ]; then
	[ -d "$TMPDIR/packages" ] || mkdir -p "$TMPDIR/packages" || exit 1
	_httpPath=http://ftp.freebsd.org
	# For older releases, use http://ftp-archive.freebsd.org
	mediaSetHTTP
	mediaOpen
	f_show_info "Downloading packages/INDEX from\n %s" "$_httpPath"
	f_device_get media packages/INDEX > $TMPDIR/packages/INDEX
fi
_directoryPath=$TMPDIR
mediaSetDirectory
configPackages
