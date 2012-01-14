# $MidnightBSD$

.include <bsd.own.mk>

LIBARCHIVEDIR=	${.CURDIR}/../../contrib/libarchive

PROG=	bsdcpio
BSDCPIO_VERSION_STRING=2.8.5

.PATH:	${LIBARCHIVEDIR}/cpio
SRCS=	cpio.c cmdline.c

.PATH:	${LIBARCHIVEDIR}/libarchive_fe
SRCS+=	err.c line_reader.c matching.c pathmatch.c

CFLAGS+= -DBSDCPIO_VERSION_STRING=\"${BSDCPIO_VERSION_STRING}\"
CFLAGS+= -DPLATFORM_CONFIG_H=\"${.CURDIR}/config_midnightbsd.h\"
CFLAGS+= -I${LIBARCHIVEDIR}/cpio -I${LIBARCHIVEDIR}/libarchive_fe

.ifdef RELEASE_CRUNCH
CFLAGS+= -DSMALLER
.endif
DPADD=	${LIBARCHIVE} ${LIBZ} ${LIBBZ2} ${LIBLZMA} ${LIBBSDXML}
LDADD=	-larchive -lz -lbz2 -llzma -lbsdxml
.if ${MK_OPENSSL} != "no"
DPADD+=	${LIBCRYPTO}
LDADD+= -lcrypto
.else
DPADD+= ${LIBMD}
LDADD+= -lmd
.endif

SYMLINKS=bsdcpio ${BINDIR}/cpio
MLINKS= bsdcpio.1 cpio.1

.include <bsd.prog.mk>
