# $FreeBSD: stable/11/stand/efi/loader/arch/arm64/Makefile.inc 329140 2018-02-11 20:58:00Z kevans $

HAVE_FDT=yes

SRCS+=	exec.c \
	start.S

.PATH:	${BOOTSRC}/arm64/libarm64
CFLAGS+=-I${BOOTSRC}/arm64/libarm64
SRCS+=	cache.c

CFLAGS+=	-mgeneral-regs-only
