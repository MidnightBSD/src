#!/bin/sh
#
# $FreeBSD: release/7.0.0/release/scripts/lib32-make.sh 147425 2005-06-16 18:16:14Z ru $
#

# Clean the dust.
cd ${RD}/trees/lib32 && \
    find . ! -path '*/libexec/*' ! -path '*/usr/lib32/*' -delete
