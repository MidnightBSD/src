#!/bin/sh

# $FreeBSD: release/10.0.0/tools/regression/usr.bin/make/execution/empty/test.t 228149 2011-11-30 05:49:17Z fjoe $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Empty command (from variable)"

# Run
TEST_N=1
TEST_1=

eval_cmd $*
