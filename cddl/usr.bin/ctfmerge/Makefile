# $MidnightBSD$
# $FreeBSD: stable/10/cddl/usr.bin/ctfmerge/Makefile 321270 2017-07-20 01:01:16Z ngie $

.PATH: ${SRCTOP}/cddl/contrib/opensolaris/tools/ctf/common
.PATH: ${SRCTOP}/cddl/contrib/opensolaris/tools/ctf/cvt

PROG=		ctfmerge
SRCS=		alist.c \
		barrier.c \
		ctf.c \
		ctfmerge.c \
		fifo.c \
		hash.c \
		iidesc.c \
		input.c \
		list.c \
		memory.c \
		merge.c \
		output.c \
		strtab.c \
		symbol.c \
		tdata.c \
		traverse.c \
		util.c

WARNS?=		1

CFLAGS+=	-I${SRCTOP}/sys/cddl/compat/opensolaris \
		-I${SRCTOP}/cddl/compat/opensolaris/include \
		-I${OPENSOLARIS_USR_DISTDIR} \
		-I${OPENSOLARIS_SYS_DISTDIR} \
		-I${OPENSOLARIS_USR_DISTDIR}/head \
		-I${OPENSOLARIS_USR_DISTDIR}/tools/ctf/common \
		-I${OPENSOLARIS_USR_DISTDIR}/tools/ctf/cvt \
		-I${OPENSOLARIS_SYS_DISTDIR}/uts/common

DPADD=		${LIBCTF} ${LIBDWARF} ${LIBELF} ${LIBZ} ${LIBPTHREAD}
LDADD=		-lctf -ldwarf -lelf -lz -lpthread

.include <bsd.prog.mk>
