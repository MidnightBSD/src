#!/bin/sh

# $FreeBSD: release/10.0.0/tools/regression/usr.bin/make/execution/plus/test.t 228149 2011-11-30 05:49:17Z fjoe $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Test '+command' execution with -n -jX"

# Run
TEST_N=1
TEST_1=

eval_cmd $*
