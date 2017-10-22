#!/bin/sh
# $FreeBSD: release/7.0.0/tools/regression/fstest/tests/chflags/04.t 166065 2007-01-17 01:42:12Z pjd $

desc="chflags returns ENOENT if the named file does not exist"

dir=`dirname $0`
. ${dir}/../misc.sh

require chflags

echo "1..4"

n0=`namegen`
n1=`namegen`

expect 0 mkdir ${n0} 0755
expect ENOENT chflags ${n0}/${n1}/test UF_IMMUTABLE
expect ENOENT chflags ${n0}/${n1} UF_IMMUTABLE
expect 0 rmdir ${n0}
