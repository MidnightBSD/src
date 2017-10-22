#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/pjdfstest/tests/truncate/09.t 166065 2007-01-17 01:42:12Z pjd $

desc="truncate returns EISDIR if the named file is a directory"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..3"

n0=`namegen`

expect 0 mkdir ${n0} 0755
expect EISDIR truncate ${n0} 123
expect 0 rmdir ${n0}
