#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/lib/msun/test-nearbyint.t 216139 2010-12-03 00:44:31Z das $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
