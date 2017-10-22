#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/lib/msun/test-rem.t 144533 2005-04-02 12:50:28Z das $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
