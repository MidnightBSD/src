#!/bin/sh
# $FreeBSD: stable/10/lib/msun/tests/lround_test.t 294243 2016-01-18 03:55:40Z ngie $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
