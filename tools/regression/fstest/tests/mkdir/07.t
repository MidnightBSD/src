#!/bin/sh
# $FreeBSD: release/7.0.0/tools/regression/fstest/tests/mkdir/07.t 166065 2007-01-17 01:42:12Z pjd $

desc="mkdir returns ELOOP if too many symbolic links were encountered in translating the pathname"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..6"

n0=`namegen`
n1=`namegen`

expect 0 symlink ${n0} ${n1}
expect 0 symlink ${n1} ${n0}
expect ELOOP mkdir ${n0}/test 0755
expect ELOOP mkdir ${n1}/test 0755
expect 0 unlink ${n0}
expect 0 unlink ${n1}
