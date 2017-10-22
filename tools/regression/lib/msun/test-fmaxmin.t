#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/lib/msun/test-fmaxmin.t 180237 2008-07-03 23:06:06Z das $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
