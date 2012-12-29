# $MidnightBSD: src/sys/boot/Makefile,v 1.4 2008/09/21 15:21:31 laffer1 Exp $
# $FreeBSD: src/sys/boot/Makefile,v 1.32 2006/11/05 22:03:02 marcel Exp $

.include <bsd.own.mk>
.include <bsd.arch.inc.mk>

.if ${MK_FORTH} != "no"
# Build the add-in FORTH interpreter.
SUBDIR+=		ficl
.endif

# Pick the machine-dependent subdir based on the target architecture.
ADIR=			${MACHINE:S/amd64/i386/:S/sun4v/sparc64/}
.if exists(${.CURDIR}/${ADIR}/.)
SUBDIR+=		${ADIR}
.endif

.include <bsd.subdir.mk>
