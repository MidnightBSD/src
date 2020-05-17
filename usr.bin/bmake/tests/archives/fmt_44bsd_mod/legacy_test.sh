#! /bin/sh
# $FreeBSD: stable/11/usr.bin/bmake/tests/archives/fmt_44bsd_mod/legacy_test.sh 263346 2014-03-19 12:29:20Z jmmv $

. $(dirname $0)/../../common.sh

# Description
DESC="Archive parsing (modified BSD4.4 format)."

# Setup
TEST_COPY_FILES="libtest.a 644"

# Run
TEST_N=7

eval_cmd $*
