# $MidnightBSD$
# $FreeBSD: stable/10/tools/tools/zfsboottest/Makefile 290984 2015-11-17 15:14:00Z avg $

.PATH:		${.CURDIR}/../../../sys/boot/zfs ${.CURDIR}/../../../sys/cddl/boot/zfs

BINDIR?=	/usr/bin
SCRIPTSDIR?=	/usr/bin

PROG=		zfsboottest
SCRIPTS=	zfsboottest.sh
SCRIPTSNAME=	zfsboottest.sh
MAN=

CFLAGS=	-O1 \
	-I${.CURDIR}/../../../sys/boot/zfs \
	-I${.CURDIR}/../../../sys/cddl/boot/zfs \
	-I. \
	-fdiagnostics-show-option \
	-W -Wextra -Wno-sign-compare -Wno-unused-parameter
CFLAGS+=-m32
LDADD+=	-lmd

.if ${MACHINE_CPUARCH} == "amd64"
beforedepend zfsboottest.o: machine
CLEANFILES+=	machine
machine:
	ln -sf ${.CURDIR}/../../../sys/i386/include machine
.endif

.include <bsd.prog.mk>
