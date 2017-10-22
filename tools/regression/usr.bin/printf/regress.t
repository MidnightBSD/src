#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/usr.bin/printf/regress.t 145028 2005-04-13 20:08:17Z stefanf $

cd `dirname $0`

m4 ../regress.m4 regress.sh | sh
