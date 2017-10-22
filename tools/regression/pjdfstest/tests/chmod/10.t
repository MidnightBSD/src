#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/pjdfstest/tests/chmod/10.t 211336 2010-08-15 14:44:48Z pjd $

desc="chmod returns EFAULT if the path argument points outside the process's allocated address space"

dir=`dirname $0`
. ${dir}/../misc.sh

if supported lchmod; then
	echo "1..4"
else
	echo "1..2"
fi

expect EFAULT chmod NULL 0644
expect EFAULT chmod DEADCODE 0644
if supported lchmod; then
	expect EFAULT lchmod NULL 0644
	expect EFAULT lchmod DEADCODE 0644
fi
