#!/bin/sh
# $FreeBSD: stable/9/tools/regression/file/closefrom/closefrom.t 194262 2009-06-15 20:38:55Z jhb $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
