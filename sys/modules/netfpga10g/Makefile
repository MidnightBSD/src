#
# $FreeBSD: stable/11/sys/modules/netfpga10g/Makefile 264601 2014-04-17 12:33:26Z bz $
#

SUBDIR=
SUBDIR+=	${_nf10bmac}

.if ${MACHINE_CPUARCH} == "mips"
_nf10bmac=	nf10bmac
.endif

.include <bsd.subdir.mk>
