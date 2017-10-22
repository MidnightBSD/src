#!/bin/sh
# $FreeBSD: stable/9/tools/regression/bin/sh/regress.t 213903 2010-10-15 20:01:35Z jilles $

export SH="${SH:-sh}"

cd `dirname $0`

${SH} regress.sh
