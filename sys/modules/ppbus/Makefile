
# $FreeBSD: stable/11/sys/modules/ppbus/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/ppbus
KMOD=	ppbus
SRCS=	bus_if.h device_if.h ppbus_if.h ppbus_if.c \
	opt_ppb_1284.h \
	ppb_1284.c ppb_base.c ppb_msq.c ppbconf.c

EXPORT_SYMS=	ppb_attach_device	\
		ppb_request_bus		\
		ppb_release_bus		\
		ppb_get_status		\
		ppb_poll_bus		\
		ppb_reset_epp_timeout	\
		ppb_ecp_sync		\
		ppb_get_epp_protocol	\
		ppb_set_mode		\
		ppb_get_mode		\
		ppb_write

.include <bsd.kmod.mk>
