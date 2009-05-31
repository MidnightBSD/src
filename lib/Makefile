#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/Makefile,v 1.205.2.1 2005/07/28 13:47:18 rwatson Exp $
# $MidnightBSD: src/lib/Makefile,v 1.9 2008/10/31 20:32:46 laffer1 Exp $

.include <bsd.own.mk>

# To satisfy shared library or ELF linkage when only the libraries being
# built are visible:
#
# csu must be built before all shared libaries for ELF.
# libc must be built before all other shared libraries.
# libcom_err must be built before libkrb5 and libpam.
# libcrypt must be built before libkrb5 and libpam.
# libkvm must be built before libdevstat.
# msun must be built before libg++ and libstdc++.
# libmd must be built before libatm, libopie, libradius, and libtacplus.
# ncurses must be built before libdialog, libedit and libreadline.
# libnetgraph must be built before libbsnmp/modules/snmp_netgraph.
# libopie must be built before libpam.
# libradius must be built before libpam.
# librpcsvc must be built before libpam.
# libsbuf must be built before libcam.
# libtacplus must be built before libpam.
# libutil must be built before libpam.
# libypclnt must be built before libpam.
# libkse must be built before libsqlite3
# libbz2 must be built before libarchive
# Otherwise, the SUBDIR list should be in alphabetical order.

SUBDIR=	${_csu} libc libbsm libcom_err libcrypt libelf libkvm msun libmd \
	ncurses libnetgraph libradius librpcsvc libsbuf libtacplus libutil \
	${_libypclnt} libalias libbz2 libarchive ${_libatm} \
	libbegemot ${_libbluetooth} libbsnmp \
	libcalendar libcam libcompat libdevinfo libdevstat libdisk \
	libedit libexpat libfetch libftpio libgeom ${_libgpib} \
	${_libgssapi} libipsec \
	libipx libkiconv libmagic libmemstat ${_libmilter} ${_libmp} \
	libmport ${_libncp} ${_libngatm} libopie libpam libpcap \
	libpmc ${_libkse} librt ${_libsdp} ${_libsm} ${_libsmb} ${_libsmdb} \
	${_libsmutil} libstand libtelnet ${_libthr} libthread_db libufs \
	libugidfw ${_libusbhid} ${_libvgl} libwrap liby libz ${_bind} \
	libsqlite3

.if exists(${.CURDIR}/csu/${MACHINE_ARCH}-elf)
_csu=csu/${MACHINE_ARCH}-elf
.elif exists(${.CURDIR}/csu/${MACHINE_ARCH}/Makefile)
_csu=csu/${MACHINE_ARCH}
.else
_csu=csu
.endif

.if ${MK_ATM} != "no"
# Not MPSAFE
#_libatm=	libatm
_libngatm=	libngatm
.endif

.if ${MK_BIND} != "no"
_bind=		bind
.endif

.if ${MK_BLUETOOTH} != "no"
_libbluetooth=	libbluetooth
_libsdp=	libsdp
.endif

.if ${MK_GSSAPI} != "no"
_libgssapi=	libgssapi
.endif

.if ${MK_IPX} != "no"
_libipx=	libipx
.endif

.if ${MACHINE_ARCH} == "i386"
.if ${MK_NCP} != "no"
_libncp=	libncp
.endif
_libsmb=	libsmb
_libvgl=	libvgl
.endif

.if ${MK_SENDMAIL} != "no"
_libmilter=	libmilter
_libsm=		libsm
_libsmdb=	libsmdb
_libsmutil=	libsmutil
.endif

.if ${MK_OPENSSL} != "no"
_libmp=		libmp
.endif

.if ${MACHINE_ARCH} == "amd64"
.if ${MK_NCP} != "no"
_libncp=	libncp
.endif
_libsmb=	libsmb
.endif

.if ${MK_LIBKSE} != "no"
_libkse=	libkse
.endif

.if ${MK_LIBTHR} != "no"
_libthr=	libthr
.endif

.if ${MK_USB} != "no"
_libusbhid=	libusbhid
.endif

.if ${MK_NIS} != "no"
_libypclnt=	libypclnt
.endif

.if ${MK_GPIB} != "no"
_libgpib=	libgpib
.endif

.include <bsd.subdir.mk>
