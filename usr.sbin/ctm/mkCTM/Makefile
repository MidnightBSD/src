# $MidnightBSD$
# $FreeBSD: stable/10/usr.sbin/ctm/mkCTM/Makefile 276486 2014-12-31 23:25:37Z ngie $

PROG=	mkctm
MAN=

DPADD=	${LIBMD}
LDADD=	-lmd

test:	mkctm
	rm -f tst.out*
	time ./mkctm -v -v /3c/210src  /a/r1/usr/src \
		2>a | md5 -p > /a/tst.out 
	ls -l /a/tst.out
	gzip -9 -v /a/tst.out
	ls -l /a/tst.out.gz
	# cd /usr/src/release && ctm -c -v -v  ${.CURDIR}/tst.out

test1:	mkctm
	rm -f tst.out*
	time ./mkctm -v -v /3c/210src  /home/ncvs/src \
		2> b | md5 -p > /a/tst2.out 
	ls -l /a/tst2.out
	gzip -9 -v /a/tst2.out
	ls -l /a/tst2.out.gz

.include <bsd.prog.mk>
