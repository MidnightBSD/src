#!/bin/sh

# $FreeBSD: release/10.0.0/tools/regression/usr.bin/make/basic/t3/test.t 146822 2005-05-31 14:13:07Z harti $

cd `dirname $0`
. ../../common.sh

# Description
DESC="No Makefile file, no command line target."

# Run
TEST_N=1
TEST_1=

eval_cmd $*
