#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/lib/libc/stdio/test-mkostemp.t 255302 2013-09-06 12:59:48Z jilles $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
