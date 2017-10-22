#!/bin/sh
# $FreeBSD: release/7.0.0/tools/regression/fstest/tests/open/03.t 166065 2007-01-17 01:42:12Z pjd $

desc="open returns ENAMETOOLONG if an entire path name exceeded 1023 characters"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..12"

expect 0 mkdir ${name255} 0755
expect 0 mkdir ${name255}/${name255} 0755
expect 0 mkdir ${name255}/${name255}/${name255} 0755
expect 0 mkdir ${path1021} 0755
expect 0 open ${path1023} O_CREAT 0642
expect 0642 stat ${path1023} mode
expect 0 unlink ${path1023}
expect ENAMETOOLONG open ${path1024} O_CREAT 0642
expect 0 rmdir ${path1021}
expect 0 rmdir ${name255}/${name255}/${name255}
expect 0 rmdir ${name255}/${name255}
expect 0 rmdir ${name255}
