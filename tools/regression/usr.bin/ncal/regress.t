#!/bin/sh
# $FreeBSD: stable/9/tools/regression/usr.bin/ncal/regress.t 205147 2010-03-14 10:24:03Z edwin $

cd `dirname $0`

m4 ../regress.m4 regress.sh | sh
