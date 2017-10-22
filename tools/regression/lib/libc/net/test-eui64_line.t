#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/lib/libc/net/test-eui64_line.t 137587 2004-11-11 19:47:55Z nik $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
