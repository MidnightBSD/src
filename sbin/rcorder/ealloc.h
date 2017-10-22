/*	$FreeBSD: release/10.0.0/sbin/rcorder/ealloc.h 173412 2007-11-07 10:53:41Z kevlo $	*/
/*	$NetBSD: ealloc.h,v 1.1.1.1 1999/11/19 04:30:56 mrg Exp $	*/

void	*emalloc(size_t len);
char	*estrdup(const char *str);
void	*erealloc(void *ptr, size_t size);
void	*ecalloc(size_t nmemb, size_t size);
