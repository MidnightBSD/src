#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/usr.bin/comm/regress.t 200442 2009-12-12 18:18:46Z jh $

cd `dirname $0`

m4 ../regress.m4 regress.sh | sh
