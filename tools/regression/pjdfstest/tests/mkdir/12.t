#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/pjdfstest/tests/mkdir/12.t 166065 2007-01-17 01:42:12Z pjd $

desc="mkdir returns EFAULT if the path argument points outside the process's allocated address space"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..2"

expect EFAULT mkdir NULL 0755
expect EFAULT mkdir DEADCODE 0755
