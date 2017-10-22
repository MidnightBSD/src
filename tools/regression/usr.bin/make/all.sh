#!/bin/sh
# $FreeBSD: stable/9/tools/regression/usr.bin/make/all.sh 237100 2012-06-14 20:44:56Z obrien $

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
