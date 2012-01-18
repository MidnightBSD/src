# $MidnightBSD: src/release/Makefile,v 1.33 2012/01/18 01:24:50 laffer1 Exp $
#
# Makefile for building releases and release media.
# 
# User-driven targets:
#  cdrom: Builds release CD-ROM media (release.iso)
#  memstick: Builds memory stick image (memstick)
#  ftp: Sets up FTP distribution area (ftp)
#  release: Build all media and FTP distribution area
#  install: Copies all release media into ${DESTDIR}
#
# Variables affecting the build process:
#  WORLDDIR: location of src tree -- must have built world and default kernel
#            (by default, the directory above this one) 
#  PORTSDIR: location of mports tree to distribute (default: /usr/mports)
#  NOPORTS:  if set, do not distribute mports tree
#  NOSRC:    if set, do not distribute source tree
#  TARGET/TARGET_ARCH: architecture of built release 

WORLDDIR?=	/usr/src
PORTSDIR?=	/usr/mports

TARGET?=	${MACHINE}
.if ${TARGET} == ${MACHINE}
TARGET_ARCH?=	${MACHINE_ARCH}
.else
TARGET_ARCH?=	${TARGET}
.endif
IMAKE=		${MAKE} TARGET_ARCH=${TARGET_ARCH} TARGET=${TARGET}
DISTDIR=	/usr/rel/dist

.if !exists(${PORTSDIR})
NOPORTS= true
.endif

EXTRA_PACKAGES= 
.if !defined(NOPORTS)
EXTRA_PACKAGES+= mports.txz
.endif
.if !defined(NOSRC)
EXTRA_PACKAGES+= src.txz
.endif

RELEASE_TARGETS= ftp
.if exists(${.CURDIR}/${TARGET}/mkisoimages.sh)
RELEASE_TARGETS+= cdrom
.endif
.if exists(${.CURDIR}/${TARGET}/make-memstick.sh)
RELEASE_TARGETS+= memstick
.endif

.include <bsd.obj.mk>

base.txz:
	-mkdir -p ${DISTDIR}
	cd ${WORLDDIR} && ${IMAKE} distributeworld DISTDIR=${DISTDIR}
# Set up mergemaster root database
	sh ${.CURDIR}/scripts/mm-mtree.sh -m ${WORLDDIR} -F \
	    "TARGET_ARCH=${TARGET_ARCH} TARGET=${TARGET}" -D "${DISTDIR}/base"
