#
# $FreeBSD: release/10.0.0/usr.sbin/auditdistd/Makefile 249657 2013-04-19 19:45:00Z ed $
#

OPENBSMDIR=${.CURDIR}/../../contrib/openbsm
.PATH: ${OPENBSMDIR}/bin/auditdistd

# Addition of auditdistd because otherwise generated parse.c can't find
# auditdistd.h.  This seems like a makefile non-feature.
CFLAGS+=-I${OPENBSMDIR} -I${OPENBSMDIR}/bin/auditdistd

NO_WFORMAT=
NO_WMISSING_VARIABLE_DECLARATIONS=

PROG=	auditdistd
SRCS=	auditdistd.c
SRCS+=	parse.y pjdlog.c
SRCS+=	proto.c proto_common.c proto_socketpair.c proto_tcp.c proto_tls.c
SRCS+=	receiver.c
SRCS+=	sandbox.c sender.c subr.c
SRCS+=	token.l trail.c
MAN=	auditdistd.8 auditdistd.conf.5

DPADD=	${LIBL} ${LIBPTHREAD} ${LIBUTIL}
LDADD=	-ll -lpthread -lutil
DPADD+=	${LIBCRYPTO} ${LIBSSL}
LDADD+=	-lcrypto -lssl

YFLAGS+=-v

CLEANFILES=parse.c parse.h parse.output

.include <bsd.prog.mk>
