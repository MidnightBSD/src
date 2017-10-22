#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/lib/msun/test-cexp.t 219362 2011-03-07 03:15:49Z das $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
