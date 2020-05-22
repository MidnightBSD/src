#! /bin/sh
# $FreeBSD: stable/11/usr.bin/bmake/tests/syntax/semi/legacy_test.sh 263346 2014-03-19 12:29:20Z jmmv $

. $(dirname $0)/../../common.sh

# Description
DESC="Test semicolon handling."

# Run
TEST_N=2
TEST_1_TODO="parser bug"

eval_cmd $*
