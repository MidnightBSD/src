#!/bin/sh
# $FreeBSD: release/7.0.0/tools/regression/fstest/tests/chown/10.t 166065 2007-01-17 01:42:12Z pjd $

desc="chown returns EFAULT if the path argument points outside the process's allocated address space"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..2"

expect EFAULT chown NULL 65534 65534
expect EFAULT chown DEADCODE 65534 65534
