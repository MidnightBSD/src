# $MidnightBSD$

CLEANFILES+=	yearistype
CONTRIBDIR=	${.CURDIR}/../../contrib/tzdata/
.PATH:		${CONTRIBDIR}

.if defined(LEAPSECONDS)
LEAPFILE=	-L leapseconds
.else
LEAPFILE=
.endif

TZFILES=	africa antarctica asia australasia etcetera europe \
		factory northamerica southamerica
POSIXRULES=	America/New_York

.if defined(OLDTIMEZONES)
TZFILES+=	backward systemv
.endif

TZFILES:=	${TZFILES:S/^/${CONTRIBDIR}/}

all: yearistype

beforeinstall:
	umask 022; cd ${.CURDIR}; \
	zic -D -d ${DESTDIR}/usr/share/zoneinfo -p ${POSIXRULES} \
	    -u ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${LEAPFILE} -y ${.OBJDIR}/yearistype ${TZFILES}
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${CONTRIBDIR}/zone.tab ${DESTDIR}/usr/share/zoneinfo/

afterinstall:
#
# If the file /var/db/zoneinfo exists, and it is owned by root:wheel,
# and the contents of it exists in /usr/share/zoneinfo, then reinstall
# it.
#
	@if [ -f ${DESTDIR}/var/db/zoneinfo -a -O ${DESTDIR}/var/db/zoneinfo \
	    -a -G ${DESTDIR}/var/db/zoneinfo ]; then \
		zf=$$(cat ${DESTDIR}/var/db/zoneinfo); \
		if [ -f ${DESTDIR}/usr/share/zoneinfo/$${zf} ]; then \
			if [ ! -z "${DESTDIR}" ]; then \
				optC="-C ${DESTDIR}"; \
			fi; \
			echo "Updating /etc/localtime"; \
			tzsetup $${optC} -r; \
		fi; \
	else \
		echo "Run tzsetup(8) manually to update /etc/localtime."; \
	fi

.include <bsd.prog.mk>
