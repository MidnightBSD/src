#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/usr.bin/cc/float.t 230368 2012-01-20 06:57:21Z das $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
