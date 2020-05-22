# $FreeBSD: stable/11/lib/libc/capability/Makefile.inc 313240 2017-02-04 17:17:38Z ngie $

# capability sources
.PATH: ${SRCTOP}/sys/kern ${LIBC_SRCTOP}/capability

SRCS+=	subr_capability.c

SYM_MAPS+=	${LIBC_SRCTOP}/capability/Symbol.map

MAN+=	cap_rights_init.3

MLINKS+=cap_rights_init.3 cap_rights_set.3
MLINKS+=cap_rights_init.3 cap_rights_clear.3
MLINKS+=cap_rights_init.3 cap_rights_is_set.3
MLINKS+=cap_rights_init.3 cap_rights_is_valid.3
MLINKS+=cap_rights_init.3 cap_rights_merge.3
MLINKS+=cap_rights_init.3 cap_rights_remove.3
MLINKS+=cap_rights_init.3 cap_rights_contains.3
