
.PATH: ${SRCTOP}/sys/net

KMOD=	if_tuntap
SRCS=	if_tuntap.c opt_inet.h opt_inet6.h

# Symlink for backwards compatibility with systems installed at 2.x or older
SYMLINKS=	${KMOD}.ko ${KMODDIR}/if_tun.ko \
	${KMOD}.ko ${KMODDIR}/if_tap.ko

.include <bsd.kmod.mk>
