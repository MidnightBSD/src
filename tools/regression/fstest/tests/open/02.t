#!/bin/sh
# $FreeBSD: release/7.0.0/tools/regression/fstest/tests/open/02.t 166065 2007-01-17 01:42:12Z pjd $

desc="open returns ENAMETOOLONG if a component of a pathname exceeded 255 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..4"

expect 0 open ${name255} O_CREAT 0620
expect 0620 stat ${name255} mode
expect 0 unlink ${name255}
expect ENAMETOOLONG open ${name256} O_CREAT 0620
