#!/bin/sh
# $FreeBSD: stable/9/tools/regression/usr.bin/calendar/regress.t 170231 2007-06-03 03:29:32Z grog $

cd `dirname $0`

m4 ../regress.m4 regress.sh | sh
