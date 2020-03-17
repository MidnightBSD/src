# $FreeBSD: stable/11/sys/modules/dtrace/Makefile.inc 285703 2015-07-19 22:14:09Z markj $

IGNORE_PRAGMA=	1

load	:
	-kldload dtrace
.if ${MACHINE_CPUARCH} == "i386"
	-kldload sdt
	-kldload fbt
	-kldload prototype
.endif
	-kldload profile
	-kldload systrace
	kldstat

unload	:
	-kldunload systrace
	-kldunload profile
.if ${MACHINE_CPUARCH} == "i386"
	-kldunload prototype
	-kldunload fbt
	-kldunload sdt
.endif
	-kldunload dtrace
	kldstat
