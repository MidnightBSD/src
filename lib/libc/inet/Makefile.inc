# $FreeBSD: stable/11/lib/libc/inet/Makefile.inc 262722 2014-03-04 02:19:39Z marcel $

# inet sources
.PATH: ${LIBC_SRCTOP}/inet

SRCS+=	inet_addr.c inet_cidr_ntop.c inet_cidr_pton.c inet_lnaof.c \
	inet_makeaddr.c inet_net_ntop.c inet_net_pton.c inet_neta.c \
	inet_netof.c inet_network.c inet_ntoa.c inet_ntop.c \
	inet_pton.c nsap_addr.c

SYM_MAPS+= ${LIBC_SRCTOP}/inet/Symbol.map
