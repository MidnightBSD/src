#!/bin/sh
# $FreeBSD: release/7.0.0/tools/build/make_check/shell_test.sh 143032 2005-03-02 12:33:23Z harti $
echo $@
if ! test -t 0 ; then
	cat
fi
