# $MidnightBSD$
# $FreeBSD: stable/10/sys/boot/pc98/pc98boot/Makefile 319025 2017-05-28 01:14:59Z ngie $

FILES=	${BOOT}
CLEANFILES= ${BOOT} ${BOOT}.part

BOOT=	pc98boot

.if exists(${.OBJDIR}/../boot0)
BOOT0=	${.OBJDIR}/../boot0/boot0
.else
BOOT0=	${.CURDIR}/../boot0/boot0
.endif
.if exists(${.OBJDIR}/../boot0.5)
BOOT05=	${.OBJDIR}/../boot0.5/boot0.5
.else
BOOT05=	${.CURDIR}/../boot0.5/boot0.5
.endif

${BOOT}: ${BOOT0} ${BOOT05} ${BOOT}.part
	cat ${BOOT0} ${BOOT}.part ${BOOT05} > ${.TARGET}

${BOOT}.part:
	${DD} if=/dev/zero of=${.TARGET} bs=512 count=1

.include <bsd.prog.mk>
