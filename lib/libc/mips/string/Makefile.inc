#	$NetBSD: Makefile.inc,v 1.2 2000/10/10 21:51:54 jeffs Exp $
# $FreeBSD: release/10.0.0/lib/libc/mips/string/Makefile.inc 229368 2012-01-03 07:14:01Z ed $

SRCS+=	bcmp.S bcopy.S bzero.S ffs.S memchr.c memcmp.c memset.c \
	memcpy.S memmove.S \
	strcat.c strchr.S strcmp.S strcpy.c strcspn.c strlen.S \
	strncat.c strncmp.c strncpy.c strrchr.S strpbrk.c strsep.c \
	strspn.c strstr.c swab.c
