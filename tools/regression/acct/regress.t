#!/bin/sh
#
# $FreeBSD: release/7.0.0/tools/regression/acct/regress.t 169851 2007-05-22 05:52:04Z dds $
#

DIR=`dirname $0`

check()
{
	NUM=$1
	shift
	if $DIR/pack $*
	then
		echo "ok $NUM"
	else
		echo "not ok $NUM"
	fi
}

echo 1..7

check 1 0 0
check 2 0 1
check 3 1 0
check 4 1 999999
check 5 1 1000000
check 6 2147483647 999999
check 7 10000000

exit 0
