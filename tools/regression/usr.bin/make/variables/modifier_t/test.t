#!/bin/sh

# $FreeBSD: release/10.0.0/tools/regression/usr.bin/make/variables/modifier_t/test.t 236977 2012-06-12 23:16:00Z obrien $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Variable expansion with t modifiers"

# Run
TEST_N=3

eval_cmd $*
