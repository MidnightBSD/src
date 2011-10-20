#	$FreeBSD: src/usr.sbin/makefs/compat/Makefile.inc,v 1.1.4.2 2011/07/26 14:41:54 marius Exp $
#

.PATH:	${.CURDIR}/compat

CFLAGS+=	-I${.CURDIR}/compat

SRCS+=	pwcache.c strsuftoll.c
