#!/bin/sh
# $FreeBSD: release/7.0.0/tools/regression/fstest/tests/truncate/02.t 166065 2007-01-17 01:42:12Z pjd $

desc="truncate returns ENAMETOOLONG if a component of a pathname exceeded 255 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..5"

expect 0 create ${name255} 0644
expect 0 truncate ${name255} 123
expect 123 stat ${name255} size
expect 0 unlink ${name255}
expect ENAMETOOLONG truncate ${name256} 123
