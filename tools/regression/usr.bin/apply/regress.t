#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/usr.bin/apply/regress.t 204761 2010-03-05 15:23:01Z jh $

cd `dirname $0`

m4 ../regress.m4 regress.sh | sh
