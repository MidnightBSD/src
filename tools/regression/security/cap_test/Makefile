# $FreeBSD: release/10.0.0/tools/regression/security/cap_test/Makefile 224989 2011-08-18 23:15:04Z jonathan $

PROG=	cap_test
SRCS=	cap_test.c \
	cap_test_capmode.c \
	cap_test_capabilities.c \
	cap_test_fcntl.c \
	cap_test_pdfork.c \
	cap_test_pdkill.c \
	cap_test_relative.c \
	cap_test_sysctl.c \

WARNS=	3
NO_MAN=

# Use headers and libc from the build, if available.
KERNCONF?=	GENERIC
OBJROOT=	${.OBJDIR}/../../../../
OBJKERN=	${OBJROOT}/sys/${KERNCONF}

SRCROOT=	${.CURDIR}/../../../../

CFLAGS+=	-DMACHINE=\"${MACHINE}\" -I${OBJKERN} -I${SRCROOT}/sys
LDFLAGS+=	-L${OBJROOT}/lib/libc -lc

.include <bsd.prog.mk>
