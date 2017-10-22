#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/pjdfstest/tests/rmdir/05.t 166065 2007-01-17 01:42:12Z pjd $

desc="rmdir returns ELOOP if too many symbolic links were encountered in translating the pathname"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..6"

n0=`namegen`
n1=`namegen`

expect 0 symlink ${n0} ${n1}
expect 0 symlink ${n1} ${n0}
expect ELOOP rmdir ${n0}/test
expect ELOOP rmdir ${n1}/test
expect 0 unlink ${n0}
expect 0 unlink ${n1}
