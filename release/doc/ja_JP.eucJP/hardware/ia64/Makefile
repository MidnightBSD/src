# $FreeBSD: stable/9/release/doc/ja_JP.eucJP/hardware/ia64/Makefile 241122 2012-10-02 06:37:46Z gabor $
# Original revision: 1.3

RELN_ROOT?= ${.CURDIR}/../../..

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=

JADEFLAGS+=	-V %generate-article-toc%

# SGML content
SRCS+=	article.xml 
# SRCS+=	proc-ia64.xml 
SRCS+=	../common/hw.ent
SRCS+=	../common/artheader.xml
SRCS+=	../common/dev.xml
SRCS+=	${DEV-AUTODIR}/catalog-auto
SRCS+=	${DEV-AUTODIR}/dev-auto.xml
SRCS+=	../../share/xml/dev-auto-ja.xml

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"
