#! /bin/sh -
#
# $MidnightBSD$

exec groff -Tps -l -C ${1+"$@"}
