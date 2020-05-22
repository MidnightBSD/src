#! /bin/sh
# $FreeBSD: stable/11/usr.bin/bmake/tests/sysmk/t1/2/1/legacy_test.sh 263346 2014-03-19 12:29:20Z jmmv $

. $(dirname $0)/../../../../common.sh

# Description
DESC="Can we traverse up to / and find a 'mk/sys.mk' with -C -m?"

# Run
TEST_N=1
TEST_1="-C ../../../t0/2/1 -m .../mk"
TEST_MAKE_DIRS="../../mk 755 ../../../t0/mk 755 ../../../t0/2/1 755"
TEST_COPY_FILES="../../mk/sys.mk 644 ../../../t0/mk/sys.mk 644 ../../../t0/2/1/Makefile.test 644"
TEST_CLEAN_FILES="../../../t0/2/1"
TEST_CLEANUP=clean-special

eval_cmd $*
