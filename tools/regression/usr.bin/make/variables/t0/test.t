#!/bin/sh

# $FreeBSD: release/10.0.0/tools/regression/usr.bin/make/variables/t0/test.t 146822 2005-05-31 14:13:07Z harti $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Variable expansion."

eval_cmd $*
