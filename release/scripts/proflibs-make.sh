#!/bin/sh
#
# $FreeBSD: stable/9/release/scripts/proflibs-make.sh 124661 2004-01-18 09:06:40Z ru $
#

# Move the profiled libraries out to their own dist
for i in ${RD}/trees/base/usr/lib/*_p.a; do
	mv $i ${RD}/trees/proflibs/usr/lib
done
