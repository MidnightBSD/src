# $FreeBSD: stable/11/lib/libc/secure/Makefile.inc 296915 2016-03-15 19:26:32Z emaste $
#
# libc sources related to security

.PATH: ${LIBC_SRCTOP}/secure

# Sources common to both syscall interfaces:
SRCS+=	stack_protector.c
.if ${MK_SYMVER} == yes
SRCS+=	stack_protector_compat.c
.endif

SYM_MAPS+=    ${LIBC_SRCTOP}/secure/Symbol.map
