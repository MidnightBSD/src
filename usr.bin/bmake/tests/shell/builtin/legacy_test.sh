#! /bin/sh
# $FreeBSD: stable/11/usr.bin/bmake/tests/shell/builtin/legacy_test.sh 263346 2014-03-19 12:29:20Z jmmv $

. $(dirname $0)/../../common.sh

# Description
DESC="Check that a command line with a builtin is passed to the shell."

# Setup
TEST_COPY_FILES="sh 755"

# Run
TEST_N=2
TEST_1="-B no-builtin"
TEST_2="-B builtin"

eval_cmd $*
