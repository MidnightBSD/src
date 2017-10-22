#!/bin/sh
# $FreeBSD: release/7.0.0/tools/regression/fstest/tests/mkfifo/02.t 166065 2007-01-17 01:42:12Z pjd $

desc="mkfifo returns ENAMETOOLONG if a component of a pathname exceeded 255 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..3"

expect 0 mkfifo ${name255} 0644
expect 0 unlink ${name255}
expect ENAMETOOLONG mkfifo ${name256} 0644
