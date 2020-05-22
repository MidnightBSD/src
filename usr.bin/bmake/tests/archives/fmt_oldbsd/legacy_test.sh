#! /bin/sh
# $FreeBSD: stable/11/usr.bin/bmake/tests/archives/fmt_oldbsd/legacy_test.sh 263346 2014-03-19 12:29:20Z jmmv $

. $(dirname $0)/../../common.sh

# Description
DESC="Archive parsing (old BSD format)."

# Setup
TEST_COPY_FILES="libtest.a 644"

# Run
TEST_N=7

eval_cmd $*
