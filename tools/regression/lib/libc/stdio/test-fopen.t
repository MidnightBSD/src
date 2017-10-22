#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/lib/libc/stdio/test-fopen.t 255303 2013-09-06 13:47:16Z jilles $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
