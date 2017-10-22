#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/lib/libutil/test-trimdomain-nodomain.t 150956 2005-10-05 04:46:10Z brooks $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
