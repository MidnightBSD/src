#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/geom_gate/runtests.sh 128883 2004-05-03 18:29:54Z pjd $

dir=`dirname $0`

for ts in `dirname $0`/test-*.sh; do
	sh $ts
done
