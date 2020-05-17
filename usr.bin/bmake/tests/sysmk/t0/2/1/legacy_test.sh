#! /bin/sh
# $FreeBSD: stable/11/usr.bin/bmake/tests/sysmk/t0/2/1/legacy_test.sh 263346 2014-03-19 12:29:20Z jmmv $

. $(dirname $0)/../../../../common.sh

# Description
DESC="Can we traverse up to / and find a 'mk/sys.mk'?"

# Run
TEST_N=1
TEST_1="-m .../mk"
TEST_MAKE_DIRS="../../mk 755"
TEST_COPY_FILES="../../mk/sys.mk 644"

eval_cmd $*
