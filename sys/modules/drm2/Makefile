# $MidnightBSD$
# $FreeBSD: stable/10/sys/modules/drm2/Makefile 291811 2015-12-04 18:44:12Z bdrewery $

.include <bsd.own.mk>

.if ${MACHINE_CPUARCH} == "amd64"
_i915kms=	i915kms
_radeonkms=	radeonkms
. if ${MK_SOURCELESS_UCODE} != "no"
_radeonkmsfw=	radeonkmsfw
. endif
.endif

.if ${MACHINE_CPUARCH} == "i386"
. if ${MACHINE} != "pc98"
_i915kms=	i915kms
_radeonkms=	radeonkms
.  if ${MK_SOURCELESS_UCODE} != "no"
_radeonkmsfw=	radeonkmsfw
.  endif
. endif
.endif

.if ${MACHINE_CPUARCH} == "powerpc"
_radeonkms=	radeonkms
. if ${MK_SOURCELESS_UCODE} != "no"
_radeonkmsfw=	radeonkmsfw
. endif
.endif

SUBDIR = \
	drm2 \
	${_i915kms} \
	${_radeonkms} \
	${_radeonkmsfw}

.include <bsd.subdir.mk>
