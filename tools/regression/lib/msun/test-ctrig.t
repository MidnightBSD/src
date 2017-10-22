#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/lib/msun/test-ctrig.t 226603 2011-10-21 06:34:38Z das $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
