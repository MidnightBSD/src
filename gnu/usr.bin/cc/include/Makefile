# $MidnightBSD: src/gnu/usr.bin/cc/include/Makefile,v 1.2 2009/05/22 00:51:38 laffer1 Exp $
# $FreeBSD: src/gnu/usr.bin/cc/include/Makefile,v 1.9 2007/05/19 04:25:59 kan Exp $

.include "../Makefile.inc"

INCSDIR=${INCLUDEDIR}/gcc/${GCCVER}

.PATH: ${GCCDIR}/config/${GCC_CPU}

.if ${TARGET_ARCH} == "i386" || ${TARGET_ARCH} == "amd64"
INCS=	emmintrin.h mmintrin.h pmmintrin.h tmmintrin.h xmmintrin.h mm_malloc.h
.endif

mm_malloc.h: pmm_malloc.h
	@rm -rf ${.TARGET}
	@cp ${.ALLSRC} ${.TARGET}
CLEANFILES+=	mm_malloc.h

.include <bsd.init.mk>
.include <bsd.incs.mk>
.include <bsd.obj.mk>
