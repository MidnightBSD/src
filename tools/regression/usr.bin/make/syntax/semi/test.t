#!/bin/sh

# $FreeBSD: release/10.0.0/tools/regression/usr.bin/make/syntax/semi/test.t 151443 2005-10-18 07:28:09Z harti $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Test semicolon handling."

# Run
TEST_N=2
TEST_1_TODO="parser bug"

eval_cmd $*
