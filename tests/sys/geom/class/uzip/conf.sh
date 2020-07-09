#!/bin/sh
# $FreeBSD: stable/11/tests/sys/geom/class/uzip/conf.sh 324404 2017-10-07 23:10:16Z ngie $

class="uzip"
base=`basename $0`

uzip_test_cleanup()
{
	if [ -n "$mntpoint" ]; then
		umount $mntpoint
		rmdir $mntpoint
	fi
	geom_test_cleanup
}
trap uzip_test_cleanup ABRT EXIT INT TERM

. `dirname $0`/../geom_subr.sh

# NOTE: make sure $TMPDIR has been set by geom_subr.sh if unset [by kyua, etc]
mntpoint=$(mktemp -d tmp.XXXXXX) || exit
