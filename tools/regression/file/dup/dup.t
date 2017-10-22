#!/bin/sh
# $FreeBSD: stable/9/tools/regression/file/dup/dup.t 164191 2006-11-11 18:32:50Z maxim $

cd `dirname $0`

executable=`basename $0 .t`

make $executable 2>&1 > /dev/null

exec ./$executable
