#!/bin/sh
# $FreeBSD: release/7.0.0/tools/regression/fstest/tests/unlink/04.t 166065 2007-01-17 01:42:12Z pjd $

desc="unlink returns ENOENT if the named file does not exist"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..4"

n0=`namegen`
n1=`namegen`

expect 0 create ${n0} 0644
expect 0 unlink ${n0}
expect ENOENT unlink ${n0}
expect ENOENT unlink ${n1}
