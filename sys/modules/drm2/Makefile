# $FreeBSD: stable/11/sys/modules/drm2/Makefile 319182 2017-05-30 04:11:12Z ngie $

SYSDIR?=${SRCTOP}/sys
.include "${SYSDIR}/conf/kern.opts.mk"

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
