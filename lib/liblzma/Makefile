# $MidnightBSD$
# $NetBSD: Makefile,v 1.3 2010/11/09 23:07:09 joerg Exp $

LIB=		lzma
USE_SHLIBDIR=	yes
NO_LINT=	yes
WARNS=		3

XZSRCDIR=	${.CURDIR}/../../contrib/xz

.include <bsd.init.mk>

# There are two different lzma.h files.
# Copy public version to distinguish them.
pub-lzma.h:	${XZSRCDIR}/src/liblzma/api/lzma.h
	rm -f ${.TARGET}
	cat ${XZSRCDIR}/src/liblzma/api/lzma.h > ${.TARGET}
CLEANFILES+=	pub-lzma.h
INCS=	pub-lzma.h
INCSDIR_pub-lzma.h=	/usr/include
INCSNAME_pub-lzma.h=	lzma.h

.PATH:	${XZSRCDIR}/src/liblzma/api/lzma
INCS+=	base.h bcj.h block.h check.h container.h delta.h \
	filter.h hardware.h index.h index_hash.h lzma.h stream_flags.h \
	version.h vli.h

INCSDIR=	/usr/include/lzma

LDFLAGS+=	-Wl,-z,defs

CFLAGS+=	-I${XZSRCDIR}
CFLAGS+=	-I${XZSRCDIR}/src/liblzma/check
CFLAGS+=	-I${XZSRCDIR}/src/liblzma/api
CFLAGS+=	-I${XZSRCDIR}/src/liblzma/common
CFLAGS+=	-I${XZSRCDIR}/src/liblzma/delta
CFLAGS+=	-I${XZSRCDIR}/src/liblzma/lz
CFLAGS+=	-I${XZSRCDIR}/src/liblzma/lzma
CFLAGS+=	-I${XZSRCDIR}/src/liblzma/rangecoder
CFLAGS+=	-I${XZSRCDIR}/src/liblzma/simple
CFLAGS+=	-I${XZSRCDIR}/src/common -I.
CFLAGS+=	-DTUKLIB_SYMBOL_PREFIX=lzma_
CFLAGS+=	-DHAVE_CONFIG_H
CFLAGS+=	-std=c99

.PATH:	${XZSRCDIR}/src/common
SRCS+=	tuklib_physmem.c

.PATH:	${XZSRCDIR}/src/liblzma/check
SRCS+=	check.c crc32_table.c crc64_table.c
SRCS+=	sha256.c
.if defined(MACHINE_ARCH) && ${MACHINE_ARCH} == "i386"
SRCS+=	crc32_x86.S \
	crc64_x86.S
.else
SRCS+=	crc32_fast.c \
	crc64_fast.c
.endif


.PATH:	${XZSRCDIR}/src/liblzma/common
SRCS+=	common.c block_util.c easy_preset.c filter_common.c \
	hardware_physmem.c index.c \
	stream_flags_common.c vli_size.c \
	alone_encoder.c block_buffer_encoder.c block_encoder.c \
	block_header_encoder.c easy_buffer_encoder.c easy_encoder.c \
	easy_encoder_memusage.c filter_buffer_encoder.c \
	filter_encoder.c filter_flags_encoder.c index_encoder.c \
	stream_buffer_encoder.c stream_encoder.c stream_flags_encoder.c \
	vli_encoder.c \
	alone_decoder.c auto_decoder.c block_buffer_decoder.c \
	block_decoder.c block_header_decoder.c easy_decoder_memusage.c \
	filter_buffer_decoder.c filter_decoder.c filter_flags_decoder.c \
	index_decoder.c index_hash.c stream_buffer_decoder.c \
	stream_decoder.c stream_flags_decoder.c vli_decoder.c

.PATH:	${XZSRCDIR}/src/liblzma/delta
SRCS+=	delta_common.c delta_encoder.c delta_decoder.c

.PATH:	${XZSRCDIR}/src/liblzma/lz
SRCS+=	lz_decoder.c lz_encoder.c lz_encoder_mf.c

.PATH:	${XZSRCDIR}/src/liblzma/lzma
SRCS+=	lzma_encoder.c lzma_encoder_presets.c \
	lzma_encoder_optimum_fast.c lzma_encoder_optimum_normal.c \
	lzma_decoder.c lzma2_encoder.c lzma2_decoder.c \
	fastpos_table.c

.PATH:	${XZSRCDIR}/src/liblzma/rangecoder
SRCS+=	price_table.c

.PATH:	${XZSRCDIR}/src/liblzma/simple
SRCS+=	simple_coder.c simple_encoder.c simple_decoder.c \
	arm.c armthumb.c ia64.c powerpc.c sparc.c x86.c

liblzma.pc:	${XZSRCDIR}/src/liblzma/liblzma.pc.in
	${_MKTARGET_CREATE}
	rm -f ${.TARGET}
	${TOOL_SED} \
	    -e 's,@prefix@,/usr,' \
	    -e 's,@exec_prefix@,/usr,' \
	    -e 's,@libdir@,/usr/lib,' \
	    -e 's,@includedir@,/usr/include,' \
	    -e 's,@PACKAGE_HOMEPAGE@,${XZHOMEPAGE:Q},' \
	    -e 's,@PACKAGE_VERSION@,${XZVERSION:Q},' \
	    -e 's,@PTHREAD_CFLAGS@ @PTHREAD_LIBS@,,' \
	    < ${XZSRCDIR}/src/liblzma/liblzma.pc.in \
	    > ${.TARGET}.tmp && \
	mv -f ${.TARGET}.tmp ${.TARGET}

CLEANFILES+=	liblzma.pc

VERSION_DEF=    ${.CURDIR}/Versions.def
SYMBOL_MAPS=    ${.CURDIR}/Symbol.map
CFLAGS+=        -DSYMBOL_VERSIONING

.include <bsd.lib.mk>
