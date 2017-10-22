#!/bin/sh
# $FreeBSD: release/7.0.0/tools/regression/fstest/tests/chflags/13.t 166065 2007-01-17 01:42:12Z pjd $

desc="chflags returns EFAULT if the path argument points outside the process's allocated address space"

dir=`dirname $0`
. ${dir}/../misc.sh

require chflags

echo "1..2"

expect EFAULT chflags NULL UF_IMMUTABLE
expect EFAULT chflags DEADCODE UF_IMMUTABLE
