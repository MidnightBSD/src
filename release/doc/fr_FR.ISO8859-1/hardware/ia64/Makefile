#
#      The FreeBSD Documentation Project
#      The FreeBSD French Documentation Project
#
#      $Id: Makefile,v 1.1.1.2 2006-02-25 02:35:16 laffer1 Exp $
#      $FreeBSD: src/release/doc/fr_FR.ISO8859-1/hardware/ia64/Makefile,v 1.1 2002/09/09 22:09:47 gioria Exp $
#      Original revision: 1.2

RELN_ROOT?= ${.CURDIR}/../../..

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=

JADEFLAGS+=	-V %generate-article-toc%

# SGML content
SRCS+=	article.sgml 
# SRCS+=	proc-ia64.sgml 
SRCS+=	../common/hw.ent
SRCS+=	../common/artheader.sgml
SRCS+=	../common/dev.sgml

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"
