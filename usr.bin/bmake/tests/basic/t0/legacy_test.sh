#! /bin/sh
# $FreeBSD: stable/11/usr.bin/bmake/tests/basic/t0/legacy_test.sh 263346 2014-03-19 12:29:20Z jmmv $

. $(dirname $0)/../../common.sh

# Description
DESC="An empty Makefile file and no target given."

# Setup
TEST_TOUCH="Makefile ''"

# Run
TEST_N=1
TEST_1=

eval_cmd $*
