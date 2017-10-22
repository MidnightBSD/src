#	$FreeBSD: stable/9/usr.sbin/makefs/compat/Makefile.inc 214921 2010-11-07 16:05:04Z cognet $
#

.PATH:	${.CURDIR}/compat

CFLAGS+=	-I${.CURDIR}/compat

SRCS+=	pwcache.c strsuftoll.c
