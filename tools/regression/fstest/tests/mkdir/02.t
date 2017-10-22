#!/bin/sh
# $FreeBSD: release/7.0.0/tools/regression/fstest/tests/mkdir/02.t 166065 2007-01-17 01:42:12Z pjd $

desc="mkdir returns ENAMETOOLONG if a component of a pathname exceeded 255 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..3"

expect 0 mkdir ${name255} 0755
expect 0 rmdir ${name255}
expect ENAMETOOLONG mkdir ${name256} 0755
