# $FreeBSD: stable/9/release/doc/ja_JP.eucJP/relnotes/alpha/Makefile 241122 2012-10-02 06:37:46Z gabor $
# Original revision: 1.6

RELN_ROOT?= ${.CURDIR}/../../..

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=

JADEFLAGS+=	-V %generate-article-toc%

# SGML content
SRCS+=	article.xml
SRCS+=	../common/relnotes.ent
SRCS+=	../common/new.xml

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"
