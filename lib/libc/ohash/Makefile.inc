# $MidnightBSD: src/lib/libc/ohash/Makefile.inc,v 1.1 2007/03/24 07:53:53 archite Exp $
# $OpenBSD: Makefile.inc,v 1.2 2001/03/02 13:28:11 espie Exp $


# open hashing functions
.PATH: ${.CURDIR}/ohash

SRCS+=	ohash_create_entry.c ohash_delete.c ohash_do.c ohash_entries.c \
	ohash_enum.c ohash_init.c ohash_interval.c \
	ohash_lookup_interval.c ohash_lookup_memory.c \
	ohash_qlookup.c ohash_qlookupi.c

CFLAGS+=	-Iohash -Wno-cast-qual
WARNS?=	3
NO_WERROR=	yes

SYM_MAPS+=${.CURDIR}/ohash/Symbol.map

MAN+=	ohash_init.3 ohash_interval.3
MLINKS+=ohash_init.3 ohash_delete.3 \
	ohash_init.3 ohash_lookup_interval.3 \
	ohash_init.3 ohash_lookup_memory.3 \
	ohash_init.3 ohash_find.3 \
	ohash_init.3 ohash_remove.3 \
	ohash_init.3 ohash_insert.3 \
	ohash_init.3 ohash_first.3 \
	ohash_init.3 ohash_next.3 \
	ohash_init.3 ohash_entries.3 \
	ohash_interval.3 ohash_create_entry.3 \
	ohash_interval.3 ohash_qlookupi.3 \
	ohash_interval.3 ohash_qlookup.3
