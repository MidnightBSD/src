# $MidnightBSD: src/release/Makefile,v 1.39 2013/04/06 04:25:45 laffer1 Exp $
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
OUTPUT=		/usr/rel

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
	mv ${DISTDIR}/*.txz ${OUTPUT}

kernel.txz:
	-mkdir -p ${DISTDIR}
	cd ${WORLDDIR} && ${IMAKE} distributekernel packagekernel DISTDIR=${DISTDIR}
	mv ${DISTDIR}/kernel.txz ${OUTPUT}

src.txz:
	echo "src.txz"
	-unlink ${DISTDIR}/usr/src
	-mkdir -p ${DISTDIR}/usr
	ln -fs ${WORLDDIR} ${DISTDIR}/usr/src
	cd ${DISTDIR} && tar cLvJf ${OUTPUT}/src.txz --exclude .svn \
	    --exclude "release/*.txz" --exclude CVS usr/src

mports.txz:
	echo "mports.txz"
	-unlink ${DISTDIR}/usr/mports
	-mkdir -p ${DISTDIR}/usr
	ln -fs ${PORTSDIR} ${DISTDIR}/usr/mports
	cd ${DISTDIR} && tar cLvJf ${OUTPUT}/mports.txz \
	    --exclude usr/mports/Distfiles --exclude usr/mports/Packages \
	    --exclude 'usr/mports/INDEX*' --exclude work \
	    --exclude CVS usr/mports

system: packagesystem
# Install system
	-mkdir ${OUTPUT}/release
	cd ${WORLDDIR} && ${IMAKE} installkernel installworld distribution \
	    DESTDIR=${OUTPUT}/release WITHOUT_RESCUE=1 WITHOUT_KERNEL_SYMBOLS=1
# Copy distfiles
	-mkdir ${OUTPUT}/release/usr/midnightbsd-dist
	cp ${OUTPUT}/*.txz ${OUTPUT}/MANIFEST \
	    ${OUTPUT}/release/usr/midnightbsd-dist
# Set up installation environment
	ln -fs /tmp/bsdinstall_etc/resolv.conf ${OUTPUT}/release/etc/resolv.conf
	echo sendmail_enable=\"NONE\" > ${OUTPUT}/release/etc/rc.conf
	echo hostid_enable=\"NO\" >> ${OUTPUT}/release/etc/rc.conf
	cp ${.CURDIR}/rc.local ${OUTPUT}/release/etc
	cp ${.CURDIR}/fstab ${OUTPUT}/release/etc
	rm ${OUTPUT}/release/etc/rc.d/firstboot
	touch ${OUTPUT}/${.TARGET}

bootonly: packagesystem
# Install system
	mkdir ${OUTPUT}/bootonly
	cd ${WORLDDIR} && ${IMAKE} installkernel installworld distribution \
	    DESTDIR=${OUTPUT}/bootonly WITHOUT_AMD=1 WITHOUT_AT=1 \
	    WITHOUT_BIND_DNSSEC=1 WITHOUT_BIND_ETC=1 WITHOUT_BIND_MTREE=1 \
	    WITHOUT_BIND_NAMED=1 WITHOUT_GAMES=1 WITHOUT_GROFF=1 \
	    WITHOUT_INSTALLLIB=1 WITHOUT_LIB32=1 WITHOUT_MAIL=1 \
	    WITHOUT_NCP=1 WITHOUT_TOOLCHAIN=1 WITHOUT_PROFILE=1 \
	    WITHOUT_INSTALLIB=1 WITHOUT_RESCUE=1 WITHOUT_DICT=1 \
	    WITHOUT_KERNEL_SYMBOLS=1
# Copy manifest only (no distfiles) to get checksums
	-mkdir ${OUTPUT}/bootonly/usr/midnightbsd-dist
	cp ${OUTPUT}/MANIFEST ${OUTPUT}/bootonly/usr/midnightbsd-dist
# Set up installation environment
	ln -fs /tmp/bsdinstall_etc/resolv.conf ${OUTPUT}/bootonly/etc/resolv.conf
	echo sendmail_enable=\"NONE\" > ${OUTPUT}/bootonly/etc/rc.conf
	echo hostid_enable=\"NO\" >> ${OUTPUT}/bootonly/etc/rc.conf
	cp ${.CURDIR}/rc.local ${OUTPUT}/bootonly/etc
	cp ${.CURDIR}/fstab ${OUTPUT}/bootonly/etc
	rm ${OUTPUT}/bootonly/etc/rc.d/firstboot

MidnightBSD-release.iso: system
	sh ${.CURDIR}/${TARGET}/mkisoimages.sh -b MidnightBSD_Install ${OUTPUT}/MidnightBSD-release.iso ${OUTPUT}/release

MidnightBSD-bootonly.iso: bootonly
	sh ${.CURDIR}/${TARGET}/mkisoimages.sh -b MidnightBSD_Install ${OUTPUT}/MidnightBSD-bootonly.iso ${OUTPUT}/bootonly

memstick: system
	sh ${.CURDIR}/${TARGET}/make-memstick.sh ${OUTPUT}/release ${OUTPUT}/memstick

packagesystem: base.txz kernel.txz ${EXTRA_PACKAGES}
	sh ${.CURDIR}/scripts/make-manifest.sh ${OUTPUT}/*.txz > ${OUTPUT}/MANIFEST
	touch ${.TARGET}

cdrom: MidnightBSD-release.iso MidnightBSD-bootonly.iso
ftp: packagesystem
	rm -rf ${OUTPUT}/ftp
	mkdir ${OUTPUT}/ftp
	cp ${OUTPUT}/*.txz ${OUTPUT}/MANIFEST ${OUTPUT}/ftp

release:
	${MAKE} -C ${.CURDIR} ${.MAKEFLAGS} obj
	${MAKE} -C ${.CURDIR} ${.MAKEFLAGS} ${RELEASE_TARGETS}

clean:
	chflags -R noschg ${OUTPUT} ${DISTDIR}
	rm -rf ${OUTPUT}/dist ${OUTPUT}/ftp
	rm -rf ${DISTDIR}
	rm -f packagesystem
	rm -f ${OUTPUT}/*.txz ${OUTPUT}/MANIFEST
	rm -f system
	rm -rf ${OUTPUT}/release ${OUTPUT}/bootonly
	rm -f MidnightBSD-release.iso MidnightBSD-bootonly.iso memstick

install:
	-mkdir ${DESTDIR}
	cp -a *.iso memstick ftp ${DESTDIR}/

