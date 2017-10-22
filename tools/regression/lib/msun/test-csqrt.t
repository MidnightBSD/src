#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/lib/msun/test-csqrt.t 174619 2007-12-15 09:16:26Z das $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
