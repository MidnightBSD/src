# $FreeBSD: src/share/mk/bsd.kmod.mk,v 1.91 2004/06/21 16:12:02 bde Exp $

# Search for kernel source tree in standard places.
.for _dir in ${.CURDIR}/../.. ${.CURDIR}/../../.. ${.CURDIR}/../../../.. \
    /sys /usr/src/sys
.if !defined(SYSDIR) && exists(${_dir}/kern/) && exists(${_dir}/conf/kmod.mk)
SYSDIR=	${_dir}
.endif
.endfor
.if !defined(SYSDIR) || !exists(${SYSDIR}/kern/) || \
    !exists(${SYSDIR}/conf/kmod.mk)
.error "can't find kernel source tree"
.endif

.include "${SYSDIR}/conf/kmod.mk"

.include <bsd.sys.mk>
