#!/bin/sh
#
# MidnightBSD-specific changes from upstream, applied after a vendor
# merge.  Derived from FreeBSD's freebsd-changes.sh.
#

# Remove Debian port
rm -fr port/debian

# /libexec -> /usr/libexec
sed -i '' -e 's| /libexec| /usr/libexec|g' bin/blocklistd.8
sed -i '' -e 's|"/libexec|"/usr/libexec|g' bin/internal.h

# NetBSD: RT_ROUNDUP -> MidnightBSD: SA_SIZE (from net/route.h)
sed -i '' -e 's/RT_ROUNDUP/SA_SIZE/g' bin/conf.c

# npfctl(8) -> ipf(8), ipfw(8), pfctl(8)
sed -i '' -e 's/npfctl 8 ,/ipf 8 ,\
.Xr ipfw 8 ,\
.Xr pfctl 8 ,/g' bin/blocklistd.8
