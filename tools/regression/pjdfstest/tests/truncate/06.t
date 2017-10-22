#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/pjdfstest/tests/truncate/06.t 166065 2007-01-17 01:42:12Z pjd $

desc="truncate returns EACCES if the named file is not writable by the user"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..8"

n0=`namegen`
n1=`namegen`

expect 0 mkdir ${n0} 0755
cdir=`pwd`
cd ${n0}
expect 0 create ${n1} 0644
expect EACCES -u 65534 -g 65534 truncate ${n1} 123
expect 0 chown ${n1} 65534 65534
expect 0 chmod ${n1} 0444
expect EACCES -u 65534 -g 65534 truncate ${n1} 123
expect 0 unlink ${n1}
cd ${cdir}
expect 0 rmdir ${n0}
