#!/bin/sh
# $FreeBSD: stable/9/tools/regression/usr.bin/tr/regress.t 175288 2008-01-13 08:33:20Z keramida $

cd `dirname $0`

m4 ../regress.m4 regress.sh | sh