# Package all components
	cd ${WORLDDIR} && ${IMAKE} packageworld DISTDIR=${DISTDIR}
	mv ${DISTDIR}/*.txz ${.OBJDIR}

kernel.txz:
	-mkdir -p ${DISTDIR}
	cd ${WORLDDIR} && ${IMAKE} distributekernel packagekernel DISTDIR=${DISTDIR}
	mv ${DISTDIR}/kernel.txz ${.OBJDIR}

src.txz:
	-mkdir -p ${DISTDIR}/usr
	ln -fs ${WORLDDIR} ${DISTDIR}/usr/src
	cd ${DISTDIR} && tar cLvJf ${.OBJDIR}/src.txz --exclude CVS usr/src

mports.txz:
	-mkdir -p ${DISTDIR}/usr
	ln -fs ${PORTSDIR} ${DISTDIR}/usr/mports
	cd ${DISTDIR} && tar cLvJf ${.OBJDIR}/mports.txz \
	    --exclude usr/mports/Distfiles --exclude usr/mports/Packages \
	    --exclude 'usr/mports/INDEX*' --exclude work usr/mports

system: packagesystem
# Install system
	-mkdir ${.OBJDIR}/release
	cd ${WORLDDIR} && ${IMAKE} installkernel installworld distribution \
	    DESTDIR=${.OBJDIR}/release WITHOUT_RESCUE=1 WITHOUT_KERNEL_SYMBOLS=1
# Copy distfiles
	-mkdir ${.OBJDIR}/release/usr/midnightbsd-dist
	cp ${.OBJDIR}/*.txz ${.OBJDIR}/MANIFEST \
	    ${.OBJDIR}/release/usr/midnightbsd-dist
# Set up installation environment
	ln -fs /tmp/bsdinstall_etc/resolv.conf ${.OBJDIR}/release/etc/resolv.conf
	echo sendmail_enable=\"NONE\" > ${.OBJDIR}/release/etc/rc.conf
	echo hostid_enable=\"NO\" >> ${.OBJDIR}/release/etc/rc.conf
	cp ${.CURDIR}/rc.local ${.OBJDIR}/release/etc
	touch ${.OBJDIR}/${.TARGET}

bootonly: packagesystem
# Install system
	mkdir ${.OBJDIR}/bootonly
	cd ${WORLDDIR} && ${IMAKE} installkernel installworld distribution \
	    DESTDIR=${.OBJDIR}/bootonly WITHOUT_AMD=1 WITHOUT_AT=1 \
	    WITHOUT_BIND_DNSSEC=1 WITHOUT_BIND_ETC=1 WITHOUT_BIND_MTREE=1 \
	    WITHOUT_BIND_NAMED=1 WITHOUT_GAMES=1 WITHOUT_GROFF=1 \
	    WITHOUT_INSTALLLIB=1 WITHOUT_LIB32=1 WITHOUT_MAIL=1 \
	    WITHOUT_NCP=1 WITHOUT_TOOLCHAIN=1 WITHOUT_PROFILE=1 \
	    WITHOUT_INSTALLIB=1 WITHOUT_RESCUE=1 WITHOUT_DICT=1 \
	    WITHOUT_KERNEL_SYMBOLS=1
# Copy manifest only (no distfiles) to get checksums
	-mkdir ${.OBJDIR}/bootonly/usr/midnightbsd-dist
	cp ${.OBJDIR}/MANIFEST ${.OBJDIR}/bootonly/usr/midnightbsd-dist
# Set up installation environment
	ln -fs /tmp/bsdinstall_etc/resolv.conf ${.OBJDIR}/bootonly/etc/resolv.conf
	echo sendmail_enable=\"NONE\" > ${.OBJDIR}/bootonly/etc/rc.conf
	echo hostid_enable=\"NO\" >> ${.OBJDIR}/bootonly/etc/rc.conf
	cp ${.CURDIR}/rc.local ${.OBJDIR}/bootonly/etc

MidnightBSD-release.iso: system
	sh ${.CURDIR}/${TARGET}/mkisoimages.sh -b MidnightBSD_Install ${.OBJDIR}/MidnightBSD-release.iso ${.OBJDIR}/release

MidnightBSD-bootonly.iso: bootonly
	sh ${.CURDIR}/${TARGET}/mkisoimages.sh -b MidnightBSD_Install ${.OBJDIR}/MidnightBSD-bootonly.iso ${.OBJDIR}/bootonly

memstick: system
	sh ${.CURDIR}/${TARGET}/make-memstick.sh ${.OBJDIR}/release ${.OBJDIR}/memstick

packagesystem: base.txz kernel.txz ${EXTRA_PACKAGES}
	sh ${.CURDIR}/scripts/make-manifest.sh ${.OBJDIR}/*.txz > ${.OBJDIR}/MANIFEST
	touch ${.TARGET}

cdrom: MidnightBSD-release.iso MidnightBSD-bootonly.iso
ftp: packagesystem
	rm -rf ftp
	mkdir ftp
	cp *.txz MANIFEST ftp

release:
	${MAKE} -C ${.CURDIR} ${.MAKEFLAGS} obj
	${MAKE} -C ${.CURDIR} ${.MAKEFLAGS} ${RELEASE_TARGETS}

clean:
	chflags -R noschg ${.OBJDIR}
	rm -rf dist ftp
	rm -f packagesystem
	rm -f *.txz MANIFEST
	rm -f system
	rm -rf release bootonly
	rm -f MidnightBSD-release.iso MidnightBSD-bootonly.iso memstick

install:
	-mkdir ${DESTDIR}
	cp -a *.iso memstick ftp ${DESTDIR}/

