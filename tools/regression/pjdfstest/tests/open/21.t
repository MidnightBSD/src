#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/pjdfstest/tests/open/21.t 166065 2007-01-17 01:42:12Z pjd $

desc="open returns EFAULT if the path argument points outside the process's allocated address space"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..2"

expect EFAULT open NULL O_RDONLY
expect EFAULT open DEADCODE O_RDONLY
