#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: stable/9/lib/libc/arm/string/Makefile.inc 150875 2005-10-03 14:21:49Z cognet $

MDSRCS+=bcmp.c bcopy.S bzero.S ffs.S index.c memchr.c memcmp.S memcpy.S \
	memmove.S memset.S rindex.c strcat.c strchr.c strcmp.S strcpy.c \
	strlen.S strncmp.S strrchr.c swab.c wcschr.c wcscmp.c wcslen.c \
	wmemchr.c
