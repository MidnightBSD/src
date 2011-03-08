# $MidnightBSD: src/usr.bin/cpio/Makefile,v 1.2 2011/02/06 22:29:51 laffer1 Exp $

.include <bsd.own.mk>

PROG=	bsdcpio
BSDCPIO_VERSION_STRING=2.8.3
SRCS=	cpio.c cmdline.c err.c line_reader.c matching.c pathmatch.c
CFLAGS+= -DBSDCPIO_VERSION_STRING=\"${BSDCPIO_VERSION_STRING}\"
CFLAGS+= -DPLATFORM_CONFIG_H=\"config_midnightbsd.h\"
.ifdef RELEASE_CRUNCH
# sysinstall uses cpio in crunched binaries that are
# statically linked, cannot use -lcrypto, and are size sensitive.
CFLAGS+= -DSMALLER
.endif
DPADD=	${LIBARCHIVE} ${LIBZ} ${LIBBZ2} ${LIBMD} ${LIBLZMA}
LDADD=	-larchive -lz -lbz2 -lmd -llzma
.if ${MK_OPENSSL} != "no"
DPADD+=	${LIBCRYPTO}
LDADD+= -lcrypto
.endif

SYMLINKS=bsdcpio ${BINDIR}/cpio
MLINKS= bsdcpio.1 cpio.1

.PHONY: check test

check test: $(PROG) bsdcpio.1.gz
	cd ${.CURDIR}/test && make clean test

.include <bsd.prog.mk>
