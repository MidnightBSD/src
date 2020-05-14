# $FreeBSD: stable/11/usr.bin/ministat/Makefile 275042 2014-11-25 14:29:10Z bapt $
PROG=	ministat

LIBADD=	m

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} < ${.CURDIR}/chameleon
	./${PROG} ${.CURDIR}/chameleon
	./${PROG} ${.CURDIR}/iguana ${.CURDIR}/chameleon
	./${PROG} -c 80 ${.CURDIR}/iguana ${.CURDIR}/chameleon
	./${PROG} -s -c 80 ${.CURDIR}/chameleon ${.CURDIR}/iguana
	./${PROG} -s -c 80 ${.CURDIR}/chameleon ${.CURDIR}/iguana ${.CURDIR}/iguana
