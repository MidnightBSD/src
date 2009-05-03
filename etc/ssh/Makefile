# $MidnightBSD: src/etc/ssh/Makefile,v 1.3 2008/05/30 22:50:50 laffer1 Exp $

NO_OBJ=

FILESDIR=      /etc/ssh
MYFILES=	${.CURDIR}/../../crypto/openssh/ssh_config \
	${.CURDIR}/../../crypto/openssh/sshd_config \
	${.CURDIR}/../../crypto/openssh/moduli 
FILESOWN=	${BINOWN}
FILESGRP=	${BINGRP}

BLACKLISTS!=	echo ${.CURDIR}/../../crypto/openssh/blacklist.*
.for bl in ${BLACKLISTS}
FILES+=	short.${bl:T}
CLEANFILES+= short.${bl:T}
FILESNAME_short.${bl:T}=	${bl:T}

short.${bl:T}: ${bl}
	@/usr/bin/printf "# After these initial comments, all the lines in this file must be\n" > ${.TARGET}
	@/usr/bin/printf "# exactly the same length, and must be in sorted order. Each line must\n" >> ${.TARGET}
	@/usr/bin/printf "# consist of the lower-case key fingerprint (ssh-keygen -l -f\n" >> ${.TARGET}
	@/usr/bin/printf "# /path/to/key), without colons, and with the first 12 characters\n" >> ${.TARGET}
	@/usr/bin/printf "# removed (that is, the lower 80 bits of the fingerprint). Unless these\n" >> ${.TARGET}
	@/usr/bin/printf "# rules are followed, the blacklist will not work properly. See\n" >> ${.TARGET}
	@/usr/bin/printf "# ssh-vulnkey(1).\n" >> ${.TARGET}
	@/usr/bin/printf "#\n" >> ${.TARGET}
	@/usr/bin/printf "# Copyright: (C) 2008 Canonical, Ltd., License: GPL-3.\n" >> ${.TARGET}
	sed -ne '/^#/!q;p;' ${.ALLSRC} >> ${.TARGET}

	grep -v '^#' ${.ALLSRC} | /usr/bin/cut -d ' ' -f 2 | /usr/bin/cut -b13- | /usr/bin/sort >> ${.TARGET}
.endfor

distribution:
.for me in ${MYFILES}
	cp ${me} ${DESTDIR}${FILESDIR}
.endfor

.include <bsd.prog.mk>
