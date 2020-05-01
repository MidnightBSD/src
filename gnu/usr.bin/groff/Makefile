# $FreeBSD: stable/11/gnu/usr.bin/groff/Makefile 289393 2015-10-15 22:55:08Z bdrewery $

SUBDIR=		contrib font man src tmac

.for subdir in ${SUBDIR:Nsrc}
SUBDIR_DEPEND_${subdir}=	src
.endfor

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
