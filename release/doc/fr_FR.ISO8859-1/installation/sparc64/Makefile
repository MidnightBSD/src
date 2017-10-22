#
#     The FreeBSD Documentation Project
#     The FreeBSD French Documentation Project
#     $Id$
#
#     $FreeBSD: stable/9/release/doc/fr_FR.ISO8859-1/installation/sparc64/Makefile 241122 2012-10-02 06:37:46Z gabor $
#     Original revision: 1.1
#


RELN_ROOT?= ${.CURDIR}/../../..

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=

# SGML content
SRCS+=	article.xml
SRCS+=	install.xml
SRCS+=	../common/artheader.xml
SRCS+=	../common/install.xml
SRCS+=	../common/layout.xml

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"
