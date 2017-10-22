#!/bin/sh

# $FreeBSD: stable/9/tools/regression/usr.bin/make/sysmk/t0/2/1/test.t 201526 2010-01-04 18:57:22Z obrien $

cd `dirname $0`
. ../../../../common.sh

# Description
DESC="Can we traverse up to / and find a 'mk/sys.mk'?"

# Run
TEST_N=1
TEST_1="-m .../mk"
TEST_MAKE_DIRS="../../mk 755"
TEST_COPY_FILES="../../mk/sys.mk 644"

eval_cmd $*
