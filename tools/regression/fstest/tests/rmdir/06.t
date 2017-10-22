#!/bin/sh
# $FreeBSD: release/7.0.0/tools/regression/fstest/tests/rmdir/06.t 166065 2007-01-17 01:42:12Z pjd $

desc="rmdir returns EEXIST or ENOTEMPTY the named directory contains files other than '.' and '..' in it"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..20"

n0=`namegen`
n1=`namegen`

expect 0 mkdir ${n0} 0755
expect 0 mkdir ${n0}/${n1} 0755
expect "EEXIST|ENOTEMPTY" rmdir ${n0}
expect 0 rmdir ${n0}/${n1}
expect 0 rmdir ${n0}

expect 0 mkdir ${n0} 0755
expect 0 create ${n0}/${n1} 0644
expect "EEXIST|ENOTEMPTY" rmdir ${n0}
expect 0 unlink ${n0}/${n1}
expect 0 rmdir ${n0}

expect 0 mkdir ${n0} 0755
expect 0 symlink test ${n0}/${n1}
expect "EEXIST|ENOTEMPTY" rmdir ${n0}
expect 0 unlink ${n0}/${n1}
expect 0 rmdir ${n0}

expect 0 mkdir ${n0} 0755
expect 0 mkfifo ${n0}/${n1} 0644
expect "EEXIST|ENOTEMPTY" rmdir ${n0}
expect 0 unlink ${n0}/${n1}
expect 0 rmdir ${n0}
