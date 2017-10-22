#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/lib/libutil/test-humanize_number.t 256130 2013-10-07 22:22:57Z jmg $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable && echo humanize_numbers ok
