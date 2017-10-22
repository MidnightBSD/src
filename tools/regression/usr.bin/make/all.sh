#!/bin/sh
# $FreeBSD: release/7.0.0/tools/regression/usr.bin/make/all.sh 145620 2005-04-28 13:20:48Z harti $

# find all test scripts below our current directory
SCRIPTS=`find . -name test.t`

if [ -z "${SCRIPTS}" ] ; then
	exit 0
fi

for i in ${SCRIPTS} ; do
	(
	cd `dirname $i`
	sh ./test.t $1
	)
done
