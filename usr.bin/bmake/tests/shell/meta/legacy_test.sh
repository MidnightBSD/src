#! /bin/sh
# $FreeBSD: stable/11/usr.bin/bmake/tests/shell/meta/legacy_test.sh 263346 2014-03-19 12:29:20Z jmmv $

. $(dirname $0)/../../common.sh

# Description
DESC="Check that a command line with meta characters is passed to the shell."

# Setup
TEST_COPY_FILES="sh 755"

# Run
TEST_N=2
TEST_1="-B no-meta"
TEST_2="-B meta"

eval_cmd $*
