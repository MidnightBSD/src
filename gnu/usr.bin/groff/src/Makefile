# $FreeBSD: stable/11/gnu/usr.bin/groff/src/Makefile 275866 2014-12-17 19:46:12Z ngie $

SUBDIR=		libs devices preproc roff utils

SUBDIR_PARALLEL=

.for subdir in ${SUBDIR:Nlibs}
SUBDIR_DEPEND_${subdir}=	libs
.endfor

.include <bsd.subdir.mk>
