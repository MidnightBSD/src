#!/bin/sh
# $FreeBSD: stable/9/tools/regression/usr.bin/file2c/regress.t 137587 2004-11-11 19:47:55Z nik $

cd `dirname $0`

m4 ../regress.m4 regress.sh | sh
