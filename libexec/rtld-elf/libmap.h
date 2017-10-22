/*
 * $FreeBSD: release/10.0.0/libexec/rtld-elf/libmap.h 255765 2013-09-21 21:03:52Z des $
 */

int	lm_init (char *);
void	lm_fini (void);
char *	lm_find (const char *, const char *);
char *	lm_findn (const char *, const char *, const int);
