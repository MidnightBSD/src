# $FreeBSD: stable/11/sbin/pflogd/Makefile 319279 2017-05-31 05:20:59Z delphij $

.PATH:	${SRCTOP}/contrib/pf/pflogd

PACKAGE=pf
PROG=	pflogd
SRCS= 	pflogd.c pidfile.c privsep.c privsep_fdpass.c
MAN=	pflogd.8

CFLAGS+=-include ${SRCTOP}/lib/libpcap/config.h
CFLAGS+=-I${.CURDIR}/../../contrib/libpcap

LIBADD=	pcap

WARNS?=	2

.include <bsd.prog.mk>
