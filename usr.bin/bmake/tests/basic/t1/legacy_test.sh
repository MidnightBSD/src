#! /bin/sh
# $FreeBSD: stable/10/usr.bin/bmake/tests/basic/t1/legacy_test.sh 263346 2014-03-19 12:29:20Z jmmv $

. $(dirname $0)/../../common.sh

# Description
DESC="A Makefile file with only a 'all:' file dependency specification."

# Run
TEST_N=1
TEST_1=

eval_cmd $*
