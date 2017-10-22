# $FreeBSD: release/10.0.0/release/doc/share/xml/Makefile 250746 2013-05-17 18:53:29Z gabor $

RELN_ROOT?= ${.CURDIR}/../..

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

dev-auto.ent: ${MAN4PAGES} ${ARCHLIST} ${MAN2HWNOTES_CMD}
	${PERL} ${MAN2HWNOTES_CMD} ${MAN2HWNOTES_FLAGS} -a ${ARCHLIST} -o ${.TARGET} ${MAN4PAGES}

all: dev-auto.ent
