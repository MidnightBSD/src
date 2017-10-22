# $FreeBSD: release/10.0.0/usr.bin/svn/lib/libsvn_repos/Makefile 251886 2013-06-18 02:53:45Z peter $

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${SVNDIR}/libsvn_repos

INTERNALLIB=	yes
LIB=		svn_repos

SRCS=		authz.c commit.c delta.c deprecated.c dump.c fs-wrap.c \
		hooks.c load-fs-vtable.c load.c log.c node_tree.c notify.c \
		replay.c reporter.c repos.c rev_hunt.c \

CFLAGS+=	-I${SVNDIR}/include -I${SVNDIR} -I${.CURDIR}/../.. \
		-I${.CURDIR}/../libapr \
		-I${APR}/include/arch/unix \
		-I${APR}/include \
		-I${.CURDIR}/../libapr_util \
		-I${APRU}/include/private \
		-I${APRU}/include

.include <bsd.lib.mk>
