.include <src.opts.mk>

_LIBARCHIVEDIR=	${SRCTOP}/contrib/libarchive

PACKAGE=	runtime
PROG=	bsdtar
BSDTAR_VERSION_STRING!=	sed -n '/define.*ARCHIVE_VERSION_ONLY_STRING/{s,[^0-9.],,gp;q;}' \
			${_LIBARCHIVEDIR}/libarchive/archive.h

.PATH: ${_LIBARCHIVEDIR}/tar
SRCS=	bsdtar.c	\
	cmdline.c	\
	creation_set.c	\
	read.c		\
	subst.c		\
	util.c		\
	write.c

.PATH: ${_LIBARCHIVEDIR}/libarchive_fe
SRCS+=	err.c		\
	line_reader.c	\
	passphrase.c

LIBADD=	archive

.if ${MK_ICONV} != "no"
CFLAGS+=	-DHAVE_ICONV=1 -DHAVE_ICONV_H=1 -DICONV_CONST=const
.endif

CFLAGS+=	-DBSDTAR_VERSION_STRING=\"${BSDTAR_VERSION_STRING}\"
CFLAGS+=	-DPLATFORM_CONFIG_H=\"${SRCTOP}/lib/libarchive/config_midnightbsd.h\"
CFLAGS+=	-I${_LIBARCHIVEDIR}/tar -I${_LIBARCHIVEDIR}/libarchive
CFLAGS+=	-I${_LIBARCHIVEDIR}/libarchive_fe
SYMLINKS=	bsdtar ${BINDIR}/tar
MLINKS=	bsdtar.1 tar.1

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
