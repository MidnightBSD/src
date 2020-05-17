#! /bin/sh
# $FreeBSD: stable/11/usr.bin/bmake/tests/execution/ellipsis/legacy_test.sh 263346 2014-03-19 12:29:20Z jmmv $

. $(dirname $0)/../../common.sh

# Description
DESC="Ellipsis command from variable"

# Run
TEST_N=1
TEST_1=

eval_cmd $*
