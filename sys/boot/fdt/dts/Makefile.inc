# $FreeBSD: stable/11/sys/boot/fdt/dts/Makefile.inc 267199 2014-06-07 02:38:13Z rpaulo $

SYSDIR?=${.CURDIR}/../../../..

test-dts:
.for dts in ${DTS}
	@env MACHINE=`basename ${.CURDIR}` ${SYSDIR}/tools/fdt/make_dtb.sh ${SYSDIR} ${dts} /tmp

.endfor

