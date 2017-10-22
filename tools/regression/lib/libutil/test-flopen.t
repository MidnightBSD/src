#!/bin/sh
#
# $FreeBSD: release/10.0.0/tools/regression/lib/libutil/test-flopen.t 171707 2007-08-03 11:29:49Z des $
#

base=$(realpath $(dirname $0))
name=$(basename $0 .t)

set -e
cd $base
make -s $name >/dev/null
exec $base/$name
