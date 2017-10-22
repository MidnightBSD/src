# $FreeBSD: release/10.0.0/lib/libc/nameser/Makefile.inc 157049 2006-03-23 16:55:13Z des $

# nameser sources
.PATH: ${.CURDIR}/nameser

SRCS+=	ns_name.c ns_netint.c ns_parse.c ns_print.c ns_samedomain.c ns_ttl.c

SYM_MAPS+= ${.CURDIR}/nameser/Symbol.map
