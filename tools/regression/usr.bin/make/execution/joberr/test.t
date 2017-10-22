#!/bin/sh

# $FreeBSD: release/10.0.0/tools/regression/usr.bin/make/execution/joberr/test.t 228523 2011-12-15 06:01:06Z fjoe $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Test job make error output"

# Run
TEST_N=1
TEST_1=

eval_cmd $*
