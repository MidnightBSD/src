# ----------------------------------------------------------------------------
# "THE BEER-WARE LICENSE" (Revision 42):
# <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
# can do whatever you want with this stuff. If we meet some day, and you think
# this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
# ----------------------------------------------------------------------------
#

PROG=	ctm
MAN=	ctm.1 ctm.5
SRCS=  	ctm.c ctm_input.c ctm_pass1.c ctm_pass2.c ctm_pass3.c \
	ctm_passb.c ctm_syntax.c ctm_ed.c

NOTYET=	ctm_ed.c

LIBADD=	md

WARNS?=	2

.if exists(${.CURDIR:H:H}/Makefile.inc)
.include "${.CURDIR:H:H}/Makefile.inc"
.endif
.include <bsd.prog.mk>
