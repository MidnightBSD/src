# $FreeBSD: stable/11/stand/mips/Makefile 330123 2018-02-28 18:31:35Z kevans $

NO_OBJ=t

SUBDIR= 	uboot

#
# The BERI boot loader port works only on 64-bit MIPS; not a hard port to
# 32-bit if someone is interested.  Build on all 64-bit MIPS platforms to
# ensure it gets adequate build-test coverage.
#
.if ${MACHINE_ARCH} == "mips64"
SUBDIR+=	beri
.endif

.include <bsd.subdir.mk>
