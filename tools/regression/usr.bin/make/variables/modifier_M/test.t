#!/bin/sh

# $FreeBSD: release/10.0.0/tools/regression/usr.bin/make/variables/modifier_M/test.t 146822 2005-05-31 14:13:07Z harti $

cd `dirname $0`
. ../../common.sh

# Description
DESC="Variable expansion with M modifier"

eval_cmd $*
