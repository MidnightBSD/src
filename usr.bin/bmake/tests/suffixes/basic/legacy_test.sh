#! /bin/sh
# $FreeBSD: stable/11/usr.bin/bmake/tests/suffixes/basic/legacy_test.sh 263346 2014-03-19 12:29:20Z jmmv $

. $(dirname $0)/../../common.sh

# Description
DESC="Basic suffix operation."

# Setup
TEST_COPY_FILES="TEST1.a 644"

# Reset
TEST_CLEAN="Test1.b"

# Run
TEST_N=1
TEST_1="-r test1"

eval_cmd $*
