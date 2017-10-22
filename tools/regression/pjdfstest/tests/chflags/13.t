#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/pjdfstest/tests/chflags/13.t 185173 2008-11-22 13:27:15Z pjd $

desc="chflags returns EFAULT if the path argument points outside the process's allocated address space"

dir=`dirname $0`
. ${dir}/../misc.sh

require chflags

echo "1..2"

expect EFAULT chflags NULL UF_NODUMP
expect EFAULT chflags DEADCODE UF_NODUMP
