#!/bin/sh
# $FreeBSD: release/7.0.0/tools/regression/fstest/tests/chflags/02.t 166065 2007-01-17 01:42:12Z pjd $

desc="chflags returns ENAMETOOLONG if a component of a pathname exceeded 255 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

require chflags

echo "1..6"

expect 0 create ${name255} 0644
expect 0 chflags ${name255} UF_IMMUTABLE
expect UF_IMMUTABLE stat ${name255} flags
expect 0 chflags ${name255} none
expect 0 unlink ${name255}
expect ENAMETOOLONG chflags ${name256} UF_IMMUTABLE
