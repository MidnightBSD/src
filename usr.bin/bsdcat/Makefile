
.include <src.opts.mk>

_LIBARCHIVEDIR=	${SRCTOP}/contrib/libarchive
_LIBARCHIVECONFDIR=	${SRCTOP}/lib/libarchive

PROG=	bsdcat
BSDCAT_VERSION_STRING!=	sed -n '/define.*ARCHIVE_VERSION_ONLY_STRING/{s,[^0-9.],,gp;q;}' \
			${_LIBARCHIVEDIR}/libarchive/archive.h

.PATH:	${_LIBARCHIVEDIR}/cat
SRCS=	bsdcat.c cmdline.c

.PATH:	${_LIBARCHIVEDIR}/libarchive_fe
SRCS+=	err.c

CFLAGS+= -DBSDCAT_VERSION_STRING=\"${BSDCAT_VERSION_STRING}\"
CFLAGS+= -DPLATFORM_CONFIG_H=\"${_LIBARCHIVECONFDIR}/config_midnightbsd.h\"
CFLAGS+= -I${_LIBARCHIVEDIR}/cat -I${_LIBARCHIVEDIR}/libarchive_fe

LIBADD=	archive

.if ${MK_ICONV} != "no"
CFLAGS+=	-DHAVE_ICONV=1 -DHAVE_ICONV_H=1 -DICONV_CONST=const
.endif

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
