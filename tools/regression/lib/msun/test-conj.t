#!/bin/sh
# $FreeBSD: stable/9/tools/regression/lib/msun/test-conj.t 196696 2009-08-31 13:23:55Z jhb $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
