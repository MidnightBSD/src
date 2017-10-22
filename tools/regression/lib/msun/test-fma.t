#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/lib/msun/test-fma.t 177876 2008-04-03 06:15:58Z das $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
