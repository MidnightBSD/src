#!/bin/sh

# $FreeBSD: release/7.0.0/tools/regression/usr.bin/make/basic/t1/test.t 146822 2005-05-31 14:13:07Z harti $

cd `dirname $0`
. ../../common.sh

# Description
DESC="A Makefile file with only a 'all:' file dependency specification."

# Run
TEST_N=1
TEST_1=

eval_cmd $*
