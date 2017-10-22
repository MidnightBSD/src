#!/bin/sh
# $FreeBSD: release/7.0.0/tools/regression/fstest/tests/unlink/02.t 166065 2007-01-17 01:42:12Z pjd $

desc="unlink returns ENAMETOOLONG if a component of a pathname exceeded 255 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..4"

expect 0 create ${name255} 0644
expect 0 unlink ${name255}
expect ENOENT unlink ${name255}
expect ENAMETOOLONG unlink ${name256}
