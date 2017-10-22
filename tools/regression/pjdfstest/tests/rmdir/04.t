#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/pjdfstest/tests/rmdir/04.t 166065 2007-01-17 01:42:12Z pjd $

desc="rmdir returns ENOENT if the named directory does not exist"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..4"

n0=`namegen`
n1=`namegen`

expect 0 mkdir ${n0} 0755
expect 0 rmdir ${n0}
expect ENOENT rmdir ${n0}
expect ENOENT rmdir ${n1}
