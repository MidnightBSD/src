# $MidnightBSD: src/sys/Makefile,v 1.5 2008/09/16 00:36:47 laffer1 Exp $
# $FreeBSD: src/sys/Makefile,v 1.33 2005/03/08 00:09:41 grog Exp $

.include <bsd.own.mk>

# The boot loader
.if ${MK_BOOT} != "no"
SUBDIR=	boot
.endif

# Directories to include in cscope name file and TAGS.
CSCOPEDIRS=	compat conf contrib crypto ddb dev fs gnu i4b isa \
		isofs kern libkern modules net netatalk netatm netgraph \
		netinet netinet6 netipx netkey netnatm netncp netsmb nfs \
		pccard pci posix4 sys ufs vm ${ARCHDIR}

ARCHDIR	?=	${MACHINE}

# Loadable kernel modules

.if defined(MODULES_WITH_WORLD)
SUBDIR+=modules
.endif

HTAGSFLAGS+= -at `awk -F= '/^RELEASE *=/{release=$2}; END {print "MidnightBSD", release, "kernel"}' < conf/newvers.sh`

cscope:	${.CURDIR}/cscopenamefile
	cd ${.CURDIR}; cscope -k -p4 -i cscopenamefile

${.CURDIR}/cscopenamefile: 
	cd ${.CURDIR}; find ${CSCOPEDIRS} -name "*.[csh]" > ${.TARGET}

TAGS ${.CURDIR}/TAGS:	${.CURDIR}/cscopenamefile
	rm -f ${.CURDIR}/TAGS
	cd ${.CURDIR}; xargs etags -a < ${.CURDIR}/cscopenamefile

.include <bsd.subdir.mk>
