#
# Makefile for building releases and release media.
#
# User-driven targets:
#  cdrom: Builds release CD-ROM media (disc1.iso)
#  dvdrom: Builds release DVD-ROM media (dvd1.iso)
#  memstick: Builds memory stick image (memstick.img)
#  mini-memstick: Builds minimal memory stick image (mini-memstick.img)
#  ftp: Sets up FTP distribution area (ftp)
#  release: Invokes real-release, vm-release, and cloudware-release targets
#  real-release: Build all media and FTP distribution area
#  vm-release: Build all virtual machine image targets
#  cloudware-release: Build all cloud hosting provider targets
#  install: Invokes the release-install and vm-install targets
#  release-install: Copies all release installation media into ${DESTDIR}
#  vm-install: Copies all virtual machine images into ${DESTDIR}
#  cloud-install: Copies non-uploaded cloud images into ${DESTDIR}
#
# Variables affecting the build process:
#  WORLDDIR: location of src tree -- must have built world and default kernel
#            (by default, the directory above this one)
#  PORTSDIR: location of ports tree to distribute (default: /usr/ports)
#  XTRADIR:  xtra-bits-dir argument for <arch>/mkisoimages.sh
#  NOPKG:    if set, do not distribute third-party packages
#  NOPORTS:  if set, do not distribute ports tree
#  NOSRC:    if set, do not distribute source tree
#  WITH_DVD: if set, generate dvd1.iso
#  WITH_COMPRESSED_IMAGES: if set, compress installation images with xz(1)
#		(uncompressed images are not removed)
#  WITH_VMIMAGES: if set, build virtual machine images with the release
#  WITH_COMPRESSED_VMIMAGES: if set, compress virtual machine disk images
#  		with xz(1) (extremely time consuming)
#  WITH_CLOUDWARE: if set, build cloud hosting disk images with the release
#  TARGET/TARGET_ARCH: architecture of built release
#

WORLDDIR?=	${.CURDIR}/..
PORTSDIR?=	/usr/mports

.if !defined(TARGET) || empty(TARGET)
TARGET=		${MACHINE}
.endif
.if !defined(TARGET_ARCH) || empty(TARGET_ARCH)
.if ${TARGET} == ${MACHINE}
TARGET_ARCH=	${MACHINE_ARCH}
.else
TARGET_ARCH=	${TARGET}
.endif
.endif
IMAKE=		${MAKE} TARGET_ARCH=${TARGET_ARCH} TARGET=${TARGET}
DISTDIR=	dist

# Define OSRELEASE by using newvers.sh
.if !defined(OSRELEASE) || empty(OSRELEASE)
.for _V in TYPE BRANCH REVISION
${_V}!=	eval $$(awk '/^${_V}=/{print}' ${.CURDIR}/../sys/conf/newvers.sh); echo $$${_V}
.endfor
.for _V in ${TARGET_ARCH}
.if !empty(TARGET:M${_V})
OSRELEASE=	${TYPE}-${REVISION}-${BRANCH}-${TARGET}
VOLUME_LABEL=	${REVISION:C/[.-]/_/g}_${BRANCH:C/[.-]/_/g}_${TARGET}
.else
OSRELEASE=	${TYPE}-${REVISION}-${BRANCH}-${TARGET}-${TARGET_ARCH}
VOLUME_LABEL=	${REVISION:C/[.-]/_/g}_${BRANCH:C/[.-]/_/g}_${TARGET_ARCH}
.endif
.endfor
.endif

.if !defined(VOLUME_LABEL) || empty(VOLUME_LABEL)
VOLUME_LABEL=	MidnightBSD_Install
.endif

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
IMAGES=
.if exists(${.CURDIR}/${TARGET}/mkisoimages.sh)
RELEASE_TARGETS+= cdrom
IMAGES+=	disc1.iso bootonly.iso
. if defined(WITH_DVD) && !empty(WITH_DVD)
RELEASE_TARGETS+= dvdrom
IMAGES+=	dvd1.iso
. endif
.endif
.if exists(${.CURDIR}/${TARGET}/make-memstick.sh)
RELEASE_TARGETS+= memstick.img
RELEASE_TARGETS+= mini-memstick.img
IMAGES+=	memstick.img
IMAGES+=	mini-memstick.img
.endif

CLEANFILES=	packagesystem *.txz MANIFEST release ${IMAGES}
.if defined(WITH_COMPRESSED_IMAGES) && !empty(WITH_COMPRESSED_IMAGES)
. for I in ${IMAGES}
CLEANFILES+=	${I}.xz
. endfor
.endif
.if defined(WITH_DVD) && !empty(WITH_DVD)
CLEANFILES+=	pkg-stage
.endif
CLEANDIRS=	dist ftp disc1 bootonly dvd
beforeclean:
	chflags -R noschg .
.include <bsd.obj.mk>
clean: beforeclean

