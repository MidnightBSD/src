# $FreeBSD: release/10.0.0/sys/modules/drm2/Makefile 254899 2013-08-26 09:17:21Z dumbbell $

.include <bsd.own.mk>

.if ${MACHINE_CPUARCH} == "amd64"
_radeonkms=	radeonkms
. if ${MK_SOURCELESS_UCODE} != "no"
_radeonkmsfw=	radeonkmsfw
. endif
.endif

.if ${MACHINE_CPUARCH} == "i386"
. if ${MACHINE} != "pc98"
_radeonkms=	radeonkms
.  if ${MK_SOURCELESS_UCODE} != "no"
_radeonkmsfw=	radeonkmsfw
.  endif
. endif
.endif

SUBDIR = \
	drm2 \
	i915kms \
	${_radeonkms} \
	${_radeonkmsfw}

.include <bsd.subdir.mk>
