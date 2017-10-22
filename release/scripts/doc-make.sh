#!/bin/sh
#
# $FreeBSD: release/7.0.0/release/scripts/doc-make.sh 95327 2002-04-23 22:16:41Z obrien $
#

# Create the doc dist.
if [ -d ${RD}/trees/base/usr/share/doc ]; then
	( cd ${RD}/trees/base/usr/share/doc;
	find . | cpio -dumpl ${RD}/trees/doc/usr/share/doc ) &&
	rm -rf ${RD}/trees/base/usr/share/doc
fi
