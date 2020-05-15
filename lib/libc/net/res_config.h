/* $FreeBSD: stable/11/lib/libc/net/res_config.h 298830 2016-04-30 01:24:24Z pfg $ */

#define	DEBUG	1	/* enable debugging code (needed for dig) */
#define	RESOLVSORT	/* allow sorting of addresses in gethostbyname */
#undef	SUNSECURITY	/* verify gethostbyaddr() calls - WE DON'T NEED IT  */
#define MULTI_PTRS_ARE_ALIASES 1 /* fold multiple PTR records into aliases */
