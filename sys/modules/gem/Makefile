
.PATH: ${SRCTOP}/sys/dev/gem

KMOD=	if_gem
SRCS=	bus_if.h device_if.h if_gem.c if_gem_pci.c ${if_gem_sbus} miibus_if.h
SRCS+=	${ofw_bus_if} pci_if.h

.include <bsd.kmod.mk>
