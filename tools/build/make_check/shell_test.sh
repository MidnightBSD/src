#!/bin/sh
# $FreeBSD: stable/11/tools/build/make_check/shell_test.sh 143032 2005-03-02 12:33:23Z harti $
echo $@
if ! test -t 0 ; then
	cat
fi
