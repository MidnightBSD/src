# $FreeBSD: release/10.0.0/release/doc/en_US.ISO8859-1/installation/Makefile 259805 2013-12-24 03:57:25Z gjb $

RELN_ROOT?= ${.CURDIR}/../..
.ifdef NO_LANGCODE_IN_DESTDIR
DESTDIR?= ${DOCDIR}/installation
.else
DESTDIR?= ${DOCDIR}/en_US.ISO8859-1/installation
.endif

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=

# SGML content
SRCS+=	article.xml

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"
