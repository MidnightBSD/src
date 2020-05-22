#! /bin/sh
# $FreeBSD: stable/11/usr.bin/bmake/tests/syntax/funny-targets/legacy_test.sh 263346 2014-03-19 12:29:20Z jmmv $

. $(dirname $0)/../../common.sh

# Description
DESC='Target names with "funny" embeded characters.'

# Run
TEST_N=2

eval_cmd $*
