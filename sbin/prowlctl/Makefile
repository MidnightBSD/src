#
# MidnightBSD prowlctl(8) - Service Management Control Utility
#

.include <src.opts.mk>

PACKAGE=	prowld
PROG=		prowlctl
SRCS=		prowlctl.c
MAN=		prowlctl.8

WARNS?=		6

SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
