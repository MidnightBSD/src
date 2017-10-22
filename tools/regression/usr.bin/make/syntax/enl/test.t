#!/bin/sh

# $FreeBSD: release/10.0.0/tools/regression/usr.bin/make/syntax/enl/test.t 151442 2005-10-18 07:20:14Z harti $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Test escaped new-lines handling."

# Run
TEST_N=5
TEST_2_TODO="bug in parser"

eval_cmd $*
