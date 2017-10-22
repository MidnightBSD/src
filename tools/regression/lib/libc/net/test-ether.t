#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/lib/libc/net/test-ether.t 169523 2007-05-13 14:03:21Z rwatson $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
