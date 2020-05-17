#! /bin/sh
# $FreeBSD: stable/11/usr.bin/bmake/tests/variables/opt_V/legacy_test.sh 263346 2014-03-19 12:29:20Z jmmv $

. $(dirname $0)/../../common.sh

# Description
DESC="Variable expansion using command line '-V'"

# Run
TEST_N=2

eval_cmd $*
