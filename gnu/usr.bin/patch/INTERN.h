/* $FreeBSD: src/gnu/usr.bin/patch/INTERN.h,v 1.6 1999/09/05 17:31:54 peter Exp $
 *
 * $Log: not supported by cvs2svn $
 * Revision 1.1.1.1  2005/12/24 02:43:05  laffer1
 * Imported from FreeBSD 6.0 sources
 *
 * Revision 2.0  86/09/17  15:35:58  lwall
 * Baseline for netwide release.
 *
 */

#ifdef EXT
#undef EXT
#endif
#define EXT

#ifdef INIT
#undef INIT
#endif
#define INIT(x) = x

#define DOINIT
