#! /bin/sh
# $FreeBSD: release/10.0.0/tools/regression/usr.bin/make/syntax/directive-t0/test.t 201478 2010-01-04 09:49:23Z obrien $

cd `dirname $0`
. ../../common.sh

# Description
DESC="A typo'ed directive."

# Run
TEST_N=1
TEST_1=

eval_cmd $*
