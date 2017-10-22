# $FreeBSD: release/10.0.0/sys/modules/ata/Makefile 249083 2013-04-04 07:12:24Z mav $

SUBDIR = atacore
SUBDIR += atacard
.if ${MACHINE} == "pc98"
SUBDIR += atacbus
.else
SUBDIR += ataisa
.endif
SUBDIR += atapci 

.include <bsd.subdir.mk>
