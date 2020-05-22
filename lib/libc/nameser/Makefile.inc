# $FreeBSD: stable/11/lib/libc/nameser/Makefile.inc 262722 2014-03-04 02:19:39Z marcel $

# nameser sources
.PATH: ${LIBC_SRCTOP}/nameser

SRCS+=	ns_name.c ns_netint.c ns_parse.c ns_print.c ns_samedomain.c ns_ttl.c

SYM_MAPS+= ${LIBC_SRCTOP}/nameser/Symbol.map
