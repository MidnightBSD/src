# $MidnightBSD$
# $FreeBSD: stable/10/usr.sbin/bsdinstall/distextract/Makefile 291819 2015-12-04 19:16:56Z bdrewery $

BINDIR= ${LIBEXECDIR}/bsdinstall
PROG=	distextract
DPADD=	${LIBARCHIVE} ${LIBDPV} ${LIBFIGPAR} ${LIBDIALOG} ${LIBM}
LDADD=	-larchive -ldpv -lfigpar -ldialog -lm

WARNS?=	6
MAN=

.include <bsd.own.mk>

.if ${MK_NCURSESW} == "no"
DPADD+=	${LIBNCURSES}
LDADD+=	-lncurses
.else
DPADD+=	${LIBNCURSESW}
LDADD+=	-lncursesw
.endif

.include <bsd.prog.mk>
