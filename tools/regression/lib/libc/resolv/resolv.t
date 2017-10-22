#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/lib/libc/resolv/resolv.t 145901 2005-05-05 11:26:47Z ume $

do_test() {
	number=$1
	comment=$2
	opt=$3
	if ./$executable $opt; then
		echo "ok $number - $comment"
	else
		echo "not ok $number - $comment"
	fi
}

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

echo 1..3
do_test 1 'getaddrinfo()'     '-r mach'
do_test 2 'gethostbyname()'   '-rH mach'
do_test 3 'getipnodebyname()' '-rI mach'
