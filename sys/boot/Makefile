# $MidnightBSD$
# $FreeBSD: src/sys/boot/Makefile,v 1.32 2006/11/05 22:03:02 marcel Exp $

.include <bsd.own.mk>
.include <bsd.arch.inc.mk>

.if ${MK_FORTH} != "no"
# Build the add-in FORTH interpreter.
SUBDIR+=		ficl
.endif

# Pick the machine-dependent subdir based on the target architecture.
ADIR=			${MACHINE:S/amd64/i386/}
.if exists(${.CURDIR}/${ADIR}/.)
SUBDIR+=		${ADIR}
.endif

.include <bsd.subdir.mk>
