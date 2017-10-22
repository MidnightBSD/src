#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/lib/libc/stdio/test-open_memstream.t 255301 2013-09-06 12:56:49Z jilles $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

echo 1..1
if ./$executable; then
	echo ok 1 - $executable successful
else
	echo not ok 1 - $executable failed
fi
