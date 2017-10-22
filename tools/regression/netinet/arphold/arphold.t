#!/bin/sh
#
# $FreeBSD: release/10.0.0/tools/regression/netinet/arphold/arphold.t 215207 2010-11-12 22:03:02Z gnn $

make arphold 2>&1 > /dev/null

./arphold 192.168.1.222
