#!/bin/sh
# $FreeBSD: stable/9/tools/regression/lib/libutil/test-trimdomain.t 150956 2005-10-05 04:46:10Z brooks $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
