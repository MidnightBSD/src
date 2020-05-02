#!/bin/sh
# $FreeBSD: stable/11/lib/msun/tests/lround_test.t 292497 2015-12-20 05:06:44Z ngie $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
