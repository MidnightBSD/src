SYSDIR?=${SRCTOP}/sys
.include "${SYSDIR}/conf/kern.opts.mk"

ATLANTIC     = ${SYSDIR}/dev/atlantic
.PATH:  ${ATLANTIC}

KMOD=	if_atlantic
SRCS=   device_if.h bus_if.h pci_if.h ifdi_if.h opt_iflib.h
SRCS+=  opt_inet.h opt_inet6.h opt_rss.h
SRCS+=  aq_main.c aq_media.c aq_irq.c
SRCS+=  aq_ring.c aq_hw.c aq_hw_llh.c
SRCS+=  aq_fw.c aq_fw1x.c aq_fw2x.c aq_fw_aq2.c aq_dbg.c
SRCS+=  aq_hostboot.c aq_hostboot_legacy.c aq_hostboot_aq2.c

CFLAGS += -DIFLIB -I${ATLANTIC} -I${SRCTOP}/include -I${SRCTOP}/sys/sys

.include <bsd.kmod.mk>
CWARNFLAGS+=    ${NO_WUNUSED_BUT_SET_VARIABLE}
