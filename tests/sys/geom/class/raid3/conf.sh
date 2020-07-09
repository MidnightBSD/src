#!/bin/sh
# $FreeBSD: stable/11/tests/sys/geom/class/raid3/conf.sh 293438 2016-01-08 19:47:49Z ngie $

name="$(mktemp -u graid3.XXXXXX)"
class="raid3"
base=`basename $0`

graid3_test_cleanup()
{
	[ -c /dev/$class/$name ] && graid3 stop $name
	geom_test_cleanup
}
trap graid3_test_cleanup ABRT EXIT INT TERM

. `dirname $0`/../geom_subr.sh
