#!/bin/sh

# $FreeBSD: release/10.0.0/tools/regression/usr.bin/make/syntax/funny-targets/test.t 238143 2012-07-05 18:23:36Z obrien $

cd `dirname $0`
. ../../common.sh

# Description
DESC='Target names with "funny" embeded characters.'

# Run
TEST_N=2

eval_cmd $*
