#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/pjdfstest/tests/truncate/03.t 211178 2010-08-11 16:33:17Z pjd $

desc="truncate returns ENAMETOOLONG if an entire path name exceeded {PATH_MAX} characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..5"

nx=`dirgen_max`
nxx="${nx}x"

mkdir -p "${nx%/*}"

expect 0 create ${nx} 0644
expect 0 truncate ${nx} 123
expect regular,123 stat ${nx} type,size
expect 0 unlink ${nx}
expect ENAMETOOLONG truncate ${nxx} 123

rm -rf "${nx%%/*}"
