#!/bin/sh
# $FreeBSD: stable/11/tests/sys/geom/class/stripe/conf.sh 293442 2016-01-08 21:28:09Z ngie $

name="$(mktemp -u stripe.XXXXXX)"
class="stripe"
base=`basename $0`

gstripe_test_cleanup()
{
	[ -c /dev/$class/$name ] && gstripe destroy $name
	geom_test_cleanup
}
trap gstripe_test_cleanup ABRT EXIT INT TERM

. `dirname $0`/../geom_subr.sh
