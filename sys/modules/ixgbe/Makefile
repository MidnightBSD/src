#$FreeBSD: release/7.0.0/sys/modules/ixgbe/Makefile 172043 2007-09-04 02:31:35Z jfv $
.PATH:  ${.CURDIR}/../../dev/ixgbe
KMOD    = ixgbe
SRCS    = device_if.h bus_if.h pci_if.h opt_bdg.h
SRCS    += ixgbe.c 
# Shared source
SRCS    += ixgbe_common.c ixgbe_api.c ixgbe_phy.c ixgbe_82598.c
CFLAGS+= -I${.CURDIR}/../../dev/ixgbe

clean:
	rm -f opt_bdg.h device_if.h bus_if.h pci_if.h setdef* *_StripErr
	rm -f *.o *.kld *.ko
	rm -f @ machine

.include <bsd.kmod.mk>
