# $FreeBSD: release/10.0.0/cddl/lib/libuutil/Makefile 204597 2010-03-02 19:04:07Z uqs $

.PATH: ${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libuutil/common
.PATH: ${.CURDIR}/../../../sys/cddl/contrib/opensolaris/common/avl

LIB=	uutil
SRCS=	avl.c \
	uu_alloc.c \
	uu_avl.c \
	uu_dprintf.c \
	uu_ident.c \
	uu_list.c \
	uu_misc.c \
	uu_open.c \
	uu_pname.c \
	uu_strtoint.c

WARNS?=	0
CFLAGS+= -DNATIVE_BUILD
CFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libuutil/common
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/compat/opensolaris
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common
CFLAGS+= -I${.CURDIR}/../../../cddl/compat/opensolaris/include
CFLAGS+= -I${.CURDIR}/../../../cddl/contrib/opensolaris/head

.include <bsd.lib.mk>
