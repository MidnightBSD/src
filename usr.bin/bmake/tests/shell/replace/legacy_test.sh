#! /bin/sh
# $FreeBSD: stable/11/usr.bin/bmake/tests/shell/replace/legacy_test.sh 263346 2014-03-19 12:29:20Z jmmv $

. $(dirname $0)/../../common.sh

# Description
DESC="Check that the shell can be replaced."

# Setup
TEST_COPY_FILES="shell 755"

# Run
TEST_N=2
TEST_1=
TEST_2=-j2

eval_cmd $*
