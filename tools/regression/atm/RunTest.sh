#!/bin/sh
# $FreeBSD: src/tools/regression/atm/RunTest.sh,v 1.3 2004/08/13 09:27:21 harti Exp $

. ./Funcs.sh

#
# Just check the legality of the options and pass them along
#
args=`getopt b:hq $*`
if [ $? -ne 0 ] ; then
	fatal "Usage: $0 [-q] [-b <localbase>]"
fi

usage() {
	msg "Usage: RunTest.sh [-hq] [-b <localbase>]"
	msg "Options:"
	msg " -h		show this info"
	msg " -b <localbase>	localbase if not /usr/local"
	msg " -q		be quite"
	exit 0
}

options=""
set -- $args
for i
do
	case "$i"
	in

	-h)	usage;;
	-b)	options="$options $i $2" ; shift; shift;;
	-q)	options="$options $i" ; shift;;
	--)	shift; break;;
	esac
done

(cd proto_sscop ; sh ./RunTest.sh -u $options)
(cd proto_sscfu ; sh ./RunTest.sh -u $options)
(cd proto_uni ; sh ./RunTest.sh -u $options)
(cd proto_cc ; sh ./RunTest.sh -u $options)

(cd proto_uni ; sh ./RunTest.sh $options)
(cd proto_sscop ; sh ./RunTest.sh $options)
(cd proto_sscfu ; sh ./RunTest.sh $options)
(cd proto_cc ; sh ./RunTest.sh $options)