base.txz:
	mkdir -p ${DISTDIR}
	cd ${WORLDDIR} && ${IMAKE} distributeworld DISTDIR=${.OBJDIR}/${DISTDIR}
# Set up mergemaster root database
	sh ${.CURDIR}/scripts/mm-mtree.sh -m ${WORLDDIR} -F \
	    "TARGET_ARCH=${TARGET_ARCH} TARGET=${TARGET} ${.MAKE.JOBS:D-j${.MAKE.JOBS}}" \
	    -D "${.OBJDIR}/${DISTDIR}/base"
	etcupdate extract -B -M "TARGET_ARCH=${TARGET_ARCH} TARGET=${TARGET}" \
	    -s ${WORLDDIR} -d "${.OBJDIR}/${DISTDIR}/base/var/db/etcupdate" -L /dev/stdout
# Package all components
	cd ${WORLDDIR} && ${IMAKE} packageworld DISTDIR=${.OBJDIR}/${DISTDIR}
	mv ${DISTDIR}/*.txz .

kernel.txz:
	mkdir -p ${DISTDIR}
	cd ${WORLDDIR} && ${IMAKE} distributekernel packagekernel DISTDIR=${.OBJDIR}/${DISTDIR}
	mv ${DISTDIR}/kernel*.txz .

src.txz:
	mkdir -p ${DISTDIR}/usr
	ln -fs ${WORLDDIR} ${DISTDIR}/usr/src
	cd ${DISTDIR} && tar cLvf - --exclude .svn --exclude .zfs \
	    --exclude .git --exclude @ --exclude usr/src/release/dist usr/src | \
	    ${XZ_CMD} > ${.OBJDIR}/src.txz

mports.txz:
	mkdir -p ${DISTDIR}/usr
	ln -fs ${PORTSDIR} ${DISTDIR}/usr/mports
	cd ${DISTDIR} && tar cLvf - \
	    --exclude .git --exclude .svn \
	    --exclude usr/mports/Distfiles --exclude usr/mports/Packages \
	    --exclude 'usr/mports/INDEX*' --exclude work usr/mports | \
	    ${XZ_CMD} > ${.OBJDIR}/mports.txz

disc1: packagesystem
# Install system
	mkdir -p ${.TARGET}
	cd ${WORLDDIR} && ${IMAKE} installworld installkernel distribution \
	    DESTDIR=${.OBJDIR}/${.TARGET} MK_AT=no \
	    MK_INSTALLLIB=no MK_LIB32=no MK_MAIL=no \
	    MK_TOOLCHAIN=no MK_PROFILE=no \
	    MK_RESCUE=no MK_DICT=no \
	    MK_KERNEL_SYMBOLS=no MK_TESTS=no MK_DEBUG_FILES=no \
	    -DDB_FROM_SRC
# Copy distfiles
	mkdir -p ${.TARGET}/usr/midnightbsd-dist
	for dist in MANIFEST $$(ls *.txz | grep -vE -- '(base|lib32)-dbg'); \
	    do cp $${dist} ${.TARGET}/usr/midnightbsd-dist; \
	done
# Set up installation environment
	ln -fs /tmp/bsdinstall_etc/resolv.conf ${.TARGET}/etc/resolv.conf
	echo sendmail_enable=\"NONE\" > ${.TARGET}/etc/rc.conf
	echo hostid_enable=\"NO\" >> ${.TARGET}/etc/rc.conf
	echo vfs.mountroot.timeout=\"10\" >> ${.TARGET}/boot/loader.conf
	echo kernels_autodetect=\"NO\" >> ${.TARGET}/boot/loader.conf
	echo loader_menu_multi_user_prompt=\"Installer\" >> ${.TARGET}/boot/loader.conf
	cp ${.CURDIR}/rc.local ${.TARGET}/etc
	touch ${.TARGET}

bootonly: packagesystem
# Install system
	mkdir -p ${.TARGET}
	cd ${WORLDDIR} && ${IMAKE} installworld installkernel distribution \
	    DESTDIR=${.OBJDIR}/${.TARGET} MK_AT=no \
	    MK_GAMES=no \
	    MK_INSTALLLIB=no MK_LIB32=no MK_MAIL=no \
	    MK_TOOLCHAIN=no MK_PROFILE=no \
	    MK_RESCUE=no MK_DICT=no \
	    MK_KERNEL_SYMBOLS=no MK_TESTS=no MK_DEBUG_FILES=no \
	    -DDB_FROM_SRC
# Copy manifest only (no distfiles) to get checksums
	mkdir -p ${.TARGET}/usr/midnightbsd-dist
	cp MANIFEST ${.TARGET}/usr/midnightbsd-dist
# Set up installation environment
	ln -fs /tmp/bsdinstall_etc/resolv.conf ${.TARGET}/etc/resolv.conf
	echo sendmail_enable=\"NONE\" > ${.TARGET}/etc/rc.conf
	echo hostid_enable=\"NO\" >> ${.TARGET}/etc/rc.conf
	echo vfs.mountroot.timeout=\"10\" >> ${.TARGET}/boot/loader.conf
	echo kernels_autodetect=\"NO\" >> ${.TARGET}/boot/loader.conf
	echo loader_menu_multi_user_prompt=\"Installer\" >> ${.TARGET}/boot/loader.conf
	cp ${.CURDIR}/rc.local ${.TARGET}/etc

dvd: packagesystem
# Install system
	mkdir -p ${.TARGET}
	cd ${WORLDDIR} && ${IMAKE} installworld installkernel distribution \
		DESTDIR=${.OBJDIR}/${.TARGET} MK_RESCUE=no MK_KERNEL_SYMBOLS=no \
		MK_TESTS=no MK_DEBUG_FILES=no \
		-DDB_FROM_SRC
# Copy distfiles
	mkdir -p ${.TARGET}/usr/midnightbsd-dist
	for dist in MANIFEST $$(ls *.txz | grep -v -- '(base|lib32)-dbg'); \
	    do cp $${dist} ${.TARGET}/usr/midnightbsd-dist; \
	done
# Set up installation environment
	ln -fs /tmp/bsdinstall_etc/resolv.conf ${.TARGET}/etc/resolv.conf
	echo sendmail_enable=\"NONE\" > ${.TARGET}/etc/rc.conf
	echo hostid_enable=\"NO\" >> ${.TARGET}/etc/rc.conf
	echo vfs.mountroot.timeout=\"10\" >> ${.TARGET}/boot/loader.conf
	echo kernels_autodetect=\"NO\" >> ${.TARGET}/boot/loader.conf
	echo loader_menu_multi_user_prompt=\"Installer\" >> ${.TARGET}/boot/loader.conf
	cp ${.CURDIR}/rc.local ${.TARGET}/etc
	touch ${.TARGET}

release.iso: disc1.iso
disc1.iso: disc1
	sh ${.CURDIR}/${TARGET}/mkisoimages.sh -b ${VOLUME_LABEL}_CD ${.TARGET} disc1 ${XTRADIR}

dvd1.iso: dvd pkg-stage
	sh ${.CURDIR}/${TARGET}/mkisoimages.sh -b ${VOLUME_LABEL}_DVD ${.TARGET} dvd ${XTRADIR}

bootonly.iso: bootonly
	sh ${.CURDIR}/${TARGET}/mkisoimages.sh -b ${VOLUME_LABEL}_BO ${.TARGET} bootonly ${XTRADIR}

memstick: memstick.img
memstick.img: disc1
	sh ${.CURDIR}/${TARGET}/make-memstick.sh disc1 ${.TARGET}

mini-memstick: mini-memstick.img
mini-memstick.img: bootonly
	sh ${.CURDIR}/${TARGET}/make-memstick.sh bootonly ${.TARGET}

packagesystem: base.txz kernel.txz ${EXTRA_PACKAGES}
	sh ${.CURDIR}/scripts/make-manifest.sh *.txz > MANIFEST
	touch ${.TARGET}

pkg-stage:
.if !defined(NOPKG) || empty(NOPKG)
	env PORTSDIR=${PORTSDIR} REPOS_DIR=${.CURDIR}/pkg_repos/ \
		sh ${.CURDIR}/scripts/pkg-stage.sh
	mkdir -p ${.OBJDIR}/dvd/packages/repos/
.endif
	touch ${.TARGET}

cdrom: disc1.iso bootonly.iso
dvdrom: dvd1.iso
ftp: packagesystem
	rm -rf ftp
	mkdir -p ftp
	cp *.txz MANIFEST ftp

release:	real-release vm-release cloudware-release
	${MAKE} -C ${.CURDIR} ${.MAKEFLAGS} release-done
	true

release-done:
	touch release

real-release:
	${MAKE} -C ${.CURDIR} ${.MAKEFLAGS} obj
	${MAKE} -C ${.CURDIR} ${.MAKEFLAGS} ${RELEASE_TARGETS}

install:	release-install vm-install .WAIT cloud-install

release-install:
.if defined(DESTDIR) && !empty(DESTDIR)
	mkdir -p ${DESTDIR}
.endif
	cp -a ftp ${DESTDIR}/
.if !empty(IMAGES)
.for I in ${IMAGES}
	cp -p ${I} ${DESTDIR}/${OSRELEASE}-${I}
. if defined(WITH_COMPRESSED_IMAGES) && !empty(WITH_COMPRESSED_IMAGES)
	${XZ_CMD} -k ${DESTDIR}/${OSRELEASE}-${I}
. endif
.endfor
	cd ${DESTDIR} && sha512 ${OSRELEASE}* > ${DESTDIR}/CHECKSUM.SHA512
	cd ${DESTDIR} && sha256 ${OSRELEASE}* > ${DESTDIR}/CHECKSUM.SHA256
.endif

.include "${.CURDIR}/Makefile.inc1"
.include "${.CURDIR}/Makefile.vm"
