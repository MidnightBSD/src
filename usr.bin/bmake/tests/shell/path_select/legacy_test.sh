#! /bin/sh
# $FreeBSD: stable/11/usr.bin/bmake/tests/shell/path_select/legacy_test.sh 263346 2014-03-19 12:29:20Z jmmv $

. $(dirname $0)/../../common.sh

# Description
DESC="New path for builtin shells (2)."

# Setup
TEST_COPY_FILES="shell 755"

# Run
TEST_N=3
TEST_1="sh_test"
TEST_2="csh_test"
TEST_3="ksh_test"
TEST_3_SKIP="no ksh on FreeBSD"

eval_cmd $*
