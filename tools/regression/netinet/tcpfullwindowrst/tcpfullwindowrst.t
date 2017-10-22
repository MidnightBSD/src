#!/bin/sh
#
# $FreeBSD: release/10.0.0/tools/regression/netinet/tcpfullwindowrst/tcpfullwindowrst.t 138269 2004-12-01 12:12:12Z nik $

make tcpfullwindowrsttest 2>&1 > /dev/null

./tcpfullwindowrsttest
