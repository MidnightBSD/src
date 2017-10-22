#!/bin/sh
# $FreeBSD: release/7.0.0/tools/regression/fstest/tests/link/10.t 166065 2007-01-17 01:42:12Z pjd $

desc="link returns EEXIST if the destination file does exist"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..14"

n0=`namegen`
n1=`namegen`

expect 0 create ${n0} 0644

expect 0 create ${n1} 0644
expect EEXIST link ${n0} ${n1}
expect 0 unlink ${n1}

expect 0 mkdir ${n1} 0755
expect EEXIST link ${n0} ${n1}
expect 0 rmdir ${n1}

expect 0 symlink test ${n1}
expect EEXIST link ${n0} ${n1}
expect 0 unlink ${n1}

expect 0 mkfifo ${n1} 0644
expect EEXIST link ${n0} ${n1}
expect 0 unlink ${n1}

expect 0 unlink ${n0}
