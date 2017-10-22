#!/bin/sh

# $FreeBSD: release/10.0.0/tools/regression/usr.bin/make/archives/fmt_oldbsd/test.t 146822 2005-05-31 14:13:07Z harti $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Archive parsing (old BSD format)."

# Setup
TEST_COPY_FILES="libtest.a 644"

# Run
TEST_N=7

eval_cmd $*
